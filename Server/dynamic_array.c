#include "dynamic_array.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expand_storage_if_required(DynamicArray *store_ref);
static int evaluate_compaction(DynamicArray *store_ref, size_t excised_idx);

int configure_memory_store(DynamicArray *store_ref, size_t starter_slots,
                           size_t footprint) {

  if (store_ref != NULL && footprint != 0) {

    store_ref->element_byte_footprint = footprint;
    store_ref->occupied_slots = 0;

    if (starter_slots <= 0) {
      store_ref->floor_threshold = 1;
    } else {
      store_ref->floor_threshold = starter_slots;
    }

    if (starter_slots <= 0) {
      store_ref->heap_region = NULL;
      store_ref->allocated_slots = 0;
    } else {
      store_ref->heap_region = calloc(starter_slots, footprint);
      if (store_ref->heap_region != NULL) {
        store_ref->allocated_slots = starter_slots;
      } else {
        return -1;
      }
    }

    if (pthread_mutex_init(&store_ref->synchronizer, NULL) == 0) {
      return 0;
    } else {
      if (store_ref->heap_region == NULL) {

      } else {
        free(store_ref->heap_region);
      }
      store_ref->heap_region = NULL;
      store_ref->allocated_slots = 0;
      return -1;
    }

  } else {
    return -1;
  }
}

static int expand_storage_if_required(DynamicArray *store_ref) {

  if (store_ref->occupied_slots >= store_ref->allocated_slots) {

    size_t next_cap;
    if (store_ref->allocated_slots != 0) {
      if (store_ref->allocated_slots <= SIZE_MAX / 2) {
        next_cap = store_ref->allocated_slots * 2;
      } else {
        return -1;
      }
    } else {
      next_cap = store_ref->floor_threshold;
    }

    if (store_ref->element_byte_footprint == 0 ||
        next_cap <= SIZE_MAX / store_ref->element_byte_footprint) {

      size_t previous_cap = store_ref->allocated_slots;
      void *relocated_block = realloc(
          store_ref->heap_region, next_cap * store_ref->element_byte_footprint);

      if (relocated_block != NULL) {
        store_ref->heap_region = relocated_block;

        if (next_cap <= previous_cap) {

        } else {
          size_t delta_bytes =
              (next_cap - previous_cap) * store_ref->element_byte_footprint;
          memset((char *)store_ref->heap_region +
                     previous_cap * store_ref->element_byte_footprint,
                 0, delta_bytes);
        }

        store_ref->allocated_slots = next_cap;
        return 0;

      } else {
        return -1;
      }

    } else {
      return -1;
    }

  } else {
    return 0;
  }
}

