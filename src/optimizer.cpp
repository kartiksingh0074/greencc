#include "optimizer.h"
#include "visitor.h"
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace greencc {

std::string Optimizer::OptimizationResult::toString() const {
    std::ostringstream os;
    os << "╔══════════════════════════════════════════════════════╗\n";
    os << "║          🌿 OPTIMIZATION SUMMARY 🌿                 ║\n";
    os << "╠══════════════════════════════════════════════════════╣\n";
    os << "║  Constants folded:         " << std::setw(5) << constantsFolded
       << std::string(19, ' ') << "║\n";
    os << "║  Constants propagated:     " << std::setw(5) << constantsPropagated
       << std::string(19, ' ') << "║\n";
    os << "║  Dead code eliminated:     " << std::setw(5) << deadCodeEliminated
       << std::string(19, ' ') << "║\n";
    os << "║  Strength reductions:      " << std::setw(5) << strengthReductions
       << std::string(19, ' ') << "║\n";
    os << "║  Loop invariants moved:    " << std::setw(5) << loopInvariantsMoved
       << std::string(19, ' ') << "║\n";
    os << "║  CSEs eliminated:          " << std::setw(5) << csesEliminated
       << std::string(19, ' ') << "║\n";
    os << "║  Functions inlined:        " << std::setw(5) << functionsInlined
       << std::string(19, ' ') << "║\n";
    os << "║  Peephole optimizations:   " << std::setw(5) << peepholeOptimizations
       << std::string(19, ' ') << "║\n";
    os << "║  Register hints emitted:   " << std::setw(5) << registerHintsEmitted
       << std::string(19, ' ') << "║\n";
    os << "╚══════════════════════════════════════════════════════╝\n";
    return os.str();
}

Optimizer::OptimizationResult Optimizer::optimize(Program& program) {
    result_ = OptimizationResult{};
    hotVariables_.clear();

    constantFolding(program);
    constantPropagation(program);
    deadCodeElimination(program);
    strengthReduction(program);
    loopInvariantCodeMotion(program);
    commonSubexprElimination(program);
    peepholeOptimization(program);
    functionInlining(program);
    energyAwareRegisterHints(program);

    return result_;
}

ExprPtr Optimizer::foldExpr(ExprPtr& expr) {
    auto* bin = dynamic_cast<BinaryExpr*>(expr.get());
    if (!bin) return nullptr;

    
    auto foldedLeft = foldExpr(bin->left);
    if (foldedLeft) bin->left = std::move(foldedLeft);

    auto foldedRight = foldExpr(bin->right);
    if (foldedRight) bin->right = std::move(foldedRight);

    
    auto* leftLit = dynamic_cast<LiteralExpr*>(bin->left.get());
    auto* rightLit = dynamic_cast<LiteralExpr*>(bin->right.get());

    if (!leftLit || !rightLit) return nullptr;
    if (leftLit->kind != LiteralExpr::INT || rightLit->kind != LiteralExpr::INT) return nullptr;

    long long lv = std::stoll(leftLit->value);
    long long rv = std::stoll(rightLit->value);
    long long result = 0;

    switch (bin->op) {
        case TokenType::PLUS:    result = lv + rv; break;
        case TokenType::MINUS:   result = lv - rv; break;
        case TokenType::STAR:    result = lv * rv; break;
        case TokenType::SLASH:   if (rv == 0) return nullptr; result = lv / rv; break;
        case TokenType::PERCENT: if (rv == 0) return nullptr; result = lv % rv; break;
        case TokenType::LSHIFT:  result = lv << rv; break;
        case TokenType::RSHIFT:  result = lv >> rv; break;
        case TokenType::AMPERSAND: result = lv & rv; break;
        case TokenType::PIPE:    result = lv | rv; break;
        case TokenType::CARET:   result = lv ^ rv; break;
        default: return nullptr;
    }

    result_.constantsFolded++;
    return std::make_unique<LiteralExpr>(LiteralExpr::INT, std::to_string(result), bin->line);
}

class ConstFoldVisitor : public ASTVisitor {
    Optimizer* opt_;
public:
    ConstFoldVisitor(Optimizer* o) : opt_(o) {}

    void foldInExpr(ExprPtr& expr) {
        auto folded = opt_->foldExpr(expr);
        if (folded) expr = std::move(folded);
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr& e) override {
        e.left->accept(*this);
        e.right->accept(*this);
    }
    void visit(UnaryExpr& e) override { e.operand->accept(*this); }
    void visit(AssignExpr& e) override {
        e.value->accept(*this);
        foldInExpr(e.value);
    }
    void visit(CallExpr& e) override {
        for (auto& a : e.args) { a->accept(*this); foldInExpr(a); }
    }
    void visit(ArrayAccessExpr& e) override {
        e.index->accept(*this);
        foldInExpr(e.index);
    }
    void visit(IOExpr& e) override {
        for (auto& o : e.operands) { o->accept(*this); foldInExpr(o); }
    }
    void visit(ExprStmt& s) override {
        s.expr->accept(*this);
        foldInExpr(s.expr);
    }
    void visit(Block& s) override {
        for (auto& st : s.stmts) st->accept(*this);
    }
    void visit(VarDecl& s) override {
        if (s.init) { s.init->accept(*this); foldInExpr(s.init); }
    }
    void visit(IfStmt& s) override {
        s.condition->accept(*this); foldInExpr(s.condition);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override {
        s.condition->accept(*this); foldInExpr(s.condition);
        s.body->accept(*this);
    }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        if (s.condition) { s.condition->accept(*this); foldInExpr(s.condition); }
        if (s.update) { s.update->accept(*this);  }
        s.body->accept(*this);
    }
    void visit(ReturnStmt& s) override {
        if (s.value) { s.value->accept(*this); foldInExpr(s.value); }
    }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override {
        if (f.body) f.body->accept(*this);
    }
    void visit(Program& p) override {
        for (auto& d : p.declarations) d->accept(*this);
    }
};

void Optimizer::constantFolding(Program& program) {
    ConstFoldVisitor v(this);
    program.accept(v);
}

class ConstPropVisitor : public ASTVisitor {
    Optimizer* opt_;
    std::unordered_map<std::string, LiteralExpr*> constants_; 

public:
    ConstPropVisitor(Optimizer* o) : opt_(o) {}

    void propagate(ExprPtr& expr) {
        if (auto* var = dynamic_cast<VarExpr*>(expr.get())) {
            auto it = constants_.find(var->name);
            if (it != constants_.end()) {
                expr = std::make_unique<LiteralExpr>(it->second->kind,
                                                      it->second->value, var->line);
                opt_->result_.constantsPropagated++;
            }
        }
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr& e) override {
        propagate(e.left);
        propagate(e.right);
        e.left->accept(*this);
        e.right->accept(*this);
    }
    void visit(UnaryExpr& e) override {
        propagate(e.operand);
        e.operand->accept(*this);
    }
    void visit(AssignExpr& e) override {
        propagate(e.value);
        e.value->accept(*this);
        
        constants_.erase(e.target);
    }
    void visit(CallExpr& e) override {
        for (auto& a : e.args) { propagate(a); a->accept(*this); }
    }
    void visit(ArrayAccessExpr& e) override {
        propagate(e.index); e.index->accept(*this);
    }
    void visit(IOExpr& e) override {
        for (auto& o : e.operands) { propagate(o); o->accept(*this); }
    }
    void visit(ExprStmt& s) override {
        propagate(s.expr); s.expr->accept(*this);
    }
    void visit(Block& s) override {
        for (auto& st : s.stmts) st->accept(*this);
    }
    void visit(VarDecl& s) override {
        if (s.init) {
            propagate(s.init);
            s.init->accept(*this);
            
            if (s.isConst) {
                if (auto* lit = dynamic_cast<LiteralExpr*>(s.init.get())) {
                    constants_[s.name] = lit;
                }
            }
        }
    }
    void visit(IfStmt& s) override {
        propagate(s.condition); s.condition->accept(*this);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override {
        propagate(s.condition); s.condition->accept(*this);
        s.body->accept(*this);
    }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        if (s.condition) { propagate(s.condition); s.condition->accept(*this); }
        if (s.update) { propagate(s.update); s.update->accept(*this); }
        s.body->accept(*this);
    }
    void visit(ReturnStmt& s) override {
        if (s.value) { propagate(s.value); s.value->accept(*this); }
    }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override {
        auto saved = constants_;
        if (f.body) f.body->accept(*this);
        constants_ = saved;
    }
    void visit(Program& p) override {
        for (auto& d : p.declarations) d->accept(*this);
    }
};

void Optimizer::constantPropagation(Program& program) {
    ConstPropVisitor v(this);
    program.accept(v);
}

void Optimizer::collectUsedVars(Expr* expr, std::unordered_map<std::string, bool>& used) {
    if (!expr) return;
    if (auto* v = dynamic_cast<VarExpr*>(expr))      { used[v->name] = true; return; }
    if (auto* b = dynamic_cast<BinaryExpr*>(expr))    { collectUsedVars(b->left.get(), used); collectUsedVars(b->right.get(), used); return; }
    if (auto* u = dynamic_cast<UnaryExpr*>(expr))     { collectUsedVars(u->operand.get(), used); return; }
    if (auto* a = dynamic_cast<AssignExpr*>(expr))    { used[a->target] = true; collectUsedVars(a->value.get(), used); return; }
    if (auto* c = dynamic_cast<CallExpr*>(expr))      { for (auto& arg : c->args) collectUsedVars(arg.get(), used); return; }
    if (auto* arr = dynamic_cast<ArrayAccessExpr*>(expr)) { used[arr->name] = true; collectUsedVars(arr->index.get(), used); return; }
    if (auto* io = dynamic_cast<IOExpr*>(expr))       { for (auto& op : io->operands) collectUsedVars(op.get(), used); return; }
}

void Optimizer::collectUsedVarsInStmt(Stmt* stmt, std::unordered_map<std::string, bool>& used) {
    if (!stmt) return;
    if (auto* e = dynamic_cast<ExprStmt*>(stmt)) { collectUsedVars(e->expr.get(), used); return; }
    if (auto* b = dynamic_cast<Block*>(stmt)) { for (auto& s : b->stmts) collectUsedVarsInStmt(s.get(), used); return; }
    if (auto* v = dynamic_cast<VarDecl*>(stmt)) { if (v->init) collectUsedVars(v->init.get(), used); return; }
    if (auto* i = dynamic_cast<IfStmt*>(stmt)) { collectUsedVars(i->condition.get(), used); collectUsedVarsInStmt(i->thenBranch.get(), used); if (i->elseBranch) collectUsedVarsInStmt(i->elseBranch.get(), used); return; }
    if (auto* w = dynamic_cast<WhileStmt*>(stmt)) { collectUsedVars(w->condition.get(), used); collectUsedVarsInStmt(w->body.get(), used); return; }
    if (auto* f = dynamic_cast<ForStmt*>(stmt)) { collectUsedVarsInStmt(f->init.get(), used); collectUsedVars(f->condition.get(), used); collectUsedVars(f->update.get(), used); collectUsedVarsInStmt(f->body.get(), used); return; }
    if (auto* r = dynamic_cast<ReturnStmt*>(stmt)) { if (r->value) collectUsedVars(r->value.get(), used); return; }
}

void Optimizer::eliminateDeadStmts(std::vector<StmtPtr>& stmts,
                                    std::unordered_map<std::string, bool>& used) {
    
    for (auto& s : stmts) collectUsedVarsInStmt(s.get(), used);

    
    auto it = stmts.begin();
    while (it != stmts.end()) {
        if (auto* vd = dynamic_cast<VarDecl*>(it->get())) {
            if (!used.count(vd->name) && !vd->isConst && vd->arraySize == 0) {
                result_.deadCodeEliminated++;
                it = stmts.erase(it);
                continue;
            }
        }
        
        if (dynamic_cast<ReturnStmt*>(it->get())) {
            auto next = std::next(it);
            while (next != stmts.end()) {
                result_.deadCodeEliminated++;
                next = stmts.erase(next);
            }
            break;
        }
        ++it;
    }
}

class DeadCodeVisitor : public ASTVisitor {
    Optimizer* opt_;
public:
    DeadCodeVisitor(Optimizer* o) : opt_(o) {}

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr&) override {}
    void visit(UnaryExpr&) override {}
    void visit(AssignExpr&) override {}
    void visit(CallExpr&) override {}
    void visit(ArrayAccessExpr&) override {}
    void visit(IOExpr&) override {}
    void visit(ExprStmt&) override {}
    void visit(VarDecl&) override {}
    void visit(IfStmt& s) override {
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override { s.body->accept(*this); }
    void visit(ForStmt& s) override { s.body->accept(*this); }
    void visit(ReturnStmt&) override {}
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}

    void visit(Block& s) override {
        std::unordered_map<std::string, bool> used;
        opt_->eliminateDeadStmts(s.stmts, used);
        for (auto& st : s.stmts) st->accept(*this);
    }
    void visit(FunctionDecl& f) override {
        if (f.body) f.body->accept(*this);
    }
    void visit(Program& p) override {
        for (auto& d : p.declarations) d->accept(*this);
    }
};

