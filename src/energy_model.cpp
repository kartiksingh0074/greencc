#include "energy_model.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace greencc {

std::string EnergyReport::toString() const {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1);
    os << "╔══════════════════════════════════════════════════════╗\n";
    os << "║           ⚡ ENERGY CONSUMPTION REPORT ⚡            ║\n";
    os << "╠══════════════════════════════════════════════════════╣\n";
    os << "║  Total Energy: " << std::setw(10) << totalEnergy
       << " units" << std::string(22, ' ') << "║\n";
    os << "╠══════════════════════════════════════════════════════╣\n";

    if (!perFunction.empty()) {
        os << "║  Per-function breakdown:                             ║\n";

        
        std::vector<std::pair<std::string, double>> sorted(perFunction.begin(), perFunction.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });

        for (auto& [name, energy] : sorted) {
            std::string entry = "    " + name + "()";
            entry.resize(30, ' ');
            os << "║  " << entry << std::setw(10) << energy << " units"
               << std::string(4, ' ') << "║\n";
        }
    }

    os << "╚══════════════════════════════════════════════════════╝\n";
    return os.str();
}

void EnergyEstimator::addCost(double cost, int line) {
    double actual = cost * loopMultiplier_;
    currentEnergy_ += actual;
    report_.totalEnergy += actual;
    if (!currentFunction_.empty()) {
        report_.perFunction[currentFunction_] += actual;
    }
    report_.perLine.push_back({line, actual});
}

double EnergyEstimator::binaryOpCost(TokenType op) const {
    switch (op) {
        case TokenType::PLUS: case TokenType::MINUS:
            return COST_INT_ADD;
        case TokenType::STAR:
            return COST_INT_MUL;
        case TokenType::SLASH: case TokenType::PERCENT:
            return COST_INT_DIV;
        case TokenType::EQ_EQ: case TokenType::BANG_EQ:
        case TokenType::LT: case TokenType::GT:
        case TokenType::LT_EQ: case TokenType::GT_EQ:
            return COST_COMPARISON;
        case TokenType::AND_AND: case TokenType::OR_OR:
            return COST_LOGICAL;
        case TokenType::AMPERSAND: case TokenType::PIPE:
        case TokenType::CARET:
            return COST_BITWISE;
        case TokenType::LSHIFT: case TokenType::RSHIFT:
            return COST_SHIFT;
        default:
            return COST_INT_ADD;
    }
}

EnergyReport EnergyEstimator::estimate(Program& program) {
    report_ = EnergyReport{};
    currentEnergy_ = 0.0;
    loopMultiplier_ = 1;
    program.accept(*this);
    return report_;
}

void EnergyEstimator::visit(LiteralExpr&) {
    
}

void EnergyEstimator::visit(VarExpr& e) {
    addCost(COST_MEMORY, e.line); 
}

void EnergyEstimator::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);
    addCost(binaryOpCost(e.op), e.line);
}

void EnergyEstimator::visit(UnaryExpr& e) {
    e.operand->accept(*this);
    addCost(COST_UNARY, e.line);
}

void EnergyEstimator::visit(AssignExpr& e) {
    e.value->accept(*this);
    addCost(COST_ASSIGN + COST_MEMORY, e.line); 
}

void EnergyEstimator::visit(CallExpr& e) {
    for (auto& arg : e.args) arg->accept(*this);
    addCost(COST_CALL, e.line);
}

void EnergyEstimator::visit(ArrayAccessExpr& e) {
    e.index->accept(*this);
    addCost(COST_MEMORY * 1.5, e.line); 
}

void EnergyEstimator::visit(IOExpr& e) {
    for (auto& op : e.operands) op->accept(*this);
    addCost(COST_IO, e.line);
}

void EnergyEstimator::visit(ExprStmt& s) {
    s.expr->accept(*this);
}

void EnergyEstimator::visit(Block& s) {
    for (auto& stmt : s.stmts) stmt->accept(*this);
}

void EnergyEstimator::visit(VarDecl& s) {
    if (s.init) s.init->accept(*this);
    addCost(COST_MEMORY, s.line); 
}

void EnergyEstimator::visit(IfStmt& s) {
    s.condition->accept(*this);
    addCost(COST_BRANCH, s.line);
    
    double saved = currentEnergy_;
    s.thenBranch->accept(*this);
    if (s.elseBranch) s.elseBranch->accept(*this);
}

void EnergyEstimator::visit(WhileStmt& s) {
    addCost(COST_BRANCH, s.line);
    int prevMul = loopMultiplier_;
    loopMultiplier_ *= DEFAULT_LOOP_ITERS;
    s.condition->accept(*this);
    s.body->accept(*this);
    loopMultiplier_ = prevMul;
}

void EnergyEstimator::visit(ForStmt& s) {
    if (s.init) s.init->accept(*this);
    addCost(COST_BRANCH, s.line);
    int prevMul = loopMultiplier_;
    loopMultiplier_ *= DEFAULT_LOOP_ITERS;
    if (s.condition) s.condition->accept(*this);
    if (s.update) s.update->accept(*this);
    s.body->accept(*this);
    loopMultiplier_ = prevMul;
}

void EnergyEstimator::visit(ReturnStmt& s) {
    if (s.value) s.value->accept(*this);
    addCost(COST_RETURN, s.line);
}

void EnergyEstimator::visit(BreakStmt&) {}
void EnergyEstimator::visit(ContinueStmt&) {}

void EnergyEstimator::visit(FunctionDecl& f) {
    currentFunction_ = f.name;
    if (f.body) f.body->accept(*this);
    currentFunction_.clear();
}

void EnergyEstimator::visit(Program& p) {
    for (auto& decl : p.declarations) {
        decl->accept(*this);
    }
}

} 
