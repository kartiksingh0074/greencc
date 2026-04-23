#include "parser.h"
#include <stdexcept>
#include <sstream>
#include <iostream>

namespace greencc {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::current() const { return tokens_[pos_]; }
const Token& Parser::previous() const { return tokens_[pos_ - 1]; }

const Token& Parser::advance() {
    if (current().type != TokenType::END_OF_FILE) pos_++;
    return previous();
}

bool Parser::check(TokenType t) const {
    return current().type == t;
}

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto t : types) if (match(t)) return true;
    return false;
}

Token Parser::consume(TokenType t, const std::string& msg) {
    if (check(t)) return advance(), previous();
    error(msg + " (got '" + current().lexeme + "')");
    return Token(t, "", current().line, current().column);
}

void Parser::synchronize() {
    advance();
    while (!check(TokenType::END_OF_FILE)) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (current().type) {
            case TokenType::KW_INT: case TokenType::KW_FLOAT: case TokenType::KW_DOUBLE:
            case TokenType::KW_CHAR: case TokenType::KW_BOOL: case TokenType::KW_VOID:
            case TokenType::KW_IF: case TokenType::KW_WHILE: case TokenType::KW_FOR:
            case TokenType::KW_RETURN: case TokenType::RBRACE:
                return;
            default: advance();
        }
    }
}

void Parser::error(const std::string& msg) {
    std::ostringstream os;
    os << "Parse error [" << current().line << ":" << current().column << "]: " << msg;
    errors_.push_back(os.str());
}

bool Parser::isTypeKeyword(TokenType t) const {
    return t == TokenType::KW_INT || t == TokenType::KW_FLOAT ||
           t == TokenType::KW_DOUBLE || t == TokenType::KW_CHAR ||
           t == TokenType::KW_BOOL || t == TokenType::KW_VOID ||
           t == TokenType::KW_STRING || t == TokenType::KW_CONST;
}

std::string Parser::parseTypeName() {
    std::string type;
    if (match(TokenType::KW_CONST)) type = "const ";

    if (matchAny({TokenType::KW_INT, TokenType::KW_FLOAT, TokenType::KW_DOUBLE,
                  TokenType::KW_CHAR, TokenType::KW_BOOL, TokenType::KW_VOID,
                  TokenType::KW_STRING})) {
        type += previous().lexeme;
    } else {
        error("expected type name");
    }
    return type;
}

void Parser::parsePreprocessor() {
    
    advance(); 
    if (match(TokenType::KW_INCLUDE)) {
        
        if (check(TokenType::LT)) {
            std::string inc;
            advance(); 
            while (!check(TokenType::GT) && !check(TokenType::END_OF_FILE)) {
                inc += current().lexeme;
                advance();
            }
            if (check(TokenType::GT)) advance(); 
            
        } else if (check(TokenType::STRING_LIT)) {
            advance();
        }
    }
}

void Parser::parseUsingNamespace() {
    advance(); 
    if (match(TokenType::KW_NAMESPACE)) {
        if (match(TokenType::KW_STD)) {
            consume(TokenType::SEMICOLON, "expected ';' after 'using namespace std'");
        }
    }
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();

    
    while (!check(TokenType::END_OF_FILE)) {
        if (check(TokenType::HASH)) {
            
            int savedPos = pos_;
            advance(); 
            if (check(TokenType::KW_INCLUDE)) {
                advance(); 
                if (check(TokenType::LT)) {
                    std::string inc;
                    advance(); 
                    while (!check(TokenType::GT) && !check(TokenType::END_OF_FILE)) {
                        inc += current().lexeme;
                        advance();
                    }
                    if (check(TokenType::GT)) advance();
                    program->includes.push_back(inc);
                } else if (check(TokenType::STRING_LIT)) {
                    program->includes.push_back(current().lexeme);
                    advance();
                }
            }
            continue;
        }
        if (check(TokenType::KW_USING)) {
            parseUsingNamespace();
            program->hasUsing = true;
            continue;
        }
        break;
    }

    
    while (!check(TokenType::END_OF_FILE)) {
        try {
            auto decl = parseDeclaration();
            if (decl) program->declarations.push_back(std::move(decl));
        } catch (...) {
            synchronize();
        }
    }

    return program;
}