void Optimizer::deadCodeElimination(Program& program) {
    DeadCodeVisitor v(this);
    program.accept(v);
}

ExprPtr Optimizer::reduceStrength(ExprPtr& expr) {
    auto* bin = dynamic_cast<BinaryExpr*>(expr.get());
    if (!bin) return nullptr;

    
    auto rl = reduceStrength(bin->left);
    if (rl) bin->left = std::move(rl);
    auto rr = reduceStrength(bin->right);
    if (rr) bin->right = std::move(rr);

    auto* rightLit = dynamic_cast<LiteralExpr*>(bin->right.get());
    if (!rightLit || rightLit->kind != LiteralExpr::INT) return nullptr;

    long long val = std::stoll(rightLit->value);

    
    if (bin->op == TokenType::STAR && val > 0 && (val & (val - 1)) == 0) {
        int shift = 0;
        long long tmp = val;
        while (tmp > 1) { tmp >>= 1; shift++; }
        result_.strengthReductions++;
        return std::make_unique<BinaryExpr>(
            TokenType::LSHIFT, std::move(bin->left),
            std::make_unique<LiteralExpr>(LiteralExpr::INT, std::to_string(shift), bin->line),
            bin->line);
    }

    
    if (bin->op == TokenType::SLASH && val > 0 && (val & (val - 1)) == 0) {
        int shift = 0;
        long long tmp = val;
        while (tmp > 1) { tmp >>= 1; shift++; }
        result_.strengthReductions++;
        return std::make_unique<BinaryExpr>(
            TokenType::RSHIFT, std::move(bin->left),
            std::make_unique<LiteralExpr>(LiteralExpr::INT, std::to_string(shift), bin->line),
            bin->line);
    }

    
    if (bin->op == TokenType::STAR && val == 1) {
        result_.strengthReductions++;
        return std::move(bin->left);
    }

    
    if (bin->op == TokenType::PLUS && val == 0) {
        result_.strengthReductions++;
        return std::move(bin->left);
    }

    
    if (bin->op == TokenType::MINUS && val == 0) {
        result_.strengthReductions++;
        return std::move(bin->left);
    }

    
    if (bin->op == TokenType::STAR && val == 0) {
        result_.strengthReductions++;
        return std::make_unique<LiteralExpr>(LiteralExpr::INT, "0", bin->line);
    }

    return nullptr;
}

