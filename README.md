# Reverse Conway's Game of Life

**A SAT-based solver written in C that reconstructs an immediately previous state of Conway's Game of Life while minimizing the number of live cells.**

This project models the reverse step of Conway's Game of Life as a Boolean satisfiability problem (SAT), generates the corresponding CNF constraints, solves them with **Kissat**, and repeatedly tightens a cardinality bound until the minimum predecessor is found or the execution time limit is reached.

The project started as an Artificial Intelligence course assignment and was later refactored and extended with stronger optimization, safer memory management, independent solution validation, explicit timeout handling, and a cleaner build system.

**Tech:** C11 · SAT / CNF · Kissat · POSIX signals · GNU Make

## Highlights

- Reverse Game of Life modeled as a **SAT problem**
- CNF encoding of Conway's local transition rules
- **Kissat** SAT solver integration through its C API
- Minimum-live-cell optimization using a **sequential-counter cardinality encoding**
- Strictly improving SAT iterations: a solution with `k` live cells is followed by a search constrained to at most `k - 1`
- Independent forward simulation to validate every returned predecessor
- 300-second solver timeout with best-so-far recovery
- Dynamic clause database with geometric capacity growth
- Input validation and explicit `SAT` / `UNSAT` / interrupted-search handling
- C11 build with warnings enabled, `-O2`, and automatic header dependency tracking
- Utility programs for generating random valid input instances

---

## The Problem

Conway's Game of Life is usually evaluated **forward**: given a board at time `t`, its state at time `t + 1` is completely determined by the state of each cell and its eight neighbors.

This project solves the opposite problem:

> Given a board at time `t + 1`, find a valid board at time `t` that evolves exactly into it — and among all valid predecessors, minimize the number of live cells.

The forward transition is deterministic, but the reverse problem is not. A target board may have many predecessors, one predecessor, or no predecessor at all.

For each cell, the standard Game of Life rules are:

1. A dead cell with exactly three live neighbors becomes alive.
2. A live cell with fewer than two live neighbors dies from underpopulation.
3. A live cell with more than three live neighbors dies from overpopulation.
4. A live cell with two or three live neighbors survives.

All updates occur simultaneously.

The input also assumes dead border cells. During evolution and validation, neighbors outside the board are treated as dead.

---

## Why SAT?

Trying every possible predecessor directly would require exploring up to

```text
2^(rows * columns)
```

possible boards.

Instead, every predecessor cell is represented by a Boolean variable:

```text
x_i = true   -> predecessor cell i is alive
x_i = false  -> predecessor cell i is dead
```

The Game of Life transition rules are then translated into **CNF clauses**. A satisfying assignment corresponds to a predecessor whose next generation is exactly the target board.

This separates the problem into two layers:

```text
Reverse Game of Life
        |
        v
Boolean constraints
        |
        v
CNF formula
        |
        v
Kissat SAT solver
        |
        v
Candidate predecessor
```

The program then adds an optimization layer on top of the SAT formulation to minimize the number of variables assigned `true`.

---

## SAT Encoding

For every target cell, the implementation enumerates the possible configurations of its valid neighbors. A cell has at most eight neighbors, so the local search space is bounded by `2^8 = 256` configurations per cell.

The encoder groups Conway's behavior into five logical cases:

### Loneliness

A cell with fewer than two live neighbors at time `t` is dead at time `t + 1`, independently of its previous state.

### Stagnation

A dead cell with exactly two live neighbors remains dead.

### Overcrowding

A cell with four or more live neighbors becomes dead, independently of its previous state.

### Preservation

A live cell with exactly two live neighbors remains alive.

### Life

A cell with exactly three live neighbors is alive in the next generation, independently of whether it was previously dead or alive.

For a target cell that is alive, the generated clauses eliminate predecessor configurations that would make it dead. For a target cell that is dead, the generated clauses eliminate configurations that would make it alive.

The result is a CNF formula whose satisfying assignments represent valid immediate predecessors of the complete target board.

---

## Minimum-Live-Cell Optimization

Finding *a* predecessor is not enough. The objective is to find one containing the fewest possible live cells.

If the Boolean variables representing predecessor cells are

```text
x1, x2, ..., xn
```

then the optimization objective is equivalent to minimizing

```text
x1 + x2 + ... + xn
```

where a live cell contributes `1` and a dead cell contributes `0`.

### Sequential Counter

The current implementation converts an upper bound

```text
x1 + x2 + ... + xn <= k
```

into CNF using a **sequential counter**.

Auxiliary variables `s(i,j)` represent information about how many live cells have appeared in a prefix of the board variables. The generated clauses propagate this count and prevent an assignment from exceeding the requested upper bound.

The original board variables use SAT variable IDs starting at `1`. Counter variables begin after the last board variable, preventing ID collisions.

