#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int check_row(unsigned short int **campo, int p)
{
    int flag = 1;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (flag == 1 && campo[i][j] != p)
                flag = 0;
            if (j == 2 && flag == 1)
                return 1;
        }
        flag = 1;
    }
    return 0;
}

int check_col(unsigned short int **campo, int p)
{
    int flag = 1;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (flag == 1 && campo[j][i] != p)
                flag = 0;
            if (j == 2 && flag == 1)
                return 1;
        }
        flag = 1;
    }
    return 0;
}

int check_diag(unsigned short int **campo, int p)
{
    int flag = 1;
    for (int i = 0; i < 3; i++)
    {
        if (flag == 1 && campo[i][i] != p)
            flag = 0;
    }
    if (flag == 1)
        return 1;

    flag = 1;
    for (int i = 0; i < 3; i++)
    {
        if (flag == 1 && campo[i][2 - i] != p)
            flag = 0;
    }
    return flag;
}

int the_winner_is(unsigned short int **campo, unsigned int g1, unsigned int g2)
{
    if (check_row(campo, g1) == 1)
        return g1;
    if (check_row(campo, g2) == 1)
        return g2;

    if (check_col(campo, g1) == 1)
        return g1;
    if (check_col(campo, g2) == 1)
        return g2;

    if (check_diag(campo, g1) == 1)
        return g1;
    if (check_diag(campo, g2) == 1)
        return g2;

    // Check for incomplete field
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (campo[i][j] == 0)
                return -2; // Incomplete field
        }
    }

    return -1; // Pareggio (Draw)
}

unsigned short int **init_table()
{
    unsigned short int **table = malloc(3 * sizeof(*table));
    if (table == NULL)
    {
        perror("Alloc table");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 3; ++i)
    {
        table[i] = calloc(3, sizeof(*table[i]));
        if (table[i] == NULL)
        {
            for (int j = 0; j < i; ++j)
                free(table[j]);
            free(table);
            perror("Alloc table");
            exit(EXIT_FAILURE);
        }
    }
    return table;
}

void free_table(unsigned short int **table)
{
    for (int i = 0; i < 3; ++i)
        free(table[i]);
    free(table);
}

int main()
{
    printf("--- Running test_tris_logic ---\n");
    unsigned short int **table;
    const unsigned short int X = 1;
    const unsigned short int O = 2;

    // Test: Riga vincente
    table = init_table();
    for (int j = 0; j < 3; ++j)
        table[0][j] = X;
    assert(check_row(table, X) == 1); // Corretto: deve essere 1 (vero)
    assert(check_row(table, O) == 0);
    assert(the_winner_is(table, X, O) == X);
    free_table(table);

    // Test: Colonna vincente
    table = init_table();
    for (int i = 0; i < 3; ++i)
        table[i][2] = O;
    assert(check_col(table, O) == 1);
    assert(check_col(table, X) == 0);
    assert(the_winner_is(table, X, O) == O);
    free_table(table);

    // Test: Diagonale principale vincente
    table = init_table();
    for (int i = 0; i < 3; ++i)
        table[i][i] = X;
    assert(check_diag(table, X) == 1);
    assert(check_diag(table, O) == 0);
    assert(the_winner_is(table, X, O) == X);
    free_table(table);

    // Test: Diagonale secondaria vincente
    table = init_table();
    for (int i = 0; i < 3; ++i)
        table[i][2 - i] = O;
    assert(check_diag(table, O) == 1);
    assert(the_winner_is(table, X, O) == O);
    free_table(table);

    // Test: Nessun vincitore (Pareggio / Draw)
    table = init_table();
    table[0][0] = X;
    table[0][1] = O;
    table[0][2] = X;
    table[1][0] = X;
    table[1][1] = X;
    table[1][2] = O;
    table[2][0] = O;
    table[2][1] = X;
    table[2][2] = O;
    assert(check_row(table, X) == 0);
    assert(check_col(table, O) == 0);
    assert(check_diag(table, X) == 0);
    assert(the_winner_is(table, X, O) == -1); // Deve restituire -1 (pareggio)
    free_table(table);

    // Test: Partita non ancora finita
    table = init_table();
    table[0][0] = X;
    assert(the_winner_is(table, X, O) == -2); // Deve restituire -2 (incompleta)
    free_table(table);

    printf("All tris logic tests passed!\n\n");
    return 0;
}