class StrengthReduceVisitor : public ASTVisitor {
    Optimizer* opt_;
public:
    StrengthReduceVisitor(Optimizer* o) : opt_(o) {}

    void reduce(ExprPtr& expr) {
        auto r = opt_->reduceStrength(expr);
        if (r) expr = std::move(r);
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr& e) override { e.left->accept(*this); e.right->accept(*this); }
    void visit(UnaryExpr& e) override { e.operand->accept(*this); }
    void visit(AssignExpr& e) override { reduce(e.value); e.value->accept(*this); }
    void visit(CallExpr& e) override { for (auto& a : e.args) { reduce(a); a->accept(*this); } }
    void visit(ArrayAccessExpr& e) override { reduce(e.index); e.index->accept(*this); }
    void visit(IOExpr& e) override { for (auto& o : e.operands) { reduce(o); o->accept(*this); } }
    void visit(ExprStmt& s) override { reduce(s.expr); s.expr->accept(*this); }
    void visit(Block& s) override { for (auto& st : s.stmts) st->accept(*this); }
    void visit(VarDecl& s) override { if (s.init) { reduce(s.init); s.init->accept(*this); } }
    void visit(IfStmt& s) override {
        reduce(s.condition); s.condition->accept(*this);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override { reduce(s.condition); s.condition->accept(*this); s.body->accept(*this); }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        if (s.condition) { reduce(s.condition); s.condition->accept(*this); }
        if (s.update) { reduce(s.update); s.update->accept(*this); }
        s.body->accept(*this);
    }
    void visit(ReturnStmt& s) override { if (s.value) { reduce(s.value); s.value->accept(*this); } }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override { if (f.body) f.body->accept(*this); }
    void visit(Program& p) override { for (auto& d : p.declarations) d->accept(*this); }
};

