#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using Board = std::array<std::array<int, 9>, 9>;
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

int getBoxIndex(int row, int col) {
    return (row / 3) * 3 + (col / 3);
}

Mask digitMask(int digit) {
    return static_cast<Mask>(1u << digit);
}

int maskToDigit(Mask mask) {
    for (int digit = 1; digit <= 9; ++digit) {
        if (mask & (static_cast<Mask>(1u) << digit)) {
            return digit;
        }
    }
    return -1;
}

int countBits(Mask mask) {
    int count = 0;
    while (mask) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
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
                       bool& changed) {
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
                placeDigit(board, rowMasks, colMasks, boxMasks, placements, row, col, maskToDigit(candidates));
                ++stats.nakedSingles;
                changed = true;
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
    return true;
}

bool applyHiddenSinglesToUnit(Board& board,
                              std::array<Mask, 9>& rowMasks,
                              std::array<Mask, 9>& colMasks,
                              std::array<Mask, 9>& boxMasks,
                              std::vector<Placement>& placements,
                              SolverStats& stats,
                              bool& changed,
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
                                         stats, changed, onlyRow, onlyCol, digit)) {
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
                        bool& changed) {
    for (int row = 0; row < SIZE; ++row) {
        std::array<std::pair<int, int>, 9> cells{};
        for (int col = 0; col < SIZE; ++col) {
            cells[col] = {row, col};
        }
        if (!applyHiddenSinglesToUnit(board, rowMasks, colMasks, boxMasks, placements,
                                      stats, changed, cells, rowMasks[row])) {
            return false;
        }
    }

    for (int col = 0; col < SIZE; ++col) {
        std::array<std::pair<int, int>, 9> cells{};
        for (int row = 0; row < SIZE; ++row) {
            cells[row] = {row, col};
        }
        if (!applyHiddenSinglesToUnit(board, rowMasks, colMasks, boxMasks, placements,
                                      stats, changed, cells, colMasks[col])) {
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
                                      stats, changed, cells, boxMasks[box])) {
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
                          SolverStats& stats) {
    bool changed = true;

    while (changed) {
        changed = false;

        if (!applyNakedSingles(board, rowMasks, colMasks, boxMasks, placements, stats, changed)) {
            return false;
        }

        if (!applyHiddenSingles(board, rowMasks, colMasks, boxMasks, placements, stats, changed)) {
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
                    int depth) {
    ++stats.recursiveCalls;
    stats.maxDepth = std::max(stats.maxDepth, depth);

    std::size_t checkpoint = placements.size();

    if (options.usePropagation &&
        !propagateConstraints(board, rowMasks, colMasks, boxMasks, placements, stats)) {
        undoPlacements(board, rowMasks, colMasks, boxMasks, placements, checkpoint);
        ++stats.backtracks;
        return 0;
    }

    CellChoice choice = options.useMrv
        ? findBestEmptyCell(board, rowMasks, colMasks, boxMasks, stats)
        : findFirstEmptyCell(board, rowMasks, colMasks, boxMasks, stats);

    if (choice.row == -1) {
        firstSolution = board;
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
            solutions += searchSolutions(board, rowMasks, colMasks, boxMasks, options, stats,
                                         firstSolution, placements, depth + 1);
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
            solutions += searchSolutions(board, rowMasks, colMasks, boxMasks, options, stats,
                                         firstSolution, placements, depth + 1);
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

    std::vector<Placement> placements;
    Board firstSolution = board;
    int solutions = searchSolutions(board, rowMasks, colMasks, boxMasks, options,
                                    report.stats, firstSolution, placements, 0);

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

void runBenchmark(const Board& puzzle) {
    struct BenchmarkCase {
        std::string name;
        SolverOptions options;
    };

    std::array<BenchmarkCase, 4> cases{{
        {"Normal DFS, simple digit loop", {false, false, false, 1}},
        {"MRV only", {true, false, true, 1}},
        {"MRV + propagation", {true, true, true, 1}},
        {"Uniqueness check", {true, true, true, 2}},
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
    }

    std::cout << '\n';
    printStats(report.stats);

    runBenchmark(puzzle);

    return 0;
}
