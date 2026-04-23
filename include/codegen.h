#pragma once
#include <string>
#include "visitor.h"
#include "ast.h"

namespace greencc {

class CodeGenerator : public ASTVisitor {
public:
    std::string generate(Program& program);

    void visit(LiteralExpr& e) override;
    void visit(VarExpr& e) override;
    void visit(BinaryExpr& e) override;
    void visit(UnaryExpr& e) override;
    void visit(AssignExpr& e) override;
    void visit(CallExpr& e) override;
    void visit(ArrayAccessExpr& e) override;
    void visit(IOExpr& e) override;
    void visit(ExprStmt& s) override;
    void visit(Block& s) override;
    void visit(VarDecl& s) override;
    void visit(IfStmt& s) override;
    void visit(WhileStmt& s) override;
    void visit(ForStmt& s) override;
    void visit(ReturnStmt& s) override;
    void visit(BreakStmt& s) override;
    void visit(ContinueStmt& s) override;
    void visit(FunctionDecl& f) override;
    void visit(Program& p) override;

private:
    std::string output_;
    int         indent_ = 0;

    void emit(const std::string& s);
    void emitLine(const std::string& s);
    void emitIndent();
};

} 
