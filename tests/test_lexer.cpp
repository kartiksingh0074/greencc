// ── GreenCC Lexer Tests ─────────────────────────────────────────────
#include "lexer.h"
#include <cassert>
#include <iostream>

using namespace greencc;

static int passed = 0, failed = 0;

#define TEST(name) { \
    std::cout << "  [lexer] " << #name << "... "; \
    try { test_##name(); passed++; std::cout << "✓\n"; } \
    catch (const std::exception& e) { failed++; std::cout << "FAIL: " << e.what() << "\n"; } \
    catch (...) { failed++; std::cout << "FAIL\n"; } \
}

#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error( \
    std::string("Expected ") + std::to_string((int)(b)) + " got " + std::to_string((int)(a)))

static void test_basic_tokens() {
    Lexer lex("int x = 42;");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::KW_INT);
    ASSERT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    ASSERT_EQ(tokens[2].type, TokenType::EQ);
    ASSERT_EQ(tokens[3].type, TokenType::INTEGER_LIT);
    ASSERT_EQ(tokens[4].type, TokenType::SEMICOLON);
    ASSERT_EQ(tokens[5].type, TokenType::END_OF_FILE);
}

static void test_operators() {
    Lexer lex("+ - * / % == != <= >= && || << >> ++ --");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::PLUS);
    ASSERT_EQ(tokens[1].type, TokenType::MINUS);
    ASSERT_EQ(tokens[2].type, TokenType::STAR);
    ASSERT_EQ(tokens[3].type, TokenType::SLASH);
    ASSERT_EQ(tokens[4].type, TokenType::PERCENT);
    ASSERT_EQ(tokens[5].type, TokenType::EQ_EQ);
    ASSERT_EQ(tokens[6].type, TokenType::BANG_EQ);
    ASSERT_EQ(tokens[7].type, TokenType::LT_EQ);
    ASSERT_EQ(tokens[8].type, TokenType::GT_EQ);
    ASSERT_EQ(tokens[9].type, TokenType::AND_AND);
    ASSERT_EQ(tokens[10].type, TokenType::OR_OR);
    ASSERT_EQ(tokens[11].type, TokenType::LSHIFT);
    ASSERT_EQ(tokens[12].type, TokenType::RSHIFT);
    ASSERT_EQ(tokens[13].type, TokenType::PLUS_PLUS);
    ASSERT_EQ(tokens[14].type, TokenType::MINUS_MINUS);
}

static void test_string_literal() {
    Lexer lex("\"hello world\"");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::STRING_LIT);
    assert(tokens[0].lexeme == "\"hello world\"");
}

static void test_float_literal() {
    Lexer lex("3.14 0.5f");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::FLOAT_LIT);
    ASSERT_EQ(tokens[1].type, TokenType::FLOAT_LIT);
}

static void test_comments() {
    Lexer lex("int x; // comment\nint y; /* block */ int z;");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    // Should have: int x ; int y ; int z ; EOF = 9 tokens
    int count = 0;
    for (auto& t : tokens) if (t.type != TokenType::END_OF_FILE) count++;
    ASSERT_EQ(count, 9);
}

static void test_keywords() {
    Lexer lex("if else while for return break continue const void bool");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::KW_IF);
    ASSERT_EQ(tokens[1].type, TokenType::KW_ELSE);
    ASSERT_EQ(tokens[2].type, TokenType::KW_WHILE);
    ASSERT_EQ(tokens[3].type, TokenType::KW_FOR);
    ASSERT_EQ(tokens[4].type, TokenType::KW_RETURN);
    ASSERT_EQ(tokens[5].type, TokenType::KW_BREAK);
    ASSERT_EQ(tokens[6].type, TokenType::KW_CONTINUE);
    ASSERT_EQ(tokens[7].type, TokenType::KW_CONST);
    ASSERT_EQ(tokens[8].type, TokenType::KW_VOID);
    ASSERT_EQ(tokens[9].type, TokenType::KW_BOOL);
}

static void test_scope_operator() {
    Lexer lex("std::cout");
    auto tokens = lex.tokenize();
    assert(lex.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::KW_STD);
    ASSERT_EQ(tokens[1].type, TokenType::SCOPE);
    ASSERT_EQ(tokens[2].type, TokenType::KW_COUT);
}

int run_lexer_tests() {
    std::cout << "\n── Lexer Tests ────────────────────────────────────────\n";
    TEST(basic_tokens);
    TEST(operators);
    TEST(string_literal);
    TEST(float_literal);
    TEST(comments);
    TEST(keywords);
    TEST(scope_operator);
    return failed;
}
