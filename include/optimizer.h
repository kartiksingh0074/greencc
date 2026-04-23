#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include "ast.h"

namespace greencc {

class Optimizer {
public:
    struct OptimizationResult {
        int constantsFolded       = 0;
        int constantsPropagated   = 0;
        int deadCodeEliminated    = 0;
        int strengthReductions    = 0;
        int loopInvariantsMoved   = 0;
        int csesEliminated        = 0;
        int functionsInlined      = 0;
        int peepholeOptimizations = 0;
        int registerHintsEmitted  = 0;

        std::string toString() const;
    };

    OptimizationResult optimize(Program& program);

    
    
    OptimizationResult result_;

    
    std::unordered_set<std::string> hotVariables_;

    
    void constantFolding(Program& program);
    void constantPropagation(Program& program);
    void deadCodeElimination(Program& program);
    void strengthReduction(Program& program);
    void loopInvariantCodeMotion(Program& program);
    void commonSubexprElimination(Program& program);
    void functionInlining(Program& program);
    void peepholeOptimization(Program& program);
    void energyAwareRegisterHints(Program& program);

    
    ExprPtr foldExpr(ExprPtr& expr);
    ExprPtr reduceStrength(ExprPtr& expr);
    ExprPtr peepholeExpr(ExprPtr& expr);
    void eliminateDeadStmts(std::vector<StmtPtr>& stmts,
                            std::unordered_map<std::string, bool>& used);
    void collectUsedVars(Expr* expr, std::unordered_map<std::string, bool>& used);
    void collectUsedVarsInStmt(Stmt* stmt, std::unordered_map<std::string, bool>& used);
    bool isLoopInvariant(Expr* expr, const std::unordered_map<std::string, bool>& modified);
    void collectModifiedVars(Stmt* stmt, std::unordered_map<std::string, bool>& modified);
};

} 