int append_to_store(DynamicArray *store_ref, const void *payload_ptr) {

  if (store_ref != NULL && payload_ptr != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (expand_storage_if_required(store_ref) == 0) {

        memcpy((char *)store_ref->heap_region +
                   store_ref->occupied_slots *
                       store_ref->element_byte_footprint,
               payload_ptr, store_ref->element_byte_footprint);
        store_ref->occupied_slots++;

        pthread_mutex_unlock(&store_ref->synchronizer);
        return 0;

      } else {
        pthread_mutex_unlock(&store_ref->synchronizer);
        return -1;
      }

    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int extract_tail_from_store(DynamicArray *store_ref, void *receiver_ptr) {
  if (store_ref != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (store_ref->occupied_slots != 0) {

        store_ref->occupied_slots--;
        void *src =
            (char *)store_ref->heap_region +
            store_ref->occupied_slots * store_ref->element_byte_footprint;
        if (receiver_ptr == NULL) {

        } else {
          memcpy(receiver_ptr, src, store_ref->element_byte_footprint);
        }

        evaluate_compaction(store_ref, store_ref->occupied_slots);

        pthread_mutex_unlock(&store_ref->synchronizer);
        return 0;

      } else {
        pthread_mutex_unlock(&store_ref->synchronizer);
        return -1;
      }

    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int implant_at_offset(DynamicArray *store_ref, size_t offset_idx,
                      const void *payload_ptr) {

  if (store_ref != NULL && payload_ptr != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (offset_idx <= store_ref->allocated_slots) {

        if (expand_storage_if_required(store_ref) == 0) {

          char *base_ptr = (char *)store_ref->heap_region;
          size_t mem_offset = offset_idx * store_ref->element_byte_footprint;

          memcpy(base_ptr + mem_offset, payload_ptr,
                 store_ref->element_byte_footprint);

          store_ref->occupied_slots++;

          pthread_mutex_unlock(&store_ref->synchronizer);
          return 0;

        } else {
          pthread_mutex_unlock(&store_ref->synchronizer);
          return -1;
        }

      } else {
        pthread_mutex_unlock(&store_ref->synchronizer);
        return -1;
      }

    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int fetch_from_offset(DynamicArray *store_ref, size_t offset_idx,
                      void *receiver_ptr) {

  if (store_ref != NULL && receiver_ptr != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (offset_idx < store_ref->allocated_slots) {

        memcpy(receiver_ptr,
               (char *)store_ref->heap_region +
                   offset_idx * store_ref->element_byte_footprint,
               store_ref->element_byte_footprint);

        pthread_mutex_unlock(&store_ref->synchronizer);
        return 0;

      } else {
        pthread_mutex_unlock(&store_ref->synchronizer);
        return -1;
      }

    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int excise_at_offset(DynamicArray *store_ref, size_t offset_idx,
                     void (*cleanup_hook)(void *)) {

  if (store_ref != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (offset_idx < store_ref->allocated_slots) {

        char *base_ptr = (char *)store_ref->heap_region;
        void *target_ptr =
            base_ptr + offset_idx * store_ref->element_byte_footprint;

        if (cleanup_hook == NULL) {

        } else {
          cleanup_hook(target_ptr);
        }

        memset(target_ptr, 0, store_ref->element_byte_footprint);

        store_ref->occupied_slots--;

        evaluate_compaction(store_ref, offset_idx);

        pthread_mutex_unlock(&store_ref->synchronizer);
        return 0;

      } else {
        pthread_mutex_unlock(&store_ref->synchronizer);
        return -1;
      }

    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

static int evaluate_compaction(DynamicArray *store_ref, size_t excised_idx) {
  if (store_ref != NULL && store_ref->allocated_slots != 0) {

    if (excised_idx == (store_ref->allocated_slots / 2) - 1) {
      size_t reduced_cap = store_ref->allocated_slots / 2;
      if (reduced_cap >= store_ref->floor_threshold) {

      } else {
        reduced_cap = store_ref->floor_threshold;
      }

      int compaction_allowed = 1;
      for (size_t k = reduced_cap; k < store_ref->allocated_slots; ++k) {
        void *elem_p = (char *)store_ref->heap_region +
                       k * store_ref->element_byte_footprint;
        void *empty_ref = NULL;
        if (memcmp(elem_p, &empty_ref, store_ref->element_byte_footprint) ==
            0) {

        } else {
          compaction_allowed = 0;
          break;
        }
      }

      if (compaction_allowed != 0) {

        void *realloc_block =
            realloc(store_ref->heap_region,
                    reduced_cap * store_ref->element_byte_footprint);
        if (realloc_block != NULL) {
          store_ref->heap_region = realloc_block;
          store_ref->allocated_slots = reduced_cap;
          return 0;
        } else {
          return -1;
        }

      } else {
        return 0;
      }

    } else {
      return 0;
    }

  } else {
    return -1;
  }
}

void demolish_memory_store(DynamicArray *store_ref,
                           void (*cleanup_hook)(void *)) {

  if (store_ref != NULL) {
    if (pthread_mutex_lock(&store_ref->synchronizer) == 0) {

      if (store_ref->heap_region != NULL && cleanup_hook != NULL) {
        for (size_t c = 0; c < store_ref->allocated_slots; ++c) {
          void *ptr = (char *)store_ref->heap_region +
                      c * store_ref->element_byte_footprint;
          cleanup_hook(ptr);
        }
      } else {
      }

      free(store_ref->heap_region);
      store_ref->heap_region = NULL;
      store_ref->element_byte_footprint = 0;
      store_ref->occupied_slots = 0;
      store_ref->allocated_slots = 0;
      store_ref->floor_threshold = 0;

      pthread_mutex_unlock(&store_ref->synchronizer);
      pthread_mutex_destroy(&store_ref->synchronizer);

    } else {
      return;
    }
  } else {
    return;
  }
}