void Optimizer::strengthReduction(Program& program) {
    StrengthReduceVisitor v(this);
    program.accept(v);
}

void Optimizer::collectModifiedVars(Stmt* stmt, std::unordered_map<std::string, bool>& modified) {
    if (!stmt) return;
    if (auto* e = dynamic_cast<ExprStmt*>(stmt)) {
        if (auto* a = dynamic_cast<AssignExpr*>(e->expr.get())) { modified[a->target] = true; }
        return;
    }
    if (auto* b = dynamic_cast<Block*>(stmt)) { for (auto& s : b->stmts) collectModifiedVars(s.get(), modified); return; }
    if (auto* v = dynamic_cast<VarDecl*>(stmt)) { modified[v->name] = true; return; }
    if (auto* i = dynamic_cast<IfStmt*>(stmt)) { collectModifiedVars(i->thenBranch.get(), modified); if (i->elseBranch) collectModifiedVars(i->elseBranch.get(), modified); return; }
    if (auto* w = dynamic_cast<WhileStmt*>(stmt)) { collectModifiedVars(w->body.get(), modified); return; }
    if (auto* f = dynamic_cast<ForStmt*>(stmt)) { collectModifiedVars(f->init.get(), modified); collectModifiedVars(f->body.get(), modified); return; }
}

bool Optimizer::isLoopInvariant(Expr* expr, const std::unordered_map<std::string, bool>& modified) {
    if (!expr) return true;
    if (dynamic_cast<LiteralExpr*>(expr)) return true;
    if (auto* v = dynamic_cast<VarExpr*>(expr)) return !modified.count(v->name);
    if (auto* b = dynamic_cast<BinaryExpr*>(expr)) return isLoopInvariant(b->left.get(), modified) && isLoopInvariant(b->right.get(), modified);
    if (auto* u = dynamic_cast<UnaryExpr*>(expr)) return isLoopInvariant(u->operand.get(), modified);
    return false;
}

class LICMVisitor : public ASTVisitor {
    Optimizer* opt_;
public:
    LICMVisitor(Optimizer* o) : opt_(o) {}

    void processLoop(StmtPtr& body) {
        auto* block = dynamic_cast<Block*>(body.get());
        if (!block) return;

        std::unordered_map<std::string, bool> modified;
        opt_->collectModifiedVars(body.get(), modified);

        
        
        
        
        for (auto& s : block->stmts) {
            if (auto* vd = dynamic_cast<VarDecl*>(s.get())) {
                if (vd->init && opt_->isLoopInvariant(vd->init.get(), modified)) {
                    opt_->result_.loopInvariantsMoved++;
                }
            }
        }
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr&) override {}
    void visit(UnaryExpr&) override {}
    void visit(AssignExpr&) override {}
    void visit(CallExpr&) override {}
    void visit(ArrayAccessExpr&) override {}
    void visit(IOExpr&) override {}
    void visit(ExprStmt&) override {}
    void visit(Block& s) override { for (auto& st : s.stmts) st->accept(*this); }
    void visit(VarDecl&) override {}
    void visit(IfStmt& s) override { s.thenBranch->accept(*this); if (s.elseBranch) s.elseBranch->accept(*this); }
    void visit(WhileStmt& s) override { processLoop(s.body); s.body->accept(*this); }
    void visit(ForStmt& s) override { processLoop(s.body); s.body->accept(*this); }
    void visit(ReturnStmt&) override {}
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override { if (f.body) f.body->accept(*this); }
    void visit(Program& p) override { for (auto& d : p.declarations) d->accept(*this); }
};

