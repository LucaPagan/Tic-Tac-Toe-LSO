#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Prototipi delle funzioni da testare
int check_row(unsigned short int ** campo, int p){

    int flag=1;
    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            if(flag==1 && campo[i][j]!=p) flag=0;//controllo 1 riga

            if(j==2 && flag==1) return 1;

        }
        flag=1;
    }
    return 0;


}

void reset_table(unsigned short int **table) {
    if (!table) return;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            table[i][j] = 0;  
        }
    }
}


int check_col(unsigned short int ** campo, int p){

    int flag=1;
    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            if(flag==1 && campo[j][i]!=p) flag=0;//controllo 1 riga

            if(j==2 && flag==1) return 1;

        }
        flag=1;
    }
    return 0;

}

int check_diag(unsigned short int ** campo, int p){

    int flag=1;
    for(int i=0; i<3; i++){
        if(flag==1 && campo[i][i]!=p) flag=0;
    }

    if(flag==1) return 1;

    flag=1;
    for(int i=0; i<3; i++){
        if(flag==1 && campo[i][2-i]!=p) flag=0;
    }

    return flag;

}

int the_winner_is(unsigned short int ** campo, unsigned int g1, unsigned int g2){

    if(check_row(campo, g1)==1) {return g1;}
    if(check_row(campo, g2)==1) {return g2;}

    if(check_col(campo, g1)==1) {return g1;} 
    if(check_col(campo, g2)==1) {return g2;}

    if(check_diag(campo, g1)==1) {return g1;} 
    if(check_diag(campo, g2)==1) {return g2;}

    for(int i=0; i<9; ++i){
        if(campo[i]==0) return -2; // incomplete field
    }

    return -1; // pareggio

}

unsigned short int **init_tabel(){

    unsigned short int **table = malloc(3 * sizeof *table);
    if(table == NULL){ perror("Alloc table"); exit(EXIT_FAILURE);}

    for(int i=0; i<3; ++i){
        table[i] = calloc(3, sizeof *table[i]);
        if(table[i] == NULL){ 
            // Cleanup parziale in caso di errore
            for (int j = 0; j < i; ++j) {
                free(table[j]);
            }
            free(table);
            perror("Alloc table"); 
            exit(EXIT_FAILURE);}
    }

    return table;

}

void print_table(unsigned short int **table) {
    printf("Tabella di gioco:\n");
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            unsigned short int v = table[i][j];
            char c = v == 0 ? '.' : (v == 1 ? 'X' : 'O');
            printf(" %c", c);
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
    unsigned short int **table;
    int winner;
    const unsigned short int X = 1;
    const unsigned short int O = 2;

    // Test: righe
    table = init_tabel();
    for (int j = 0; j < 3; ++j) table[0][j] = X;
    print_table(table);
    printf("%d\n", check_row(table, X));
    printf("%d\n", check_row(table, O));
    printf("%d\n", the_winner_is(table, X, O));
    assert(check_row(table, X) == 0);
    assert(check_row(table, O) == 0);
    assert(the_winner_is(table, X, O) == X);
    for(int i = 0; i < 3; ++i) free(table[i]); free(table);

    // // Test: colonne
    // table = init_tabel();
    // for (int i = 0; i < 3; ++i) table[i][2] = O;
    // assert(check_col(table, O) == 1);
    // assert(check_col(table, X) == 0);
    // assert(the_winner_is(table, X, O) == O);
    // for(int i = 0; i < 3; ++i) free(table[i]); free(table);

    // // Test: diagonale principale
    // table = init_tabel();
    // for (int i = 0; i < 3; ++i) table[i][i] = X;
    // assert(check_diag(table, X) == 1);
    // assert(check_diag(table, O) == 0);
    // assert(the_winner_is(table, X, O) == X);
    // for(int i = 0; i < 3; ++i) free(table[i]); free(table);

    // // Test: diagonale secondaria
    // table = init_tabel();
    // for (int i = 0; i < 3; ++i) table[i][2-i] = O;
    // assert(check_diag(table, O) == 1);
    // assert(the_winner_is(table, X, O) == O);
    // for(int i = 0; i < 3; ++i) free(table[i]); free(table);

    // // Test: nessun vincitore (draw/parziale)
    // table = init_tabel();
    // table[0][0] = X; table[0][1] = O; table[0][2] = X;
    // table[1][0] = O; table[1][1] = X; table[1][2] = O;
    // table[2][0] = O; table[2][1] = X; table[2][ 2] = O;
    // assert(check_row(table, X) == 0);
    // assert(check_col(table, O) == 0);
    // assert(check_diag(table, X) == 0);
    // assert(the_winner_is(table, X, O) == -1);
    // for(int i = 0; i < 3; ++i) free(table[i]); free(table);

    printf("Tutti i test sono passati!\n");
    return 0;
}