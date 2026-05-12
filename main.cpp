#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using Board = std::array<std::array<int, 9>, 9>;
using FixedCells = std::array<std::array<bool, 9>, 9>;
using Mask = std::uint16_t;

constexpr int SIZE = 9;
constexpr int EMPTY = 0;
constexpr Mask ALL_DIGITS_MASK = 0b1111111110; // Bits 1 through 9 are set.

struct CellChoice {
    int row = -1;
    int col = -1;
    Mask candidatesMask = 0;
    int candidateCount = 0;
};

struct Placement {
    int row = -1;
    int col = -1;
    int digit = 0;
};

struct SolverOptions {
    bool useMrv = true;
    bool usePropagation = true;
    bool useOptimizedIteration = true;
    bool visualMode = false;
    bool stepMode = false;
    bool clearBetweenFrames = true;
    bool useColor = true;
    int animationDelayMs = 35;
    int solutionLimit = 1;
};

struct SolverStats {
    long long recursiveCalls = 0;
    long long backtracks = 0;
    long long nakedSingles = 0;
    long long hiddenSingles = 0;
    long long candidateEliminations = 0;
    int maxDepth = 0;
    double elapsedMs = 0.0;
};

enum class SolveStatus {
    Invalid,
    NoSolution,
    UniqueSolution,
    MultipleSolutions
};

struct SolveReport {
    SolveStatus status = SolveStatus::NoSolution;
    int solutionsFound = 0;
    Board solvedBoard{};
    SolverStats stats{};
};

enum class CellEvent {
    None,
    Guess,
    Propagation,
    Rollback,
    Solution
};

struct RenderState {
    int row = -1;
    int col = -1;
    CellEvent event = CellEvent::None;
    std::string message;
};

int getBoxIndex(int row, int col) {
    return (row / 3) * 3 + (col / 3);
}

Mask digitMask(int digit) {
    return static_cast<Mask>(1u << digit);
}

int maskToDigit(Mask mask) {
    // GCC/Clang intrinsic: returns the index of the least significant set bit.
    // Example: mask 0b10000 -> 4, which represents digit 4 in our encoding.
    return __builtin_ctz(static_cast<unsigned int>(mask));
}

int countBits(Mask mask) {
    // GCC/Clang intrinsic: counts how many candidate bits are currently set.
    return __builtin_popcount(static_cast<unsigned int>(mask));
}

FixedCells makeFixedCells(const Board& board) {
    FixedCells fixed{};

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            fixed[row][col] = board[row][col] != EMPTY;
        }
    }

    return fixed;
}

void printBoard(const Board& board) {
    std::cout << "+-------+-------+-------+\n";
    for (int row = 0; row < SIZE; ++row) {
        std::cout << "| ";
        for (int col = 0; col < SIZE; ++col) {
            std::cout << (board[row][col] == EMPTY ? '.' : static_cast<char>('0' + board[row][col])) << ' ';

            if ((col + 1) % 3 == 0) {
                std::cout << "| ";
            }
        }
        std::cout << '\n';

        if ((row + 1) % 3 == 0) {
            std::cout << "+-------+-------+-------+\n";
        }
    }
}

namespace Ansi {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* CLEAR = "\033[2J\033[H";
}

std::string colorize(const std::string& text, const char* color, const SolverOptions& options) {
    if (!options.useColor) {
        return text;
    }

    return std::string(color) + text + Ansi::RESET;
}

std::string styledDigit(int digit,
                        bool isFixed,
                        bool isHighlighted,
                        CellEvent event,
                        const SolverOptions& options) {
    std::string text = digit == EMPTY ? "." : std::string(1, static_cast<char>('0' + digit));

    if (isHighlighted && event == CellEvent::Rollback) {
        return colorize(text, Ansi::RED, options);
    }

    if (isHighlighted && event == CellEvent::Guess) {
        return colorize(text, Ansi::YELLOW, options);
    }

    if (isHighlighted && event == CellEvent::Propagation) {
        return colorize(text, Ansi::BLUE, options);
    }

    if (isFixed) {
        return colorize(text, Ansi::CYAN, options);
    }

    if (digit != EMPTY) {
        return colorize(text, Ansi::GREEN, options);
    }

    return colorize(text, Ansi::DIM, options);
}

