#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
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
    bool dashboardMode = true;
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

enum class Difficulty {
    Easy,
    Medium,
    Hard,
    Expert
};

struct SolveReport {
    SolveStatus status = SolveStatus::NoSolution;
    int solutionsFound = 0;
    Board solvedBoard{};
    SolverStats stats{};
};

struct DifficultyAnalysis {
    Difficulty estimatedDifficulty = Difficulty::Medium;
    SolveStatus status = SolveStatus::NoSolution;
    int score = 0;
    int clues = 0;
    int blanks = 0;
    SolverStats propagationStats{};
    SolverStats searchStats{};
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
    constexpr const char* HIDE_CURSOR = "\033[?25l";
    constexpr const char* SHOW_CURSOR = "\033[?25h";
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

std::string eventName(CellEvent event) {
    switch (event) {
        case CellEvent::Guess:
            return "guess";
        case CellEvent::Propagation:
            return "propagation";
        case CellEvent::Rollback:
            return "rollback";
        case CellEvent::Solution:
            return "solution";
        case CellEvent::None:
            return "idle";
    }

    return "unknown";
}

int countFilledCells(const Board& board) {
    int filled = 0;

    for (const auto& row : board) {
        for (int value : row) {
            if (value != EMPTY) {
                ++filled;
            }
        }
    }

    return filled;
}

std::size_t visibleLength(const std::string& text) {
    std::size_t length = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
            i += 2;
            while (i < text.size() && !std::isalpha(static_cast<unsigned char>(text[i]))) {
                ++i;
            }
            continue;
        }

        ++length;
    }

    return length;
}

std::string padRight(std::string text, std::size_t width) {
    std::size_t visible = visibleLength(text);
    if (visible < width) {
        text += std::string(width - visible, ' ');
    }

    return text;
}

std::vector<std::string> boardLines(const Board& board,
                                    const FixedCells& fixedCells,
                                    const RenderState& state,
                                    const SolverOptions& options) {
    std::vector<std::string> lines;
    lines.push_back("+-------+-------+-------+");

    for (int row = 0; row < SIZE; ++row) {
        std::string line = "| ";
        for (int col = 0; col < SIZE; ++col) {
            bool highlighted = row == state.row && col == state.col;
            line += styledDigit(board[row][col], fixedCells[row][col], highlighted, state.event, options);
            line += ' ';

            if ((col + 1) % 3 == 0) {
                line += "| ";
            }
        }
        lines.push_back(line);

        if ((row + 1) % 3 == 0) {
            lines.push_back("+-------+-------+-------+");
        }
    }

    return lines;
}

std::vector<std::string> dashboardPanelLines(const Board& board,
                                             const SolverStats& stats,
                                             const RenderState& state,
                                             const SolverOptions& options,
                                             const std::vector<std::string>& eventLog,
                                             int depth) {
    std::vector<std::string> lines;
    int filled = countFilledCells(board);

    lines.push_back(colorize("ENGINE STATUS", Ansi::BOLD, options));
    lines.push_back("event     : " + eventName(state.event));
    lines.push_back("cell      : " + (state.row == -1 ? std::string("-")
                                                       : "r" + std::to_string(state.row + 1) +
                                                             " c" + std::to_string(state.col + 1)));
    lines.push_back("filled    : " + std::to_string(filled) + "/81");
    lines.push_back("depth     : " + std::to_string(depth));
    lines.push_back("max depth : " + std::to_string(stats.maxDepth));
    lines.push_back("");
    lines.push_back(colorize("SEARCH", Ansi::BOLD, options));
    lines.push_back("calls     : " + std::to_string(stats.recursiveCalls));
    lines.push_back("backtracks: " + std::to_string(stats.backtracks));
    lines.push_back("naked     : " + std::to_string(stats.nakedSingles));
    lines.push_back("hidden    : " + std::to_string(stats.hiddenSingles));
    lines.push_back("elim      : " + std::to_string(stats.candidateEliminations));
    lines.push_back("");
    lines.push_back(colorize("CONTROLS", Ansi::BOLD, options));
    lines.push_back("mode      : " + std::string(options.stepMode ? "step" : "auto"));
    lines.push_back("delay     : " + std::to_string(options.animationDelayMs) + " ms");
    lines.push_back("MRV       : " + std::string(options.useMrv ? "on" : "off"));
    lines.push_back("propagate : " + std::string(options.usePropagation ? "on" : "off"));
    lines.push_back("");
    lines.push_back(colorize("EVENT LOG", Ansi::BOLD, options));

    for (const std::string& event : eventLog) {
        lines.push_back(event);
    }

    return lines;
}

