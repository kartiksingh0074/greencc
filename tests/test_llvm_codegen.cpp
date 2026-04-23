 #include <iostream>
#include <string>
#include <cassert>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "optimizer.h"
#include "llvm_codegen.h"

using namespace greencc;

static int passed = 0, failed = 0;

static void check(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "  [llvm_codegen] " << name << "... ✓\n";
        passed++;
    } else {
        std::cout << "  [llvm_codegen] " << name << "... ✗\n";
        failed++;
    }
}

static std::unique_ptr<Program> parseSource(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

static std::string generateIR(const std::string& source) {
    auto program = parseSource(source);
    SemanticAnalyzer analyzer;
    analyzer.analyze(*program);
    LLVMCodeGen gen;
    return gen.generate(*program);
}

static std::string optimizeAndGenerateIR(const std::string& source) {
    auto program = parseSource(source);
    SemanticAnalyzer analyzer;
    analyzer.analyze(*program);
    Optimizer optimizer;
    optimizer.optimize(*program);
    LLVMCodeGen gen;
    return gen.generate(*program);
}

// ── Test: Simple function generates valid IR ─────────────────────
static void test_simple_function() {
    std::string ir = generateIR(R"(
        int add(int a, int b) {
            return a + b;
        }
    )");
    check("simple_function",
        ir.find("define i32 @add(i32") != std::string::npos &&
        ir.find("add i32") != std::string::npos &&
        ir.find("ret i32") != std::string::npos);
}

// ── Test: Main function ──────────────────────────────────────────
static void test_main_function() {
    std::string ir = generateIR(R"(
        int main() {
            return 0;
        }
    )");
    check("main_function",
        ir.find("define i32 @main()") != std::string::npos &&
        ir.find("ret i32 0") != std::string::npos);
}

// ── Test: Variable alloca and store ──────────────────────────────
static void test_variable_alloca() {
    std::string ir = generateIR(R"(
        int main() {
            int x = 42;
            return x;
        }
    )");
    check("variable_alloca",
        ir.find("alloca i32") != std::string::npos &&
        ir.find("store i32 42") != std::string::npos &&
        ir.find("load i32") != std::string::npos);
}

// ── Test: Strength reduction produces shl ────────────────────────
static void test_strength_reduction_shl() {
    std::string ir = optimizeAndGenerateIR(R"(
        int main() {
            int x = 5;
            int y = x * 8;
            return y;
        }
    )");
    check("strength_reduction_shl",
        ir.find("shl i32") != std::string::npos);
}

// ── Test: Strength reduction produces ashr ───────────────────────
static void test_strength_reduction_ashr() {
    std::string ir = optimizeAndGenerateIR(R"(
        int main() {
            int x = 100;
            int y = x / 4;
            return y;
        }
    )");
    check("strength_reduction_ashr",
        ir.find("ashr i32") != std::string::npos);
}

// ── Test: If/else generates proper control flow ──────────────────
static void test_if_else_branching() {
    std::string ir = generateIR(R"(
        int main() {
            int x = 10;
            if (x > 5) {
                return 1;
            } else {
                return 0;
            }
        }
    )");
    check("if_else_branching",
        ir.find("br i1") != std::string::npos &&
        ir.find("icmp sgt") != std::string::npos &&
        ir.find("then") != std::string::npos &&
        ir.find("else") != std::string::npos);
}

// ── Test: For loop generates proper blocks ───────────────────────
static void test_for_loop() {
    std::string ir = generateIR(R"(
        int main() {
            int sum = 0;
            for (int i = 0; i < 10; i++) {
                sum = sum + i;
            }
            return sum;
        }
    )");
    check("for_loop",
        ir.find("for.cond") != std::string::npos &&
        ir.find("for.body") != std::string::npos &&
        ir.find("for.inc") != std::string::npos &&
        ir.find("for.exit") != std::string::npos);
}

// ── Test: Constant folding in IR ─────────────────────────────────
static void test_constant_folding() {
    std::string ir = optimizeAndGenerateIR(R"(
        int main() {
            int x = 2 * 3 * 4;
            return x;
        }
    )");
    // After constant folding, should store the literal 24
    check("constant_folding",
        ir.find("store i32 24") != std::string::npos);
}

// ── Test: Printf declaration ─────────────────────────────────────
static void test_printf_declaration() {
    std::string ir = generateIR(R"(
        int main() {
            cout << "hello" << endl;
            return 0;
        }
    )");
    check("printf_declaration",
        ir.find("declare i32 @printf(ptr, ...)") != std::string::npos);
}

// ── Entry point ──────────────────────────────────────────────────
int main() {
    std::cout << "\n── LLVM CodeGen Tests ─────────────────────────────────\n";

    test_simple_function();
    test_main_function();
    test_variable_alloca();
    test_strength_reduction_shl();
    test_strength_reduction_ashr();
    test_if_else_branching();
    test_for_loop();
    test_constant_folding();
    test_printf_declaration();

    std::cout << "\n  " << passed << " passed, " << failed << " failed\n";
    if (failed == 0) {
        std::cout << "  ✅ All LLVM codegen tests passed!\n\n";
    }
    return failed > 0 ? 1 : 0;
}
