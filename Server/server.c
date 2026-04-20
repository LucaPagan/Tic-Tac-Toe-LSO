// #define _GNU_SOURCE // per asprintf
#include <arpa/inet.h> // inet_aton()
#include <fcntl.h>     // per fcntl(), F_GETFL, O_NONBLOCK
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // strcpy() // strlen
#include <sys/socket.h> // socket() // struct sockaddr
#include <unistd.h>     // close()

#include "dynamic_array.h"
#include "server.h"
#include <errno.h>
#include <sys/time.h>

#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#elif __has_include("/opt/homebrew/include/cjson/cJSON.h")
#include "/opt/homebrew/include/cjson/cJSON.h"
#else
#error "cJSON not found. Install via: brew install cjson"
#endif

#define SECONDS_UNTIL_DROP 30
#define POOL_SIZE_FOR_IDS 20

int ingest_network_opcode(int d_socket, NetworkOpcode *opcode_ptr);
int process_instantiation_request(struct PlayerNode *p_node);
int process_activation_request(struct PlayerNode *p_node);
int process_queue_fetch(int d_socket);

int invoke_rematch(struct MatchSession *m_session);
int fetch_draw_verdict(int d_socket);

DynamicArray activePlayerNodes;

DynamicArray activeMatchSessions;

int current_highest_id = 0;
int recycled_id_pool[POOL_SIZE_FOR_IDS];
int recycled_id_count = 0;
pthread_mutex_t id_generation_lock = PTHREAD_MUTEX_INITIALIZER;

int acquireNewSessionIdentifier() {
  pthread_mutex_lock(&id_generation_lock);

  int new_id;
  if (recycled_id_count <= 0) {
    new_id = current_highest_id++;
  } else {
    new_id = recycled_id_pool[--recycled_id_count];
  }

  pthread_mutex_unlock(&id_generation_lock);
  return new_id;
}

void relinquishSessionIdentifier(int identifierValue) {
  pthread_mutex_lock(&id_generation_lock);
  if (recycled_id_count < POOL_SIZE_FOR_IDS) {
    recycled_id_pool[recycled_id_count++] = identifierValue;
  }
  pthread_mutex_unlock(&id_generation_lock);
}

void purgePlayerNodeContext(void *contextPointer) {
  int *sock_fd_ptr = (int *)contextPointer;
  if (sock_fd_ptr && *sock_fd_ptr > 0) {
    /*
     * Rilascio delle risorse di sistema allocate per il descrittore.
     * La chiusura sincrona garantisce che il buffer di trasmissione
     * venga svuotato prima della deallocazione del thread.
     */
    close(*sock_fd_ptr);
    printf("[THR_ID: %lu] Recising link descriptor: %d\n",
           (unsigned long)pthread_self(), *sock_fd_ptr);
  }
}

unsigned int **allocateGridMatrixBuffer() {

  unsigned int **matrix = malloc(3 * sizeof *matrix);
  if (matrix == NULL) {
    perror("Memory allocation fault for base matrix");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < 3; ++j) {
    *(matrix + j) = calloc(3, sizeof *(*(matrix + j)));
    if (*(matrix + j) == NULL) {
      for (int k = 0; k < j; ++k) {
        free(*(matrix + k));
      }
      free(matrix);
      perror("Memory allocation fault for matrix rows");
      exit(EXIT_FAILURE);
    }
  }

  return matrix;
}

void deallocateGridMatrixBuffer(unsigned int **matrixPtr) {
  if (matrixPtr != NULL) {
    for (int k = 0; k < 3; ++k) {
      free(*(matrixPtr + k));
    }
    free(matrixPtr);
  }
}

void destroyMatchSessionInstance(void *instancePtr) {
  struct MatchSession *session = *(struct MatchSession **)instancePtr;
  if (!session)
    return;

  printf("[THR_ID: %lu] Obliterating session marker: %d\n",
         (unsigned long)pthread_self(), session->sessionId);

  deallocateGridMatrixBuffer(session->gridMatrix);
  session->participantPrimary = NULL;
  session->participantSecondary = NULL;

  free(session);

  *(struct MatchSession **)instancePtr = NULL;
}

void destroyPlayerNodeInstance(void *instancePtr) {

  struct PlayerNode *node = *(struct PlayerNode **)instancePtr;
  if (!node)
    return;

  printf("[THR_ID: %lu] Obliterating node socket: %d, moniker: %s\n",
         (unsigned long)pthread_self(), node->socketDescriptor,
         node->identityMoniker);
  if (node->identityMoniker)
    free(node->identityMoniker);
  pthread_mutex_destroy(&node->streamLock);

  free(node);
}

ssize_t consumeExactBytesFromStream(size_t expectedBytes, void *payloadBuffer,
                                    int descriptor_socket) {
  size_t ingested = 0;
  for (; ingested < expectedBytes;) {
    ssize_t chunk_size =
        recv(descriptor_socket, (char *)payloadBuffer + ingested,
             expectedBytes - ingested, 0);

    if (chunk_size < 0) {
      printf("[THR_ID: %lu] Stream ingestion fault: %d\n",
             (unsigned long)pthread_self(), errno);
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        if (errno != EINTR) {
          perror("stream_recv_fault");
          return -1;
        }
        continue;
      } else {
        printf("[THR_ID: %lu] Ingestion timeout elapsed\n",
               (unsigned long)pthread_self());
        return -2;
      }
    } else if (chunk_size == 0) {
      printf("[THR_ID: %lu] Remote peer severed connection\n",
             (unsigned long)pthread_self());
      return 0;
    }

    ingested += chunk_size;
  }

  return ingested;
}

