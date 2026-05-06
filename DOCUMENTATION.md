# 🌿 GreenCC — Energy-Aware C++ Compiler

## Project Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Directory Structure](#directory-structure)
4. [Compilation Pipeline](#compilation-pipeline)
5. [Module Reference](#module-reference)
6. [Energy Model](#energy-model)
7. [Optimizer](#optimizer)
8. [Code Generation](#code-generation)
9. [Web Interface](#web-interface)
10. [Build Instructions](#build-instructions)
11. [Usage](#usage)
12. [Testing](#testing)
13. [Examples](#examples)

---

## Overview

**GreenCC** is an energy-aware C++ compiler built with an LLVM 18 backend. It parses a subset of C++, performs semantic analysis, estimates energy consumption of the input program, applies energy-focused optimizations, and generates either optimized C++ source, LLVM IR, native object files, or fully linked executables.

The compiler features a **6-stage pipeline**: Lexing → Parsing → Semantic Analysis → Energy Estimation → Optimization → Code Generation. It also includes a **web-based IDE** for interactive use.

### Key Features

- **Energy estimation** via a cost-model that assigns energy units to operations (arithmetic, memory, I/O, branches, function calls)
- **9 optimization passes** including constant folding, strength reduction, dead code elimination, loop-invariant code motion, and energy-aware register hints
- **Dual code generation**: C++ source-to-source and LLVM IR with native object file emission
- **Web IDE** with a live code editor, real-time compilation, and visual energy reports

### Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 |
| Build System | CMake 3.14+ |
| Backend | LLVM 18 |
| Web Server | Node.js + Express |
| Web Frontend | Vanilla HTML/CSS/JS |

---

## Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                     GreenCC Compiler Pipeline                  │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Source Code (.cpp)                                             │
│       │                                                        │
│       ▼                                                        │
│  ┌──────────┐    Tokens    ┌──────────┐     AST                │
│  │  Lexer   │ ──────────▶  │  Parser  │ ──────────▶            │
│  └──────────┘              └──────────┘                        │
│                                              │                 │
│                                              ▼                 │
│                                     ┌────────────────┐         │
│                                     │   Semantic      │         │
│                                     │   Analyzer      │         │
│                                     └────────────────┘         │
│                                              │                 │
│                                              ▼                 │
│                                     ┌────────────────┐         │
│                                     │   Energy        │         │
│                                     │   Estimator     │         │
│                                     └────────────────┘         │
│                                              │                 │
│                                              ▼                 │
│                                     ┌────────────────┐         │
│                                     │   Optimizer     │         │
│                                     │   (9 passes)    │         │
│                                     └────────────────┘         │
│                                              │                 │
│                              ┌───────────────┼───────────┐     │
│                              ▼               ▼           ▼     │
│                        ┌──────────┐  ┌────────────┐ ┌───────┐  │
│                        │ C++ Code │  │  LLVM IR   │ │ .o /  │  │
│                        │   Gen    │  │   Gen      │ │ EXE   │  │
│                        └──────────┘  └────────────┘ └───────┘  │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Design Patterns

- **Visitor Pattern**: All AST traversals (semantic analysis, energy estimation, optimization, code generation) use the `ASTVisitor` interface for clean separation of concerns.
- **AST Ownership**: All AST nodes use `std::unique_ptr` for automatic memory management.

---

## Directory Structure

```
greencc/
├── CMakeLists.txt          # Build configuration (CMake)
├── .gitignore
│
├── include/                # Header files
│   ├── token.h             # Token types, Token struct, keyword map
│   ├── ast.h               # AST node definitions (Expr, Stmt, Decl)
│   ├── visitor.h           # ASTVisitor interface (pure virtual)
│   ├── lexer.h             # Lexer class interface
│   ├── parser.h            # Parser class interface
│   ├── semantic.h          # SemanticAnalyzer class interface
│   ├── energy_model.h      # EnergyEstimator + EnergyReport
│   ├── optimizer.h         # Optimizer class + OptimizationResult
│   ├── codegen.h           # C++ CodeGenerator interface
│   └── llvm_codegen.h      # LLVMCodeGen interface
│
├── src/                    # Implementation files
│   ├── main.cpp            # CLI entry point (6-stage pipeline)
│   ├── lexer.cpp           # Lexical analysis implementation
│   ├── parser.cpp          # Recursive-descent parser
│   ├── semantic.cpp        # Semantic analysis + AST accept() methods
│   ├── energy_model.cpp    # Energy cost model implementation
│   ├── optimizer.cpp       # 9 optimization passes (~1039 lines)
│   ├── codegen.cpp         # C++ source code generator
│   └── llvm_codegen.cpp    # LLVM IR code generator (~773 lines)
│
├── tests/                  # Unit tests
│   ├── test_lexer.cpp      # Lexer tests (7 test cases)
│   ├── test_parser.cpp     # Parser tests
│   ├── test_optimizer.cpp  # Optimizer tests
│   └── test_llvm_codegen.cpp # LLVM codegen tests
│
├── examples/               # Sample C++ input files
│   ├── sample1.cpp         # Optimization-friendly patterns
│   └── sample2.cpp         # Multiple optimization opportunities
│
└── web/                    # Web-based IDE
    ├── package.json        # Node.js dependencies (express, cors)
    ├── server.js           # Express API server
    └── public/
        ├── index.html      # Frontend HTML
        ├── styles.css      # Frontend styles
        └── app.js          # Frontend JavaScript
```

---

## Compilation Pipeline

The compiler executes 6 sequential stages:

### Stage 1: Lexing (`lexer.cpp`)

Converts raw source text into a stream of tokens.

**Supported token categories:**
- **Literals**: `INTEGER_LIT`, `FLOAT_LIT`, `STRING_LIT`, `CHAR_LIT`
- **Keywords**: `int`, `float`, `double`, `char`, `bool`, `void`, `string`, `const`, `if`, `else`, `while`, `for`, `return`, `break`, `continue`, `true`, `false`, `cout`, `cin`, `endl`, `include`, `using`, `namespace`, `std`
- **Operators**: All arithmetic (`+ - * / %`), comparison (`== != < > <= >=`), logical (`&& ||`), bitwise (`& | ^ ~ << >>`), assignment (`= += -= *= /=`), increment/decrement (`++ --`)
- **Delimiters**: `( ) { } [ ] ; , . : :: #`

**Features:**
- Line/column tracking for error reporting
- Line comments (`//`) and block comments (`/* */`)
- String and char literal parsing with escape sequences
- Float suffix support (`f` / `F`)

### Stage 2: Parsing (`parser.cpp`)

Recursive-descent parser that builds an Abstract Syntax Tree (AST).

**Supported language constructs:**
- Preprocessor directives (`#include <...>`)
- `using namespace std;`
- Function declarations with parameters
- Variable declarations (with optional `const`, array sizes, initializers)
- Control flow: `if/else`, `while`, `for`, `return`, `break`, `continue`
- Expressions: full operator precedence (14 levels), function calls, array access
- I/O: `cout << ... << endl` and `cin >> ...`

**Operator Precedence (lowest to highest):**

| Level | Operators | Method |
|-------|-----------|--------|
| 1 | `=  +=  -=  *=  /=` | `parseAssignment()` |
| 2 | `\|\|` | `parseLogicalOr()` |
| 3 | `&&` | `parseLogicalAnd()` |
| 4 | `\|` | `parseBitwiseOr()` |
| 5 | `^` | `parseBitwiseXor()` |
| 6 | `&` | `parseBitwiseAnd()` |
| 7 | `==  !=` | `parseEquality()` |
| 8 | `<  >  <=  >=` | `parseRelational()` |
| 9 | `<<  >>` | `parseShift()` |
| 10 | `+  -` | `parseAdditive()` |
| 11 | `*  /  %` | `parseMultiplicative()` |
| 12 | `- ! ~ ++ --` (prefix) | `parseUnary()` |
| 13 | `[] ++ --` (postfix) | `parsePostfix()` |
| 14 | Literals, identifiers, calls | `parsePrimary()` |

### Stage 3: Semantic Analysis (`semantic.cpp`)

Validates the AST for correctness using scoped symbol tables.

**Checks performed:**
- Variable declaration before use
- Redeclaration within same scope
- Assignment to `const` variables
- Function existence and arity matching
- `break`/`continue` only inside loops
- Scoped variable resolution (inner scopes shadow outer)

### Stage 4: Energy Estimation (`energy_model.cpp`)

Traverses the AST and assigns energy cost units to each operation.

**Cost Table:**

| Operation | Energy Cost (units) |
|-----------|-------------------|
| Integer add/subtract | 1.0 |
| Integer multiply | 3.0 |
| Integer divide/modulo | 8.0 |
| Float add/subtract | 4.0 |
| Float multiply | 5.0 |
| Float divide | 12.0 |
| Memory access (load/store) | 10.0 |
| Branch (if/while/for) | 2.0 |
| Function call | 15.0 |
| I/O operation | 50.0 |
| Assignment | 2.0 |
| Comparison | 1.5 |
| Logical operation | 1.0 |
| Bitwise operation | 1.0 |
| Shift operation | 1.0 |
| Unary operation | 1.0 |
| Return | 3.0 |

**Loop multiplier:** Loops assume **100 iterations** by default, multiplying all inner costs by the nesting factor.

**Output:** An `EnergyReport` with total energy, per-function breakdown, and per-line costs.

### Stage 5: Optimization (`optimizer.cpp`)

Applies 9 optimization passes sequentially:

| # | Pass | Description |
|---|------|-------------|
| 1 | **Constant Folding** | Evaluates constant expressions at compile time (e.g., `2 * 3 * 4` → `24`) |
| 2 | **Constant Propagation** | Replaces `const` variable references with their literal values |
| 3 | **Dead Code Elimination** | Removes unused variables and unreachable code after `return` |
| 4 | **Strength Reduction** | Replaces expensive operations with cheaper ones (e.g., `x * 8` → `x << 3`, `x / 4` → `x >> 2`, `x * 1` → `x`, `x + 0` → `x`) |
| 5 | **Loop-Invariant Code Motion** | Detects computations inside loops that don't depend on loop variables |
| 6 | **Common Subexpression Elimination** | Identifies duplicate expressions that could be computed once |
| 7 | **Peephole Optimization** | Pattern-based local optimizations (e.g., `x - x` → `0`, `x / x` → `1`, `!!x` → `x`, dead branch elimination) |
| 8 | **Function Inlining** | Inlines small, non-recursive functions (≤5 statements) at call sites |
| 9 | **Energy-Aware Register Hints** | Identifies "hot" variables (accessed ≥50 times, weighted by loop nesting) and annotates them with register priority metadata |

### Stage 6: Code Generation

Two backends are available:

#### C++ Source Generator (`codegen.cpp`)
Regenerates clean C++ source code from the optimized AST with proper formatting and indentation. Activated with `--emit-cpp`.

#### LLVM IR Generator (`llvm_codegen.cpp`)
Generates LLVM IR from the optimized AST. Supports:
- All primitive types (`int`, `float`, `double`, `char`, `bool`, `void`, `string`)
- Stack-allocated variables via `alloca` in function entry blocks
- Full arithmetic, comparison, logical, and bitwise operations
- Control flow (if/else, while, for) with proper basic block structure
- Function calls with type coercion
- Array access via GEP instructions
- I/O via `printf` calls
- Hot variable register priority metadata (`greencc.regpriority`)
- Object file emission using the host's target machine
- Native executable linking via `clang-18`

---

## Module Reference

### AST Nodes (`ast.h`)

**Expressions (`Expr`):**
- `LiteralExpr` — Integer, float, string, char, bool literals
- `VarExpr` — Variable reference
- `BinaryExpr` — Binary operations (e.g., `a + b`)
- `UnaryExpr` — Unary operations, prefix/postfix (e.g., `++i`, `i--`)
- `AssignExpr` — Assignment with optional compound operator (e.g., `x += 1`)
- `CallExpr` — Function call (e.g., `foo(a, b)`)
- `ArrayAccessExpr` — Array indexing (e.g., `arr[i]`)
- `IOExpr` — `cout`/`cin` I/O expressions

**Statements (`Stmt`):**
- `ExprStmt` — Expression as statement
- `Block` — `{ ... }` block of statements
- `VarDecl` — Variable declaration (type, name, initializer, const, array size)
- `IfStmt` — If/else conditional
- `WhileStmt` — While loop
- `ForStmt` — For loop (init, condition, update, body)
- `ReturnStmt` — Return statement
- `BreakStmt` / `ContinueStmt` — Loop control

**Top-Level:**
- `FunctionDecl` — Function declaration (return type, name, params, body)
- `Program` — Root node (includes, using-declaration, declarations)

---

## Web Interface

### Server (`web/server.js`)

An Express.js server that wraps the GreenCC CLI binary.

**API Endpoints:**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/optimize` | Accepts `{ code: string }`, compiles with `--emit-cpp`, returns structured results |
| `GET` | `/api/sample` | Returns sample C++ code from `examples/sample1.cpp` |
| `GET` | `*` | Serves the static frontend |

**Response format (`/api/optimize`):**
```json
{
  "success": true,
  "optimizedCode": "...",
  "energyBefore": 12345.0,
  "energyAfter": 6789.0,
  "energySavings": 45.0,
  "compilationTime": 12.5,
  "optimizations": {
    "constantsFolded": 3,
    "constantsPropagated": 2,
    "deadCodeEliminated": 1,
    "strengthReductions": 4,
    "loopInvariantsMoved": 1,
    "csesEliminated": 0,
    "functionsInlined": 1,
    "peepholeOptimizations": 2,
    "registerHintsEmitted": 3
  },
  "stages": [
    { "step": 1, "name": "Lexing", "status": "success", "detail": "(42 tokens)" }
  ]
}
```

### Frontend (`web/public/`)

- **Code Editor**: Textarea with syntax-highlighted C++ input
- **Optimize Button**: Sends code to `/api/optimize`
- **Results Panel**: Displays optimized code, energy comparison, optimization summary, and compilation stages
- **Sample Loader**: "Load Sample" button fetches example code

---

## Build Instructions

### Prerequisites

- **CMake** ≥ 3.14
- **C++17 compiler** (GCC 7+ or Clang 5+)
- **LLVM 18** development libraries (`llvm-18-dev`)
- **Node.js** ≥ 16 (for web interface)

### Building the Compiler

```bash
# From project root
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

This produces:
- `build/greencc` — The CLI compiler executable
- `build/greencc_tests` — The test runner

### Setting Up the Web Interface

```bash
cd web
npm install
npm start
# Server runs at http://localhost:3000
```

> **Note:** The web server expects the compiler binary at `../build/greencc` relative to the `web/` directory.

---

## Usage

### CLI Usage

```
Usage: greencc <input.cpp> [options]

Output Modes:
  --emit-llvm       Emit LLVM IR text (.ll) [default]
  --emit-obj        Emit native object file (.o)
  --emit-cpp        Emit optimized C++ source
  --compile         Compile to native executable (via clang)

Options:
  -o <file>         Output file path
  --report          Print detailed energy report
  --help            Show this help message
```

### Examples

```bash
# Emit optimized C++ source
./build/greencc examples/sample1.cpp --emit-cpp

# Emit LLVM IR to a file
./build/greencc examples/sample1.cpp --emit-llvm -o output.ll

# Compile to native executable
./build/greencc examples/sample1.cpp --compile -o myprogram

# Emit object file with detailed energy report
./build/greencc examples/sample1.cpp --emit-obj -o output.o --report
```

### Output

The compiler prints a structured report:

```
  ╔══════════════════════════════════════════════════════╗
  ║     🌿  G r e e n C C  —  v2.0                      ║
  ║     Energy-Aware C++ Compiler (LLVM Backend)         ║
  ╚══════════════════════════════════════════════════════╝

  [1/6] Lexing...             ✓ (42 tokens)
  [2/6] Parsing...            ✓ (3 declarations)
  [3/6] Analyzing semantics... ✓
  [4/6] Estimating energy...  ✓ (245670.0 units)
  [5/6] Optimizing...         ✓
  [6/6] Generating code...    ✓ (C++ source)

  ⚡ ENERGY SUMMARY:
     Before optimization: 245670.0 energy units
     After optimization:  123456.0 energy units
     ✅ Energy saved:      49.7%

  ╔══════════════════════════════════════════════════════╗
  ║          🌿 OPTIMIZATION SUMMARY 🌿                 ║
  ╠══════════════════════════════════════════════════════╣
  ║  Constants folded:             3                    ║
  ║  Constants propagated:         2                    ║
  ║  Dead code eliminated:         1                    ║
  ║  Strength reductions:          4                    ║
  ║  Loop invariants moved:        1                    ║
  ║  CSEs eliminated:              0                    ║
  ║  Functions inlined:            1                    ║
  ║  Peephole optimizations:       2                    ║
  ║  Register hints emitted:       3                    ║
  ╚══════════════════════════════════════════════════════╝
```

---

## Testing

### Running Tests

```bash
cd build
cmake ..
make -j$(nproc)
ctest
# Or run directly:
./greencc_tests
```

### Test Coverage

| Test File | Module | Test Cases |
|-----------|--------|------------|
| `test_lexer.cpp` | Lexer | Basic tokens, operators, string/float literals, comments, keywords, scope operator |
| `test_parser.cpp` | Parser | Parsing declarations, expressions, control flow |
| `test_optimizer.cpp` | Optimizer | Constant folding, strength reduction, dead code elimination |
| `test_llvm_codegen.cpp` | LLVM CodeGen | IR generation correctness |

---

## Examples

### Sample 1: Optimization-Friendly Patterns (`examples/sample1.cpp`)

Demonstrates:
- **Constant folding**: `2 * 3 * 4` → `24`
- **Strength reduction**: `i * 8` → `i << 3`, `result / 2` → `result >> 1`
- **Dead code elimination**: `unused = 42` removed
- **Constant propagation**: `const int MAX = 10` propagated to use sites
- **Peephole optimization**: `i + zero` → `i` (add with 0)

### Sample 2: Multiple Optimization Opportunities (`examples/sample2.cpp`)

Demonstrates:
- **Function inlining**: Small `factorial()` and `compute()` functions inlined into `main()`
- **Constant folding**: `10 * 20 + 30` → `230`, `5 + 3 + 2` → `10`
- **Strength reduction**: `i * 2` → `i << 1`, `x / 2` → `x >> 1`, `x / 8` → `x >> 3`
- **Dead code elimination**: `unused_var = 99` removed

---

## Supported C++ Subset

| Feature | Supported |
|---------|-----------|
| Primitive types (`int`, `float`, `double`, `char`, `bool`, `void`, `string`) | ✅ |
| `const` qualifier | ✅ |
| Arrays (fixed-size) | ✅ |
| Functions (declaration, calls, parameters) | ✅ |
| `if` / `else` | ✅ |
| `while` / `for` loops | ✅ |
| `break` / `continue` | ✅ |
| `return` | ✅ |
| `cout << ...` / `cin >> ...` | ✅ |
| `#include` directives | ✅ (parsed, not resolved) |
| `using namespace std` | ✅ |
| Operator precedence (14 levels) | ✅ |
| Classes / structs | ❌ |
| Templates | ❌ |
| Pointers / references | ❌ |
| Exceptions | ❌ |
| Standard library (beyond I/O) | ❌ |

---

## License

*No license file found in the repository.*

---

*Generated from GreenCC v2.0 source code analysis.*