void renderBoard(const Board& board,
                 const FixedCells& fixedCells,
                 const RenderState& state,
                 const SolverOptions& options) {
    std::cout << "+-------+-------+-------+\n";
    for (int row = 0; row < SIZE; ++row) {
        std::cout << "| ";
        for (int col = 0; col < SIZE; ++col) {
            bool highlighted = row == state.row && col == state.col;
            std::cout << styledDigit(board[row][col], fixedCells[row][col],
                                     highlighted, state.event, options) << ' ';

            if ((col + 1) % 3 == 0) {
                std::cout << "| ";
            }
        }
        std::cout << '\n';

        if ((row + 1) % 3 == 0) {
            std::cout << "+-------+-------+-------+\n";
        }
    }
}

void renderSolverFrame(const Board& board,
                       const FixedCells& fixedCells,
                       const SolverStats& stats,
                       const RenderState& state,
                       const SolverOptions& options,
                       int depth) {
    if (!options.visualMode) {
        return;
    }

    if (options.clearBetweenFrames) {
        std::cout << Ansi::CLEAR;
    }

    std::cout << colorize("Sudoku Solver Visual Mode\n", Ansi::BOLD, options);
    renderBoard(board, fixedCells, state, options);

    std::cout << "\nEvent: " << state.message << '\n';
    std::cout << "Depth: " << depth
              << " | Calls: " << stats.recursiveCalls
              << " | Backtracks: " << stats.backtracks
              << " | Naked: " << stats.nakedSingles
              << " | Hidden: " << stats.hiddenSingles << "\n\n";

    std::cout << colorize("Legend: fixed=cyan, solved=green, guess=yellow, propagation=blue, rollback=red\n",
                          Ansi::DIM, options);

    if (options.stepMode) {
        std::cout << "Press Enter for next step...";
        std::cin.get();
    } else if (options.animationDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(options.animationDelayMs));
    }
}

bool isValidInitialBoard(const Board& board,
                         std::array<Mask, 9>& rowMasks,
                         std::array<Mask, 9>& colMasks,
                         std::array<Mask, 9>& boxMasks) {
    rowMasks.fill(0);
    colMasks.fill(0);
    boxMasks.fill(0);

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            int digit = board[row][col];

            if (digit == EMPTY) {
                continue;
            }

            if (digit < 1 || digit > 9) {
                std::cout << "Invalid board: value " << digit
                          << " found at row " << row + 1
                          << ", column " << col + 1 << ".\n";
                return false;
            }

            int box = getBoxIndex(row, col);
            Mask mask = digitMask(digit);

            if ((rowMasks[row] & mask) || (colMasks[col] & mask) || (boxMasks[box] & mask)) {
                std::cout << "Invalid board: duplicate digit " << digit
                          << " conflicts at row " << row + 1
                          << ", column " << col + 1 << ".\n";
                return false;
            }

            rowMasks[row] |= mask;
            colMasks[col] |= mask;
            boxMasks[box] |= mask;
        }
    }

    return true;
}

Mask getCandidatesMask(int row,
                       int col,
                       const std::array<Mask, 9>& rowMasks,
                       const std::array<Mask, 9>& colMasks,
                       const std::array<Mask, 9>& boxMasks,
                       SolverStats* stats = nullptr) {
    int box = getBoxIndex(row, col);
    Mask usedDigits = static_cast<Mask>(rowMasks[row] | colMasks[col] | boxMasks[box]);

    if (stats != nullptr) {
        stats->candidateEliminations += countBits(static_cast<Mask>(usedDigits & ALL_DIGITS_MASK));
    }

    return static_cast<Mask>(ALL_DIGITS_MASK & ~usedDigits);
}

void placeDigit(Board& board,
                std::array<Mask, 9>& rowMasks,
                std::array<Mask, 9>& colMasks,
                std::array<Mask, 9>& boxMasks,
                std::vector<Placement>& placements,
                int row,
                int col,
                int digit) {
    Mask mask = digitMask(digit);
    int box = getBoxIndex(row, col);

    board[row][col] = digit;
    rowMasks[row] |= mask;
    colMasks[col] |= mask;
    boxMasks[box] |= mask;
    placements.push_back({row, col, digit});
}

void undoPlacements(Board& board,
                    std::array<Mask, 9>& rowMasks,
                    std::array<Mask, 9>& colMasks,
                    std::array<Mask, 9>& boxMasks,
                    std::vector<Placement>& placements,
                    std::size_t checkpoint) {
    while (placements.size() > checkpoint) {
        Placement placement = placements.back();
        placements.pop_back();

        Mask mask = digitMask(placement.digit);
        int box = getBoxIndex(placement.row, placement.col);

        board[placement.row][placement.col] = EMPTY;
        rowMasks[placement.row] &= static_cast<Mask>(~mask);
        colMasks[placement.col] &= static_cast<Mask>(~mask);
        boxMasks[box] &= static_cast<Mask>(~mask);
    }
}