void trimEventLog(std::vector<std::string>& eventLog, std::size_t maxEvents) {
    while (eventLog.size() > maxEvents) {
        eventLog.erase(eventLog.begin());
    }
}

void renderDashboard(const Board& board,
                     const FixedCells& fixedCells,
                     const SolverStats& stats,
                     const RenderState& state,
                     const SolverOptions& options,
                     const std::vector<std::string>& eventLog,
                     int depth) {
    std::vector<std::string> left = boardLines(board, fixedCells, state, options);
    std::vector<std::string> right = dashboardPanelLines(board, stats, state, options, eventLog, depth);
    std::size_t rows = std::max(left.size(), right.size());

    std::cout << colorize("Sudoku Solver Visual Dashboard\n", Ansi::BOLD, options);
    std::cout << colorize("fixed=cyan  solved=green  guess=yellow  propagation=blue  rollback=red\n\n",
                          Ansi::DIM, options);

    for (std::size_t row = 0; row < rows; ++row) {
        std::string leftText = row < left.size() ? left[row] : "";
        std::string rightText = row < right.size() ? right[row] : "";
        std::cout << padRight(leftText, 34) << rightText << '\n';
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

    static std::vector<std::string> eventLog;
    static bool hasActiveSession = false;

    if (!hasActiveSession && stats.recursiveCalls <= 1 && depth == 0 && state.event != CellEvent::None) {
        hasActiveSession = true;
        eventLog.clear();
        eventLog.push_back("visual session started");
    }

    if (!state.message.empty()) {
        eventLog.push_back(state.message);
        trimEventLog(eventLog, 8);
    }

    if (options.clearBetweenFrames) {
        std::cout << Ansi::CLEAR;
    }

    if (options.dashboardMode) {
        renderDashboard(board, fixedCells, stats, state, options, eventLog, depth);
    } else {
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
    }

    if (options.stepMode) {
        std::cout << "Press Enter for next step...";
        std::cin.get();
    } else if (options.animationDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(options.animationDelayMs));
    }

    if (state.event == CellEvent::Solution) {
        hasActiveSession = false;
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

std::string difficultyName(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return "Easy";
        case Difficulty::Medium:
            return "Medium";
        case Difficulty::Hard:
            return "Hard";
        case Difficulty::Expert:
            return "Expert";
    }

    return "Unknown";
}

int targetCluesForDifficulty(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return 42;
        case Difficulty::Medium:
            return 36;
        case Difficulty::Hard:
            return 30;
        case Difficulty::Expert:
            return 26;
    }

    return 36;
}

int difficultyRank(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return 0;
        case Difficulty::Medium:
            return 1;
        case Difficulty::Hard:
            return 2;
        case Difficulty::Expert:
            return 3;
    }

    return 1;
}

