#include "board.hpp"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int getBoxIndex(int row, int col) {
    return (row / 3) * 3 + (col / 3);
}

Mask digitMask(int digit) {
    return static_cast<Mask>(1u << digit);
}

int maskToDigit(Mask mask) {
    return __builtin_ctz(static_cast<unsigned int>(mask));
}

int countBits(Mask mask) {
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

int countClues(const Board& board) {
    return countFilledCells(board);
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
        int row = static_cast<int>(index / SIZE);
        int col = static_cast<int>(index % SIZE);
        parsed[row][col] = cells[index] == '.' ? EMPTY : cells[index] - '0';
    }

    board = parsed;
    return true;
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

bool isStructurallyValidPuzzle(const Board& puzzle) {
    std::array<Mask, 9> rowMasks{};
    std::array<Mask, 9> colMasks{};
    std::array<Mask, 9> boxMasks{};
    return isValidInitialBoard(puzzle, rowMasks, colMasks, boxMasks);
}

Mask getCandidatesMask(int row,
                       int col,
                       const std::array<Mask, 9>& rowMasks,
                       const std::array<Mask, 9>& colMasks,
                       const std::array<Mask, 9>& boxMasks,
                       SolverStats* stats) {
    int box = getBoxIndex(row, col);
    Mask usedDigits = static_cast<Mask>(rowMasks[row] | colMasks[col] | boxMasks[box]);
    if (stats != nullptr) {
        stats->candidateEliminations += countBits(static_cast<Mask>(usedDigits & ALL_DIGITS_MASK));
    }
    return static_cast<Mask>(ALL_DIGITS_MASK & ~usedDigits);
}

void applyDigitToState(Board& board,
                       std::array<Mask, 9>& rowMasks,
                       std::array<Mask, 9>& colMasks,
                       std::array<Mask, 9>& boxMasks,
                       int row,
                       int col,
                       int digit) {
    Mask mask = digitMask(digit);
    int box = getBoxIndex(row, col);
    board[row][col] = digit;
    rowMasks[row] |= mask;
    colMasks[col] |= mask;
    boxMasks[box] |= mask;
}

void removeDigitFromState(Board& board,
                          std::array<Mask, 9>& rowMasks,
                          std::array<Mask, 9>& colMasks,
                          std::array<Mask, 9>& boxMasks,
                          int row,
                          int col,
                          int digit) {
    Mask mask = digitMask(digit);
    int box = getBoxIndex(row, col);
    board[row][col] = EMPTY;
    rowMasks[row] &= static_cast<Mask>(~mask);
    colMasks[col] &= static_cast<Mask>(~mask);
    boxMasks[box] &= static_cast<Mask>(~mask);
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