CellChoice findFirstEmptyCell(const Board& board,
                              const std::array<Mask, 9>& rowMasks,
                              const std::array<Mask, 9>& colMasks,
                              const std::array<Mask, 9>& boxMasks,
                              SolverStats& stats) {
    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            if (board[row][col] == EMPTY) {
                Mask candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks, &stats);
                return {row, col, candidates, countBits(candidates)};
            }
        }
    }

    return {};
}

CellChoice findBestEmptyCell(const Board& board,
                             const std::array<Mask, 9>& rowMasks,
                             const std::array<Mask, 9>& colMasks,
                             const std::array<Mask, 9>& boxMasks,
                             SolverStats& stats) {
    CellChoice best;
    best.candidateCount = 10;

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            if (board[row][col] != EMPTY) {
                continue;
            }

            Mask candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks, &stats);
            int count = countBits(candidates);

            if (count < best.candidateCount) {
                best = {row, col, candidates, count};

                if (count == 0) {
                    return best;
                }
            }
        }
    }

    return best;
}

bool applyNakedSingles(Board& board,
                       std::array<Mask, 9>& rowMasks,
                       std::array<Mask, 9>& colMasks,
                       std::array<Mask, 9>& boxMasks,
                       std::vector<Placement>& placements,
                       SolverStats& stats,
                       bool& changed,
                       const FixedCells& fixedCells,
                       const SolverOptions& options,
                       int depth) {
    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            if (board[row][col] != EMPTY) {
                continue;
            }

            Mask candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks, &stats);
            int count = countBits(candidates);

            if (count == 0) {
                return false;
            }

            if (count == 1) {
                int digit = maskToDigit(candidates);
                placeDigit(board, rowMasks, colMasks, boxMasks, placements, row, col, digit);
                ++stats.nakedSingles;
                changed = true;
                renderSolverFrame(board, fixedCells, stats,
                                  {row, col, CellEvent::Propagation,
                                   "Naked single: placed " + std::to_string(digit)},
                                  options, depth);
            }
        }
    }

    return true;
}

bool placeHiddenSingleInCell(Board& board,
                             std::array<Mask, 9>& rowMasks,
                             std::array<Mask, 9>& colMasks,
                             std::array<Mask, 9>& boxMasks,
                             std::vector<Placement>& placements,
                             SolverStats& stats,
                             bool& changed,
                             const FixedCells& fixedCells,
                             const SolverOptions& options,
                             int depth,
                             int row,
                             int col,
                             int digit) {
    if (board[row][col] != EMPTY) {
        return true;
    }

    Mask candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks, &stats);
    if ((candidates & digitMask(digit)) == 0) {
        return false;
    }

    placeDigit(board, rowMasks, colMasks, boxMasks, placements, row, col, digit);
    ++stats.hiddenSingles;
    changed = true;
    renderSolverFrame(board, fixedCells, stats,
                      {row, col, CellEvent::Propagation,
                       "Hidden single: placed " + std::to_string(digit)},
                      options, depth);
    return true;
}

bool applyHiddenSinglesToUnit(Board& board,
                              std::array<Mask, 9>& rowMasks,
                              std::array<Mask, 9>& colMasks,
                              std::array<Mask, 9>& boxMasks,
                              std::vector<Placement>& placements,
                              SolverStats& stats,
                              bool& changed,
                              const FixedCells& fixedCells,
                              const SolverOptions& options,
                              int depth,
                              const std::array<std::pair<int, int>, 9>& cells,
                              Mask unitMask) {
    for (int digit = 1; digit <= 9; ++digit) {
        if (unitMask & digitMask(digit)) {
            continue;
        }

        int possibleCount = 0;
        int onlyRow = -1;
        int onlyCol = -1;

        for (const auto& [row, col] : cells) {
            if (board[row][col] != EMPTY) {
                continue;
            }

            Mask candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks, &stats);
            if (candidates & digitMask(digit)) {
                ++possibleCount;
                onlyRow = row;
                onlyCol = col;
            }
        }

        if (possibleCount == 0) {
            return false;
        }

        if (possibleCount == 1) {
            if (!placeHiddenSingleInCell(board, rowMasks, colMasks, boxMasks, placements,
                                         stats, changed, fixedCells, options, depth,
                                         onlyRow, onlyCol, digit)) {
                return false;
            }
        }
    }

    return true;
}

