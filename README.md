# Sudoku Solver CLI

A high-performance console-based Sudoku Solver built in modern C++ using **Backtracking**, **Bitmasking**, and the **Minimum Remaining Values (MRV)** heuristic for efficient constraint solving.

This project focuses on writing a clean, optimized, and scalable Sudoku engine while maintaining strong software engineering practices and readable architecture.

---

## Features Implemented

### Optimized Backtracking Engine
- Recursive Depth-First Search (DFS) solver
- Efficient state restoration during backtracking
- Early pruning for impossible states

---

### Bitmask-Based Constraint Tracking
Uses bit-level operations instead of expensive data structures like sets or vectors.

Maintains:
- Row masks
- Column masks
- 3×3 box masks

This allows:
- O(1) constraint checking
- Faster candidate generation
- Reduced memory overhead

Example:

```cpp
used = rowMask[r] | colMask[c] | boxMask[b];
```

---

### Minimum Remaining Values (MRV) Heuristic
Implements MRV to select the empty cell with the fewest possible candidates first.

Benefits:
- Aggressive search-space pruning
- Faster solving for difficult puzzles
- Reduced recursion depth

---

### Sudoku Board Validation
The solver validates the initial board before solving.

Checks:
- Invalid digits
- Duplicate values in rows
- Duplicate values in columns
- Duplicate values in subgrids

---

### Clean Console Rendering
Includes formatted Sudoku board visualization using a structured console layout.

---

## Tech Stack

- **Language:** C++
- **Core Concepts:**
  - Backtracking
  - Bitmasking
  - Heuristic Search
  - Constraint Solving
- **Standard Library:**
  - `std::array`
  - Bitwise operations
  - Recursive algorithms

---

## Build and Run

### Option 1: Direct g++

```powershell
g++ -std=c++20 -Wall -Wextra -pedantic main.cpp -o sudoku_solver.exe
.\sudoku_solver.exe
```

### Option 2: CMake

```powershell
cmake -S . -B build
cmake --build build
.\build\sudoku_solver.exe
```

If your generator places the executable under a configuration folder such as
`build\Debug`, run that generated executable instead.

---

## Algorithm Overview

### Constraint Representation

Each digit is represented using a bitmask.

Example:

```cpp
digitMask(d) = 1 << d;
```

This enables constant-time validity checks.

---

### Solving Strategy

The solver works using:

1. Initial board validation
2. Candidate mask generation
3. MRV-based empty cell selection
4. Recursive DFS exploration
5. Backtracking when conflicts occur

---

## Project Structure

```text
.
├── main.cpp
├── Sudoku Solver Logic
│   ├── Board Validation
│   ├── Candidate Generation
│   ├── MRV Cell Selection
│   ├── Recursive Solver
│   └── Console Rendering
```

---

## Example Puzzle

### Original Sudoku:

```text
+-------+-------+-------+
| 5 3 . | . 7 . | . . . |
| 6 . . | 1 9 5 | . . . |
| . 9 8 | . . . | . 6 . |
+-------+-------+-------+
| 8 . . | . 6 . | . . 3 |
| 4 . . | 8 . 3 | . . 1 |
| 7 . . | . 2 . | . . 6 |
+-------+-------+-------+
| . 6 . | . . . | 2 8 . |
| . . . | 4 1 9 | . . 5 |
| . . . | . 8 . | . 7 9 |
+-------+-------+-------+
```

---

### Solved Sudoku:

```text
+-------+-------+-------+
| 5 3 4 | 6 7 8 | 9 1 2 |
| 6 7 2 | 1 9 5 | 3 4 8 |
| 1 9 8 | 3 4 2 | 5 6 7 |
+-------+-------+-------+
| 8 5 9 | 7 6 1 | 4 2 3 |
| 4 2 6 | 8 5 3 | 7 9 1 |
| 7 1 3 | 9 2 4 | 8 5 6 |
+-------+-------+-------+
| 9 6 1 | 5 3 7 | 2 8 4 |
| 2 8 7 | 4 1 9 | 6 3 5 |
| 3 4 5 | 2 8 6 | 1 7 9 |
+-------+-------+-------+
```

---

# Future Scope

The following enhancements are planned for future development:

- Animated console visualization
- ANSI color-based terminal UI
- Constraint propagation
  - Naked Singles
  - Hidden Singles
- Puzzle generator with uniqueness validation
- Difficulty analyzer
- Interactive play mode
- Replay system
- Solver statistics and benchmarking
- Multiple solution detection
- Support for 16×16 Sudoku
- NCurses-based professional terminal interface

---

# Learning Outcomes

This project demonstrates practical implementation of:

- Backtracking algorithms
- Constraint Satisfaction Problems (CSP)
- Bitmask optimization
- Recursive problem solving
- Heuristic-driven search
- Search-space pruning
- Clean systems-level C++ design

---

# Why This Project Matters

Sudoku solving is a classic problem used in:
- Technical interview preparation
- Algorithm engineering
- AI search demonstrations
- Constraint optimization research

This project focuses not only on solving Sudoku correctly, but on solving it efficiently using modern optimization techniques.

---

# Author

**Arya Das**
Computer Science Engineering Student specializing in Data Science.

Interests:
- Algorithms
- Artificial Intelligence
- Optimization Systems
- Software Engineering
- Intelligent Problem Solving

---

<div align="center">

### ⭐ If this project helped you learn Bitmask Optimization and Backtracking algorithms, consider starring the repository.

</div>