Difficulty difficultyFromScore(int score) {
    if (score <= 40) {
        return Difficulty::Easy;
    }
    if (score <= 47) {
        return Difficulty::Medium;
    }
    if (score <= 54) {
        return Difficulty::Hard;
    }

    return Difficulty::Expert;
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

void waitForEnter() {
    std::cout << "\nPress Enter to return to the menu...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int readIntInRange(const std::string& prompt, int minValue, int maxValue) {
    while (true) {
        std::cout << prompt;

        int value = 0;
        if (std::cin >> value && value >= minValue && value <= maxValue) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }

        std::cout << "Please enter a number from " << minValue << " to " << maxValue << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

bool readYesNo(const std::string& prompt) {
    while (true) {
        std::cout << prompt;

        char answer = '\0';
        if (std::cin >> answer) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            answer = static_cast<char>(std::tolower(static_cast<unsigned char>(answer)));
            if (answer == 'y') {
                return true;
            }
            if (answer == 'n') {
                return false;
            }
        }

        std::cout << "Please enter y or n.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void clearScreen() {
    std::cout << Ansi::CLEAR;
}

void printAppHeader() {
    std::cout << colorize("Sudoku Solver Pro\n", Ansi::BOLD, SolverOptions{});
    std::cout << "Backtracking | Bitmasks | MRV | Propagation | Terminal UI\n\n";
}

void showMenu() {
    clearScreen();
    printAppHeader();
    std::cout << "1. Show current puzzle\n";
    std::cout << "2. Enter custom puzzle\n";
    std::cout << "3. Load puzzle from file\n";
    std::cout << "4. Save current puzzle to file\n";
    std::cout << "5. Save solved report to file\n";
    std::cout << "6. Generate new puzzle\n";
    std::cout << "7. Reset to sample puzzle\n";
    std::cout << "8. Validate current puzzle\n";
    std::cout << "9. Solve instantly\n";
    std::cout << "10. Visual solve dashboard\n";
    std::cout << "11. Step-by-step solve\n";
    std::cout << "12. Benchmark solver modes\n";
    std::cout << "13. About engine\n";
    std::cout << "14. Exit\n\n";
}

void showCurrentPuzzle(const Board& puzzle) {
    clearScreen();
    printAppHeader();
    std::cout << "Current puzzle:\n";
    printBoard(puzzle);
    waitForEnter();
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

bool parsePuzzleText(const std::string& text, Board& board, std::string& errorMessage) {
    std::string cells;

    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
            cells.push_back(ch);
        } else if (!std::isspace(static_cast<unsigned char>(ch))) {
            errorMessage = "Only digits, '.', and whitespace are allowed.";
            return false;
        }
    }

    if (cells.size() != 81) {
        errorMessage = "Expected exactly 81 cells, but received " + std::to_string(cells.size()) + ".";
        return false;
    }

    Board parsed{};
    for (std::size_t index = 0; index < cells.size(); ++index) {
        char ch = cells[index];
        int row = static_cast<int>(index / SIZE);
        int col = static_cast<int>(index % SIZE);

        parsed[row][col] = (ch == '.') ? EMPTY : ch - '0';
    }

    board = parsed;
    return true;
}

bool isStructurallyValidPuzzle(const Board& puzzle) {
    std::array<Mask, 9> rowMasks{};
    std::array<Mask, 9> colMasks{};
    std::array<Mask, 9> boxMasks{};
    return isValidInitialBoard(puzzle, rowMasks, colMasks, boxMasks);
}

std::string boardToCompactText(const Board& board) {
    std::ostringstream out;

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            out << board[row][col];
        }
        out << '\n';
    }

    return out.str();
}

std::string boardToPrettyText(const Board& board) {
    std::ostringstream out;

    out << "+-------+-------+-------+\n";
    for (int row = 0; row < SIZE; ++row) {
        out << "| ";
        for (int col = 0; col < SIZE; ++col) {
            out << (board[row][col] == EMPTY ? '.' : static_cast<char>('0' + board[row][col])) << ' ';

            if ((col + 1) % 3 == 0) {
                out << "| ";
            }
        }
        out << '\n';

        if ((row + 1) % 3 == 0) {
            out << "+-------+-------+-------+\n";
        }
    }

    return out.str();
}

std::string statsToText(const SolverStats& stats) {
    std::ostringstream out;

    out << "Recursive calls       : " << stats.recursiveCalls << '\n';
    out << "Backtracks            : " << stats.backtracks << '\n';
    out << "Max recursion depth   : " << stats.maxDepth << '\n';
    out << "Naked singles         : " << stats.nakedSingles << '\n';
    out << "Hidden singles        : " << stats.hiddenSingles << '\n';
    out << "Candidate eliminations: " << stats.candidateEliminations << '\n';
    out << "Solve time            : " << std::fixed << std::setprecision(3)
        << stats.elapsedMs << " ms\n";

    return out.str();
}

