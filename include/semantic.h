#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "visitor.h"
#include "ast.h"

namespace greencc {

class SemanticAnalyzer : public ASTVisitor {
public:
    void analyze(Program& program);
    const std::vector<std::string>& errors() const { return errors_; }

    
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
    struct VarInfo {
        std::string type;
        bool isConst;
    };
    struct FuncInfo {
        std::string returnType;
        int arity;
    };

    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, FuncInfo> functions_;
    std::vector<std::string> errors_;
    std::string currentFunction_;
    int loopDepth_ = 0;

    void pushScope();
    void popScope();
    void declare(const std::string& name, const std::string& type, bool isConst, int line);
    VarInfo* resolve(const std::string& name);
    void error(int line, const std::string& msg);
};

} 
