#pragma once

namespace greencc {

struct LiteralExpr;
struct VarExpr;
struct BinaryExpr;
struct UnaryExpr;
struct AssignExpr;
struct CallExpr;
struct ArrayAccessExpr;
struct IOExpr;
struct ExprStmt;
struct Block;
struct VarDecl;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct ReturnStmt;
struct BreakStmt;
struct ContinueStmt;
struct FunctionDecl;
struct Program;

struct ASTVisitor {
    virtual ~ASTVisitor() = default;

    
    virtual void visit(LiteralExpr&)     = 0;
    virtual void visit(VarExpr&)         = 0;
    virtual void visit(BinaryExpr&)      = 0;
    virtual void visit(UnaryExpr&)       = 0;
    virtual void visit(AssignExpr&)      = 0;
    virtual void visit(CallExpr&)        = 0;
    virtual void visit(ArrayAccessExpr&) = 0;
    virtual void visit(IOExpr&)          = 0;

    
    virtual void visit(ExprStmt&)        = 0;
    virtual void visit(Block&)           = 0;
    virtual void visit(VarDecl&)         = 0;
    virtual void visit(IfStmt&)          = 0;
    virtual void visit(WhileStmt&)       = 0;
    virtual void visit(ForStmt&)         = 0;
    virtual void visit(ReturnStmt&)      = 0;
    virtual void visit(BreakStmt&)       = 0;
    virtual void visit(ContinueStmt&)    = 0;

    
    virtual void visit(FunctionDecl&)    = 0;
    virtual void visit(Program&)         = 0;
};

} 
