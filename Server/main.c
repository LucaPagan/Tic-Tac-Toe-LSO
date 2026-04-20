#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <signal.h>

#include "dynamic_array.h"
#include "server.h"

#include <assert.h>

#define BIND_ADDRESS INADDR_ANY
#define BIND_PORT 5200

int global_listen_descriptor;

void interrupt_signal_routine(int signal_code) {
  if (global_listen_descriptor >= 0) {
    close(global_listen_descriptor);
    printf("Termination signal intercepted: listener stream severed.\n");
  }

  printf("\n=== [DIAGNOSTICS] Player Nodes Hierarchy ===\n");
  for (size_t iter_idx = 0; iter_idx < activePlayerNodes.allocated_slots;
       ++iter_idx) {
    struct PlayerNode *node_ptr = NULL;
    if (fetch_from_offset(&activePlayerNodes, iter_idx, &node_ptr) == 0 &&
        node_ptr != NULL) {
      dumpPlayerNodeDiagnostics(node_ptr);
    }
  }

  printf("\n=== [DIAGNOSTICS] Match Sessions Hierarchy ===\n");
  for (size_t iter_idx = 0; iter_idx < activeMatchSessions.allocated_slots;
       ++iter_idx) {
    struct MatchSession *session_ptr = NULL;
    if (fetch_from_offset(&activeMatchSessions, iter_idx, &session_ptr) == 0 &&
        session_ptr != NULL) {
      dumpMatchSessionDiagnostics(session_ptr);
    }
  }

  demolish_memory_store(&activeMatchSessions, destroyMatchSessionInstance);
  demolish_memory_store(&activePlayerNodes, destroyPlayerNodeInstance);

  exit(0);
}

int main(int argument_count, char *argument_vector[]) {

  int target_port = BIND_PORT;

  if (argument_count > 1) {
    target_port = atoi(argument_vector[1]);
    if (target_port <= 0 || target_port > 65535) {
      fprintf(stderr, "Out of bounds port value: %s\n", argument_vector[1]);
      exit(EXIT_FAILURE);
    }
  }

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  srand(time(NULL));

  signal(SIGINT, interrupt_signal_routine);

  int connection_descriptor;
  struct sockaddr_in transport_address;
  socklen_t transport_len;

  pthread_t handler_thread_id;

  global_listen_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (global_listen_descriptor == -1) {
    perror("Socket instantiation fault");
    exit(EXIT_FAILURE);
  }

  int reuse_flag = 1;
  if (setsockopt(global_listen_descriptor, SOL_SOCKET, SO_REUSEADDR,
                 &reuse_flag, sizeof(reuse_flag)) < 0) {
    perror("Reuse address directive fault");
  }
#ifdef SO_REUSEPORT
  if (setsockopt(global_listen_descriptor, SOL_SOCKET, SO_REUSEPORT,
                 &reuse_flag, sizeof(reuse_flag)) < 0) {
    perror("Reuse port directive fault");
  }
#endif

  memset(&transport_address, 0, sizeof(transport_address));
  transport_address.sin_family = AF_INET;
  transport_address.sin_addr.s_addr = htonl(BIND_ADDRESS);
  transport_address.sin_port = htons(target_port);

  if (bind(global_listen_descriptor, (struct sockaddr *)&transport_address,
           sizeof(transport_address)) < 0) {
    perror("Binding fault");
    exit(EXIT_FAILURE);
  }

  if (listen(global_listen_descriptor, 5) != 0) {
    perror("Listen directive fault");
    exit(EXIT_FAILURE);
  }

  printf("%s %d\n", "Service daemon activated on boundary port:", target_port);

  configure_memory_store(&activeMatchSessions, 3,
                         sizeof(struct MatchSession *));
  configure_memory_store(&activePlayerNodes, 6, sizeof(struct PlayerNode *));
  assert(activeMatchSessions.element_byte_footprint ==
         sizeof(struct MatchSession *));

  transport_len = sizeof(transport_address);
  for (; (connection_descriptor = accept(global_listen_descriptor,
                                         (struct sockaddr *)&transport_address,
                                         &transport_len)) > -1;) {

    int *descriptor_payload = malloc(sizeof(int));
    if (descriptor_payload == NULL) {
      perror("Descriptor allocation fault");
      close(connection_descriptor);
      continue;
    }
    *descriptor_payload = connection_descriptor;

    if (pthread_create(&handler_thread_id, NULL, executePlayerNodeLifecycle,
                       descriptor_payload) != 0) {
      perror("Lifecycle thread instantiation fault");
      free(descriptor_payload);
      close(connection_descriptor);
      continue;
    }

    pthread_detach(handler_thread_id);
  }

  demolish_memory_store(&activeMatchSessions, destroyMatchSessionInstance);
  demolish_memory_store(&activePlayerNodes, free);
  close(global_listen_descriptor);

  return 0;
}