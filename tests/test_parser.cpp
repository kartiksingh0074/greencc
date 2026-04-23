// ── GreenCC Parser Tests ────────────────────────────────────────────
#include "lexer.h"
#include "parser.h"
#include <cassert>
#include <iostream>

using namespace greencc;

static int passed = 0, failed = 0;

#define TEST(name) { \
    std::cout << "  [parser] " << #name << "... "; \
    try { test_##name(); passed++; std::cout << "✓\n"; } \
    catch (const std::exception& e) { failed++; std::cout << "FAIL: " << e.what() << "\n"; } \
    catch (...) { failed++; std::cout << "FAIL\n"; } \
}

static std::unique_ptr<Program> parseSource(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    Parser parser(tokens);
    auto program = parser.parse();
    if (!parser.errors().empty()) {
        for (auto& e : parser.errors()) std::cerr << "    " << e << "\n";
        throw std::runtime_error("parse errors");
    }
    return program;
}

static void test_simple_function() {
    auto p = parseSource("int main() { return 0; }");
    assert(p->declarations.size() == 1);
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    assert(fn->name == "main");
    assert(fn->returnType == "int");
    assert(fn->params.empty());
}

static void test_variable_declaration() {
    auto p = parseSource("int main() { int x = 42; return x; }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    assert(fn->body->stmts.size() == 2);
    auto* vd = dynamic_cast<VarDecl*>(fn->body->stmts[0].get());
    assert(vd);
    assert(vd->name == "x");
    assert(vd->typeName == "int");
}

static void test_if_else() {
    auto p = parseSource("int main() { if (1) { return 1; } else { return 0; } }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    auto* ifst = dynamic_cast<IfStmt*>(fn->body->stmts[0].get());
    assert(ifst);
    assert(ifst->elseBranch != nullptr);
}

static void test_for_loop() {
    auto p = parseSource("int main() { for (int i = 0; i < 10; i++) { } return 0; }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    auto* forst = dynamic_cast<ForStmt*>(fn->body->stmts[0].get());
    assert(forst);
    assert(forst->init != nullptr);
    assert(forst->condition != nullptr);
    assert(forst->update != nullptr);
}

static void test_while_loop() {
    auto p = parseSource("int main() { int x = 10; while (x > 0) { x = x - 1; } return 0; }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    auto* ws = dynamic_cast<WhileStmt*>(fn->body->stmts[1].get());
    assert(ws);
}

static void test_function_with_params() {
    auto p = parseSource("int add(int a, int b) { return a + b; }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    assert(fn);
    assert(fn->name == "add");
    assert(fn->params.size() == 2);
    assert(fn->params[0].name == "a");
    assert(fn->params[1].name == "b");
}

static void test_binary_expression() {
    auto p = parseSource("int main() { int x = 2 + 3 * 4; return x; }");
    auto* fn = dynamic_cast<FunctionDecl*>(p->declarations[0].get());
    auto* vd = dynamic_cast<VarDecl*>(fn->body->stmts[0].get());
    assert(vd);
    // Should be (2 + (3 * 4)) due to precedence
    auto* add = dynamic_cast<BinaryExpr*>(vd->init.get());
    assert(add);
    assert(add->op == TokenType::PLUS);
}

static void test_includes_and_using() {
    auto p = parseSource("#include <iostream>\nusing namespace std;\nint main() { return 0; }");
    assert(p->includes.size() == 1);
    assert(p->includes[0] == "iostream");
    assert(p->hasUsing);
}

int run_parser_tests() {
    std::cout << "\n── Parser Tests ───────────────────────────────────────\n";
    TEST(simple_function);
    TEST(variable_declaration);
    TEST(if_else);
    TEST(for_loop);
    TEST(while_loop);
    TEST(function_with_params);
    TEST(binary_expression);
    TEST(includes_and_using);
    return failed;
}