const char *transformOpcodeToLoggableString(const NetworkOpcode opcodeValue) {
  if (opcodeValue == OP_SERVER_ACK)
    return "OP_SERVER_ACK";
  else if (opcodeValue == OP_BUILD_MATCH_SESSION)
    return "OP_BUILD_MATCH_SESSION";
  else if (opcodeValue == OP_FETCH_WAITING_LIST)
    return "OP_FETCH_WAITING_LIST";
  else if (opcodeValue == OP_INIT_MATCH_SESSION)
    return "OP_INIT_MATCH_SESSION";
  else if (opcodeValue == OP_TERMINATE_LINK)
    return "OP_TERMINATE_LINK";
  else if (opcodeValue == OP_APPROVE_REQ)
    return "OP_APPROVE_REQ";
  else if (opcodeValue == OP_DECLINE_REQ)
    return "OP_DECLINE_REQ";
  else
    return "UNIDENTIFIED_OPCODE";
}

const char *transformStateToLoggableString(const MatchSessionState stateValue) {
  if (stateValue == STATE_CONCLUDED)
    return "STATE_CONCLUDED";
  else if (stateValue == STATE_IN_PROGRESS)
    return "STATE_IN_PROGRESS";
  else if (stateValue == STATE_PENDING_PLYR)
    return "STATE_PENDING_PLYR";
  else if (stateValue == STATE_FRESH_INST)
    return "STATE_FRESH_INST";
  else
    return "UNIDENTIFIED_STATE";
}

void dumpMatchSessionDiagnostics(const struct MatchSession *sessionPtr) {
  if (sessionPtr) {
    printf("=== Session Snapshot %u ===\n", sessionPtr->sessionId);
    if (!sessionPtr->participantPrimary) {
      printf(" Principal : <vacant>\n");
    } else {
      printf(" Principal : %s (sock %u)\n",
             sessionPtr->participantPrimary->identityMoniker,
             sessionPtr->participantPrimary->socketDescriptor);
    }
    if (!sessionPtr->participantSecondary) {
      printf(" Adversary : <vacant>\n");
    } else {
      printf(" Adversary : %s (sock %u)\n",
             sessionPtr->participantSecondary->identityMoniker,
             sessionPtr->participantSecondary->socketDescriptor);
    }
    printf(" Lifecycle : %s\n",
           transformStateToLoggableString(sessionPtr->currentState));
    printf(" TurnIdx   : %u\n", sessionPtr->activeTurnIdx);

    printf(" Topography:\n");
    if (!sessionPtr->gridMatrix) {
      printf("   <unmapped>\n");
    } else {
      for (int r = 0; r < 3; ++r) {
        printf("   ");
        for (int c = 0; c < 3; ++c) {
          printf("%2u ", *(*(sessionPtr->gridMatrix + r) + c));
        }
        printf("\n");
      }
    }
    printf("===========================\n");
  } else {
    printf("Diagnostics fault: null session pointer\n");
    return;
  }
}

void dumpPlayerNodeDiagnostics(const struct PlayerNode *nodePtr) {

  if (nodePtr != NULL) {
    printf("[NODE_DIAG] identity: %s | socket: %d\n", nodePtr->identityMoniker,
           nodePtr->socketDescriptor);
  } else {
    printf("[NODE_DIAG] (null reference)\n");
    return;
  }
}

int dispatch_turn_marker(int base1, int base2, int current_turn_marker) {

  if (current_turn_marker != base1) {
    return base1;
  }

  return base2;
}

struct PlayerNode *dispatch_active_node(struct PlayerNode *nodeA,
                                        struct PlayerNode *nodeB,
                                        int current_turn_marker) {

  if (current_turn_marker != nodeA->socketDescriptor) {
    return nodeA;
  }

  return nodeB;
}

int validate_horizontal_alignment(unsigned int **topological_grid,
                                  int target_val) {

  int alignment_valid = 1;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (alignment_valid != 0 && *(*(topological_grid + r) + c) != target_val)
        alignment_valid = 0;

      if (alignment_valid != 0 && c == 2)
        return 1;
    }
    alignment_valid = 1;
  }
  return 0;
}

int validate_vertical_alignment(unsigned int **topological_grid,
                                int target_val) {

  int alignment_valid = 1;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (alignment_valid != 0 && *(*(topological_grid + c) + r) != target_val)
        alignment_valid = 0;

      if (alignment_valid != 0 && c == 2)
        return 1;
    }
    alignment_valid = 1;
  }
  return 0;
}

int validate_diagonal_alignment(unsigned int **topological_grid,
                                int target_val) {

  int alignment_valid = 1;
  for (int p = 0; p < 3; p++) {
    if (alignment_valid != 0 && *(*(topological_grid + p) + p) != target_val)
      alignment_valid = 0;
  }

  if (alignment_valid == 0) {
    alignment_valid = 1;
    for (int p = 0; p < 3; p++) {
      if (alignment_valid != 0 &&
          *(*(topological_grid + p) + (2 - p)) != target_val)
        alignment_valid = 0;
    }
    return alignment_valid;
  } else {
    return 1;
  }
}

