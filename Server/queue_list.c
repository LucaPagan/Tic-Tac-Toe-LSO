#include "queue_list.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bootstrap_chain_ledger(struct chain_ledger *ledger_ptr,
                            size_t blueprint_size) {

  ledger_ptr->payload_footprint = blueprint_size;
  ledger_ptr->tail_node = NULL;
  ledger_ptr->head_node = NULL;
  pthread_mutex_init(&ledger_ptr->access_lock, NULL);
}

void dump_chain_ledger(struct chain_ledger *ledger_ptr,
                       void (*render_hook)(const void *)) {

  pthread_mutex_lock(&ledger_ptr->access_lock);

  struct link_unit *traversal_ptr;
  for (traversal_ptr = ledger_ptr->head_node; traversal_ptr != NULL;
       traversal_ptr = traversal_ptr->successor_link) {
    printf("%p: fwd %p bwd %p payload:\n", (void *)traversal_ptr,
           (void *)traversal_ptr->successor_link,
           (void *)traversal_ptr->predecessor_link);

    if (ledger_ptr->payload_footprint != sizeof(void *)) {
      render_hook(traversal_ptr->payload_data);
    } else {
      void *decoded_reference;
      memcpy(&decoded_reference, traversal_ptr->payload_data, sizeof(void *));
      render_hook(decoded_reference);
    }

    printf("\n");
  }
  printf("\n");

  pthread_mutex_unlock(&ledger_ptr->access_lock);
}

void eradicate_chain_ledger(struct chain_ledger *ledger_ptr) {
  if (ledger_ptr != NULL) {
    pthread_mutex_lock(&ledger_ptr->access_lock);

    pthread_mutex_unlock(&ledger_ptr->access_lock);
  } else {
    return;
  }
}

int push_to_chain(struct chain_ledger *ledger_ptr, const void *payload_ptr) {
  if (ledger_ptr != NULL && payload_ptr != NULL) {
    pthread_mutex_lock(&ledger_ptr->access_lock);

    struct link_unit *fresh_link = malloc(sizeof(struct link_unit));
    if (fresh_link != NULL) {
      fresh_link->payload_data = malloc(ledger_ptr->payload_footprint);
      if (fresh_link->payload_data != NULL) {
        memcpy(fresh_link->payload_data, payload_ptr,
               ledger_ptr->payload_footprint);
        fresh_link->predecessor_link = NULL;
        fresh_link->successor_link = NULL;

        if (ledger_ptr->head_node != NULL) {
          fresh_link->predecessor_link = ledger_ptr->tail_node;
          ledger_ptr->tail_node->successor_link = fresh_link;
          ledger_ptr->tail_node = fresh_link;
        } else {
          ledger_ptr->tail_node = fresh_link;
          ledger_ptr->head_node = fresh_link;
        }

        pthread_mutex_unlock(&ledger_ptr->access_lock);
        return NORMAL_STATE;
      } else {
        free(fresh_link);
        pthread_mutex_unlock(&ledger_ptr->access_lock);
        return ALLOC_FAULT;
      }
    } else {
      pthread_mutex_unlock(&ledger_ptr->access_lock);
      return ALLOC_FAULT;
    }

  } else {
    return ALLOC_FAULT;
  }
}

int pop_from_chain(struct chain_ledger *ledger_ptr, void *receiver_ptr) {

  pthread_mutex_lock(&ledger_ptr->access_lock);

  if (ledger_ptr->head_node != NULL && ledger_ptr->tail_node != NULL) {

    struct link_unit *extraction_target = ledger_ptr->head_node;
    memcpy(receiver_ptr, extraction_target->payload_data,
           ledger_ptr->payload_footprint);
    ledger_ptr->head_node = ledger_ptr->head_node->successor_link;

    if (ledger_ptr->head_node == NULL) {
      ledger_ptr->tail_node = NULL;
    } else {
      ledger_ptr->head_node->predecessor_link = NULL;
    }

    free(extraction_target->payload_data);
    free(extraction_target);

    pthread_mutex_unlock(&ledger_ptr->access_lock);

    return NORMAL_STATE;

  } else {
    pthread_mutex_unlock(&ledger_ptr->access_lock);
    return VOID_STATE;
  }
}

