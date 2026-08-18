#include <stdio.h>
#include <stdlib.h>
#include <kissat.h>
#include <time.h>
#include "rgsmlclib.h"

int main() 
{
    int rows, cols, clause_count = 0, found_solution = 0;

    // Lê as dimensões do tabuleiro
    if (scanf("%d %d", &rows, &cols) != 2) {
        fprintf(stderr, "Error: Failed to read grid dimensions.\n");
        return EXIT_FAILURE;
    }

    if (rows <= 0 || cols <= 0) {
        fprintf(stderr, "Error: Grid dimensions must be positive.\n");
        return EXIT_FAILURE;
    }

    size_t cell_count = (size_t) rows * (size_t) cols;

    int *target_grid, *best_predecessor, *live_cells, **clauses = NULL;

    // Alocando espaço para o tabuleiro alvo
    target_grid = malloc(cell_count * sizeof *target_grid);

    if (target_grid == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for the target grid.\n");
        return EXIT_FAILURE;
    }

    // Lê o tabuleiro alvo
    for (int row = 0; row < rows; row++) 
    {
        for (int col = 0; col < cols; col++) 
        {
            int cell;

            if (scanf("%d", &cell) != 1) {
                fprintf(stderr, "Error: Failed to read grid cell.\n");
                free(target_grid);
                return EXIT_FAILURE;
            }

            if (cell != 0 && cell != 1) {
                fprintf(stderr, "Error: Grid cells must contain only 0 or 1.\n");
                free(target_grid);
                return EXIT_FAILURE;
            }

            target_grid[row * cols + col] = cell;
        }
    }

    // Verifica se todas as células da borda estão mortas
    for (int row = 0; row < rows; row++) 
    {
        if (target_grid[row * cols] != 0 || target_grid[row * cols + (cols - 1)] != 0) {
            fprintf(stderr, "Error: Border cells must be dead.\n");
            free(target_grid);
            return EXIT_FAILURE;
        }
    }

    for (int col = 0; col < cols; col++) 
    {
        if (target_grid[col] != 0 || target_grid[(rows - 1) * cols + col] != 0) {
            fprintf(stderr, "Error: Border cells must be dead.\n");
            free(target_grid);
            return EXIT_FAILURE;
        }
    }

    // Alocando espaço para o tabuleiro resposta
    best_predecessor = calloc(cell_count, sizeof *best_predecessor);

    if (best_predecessor == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for the predecessor grid.\n");
        free(target_grid);
        return EXIT_FAILURE;
    }

    // Alocando espaço para o vetor de células vivas
    live_cells = malloc(cell_count * sizeof *live_cells);

    if (live_cells == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for the live-cell buffer.\n");
        free(target_grid);
        free(best_predecessor);
        return EXIT_FAILURE;
    }

    // Cria as cláusulas CNF que representam os predecessores válidos do tabuleiro alvo
    build_predecessor_cnf(target_grid, &clauses, &clause_count, rows, cols);

    fprintf(stderr, "Searching for a minimum predecessor...\n");

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
        printf("UNSAT\n");
    } else {
        printf("%d %d\n", rows, cols);
        print_grid(best_predecessor, rows, cols);
    }

    free(target_grid);
    free(best_predecessor);
    free(live_cells);

    // Libera todas as cláusulas e a matriz que as armazena
    free_clauses(clauses, clause_count);

    return EXIT_SUCCESS;
}