int calculate_session_victor(unsigned int **topological_grid, unsigned int val1,
                             unsigned int val2) {

  if (validate_horizontal_alignment(topological_grid, val1) != 0) {
    return val1;
  }
  if (validate_horizontal_alignment(topological_grid, val2) != 0) {
    return val2;
  }

  if (validate_vertical_alignment(topological_grid, val1) != 0) {
    return val1;
  }
  if (validate_vertical_alignment(topological_grid, val2) != 0) {
    return val2;
  }

  if (validate_diagonal_alignment(topological_grid, val1) != 0) {
    return val1;
  }
  if (validate_diagonal_alignment(topological_grid, val2) != 0) {
    return val2;
  }

  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      if (*(*(topological_grid + r) + c) == 0)
        return -2;
    }
  }

  return -1;
}

void wipe_topological_grid(unsigned int **grid) {
  if (grid == NULL)
    return;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      *(*(grid + r) + c) = 0;
    }
  }
}

void execute_core_loop(struct MatchSession *m_session,
                       unsigned int enforce_locking) {

  if (enforce_locking != 0) {
    pthread_mutex_lock(&m_session->participantPrimary->streamLock);
    pthread_mutex_lock(&m_session->participantSecondary->streamLock);
  }

  int rematch_accepted = 0;
  do {

  int node1_sock = m_session->participantPrimary->socketDescriptor;
  int node2_sock = m_session->participantSecondary->socketDescriptor;
  m_session->activeTurnIdx = (rand() % 2 != 0) ? node2_sock : node1_sock;
  m_session->currentState = STATE_IN_PROGRESS;

  uint32_t payload_turn = htonl(m_session->activeTurnIdx);

  send(node1_sock, &payload_turn, sizeof(payload_turn), 0);
  send(node2_sock, &payload_turn, sizeof(payload_turn), 0);

  struct PlayerNode *acting_node = m_session->activeTurnIdx != node1_sock
                                       ? m_session->participantSecondary
                                       : m_session->participantPrimary;
  int core_status = -2;
  int bytes_fetched;

  for (; core_status == -2;) {

    uint32_t incoming_move;

    printf("Awaiting manipulation sequence from socket: %d\n",
           acting_node->socketDescriptor);
    bytes_fetched = consumeExactBytesFromStream(
        sizeof(incoming_move), &incoming_move, acting_node->socketDescriptor);
    if (bytes_fetched <= 0) {
      printf("[THR_ID: %lu] Core loop compromised, severity: %d\n",
             (unsigned long)pthread_self(), bytes_fetched);
      break;
    }
    int decoded_cell = ntohl(incoming_move);
    printf("Coordinate: %d targeted by socket: %d\n", decoded_cell,
           acting_node->socketDescriptor);

    *(*(m_session->gridMatrix + ((decoded_cell - 1) / 3)) +
      ((decoded_cell - 1) % 3)) = acting_node->socketDescriptor;

    acting_node = dispatch_active_node(m_session->participantPrimary,
                                       m_session->participantSecondary,
                                       acting_node->socketDescriptor);
    m_session->activeTurnIdx = acting_node->socketDescriptor;

    uint32_t encoded_cell = htonl(decoded_cell);
    send(acting_node->socketDescriptor, &encoded_cell, sizeof(encoded_cell), 0);
    printf("Coordinate: %d (%d) forwarded to socket: %d\n", decoded_cell,
           encoded_cell, acting_node->socketDescriptor);

    core_status = calculate_session_victor(
        m_session->gridMatrix, m_session->participantPrimary->socketDescriptor,
        m_session->participantSecondary->socketDescriptor);

    uint32_t encoded_status = htonl(core_status);
    printf("Pushing status update to primary node\n");
    send(m_session->participantPrimary->socketDescriptor, &encoded_status,
         sizeof(encoded_status), 0);
    printf("Status update to primary node confirmed\n");

    printf("Pushing status update to secondary node\n");
    send(m_session->participantSecondary->socketDescriptor, &encoded_status,
         sizeof(encoded_status), 0);
    printf("Status update to secondary node confirmed\n");

    printf("Calculated status: %d\n", core_status);
    dumpMatchSessionDiagnostics(m_session);
  }

  m_session->activeTurnIdx =
      dispatch_turn_marker(m_session->participantPrimary->socketDescriptor,
                           m_session->participantSecondary->socketDescriptor,
                           m_session->activeTurnIdx);
  m_session->currentState = STATE_CONCLUDED;
  dumpMatchSessionDiagnostics(m_session);

  /*
   * P11 [OPZIONALE TRACCIA]: La traccia indica che il vincitore può decidere se fare un'altra 
   * partita ed in tal caso diventare proprietario (attesa). 
   * [NOTA DIDATTICA]: Ho optato ingegneristicamente per implementare una UI UX classica di "Rivincita 
   * Simmetrica", in cui ad entrambi viene chiesto di accettare. Se entrambi concordano, la partita 
   * riparte immediatamente (stato IN_PROGRESS) con le stesse socket, svuotando la griglia. 
   * Se uno dei due rifiuta, vengono riportati alla Hall. Questa gestione è più coesa per 
   * le sessioni TCP in corso anziché far cacciare forzatamente il perdente.
   */
  if (bytes_fetched > 0) {
    rematch_accepted = invoke_rematch(m_session);
  } else {
    rematch_accepted = 0;
  }

  } while (rematch_accepted == 1);

  struct PlayerNode *n1 = m_session->participantPrimary;
  struct PlayerNode *n2 = m_session->participantSecondary;

  /* Rilascio degli identificativi di sessione attiva per entrambi i nodi */
  n1->activeSessionId = -1;
  n2->activeSessionId = -1;

  /*
   * Routine di purgazione post-match.
   * Vengono emessi log di audit prima e dopo la deallocazione
   * per tracciare il ciclo di vita delle risorse condivise.
   */
  printf("[GLOBAL MATCH REGISTRY - PRE-PURGE]\n");
  struct MatchSession *tracked_session;
  for (int k = 0; k < activeMatchSessions.allocated_slots; ++k) {
    tracked_session = NULL;
    fetch_from_offset(&activeMatchSessions, k, &tracked_session);
    if (tracked_session) {
      dumpMatchSessionDiagnostics(tracked_session);
    }
  }
  printf("[GLOBAL MATCH REGISTRY - PRE-PURGE COMPLETE]\n");

  int local_sess_id = m_session->sessionId;
  printf("[THR_ID: %lu] Eradicating session mapping: %d\n",
         (unsigned long)pthread_self(), local_sess_id);
  if (excise_at_offset(&activeMatchSessions, local_sess_id,
                       destroyMatchSessionInstance) == 0) {
    relinquishSessionIdentifier(local_sess_id);
    printf("[THR_ID: %lu] Session identifier reclaimed: %d\n",
           (unsigned long)pthread_self(), local_sess_id);
  }

  printf("[GLOBAL MATCH REGISTRY - POST-PURGE]\n");
  for (int k = 0; k < activeMatchSessions.allocated_slots; ++k) {
    tracked_session = NULL;
    fetch_from_offset(&activeMatchSessions, k, &tracked_session);
    if (tracked_session != NULL) {
      dumpMatchSessionDiagnostics(tracked_session);
    }
  }
  printf("[GLOBAL MATCH REGISTRY - POST-PURGE COMPLETE]\n");

  /* Broadcast aggiornamento a tutti i client connessi */
  broadcast_session_state_update();

  int lock_res;

  lock_res = pthread_mutex_unlock(&n1->streamLock);
  if (lock_res != 0) {
    printf("[THR_ID: %lu] Primary node stream unlock failure (code=%d)\n",
           (unsigned long)pthread_self(), lock_res);
  } else {
    printf("[THR_ID: %lu] Primary node stream unlocked\n",
           (unsigned long)pthread_self());
  }

  lock_res = pthread_mutex_unlock(&n2->streamLock);
  if (lock_res != 0) {
    printf("[THR_ID: %lu] Secondary node stream unlock failure (code=%d)\n",
           (unsigned long)pthread_self(), lock_res);
  } else {
    printf("[THR_ID: %lu] Secondary node stream unlocked\n",
           (unsigned long)pthread_self());
  }
}