DifficultyAnalysis analyzeDifficulty(const Board& puzzle) {
    DifficultyAnalysis analysis;
    analysis.clues = countFilledCells(puzzle);
    analysis.blanks = SIZE * SIZE - analysis.clues;

    SolverOptions propagationOptions;
    propagationOptions.solutionLimit = 2;
    SolveReport propagationReport = solveSudoku(puzzle, propagationOptions);

    analysis.status = propagationReport.status;
    analysis.propagationStats = propagationReport.stats;

    if (propagationReport.status != SolveStatus::UniqueSolution) {
        analysis.estimatedDifficulty = Difficulty::Expert;
        analysis.score = 100;
        return analysis;
    }

    SolverOptions searchOptions;
    searchOptions.usePropagation = false;
    searchOptions.solutionLimit = 1;
    SolveReport searchReport = solveSudoku(puzzle, searchOptions);
    analysis.searchStats = searchReport.stats;

    int propagationGuessPenalty = propagationReport.stats.maxDepth * 8;
    int propagationBacktrackPenalty = static_cast<int>(std::min<long long>(40, propagationReport.stats.backtracks * 10));
    int rawSearchPenalty = static_cast<int>(std::min<long long>(25, searchReport.stats.backtracks / 12));
    int cluePressure = analysis.blanks;

    analysis.score = cluePressure + propagationGuessPenalty + propagationBacktrackPenalty + rawSearchPenalty;
    analysis.estimatedDifficulty = difficultyFromScore(analysis.score);

    return analysis;
}

std::string difficultyAnalysisToText(const DifficultyAnalysis& analysis) {
    std::ostringstream out;

    out << "Estimated difficulty : " << difficultyName(analysis.estimatedDifficulty) << '\n';
    out << "Difficulty score     : " << analysis.score << '\n';
    out << "Clues                : " << analysis.clues << '\n';
    out << "Empty cells          : " << analysis.blanks << '\n';
    out << "Status               : " << statusName(analysis.status) << '\n';
    out << "Propagation depth    : " << analysis.propagationStats.maxDepth << '\n';
    out << "Propagation backs    : " << analysis.propagationStats.backtracks << '\n';
    out << "Raw search backs     : " << analysis.searchStats.backtracks << '\n';

    return out.str();
}

void printDifficultyAnalysis(const DifficultyAnalysis& analysis) {
    std::cout << difficultyAnalysisToText(analysis);
}

std::string readTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open file for reading: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Could not open file for writing: " + path);
    }

    output << text;
    if (!output) {
        throw std::runtime_error("Failed while writing file: " + path);
    }
}

void validateCurrentPuzzle(const Board& puzzle) {
    clearScreen();
    printAppHeader();

    std::cout << "Current puzzle:\n";
    printBoard(puzzle);
    std::cout << '\n';

    if (!isStructurallyValidPuzzle(puzzle)) {
        std::cout << "\nValidation result: invalid board structure.\n";
        waitForEnter();
        return;
    }

    SolverOptions options;
    options.solutionLimit = 2;
    SolveReport report = solveSudoku(puzzle, options);

    std::cout << "Validation result: " << statusName(report.status) << '\n';
    if (report.status == SolveStatus::MultipleSolutions) {
        std::cout << "This is playable, but not a well-formed Sudoku puzzle because it has more than one solution.\n";
    }

    if (report.status == SolveStatus::UniqueSolution) {
        std::cout << '\n';
        printDifficultyAnalysis(analyzeDifficulty(puzzle));
    }

    waitForEnter();
}

Board readPuzzleFromUser() {
    clearScreen();
    printAppHeader();

    std::cout << "Puzzle input format:\n";
    std::cout << "- Use digits 1-9 for clues.\n";
    std::cout << "- Use 0 or . for empty cells.\n";
    std::cout << "- Spaces are ignored.\n\n";

    std::cout << "1. Enter one 81-character line\n";
    std::cout << "2. Enter 9 rows\n\n";

    int mode = readIntInRange("Choose input mode: ", 1, 2);
    std::string text;

    if (mode == 1) {
        std::cout << "\nEnter puzzle: ";
        std::getline(std::cin, text);
    } else {
        std::cout << "\nEnter 9 rows:\n";
        for (int row = 0; row < SIZE; ++row) {
            std::string line;
            std::cout << "Row " << row + 1 << ": ";
            std::getline(std::cin, line);
            text += line;
            text += '\n';
        }
    }

    Board parsed{};
    std::string errorMessage;
    if (!parsePuzzleText(text, parsed, errorMessage)) {
        throw std::runtime_error(errorMessage);
    }

    return parsed;
}

