#pragma once
#include <memory>
#include <string>
#include <vector>
#include "token.h"
#include "ast.h"

namespace greencc {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<Program> parse();
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::vector<Token>       tokens_;
    std::vector<std::string> errors_;
    size_t                   pos_ = 0;

    
    const Token& current() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType t) const;
    bool match(TokenType t);
    bool matchAny(std::initializer_list<TokenType> types);
    Token consume(TokenType t, const std::string& msg);
    void synchronize();
    void error(const std::string& msg);

    
    bool isTypeKeyword(TokenType t) const;
    std::string parseTypeName();

    
    void parsePreprocessor();
    void parseUsingNamespace();
    ASTPtr parseDeclaration();
    std::unique_ptr<FunctionDecl> parseFunctionDecl(const std::string& type, const std::string& name, int line);
    StmtPtr parseGlobalVarDecl(const std::string& type, const std::string& name, int line);

    
    StmtPtr parseStatement();
    std::unique_ptr<Block> parseBlock();
    StmtPtr parseVarDecl();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseReturnStmt();

    
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseRelational();
    ExprPtr parseShift();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    ExprPtr parseIOExpr();
};

} 