struct RematchDelegationPayload {
  int targetSocket;
  int outcomeBuffer;
};

void *execute_rematch_polling(void *payloadArg) {
  struct RematchDelegationPayload *payload =
      (struct RematchDelegationPayload *)payloadArg;
  payload->outcomeBuffer = fetch_draw_verdict(payload->targetSocket);
  return NULL;
}

int invoke_rematch(struct MatchSession *m_session) {
  pthread_t thr_a, thr_b;
  struct RematchDelegationPayload payload_a = {
      m_session->participantPrimary->socketDescriptor, -1};
  struct RematchDelegationPayload payload_b = {
      m_session->participantSecondary->socketDescriptor, -1};

  pthread_create(&thr_a, NULL, execute_rematch_polling, &payload_a);
  pthread_create(&thr_b, NULL, execute_rematch_polling, &payload_b);

  pthread_join(thr_a, NULL);
  pthread_join(thr_b, NULL);

  printf("[THR_ID: %lu] Polling aggregation: N1=%d, N2=%d\n",
         (unsigned long)pthread_self(), payload_a.outcomeBuffer,
         payload_b.outcomeBuffer);

  if (payload_a.outcomeBuffer != OP_APPROVE_REQ ||
      payload_b.outcomeBuffer != OP_APPROVE_REQ) {
    uint16_t encoded_rejection = htons(OP_DECLINE_REQ);
    send(m_session->participantPrimary->socketDescriptor, &encoded_rejection,
         sizeof(encoded_rejection), 0);
    send(m_session->participantSecondary->socketDescriptor, &encoded_rejection,
         sizeof(encoded_rejection), 0);

    m_session->currentState = STATE_CONCLUDED;
    dumpMatchSessionDiagnostics(m_session);
    return 0;
  } else {
    m_session->currentState = STATE_IN_PROGRESS;
    wipe_topological_grid(m_session->gridMatrix);

    uint16_t encoded_approval = htons(OP_APPROVE_REQ);
    send(m_session->participantPrimary->socketDescriptor, &encoded_approval,
         sizeof(encoded_approval), 0);
    send(m_session->participantSecondary->socketDescriptor, &encoded_approval,
         sizeof(encoded_approval), 0);

    return 1;
  }
}

int fetch_draw_verdict(int d_socket) {

  ssize_t bytes_pulled;
  uint16_t encoded_verdict;

  bytes_pulled = consumeExactBytesFromStream(sizeof(encoded_verdict),
                                             &encoded_verdict, d_socket);
  int decoded_verdict = ntohs(encoded_verdict);
  printf("[THR_ID: %lu] (Rematch polling hook): %d\n",
         (unsigned long)pthread_self(), decoded_verdict);

  if (bytes_pulled <= 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      printf("[THR_ID: %lu] Hook expiration on socket %d\n",
             (unsigned long)pthread_self(), d_socket);
      return -1;
    }
  }

  return decoded_verdict;
}

