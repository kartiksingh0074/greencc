#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "visitor.h"
#include "ast.h"

namespace greencc {

struct EnergyReport {
    double totalEnergy = 0.0;
    std::unordered_map<std::string, double> perFunction; 
    std::vector<std::pair<int, double>> perLine;         

    std::string toString() const;
};

class EnergyEstimator : public ASTVisitor {
public:
    EnergyReport estimate(Program& program);

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
    
    static constexpr double COST_INT_ADD     = 1.0;
    static constexpr double COST_INT_MUL     = 3.0;
    static constexpr double COST_INT_DIV     = 8.0;
    static constexpr double COST_FLOAT_ADD   = 4.0;
    static constexpr double COST_FLOAT_MUL   = 5.0;
    static constexpr double COST_FLOAT_DIV   = 12.0;
    static constexpr double COST_MEMORY      = 10.0;
    static constexpr double COST_BRANCH      = 2.0;
    static constexpr double COST_CALL        = 15.0;
    static constexpr double COST_IO          = 50.0;
    static constexpr double COST_ASSIGN      = 2.0;
    static constexpr double COST_COMPARISON  = 1.5;
    static constexpr double COST_LOGICAL     = 1.0;
    static constexpr double COST_BITWISE     = 1.0;
    static constexpr double COST_SHIFT       = 1.0;
    static constexpr double COST_UNARY       = 1.0;
    static constexpr double COST_RETURN      = 3.0;

    static constexpr int    DEFAULT_LOOP_ITERS = 100; 

    double        currentEnergy_ = 0.0;
    int           loopMultiplier_ = 1;
    std::string   currentFunction_;
    EnergyReport  report_;

    void addCost(double cost, int line);
    double binaryOpCost(TokenType op) const;
};

} 
