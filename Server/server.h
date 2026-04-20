#ifndef CORE_MATCH_DISPATCHER_H_
#define CORE_MATCH_DISPATCHER_H_

#include <stddef.h>
#include <sys/types.h>

#include "dynamic_array.h"

#define LIMIT_IDENTITY_CAPACITY 64
#define LIMIT_PAYLOAD_CAPACITY 256

/*
 * La seguente enumerazione mappa in modo inequivocabile i segnali operativi
 * scambiati sul canale trasmissivo bidirezionale. È fondamentale preservare
 * l'ordine lessicografico dei valori associati per garantire la
 * retrocompatibilità degli artefatti serializzati.
 */
typedef enum {
  OP_SERVER_ACK = 0,
  OP_BUILD_MATCH_SESSION = 1,
  OP_FETCH_WAITING_LIST = 2,
  OP_INIT_MATCH_SESSION = 3,
  OP_TERMINATE_LINK = 4,
  OP_APPROVE_REQ = 5,
  OP_DECLINE_REQ = 6,
  OP_SESSION_STATE_UPDATE = 7
} NetworkOpcode;

typedef enum {
  STATE_CONCLUDED = 0,
  STATE_IN_PROGRESS = 1,
  STATE_PENDING_PLYR = 2,
  STATE_FRESH_INST = 3
} MatchSessionState;

struct MatchSessionAlert {
  MatchSessionState currentState;
  unsigned short int sessionId;
};

struct PlayerNode {
  unsigned short int timeoutTicks;
  unsigned int socketDescriptor;
  pthread_mutex_t streamLock;
  char *identityMoniker;
  int activeSessionId;
};

/*
 * L'infrastruttura sottostante adotta un paradigma di rappresentazione
 * sparpagliata in memoria, dove i puntatori transitano attraverso una griglia
 * di referenze dirette. L'ottimizzazione dell'allineamento è parzialmente
 * demandata al compilatore ma la coerenza transazionale permane responsabilità
 * del gestore di risorse di sistema.
 */
struct MatchSession {
  MatchSessionState currentState;
  unsigned int activeTurnIdx;
  struct PlayerNode *participantSecondary;
  unsigned int **gridMatrix;
  unsigned short int sessionId;
  struct PlayerNode *participantPrimary;
};

extern DynamicArray activePlayerNodes;

extern DynamicArray activeMatchSessions;

int acquireNewSessionIdentifier(void);
void relinquishSessionIdentifier(int identifierValue);

void purgePlayerNodeContext(void *contextPointer);

unsigned int **allocateGridMatrixBuffer(void);
void deallocateGridMatrixBuffer(unsigned int **matrixPtr);

void destroyMatchSessionInstance(void *instancePtr);

void destroyPlayerNodeInstance(void *instancePtr);

void broadcast_session_state_update(void);

ssize_t consumeExactBytesFromStream(size_t expectedBytes, void *payloadBuffer,
                                    int descriptor_socket);

const char *transformOpcodeToLoggableString(const NetworkOpcode opcodeValue);
const char *transformStateToLoggableString(const MatchSessionState stateValue);

void dumpMatchSessionDiagnostics(const struct MatchSession *sessionPtr);

void dumpPlayerNodeDiagnostics(const struct PlayerNode *nodePtr);

void *executePlayerNodeLifecycle(void *threadParam);

#endif