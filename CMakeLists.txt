cmake_minimum_required(VERSION 3.14)
project(GreenCC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find LLVM 18 ─────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG
    PATHS /usr/lib/llvm-18/cmake
          /usr/lib/llvm-18/lib/cmake/llvm
)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Main library (shared by CLI + tests) ─────────────────────────────
add_library(greencc_lib STATIC
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/energy_model.cpp
    src/optimizer.cpp
    src/codegen.cpp
    src/llvm_codegen.cpp
)
target_include_directories(greencc_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

# ── CLI executable ────────────────────────────────────────────────────
add_executable(greencc src/main.cpp)

# Map LLVM components to library names
llvm_map_components_to_libnames(LLVM_LIBS
    core support irreader passes target
    X86CodeGen X86AsmParser X86Desc X86Info
)
target_link_libraries(greencc PRIVATE greencc_lib ${LLVM_LIBS})

# ── Tests ─────────────────────────────────────────────────────────────
enable_testing()
add_executable(greencc_tests
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_optimizer.cpp
)
target_link_libraries(greencc_tests PRIVATE greencc_lib)
add_test(NAME GreenCCTests COMMAND greencc_tests)