ASTPtr Parser::parseDeclaration() {
    int ln = current().line;
    std::string type = parseTypeName();
    std::string name = consume(TokenType::IDENTIFIER, "expected identifier").lexeme;

    if (check(TokenType::LPAREN)) {
        return parseFunctionDecl(type, name, ln);
    } else {
        return parseGlobalVarDecl(type, name, ln);
    }
}

std::unique_ptr<FunctionDecl> Parser::parseFunctionDecl(
        const std::string& type, const std::string& name, int line) {
    consume(TokenType::LPAREN, "expected '('");

    std::vector<Param> params;
    if (!check(TokenType::RPAREN)) {
        do {
            Param p;
            p.typeName = parseTypeName();
            p.name = consume(TokenType::IDENTIFIER, "expected parameter name").lexeme;
            params.push_back(std::move(p));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "expected ')'");

    auto body = parseBlock();
    return std::make_unique<FunctionDecl>(type, name, std::move(params), std::move(body), line);
}

StmtPtr Parser::parseGlobalVarDecl(
        const std::string& type, const std::string& name, int line) {
    bool isConst = (type.find("const") != std::string::npos);
    int arrSize = 0;
    ExprPtr init;

    if (match(TokenType::LBRACKET)) {
        if (check(TokenType::INTEGER_LIT)) {
            arrSize = std::stoi(current().lexeme);
            advance();
        }
        consume(TokenType::RBRACKET, "expected ']'");
    }

    if (match(TokenType::EQ)) {
        init = parseExpression();
    }
    consume(TokenType::SEMICOLON, "expected ';'");
    return std::make_unique<VarDecl>(type, name, std::move(init), isConst, arrSize, line);
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LBRACE))      return parseBlock();
    if (check(TokenType::KW_IF))       return parseIfStmt();
    if (check(TokenType::KW_WHILE))    return parseWhileStmt();
    if (check(TokenType::KW_FOR))      return parseForStmt();
    if (check(TokenType::KW_RETURN))   return parseReturnStmt();
    if (check(TokenType::KW_BREAK))    { int ln = current().line; advance(); consume(TokenType::SEMICOLON, "expected ';'"); return std::make_unique<BreakStmt>(ln); }
    if (check(TokenType::KW_CONTINUE)) { int ln = current().line; advance(); consume(TokenType::SEMICOLON, "expected ';'"); return std::make_unique<ContinueStmt>(ln); }

    
    if (isTypeKeyword(current().type)) {
        return parseVarDecl();
    }

    
    int ln = current().line;
    auto expr = parseExpression();
    consume(TokenType::SEMICOLON, "expected ';' after expression");
    return std::make_unique<ExprStmt>(std::move(expr), ln);
}

std::unique_ptr<Block> Parser::parseBlock() {
    int ln = current().line;
    consume(TokenType::LBRACE, "expected '{'");
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        stmts.push_back(parseStatement());
    }
    consume(TokenType::RBRACE, "expected '}'");
    return std::make_unique<Block>(std::move(stmts), ln);
}

StmtPtr Parser::parseVarDecl() {
    int ln = current().line;
    std::string type = parseTypeName();
    bool isConst = (type.find("const") != std::string::npos);
    std::string name = consume(TokenType::IDENTIFIER, "expected variable name").lexeme;

    int arrSize = 0;
    if (match(TokenType::LBRACKET)) {
        if (check(TokenType::INTEGER_LIT)) {
            arrSize = std::stoi(current().lexeme);
            advance();
        }
        consume(TokenType::RBRACKET, "expected ']'");
    }

    ExprPtr init;
    if (match(TokenType::EQ)) {
        init = parseExpression();
    }
    consume(TokenType::SEMICOLON, "expected ';' after variable declaration");
    return std::make_unique<VarDecl>(type, name, std::move(init), isConst, arrSize, ln);
}

