#include "ui.hpp"

#include "solver.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace Ansi {
    const char* RESET = "\033[0m";
    const char* BOLD = "\033[1m";
    const char* DIM = "\033[2m";
    const char* CYAN = "\033[36m";
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* RED = "\033[31m";
    const char* BLUE = "\033[34m";
    const char* CLEAR = "\033[2J\033[H";
    const char* HIDE_CURSOR = "\033[?25l";
    const char* SHOW_CURSOR = "\033[?25h";
}

std::string colorize(const std::string& text, const char* color, const SolverOptions& options) {
    if (!options.useColor) {
        return text;
    }
    return std::string(color) + text + Ansi::RESET;
}

void printBoard(const Board& board) {
    std::cout << boardToPrettyText(board);
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

void printStats(const SolverStats& stats) {
    std::cout << statsToText(stats);
}

void printDifficultyAnalysis(const DifficultyAnalysis& analysis) {
    std::cout << difficultyAnalysisToText(analysis);
}

void clearScreen() {
    std::cout << Ansi::CLEAR;
}
