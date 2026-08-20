#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "rgsmlclib.h"

#define NEIGHBOURS 8
#define INITIAL_CLAUSE_CAPACITY 16

typedef enum {
    LONELINESS,
    STAGNATION,
    OVERCROWDING,
    PRESERVATION,
    LIFE
} GameOfLifeRule;

// Imprime o tabuleiro no formato esperado pela saída do programa
void print_grid(const int *grid, int rows, int cols)
{
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
            printf("%d ", grid[i * cols + j]);

        printf("\n");
    }
}

// Adiciona uma nova cláusula à base dinâmica de cláusulas
static void add_clause_row(ClauseDatabase *clauses, int clause_capacity)
{
    if (clauses->count == clauses->capacity)
    {
        size_t new_capacity =
            clauses->capacity == 0 ? INITIAL_CLAUSE_CAPACITY : clauses->capacity * 2;

        int **new_data = realloc(clauses->data, new_capacity * sizeof *new_data);

        if (new_data == NULL) {
            fprintf(stderr, "Error: Failed to expand clause database.\n");
            exit(EXIT_FAILURE);
        }

        clauses->data = new_data;
        clauses->capacity = new_capacity;
    }

    clauses->data[clauses->count] = 
        malloc((size_t) clause_capacity * sizeof *clauses->data[clauses->count]);

    if (clauses->data[clauses->count] == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for a clause.\n");
        exit(EXIT_FAILURE);
    }

    clauses->count++;
}

// Adiciona uma cláusula CNF ao solver Kissat
static void write_clause(kissat *solver, const int *clause, int size)
{
    for(int i = 0; i < size; i++)
        kissat_add(solver, clause[i]);
    
    kissat_add(solver, 0);
}

// Envia todas as cláusulas CNF armazenadas para o solver Kissat
void write_all_clauses(kissat *solver, const ClauseDatabase *clauses)
{
    for (size_t i = 0; i < clauses->count; i++)
    {
        int literal_count = clauses->data[i][0];

        write_clause(solver, &clauses->data[i][1], literal_count);
    }
}