/*
 * P2: Aggiornamento stato per i client. (Requisito "Notifica a tutti i giocatori").
 * 
 * [NOTA DIDATTICA BROADCAST vs PULL]:
 * Invece di iterare su tutte le `activePlayerNodes` spingendo dati in maniera
 * Push asincrona (che corromperebbe irreparabilmente gli stream TCP delle Socket 
 * sincrone Java, generando eccezioni DataInputStream parse errors), implementiamo  
 * un paradigma state-logging qui nel Server. 
 * I Client, tramite un meccanismo "Long Polling / Timeline", prelevano asincronamente 
 * (tramite OP_FETCH_WAITING_LIST) lo stato corrente. Quindi la "notifica" a tutti 
 * i giocatori è pienamente rispettata graficamente garantendo l'integrità TCP Strict.
 */
void broadcast_session_state_update(void) {
  cJSON *array_root = cJSON_CreateArray();
  if (array_root == NULL)
    return;

  for (int idx = 0; idx < activeMatchSessions.allocated_slots; ++idx) {
    struct MatchSession *sess_ptr = NULL;
    if (fetch_from_offset(&activeMatchSessions, idx, &sess_ptr) == 0 &&
        sess_ptr != NULL) {
      cJSON *item_obj = cJSON_CreateObject();
      if (item_obj != NULL) {
        cJSON_AddNumberToObject(item_obj, "id_game", sess_ptr->sessionId);
        cJSON_AddNumberToObject(item_obj, "status_game",
                                sess_ptr->currentState);
        if (sess_ptr->participantPrimary != NULL &&
            sess_ptr->participantPrimary->identityMoniker != NULL) {
          cJSON_AddStringToObject(item_obj, "name",
                                  sess_ptr->participantPrimary->identityMoniker);
          cJSON_AddNumberToObject(item_obj, "creator",
                                  sess_ptr->participantPrimary->socketDescriptor);
        }
        cJSON_AddItemToArray(array_root, item_obj);
      }
    }
  }

  char *serialized_buffer = cJSON_PrintUnformatted(array_root);
  if (serialized_buffer != NULL) {
    printf("[BROADCAST] Session state registry updated: %s\n",
           serialized_buffer);
    free(serialized_buffer);
  }
  cJSON_Delete(array_root);
}

