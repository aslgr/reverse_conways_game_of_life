#include <stdio.h>
#include <stdlib.h>
#include <kissat.h>
#include <time.h>
#include "rgsmlclib.h"

int main() 
{
    int rows, cols, clause_count = 0, found_solution = 0;

    // Lendo os valores rows e cols
    scanf("%d %d", &rows, &cols);

    int *target_grid, *best_predecessor, *live_cells, **clauses = NULL;

    // Lendo o tabuleiro de entrada
    if((target_grid = (int *) malloc(rows * cols * sizeof(int))) != NULL) {
        for(int i = 0; i < rows; i++) {
            for(int j = 0; j < cols; j++) {
                scanf("%d", &target_grid[i * cols + j]);
            }
        }
    }
    else {
        fprintf(stderr, "Error: Failed to allocate memory for the target grid.\n");
        return 1;
    }

    // Alocando espaço para o tabuleiro resposta
    if((best_predecessor = (int *) malloc(rows * cols * sizeof(int))) != NULL) {
        for(int i = 0; i < rows; i++) {
            for(int j = 0; j < cols; j++) {
                best_predecessor[i * cols + j] = 0;
            }
        }
    }
    else {
        fprintf(stderr, "Error: Failed to allocate memory for the predecessor grid.\n");
        return 1;
    }

    // Alocando espaço para o vetor de células vivas
    if((live_cells = (int *) malloc(rows * cols * sizeof(int))) == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for the live-cell buffer.\n");
        return 1;
    }

    // Cria as cláusulas CNF que representam os predecessores válidos do tabuleiro alvo
    build_predecessor_cnf(target_grid, &clauses, &clause_count, rows, cols);

    printf("Searching for a minimum predecessor...\n");

    // Inicia o solver
    kissat *solver = kissat_init();
    // Silencia as mensagens do kissat
    kissat_set_option(solver, "quiet", 1);

    // Compartilha as cláusulas obtidas com o solver
    write_all_clauses(solver, clause_count, clauses);

    // Tenta encontrar uma solução válida
    int result = kissat_solve(solver);

    int min_live_cells = rows*cols;

    clock_t tempo_inicial = clock(); // Captura o tempo inicial
    double tempo_max = 300.0; // Tempo limite em segundos

    while (result == 10)
    {
        found_solution = 1;

        // Verificar se o tempo limite foi atingido
        double tempo = (double)(clock() - tempo_inicial) / CLOCKS_PER_SEC;
        if (tempo >= tempo_max) 
        {
            fprintf(stderr, "Warning: Time limit of %.2f seconds reached.\n", tempo_max);
            break;
        }

        int count_live_cells = 0;

        for (int i = 0; i < rows; i++) 
        {
            for (int j = 0; j < cols; j++) 
            {
                int variable = i * cols + j + 1; // Variável lógica 1-baseada
                int value = kissat_value(solver, variable); // Recupera o valor do solver

                if (value > 0)
                    live_cells[count_live_cells++] = value;
            }
        }

        if (count_live_cells < min_live_cells)
        {
            min_live_cells = count_live_cells;

            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    int variable = i * cols + j + 1; // Variável lógica 1-baseada
                    int value = kissat_value(solver, variable); // Recupera o valor do solver
                    best_predecessor[i * cols + j] = (value > 0) ? 1 : 0;
                }
            }
        }

        kissat_release(solver);

        // Inicia o solver
        solver = kissat_init();
        // Silencia as mensagens do kissat
        kissat_set_option(solver, "quiet", 1);

        add_model_blocking_clause(live_cells, count_live_cells, &clauses, &clause_count);

        write_all_clauses(solver, clause_count, clauses);

        result = kissat_solve(solver);
    }
    
    kissat_release(solver);

    if (!found_solution)
    {
        printf("\nUNSAT\n");
    } else {
        printf("\nResult:\n\n%d %d\n", rows, cols);
        print_grid(best_predecessor, rows, cols);
        printf("\n");
    }

    return 0;
}