bool applyHiddenSingles(Board& board,
                        std::array<Mask, 9>& rowMasks,
                        std::array<Mask, 9>& colMasks,
                        std::array<Mask, 9>& boxMasks,
                        std::vector<Placement>& placements,
                        SolverStats& stats,
                        bool& changed,
                        const FixedCells& fixedCells,
                        const SolverOptions& options,
                        int depth) {
    for (int row = 0; row < SIZE; ++row) {
        std::array<std::pair<int, int>, 9> cells{};
        for (int col = 0; col < SIZE; ++col) {
            cells[col] = {row, col};
        }
        if (!applyHiddenSinglesToUnit(board, rowMasks, colMasks, boxMasks, placements,
                                      stats, changed, fixedCells, options, depth,
                                      cells, rowMasks[row])) {
            return false;
        }
    }

    for (int col = 0; col < SIZE; ++col) {
        std::array<std::pair<int, int>, 9> cells{};
        for (int row = 0; row < SIZE; ++row) {
            cells[row] = {row, col};
        }
        if (!applyHiddenSinglesToUnit(board, rowMasks, colMasks, boxMasks, placements,
                                      stats, changed, fixedCells, options, depth,
                                      cells, colMasks[col])) {
            return false;
        }
    }

    for (int box = 0; box < SIZE; ++box) {
        std::array<std::pair<int, int>, 9> cells{};
        int startRow = (box / 3) * 3;
        int startCol = (box % 3) * 3;
        int index = 0;

        for (int rowOffset = 0; rowOffset < 3; ++rowOffset) {
            for (int colOffset = 0; colOffset < 3; ++colOffset) {
                cells[index++] = {startRow + rowOffset, startCol + colOffset};
            }
        }

        if (!applyHiddenSinglesToUnit(board, rowMasks, colMasks, boxMasks, placements,
                                      stats, changed, fixedCells, options, depth,
                                      cells, boxMasks[box])) {
            return false;
        }
    }

    return true;
}

bool propagateConstraints(Board& board,
                          std::array<Mask, 9>& rowMasks,
                          std::array<Mask, 9>& colMasks,
                          std::array<Mask, 9>& boxMasks,
                          std::vector<Placement>& placements,
                          SolverStats& stats,
                          const FixedCells& fixedCells,
                          const SolverOptions& options,
                          int depth) {
    bool changed = true;

    while (changed) {
        changed = false;

        if (!applyNakedSingles(board, rowMasks, colMasks, boxMasks, placements, stats,
                               changed, fixedCells, options, depth)) {
            return false;
        }

        if (!applyHiddenSingles(board, rowMasks, colMasks, boxMasks, placements, stats,
                                changed, fixedCells, options, depth)) {
            return false;
        }
    }

    return true;
}

