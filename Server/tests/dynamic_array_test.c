// tests/test_dynamic_array.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include "../dynamic_array.h"

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED: %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

static void print_ok(const char *name) {
    printf("[OK] %s\n", name);
}

/* Test 1: init, push, pop basic con int */
static void test_push_pop_basic(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 2, sizeof(int)) == 0, "da_init");
    int v;

    v = 10; ASSERT(da_push(&da, &v) == 0, "push 10");
    v = 20; ASSERT(da_push(&da, &v) == 0, "push 20");
    ASSERT(da.size == 2, "size after two pushes");

    int out;
    ASSERT(da_pop(&da, &out) == 0, "pop ok"); ASSERT(out == 20, "pop value 20");
    ASSERT(da_pop(&da, &out) == 0, "pop ok 2"); ASSERT(out == 10, "pop value 10");
    ASSERT(da_pop(&da, &out) == -1, "pop empty");

    da_free(&da, NULL);
    print_ok("test_push_pop_basic");
}

/* Test 2: insert_at and order */
static void test_insert_at(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 1, sizeof(int)) == 0, "da_init insert");
    int v;

    v = 1; da_push(&da, &v);
    v = 3; da_push(&da, &v);
    v = 2; ASSERT(da_insert_at(&da, 1, &v) == 0, "insert at 1");

    ASSERT(da.size == 3, "size after insert");
    int *arr = (int*)da.data;
    ASSERT(arr[0] == 1 && arr[1] == 2 && arr[2] == 3, "order 1,2,3");

    da_free(&da, NULL);
    print_ok("test_insert_at");
}

/* Test 3: remove_at with callback that records removed value */
static int removed_val;
static void remove_callback(void *elem) {
    if (!elem) return;
    removed_val = *(int*)elem;
}
static void test_remove_at_callback(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 2, sizeof(int)) == 0, "da_init remove");
    int v;
    v = 10; da_push(&da, &v);
    v = 20; da_push(&da, &v);
    v = 30; da_push(&da, &v);

    ASSERT(da_remove_at(&da, 1, remove_callback) == 0, "remove_at index 1");
    ASSERT(removed_val == 20, "callback got 20");
    int *arr = (int*)da.data;
    ASSERT(da.size == 2, "size after remove");
    ASSERT(arr[0] == 10 && arr[1] == 30, "array now 10,30");

    /* out of range */
    ASSERT(da_remove_at(&da, 5, remove_callback) == -1, "remove out of range");

    da_free(&da, NULL);
    print_ok("test_remove_at_callback");
}

/* Test 4: da_free with free_callback for pointer elements */
static void free_callback(void *elem_ptr) {
    /* elem_ptr is pointer-sized slot holding a heap pointer */
    void *p = *(void**)elem_ptr;
    if (p) free(p);
}
static void test_free_callback(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 2, sizeof(void*)) == 0, "da_init ptrs");

    for (int i = 0; i < 5; ++i) {
        int *p = malloc(sizeof(int));
        *p = i * 10;
        da_push(&da, &p);
    }
    ASSERT(da.size == 5, "pushed 5 pointers");

    /* free with callback should free the pointees too */
    da_free(&da, free_callback);
    /* after free, da.size should be 0 */
    ASSERT(da.size == 0, "size after free");
    print_ok("test_free_callback");
}

/* Test 5: invalid inserts/removes */
static void test_invalid_indices(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 1, sizeof(int)) == 0, "da_init invalid");
    int v = 1;
    ASSERT(da_insert_at(&da, 2, &v) == -1, "insert_at > size returns -1");
    ASSERT(da_remove_at(&da, 0, NULL) == -1, "remove_at empty returns -1");
    da_free(&da, NULL);
    print_ok("test_invalid_indices");
}

/* Test 6: multithreaded push */
#define THREADS 4
#define PER_THREAD 10000

static atomic_int global_counter;
typedef struct { DynamicArray *da; } tharg_t;
static void *pusher_thread(void *arg) {
    tharg_t *a = (tharg_t*)arg;
    for (int i = 0; i < PER_THREAD; ++i) {
        int val = atomic_fetch_add(&global_counter, 1);
        if (da_push(a->da, &val) != 0) {
            fprintf(stderr, "push failed in thread\n");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}
static void test_multithread_push(void) {
    DynamicArray da;
    ASSERT(da_init(&da, 1, sizeof(int)) == 0, "da_init mt");
    atomic_init(&global_counter, 0);

    pthread_t th[THREADS];
    tharg_t arg = { .da = &da };

    for (int i = 0; i < THREADS; ++i) {
        if (pthread_create(&th[i], NULL, pusher_thread, &arg) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < THREADS; ++i) pthread_join(th[i], NULL);

    int total = THREADS * PER_THREAD;
    ASSERT((int)da.size == total, "size equals total pushed");

    /* verify values are permutation of 0..total-1 */
    char *seen = calloc(total, 1);
    ASSERT(seen != NULL, "calloc seen");
    for (size_t i = 0; i < da.size; ++i) {
        int v = ((int*)da.data)[i];
        if (v < 0 || v >= total) {
            free(seen);
            da_free(&da, NULL);
            ASSERT(0, "value out of expected range");
        }
        seen[v] = 1;
    }
    for (int i = 0; i < total; ++i) {
        if (!seen[i]) {
            free(seen);
            da_free(&da, NULL);
            ASSERT(0, "missing pushed value");
        }
    }
    free(seen);
    da_free(&da, NULL);
    print_ok("test_multithread_push");
}

/* main: run tests */
int main(void) {
    test_push_pop_basic();
    test_insert_at();
    test_remove_at_callback();
    test_free_callback();
    test_invalid_indices();
    test_multithread_push();
    printf("All tests passed.\n");
    return 0;
}