void enterCustomPuzzle(Board& puzzle) {
    try {
        Board candidate = readPuzzleFromUser();

        clearScreen();
        printAppHeader();
        std::cout << "Parsed puzzle:\n";
        printBoard(candidate);
        std::cout << '\n';

        if (!isStructurallyValidPuzzle(candidate)) {
            std::cout << "\nPuzzle was not loaded because it has invalid Sudoku conflicts.\n";
            waitForEnter();
            return;
        }

        SolverOptions options;
        options.solutionLimit = 2;
        SolveReport report = solveSudoku(candidate, options);

        if (report.status == SolveStatus::NoSolution || report.status == SolveStatus::Invalid) {
            std::cout << "Puzzle was not loaded because it has no valid solution.\n";
            waitForEnter();
            return;
        }

        puzzle = candidate;
        std::cout << "Puzzle loaded successfully: " << statusName(report.status) << ".\n";
        if (report.status == SolveStatus::MultipleSolutions) {
            std::cout << "Warning: this puzzle has multiple solutions.\n";
        }
    } catch (const std::exception& ex) {
        clearScreen();
        printAppHeader();
        std::cout << "Puzzle input failed: " << ex.what() << '\n';
    }

    waitForEnter();
}

bool validateCandidateForLoading(const Board& candidate) {
    if (!isStructurallyValidPuzzle(candidate)) {
        std::cout << "\nPuzzle was not loaded because it has invalid Sudoku conflicts.\n";
        return false;
    }

    SolverOptions options;
    options.solutionLimit = 2;
    SolveReport report = solveSudoku(candidate, options);

    if (report.status == SolveStatus::NoSolution || report.status == SolveStatus::Invalid) {
        std::cout << "Puzzle was not loaded because it has no valid solution.\n";
        return false;
    }

    std::cout << "Puzzle accepted: " << statusName(report.status) << ".\n";
    if (report.status == SolveStatus::MultipleSolutions) {
        std::cout << "Warning: this puzzle has multiple solutions.\n";
    }

    return true;
}

void loadPuzzleFromFile(Board& puzzle) {
    clearScreen();
    printAppHeader();

    try {
        std::string path = readLine("Enter puzzle file path: ");
        std::string text = readTextFile(path);

        Board candidate{};
        std::string errorMessage;
        if (!parsePuzzleText(text, candidate, errorMessage)) {
            throw std::runtime_error(errorMessage);
        }

        std::cout << "\nLoaded file contents as puzzle:\n";
        printBoard(candidate);
        std::cout << '\n';

        if (validateCandidateForLoading(candidate)) {
            puzzle = candidate;
        }
    } catch (const std::exception& ex) {
        std::cout << "\nFile load failed: " << ex.what() << '\n';
    }

    waitForEnter();
}

void saveCurrentPuzzleToFile(const Board& puzzle) {
    clearScreen();
    printAppHeader();

    try {
        std::string path = readLine("Save current puzzle to file path: ");
        writeTextFile(path, boardToCompactText(puzzle));
        std::cout << "\nCurrent puzzle saved to: " << path << '\n';
    } catch (const std::exception& ex) {
        std::cout << "\nSave failed: " << ex.what() << '\n';
    }

    waitForEnter();
}

void saveSolvedReportToFile(const Board& puzzle) {
    clearScreen();
    printAppHeader();

    try {
        SolverOptions options;
        options.solutionLimit = 2;
        SolveReport report = solveSudoku(puzzle, options);

        if (report.status == SolveStatus::NoSolution || report.status == SolveStatus::Invalid) {
            std::cout << "Solved report was not saved because the current puzzle is "
                      << statusName(report.status) << ".\n";
            waitForEnter();
            return;
        }

        std::string path = readLine("Save solved report to file path: ");

        std::ostringstream out;
        out << "Sudoku Solver Pro Report\n";
        out << "Status: " << statusName(report.status) << "\n\n";
        out << "Original puzzle:\n";
        out << boardToPrettyText(puzzle) << '\n';
        out << "Solved puzzle:\n";
        out << boardToPrettyText(report.solvedBoard) << '\n';
        out << "Compact solved puzzle:\n";
        out << boardToCompactText(report.solvedBoard) << '\n';
        out << "Difficulty analysis:\n";
        out << difficultyAnalysisToText(analyzeDifficulty(puzzle)) << '\n';
        out << "Statistics:\n";
        out << statsToText(report.stats);

        writeTextFile(path, out.str());
        std::cout << "\nSolved report saved to: " << path << '\n';
    } catch (const std::exception& ex) {
        std::cout << "\nSave failed: " << ex.what() << '\n';
    }

    waitForEnter();
}

