// ── GreenCC Optimizer Tests ─────────────────────────────────────────
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "energy_model.h"
#include "optimizer.h"
#include "codegen.h"
#include <cassert>
#include <iostream>

using namespace greencc;

static int passed = 0, failed = 0;

#define TEST(name) { \
    std::cout << "  [optimizer] " << #name << "... "; \
    try { test_##name(); passed++; std::cout << "✓\n"; } \
    catch (const std::exception& e) { failed++; std::cout << "FAIL: " << e.what() << "\n"; } \
    catch (...) { failed++; std::cout << "FAIL\n"; } \
}

static std::unique_ptr<Program> compile(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

static void test_constant_folding() {
    auto prog = compile("int main() { int x = 2 + 3 * 4; return x; }");
    EnergyEstimator est;
    auto before = est.estimate(*prog);

    Optimizer opt;
    auto result = opt.optimize(*prog);

    EnergyEstimator est2;
    auto after = est2.estimate(*prog);

    assert(result.constantsFolded > 0);
    // The expression 2 + 3 * 4 should be folded to 14
    CodeGenerator gen;
    std::string code = gen.generate(*prog);
    assert(code.find("14") != std::string::npos);
}

static void test_strength_reduction() {
    auto prog = compile("int main() { int y = 5; int x = y * 8; return x; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    // y * 8 should become y << 3
    assert(result.strengthReductions > 0);

    CodeGenerator gen;
    std::string code = gen.generate(*prog);
    assert(code.find("<<") != std::string::npos);
}

static void test_dead_code_elimination() {
    auto prog = compile("int main() { int used = 1; int unused = 42; return used; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.deadCodeEliminated > 0);

    CodeGenerator gen;
    std::string code = gen.generate(*prog);
    // "unused" should be eliminated
    assert(code.find("unused") == std::string::npos);
}

static void test_energy_reduction() {
    auto prog = compile(
        "int main() {"
        "  int x = 2 * 3 * 4;"  // constant folding
        "  int y = x * 16;"      // strength reduction: * 16 → << 4
        "  int unused = 99;"     // dead code
        "  return y;"
        "}");

    EnergyEstimator est1;
    auto before = est1.estimate(*prog);

    Optimizer opt;
    opt.optimize(*prog);

    EnergyEstimator est2;
    auto after = est2.estimate(*prog);

    // Energy should be lower after optimization
    assert(after.totalEnergy <= before.totalEnergy);
}

static void test_post_return_elimination() {
    auto prog = compile("int main() { return 0; int x = 5; int y = 10; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.deadCodeEliminated >= 2);
}

static void test_multiply_by_zero() {
    auto prog = compile("int main() { int x = 5; int y = x * 0; return y; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.strengthReductions > 0);

    CodeGenerator gen;
    std::string code = gen.generate(*prog);
    // x * 0 should become just 0
    assert(code.find("0") != std::string::npos);
}

static void test_function_inlining() {
    auto prog = compile(
        "int add(int a, int b) { return a + b; }\n"
        "int main() { int result = add(3, 4); return result; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.functionsInlined > 0);
}

static void test_peephole_double_negation() {
    auto prog = compile("int main() { int x = 5; int y = !!x; return y; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.peepholeOptimizations > 0);
}

static void test_peephole_self_subtract() {
    auto prog = compile("int main() { int x = 10; int y = x - x; return y; }");

    Optimizer opt;
    auto result = opt.optimize(*prog);

    assert(result.peepholeOptimizations > 0);

    CodeGenerator gen;
    std::string code = gen.generate(*prog);
    // x - x should become 0
    assert(code.find("0") != std::string::npos);
}

// Entry points declared in other test files
extern int run_lexer_tests();
extern int run_parser_tests();

int run_optimizer_tests() {
    std::cout << "\n── Optimizer Tests ────────────────────────────────────\n";
    TEST(constant_folding);
    TEST(strength_reduction);
    TEST(dead_code_elimination);
    TEST(energy_reduction);
    TEST(post_return_elimination);
    TEST(multiply_by_zero);
    TEST(function_inlining);
    TEST(peephole_double_negation);
    TEST(peephole_self_subtract);
    return failed;
}

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║          🌿 GreenCC Test Suite 🌿                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    int totalFailed = 0;
    totalFailed += run_lexer_tests();
    totalFailed += run_parser_tests();
    totalFailed += run_optimizer_tests();

    std::cout << "\n──────────────────────────────────────────────────────\n";
    if (totalFailed == 0) {
        std::cout << "  ✅ All tests passed!\n";
    } else {
        std::cout << "  ❌ " << totalFailed << " test(s) failed!\n";
    }
    std::cout << "──────────────────────────────────────────────────────\n\n";

    return totalFailed;
}