This encoding requires roughly `O(nk)` auxiliary state in the general case, which is a deliberate trade-off: it creates a larger SAT formula but makes the optimization search much more directed.

### Iterative Tightening

The optimization loop works as follows:

```text
1. Solve the base Game of Life CNF.
2. Count the live cells in the SAT model -> k.
3. Save the model as the current best solution.
4. Restore the base CNF.
5. Add the cardinality constraint: live_cells <= k - 1.
6. Solve again.
7. Repeat until UNSAT or timeout.
```

If the search for `k - 1` live cells is `UNSAT`, the current `k`-cell predecessor is globally optimal: no valid predecessor with fewer live cells exists.

A zero-live-cell predecessor is also immediately optimal.

---

## Why the Optimization Strategy Changed

An earlier version of the project minimized solutions using **model-blocking clauses**. After finding a model, it added a clause containing the negation of every live variable in that model.

That correctly prevented the same model and its supersets from being returned again, but the next SAT solution was not required to have fewer live cells. The search could therefore move through models such as:

```text
25 -> 34 -> 29 -> 21 -> 27 -> 18 -> ...
```

The current cardinality-based strategy instead guarantees strict improvement:

```text
25 -> at most 24 -> 17 -> at most 16 -> 11 -> at most 10 -> UNSAT
```

This turns the minimization process from model enumeration into a sequence of increasingly restrictive optimization problems.

---

## Clause Database

CNF clauses are stored in a custom `ClauseDatabase`:

```c
typedef struct {
    int **data;
    size_t count;
    size_t capacity;
} ClauseDatabase;
```

Each clause stores its literal count followed by its literals.

The database grows geometrically:

```text
16 -> 32 -> 64 -> 128 -> 256 -> ...
```

instead of reallocating the outer clause array for every new clause. This reduces allocation overhead and preserves the original allocation safely if a `realloc` attempt fails.

During optimization, the number of clauses belonging to the original Game of Life encoding is saved. Temporary cardinality clauses can then be removed with `truncate_clauses()`, while the allocated database capacity is retained and reused for the next bound.

---

## Independent Solution Validation

The SAT model is **not accepted blindly**.

Before a predecessor is printed, the program performs a conventional forward Game of Life simulation on it and compares every resulting cell against the target board.

This provides an independent correctness check between two different implementations of the same rules:

```text
SAT encoding -> candidate predecessor
                    |
                    v
       regular Game of Life simulation
                    |
                    v
             target comparison
```

If the candidate does not evolve exactly into the input board, the program reports an error instead of returning it as a valid result.

---

## Timeout and Search Results

The optimization phase has a **300-second time limit**.

A POSIX `SIGALRM` handler marks the timeout and requests termination of the currently active Kissat instance. The program distinguishes between several outcomes:

- **Optimal solution found:** the tighter bound becomes `UNSAT`, proving that the current predecessor is minimum.
- **Zero-live-cell predecessor found:** optimality is immediate.
- **Timeout after at least one solution:** the best valid predecessor found so far is printed, together with a warning on `stderr` that optimality was not proven.
- **Timeout before any predecessor is found:** the program exits with an error.
- **Base formula is UNSAT:** `UNSAT` is printed, meaning the target board has no immediate predecessor satisfying the encoded rules.

Progress messages, warnings, and errors are written to `stderr`, keeping successful matrix output on `stdout` clean and redirectable.

---

## Project Structure

```text
ReverseConwaysGameOfLife/
|
|-- src/
|   |-- ConwayGoLRGSMLC.c    # Input, optimization loop, timeout and program flow
|   |-- rgsmlclib.c          # CNF encoding, cardinality, validation and clause management
|   `-- rgsmlclib.h          # Public interface and ClauseDatabase definition
|
|-- inputs/
|   |-- inputEnunciado.txt   # Original 4x6 example instance
|   |-- input8x8.txt
|   |-- input13x13.txt
|   |-- input25x25.txt
|   |-- generateInput.c      # Random instance generator
|   `-- generateInputs.sh    # Interactive helper for generating one or many instances
|
|-- kissat/                  # External dependency, created by make install_kissat
|-- obj/                     # Generated object/dependency files
|-- Makefile
`-- README.md
```

`kissat/`, `obj/`, and the executable are build artifacts/dependencies and do not need to be committed to the repository.

---

## Requirements

The project is intended for a Linux/POSIX environment and requires:

- GCC with C11 support
- GNU Make
- Git
- A POSIX-compatible system (`signal`, `alarm`, and `unistd.h` are used)

Kissat is downloaded and built through the provided Makefile.

---

## Build

Clone the repository and enter the project directory.

First, install the SAT solver dependency:

```bash
make install_kissat
```

This clones Kissat into `kissat/`, configures it, and builds its static library.

Then compile the project:

```bash
make
```

The default build uses:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2 -MMD -MP
```

The resulting executable is:

```text
ConwayGoLRGSMLC
```

`RGSMLC` stands for **Reverse Game State with Minimum Live Cells**.

### Useful Make Targets

```bash
make                 # Build the solver
make run             # Run the original example input
make clean           # Remove project object files and executable
make clean_kissat    # Clean Kissat build artifacts
make install_kissat  # Clone/configure/build Kissat
```

The `-MMD -MP` flags generate dependency files automatically, so changes to project headers correctly trigger recompilation of dependent source files.

---

## Running the Solver

The solver reads the board from standard input:

```bash
./ConwayGoLRGSMLC < inputs/inputEnunciado.txt
```

Other included instances can be tested in the same way:

```bash
./ConwayGoLRGSMLC < inputs/input8x8.txt
./ConwayGoLRGSMLC < inputs/input13x13.txt
./ConwayGoLRGSMLC < inputs/input25x25.txt
```

Because the result is written to standard output, it can also be redirected:

```bash
./ConwayGoLRGSMLC < inputs/input13x13.txt > predecessor.txt
```

Warnings and progress information remain on `stderr`.

---

## Input Format

The first line contains the number of rows and columns:

```text
rows columns
```

It is followed by the grid, where `0` represents a dead cell and `1` represents a live cell.

Example:

```text
4 6
0 0 0 0 0 0
0 0 1 1 0 0
0 0 0 1 1 0
0 0 0 0 0 0
```

The program validates that:

- dimensions are positive;
- every cell is either `0` or `1`;
- all input border cells are dead.

---

## Output Format

When a predecessor is found, the output uses exactly the same matrix format:

```text
rows columns
<predecessor grid>
```

The returned grid is guaranteed to have passed the independent forward-evolution validation.

If no predecessor exists, the program prints:

```text
UNSAT
```

If the 300-second limit is reached after at least one valid predecessor has been found, the best solution available at that moment is returned and a warning indicates that global optimality was not proven.

---

## Generating Random Inputs

The repository also contains a small input generator.

Compile it from the `inputs/` directory:

```bash
cd inputs
gcc -std=c11 -Wall -Wextra -O2 generateInput.c -o generateInput
```

Generate one instance directly:

```bash
./generateInput 13 13 random13.in
```

The generator keeps all border cells dead and randomly assigns `0` or `1` to internal cells.

For an interactive interface that can generate one file or a sequence of files:

```bash
./generateInputs.sh
```

Randomly generated target boards are valid *inputs*, but they are not guaranteed to have a predecessor. Therefore, generated instances may legitimately produce `UNSAT`.

---

## Performance Notes

SAT performance depends strongly on the structure of each target board, not only on its dimensions.

The sequential-counter encoding makes every optimization iteration meaningful by enforcing a strictly lower live-cell bound, but it also introduces auxiliary variables and clauses. For a board with `n` cells and an upper bound `k`, the encoding has roughly `O(nk)` size.

In local development tests:

| Instance | Observed behavior |
|---|---|
| `13x13` | Completed well within the 300-second limit after the cardinality-based optimization was introduced |
| `25x25` | Reached the 300-second limit and returned a valid best-so-far predecessor with **175 live cells** |

For the same `25x25` instance, the previous model-blocking strategy had returned a best-so-far solution with **190 live cells** under the same time limit. This is not a formal benchmark, but it illustrates the trade-off: the cardinality encoding creates a larger CNF while directing the solver toward strictly better solutions.

Runtime and solution-search behavior are hardware- and instance-dependent.

---

## Implementation Details Worth Noticing

### Safe dynamic memory management

The outer clause array uses geometric growth and a temporary pointer for `realloc`, avoiding loss of the original allocation if expansion fails.

### Clean solver reconstruction

Optimization-specific clauses are separated from the base Game of Life formula. Before applying a tighter bound, the temporary clauses are discarded and a fresh Kissat instance is populated with the base CNF plus the new bound.

### Explicit solver-state handling

Kissat results are treated separately as `SAT`, `UNSAT`, or unknown/interrupted rather than assuming every non-SAT result has the same meaning.

### Output discipline

Machine-readable board output goes to `stdout`; diagnostics go to `stderr`. This makes the executable easy to use in shell pipelines and automated tests.

### Defensive validation

Malformed dimensions, invalid cell values, invalid borders, allocation failures, solver interruptions, and invalid solver-produced predecessors are handled explicitly.

---

## Background

This repository originated from an Artificial Intelligence course assignment focused on reversing Conway's Game of Life under a 300-second execution limit and an 8 GB memory limit.

The final version keeps the SAT-based core of the original project while extending it with a cardinality-based minimization strategy, stronger resource handling, independent correctness validation, improved clause storage, and a more maintainable build configuration.

---

## Acknowledgements

- **John Conway** for Conway's Game of Life.
- **Kissat** for the SAT-solving backend used by the project.
