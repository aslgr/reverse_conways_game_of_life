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

void build_predecessor_cnf(const int *target_grid, ClauseDatabase *clauses,
                           int rows, int cols);
                        
void add_live_cells_upper_bound(int cell_count, int max_live_cells,
                                ClauseDatabase *clauses);

void write_all_clauses(kissat *solver, const ClauseDatabase *clauses);                

bool is_valid_predecessor(const int *predecessor, const int *target_grid,
                          int rows, int cols);

void print_grid(const int *grid, int rows, int cols);

void truncate_clauses(ClauseDatabase *clauses, size_t new_count);

void free_clauses(ClauseDatabase *clauses);

#endif