bool fillCompleteBoard(Board& board,
                       std::array<Mask, 9>& rowMasks,
                       std::array<Mask, 9>& colMasks,
                       std::array<Mask, 9>& boxMasks,
                       std::mt19937& rng) {
    SolverStats ignoredStats;
    CellChoice choice = findBestEmptyCell(board, rowMasks, colMasks, boxMasks, ignoredStats);
    if (choice.row == -1) {
        return true;
    }

    std::vector<int> digits;
    for (Mask candidates = choice.candidatesMask; candidates != 0;) {
        Mask lowestBit = static_cast<Mask>(candidates & -candidates);
        digits.push_back(maskToDigit(lowestBit));
        candidates = static_cast<Mask>(candidates & (candidates - 1));
    }

    std::shuffle(digits.begin(), digits.end(), rng);

    for (int digit : digits) {
        Mask mask = digitMask(digit);
        int box = getBoxIndex(choice.row, choice.col);

        board[choice.row][choice.col] = digit;
        rowMasks[choice.row] |= mask;
        colMasks[choice.col] |= mask;
        boxMasks[box] |= mask;

        if (fillCompleteBoard(board, rowMasks, colMasks, boxMasks, rng)) {
            return true;
        }

        board[choice.row][choice.col] = EMPTY;
        rowMasks[choice.row] &= static_cast<Mask>(~mask);
        colMasks[choice.col] &= static_cast<Mask>(~mask);
        boxMasks[box] &= static_cast<Mask>(~mask);
    }

    return false;
}

Board generateCompleteBoard(std::mt19937& rng) {
    Board board{};
    std::array<Mask, 9> rowMasks{};
    std::array<Mask, 9> colMasks{};
    std::array<Mask, 9> boxMasks{};

    if (!fillCompleteBoard(board, rowMasks, colMasks, boxMasks, rng)) {
        throw std::runtime_error("Failed to generate a complete Sudoku board.");
    }

    return board;
}

int countClues(const Board& board) {
    return countFilledCells(board);
}

bool hasUniqueSolution(const Board& puzzle) {
    SolverOptions options;
    options.solutionLimit = 2;
    SolveReport report = solveSudoku(puzzle, options);
    return report.status == SolveStatus::UniqueSolution;
}

std::vector<int> shuffledCellIndices(std::mt19937& rng) {
    std::vector<int> indices;
    indices.reserve(SIZE * SIZE);

    for (int index = 0; index < SIZE * SIZE; ++index) {
        indices.push_back(index);
    }

    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

Board removeCluesPreservingUniqueness(Board solvedBoard,
                                      Difficulty difficulty,
                                      std::mt19937& rng,
                                      int& removedCells) {
    Board puzzle = solvedBoard;
    int targetClues = targetCluesForDifficulty(difficulty);
    removedCells = 0;

    for (int index : shuffledCellIndices(rng)) {
        if (countClues(puzzle) <= targetClues) {
            break;
        }

        int row = index / SIZE;
        int col = index % SIZE;
        int oldValue = puzzle[row][col];

        if (oldValue == EMPTY) {
            continue;
        }

        puzzle[row][col] = EMPTY;

        if (hasUniqueSolution(puzzle)) {
            ++removedCells;
        } else {
            puzzle[row][col] = oldValue;
        }
    }

    return puzzle;
}

struct GeneratedPuzzle {
    Board puzzle{};
    Board solution{};
    Difficulty requestedDifficulty = Difficulty::Medium;
    DifficultyAnalysis analysis{};
    int removedCells = 0;
    double elapsedMs = 0.0;
};

int difficultyDistance(Difficulty lhs, Difficulty rhs) {
    int diff = difficultyRank(lhs) - difficultyRank(rhs);
    return diff < 0 ? -diff : diff;
}

GeneratedPuzzle generatePuzzleCandidate(Difficulty difficulty, std::mt19937& rng) {
    auto start = std::chrono::steady_clock::now();

    Board solvedBoard = generateCompleteBoard(rng);
    int removedCells = 0;
    Board generated = removeCluesPreservingUniqueness(solvedBoard, difficulty, rng, removedCells);
    DifficultyAnalysis analysis = analyzeDifficulty(generated);

    auto stop = std::chrono::steady_clock::now();

    GeneratedPuzzle result;
    result.puzzle = generated;
    result.solution = solvedBoard;
    result.requestedDifficulty = difficulty;
    result.analysis = analysis;
    result.removedCells = removedCells;
    result.elapsedMs = std::chrono::duration<double, std::milli>(stop - start).count();
    return result;
}

GeneratedPuzzle generateBestMatchingPuzzle(Difficulty difficulty, std::mt19937& rng, int maxAttempts) {
    GeneratedPuzzle best;
    bool hasBest = false;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        GeneratedPuzzle candidate = generatePuzzleCandidate(difficulty, rng);

        if (!hasBest ||
            difficultyDistance(candidate.analysis.estimatedDifficulty, difficulty) <
                difficultyDistance(best.analysis.estimatedDifficulty, difficulty) ||
            (difficultyDistance(candidate.analysis.estimatedDifficulty, difficulty) ==
                 difficultyDistance(best.analysis.estimatedDifficulty, difficulty) &&
             candidate.analysis.score > best.analysis.score)) {
            best = candidate;
            hasBest = true;
        }

        if (candidate.analysis.estimatedDifficulty == difficulty) {
            break;
        }
    }

    return best;
}