void Optimizer::loopInvariantCodeMotion(Program& program) {
    LICMVisitor v(this);
    program.accept(v);
}

static std::string exprKey(Expr* expr) {
    if (auto* lit = dynamic_cast<LiteralExpr*>(expr))
        return "L:" + lit->value;
    if (auto* v = dynamic_cast<VarExpr*>(expr))
        return "V:" + v->name;
    if (auto* b = dynamic_cast<BinaryExpr*>(expr))
        return "B:" + std::to_string((int)b->op) + "(" + exprKey(b->left.get()) + "," + exprKey(b->right.get()) + ")";
    if (auto* u = dynamic_cast<UnaryExpr*>(expr))
        return "U:" + std::to_string((int)u->op) + "(" + exprKey(u->operand.get()) + ")";
    return "?";
}

class CSEVisitor : public ASTVisitor {
    Optimizer* opt_;
    std::unordered_set<std::string> seen_;
public:
    CSEVisitor(Optimizer* o) : opt_(o) {}

    void checkExpr(Expr* expr) {
        if (!expr) return;
        auto* bin = dynamic_cast<BinaryExpr*>(expr);
        if (!bin) return;

        std::string key = exprKey(expr);
        if (key.size() > 5) { 
            if (seen_.count(key)) {
                opt_->result_.csesEliminated++;
            } else {
                seen_.insert(key);
            }
        }
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr& e) override { checkExpr(&e); e.left->accept(*this); e.right->accept(*this); }
    void visit(UnaryExpr& e) override { e.operand->accept(*this); }
    void visit(AssignExpr& e) override { e.value->accept(*this); checkExpr(e.value.get()); }
    void visit(CallExpr& e) override { for (auto& a : e.args) a->accept(*this); }
    void visit(ArrayAccessExpr& e) override { e.index->accept(*this); }
    void visit(IOExpr& e) override { for (auto& o : e.operands) o->accept(*this); }
    void visit(ExprStmt& s) override { s.expr->accept(*this); checkExpr(s.expr.get()); }
    void visit(Block& s) override {
        auto savedSeen = seen_;
        for (auto& st : s.stmts) st->accept(*this);
        seen_ = savedSeen;
    }
    void visit(VarDecl& s) override { if (s.init) { s.init->accept(*this); checkExpr(s.init.get()); } }
    void visit(IfStmt& s) override {
        s.condition->accept(*this);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override { s.condition->accept(*this); s.body->accept(*this); }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        if (s.condition) s.condition->accept(*this);
        if (s.update) s.update->accept(*this);
        s.body->accept(*this);
    }
    void visit(ReturnStmt& s) override { if (s.value) s.value->accept(*this); }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override {
        seen_.clear();
        if (f.body) f.body->accept(*this);
    }
    void visit(Program& p) override { for (auto& d : p.declarations) d->accept(*this); }
};

void Optimizer::commonSubexprElimination(Program& program) {
    CSEVisitor v(this);
    program.accept(v);
}

ExprPtr Optimizer::peepholeExpr(ExprPtr& expr) {
    auto* bin = dynamic_cast<BinaryExpr*>(expr.get());
    if (bin) {
        
        auto pl = peepholeExpr(bin->left);
        if (pl) bin->left = std::move(pl);
        auto pr = peepholeExpr(bin->right);
        if (pr) bin->right = std::move(pr);

        
        if (bin->op == TokenType::MINUS) {
            auto* lv = dynamic_cast<VarExpr*>(bin->left.get());
            auto* rv = dynamic_cast<VarExpr*>(bin->right.get());
            if (lv && rv && lv->name == rv->name) {
                result_.peepholeOptimizations++;
                return std::make_unique<LiteralExpr>(LiteralExpr::INT, "0", bin->line);
            }
        }

        
        if (bin->op == TokenType::SLASH) {
            auto* lv = dynamic_cast<VarExpr*>(bin->left.get());
            auto* rv = dynamic_cast<VarExpr*>(bin->right.get());
            if (lv && rv && lv->name == rv->name) {
                result_.peepholeOptimizations++;
                return std::make_unique<LiteralExpr>(LiteralExpr::INT, "1", bin->line);
            }
        }

        
        if (bin->op == TokenType::STAR) {
            auto* rightLit = dynamic_cast<LiteralExpr*>(bin->right.get());
            if (rightLit && rightLit->kind == LiteralExpr::INT && rightLit->value == "-1") {
                result_.peepholeOptimizations++;
                return std::make_unique<UnaryExpr>(TokenType::MINUS, std::move(bin->left), true, bin->line);
            }
            auto* leftLit = dynamic_cast<LiteralExpr*>(bin->left.get());
            if (leftLit && leftLit->kind == LiteralExpr::INT && leftLit->value == "-1") {
                result_.peepholeOptimizations++;
                return std::make_unique<UnaryExpr>(TokenType::MINUS, std::move(bin->right), true, bin->line);
            }
        }
    }

    
    auto* unary = dynamic_cast<UnaryExpr*>(expr.get());
    if (unary && unary->op == TokenType::BANG) {
        auto* inner = dynamic_cast<UnaryExpr*>(unary->operand.get());
        if (inner && inner->op == TokenType::BANG) {
            result_.peepholeOptimizations++;
            return std::move(inner->operand);
        }
    }

    return nullptr;
}