int searchSolutions(Board& board,
                    std::array<Mask, 9>& rowMasks,
                    std::array<Mask, 9>& colMasks,
                    std::array<Mask, 9>& boxMasks,
                    const SolverOptions& options,
                    SolverStats& stats,
                    Board& firstSolution,
                    std::vector<Placement>& placements,
                    const FixedCells& fixedCells,
                    int depth) {
    ++stats.recursiveCalls;
    stats.maxDepth = std::max(stats.maxDepth, depth);

    std::size_t checkpoint = placements.size();

    if (options.usePropagation &&
        !propagateConstraints(board, rowMasks, colMasks, boxMasks, placements, stats,
                              fixedCells, options, depth)) {
        undoPlacements(board, rowMasks, colMasks, boxMasks, placements, checkpoint);
        ++stats.backtracks;
        return 0;
    }

    CellChoice choice = options.useMrv
        ? findBestEmptyCell(board, rowMasks, colMasks, boxMasks, stats)
        : findFirstEmptyCell(board, rowMasks, colMasks, boxMasks, stats);

    if (choice.row == -1) {
        firstSolution = board;
        renderSolverFrame(board, fixedCells, stats,
                          {-1, -1, CellEvent::Solution, "Solved board found"},
                          options, depth);
        undoPlacements(board, rowMasks, colMasks, boxMasks, placements, checkpoint);
        return 1;
    }

    if (choice.candidateCount == 0) {
        undoPlacements(board, rowMasks, colMasks, boxMasks, placements, checkpoint);
        ++stats.backtracks;
        return 0;
    }

    int solutions = 0;

    if (options.useOptimizedIteration) {
        for (Mask candidates = choice.candidatesMask; candidates != 0;) {
            Mask lowestBit = static_cast<Mask>(candidates & -candidates);
            int digit = maskToDigit(lowestBit);
            std::size_t guessCheckpoint = placements.size();

            placeDigit(board, rowMasks, colMasks, boxMasks, placements, choice.row, choice.col, digit);
            renderSolverFrame(board, fixedCells, stats,
                              {choice.row, choice.col, CellEvent::Guess,
                               "Guess: trying " + std::to_string(digit)},
                              options, depth);
            int branchSolutions = searchSolutions(board, rowMasks, colMasks, boxMasks, options, stats,
                                                  firstSolution, placements, fixedCells, depth + 1);
            solutions += branchSolutions;

            if (branchSolutions == 0) {
                renderSolverFrame(board, fixedCells, stats,
                                  {choice.row, choice.col, CellEvent::Rollback,
                                   "Rollback: undo " + std::to_string(digit)},
                                  options, depth);
            }
            undoPlacements(board, rowMasks, colMasks, boxMasks, placements, guessCheckpoint);

            if (solutions >= options.solutionLimit) {
                break;
            }

            candidates = static_cast<Mask>(candidates & (candidates - 1));
        }
    } else {
        for (int digit = 1; digit <= 9; ++digit) {
            Mask mask = digitMask(digit);
            if ((choice.candidatesMask & mask) == 0) {
                continue;
            }

            std::size_t guessCheckpoint = placements.size();
            placeDigit(board, rowMasks, colMasks, boxMasks, placements, choice.row, choice.col, digit);
            renderSolverFrame(board, fixedCells, stats,
                              {choice.row, choice.col, CellEvent::Guess,
                               "Guess: trying " + std::to_string(digit)},
                              options, depth);
            int branchSolutions = searchSolutions(board, rowMasks, colMasks, boxMasks, options, stats,
                                                  firstSolution, placements, fixedCells, depth + 1);
            solutions += branchSolutions;

            if (branchSolutions == 0) {
                renderSolverFrame(board, fixedCells, stats,
                                  {choice.row, choice.col, CellEvent::Rollback,
                                   "Rollback: undo " + std::to_string(digit)},
                                  options, depth);
            }
            undoPlacements(board, rowMasks, colMasks, boxMasks, placements, guessCheckpoint);

            if (solutions >= options.solutionLimit) {
                break;
            }
        }
    }

    if (solutions == 0) {
        ++stats.backtracks;
    }

    undoPlacements(board, rowMasks, colMasks, boxMasks, placements, checkpoint);
    return solutions;
}

SolveReport solveSudoku(Board board, SolverOptions options = {}) {
    SolveReport report;
    std::array<Mask, 9> rowMasks{};
    std::array<Mask, 9> colMasks{};
    std::array<Mask, 9> boxMasks{};

    auto start = std::chrono::steady_clock::now();

    if (!isValidInitialBoard(board, rowMasks, colMasks, boxMasks)) {
        report.status = SolveStatus::Invalid;
        return report;
    }

    FixedCells fixedCells = makeFixedCells(board);
    std::vector<Placement> placements;
    Board firstSolution = board;
    int solutions = searchSolutions(board, rowMasks, colMasks, boxMasks, options,
                                    report.stats, firstSolution, placements, fixedCells, 0);

    auto stop = std::chrono::steady_clock::now();
    report.stats.elapsedMs = std::chrono::duration<double, std::milli>(stop - start).count();
    report.solutionsFound = solutions;
    report.solvedBoard = firstSolution;

    if (solutions == 0) {
        report.status = SolveStatus::NoSolution;
    } else if (solutions == 1) {
        report.status = SolveStatus::UniqueSolution;
    } else {
        report.status = SolveStatus::MultipleSolutions;
    }

    return report;
}

std::string statusName(SolveStatus status) {
    switch (status) {
        case SolveStatus::Invalid:
            return "invalid puzzle";
        case SolveStatus::NoSolution:
            return "no solution";
        case SolveStatus::UniqueSolution:
            return "unique solution";
        case SolveStatus::MultipleSolutions:
            return "multiple solutions";
    }

    return "unknown";
}

