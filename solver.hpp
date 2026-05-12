#ifndef SUDOKU_SOLVER_HPP
#define SUDOKU_SOLVER_HPP

#include "board.hpp"

#include <string>

CellChoice findBestEmptyCell(const Board& board,
                             const std::array<Mask, 9>& rowMasks,
                             const std::array<Mask, 9>& colMasks,
                             const std::array<Mask, 9>& boxMasks,
                             SolverStats& stats);
SolveReport solveSudoku(Board board, SolverOptions options = {});
bool hasUniqueSolution(const Board& puzzle);
DifficultyAnalysis analyzeDifficulty(const Board& puzzle);
std::string difficultyAnalysisToText(const DifficultyAnalysis& analysis);

#endif
