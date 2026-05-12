#ifndef SUDOKU_BOARD_HPP
#define SUDOKU_BOARD_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

using Board = std::array<std::array<int, 9>, 9>;
using FixedCells = std::array<std::array<bool, 9>, 9>;
using Mask = std::uint16_t;

constexpr int SIZE = 9;
constexpr int EMPTY = 0;
constexpr Mask ALL_DIGITS_MASK = 0b1111111110;

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

int getBoxIndex(int row, int col);
Mask digitMask(int digit);
int maskToDigit(Mask mask);
int countBits(Mask mask);

FixedCells makeFixedCells(const Board& board);
int countFilledCells(const Board& board);
int countClues(const Board& board);
Board defaultPuzzle();

bool parsePuzzleText(const std::string& text, Board& board, std::string& errorMessage);
bool isValidInitialBoard(const Board& board,
                         std::array<Mask, 9>& rowMasks,
                         std::array<Mask, 9>& colMasks,
                         std::array<Mask, 9>& boxMasks);
bool isStructurallyValidPuzzle(const Board& puzzle);

Mask getCandidatesMask(int row,
                       int col,
                       const std::array<Mask, 9>& rowMasks,
                       const std::array<Mask, 9>& colMasks,
                       const std::array<Mask, 9>& boxMasks,
                       SolverStats* stats = nullptr);
void applyDigitToState(Board& board,
                       std::array<Mask, 9>& rowMasks,
                       std::array<Mask, 9>& colMasks,
                       std::array<Mask, 9>& boxMasks,
                       int row,
                       int col,
                       int digit);
void removeDigitFromState(Board& board,
                          std::array<Mask, 9>& rowMasks,
                          std::array<Mask, 9>& colMasks,
                          std::array<Mask, 9>& boxMasks,
                          int row,
                          int col,
                          int digit);

std::string statusName(SolveStatus status);
std::string difficultyName(Difficulty difficulty);
int targetCluesForDifficulty(Difficulty difficulty);
int difficultyRank(Difficulty difficulty);
Difficulty difficultyFromScore(int score);

std::string boardToCompactText(const Board& board);
std::string boardToPrettyText(const Board& board);
std::string statsToText(const SolverStats& stats);
std::string readTextFile(const std::string& path);
void writeTextFile(const std::string& path, const std::string& text);

#endif
