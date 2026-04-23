#pragma once
#include <memory>
#include <string>
#include <vector>
#include "token.h"

namespace greencc {

struct ASTVisitor;

struct ASTNode {
    int line = 0;
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& v) = 0;
};

using ASTPtr = std::unique_ptr<ASTNode>;

struct Expr : ASTNode {};
using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr : Expr {
    enum Kind { INT, FLOAT, STRING, CHAR, BOOL };
    Kind        kind;
    std::string value;     
    LiteralExpr(Kind k, std::string v, int ln) : kind(k), value(std::move(v)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct VarExpr : Expr {
    std::string name;
    VarExpr(std::string n, int ln) : name(std::move(n)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct BinaryExpr : Expr {
    TokenType op;
    ExprPtr   left, right;
    BinaryExpr(TokenType o, ExprPtr l, ExprPtr r, int ln)
        : op(o), left(std::move(l)), right(std::move(r)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr   operand;
    bool      prefix; 
    UnaryExpr(TokenType o, ExprPtr e, bool pre, int ln)
        : op(o), operand(std::move(e)), prefix(pre) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct AssignExpr : Expr {
    TokenType op;        
    std::string target;  
    ExprPtr     value;
    AssignExpr(TokenType o, std::string t, ExprPtr val, int ln)
        : op(o), target(std::move(t)), value(std::move(val)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct CallExpr : Expr {
    std::string           callee;
    std::vector<ExprPtr>  args;
    CallExpr(std::string c, std::vector<ExprPtr> a, int ln)
        : callee(std::move(c)), args(std::move(a)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct ArrayAccessExpr : Expr {
    std::string name;
    ExprPtr     index;
    ArrayAccessExpr(std::string n, ExprPtr idx, int ln)
        : name(std::move(n)), index(std::move(idx)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct IOExpr : Expr {
    bool                  isOutput; 
    std::vector<ExprPtr>  operands;
    IOExpr(bool out, std::vector<ExprPtr> ops, int ln)
        : isOutput(out), operands(std::move(ops)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct Stmt : ASTNode {};
using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e, int ln) : expr(std::move(e)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct Block : Stmt {
    std::vector<StmtPtr> stmts;
    Block(std::vector<StmtPtr> s, int ln) : stmts(std::move(s)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct VarDecl : Stmt {
    std::string typeName;
    std::string name;
    ExprPtr     init;       
    bool        isConst;
    int         arraySize;  
    VarDecl(std::string type, std::string n, ExprPtr i, bool c, int arr, int ln)
        : typeName(std::move(type)), name(std::move(n)),
          init(std::move(i)), isConst(c), arraySize(arr) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct IfStmt : Stmt {
    ExprPtr  condition;
    StmtPtr  thenBranch;
    StmtPtr  elseBranch; 
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e, int ln)
        : condition(std::move(c)), thenBranch(std::move(t)),
          elseBranch(std::move(e)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b, int ln)
        : condition(std::move(c)), body(std::move(b)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct ForStmt : Stmt {
    StmtPtr init;       
    ExprPtr condition;
    ExprPtr update;
    StmtPtr body;
    ForStmt(StmtPtr i, ExprPtr c, ExprPtr u, StmtPtr b, int ln)
        : init(std::move(i)), condition(std::move(c)),
          update(std::move(u)), body(std::move(b)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct ReturnStmt : Stmt {
    ExprPtr value; 
    ReturnStmt(ExprPtr v, int ln) : value(std::move(v)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct BreakStmt : Stmt {
    BreakStmt(int ln) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct ContinueStmt : Stmt {
    ContinueStmt(int ln) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct Param {
    std::string typeName;
    std::string name;
};

struct FunctionDecl : ASTNode {
    std::string        returnType;
    std::string        name;
    std::vector<Param> params;
    std::unique_ptr<Block> body;
    FunctionDecl(std::string ret, std::string n, std::vector<Param> p,
                 std::unique_ptr<Block> b, int ln)
        : returnType(std::move(ret)), name(std::move(n)),
          params(std::move(p)), body(std::move(b)) { line = ln; }
    void accept(ASTVisitor& v) override;
};

struct Program : ASTNode {
    std::vector<ASTPtr> declarations; 
    std::vector<std::string> includes;
    bool hasUsing = false;            
    Program() { line = 0; }
    void accept(ASTVisitor& v) override;
};

} 