class PeepholeVisitor : public ASTVisitor {
    Optimizer* opt_;
public:
    PeepholeVisitor(Optimizer* o) : opt_(o) {}

    void peep(ExprPtr& expr) {
        auto r = opt_->peepholeExpr(expr);
        if (r) expr = std::move(r);
    }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr&) override {}
    void visit(BinaryExpr& e) override { e.left->accept(*this); e.right->accept(*this); }
    void visit(UnaryExpr& e) override { e.operand->accept(*this); }
    void visit(AssignExpr& e) override { peep(e.value); e.value->accept(*this); }
    void visit(CallExpr& e) override { for (auto& a : e.args) { peep(a); a->accept(*this); } }
    void visit(ArrayAccessExpr& e) override { peep(e.index); e.index->accept(*this); }
    void visit(IOExpr& e) override { for (auto& o : e.operands) { peep(o); o->accept(*this); } }
    void visit(ExprStmt& s) override { peep(s.expr); s.expr->accept(*this); }
    void visit(Block& s) override {
        
        auto it = s.stmts.begin();
        while (it != s.stmts.end()) {
            if (auto* ifStmt = dynamic_cast<IfStmt*>(it->get())) {
                auto* condLit = dynamic_cast<LiteralExpr*>(ifStmt->condition.get());
                if (condLit && condLit->kind == LiteralExpr::BOOL) {
                    if (condLit->value == "true") {
                        
                        *it = std::move(ifStmt->thenBranch);
                        opt_->result_.peepholeOptimizations++;
                    } else if (condLit->value == "false" && ifStmt->elseBranch) {
                        *it = std::move(ifStmt->elseBranch);
                        opt_->result_.peepholeOptimizations++;
                    } else if (condLit->value == "false") {
                        it = s.stmts.erase(it);
                        opt_->result_.peepholeOptimizations++;
                        continue;
                    }
                }
            }
            (*it)->accept(*this);
            ++it;
        }
    }
    void visit(VarDecl& s) override { if (s.init) { peep(s.init); s.init->accept(*this); } }
    void visit(IfStmt& s) override {
        peep(s.condition); s.condition->accept(*this);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override { peep(s.condition); s.condition->accept(*this); s.body->accept(*this); }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        if (s.condition) { peep(s.condition); s.condition->accept(*this); }
        if (s.update) { peep(s.update); s.update->accept(*this); }
        s.body->accept(*this);
    }
    void visit(ReturnStmt& s) override { if (s.value) { peep(s.value); s.value->accept(*this); } }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override { if (f.body) f.body->accept(*this); }
    void visit(Program& p) override { for (auto& d : p.declarations) d->accept(*this); }
};

void Optimizer::peepholeOptimization(Program& program) {
    PeepholeVisitor v(this);
    program.accept(v);
}

static ExprPtr cloneExpr(Expr* expr, const std::unordered_map<std::string, std::string>& renames) {
    if (!expr) return nullptr;

    if (auto* lit = dynamic_cast<LiteralExpr*>(expr))
        return std::make_unique<LiteralExpr>(lit->kind, lit->value, lit->line);

    if (auto* var = dynamic_cast<VarExpr*>(expr)) {
        auto it = renames.find(var->name);
        std::string name = (it != renames.end()) ? it->second : var->name;
        return std::make_unique<VarExpr>(name, var->line);
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(expr))
        return std::make_unique<BinaryExpr>(bin->op,
            cloneExpr(bin->left.get(), renames),
            cloneExpr(bin->right.get(), renames), bin->line);

    if (auto* un = dynamic_cast<UnaryExpr*>(expr))
        return std::make_unique<UnaryExpr>(un->op,
            cloneExpr(un->operand.get(), renames), un->prefix, un->line);

    if (auto* assign = dynamic_cast<AssignExpr*>(expr)) {
        auto it = renames.find(assign->target);
        std::string target = (it != renames.end()) ? it->second : assign->target;
        return std::make_unique<AssignExpr>(assign->op, target,
            cloneExpr(assign->value.get(), renames), assign->line);
    }

    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        std::vector<ExprPtr> args;
        for (auto& a : call->args) args.push_back(cloneExpr(a.get(), renames));
        return std::make_unique<CallExpr>(call->callee, std::move(args), call->line);
    }

    if (auto* arr = dynamic_cast<ArrayAccessExpr*>(expr)) {
        auto it = renames.find(arr->name);
        std::string name = (it != renames.end()) ? it->second : arr->name;
        return std::make_unique<ArrayAccessExpr>(name,
            cloneExpr(arr->index.get(), renames), arr->line);
    }

    if (auto* io = dynamic_cast<IOExpr*>(expr)) {
        std::vector<ExprPtr> ops;
        for (auto& o : io->operands) ops.push_back(cloneExpr(o.get(), renames));
        return std::make_unique<IOExpr>(io->isOutput, std::move(ops), io->line);
    }

    return nullptr;
}

