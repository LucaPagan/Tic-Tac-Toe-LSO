#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "../dynamic_array.h"

#define INITIAL_SIZE 5

int main()
{
    printf("--- Running test_dynamic_array_capacity ---\n");
    DynamicArray da;

    assert(da_init(&da, INITIAL_SIZE, sizeof(int)) == 0);
    assert(da.capacity == INITIAL_SIZE);
    assert(da.size == 0);

    // Inseriamo x elementi per forzare l'aumento della capacità
    int inserts = 11;
    for (int i = 0; i < inserts; i++)
    {
        assert(da_push(&da, &i) == 0);
    }

    // La capacità deve essere almeno raddoppiata
    assert(da.size == inserts);
    assert(da.capacity >= INITIAL_SIZE * 2);

    // Rimuoviamo elementi per testare un eventuale rimpicciolimento (shrink)
    for (int i = da.size - 1; i >= 0; --i)
    {
        assert(da_remove_at(&da, i, NULL) == 0);
    }

    assert(da.size == 0);

    // Test di stress casuale per verificare che non crashi
    srand((unsigned int)time(NULL));
    int more_inserts = 20;
    for (int i = 0; i < more_inserts; i++)
    {
        assert(da_push(&da, &i) == 0);
    }

    while (da.size > 0)
    {
        int index = rand() % da.size;
        assert(da_remove_at(&da, index, NULL) == 0);
    }

    da_free(&da, NULL);
    printf("All capacity tests passed.\n\n");
    return 0;
}