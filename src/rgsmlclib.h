#ifndef RGSMLCLIB_H
#define RGSMLCLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <kissat.h>

typedef struct {
    int **data;
    size_t count;
    size_t capacity;
} ClauseDatabase;

void print_grid(const int *grid, int rows, int cols);

void write_all_clauses(kissat *solver, const ClauseDatabase *clauses);

void build_predecessor_cnf(const int *target_grid, ClauseDatabase *clauses,
                           int rows, int cols);

void add_model_blocking_clause(const int *live_cells, int live_cell_count,
                               ClauseDatabase *clauses);

bool is_valid_predecessor(const int *predecessor, const int *target_grid,
                          int rows, int cols);

void free_clauses(ClauseDatabase *clauses);

#endif