static StmtPtr cloneStmt(Stmt* stmt, const std::unordered_map<std::string, std::string>& renames);

static StmtPtr cloneStmt(Stmt* stmt, const std::unordered_map<std::string, std::string>& renames) {
    if (!stmt) return nullptr;

    if (auto* es = dynamic_cast<ExprStmt*>(stmt))
        return std::make_unique<ExprStmt>(cloneExpr(es->expr.get(), renames), es->line);

    if (auto* vd = dynamic_cast<VarDecl*>(stmt)) {
        auto it = renames.find(vd->name);
        std::string name = (it != renames.end()) ? it->second : vd->name;
        return std::make_unique<VarDecl>(vd->typeName, name,
            vd->init ? cloneExpr(vd->init.get(), renames) : nullptr,
            vd->isConst, vd->arraySize, vd->line);
    }

    if (auto* blk = dynamic_cast<Block*>(stmt)) {
        std::vector<StmtPtr> stmts;
        for (auto& s : blk->stmts) stmts.push_back(cloneStmt(s.get(), renames));
        return std::make_unique<Block>(std::move(stmts), blk->line);
    }

    if (auto* ifs = dynamic_cast<IfStmt*>(stmt))
        return std::make_unique<IfStmt>(
            cloneExpr(ifs->condition.get(), renames),
            cloneStmt(ifs->thenBranch.get(), renames),
            ifs->elseBranch ? cloneStmt(ifs->elseBranch.get(), renames) : nullptr,
            ifs->line);

    if (auto* ws = dynamic_cast<WhileStmt*>(stmt))
        return std::make_unique<WhileStmt>(
            cloneExpr(ws->condition.get(), renames),
            cloneStmt(ws->body.get(), renames), ws->line);

    if (auto* fs = dynamic_cast<ForStmt*>(stmt))
        return std::make_unique<ForStmt>(
            fs->init ? cloneStmt(fs->init.get(), renames) : nullptr,
            fs->condition ? cloneExpr(fs->condition.get(), renames) : nullptr,
            fs->update ? cloneExpr(fs->update.get(), renames) : nullptr,
            cloneStmt(fs->body.get(), renames), fs->line);

    if (auto* rs = dynamic_cast<ReturnStmt*>(stmt))
        return std::make_unique<ReturnStmt>(
            rs->value ? cloneExpr(rs->value.get(), renames) : nullptr, rs->line);

    if (dynamic_cast<BreakStmt*>(stmt))
        return std::make_unique<BreakStmt>(stmt->line);

    if (dynamic_cast<ContinueStmt*>(stmt))
        return std::make_unique<ContinueStmt>(stmt->line);

    return nullptr;
}

static int inlineIdCounter = 0;

static bool isRecursive(const std::string& name, Block* body) {
    if (!body) return false;
    for (auto& s : body->stmts) {
        if (auto* es = dynamic_cast<ExprStmt*>(s.get())) {
            if (auto* call = dynamic_cast<CallExpr*>(es->expr.get())) {
                if (call->callee == name) return true;
            }
        }
        if (auto* vd = dynamic_cast<VarDecl*>(s.get())) {
            if (vd->init) {
                if (auto* call = dynamic_cast<CallExpr*>(vd->init.get())) {
                    if (call->callee == name) return true;
                }
            }
        }
        if (auto* ret = dynamic_cast<ReturnStmt*>(s.get())) {
            if (ret->value) {
                if (auto* call = dynamic_cast<CallExpr*>(ret->value.get())) {
                    if (call->callee == name) return true;
                }
            }
        }
    }
    return false;
}

