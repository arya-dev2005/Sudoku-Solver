<h1 align="center">🧩 Sudoku Solver Pro</h1>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B" alt="C++20" />
  <img src="https://img.shields.io/badge/algorithms-optimization-orange" alt="Algorithms" />
  <img src="https://img.shields.io/badge/backtracking-DFS-red" alt="Backtracking" />
  <img src="https://img.shields.io/badge/bitmasking-electronics-purple" alt="Bitmasking" />
  <img src="https://img.shields.io/badge/open_source-%F0%9F%8C%8D-blue" alt="Open Source" />
</p>

---

## 📖 Overview

<strong> Modern C++ CLI engine for high-performance Sudoku solving, real-time visualization, and algorithm engineering.</strong>

It is a polished open-source portfolio project that combines constraint satisfaction, bit-level optimization, and heuristic search in a clean C++ architecture. The engine is built around a production-grade CLI experience and is designed to showcase both algorithmic rigor and systems programming craftsmanship.

### 🤔 Why this problem is interesting

- Sudoku is a compact constraint satisfaction problem that is easy to state but expensive to solve naively.
- Pure backtracking can explode into tens of millions of recursive branches for hard puzzles.
- Modern optimization like Minimum Remaining Values (MRV) and bitmask candidate generation turns Sudoku solving from brute-force into practical performance engineering.
- This repository demonstrates how algorithmic design and low-level C++ efficiency combine to make a solver fast, deterministic, and recruiter-friendly.

---

## 🚀 Feature Matrix

| Feature | Description | Status |
|---|---|:---:|
| Recursive DFS solving | Classic depth-first search with backtracking | ✅ Complete |
| Bitmask constraint engine | Row, column and 3×3 box occupancy stored as masks | ✅ Complete |
| MRV heuristic | Chooses the empty cell with the fewest candidates first | ✅ Complete |
| Puzzle validation | Detects invalid boards, duplicates, and unsolvable states | ✅ Complete |
| Visual solve mode | Terminal dashboard with animated step/visualization options | ✅ Complete |
| Puzzle generator | Difficulty-based puzzle creation with uniqueness analysis | ✅ Complete |
| Benchmark mode | Compare solver behavior across modes and difficulty classes | ✅ Complete |
| File I/O | Load/save puzzles, reports, and compact/pretty board formats | ✅ Complete |
| Difficulty analysis | Score-based difficulty estimation and solver report output | ✅ Complete |

---

## ⚙️ Engineering Highlights

- **O(1) constraint checking** using precomputed row/column/box bitmasks.
- **Search-space pruning** with MRV to dramatically reduce candidate branching.
- **Efficient rollback** by restoring only the affected masks and board cell state.
- **Heuristic search design**: candidate generation is separated from recursive search for clarity and performance.
- **CSP engineering**: the solver is modeled as a constraint satisfaction pipeline with validation, propagation, solving, and reporting.

---

## 📊 Performance Profile

### 🔁 Recursion reduction

The pairing of MRV and bitmask candidates creates a much tighter search space than naive backtracking. Rather than trying every empty cell in order, the solver chooses the most constrained cell and only explores legal digits.

### 🧠 Complexity discussion

- Naive brute-force backtracking can approach `O(9^m)` where `m` is the number of empty cells.
- Practical performance is orders of magnitude better for real Sudoku puzzles due to early pruning, candidate filtering, and propagation.
- Hard Sudoku puzzles often solve in milliseconds with this approach, while a naive solver may still explore tens of thousands of dead ends.

### 🧪 Benchmark ideas

- Compare `MRV + bitmask` vs. plain sequential backtracking.
- Measure solve time across `Easy`, `Medium`, `Hard`, `Expert` puzzle families.
- Track recursion depth, total branch count, and backtrack events.
- Profile propagation-enabled solves against pure search-only solves.

---

## 🖥️ Terminal Mockups

### 🎞️ Visual solve dashboard

```text
[ Sudoku Solver Pro ]   Mode: Visual solve   Delay: 80ms   Propagation: ON
+-------+-------+-------+    [Step 14 / 81]
| 5 3 4 | 6 7 8 | 9 1 2 |    Candidates: [2,8]
| 6 7 2 | 1 9 5 | 3 4 8 |    Backtracks: 18
| 1 9 8 | 3 4 2 | 5 6 7 |
+-------+-------+-------+
| 8 5 9 | 7 6 1 | 4 2 3 |    Status: searching
| 4 2 6 | 8 5 3 | 7 9 1 |
| 7 1 3 | 9 2 4 | 8 5 6 |
+-------+-------+-------+
| 9 6 1 | 5 3 7 | 2 8 4 |
| 2 8 7 | 4 1 9 | 6 3 5 |
| 3 4 5 | 2 8 6 | 1 7 9 |
+-------+-------+-------+
```

### 📈 Statistics dashboard

```text
Solver statistics
-----------------
Status          : Unique solution
Solve time      : 0.84 ms
Recursive calls : 1,284
Backtracks      : 27
Empty cells     : 43
Difficulty score : Expert

Mode comparison
---------------
- MRV + Propagation : 0.84 ms
- MRV only          : 1.26 ms
- Plain backtracking: 8.40 ms
```

