#include <array>
#include <iostream>

using Board = std::array<std::array<int, 9>, 9>;

constexpr int SIZE = 9;
constexpr int EMPTY = 0;
constexpr int ALL_DIGITS_MASK = 0b1111111110; // Bits 1 through 9 are set.

struct CellChoice {
    int row = -1;
    int col = -1;
    int candidatesMask = 0;
    int candidateCount = 0;
};

int getBoxIndex(int row, int col) {
    return (row / 3) * 3 + (col / 3);
}

int digitMask(int digit) {
    return 1 << digit;
}

int countBits(int mask) {
    int count = 0;
    while (mask != 0) {
        mask &= (mask - 1);
        ++count;
    }
    return count;
}

void printBoard(const Board& board) {
    std::cout << "+-------+-------+-------+\n";
    for (int row = 0; row < SIZE; ++row) {
        std::cout << "| ";
        for (int col = 0; col < SIZE; ++col) {
            if (board[row][col] == EMPTY) {
                std::cout << ". ";
            } else {
                std::cout << board[row][col] << ' ';
            }

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
                         std::array<int, 9>& rowMasks,
                         std::array<int, 9>& colMasks,
                         std::array<int, 9>& boxMasks) {
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
            int mask = digitMask(digit);

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

int getCandidatesMask(int row,
                      int col,
                      const std::array<int, 9>& rowMasks,
                      const std::array<int, 9>& colMasks,
                      const std::array<int, 9>& boxMasks) {
    int box = getBoxIndex(row, col);
    int usedDigits = rowMasks[row] | colMasks[col] | boxMasks[box];
    return ALL_DIGITS_MASK & ~usedDigits;
}

CellChoice findBestEmptyCell(const Board& board,
                             const std::array<int, 9>& rowMasks,
                             const std::array<int, 9>& colMasks,
                             const std::array<int, 9>& boxMasks) {
    CellChoice best;
    best.candidateCount = 10;

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            if (board[row][col] != EMPTY) {
                continue;
            }

            int candidates = getCandidatesMask(row, col, rowMasks, colMasks, boxMasks);
            int count = countBits(candidates);

            // Minimum Remaining Values: choose the hardest empty cell first.
            if (count < best.candidateCount) {
                best = {row, col, candidates, count};

                // No candidate means this path is impossible, so stop scanning early.
                if (count == 0) {
                    return best;
                }
            }
        }
    }

    return best;
}

bool solveSudoku(Board& board,
                 std::array<int, 9>& rowMasks,
                 std::array<int, 9>& colMasks,
                 std::array<int, 9>& boxMasks) {
    CellChoice choice = findBestEmptyCell(board, rowMasks, colMasks, boxMasks);

    // No empty cell remains, so the board is completely solved.
    if (choice.row == -1) {
        return true;
    }

    // An empty cell with no legal digit means the current path cannot work.
    if (choice.candidateCount == 0) {
        return false;
    }

    int box = getBoxIndex(choice.row, choice.col);

    for (int digit = 1; digit <= 9; ++digit) {
        int mask = digitMask(digit);

        if ((choice.candidatesMask & mask) == 0) {
            continue;
        }

        // Place the digit and update our fast constraint trackers.
        board[choice.row][choice.col] = digit;
        rowMasks[choice.row] |= mask;
        colMasks[choice.col] |= mask;
        boxMasks[box] |= mask;

        // If the recursive call solves the rest of the board, stop immediately.
        if (solveSudoku(board, rowMasks, colMasks, boxMasks)) {
            return true;
        }

        // Backtrack: undo the placement and try the next candidate.
        board[choice.row][choice.col] = EMPTY;
        rowMasks[choice.row] &= ~mask;
        colMasks[choice.col] &= ~mask;
        boxMasks[box] &= ~mask;
    }

    return false;
}

bool solveSudoku(Board& board) {
    std::array<int, 9> rowMasks{};
    std::array<int, 9> colMasks{};
    std::array<int, 9> boxMasks{};

    if (!isValidInitialBoard(board, rowMasks, colMasks, boxMasks)) {
        return false;
    }

    return solveSudoku(board, rowMasks, colMasks, boxMasks);
}

int main() {
    Board board = {{
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
    printBoard(board);

    if (solveSudoku(board)) {
        std::cout << "\nSolved Sudoku:\n";
        printBoard(board);
    } else {
        std::cout << "\nThis Sudoku board is invalid or has no solution.\n";
    }

    return 0;
}
