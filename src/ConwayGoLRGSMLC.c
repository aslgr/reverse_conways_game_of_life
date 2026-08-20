#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <kissat.h>
#include "rgsmlclib.h"

enum {
    SAT_RESULT_UNKNOWN = 0,
    SAT_RESULT_SAT = 10,
    SAT_RESULT_UNSAT = 20,
    TIME_LIMIT_SECONDS = 300
};

static kissat *volatile active_solver = NULL;
static volatile sig_atomic_t timeout_reached = 0;

// Interrompe o solver ativo quando o limite de tempo é atingido
static void handle_timeout(int signal_number)
{
    (void) signal_number;

    timeout_reached = 1;

    if (active_solver != NULL)
        kissat_terminate(active_solver);
}

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

    // Configura o tratamento do limite de tempo
    if (signal(SIGALRM, handle_timeout) == SIG_ERR) {
        fprintf(stderr, "Error: Failed to configure timeout handler.\n");
        free(target_grid);
        free(best_predecessor);
        free(live_cells);
        free_clauses(clauses, clause_count);
        return EXIT_FAILURE;
    }

    alarm(TIME_LIMIT_SECONDS);

    fprintf(stderr, "Searching for a minimum predecessor...\n");

    // Inicia o solver
    kissat *solver = kissat_init();
    
    // Silencia as mensagens do kissat
    kissat_set_option(solver, "quiet", 1);

    // Compartilha as cláusulas obtidas com o solver
    write_all_clauses(solver, clause_count, clauses);

    // Tenta encontrar uma solução válida
    active_solver = solver;
    int result = kissat_solve(solver);
    active_solver = NULL;

    int min_live_cells = rows*cols;

    while (result == SAT_RESULT_SAT)
    {
        found_solution = 1;

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

        // Interrompe a otimização caso o limite tenha sido atingido após processar
        // a solução atual
        if (timeout_reached) {
            result = SAT_RESULT_UNKNOWN;
            break;
        }

        kissat_release(solver);

        solver = kissat_init();

        kissat_set_option(solver, "quiet", 1);

        add_model_blocking_clause(live_cells, count_live_cells, &clauses, &clause_count);

        write_all_clauses(solver, clause_count, clauses);

        // Evita iniciar uma nova busca caso o limite tenha sido atingido entre iterações
        if (timeout_reached) {
            result = SAT_RESULT_UNKNOWN;
            break;
        }

        active_solver = solver;
        result = kissat_solve(solver);
        active_solver = NULL;
    }

    alarm(0);
    active_solver = NULL;
    
    kissat_release(solver);

    int exit_status = EXIT_SUCCESS;

    if (!found_solution) {
        if (timeout_reached) {
            fprintf(stderr, "Error: Time limit reached before a predecessor was found.\n");
            exit_status = EXIT_FAILURE;
        } else if (result == SAT_RESULT_UNSAT) {
            printf("UNSAT\n");
        } else {
            fprintf(stderr, "Error: SAT solver returned an unknown result.\n");
            exit_status = EXIT_FAILURE;
        }
    } else if (!is_valid_predecessor(best_predecessor, target_grid, rows, cols)) {
        fprintf(stderr, "Error: Solver produced an invalid predecessor.\n");
        exit_status = EXIT_FAILURE;
    } else {
        if (timeout_reached)
            fprintf(stderr, "Warning: Time limit reached before optimality was proven.\n");

        printf("%d %d\n", rows, cols);
        print_grid(best_predecessor, rows, cols);
    }

    free(target_grid);
    free(best_predecessor);
    free(live_cells);

    // Libera todas as cláusulas e a matriz que as armazena
    free_clauses(clauses, clause_count);

    return exit_status;
}