StmtPtr Parser::parseIfStmt() {
    int ln = current().line;
    advance(); 
    consume(TokenType::LPAREN, "expected '(' after 'if'");
    auto cond = parseExpression();
    consume(TokenType::RPAREN, "expected ')'");
    auto thenBranch = parseStatement();
    StmtPtr elseBranch;
    if (match(TokenType::KW_ELSE)) {
        elseBranch = parseStatement();
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch),
                                     std::move(elseBranch), ln);
}

StmtPtr Parser::parseWhileStmt() {
    int ln = current().line;
    advance(); 
    consume(TokenType::LPAREN, "expected '(' after 'while'");
    auto cond = parseExpression();
    consume(TokenType::RPAREN, "expected ')'");
    auto body = parseStatement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), ln);
}

StmtPtr Parser::parseForStmt() {
    int ln = current().line;
    advance(); 
    consume(TokenType::LPAREN, "expected '(' after 'for'");

    
    StmtPtr init;
    if (!check(TokenType::SEMICOLON)) {
        if (isTypeKeyword(current().type)) {
            init = parseVarDecl(); 
        } else {
            auto expr = parseExpression();
            consume(TokenType::SEMICOLON, "expected ';'");
            init = std::make_unique<ExprStmt>(std::move(expr), ln);
        }
    } else {
        advance(); 
    }

    
    ExprPtr cond;
    if (!check(TokenType::SEMICOLON)) cond = parseExpression();
    consume(TokenType::SEMICOLON, "expected ';'");

    
    ExprPtr update;
    if (!check(TokenType::RPAREN)) update = parseExpression();
    consume(TokenType::RPAREN, "expected ')'");

    auto body = parseStatement();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond),
                                      std::move(update), std::move(body), ln);
}

StmtPtr Parser::parseReturnStmt() {
    int ln = current().line;
    advance(); 
    ExprPtr value;
    if (!check(TokenType::SEMICOLON)) {
        value = parseExpression();
    }
    consume(TokenType::SEMICOLON, "expected ';' after return");
    return std::make_unique<ReturnStmt>(std::move(value), ln);
}

ExprPtr Parser::parseExpression() {
    
    if (check(TokenType::KW_COUT) || check(TokenType::KW_CIN)) {
        return parseIOExpr();
    }
    
    if (check(TokenType::KW_STD) && pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::SCOPE &&
        pos_ + 2 < tokens_.size() &&
        (tokens_[pos_ + 2].type == TokenType::KW_COUT || tokens_[pos_ + 2].type == TokenType::KW_CIN)) {
        advance(); 
        advance(); 
        return parseIOExpr();
    }
    return parseAssignment();
}

ExprPtr Parser::parseIOExpr() {
    int ln = current().line;
    bool isOutput = (current().type == TokenType::KW_COUT);
    advance(); 

    TokenType ioOp = isOutput ? TokenType::LSHIFT : TokenType::RSHIFT;
    consume(ioOp, isOutput ? "expected '<<' after cout" : "expected '>>' after cin");

    std::vector<ExprPtr> operands;

    
    auto checkEndl = [&]() -> bool {
        if (check(TokenType::KW_ENDL)) {
            advance();
            return true;
        }
        
        if (check(TokenType::KW_STD) && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::SCOPE &&
            pos_ + 2 < tokens_.size() &&
            tokens_[pos_ + 2].type == TokenType::KW_ENDL) {
            advance(); advance(); advance(); 
            return true;
        }
        return false;
    };

    
    
    if (isOutput && checkEndl()) {
        operands.push_back(std::make_unique<VarExpr>("endl", ln));
    } else {
        operands.push_back(parseAdditive());
    }

    
    while (match(ioOp)) {
        if (isOutput && checkEndl()) {
            operands.push_back(std::make_unique<VarExpr>("endl", ln));
        } else {
            operands.push_back(parseAdditive());
        }
    }

    return std::make_unique<IOExpr>(isOutput, std::move(operands), ln);
}

