#include "generator.hpp"

#include "solver.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

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
        applyDigitToState(board, rowMasks, colMasks, boxMasks, choice.row, choice.col, digit);
        if (fillCompleteBoard(board, rowMasks, colMasks, boxMasks, rng)) {
            return true;
        }
        removeDigitFromState(board, rowMasks, colMasks, boxMasks, choice.row, choice.col, digit);
    }

    return false;
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

} // namespace

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

GeneratedPuzzle generateBestMatchingPuzzle(Difficulty difficulty, std::mt19937& rng, int maxAttempts) {
    GeneratedPuzzle best;
    bool hasBest = false;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        GeneratedPuzzle candidate = generatePuzzleCandidate(difficulty, rng);
        int candidateDistance = difficultyDistance(candidate.analysis.estimatedDifficulty, difficulty);
        int bestDistance = hasBest ? difficultyDistance(best.analysis.estimatedDifficulty, difficulty) : 100;

        if (!hasBest ||
            candidateDistance < bestDistance ||
            (candidateDistance == bestDistance && candidate.analysis.score > best.analysis.score)) {
            best = candidate;
            hasBest = true;
        }

        if (candidate.analysis.estimatedDifficulty == difficulty) {
            break;
        }
    }

    return best;
}
