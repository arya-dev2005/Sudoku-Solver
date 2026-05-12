cmake_minimum_required(VERSION 3.20)

project(SudokuSolverPro LANGUAGES CXX)

add_executable(sudoku_solver
    main.cpp
    board.cpp
    solver.cpp
    ui.cpp
    generator.cpp
    benchmark.cpp
)

target_compile_features(sudoku_solver PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(sudoku_solver PRIVATE /W4)
else()
    target_compile_options(sudoku_solver PRIVATE -Wall -Wextra -pedantic)
endif()