Difficulty chooseDifficulty() {
    std::cout << "1. Easy   (about 42 clues)\n";
    std::cout << "2. Medium (about 36 clues)\n";
    std::cout << "3. Hard   (about 30 clues)\n";
    std::cout << "4. Expert (about 26 clues)\n\n";

    int choice = readIntInRange("Choose difficulty: ", 1, 4);
    switch (choice) {
        case 1:
            return Difficulty::Easy;
        case 2:
            return Difficulty::Medium;
        case 3:
            return Difficulty::Hard;
        case 4:
            return Difficulty::Expert;
    }

    return Difficulty::Medium;
}

void generatePuzzleFromMenu(Board& puzzle) {
    clearScreen();
    printAppHeader();

    Difficulty difficulty = chooseDifficulty();
    std::random_device device;
    std::mt19937 rng(device());

    std::cout << "\nGenerating a " << difficultyName(difficulty) << " puzzle...\n";
    std::cout << "This may take a moment because each removal checks uniqueness.\n\n";

    int attempts = difficulty == Difficulty::Easy ? 2 : 3;
    GeneratedPuzzle generated = generateBestMatchingPuzzle(difficulty, rng, attempts);
    puzzle = generated.puzzle;

    std::cout << "Requested difficulty : " << difficultyName(difficulty) << '\n';
    std::cout << "Measured difficulty  : " << difficultyName(generated.analysis.estimatedDifficulty) << '\n';
    std::cout << "Difficulty score     : " << generated.analysis.score << '\n';
    std::cout << "Clues kept           : " << countClues(puzzle) << '\n';
    std::cout << "Cells removed        : " << generated.removedCells << '\n';
    std::cout << "Generation time      : " << std::fixed << std::setprecision(3)
              << generated.elapsedMs << " ms\n\n";

    printBoard(puzzle);
    waitForEnter();
}

Board defaultPuzzle();

void resetToDefaultPuzzle(Board& puzzle) {
    puzzle = defaultPuzzle();
    clearScreen();
    printAppHeader();
    std::cout << "Puzzle reset to the built-in sample.\n\n";
    printBoard(puzzle);
    waitForEnter();
}

void solveInstantly(const Board& puzzle) {
    clearScreen();
    printAppHeader();

    SolverOptions options;
    options.solutionLimit = 2;

    std::cout << "Solving current puzzle...\n\n";
    SolveReport report = solveSudoku(puzzle, options);

    std::cout << "Result: " << statusName(report.status) << "\n\n";

    if (report.status == SolveStatus::UniqueSolution ||
        report.status == SolveStatus::MultipleSolutions) {
        std::cout << "Solved board:\n";
        renderBoard(report.solvedBoard, makeFixedCells(puzzle), {}, options);
        std::cout << '\n';
    }

    if (report.status == SolveStatus::UniqueSolution) {
        std::cout << "Difficulty analysis:\n";
        printDifficultyAnalysis(analyzeDifficulty(puzzle));
        std::cout << '\n';
    }

    printStats(report.stats);
    waitForEnter();
}

