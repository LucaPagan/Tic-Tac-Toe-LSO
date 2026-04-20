#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
// #include "../dynamic_array.h"

#define MAX_CLIENTS 3

int next_id = 0;
int available_ids[MAX_CLIENTS];
int available_count = 0;
pthread_mutex_t gen_id_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_avaible(){
    printf("Available IDs: ");
    for(int i=0; i<MAX_CLIENTS; ++i){
        printf("%d ", available_ids[i]);
    }
    printf("\n");
}

int generate_id() {
    pthread_mutex_lock(&gen_id_mutex);
    
    int id;
    if (available_count > 0) {
        id = available_ids[--available_count];
    } else {
        id = next_id++;
    }
    
    pthread_mutex_unlock(&gen_id_mutex);
    return id;
}

void release_id(int id) {
    pthread_mutex_lock(&gen_id_mutex);
    if (available_count < MAX_CLIENTS) {
        available_ids[available_count++] = id;
    }
    pthread_mutex_unlock(&gen_id_mutex);
}

int main() {
    // Test ID generation and release
    int ids[5];

    print_avaible();
    
    // Generate IDs
    for (int i = 0; i < 5; i++) {
        ids[i] = generate_id();
        printf("Generated ID: %d\n", ids[i]);
    }

    print_avaible();
    
    // Release some IDs
    release_id(ids[1]);
    printf("Released ID: %d\n", ids[1]);

    release_id(ids[3]);
    printf("Released ID: %d\n", ids[3]);
    print_avaible();

    release_id(ids[2]);
    printf("Released ID: %d\n", ids[2]);
    print_avaible();

    release_id(ids[4]);
    printf("Released ID: %d\n", ids[4]);
    print_avaible();
    
    // Generate more IDs to see if released ones are reused
    for (int i = 0; i < 5; i++) {
        int new_id = generate_id();
        printf("Generated ID: %d\n", new_id);
    }
    
    return 0;
}