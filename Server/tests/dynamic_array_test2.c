#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include "../dynamic_array.h"

#define SIZE 5

int main() {

    DynamicArray da;

    if (da_init(&da, SIZE, sizeof(int)) != 0) {
        printf("Errore inizializzazione\n");
        return 1;
    }
    printf("Init -> size=%zu capacity=%zu\n", da.size, da.capacity);


    // Inseriamo x elementi (così capacity raddoppia almeno una volta)
    int inserts = 11;
    for (int i = 0; i < inserts; i++) {
        da_push(&da, &i);
        printf("Inserito index %d -> size=%zu capacity=%zu\n", i, da.size, da.capacity);
    }
    printf("Dopo %d insert -> size=%zu capacity=%zu\n", inserts, da.size, da.capacity);


    for (int i = da.capacity; i>=0; --i){
        int pre_cap = da.capacity;
        da_remove_at(&da, i, NULL);
        printf("Rimosso index %d -> size=%zu capacity=%zu\n", i, da.size, da.capacity);
        if (da.capacity != pre_cap) printf("(trigger shrink)\n");

    }

    printf("\n\n");
    inserts = 5;
    for (int i = 0; i < inserts; i++) {
        da_push(&da, &i);
        printf("Inserito index %d -> size=%zu capacity=%zu\n", i, da.size, da.capacity);
    }
    printf("Dopo %d insert -> size=%zu capacity=%zu\n", inserts, da.size, da.capacity);

    printf("insert for increse capacity\n");
    int data = 100;
    da_push(&da, &data);
    printf("Dopo insert -> size=%zu capacity=%zu\n", da.size, da.capacity);

    srand(time(NULL));
    int index;
    for (int i = da.size; i>=0; --i){
        int pre_cap = da.capacity;
        index = rand()%da.size;
        da_remove_at(&da, index, NULL);
        printf("Rimosso index %d -> size=%zu capacity=%zu\n", index, da.size, da.capacity);
        if (da.capacity != pre_cap) printf("(trigger shrink)\n");
    }

    da_free(&da, NULL);
    return 0;
}
