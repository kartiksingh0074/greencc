#include "lexer.h"
#include <cctype>
#include <sstream>

namespace greencc {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else { col_++; }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

void Lexer::addToken(TokenType type, const std::string& lexeme) {
    tokens_.emplace_back(type, lexeme, line_, col_ - (int)lexeme.size());
}

void Lexer::error(const std::string& msg) {
    std::ostringstream os;
    os << "Lexer error [" << line_ << ":" << col_ << "]: " << msg;
    errors_.push_back(os.str());
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(peek())) advance();
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n') advance();
}

void Lexer::skipBlockComment() {
    int startLine = line_;
    advance(); advance(); 
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
            advance(); advance();
            return;
        }
        advance();
    }
    error("unterminated block comment starting at line " + std::to_string(startLine));
}

void Lexer::readString() {
    int startLine = line_;
    advance(); 
    std::string val;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') { val += advance(); } 
        val += advance();
    }
    if (isAtEnd()) {
        error("unterminated string literal starting at line " + std::to_string(startLine));
        return;
    }
    advance(); 
    addToken(TokenType::STRING_LIT, "\"" + val + "\"");
}

void Lexer::readChar() {
    advance(); 
    std::string val;
    if (peek() == '\\') val += advance();
    if (!isAtEnd()) val += advance();
    if (isAtEnd() || peek() != '\'') {
        error("unterminated char literal");
        return;
    }
    advance(); 
    addToken(TokenType::CHAR_LIT, "'" + val + "'");
}

void Lexer::readNumber() {
    std::string num;
    bool isFloat = false;
    while (!isAtEnd() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') {
            if (isFloat) break; 
            isFloat = true;
        }
        num += advance();
    }
    
    if (!isAtEnd() && (peek() == 'f' || peek() == 'F')) {
        isFloat = true;
        num += advance();
    }
    addToken(isFloat ? TokenType::FLOAT_LIT : TokenType::INTEGER_LIT, num);
}

void Lexer::readIdentifier() {
    std::string id;
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        id += advance();
    }
    auto it = keywords().find(id);
    if (it != keywords().end()) {
        addToken(it->second, id);
    } else {
        addToken(TokenType::IDENTIFIER, id);
    }
}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        char c = peek();

        
        if (c == '/' && peekNext() == '/') { skipLineComment(); continue; }
        if (c == '/' && peekNext() == '*') { skipBlockComment(); continue; }

        
        if (c == '"')  { readString();     continue; }
        if (c == '\'') { readChar();       continue; }

        
        if (std::isdigit(c)) { readNumber(); continue; }

        
        if (std::isalpha(c) || c == '_') { readIdentifier(); continue; }

        
        advance(); 
        switch (c) {
            case '+':
                if (match('+'))       addToken(TokenType::PLUS_PLUS, "++");
                else if (match('='))  addToken(TokenType::PLUS_EQ, "+=");
                else                  addToken(TokenType::PLUS, "+");
                break;
            case '-':
                if (match('-'))       addToken(TokenType::MINUS_MINUS, "--");
                else if (match('='))  addToken(TokenType::MINUS_EQ, "-=");
                else                  addToken(TokenType::MINUS, "-");
                break;
            case '*':
                if (match('='))       addToken(TokenType::STAR_EQ, "*=");
                else                  addToken(TokenType::STAR, "*");
                break;
            case '/':
                if (match('='))       addToken(TokenType::SLASH_EQ, "/=");
                else                  addToken(TokenType::SLASH, "/");
                break;
            case '%':               addToken(TokenType::PERCENT, "%"); break;
            case '&':
                if (match('&'))       addToken(TokenType::AND_AND, "&&");
                else                  addToken(TokenType::AMPERSAND, "&");
                break;
            case '|':
                if (match('|'))       addToken(TokenType::OR_OR, "||");
                else                  addToken(TokenType::PIPE, "|");
                break;
            case '^':               addToken(TokenType::CARET, "^"); break;
            case '~':               addToken(TokenType::TILDE, "~"); break;
            case '!':
                if (match('='))       addToken(TokenType::BANG_EQ, "!=");
                else                  addToken(TokenType::BANG, "!");
                break;
            case '=':
                if (match('='))       addToken(TokenType::EQ_EQ, "==");
                else                  addToken(TokenType::EQ, "=");
                break;
            case '<':
                if (match('<'))       addToken(TokenType::LSHIFT, "<<");
                else if (match('='))  addToken(TokenType::LT_EQ, "<=");
                else                  addToken(TokenType::LT, "<");
                break;
            case '>':
                if (match('>'))       addToken(TokenType::RSHIFT, ">>");
                else if (match('='))  addToken(TokenType::GT_EQ, ">=");
                else                  addToken(TokenType::GT, ">");
                break;
            case '(':               addToken(TokenType::LPAREN, "("); break;
            case ')':               addToken(TokenType::RPAREN, ")"); break;
            case '{':               addToken(TokenType::LBRACE, "{"); break;
            case '}':               addToken(TokenType::RBRACE, "}"); break;
            case '[':               addToken(TokenType::LBRACKET, "["); break;
            case ']':               addToken(TokenType::RBRACKET, "]"); break;
            case ';':               addToken(TokenType::SEMICOLON, ";"); break;
            case ',':               addToken(TokenType::COMMA, ","); break;
            case '.':               addToken(TokenType::DOT, "."); break;
            case ':':
                if (match(':'))       addToken(TokenType::SCOPE, "::");
                else                  addToken(TokenType::COLON, ":");
                break;
            case '#':               addToken(TokenType::HASH, "#"); break;
            default:
                error(std::string("unexpected character '") + c + "'");
                break;
        }
    }

    tokens_.emplace_back(TokenType::END_OF_FILE, "", line_, col_);
    return tokens_;
}

} 
