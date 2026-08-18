#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "rgsmlclib.h"

#define NEIGHBOURS 8

typedef enum {
    LONELINESS,
    STAGNATION,
    OVERCROWDING,
    PRESERVATION,
    LIFE
} GameOfLifeRule;

void print_grid(const int *grid, int rows, int cols)
{
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
            printf("%d ", grid[i * cols + j]);

        printf("\n");
    }
}

static void add_clause_row(int ***clauses, int *clause_count, int clause_capacity)
{
    // Realoca memória para uma nova linha
    (*clauses) = realloc((*clauses), ((*clause_count) + 1) * sizeof(int *));
    if ((*clauses) == NULL) {
        fprintf(stderr, "Error: Failed to reallocate memory for clauses.\n");
        exit(1);
    }

    // Aloca memória para a nova linha com o número fixo de colunas
    (*clauses)[(*clause_count)] = malloc(clause_capacity * sizeof(int));
    if ((*clauses)[*clause_count] == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for a clause.\n");
        exit(1);
    }

    // Incrementa o número de linhas
    (*clause_count)++;
}

static void write_clause(kissat *solver, int *clause, int size)
{
    for(int i = 0; i < size; i++)
        kissat_add(solver, clause[i]);
    
    kissat_add(solver, 0);
}

void write_all_clauses(kissat *solver, int clause_count, int **clauses)
{
    for (int i = 0; i < clause_count; i++)
    {
        int literal_count = clauses[i][0];
        int clause[literal_count];

        for (int j = 0; j < literal_count; j++)
            clause[j] = clauses[i][j+1];

        write_clause(solver, clause, literal_count);
    }
}

// Verifica se uma célula está dentro da matriz
static bool is_valid_cell(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

// Operações loneliness, stagnation, overcrowding, preservation e life
static void encode_rule_constraints(int row, int col, int rows, int cols, int ***clauses,
                                    int *clause_count, GameOfLifeRule rule)
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
                add_clause_row(clauses, clause_count, NEIGHBOURS+2);

                int clause_index = 0;
                
                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (!(comb & (1 << bit)))
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = neighbors[bit];
                }

                (*clauses)[(*clause_count)-1][0] = clause_index;
            }

        break;

        // Stagnation: A dead cell with exactly two live neighbours at time t0 will
        // still be dead at time t1.
        case STAGNATION:

            if (alive_count == 2) 
            {
                add_clause_row(clauses, clause_count, NEIGHBOURS+2);

                int clause_index = 0;

                (*clauses)[(*clause_count)-1][(clause_index++)+1] = (row * cols + col + 1);

                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (comb & (1 << bit)) {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                (*clauses)[(*clause_count)-1][0] = clause_index;
            }

        break;

        // Overcrowding: A cell with four or more live neighbours at time t0 will be dead
        // at time t1 irrespective of its own state at t0.
        case OVERCROWDING:

            if (alive_count == 4) 
            {
                add_clause_row(clauses, clause_count, NEIGHBOURS+2);

                int clause_index = 0;

                for (int bit = 0; bit < valid_neighbors; bit++) {
                    if (comb & (1 << bit))
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = -neighbors[bit];
                }

                (*clauses)[(*clause_count)-1][0] = clause_index;
            }

        break;

        // Preservation: A cell that is alive at time t0 with exactly two live neighbours
        // will remain alive at time t1.
        case PRESERVATION:

            if (alive_count == 2) 
            {
                add_clause_row(clauses, clause_count, NEIGHBOURS+2);

                int clause_index = 0;

                (*clauses)[(*clause_count)-1][(clause_index++)+1] = -(row * cols + col + 1);

                for (int bit = 0; bit < valid_neighbors; bit++)
                {
                    if (comb & (1 << bit)) {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                (*clauses)[(*clause_count)-1][0] = clause_index;
            }

        break;

        // Life: A cell with exactly 3 live neighbours at time t0 will be alive at time t1,
        // irrespective of its prior state.
        case LIFE:

            if (alive_count == 3)
            {
                add_clause_row(clauses, clause_count, NEIGHBOURS+2);

                int clause_index = 0;

                for (int bit = 0; bit < valid_neighbors; bit++) {
                    if (comb & (1 << bit)) {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = -neighbors[bit];
                    } else {
                        (*clauses)[(*clause_count)-1][(clause_index++)+1] = neighbors[bit];
                    }
                }

                (*clauses)[(*clause_count)-1][0] = clause_index;
            }

        break; 
        
        default:

            fprintf(stderr, "Error: Unknown Game of Life rule.\n");
            exit(1);

        break;

        }
    }
}

void build_predecessor_cnf(const int *target_grid, int ***clauses, int *clause_count,
                           int rows, int cols)
{
    for(int row = 0; row < rows; row++)
    {
        for(int col = 0; col < cols; col++)
        {
            if (target_grid[row * cols + col] == 1) {
                encode_rule_constraints(row, col, rows, cols, clauses,
                                        clause_count, LONELINESS);
                encode_rule_constraints(row, col, rows, cols, clauses,
                                        clause_count, STAGNATION);
                encode_rule_constraints(row, col, rows, cols, clauses,
                                        clause_count, OVERCROWDING);
            } else {
                encode_rule_constraints(row, col, rows, cols, clauses,
                                        clause_count, PRESERVATION);
                encode_rule_constraints(row, col, rows, cols, clauses,
                                        clause_count, LIFE);
            }
        }
    }
}

void add_model_blocking_clause(const int *live_cells, int live_cell_count,
                               int ***clauses, int *clause_count)
{
    add_clause_row(clauses, clause_count, live_cell_count+1);

    int clause_index = 0;

    for (int i = 0; i < live_cell_count; i++)
        (*clauses)[(*clause_count)-1][(clause_index++)+1] = -live_cells[i];

    (*clauses)[(*clause_count)-1][0] = clause_index;
}