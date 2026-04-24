#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#define MAX_CLIENTS 3

int next_id = 0;
int available_ids[MAX_CLIENTS];
int available_count = 0;
pthread_mutex_t gen_id_mutex = PTHREAD_MUTEX_INITIALIZER;

int generate_id()
{
    pthread_mutex_lock(&gen_id_mutex);

    int id;
    if (available_count > 0)
    {
        id = available_ids[--available_count];
    }
    else
    {
        id = next_id++;
    }

    pthread_mutex_unlock(&gen_id_mutex);
    return id;
}

void release_id(int id)
{
    pthread_mutex_lock(&gen_id_mutex);
    if (available_count < MAX_CLIENTS)
    {
        available_ids[available_count++] = id;
    }
    pthread_mutex_unlock(&gen_id_mutex);
}

int main()
{
    printf("--- Running test_gen_id ---\n");
    int ids[5];

    // Generate IDs
    for (int i = 0; i < 5; i++)
    {
        ids[i] = generate_id();
        assert(ids[i] == i); // Should sequence: 0, 1, 2, 3, 4
    }

    assert(next_id == 5);

    // Release some IDs
    release_id(ids[1]);
    release_id(ids[3]);

    // Check if released ones are reused correctly
    int new_id1 = generate_id();
    int new_id2 = generate_id();

    // Depending on stack logic (LIFO), order might be 3 then 1, or 1 then 3.
    assert((new_id1 == 3 && new_id2 == 1) || (new_id1 == 1 && new_id2 == 3));

    // Next ID should be a completely new one
    int new_id3 = generate_id();
    assert(new_id3 == 5);

    printf("All gen_id tests passed.\n\n");
    return 0;
}