// Verifica se uma célula está dentro da matriz
static bool is_valid_cell(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

// Gera as cláusulas CNF correspondentes a uma regra do Game of Life para uma célula
static void encode_rule_constraints(int row, int col, int rows, int cols,
                                    ClauseDatabase *clauses, GameOfLifeRule rule)
{
    // Deslocamentos para os 8 vizinhos
    int dx[NEIGHBOURS] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dy[NEIGHBOURS] = {-1, 0, 1, -1, 1, -1, 0, 1};

    // Obter os vizinhos válidos
    int neighbors[NEIGHBOURS] = {0};
    int valid_neighbors = 0;

    for (int k = 0; k < NEIGHBOURS; k++)
    {
        int neighbor_row = row + dx[k];
        int neighbor_col = col + dy[k];

        // Indexação positiva para SAT solver
        if (is_valid_cell(neighbor_row, neighbor_col, rows, cols))
            neighbors[valid_neighbors++] = neighbor_row * cols + neighbor_col + 1;
    }

    // Gerar combinações de vizinhos
    int total_combinations = 1 << valid_neighbors; // 2^valid_neighbors

    for (int comb = 0; comb < total_combinations; comb++)
    {
        int alive_count = 0;
        for (int bit = 0; bit < valid_neighbors; bit++) 
        {
            if ((comb & (1 << bit)))
                alive_count++;
        }

        switch (rule)
        {
        
        // Loneliness: A cell with fewer than 2 live neighbours (at least 7 dead neighbours)
        // at time t0 is dead at time t1, irrespective of its own state at t0.
        case LONELINESS:
            
            if (alive_count == 1) 
            {
                add_clause_row(clauses, NEIGHBOURS+2);

                int clause_index = 0;
                
                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (!(comb & (1 << bit)))
                        clauses->data[clauses->count-1][(clause_index++)+1] = neighbors[bit];
                }

                clauses->data[clauses->count-1][0] = clause_index;
            }

        break;

        // Stagnation: A dead cell with exactly two live neighbours at time t0 will
        // still be dead at time t1.
        case STAGNATION:

            if (alive_count == 2) 
            {
                add_clause_row(clauses, NEIGHBOURS+2);

                int clause_index = 0;

                clauses->data[clauses->count-1][(clause_index++)+1] = (row * cols + col + 1);

                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (comb & (1 << bit)) {
                        clauses->data[clauses->count-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        clauses->data[clauses->count-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                clauses->data[clauses->count-1][0] = clause_index;
            }

        break;

        // Overcrowding: A cell with four or more live neighbours at time t0 will be dead
        // at time t1 irrespective of its own state at t0.
        case OVERCROWDING:

            if (alive_count == 4) 
            {
                add_clause_row(clauses, NEIGHBOURS+2);

                int clause_index = 0;

                for (int bit = 0; bit < valid_neighbors; bit++) {
                    if (comb & (1 << bit))
                        clauses->data[clauses->count-1][(clause_index++)+1] = -neighbors[bit];
                }

                clauses->data[clauses->count-1][0] = clause_index;
            }

        break;

        // Preservation: A cell that is alive at time t0 with exactly two live neighbours
        // will remain alive at time t1.
        case PRESERVATION:

            if (alive_count == 2) 
            {
                add_clause_row(clauses, NEIGHBOURS+2);

                int clause_index = 0;

                clauses->data[clauses->count-1][(clause_index++)+1] = -(row * cols + col + 1);

                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (comb & (1 << bit)) {
                        clauses->data[clauses->count-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        clauses->data[clauses->count-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                clauses->data[clauses->count-1][0] = clause_index;
            }

        break;

        // Life: A cell with exactly 3 live neighbours at time t0 will be alive at time t1,
        // irrespective of its prior state.
        case LIFE:

            if (alive_count == 3)
            {
                add_clause_row(clauses, NEIGHBOURS+2);

                int clause_index = 0;

                for (int bit = 0; bit < valid_neighbors; bit++) {
                    if (comb & (1 << bit)) {
                        clauses->data[clauses->count-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        clauses->data[clauses->count-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                clauses->data[clauses->count-1][0] = clause_index;
            }

        break; 
        
        default:

            fprintf(stderr, "Error: Unknown Game of Life rule.\n");
            exit(EXIT_FAILURE);

        break;

        }
    }
}

// Gera as restrições CNF que representam todos os predecessores válidos do tabuleiro alvo
void build_predecessor_cnf(const int *target_grid, ClauseDatabase *clauses, int rows, int cols)
{
    for(int row = 0; row < rows; row++)
    {
        for(int col = 0; col < cols; col++)
        {
            if (target_grid[row * cols + col] == 1) {
                encode_rule_constraints(row, col, rows, cols, clauses, LONELINESS);
                encode_rule_constraints(row, col, rows, cols, clauses, STAGNATION);
                encode_rule_constraints(row, col, rows, cols, clauses, OVERCROWDING);
            } else {
                encode_rule_constraints(row, col, rows, cols, clauses, PRESERVATION);
                encode_rule_constraints(row, col, rows, cols, clauses, LIFE);
            }
        }
    }
}

// Adiciona uma cláusula que bloqueia o modelo atual e seus supersets nas próximas buscas
void add_model_blocking_clause(const int *live_cells, int live_cell_count,
                               ClauseDatabase *clauses)
{
    add_clause_row(clauses, live_cell_count+1);

    int clause_index = 0;

    for (int i = 0; i < live_cell_count; i++)
        clauses->data[clauses->count-1][(clause_index++)+1] = -live_cells[i];

    clauses->data[clauses->count-1][0] = clause_index;
}

// Conta o número de vizinhos vivos de uma célula
static int count_live_neighbors(const int *grid, int row, int col, int rows, int cols)
{
    int live_neighbors = 0;

    for (int row_offset = -1; row_offset <= 1; row_offset++) 
    {
        for (int col_offset = -1; col_offset <= 1; col_offset++) 
        {
            if (row_offset == 0 && col_offset == 0)
                continue;

            int neighbor_row = row + row_offset;
            int neighbor_col = col + col_offset;

            if (is_valid_cell(neighbor_row, neighbor_col, rows, cols))
                live_neighbors += grid[neighbor_row * cols + neighbor_col];
        }
    }

    return live_neighbors;
}

// Verifica se o predecessor evolui exatamente para o tabuleiro alvo
bool is_valid_predecessor(const int *predecessor, const int *target_grid, int rows, int cols)
{
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            int index = row * cols + col;

            int live_neighbors = count_live_neighbors(predecessor, row, col, rows, cols);

            int next_state;

            if (predecessor[index] == 1) {
                next_state = (live_neighbors == 2 || live_neighbors == 3);
            } else {
                next_state = (live_neighbors == 3);
            }

            if (next_state != target_grid[index])
                return false;
        }
    }

    return true;
}

// Libera a memória utilizada pela base dinâmica de cláusulas
void free_clauses(ClauseDatabase *clauses)
{
    if (clauses == NULL)
        return;

    for (size_t i = 0; i < clauses->count; i++)
        free(clauses->data[i]);

    free(clauses->data);

    clauses->data = NULL;
    clauses->count = 0;
    clauses->capacity = 0;
}