void Optimizer::functionInlining(Program& program) {
    
    std::unordered_map<std::string, FunctionDecl*> candidates;

    for (auto& decl : program.declarations) {
        auto* fn = dynamic_cast<FunctionDecl*>(decl.get());
        if (!fn || fn->name == "main") continue;
        if (!fn->body) continue;
        
        if (fn->body->stmts.size() > 5) continue;
        if (isRecursive(fn->name, fn->body.get())) continue;
        candidates[fn->name] = fn;
    }

    if (candidates.empty()) return;

    
    for (auto& decl : program.declarations) {
        auto* fn = dynamic_cast<FunctionDecl*>(decl.get());
        if (!fn || !fn->body) continue;

        auto& stmts = fn->body->stmts;
        for (size_t i = 0; i < stmts.size(); i++) {
            
            auto* vd = dynamic_cast<VarDecl*>(stmts[i].get());
            if (!vd || !vd->init) continue;

            auto* call = dynamic_cast<CallExpr*>(vd->init.get());
            if (!call) continue;

            auto it = candidates.find(call->callee);
            if (it == candidates.end()) continue;

            FunctionDecl* target = it->second;
            if (call->args.size() != target->params.size()) continue;

            
            int id = inlineIdCounter++;
            std::string suffix = "_inl" + std::to_string(id);
            std::unordered_map<std::string, std::string> renames;

            
            std::vector<StmtPtr> inlinedStmts;

            for (size_t p = 0; p < target->params.size(); p++) {
                std::string newName = target->params[p].name + suffix;
                renames[target->params[p].name] = newName;
                inlinedStmts.push_back(std::make_unique<VarDecl>(
                    target->params[p].typeName, newName,
                    cloneExpr(call->args[p].get(), renames),
                    false, 0, call->line));
            }

            
            std::string resultVar = "_inline_result" + suffix;

            
            for (auto& bodyStmt : target->body->stmts) {
                if (auto* ret = dynamic_cast<ReturnStmt*>(bodyStmt.get())) {
                    if (ret->value) {
                        
                        inlinedStmts.push_back(std::make_unique<VarDecl>(
                            target->returnType, resultVar,
                            cloneExpr(ret->value.get(), renames),
                            false, 0, ret->line));
                    }
                } else {
                    inlinedStmts.push_back(cloneStmt(bodyStmt.get(), renames));
                }
            }

            
            vd->init = std::make_unique<VarExpr>(resultVar, call->line);

            
            for (size_t j = 0; j < inlinedStmts.size(); j++) {
                stmts.insert(stmts.begin() + i + j, std::move(inlinedStmts[j]));
            }
            i += inlinedStmts.size(); 

            result_.functionsInlined++;
        }
    }
}

class VarAccessCounterVisitor : public ASTVisitor {
    Optimizer* opt_;
    int loopMultiplier_ = 1;
    std::unordered_map<std::string, int> accessCounts_;

    static constexpr int LOOP_ITER_ESTIMATE = 100;
    static constexpr int HOT_THRESHOLD = 50;

public:
    VarAccessCounterVisitor(Optimizer* o) : opt_(o) {}

    const std::unordered_map<std::string, int>& getAccessCounts() const { return accessCounts_; }

    void visit(LiteralExpr&) override {}
    void visit(VarExpr& e) override { accessCounts_[e.name] += loopMultiplier_; }
    void visit(BinaryExpr& e) override { e.left->accept(*this); e.right->accept(*this); }
    void visit(UnaryExpr& e) override { e.operand->accept(*this); }
    void visit(AssignExpr& e) override {
        accessCounts_[e.target] += loopMultiplier_;
        e.value->accept(*this);
    }
    void visit(CallExpr& e) override { for (auto& a : e.args) a->accept(*this); }
    void visit(ArrayAccessExpr& e) override {
        accessCounts_[e.name] += loopMultiplier_;
        e.index->accept(*this);
    }
    void visit(IOExpr& e) override { for (auto& o : e.operands) o->accept(*this); }
    void visit(ExprStmt& s) override { s.expr->accept(*this); }
    void visit(Block& s) override { for (auto& st : s.stmts) st->accept(*this); }
    void visit(VarDecl& s) override { if (s.init) s.init->accept(*this); }
    void visit(IfStmt& s) override {
        s.condition->accept(*this);
        s.thenBranch->accept(*this);
        if (s.elseBranch) s.elseBranch->accept(*this);
    }
    void visit(WhileStmt& s) override {
        int prev = loopMultiplier_;
        loopMultiplier_ *= LOOP_ITER_ESTIMATE;
        s.condition->accept(*this);
        s.body->accept(*this);
        loopMultiplier_ = prev;
    }
    void visit(ForStmt& s) override {
        if (s.init) s.init->accept(*this);
        int prev = loopMultiplier_;
        loopMultiplier_ *= LOOP_ITER_ESTIMATE;
        if (s.condition) s.condition->accept(*this);
        if (s.update) s.update->accept(*this);
        s.body->accept(*this);
        loopMultiplier_ = prev;
    }
    void visit(ReturnStmt& s) override { if (s.value) s.value->accept(*this); }
    void visit(BreakStmt&) override {}
    void visit(ContinueStmt&) override {}
    void visit(FunctionDecl& f) override {
        if (f.body) f.body->accept(*this);
    }
    void visit(Program& p) override { for (auto& d : p.declarations) d->accept(*this); }

    void computeHotVars() {
        for (auto& [name, count] : accessCounts_) {
            if (count >= HOT_THRESHOLD) {
                opt_->hotVariables_.insert(name);
                opt_->result_.registerHintsEmitted++;
            }
        }
    }
};

void Optimizer::energyAwareRegisterHints(Program& program) {
    VarAccessCounterVisitor v(this);
    program.accept(v);
    v.computeHotVars();
}

} 