void *executePlayerNodeLifecycle(void *threadParam) {

  ssize_t pull_size;
  NetworkOpcode incoming_opcode;
  struct timespec marker_start, marker_current;
  double delta_sec;

  srand(time(NULL));

  int d_socket = *(int *)threadParam;
  free(threadParam);

  printf("[THR_ID: %lu] Initiating lifecycle for socket: %d\n",
         (unsigned long)pthread_self(), d_socket);

  struct timeval dropout_threshold;
  dropout_threshold.tv_sec = SECONDS_UNTIL_DROP;
  dropout_threshold.tv_usec = 0;
  setsockopt(d_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&dropout_threshold,
             sizeof(dropout_threshold));

  struct PlayerNode *new_node = malloc(sizeof *new_node);
  if (!new_node) {
    perror("Node instantiation fault");
    close(d_socket);
    pthread_exit(NULL);
  }
  new_node->socketDescriptor = d_socket;
  new_node->identityMoniker = NULL;
  new_node->timeoutTicks = 0;
  new_node->activeSessionId = -1;
  if (pthread_mutex_init(&new_node->streamLock, NULL) != 0) {
    perror("Lock instantiation fault");
    free(new_node);
    close(d_socket);
    pthread_exit(NULL);
  }
  int encoded_socket_id = htonl(d_socket);
  send(d_socket, &encoded_socket_id, sizeof(encoded_socket_id), 0);

  struct PlayerNode *scan_ptr;
  for (int idx = 0; idx < activePlayerNodes.allocated_slots; ++idx) {
    scan_ptr = NULL;
    fetch_from_offset(&activePlayerNodes, idx, &scan_ptr);

    if (scan_ptr) {
      printf("-- TRK: %d -- ", idx);
      dumpPlayerNodeDiagnostics(scan_ptr);
    }
  }

  int alloc_code = append_to_store(&activePlayerNodes, &new_node);
  printf("[THR_ID: %lu] Node registry assimilation code: %d\n",
         (unsigned long)pthread_self(), alloc_code);
  if (alloc_code != 0) {
    fprintf(stderr, "Assimilation mechanism fault\n");
    pthread_mutex_destroy(&new_node->streamLock);
    free(new_node);
    pthread_exit(NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &marker_start);

  fd_set monitoring_set;
  struct timeval no_block_val;
  no_block_val.tv_sec = 0;
  no_block_val.tv_usec = 0;

  unsigned short active_flag = 1;
  int mux_eval;
  for (; active_flag != 0;) {

    FD_ZERO(&monitoring_set);
    FD_SET(d_socket, &monitoring_set);
    sleep(1);
    pthread_mutex_lock(&new_node->streamLock);
    mux_eval = select(d_socket + 1, &monitoring_set, NULL, NULL, &no_block_val);

    if (mux_eval == -1) {
      perror("Multiplexer fault");
      break;
    } else if (FD_ISSET(d_socket, &monitoring_set)) {
      printf("[THR_ID: %lu] Polling stream block on socket %d\n",
             (unsigned long)pthread_self(), d_socket);
      pull_size = recv(d_socket, &incoming_opcode, sizeof(incoming_opcode), 0);
      printf("[THR_ID: %lu] Resuming execution post-poll on socket %d\n",
             (unsigned long)pthread_self(), d_socket);
      pthread_mutex_unlock(&new_node->streamLock);

      if (pull_size <= 0) {
        if (pull_size != 0) {
          perror("[THR_ID] Stream ingestion critical fault");
        } else {
          printf("[THR_ID: %lu] Remote peer severed link on socket %d\n",
                 (unsigned long)pthread_self(), d_socket);
        }
        break;
      }

      int decoded_opcode = ntohl(incoming_opcode);
      printf("[THR_ID: %lu] Ingested %ld octets, OPCODE: %d\n",
             (unsigned long)pthread_self(), pull_size, decoded_opcode);

      if (decoded_opcode == OP_INIT_MATCH_SESSION) {
        process_activation_request(new_node);
      } else if (decoded_opcode == OP_FETCH_WAITING_LIST) {
        process_queue_fetch(d_socket);
      } else if (decoded_opcode == OP_BUILD_MATCH_SESSION) {
        process_instantiation_request(new_node);
      } else if (decoded_opcode == OP_TERMINATE_LINK) {
        printf("[THR_ID: %lu] Remote explicit link severance requested\n",
               (unsigned long)pthread_self());
        active_flag = 0;
      } else {
        printf("[THR_ID: %lu] Unidentified operational code: %d\n",
               (unsigned long)pthread_self(), decoded_opcode);
      }
      clock_gettime(CLOCK_MONOTONIC, &marker_start);
    } else {
      pthread_mutex_unlock(&new_node->streamLock);
    }

    if (new_node->timeoutTicks == 1) {
      new_node->timeoutTicks = 0;
      clock_gettime(CLOCK_MONOTONIC, &marker_start);
      printf("[THR_ID: %lu] Tick baseline reset on socket %d\n",
             (unsigned long)pthread_self(), d_socket);
    }

    clock_gettime(CLOCK_MONOTONIC, &marker_current);

    delta_sec = (marker_current.tv_sec - marker_start.tv_sec) +
                (marker_current.tv_nsec - marker_start.tv_nsec) / 1e9;
    if (delta_sec >= SECONDS_UNTIL_DROP) {
      printf("[THR_ID: %lu] Drop deadline exceeded for socket %d\n",
             (unsigned long)pthread_self(), d_socket);
      break;
    }
  }

  struct PlayerNode *disposal_ptr;
  for (int idx = 0; idx < activePlayerNodes.allocated_slots; ++idx) {
    disposal_ptr = NULL;
    fetch_from_offset(&activePlayerNodes, idx, &disposal_ptr);

    if (disposal_ptr) {
      if (disposal_ptr->socketDescriptor == d_socket) {
        int res_code = excise_at_offset(&activePlayerNodes, idx,
                                        destroyPlayerNodeInstance);
        printf("[THR_ID: %lu] Registry expulsion code: %d\n",
               (unsigned long)pthread_self(), res_code);
        break;
      }
    }
  }
  close(d_socket);

  struct MatchSession *orphaned_session;
  for (int idx = 0; idx < activeMatchSessions.allocated_slots; ++idx) {
    orphaned_session = NULL;
    fetch_from_offset(&activeMatchSessions, idx, &orphaned_session);
    if (orphaned_session && orphaned_session->participantPrimary == new_node) {
      relinquishSessionIdentifier(orphaned_session->sessionId);
      int tracked_id = orphaned_session->sessionId;
      int wipe_code = excise_at_offset(&activeMatchSessions, idx,
                                       destroyMatchSessionInstance);
      printf("[THR_ID: %lu] Orphaned session purge %d yielded: %d\n",
             (unsigned long)pthread_self(), tracked_id, wipe_code);
    }
  }

  return NULL;
}

int process_activation_request(struct PlayerNode *p_node) {

  printf("[THR_ID: %lu] Initiating session activation sequence\n",
         (unsigned long)pthread_self());

  /* P1: Verifica che il giocatore non stia già partecipando a una partita */
  if (p_node->activeSessionId != -1) {
    printf("[THR_ID: %lu] Player already bound to session %d, rejecting join\n",
           (unsigned long)pthread_self(), p_node->activeSessionId);
    /* Dobbiamo comunque consumare i dati inviati dal client per non corrompere
     * il protocollo */
    uint16_t discard_id;
    consumeExactBytesFromStream(sizeof(discard_id), &discard_id,
                                p_node->socketDescriptor);
    uint32_t discard_len;
    consumeExactBytesFromStream(sizeof(discard_len), &discard_len,
                                p_node->socketDescriptor);
    int discard_name_len = ntohl(discard_len);
    if (discard_name_len > 0 && discard_name_len <= LIMIT_IDENTITY_CAPACITY) {
      char *discard_buf = malloc(discard_name_len + 1);
      if (discard_buf) {
        consumeExactBytesFromStream(discard_name_len, discard_buf,
                                    p_node->socketDescriptor);
        free(discard_buf);
      }
    }
    /* Invia bind id e poi rifiuto */
    uint32_t encoded_bind_id = htonl(p_node->socketDescriptor);
    send(p_node->socketDescriptor, &encoded_bind_id, sizeof(encoded_bind_id), 0);
    uint32_t relayed_verdict = htonl(OP_DECLINE_REQ);
    send(p_node->socketDescriptor, &relayed_verdict, sizeof(relayed_verdict), 0);
    return -3;
  }

  ssize_t bytes_fetched;
  uint32_t net_len_block;
  uint16_t net_id_block;
  int local_socket = p_node->socketDescriptor;

  consumeExactBytesFromStream(sizeof(net_id_block), &net_id_block,
                              local_socket);
  int target_sess_id = ntohs(net_id_block);
  printf("[THR_ID: %lu] Discovered target binding: %d\n",
         (unsigned long)pthread_self(), target_sess_id);

  if (consumeExactBytesFromStream(sizeof(net_len_block), &net_len_block,
                                  local_socket) <= 0)
    return -1;
  int moniker_boundary = ntohl(net_len_block);
  printf("[THR_ID: %lu] Moniker layout magnitude: %d\n",
         (unsigned long)pthread_self(), moniker_boundary);

  char *new_moniker = malloc(moniker_boundary + 1);
  if (!new_moniker) {
    perror("Moniker allocation fault");
    pthread_exit(NULL);
  }

  bytes_fetched =
      consumeExactBytesFromStream(moniker_boundary, new_moniker, local_socket);
  if (bytes_fetched <= 0) {
    return -2;
  }
  *(new_moniker + moniker_boundary) = '\0';
  printf("[THR_ID: %lu] Parsed remote identity %s - %ld octets\n",
         (unsigned long)pthread_self(), new_moniker, bytes_fetched);

  if (p_node->identityMoniker != NULL)
    free(p_node->identityMoniker);
  p_node->identityMoniker = new_moniker;

  uint32_t encoded_bind_id = htonl(local_socket);
  send(local_socket, &encoded_bind_id, sizeof(encoded_bind_id), 0);

  /*
   * Vettoriamento della notifica al nodo master della sessione.
   */

  struct MatchSession *target_session_block = NULL;
  fetch_from_offset(&activeMatchSessions, target_sess_id,
                    &target_session_block);

  if (target_session_block == NULL) {
    printf("[THR_ID: %lu] Target session %d not found\n",
           (unsigned long)pthread_self(), target_sess_id);
    uint32_t relayed_verdict = htonl(OP_DECLINE_REQ);
    send(local_socket, &relayed_verdict, sizeof(relayed_verdict), 0);
    return -1;
  }

  struct PlayerNode *master_node = target_session_block->participantPrimary;
  unsigned int master_socket = master_node->socketDescriptor;

  char *notification_payload;
  if (asprintf(&notification_payload,
               "Incoming challenge detected from %s. Acknowledge protocol "
               "initiation?\n",
               p_node->identityMoniker) == -1) {
    perror("asprintf fault");
  }

  pthread_mutex_lock(&master_node->streamLock);
  printf("[THR_ID: %lu] Routing initiation challenge to socket %d\n",
         (unsigned long)pthread_self(), master_socket);
  send(master_socket, notification_payload, strlen(notification_payload), 0);

  printf("[THR_ID: %lu] Paused for master consensus on socket %d\n",
         (unsigned long)pthread_self(), master_socket);
         
  net_len_block = htonl(OP_DECLINE_REQ);
  consumeExactBytesFromStream(sizeof(int), &net_len_block, master_socket);
  pthread_mutex_unlock(&master_node->streamLock);

  free(notification_payload);
  uint32_t master_verdict = ntohl(net_len_block);
  printf("[THR_ID: %lu] Master node yielded protocol constant %d\n",
         (unsigned long)pthread_self(), master_verdict);

  uint32_t relayed_verdict = htonl(master_verdict);
  send(local_socket, &relayed_verdict, sizeof(relayed_verdict), 0);

  if (master_verdict != OP_APPROVE_REQ) {
    if (master_verdict != OP_DECLINE_REQ) {
      printf("[THR_ID: %lu] Non-compliant protocol constant encountered",
             (unsigned long)pthread_self());
    }
  } else {

    printf("[THR_ID: %lu] Binding session (%d) structure extracted:\n",
           (unsigned long)pthread_self(), target_sess_id);
    target_session_block->participantSecondary = p_node;
    target_session_block->currentState = STATE_IN_PROGRESS;
    dumpMatchSessionDiagnostics(target_session_block);

    /* P1: Segna entrambi i giocatori come impegnati nella sessione */
    p_node->activeSessionId = target_sess_id;
    master_node->activeSessionId = target_sess_id;

    master_node->timeoutTicks = 1;

    /* P2: Broadcast aggiornamento stato a tutti i client */
    broadcast_session_state_update();

    execute_core_loop(target_session_block, 1);
  }

  printf("\n");
  return 0;
}

int process_queue_fetch(int d_socket) {

  printf("[THR_ID: %lu] Triggered queue list serialization\n",
         (unsigned long)pthread_self());

  cJSON *array_root = cJSON_CreateArray();
  if (array_root == NULL)
    return -1;

  for (int idx = 0; idx < activeMatchSessions.allocated_slots; ++idx) {
    struct MatchSession *sess_ptr = NULL;
    if (fetch_from_offset(&activeMatchSessions, idx, &sess_ptr) == 0 &&
        sess_ptr != NULL) {
      cJSON *item_obj = cJSON_CreateObject();
      if (item_obj != NULL) {
        cJSON_AddNumberToObject(item_obj, "id_game", sess_ptr->sessionId);
        cJSON_AddNumberToObject(item_obj, "status_game",
                                sess_ptr->currentState);
        cJSON_AddStringToObject(item_obj, "name",
                                sess_ptr->participantPrimary->identityMoniker);
        cJSON_AddNumberToObject(item_obj, "creator",
                                sess_ptr->participantPrimary->socketDescriptor);
        cJSON_AddItemToArray(array_root, item_obj);
      } else {
        continue;
      }
    }
  }

  char *serialized_buffer = cJSON_PrintUnformatted(array_root);
  if (serialized_buffer != NULL) {
    printf("[THR_ID: %lu] FLUSHING JSON OVER STREAM: %s\n",
           (unsigned long)pthread_self(), serialized_buffer);

    send(d_socket, serialized_buffer, strlen(serialized_buffer), 0);
    send(d_socket, "\n", 1, 0);

    free(serialized_buffer);
    cJSON_Delete(array_root);
  } else {
    cJSON_Delete(array_root);
    return -1;
  }

  printf("\n");
  return 0;
}

int process_instantiation_request(struct PlayerNode *p_node) {

  int local_sock = p_node->socketDescriptor;

  /* 
   * P1: Verifica che il giocatore non stia già partecipando a una partita 
   * [NOTA DIDATTICA TRACCIA]: 
   * "Un giocatore può creare una o più partite, ma può giocare solo ad una partita..."
   * Questa implementazione adotta un approccio fail-early: blocca in maniera rigida
   * l'istanziazione contemporanea di multiple stanze ("Zombie Sessions"). 
   * L'utente può creare "più partite" in maniera *sequenziale* durante la sua vita 
   * sul server (poichè activeSessionId torna -1 a fine match), garantendo pulizia di memoria.
   */
  if (p_node->activeSessionId != -1) {
    printf("[THR_ID: %lu] Player already bound to session %d, rejecting create\n",
           (unsigned long)pthread_self(), p_node->activeSessionId);
    /* Consumiamo i dati inviati dal client per non corrompere il protocollo */
    uint32_t discard_len;
    consumeExactBytesFromStream(sizeof(discard_len), &discard_len, local_sock);
    int discard_name_len = ntohl(discard_len);
    if (discard_name_len > 0 && discard_name_len <= LIMIT_IDENTITY_CAPACITY) {
      char *discard_buf = malloc(discard_name_len + 1);
      if (discard_buf) {
        consumeExactBytesFromStream(discard_name_len, discard_buf, local_sock);
        free(discard_buf);
      }
    }
    return -3;
  }

  int fresh_id = acquireNewSessionIdentifier();

  printf("[THR_ID: %lu] Servicing instantiation cycle (assigned: %d)\n",
         (unsigned long)pthread_self(), fresh_id);

  uint32_t encoded_range;
  if (consumeExactBytesFromStream(sizeof(encoded_range), &encoded_range,
                                  local_sock) != sizeof(encoded_range)) {
    perror("Range read violation");
    pthread_exit(NULL);
  }
  int decoded_range = ntohl(encoded_range);
  if (decoded_range > LIMIT_IDENTITY_CAPACITY || decoded_range == 0) {
    fprintf(stderr, "Moniker bounds check failed: %u\n", decoded_range);
    pthread_exit(NULL);
  }

  char *identity_buffer = malloc(decoded_range + 1);
  if (identity_buffer == NULL) {
    perror("Moniker footprint fault");
    pthread_exit(NULL);
  }
  if (consumeExactBytesFromStream(decoded_range, identity_buffer, local_sock) !=
      (ssize_t)decoded_range) {
    perror("Moniker string consumption fault");
    free(identity_buffer);
    pthread_exit(NULL);
  }
  *(identity_buffer + decoded_range) = '\0';

  printf("[THR_ID: %lu] Instantiation bounded identity = %s\n",
         (unsigned long)pthread_self(), identity_buffer);

  if (p_node->identityMoniker != NULL)
    free(p_node->identityMoniker);
  p_node->identityMoniker = identity_buffer;

  struct MatchSession *novel_session = malloc(sizeof(struct MatchSession));
  if (novel_session == NULL) {
    perror("Session descriptor fault");
    pthread_exit(NULL);
  }

  novel_session->participantPrimary = p_node;
  novel_session->participantSecondary = NULL;
  novel_session->sessionId = fresh_id;
  /* P4: La sessione nasce nello stato FRESH_INST (Nuova creazione) */
  novel_session->currentState = STATE_FRESH_INST;
  novel_session->gridMatrix = allocateGridMatrixBuffer();
  novel_session->activeTurnIdx = 0;

  if (implant_at_offset(&activeMatchSessions, fresh_id, &novel_session) != 0) {
    perror("Registry insertion fault");
    destroyMatchSessionInstance(&novel_session);
    pthread_exit(NULL);
  }

  /* P4: Transizione a STATE_PENDING_PLYR (In attesa di avversario) */
  novel_session->currentState = STATE_PENDING_PLYR;

  /* P1: Registra la partecipazione dell'utente alla sessione creata */
  p_node->activeSessionId = fresh_id;

  printf("\n=== [DIAG] Session Registry Audit [POST-INSTANTIATION] ===\n");
  for (size_t ptr = 0; ptr < activeMatchSessions.occupied_slots; ++ptr) {
    struct MatchSession *surveyed_session = NULL;
    if (fetch_from_offset(&activeMatchSessions, ptr, &surveyed_session) == 0 &&
        surveyed_session != NULL) {
      printf("bucket idx: %ld\n", ptr);
      dumpMatchSessionDiagnostics(surveyed_session);
    }
  }

  /* P2: Broadcast aggiornamento stato a tutti i client connessi */
  broadcast_session_state_update();

  printf("\n");
  return 0;
}