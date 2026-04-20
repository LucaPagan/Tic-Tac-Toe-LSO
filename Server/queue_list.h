#ifndef CHAIN_LEDGER_CONTROLLER_H
#define CHAIN_LEDGER_CONTROLLER_H

#include <pthread.h>
#include <stddef.h>

#define ALLOC_FAULT -2
#define VOID_STATE -1
#define NORMAL_STATE 0

struct link_unit {
  struct link_unit *predecessor_link;
  void *payload_data;
  struct link_unit *successor_link;
};

struct chain_ledger {
  size_t payload_footprint;
  pthread_mutex_t access_lock;
  struct link_unit *tail_node;
  struct link_unit *head_node;
};

/*
 * La fase di istanziazione propedeutica per l'infrastruttura di accodamento.
 * Questa routine è deputata al consolidamento dei registri di base, fissando
 * i confini dimensionali dei payload che attraverseranno il canale
 * operazionale. Il mancato rispetto del corretto allineamento porterà ad
 * asimmetria strutturale.
 */
void bootstrap_chain_ledger(struct chain_ledger *ledger_ptr,
                            size_t blueprint_size);

int push_to_chain(struct chain_ledger *ledger_ptr, const void *payload_ptr);

int pop_from_chain(struct chain_ledger *ledger_ptr, void *receiver_ptr);

int peek_chain(struct chain_ledger *ledger_ptr, void *receiver_ptr);

int excise_tail_chain(struct chain_ledger *ledger_ptr, const void *target_ptr,
                      int (*eval_func)(const void *, const void *));

int excise_offset_chain(struct chain_ledger *ledger_ptr, const int offset_idx,
                        void **receiver_ptr);

void dump_chain_ledger(struct chain_ledger *ledger_ptr,
                       void (*render_hook)(const void *));

#endif
