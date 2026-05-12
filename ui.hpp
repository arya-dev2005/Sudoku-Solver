#ifndef SUDOKU_UI_HPP
#define SUDOKU_UI_HPP

#include "board.hpp"

#include <string>

namespace Ansi {
    extern const char* RESET;
    extern const char* BOLD;
    extern const char* DIM;
    extern const char* CYAN;
    extern const char* GREEN;
    extern const char* YELLOW;
    extern const char* RED;
    extern const char* BLUE;
    extern const char* CLEAR;
    extern const char* HIDE_CURSOR;
    extern const char* SHOW_CURSOR;
}

std::string colorize(const std::string& text, const char* color, const SolverOptions& options);
void printBoard(const Board& board);
void renderBoard(const Board& board,
                 const FixedCells& fixedCells,
                 const RenderState& state,
                 const SolverOptions& options);
void renderSolverFrame(const Board& board,
                       const FixedCells& fixedCells,
                       const SolverStats& stats,
                       const RenderState& state,
                       const SolverOptions& options,
                       int depth);
void printStats(const SolverStats& stats);
void printDifficultyAnalysis(const DifficultyAnalysis& analysis);
void clearScreen();

#endif
