#ifndef MEMORY_CONTIGUOUS_STORE_H
#define MEMORY_CONTIGUOUS_STORE_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
  size_t floor_threshold;
  size_t element_byte_footprint;
  pthread_mutex_t synchronizer;
  size_t allocated_slots;
  size_t occupied_slots;
  void *heap_region;
} DynamicArray;

int configure_memory_store(DynamicArray *store_ref, size_t starter_slots,
                           size_t footprint);

int append_to_store(DynamicArray *store_ref, const void *payload_ptr);

int extract_tail_from_store(DynamicArray *store_ref, void *receiver_ptr);

void demolish_memory_store(DynamicArray *store_ref,
                           void (*cleanup_hook)(void *));

int implant_at_offset(DynamicArray *store_ref, size_t offset_idx,
                      const void *payload_ptr);
int excise_at_offset(DynamicArray *store_ref, size_t offset_idx,
                     void (*cleanup_hook)(void *));
int fetch_from_offset(DynamicArray *store_ref, size_t offset_idx,
                      void *receiver_ptr);

#endif