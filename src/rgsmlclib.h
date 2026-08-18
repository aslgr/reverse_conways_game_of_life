#ifndef RGSMLCLIB_H
#define RGSMLCLIB_H

#include <stdbool.h>
#include <kissat.h>

void print_grid(const int *grid, int rows, int cols);

void write_all_clauses(kissat *solver, int clause_count, int **clauses);

void build_predecessor_cnf(const int *target_grid, int ***clauses, int *clause_count,
                           int rows, int cols);

void add_model_blocking_clause(const int *live_cells, int live_cell_count,
                               int ***clauses, int *clause_count);

bool is_valid_predecessor(const int *predecessor, const int *target_grid,
                          int rows, int cols);

void free_clauses(int **clauses, int clause_count);

#endif