void printStats(const SolverStats& stats) {
    std::cout << "Recursive calls       : " << stats.recursiveCalls << '\n';
    std::cout << "Backtracks            : " << stats.backtracks << '\n';
    std::cout << "Max recursion depth   : " << stats.maxDepth << '\n';
    std::cout << "Naked singles         : " << stats.nakedSingles << '\n';
    std::cout << "Hidden singles        : " << stats.hiddenSingles << '\n';
    std::cout << "Candidate eliminations: " << stats.candidateEliminations << '\n';
    std::cout << "Solve time            : " << std::fixed << std::setprecision(3)
              << stats.elapsedMs << " ms\n";
}

void runVisualSolve(const Board& puzzle, int delayMs = 35, bool stepMode = false) {
    SolverOptions visualOptions;
    visualOptions.usePropagation = false; // Shows recursive guessing clearly for learning mode.
    visualOptions.visualMode = true;
    visualOptions.stepMode = stepMode;
    visualOptions.animationDelayMs = delayMs;
    visualOptions.solutionLimit = 1;

    SolveReport report = solveSudoku(puzzle, visualOptions);

    std::cout << "\nVisual solve finished: " << statusName(report.status) << "\n";
    printStats(report.stats);
}

void runBenchmark(const Board& puzzle) {
    struct BenchmarkCase {
        std::string name;
        SolverOptions options;
    };

    SolverOptions normalDfs;
    normalDfs.useMrv = false;
    normalDfs.usePropagation = false;
    normalDfs.useOptimizedIteration = false;

    SolverOptions mrvOnly;
    mrvOnly.usePropagation = false;

    SolverOptions mrvWithPropagation;

    SolverOptions uniquenessCheck;
    uniquenessCheck.solutionLimit = 2;

    std::array<BenchmarkCase, 4> cases{{
        {"Normal DFS, simple digit loop", normalDfs},
        {"MRV only", mrvOnly},
        {"MRV + propagation", mrvWithPropagation},
        {"Uniqueness check", uniquenessCheck},
    }};

    std::cout << "\nBenchmark:\n";
    std::cout << std::left << std::setw(30) << "Mode"
              << std::right << std::setw(12) << "Calls"
              << std::setw(12) << "Backtracks"
              << std::setw(12) << "Depth"
              << std::setw(12) << "Time(ms)"
              << "  Result\n";

    for (const auto& benchmarkCase : cases) {
        SolveReport report = solveSudoku(puzzle, benchmarkCase.options);

        std::cout << std::left << std::setw(30) << benchmarkCase.name
                  << std::right << std::setw(12) << report.stats.recursiveCalls
                  << std::setw(12) << report.stats.backtracks
                  << std::setw(12) << report.stats.maxDepth
                  << std::setw(12) << std::fixed << std::setprecision(3) << report.stats.elapsedMs
                  << "  " << statusName(report.status) << '\n';
    }
}

int main() {
    Board puzzle = {{
        {{5, 3, 0, 0, 7, 0, 0, 0, 0}},
        {{6, 0, 0, 1, 9, 5, 0, 0, 0}},
        {{0, 9, 8, 0, 0, 0, 0, 6, 0}},
        {{8, 0, 0, 0, 6, 0, 0, 0, 3}},
        {{4, 0, 0, 8, 0, 3, 0, 0, 1}},
        {{7, 0, 0, 0, 2, 0, 0, 0, 6}},
        {{0, 6, 0, 0, 0, 0, 2, 8, 0}},
        {{0, 0, 0, 4, 1, 9, 0, 0, 5}},
        {{0, 0, 0, 0, 8, 0, 0, 7, 9}}
    }};

    std::cout << "Original Sudoku:\n";
    printBoard(puzzle);

    SolverOptions options;
    options.solutionLimit = 2; // Search for a second solution so we can report uniqueness.

    SolveReport report = solveSudoku(puzzle, options);

    std::cout << "\nResult: " << statusName(report.status) << "\n\n";

    if (report.status == SolveStatus::UniqueSolution ||
        report.status == SolveStatus::MultipleSolutions) {
        std::cout << "Solved Sudoku:\n";
        printBoard(report.solvedBoard);

        std::cout << "\nColored terminal view:\n";
        renderBoard(report.solvedBoard, makeFixedCells(puzzle), {}, options);
    }

    std::cout << '\n';
    printStats(report.stats);

    runBenchmark(puzzle);

    std::cout << "\nVisual mode is available through runVisualSolve(puzzle, delayMs, stepMode).\n";
    std::cout << "For example, call runVisualSolve(puzzle, 50, false) from main to watch recursion.\n";

    return 0;
}
