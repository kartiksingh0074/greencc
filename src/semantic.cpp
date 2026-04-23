#include "semantic.h"
#include <sstream>

namespace greencc {

void SemanticAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::popScope() {
    scopes_.pop_back();
}

void SemanticAnalyzer::declare(const std::string& name, const std::string& type,
                                bool isConst, int line) {
    auto& scope = scopes_.back();
    if (scope.count(name)) {
        error(line, "redeclaration of '" + name + "'");
        return;
    }
    scope[name] = {type, isConst};
}

SemanticAnalyzer::VarInfo* SemanticAnalyzer::resolve(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

void SemanticAnalyzer::error(int line, const std::string& msg) {
    std::ostringstream os;
    os << "Semantic error [line " << line << "]: " << msg;
    errors_.push_back(os.str());
}

void SemanticAnalyzer::analyze(Program& program) {
    program.accept(*this);
}

void SemanticAnalyzer::visit(Program& p) {
    pushScope(); 

    
    for (auto& decl : p.declarations) {
        if (auto* fn = dynamic_cast<FunctionDecl*>(decl.get())) {
            functions_[fn->name] = {fn->returnType, (int)fn->params.size()};
        }
    }

    
    for (auto& decl : p.declarations) {
        decl->accept(*this);
    }

    popScope();
}

void SemanticAnalyzer::visit(FunctionDecl& f) {
    currentFunction_ = f.name;
    pushScope();

    for (auto& p : f.params) {
        declare(p.name, p.typeName, false, f.line);
    }

    if (f.body) f.body->accept(*this);
    popScope();
    currentFunction_.clear();
}

void SemanticAnalyzer::visit(Block& s) {
    pushScope();
    for (auto& stmt : s.stmts) {
        stmt->accept(*this);
    }
    popScope();
}

void SemanticAnalyzer::visit(VarDecl& s) {
    if (s.init) s.init->accept(*this);
    declare(s.name, s.typeName, s.isConst, s.line);
}

void SemanticAnalyzer::visit(IfStmt& s) {
    s.condition->accept(*this);
    s.thenBranch->accept(*this);
    if (s.elseBranch) s.elseBranch->accept(*this);
}

void SemanticAnalyzer::visit(WhileStmt& s) {
    s.condition->accept(*this);
    loopDepth_++;
    s.body->accept(*this);
    loopDepth_--;
}

void SemanticAnalyzer::visit(ForStmt& s) {
    pushScope();
    if (s.init) s.init->accept(*this);
    if (s.condition) s.condition->accept(*this);
    if (s.update) s.update->accept(*this);
    loopDepth_++;
    s.body->accept(*this);
    loopDepth_--;
    popScope();
}

void SemanticAnalyzer::visit(ReturnStmt& s) {
    if (s.value) s.value->accept(*this);
}

void SemanticAnalyzer::visit(BreakStmt& s) {
    if (loopDepth_ == 0) error(s.line, "'break' outside of loop");
}

void SemanticAnalyzer::visit(ContinueStmt& s) {
    if (loopDepth_ == 0) error(s.line, "'continue' outside of loop");
}

void SemanticAnalyzer::visit(ExprStmt& s) {
    s.expr->accept(*this);
}

void SemanticAnalyzer::visit(LiteralExpr&) {  }

void SemanticAnalyzer::visit(VarExpr& e) {
    if (e.name == "endl") return; 
    if (!resolve(e.name)) {
        error(e.line, "use of undeclared variable '" + e.name + "'");
    }
}

void SemanticAnalyzer::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);
}

void SemanticAnalyzer::visit(UnaryExpr& e) {
    e.operand->accept(*this);
}

void SemanticAnalyzer::visit(AssignExpr& e) {
    auto* info = resolve(e.target);
    if (!info) {
        error(e.line, "assignment to undeclared variable '" + e.target + "'");
    } else if (info->isConst) {
        error(e.line, "assignment to const variable '" + e.target + "'");
    }
    e.value->accept(*this);
}

void SemanticAnalyzer::visit(CallExpr& e) {
    auto it = functions_.find(e.callee);
    if (it == functions_.end()) {
        error(e.line, "call to undefined function '" + e.callee + "'");
    } else if ((int)e.args.size() != it->second.arity) {
        error(e.line, "function '" + e.callee + "' expects " +
              std::to_string(it->second.arity) + " arguments, got " +
              std::to_string(e.args.size()));
    }
    for (auto& arg : e.args) arg->accept(*this);
}

void SemanticAnalyzer::visit(ArrayAccessExpr& e) {
    if (!resolve(e.name)) {
        error(e.line, "use of undeclared array '" + e.name + "'");
    }
    e.index->accept(*this);
}

void SemanticAnalyzer::visit(IOExpr& e) {
    for (auto& op : e.operands) op->accept(*this);
}

void LiteralExpr::accept(ASTVisitor& v)     { v.visit(*this); }
void VarExpr::accept(ASTVisitor& v)         { v.visit(*this); }
void BinaryExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void UnaryExpr::accept(ASTVisitor& v)       { v.visit(*this); }
void AssignExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void CallExpr::accept(ASTVisitor& v)        { v.visit(*this); }
void ArrayAccessExpr::accept(ASTVisitor& v) { v.visit(*this); }
void IOExpr::accept(ASTVisitor& v)          { v.visit(*this); }
void ExprStmt::accept(ASTVisitor& v)        { v.visit(*this); }
void Block::accept(ASTVisitor& v)           { v.visit(*this); }
void VarDecl::accept(ASTVisitor& v)         { v.visit(*this); }
void IfStmt::accept(ASTVisitor& v)          { v.visit(*this); }
void WhileStmt::accept(ASTVisitor& v)       { v.visit(*this); }
void ForStmt::accept(ASTVisitor& v)         { v.visit(*this); }
void ReturnStmt::accept(ASTVisitor& v)      { v.visit(*this); }
void BreakStmt::accept(ASTVisitor& v)       { v.visit(*this); }
void ContinueStmt::accept(ASTVisitor& v)    { v.visit(*this); }
void FunctionDecl::accept(ASTVisitor& v)    { v.visit(*this); }
void Program::accept(ASTVisitor& v)         { v.visit(*this); }

} 
