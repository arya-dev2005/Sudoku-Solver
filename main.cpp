#include "board.hpp"
#include "generator.hpp"
#include "solver.hpp"
#include "ui.hpp"
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

void runBenchmark(const Board& puzzle);

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

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
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
        text = readLine("\nEnter puzzle: ");
    } else {
        std::cout << "\nEnter 9 rows:\n";
        for (int row = 0; row < SIZE; ++row) {
            text += readLine("Row " + std::to_string(row + 1) + ": ");
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

void enterCustomPuzzle(Board& puzzle) {
    try {
        Board candidate = readPuzzleFromUser();

        clearScreen();
        printAppHeader();
        std::cout << "Parsed puzzle:\n";
        printBoard(candidate);
        std::cout << '\n';

        if (validateCandidateForLoading(candidate)) {
            puzzle = candidate;
        }
    } catch (const std::exception& ex) {
        clearScreen();
        printAppHeader();
        std::cout << "Puzzle input failed: " << ex.what() << '\n';
    }
    waitForEnter();
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
        out << "Original puzzle:\n" << boardToPrettyText(puzzle) << '\n';
        out << "Solved puzzle:\n" << boardToPrettyText(report.solvedBoard) << '\n';
        out << "Compact solved puzzle:\n" << boardToCompactText(report.solvedBoard) << '\n';
        out << "Difficulty analysis:\n" << difficultyAnalysisToText(analyzeDifficulty(puzzle)) << '\n';
        out << "Statistics:\n" << statsToText(report.stats);

        writeTextFile(path, out.str());
        std::cout << "\nSolved report saved to: " << path << '\n';
    } catch (const std::exception& ex) {
        std::cout << "\nSave failed: " << ex.what() << '\n';
    }
    waitForEnter();
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

void resetToDefaultPuzzle(Board& puzzle) {
    puzzle = defaultPuzzle();
    clearScreen();
    printAppHeader();
    std::cout << "Puzzle reset to the built-in sample.\n\n";
    printBoard(puzzle);
    waitForEnter();
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
    std::cout << "Phase 5B split:\n";
    std::cout << "- board.* owns shared data types and serialization.\n";
    std::cout << "- solver.* owns recursive search and difficulty analysis.\n";
    std::cout << "- generator.* owns puzzle generation.\n";
    std::cout << "- ui.* owns rendering.\n";
    std::cout << "- benchmark.* owns benchmark comparisons.\n";
    waitForEnter();
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
}
