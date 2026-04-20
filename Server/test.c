#include <stdio.h>
#include "server.h"

#include "queue_list.h"
// #include "server.c"

int gen_id_print(){
    
    int id = generate_id();
    printf("[d] id: %d\n", id);
    return id;

}

int main(){

struct queue q;
    // 1) Inizializzo la coda per contenere interi (sizeof(int) byte)
    init_queue(&q, sizeof(int));

    // 2) Inserisco tre valori nella coda
    int v1 = 10, v2 = 20, v3 = 30;
    insert_queue(&q, &v1);
    insert_queue(&q, &v2);
    insert_queue(&q, &v3);

    printf("Coda di int inizializzata e popolata.\n");

    // 3) Rimuovo l’elemento in testa (indice 0)
    void *out_ptr = NULL;
    if (remove_at_queue(&q, 2, &out_ptr) == 0) {
        // out_ptr punta a un blocco di sizeof(int) contenente il valore
        int extracted = *(int *)out_ptr;
        printf("Ho estratto (int) dalla coda: %d\n", extracted);
    } else {
        printf("Errore: impossibile rimuovere elemento indice 0.\n");
    }

    // 4) Provo a estrarre di nuovo (dovrebbero restare due elementi)
    if (remove_at_queue(&q, 1, &out_ptr) == 0) {
        int extracted = *(int *)out_ptr;
        printf("Ho estratto (int) dalla coda: %d\n", extracted);
    } else {
        printf("Errore: impossibile rimuovere elemento indice 0.\n");
    }

    // 5) Estrai l’ultimo rimasto
    if (remove_at_queue(&q, 0, &out_ptr) == 0) {
        int extracted = *(int *)out_ptr;
        printf("Ho estratto (int) dalla coda: %d\n", extracted);
    } else {
        printf("Errore: impossibile rimuovere elemento indice 0.\n");
    }

    // 6) Ora la coda è vuota, provo a rimuovere ancora
    if (remove_at_queue(&q, 0, &out_ptr) != 0) {
        printf("Coda vuota, non ci sono più int da estrarre.\n");
    }

    // TEST ID

    int id0 = gen_id_print();
    int id1 = gen_id_print();

    release_id(id0);

    int id01 = gen_id_print();
    int id2 = gen_id_print();

    // free queue

}