void runVisualSolve(const Board& puzzle, int delayMs = 35, bool stepMode = false) {
    SolverOptions visualOptions;
    visualOptions.usePropagation = false; // Shows recursive guessing clearly for learning mode.
    visualOptions.visualMode = true;
    visualOptions.dashboardMode = true;
    visualOptions.stepMode = stepMode;
    visualOptions.animationDelayMs = delayMs;
    visualOptions.solutionLimit = 1;

    std::cout << Ansi::HIDE_CURSOR;
    SolveReport report = solveSudoku(puzzle, visualOptions);
    std::cout << Ansi::SHOW_CURSOR;

    std::cout << "\nVisual solve finished: " << statusName(report.status) << "\n";
    printStats(report.stats);
}

void runVisualSolveFromMenu(const Board& puzzle, bool stepMode) {
    clearScreen();
    printAppHeader();

    int delayMs = 0;
    if (!stepMode) {
        delayMs = readIntInRange("Animation delay in milliseconds (0-500): ", 0, 500);
    }

    bool usePropagation = readYesNo("Enable propagation during visualization? (y/n): ");

    SolverOptions visualOptions;
    visualOptions.usePropagation = usePropagation;
    visualOptions.visualMode = true;
    visualOptions.dashboardMode = true;
    visualOptions.stepMode = stepMode;
    visualOptions.animationDelayMs = delayMs;
    visualOptions.solutionLimit = 1;

    std::cout << Ansi::HIDE_CURSOR;
    SolveReport report = solveSudoku(puzzle, visualOptions);
    std::cout << Ansi::SHOW_CURSOR;

    std::cout << "\nVisual solve finished: " << statusName(report.status) << "\n";
    printStats(report.stats);
    waitForEnter();
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

void benchmarkFromMenu(const Board& puzzle) {
    clearScreen();
    printAppHeader();
    runBenchmark(puzzle);
    waitForEnter();
}

void showAboutEngine() {
    clearScreen();
    printAppHeader();

    std::cout << "Engine architecture:\n";
    std::cout << "- Board: std::array based 9x9 grid, empty cells stored as 0.\n";
    std::cout << "- Constraints: row, column, and box masks track used digits.\n";
    std::cout << "- Search: recursive DFS backtracking with optional MRV.\n";
    std::cout << "- Optimization: candidate masks enumerate only legal digits.\n";
    std::cout << "- Propagation: Naked Singles and Hidden Singles reduce guessing.\n";
    std::cout << "- Reporting: unique, multiple, invalid, or unsolvable puzzle states.\n\n";

    std::cout << "Backtracking flow:\n";
    std::cout << "1. Pick an empty cell.\n";
    std::cout << "2. Generate candidates from bitmasks.\n";
    std::cout << "3. Place a candidate digit.\n";
    std::cout << "4. Recurse into the smaller puzzle.\n";
    std::cout << "5. Roll back if the branch fails.\n\n";

    std::cout << "Phase 3 focus:\n";
    std::cout << "This menu wraps the solver as an application without changing the core engine.\n";

    waitForEnter();
}

Board defaultPuzzle() {
    return {{
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
}

int main() {
    Board puzzle = defaultPuzzle();

    while (true) {
        showMenu();
        int choice = readIntInRange("Choose an option: ", 1, 14);

        switch (choice) {
            case 1:
                showCurrentPuzzle(puzzle);
                break;
            case 2:
                enterCustomPuzzle(puzzle);
                break;
            case 3:
                loadPuzzleFromFile(puzzle);
                break;
            case 4:
                saveCurrentPuzzleToFile(puzzle);
                break;
            case 5:
                saveSolvedReportToFile(puzzle);
                break;
            case 6:
                generatePuzzleFromMenu(puzzle);
                break;
            case 7:
                resetToDefaultPuzzle(puzzle);
                break;
            case 8:
                validateCurrentPuzzle(puzzle);
                break;
            case 9:
                solveInstantly(puzzle);
                break;
            case 10:
                runVisualSolveFromMenu(puzzle, false);
                break;
            case 11:
                runVisualSolveFromMenu(puzzle, true);
                break;
            case 12:
                benchmarkFromMenu(puzzle);
                break;
            case 13:
                showAboutEngine();
                break;
            case 14:
                clearScreen();
                std::cout << "Goodbye.\n";
                return 0;
        }
    }

    return 0;
}