ExprPtr Parser::parseAssignment() {
    auto expr = parseLogicalOr();

    if (auto* var = dynamic_cast<VarExpr*>(expr.get())) {
        if (matchAny({TokenType::EQ, TokenType::PLUS_EQ, TokenType::MINUS_EQ,
                      TokenType::STAR_EQ, TokenType::SLASH_EQ})) {
            TokenType op = previous().type;
            int ln = previous().line;
            auto val = parseAssignment(); 
            return std::make_unique<AssignExpr>(op, var->name, std::move(val), ln);
        }
    }
    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::OR_OR)) {
        int ln = previous().line;
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(TokenType::OR_OR, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();
    while (match(TokenType::AND_AND)) {
        int ln = previous().line;
        auto right = parseBitwiseOr();
        left = std::make_unique<BinaryExpr>(TokenType::AND_AND, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();
    while (match(TokenType::PIPE)) {
        int ln = previous().line;
        auto right = parseBitwiseXor();
        left = std::make_unique<BinaryExpr>(TokenType::PIPE, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();
    while (match(TokenType::CARET)) {
        int ln = previous().line;
        auto right = parseBitwiseAnd();
        left = std::make_unique<BinaryExpr>(TokenType::CARET, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseBitwiseAnd() {
    auto left = parseEquality();
    while (match(TokenType::AMPERSAND)) {
        int ln = previous().line;
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>(TokenType::AMPERSAND, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    auto left = parseRelational();
    while (matchAny({TokenType::EQ_EQ, TokenType::BANG_EQ})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto right = parseRelational();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseRelational() {
    auto left = parseShift();
    while (matchAny({TokenType::LT, TokenType::GT, TokenType::LT_EQ, TokenType::GT_EQ})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto right = parseShift();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseShift() {
    auto left = parseAdditive();
    while (matchAny({TokenType::LSHIFT, TokenType::RSHIFT})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto right = parseAdditive();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto right = parseUnary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (matchAny({TokenType::MINUS, TokenType::BANG, TokenType::TILDE,
                  TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        TokenType op = previous().type;
        int ln = previous().line;
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(operand), true, ln);
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        if (match(TokenType::LBRACKET)) {
            int ln = previous().line;
            auto idx = parseExpression();
            consume(TokenType::RBRACKET, "expected ']'");
            auto* var = dynamic_cast<VarExpr*>(expr.get());
            std::string name = var ? var->name : "unknown";
            expr = std::make_unique<ArrayAccessExpr>(name, std::move(idx), ln);
        } else if (match(TokenType::PLUS_PLUS) || previous().type == TokenType::MINUS_MINUS) {
            
            if (previous().type == TokenType::PLUS_PLUS || previous().type == TokenType::MINUS_MINUS) {
                int ln = previous().line;
                TokenType op = previous().type;
                expr = std::make_unique<UnaryExpr>(op, std::move(expr), false, ln);
            }
            break;
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    int ln = current().line;

    
    if (match(TokenType::INTEGER_LIT)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::INT, previous().lexeme, ln);
    }
    
    if (match(TokenType::FLOAT_LIT)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::FLOAT, previous().lexeme, ln);
    }
    
    if (match(TokenType::STRING_LIT)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::STRING, previous().lexeme, ln);
    }
    
    if (match(TokenType::CHAR_LIT)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::CHAR, previous().lexeme, ln);
    }
    
    if (match(TokenType::KW_TRUE)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::BOOL, "true", ln);
    }
    if (match(TokenType::KW_FALSE)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::BOOL, "false", ln);
    }
    
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "expected ')'");
        return expr;
    }
    
    if (match(TokenType::IDENTIFIER)) {
        std::string name = previous().lexeme;
        
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "expected ')'");
            return std::make_unique<CallExpr>(name, std::move(args), ln);
        }
        return std::make_unique<VarExpr>(name, ln);
    }

    error("expected expression");
    advance(); 
    return std::make_unique<LiteralExpr>(LiteralExpr::INT, "0", ln);
}

} 
