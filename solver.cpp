#include "solver.hpp"

#include "ui.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <utility>
#include <vector>

namespace {

void placeDigit(Board& board,
                std::array<Mask, 9>& rowMasks,
                std::array<Mask, 9>& colMasks,
                std::array<Mask, 9>& boxMasks,
                std::vector<Placement>& placements,
                int row,
                int col,
                int digit) {
    applyDigitToState(board, rowMasks, colMasks, boxMasks, row, col, digit);
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
        removeDigitFromState(board, rowMasks, colMasks, boxMasks,
                             placement.row, placement.col, placement.digit);
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
        if (possibleCount == 1 &&
            !placeHiddenSingleInCell(board, rowMasks, colMasks, boxMasks, placements,
                                     stats, changed, fixedCells, options, depth,
                                     onlyRow, onlyCol, digit)) {
            return false;
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
    auto tryDigit = [&](int digit) {
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
    };

    if (options.useOptimizedIteration) {
        for (Mask candidates = choice.candidatesMask; candidates != 0;) {
            Mask lowestBit = static_cast<Mask>(candidates & -candidates);
            tryDigit(maskToDigit(lowestBit));
            if (solutions >= options.solutionLimit) {
                break;
            }
            candidates = static_cast<Mask>(candidates & (candidates - 1));
        }
    } else {
        for (int digit = 1; digit <= 9; ++digit) {
            if ((choice.candidatesMask & digitMask(digit)) == 0) {
                continue;
            }
            tryDigit(digit);
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

} // namespace

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

SolveReport solveSudoku(Board board, SolverOptions options) {
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

bool hasUniqueSolution(const Board& puzzle) {
    SolverOptions options;
    options.solutionLimit = 2;
    SolveReport report = solveSudoku(puzzle, options);
    return report.status == SolveStatus::UniqueSolution;
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
    int propagationBacktrackPenalty =
        static_cast<int>(std::min<long long>(40, propagationReport.stats.backtracks * 10));
    int rawSearchPenalty = static_cast<int>(std::min<long long>(25, searchReport.stats.backtracks / 12));
    analysis.score = analysis.blanks + propagationGuessPenalty + propagationBacktrackPenalty + rawSearchPenalty;
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