### ⏱️ Benchmark mode

```text
Benchmark mode
--------------
Puzzle set     : Expert samples
Solver variants: MRV + propagation, MRV only, plain backtracking
Best result    : 0.78 ms
Worst result   : 12.34 ms
Average solve  : 2.17 ms
```

---

## 🧠 Algorithm Visualization

### 1️⃣ Recursive DFS

The solver uses a depth-first search over empty cells. When a candidate fails, it backtracks and restores the board state.

### 2️⃣ MRV heuristic

Minimum Remaining Values means the next cell is chosen by the fewest legal candidates. This minimizes branching and prioritizes the hardest decisions first.

### 3️⃣ Bitmask candidate generation

Each cell computes its allowed digits with a single bitwise expression:

```cpp
const uint16_t used = rowMask[r] | colMask[c] | boxMask[b];
const uint16_t candidates = allowedMask & ~used;
```

This avoids scanning all digits and keeps candidate generation constant-time.

### 4️⃣ Rollback mechanism

When a candidate value does not lead to a solution, the solver restores:
- the board cell to empty
- the row mask
- the column mask
- the box mask

This localized rollback preserves performance and avoids expensive copies.

---

## 🏗️ Software Architecture

### 📂 Folder structure

```text
Sudoku-Solver/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── main.cpp          # CLI orchestration and menu flow
├── board.cpp/.hpp    # board model, parsing, validation, serialization
├── solver.cpp/.hpp   # backtracking engine, MRV, propagation, reports
├── generator.cpp/.hpp# puzzle generator and difficulty scoring
├── ui.cpp/.hpp       # terminal rendering, dashboard, ANSI helpers
├── benchmark.cpp/.hpp # solver benchmarking and mode comparison
└── sample_puzzle.txt # example starting grid
```

### 🔄 Component flow

```text
User input -> Board parser -> Validator -> Solver engine -> UI renderer
                                 │
                                 └-> Benchmark / Reports / Generator
```

### 🛠️ Solver pipeline

```text
Input board
  └─> validate structure
      └─> prepare row/col/box masks
          └─> select MRV cell
              └─> generate candidate bitmask
                  └─> recursive DFS search
                      └─> solved board / unsolvable state
```

---

## 🛣️ Future Roadmap

- [ ] Animated visualization
- [ ] NCurses-style terminal UI
- [ ] Sudoku puzzle generator enhancements
- [ ] Parallel solving and multi-threaded benchmark mode
- [ ] Dancing Links / Algorithm X implementation
- [ ] 16×16 variant support
- [ ] Replayable solve traces
- [ ] GitHub Actions CI and code coverage

---

## 🧰 Installation

### 📋 Requirements

- `g++` or `clang++` with **C++20** support
- `CMake` **3.20+**
- Recommended: VS Code with the C++ extension

### ⚡ Build with g++

```powershell
cd path\to\Sudoku-Solver
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
    main.cpp board.cpp solver.cpp ui.cpp generator.cpp benchmark.cpp \
    -o sudoku_solver.exe
.
\sudoku_solver.exe
```

### 🏗️ Build with CMake

```powershell
cd path\to\Sudoku-Solver
cmake -S . -B build
cmake --build build --config Release
.
\build\sudoku_solver.exe
```

### 💻 VS Code workflow

1. Open the repository folder in VS Code.
2. Install the C/C++ extension and CMake Tools.
3. Configure a debug target for `sudoku_solver`.
4. Use the integrated terminal for build and run cycles.

---

## 📝 Developer Notes

- The solver separates UI and engine logic to keep the algorithm portable and testable.
- `std::array` and fixed-size buffers are used for predictable memory layout.
- Row/column/box masks eliminate dynamic containers and make constraint checks constant-time.
- Backtracking state restoration is intentionally minimal: only the impacted masks and cell are restored.
- Propagation is implemented as a preprocessing pass to accelerate search for harder puzzles.

---

## 🎓 Interview Value

This project is a strong portfolio asset because it demonstrates:

- **DSA fundamentals**: backtracking, recursion, complexity analysis.
- **Constraint satisfaction**: modeling Sudoku as a CSP and applying heuristics.
- **Heuristic engineering**: MRV plus propagation to optimize search.
- **Systems programming**: C++20, CLI architecture, build configuration.
- **Performance thinking**: bitmasking, pruning, benchmarking.
- **Modular software design**: clean separation of solver, board, UI, generator, benchmark.

---

## 🖼️ Screenshots & Assets

> Add these assets when the UI is ready:

- `assets/demo.gif`
- `assets/solver-ui.png`
- `assets/benchmark.png`
- `assets/architecture.png`
- `assets/dashboard.png`

---

## 🤝 Contributing

Contributions are welcome. Please follow these guidelines:

1. Open an issue before starting larger work.
2. Create a feature branch from `main`.
3. Keep changes focused and add tests or sample puzzles when relevant.
4. Use clear commit messages and follow a consistent style.
5. Submit a pull request for review.

---

## 📜 License

This project is released under the **MIT License**. See `LICENSE` for details.

---

## ⭐ Star the repository

### If you enjoy algorithm engineering, optimization, and polished CLI tools, please give this repository a star to support future enhancements.

---