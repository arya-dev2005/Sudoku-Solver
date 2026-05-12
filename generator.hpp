#ifndef SUDOKU_GENERATOR_HPP
#define SUDOKU_GENERATOR_HPP

#include "board.hpp"

#include <random>

struct GeneratedPuzzle {
    Board puzzle{};
    Board solution{};
    Difficulty requestedDifficulty = Difficulty::Medium;
    DifficultyAnalysis analysis{};
    int removedCells = 0;
    double elapsedMs = 0.0;
};

Board generateCompleteBoard(std::mt19937& rng);
GeneratedPuzzle generateBestMatchingPuzzle(Difficulty difficulty, std::mt19937& rng, int maxAttempts);

#endif
