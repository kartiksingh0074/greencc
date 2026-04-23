#pragma once
#include <string>
#include <vector>
#include "token.h"

namespace greencc {

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::string             source_;
    std::vector<Token>      tokens_;
    std::vector<std::string> errors_;
    size_t                  pos_  = 0;
    int                     line_ = 1;
    int                     col_  = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    void readString();
    void readChar();
    void readNumber();
    void readIdentifier();
    void addToken(TokenType type, const std::string& lexeme);
    void error(const std::string& msg);
};

} 