int peek_chain(struct chain_ledger *ledger_ptr, void *receiver_ptr) {

  pthread_mutex_lock(&ledger_ptr->access_lock);

  if (ledger_ptr->head_node != NULL && ledger_ptr->tail_node != NULL) {
    memcpy(receiver_ptr, ledger_ptr->head_node->payload_data,
           ledger_ptr->payload_footprint);
    pthread_mutex_unlock(&ledger_ptr->access_lock);
    return NORMAL_STATE;
  } else {
    pthread_mutex_unlock(&ledger_ptr->access_lock);
    return VOID_STATE;
  }
}

int excise_tail_chain(struct chain_ledger *ledger_ptr, const void *target_ptr,
                      int (*eval_func)(const void *, const void *)) {

  pthread_mutex_lock(&ledger_ptr->access_lock);

  struct link_unit *scanner_ptr = ledger_ptr->head_node;
  for (; scanner_ptr != NULL;) {
    if (eval_func(scanner_ptr->payload_data, target_ptr) != 0) {
      scanner_ptr = scanner_ptr->successor_link;
    } else {
      if (scanner_ptr->predecessor_link == NULL) {
        ledger_ptr->head_node = scanner_ptr->successor_link;
      } else {
        scanner_ptr->predecessor_link->successor_link =
            scanner_ptr->successor_link;
      }

      if (scanner_ptr->successor_link == NULL) {
        ledger_ptr->tail_node = scanner_ptr->predecessor_link;
      } else {
        scanner_ptr->successor_link->predecessor_link =
            scanner_ptr->predecessor_link;
      }

      free(scanner_ptr->payload_data);
      free(scanner_ptr);
      pthread_mutex_unlock(&ledger_ptr->access_lock);
      return NORMAL_STATE;
    }
  }

  pthread_mutex_unlock(&ledger_ptr->access_lock);

  return VOID_STATE;
}

int excise_offset_chain(struct chain_ledger *ledger_ptr, const int offset_idx,
                        void **receiver_ptr) {
  if (ledger_ptr != NULL && receiver_ptr != NULL) {

    pthread_mutex_lock(&ledger_ptr->access_lock);

    struct link_unit *tracker_ptr = ledger_ptr->head_node;
    size_t stepper = 0;
    for (; tracker_ptr != NULL && stepper < (size_t)offset_idx;) {
      tracker_ptr = tracker_ptr->successor_link;
      stepper++;
    }

    if (tracker_ptr != NULL) {

      if (ledger_ptr->payload_footprint != sizeof(void *)) {
        *receiver_ptr = tracker_ptr->payload_data;
      } else {
        void *extracted_ref;
        memcpy(&extracted_ref, tracker_ptr->payload_data, sizeof(void *));
        *receiver_ptr = extracted_ref;
        free(tracker_ptr->payload_data);
      }

      if (tracker_ptr->predecessor_link == NULL) {
        ledger_ptr->head_node = tracker_ptr->successor_link;
      } else {
        tracker_ptr->predecessor_link->successor_link =
            tracker_ptr->successor_link;
      }

      if (tracker_ptr->successor_link == NULL) {
        ledger_ptr->tail_node = tracker_ptr->predecessor_link;
      } else {
        tracker_ptr->successor_link->predecessor_link =
            tracker_ptr->predecessor_link;
      }

      free(tracker_ptr);
      pthread_mutex_unlock(&ledger_ptr->access_lock);
      return 0;

    } else {
      pthread_mutex_unlock(&ledger_ptr->access_lock);
      return -1;
    }

  } else {
    return -1;
  }
}