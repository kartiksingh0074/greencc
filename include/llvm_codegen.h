#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include "visitor.h"
#include "ast.h"

namespace greencc {

class LLVMCodeGen : public ASTVisitor {
public:
    LLVMCodeGen();
    ~LLVMCodeGen();

    
    std::string generate(Program& program);

    
    llvm::Module* getModule() { return module_.get(); }

    
    bool writeIRToFile(const std::string& path);

    
    bool writeObjectFile(const std::string& path);

    
    void setHotVariables(const std::unordered_set<std::string>& hotVars);

    
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
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module>      module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;

    
    llvm::Function* currentFunction_ = nullptr;
    llvm::Value*    lastValue_        = nullptr; 

    
    std::unordered_map<std::string, llvm::AllocaInst*> namedValues_;

    
    llvm::BasicBlock* breakTarget_    = nullptr;
    llvm::BasicBlock* continueTarget_ = nullptr;

    
    llvm::Function* printfFunc_  = nullptr;
    llvm::Function* putsFunc_    = nullptr;

    
    llvm::Type* getLLVMType(const std::string& typeName);
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
                                              const std::string& name,
                                              llvm::Type* type);
    void declarePrintf();
    llvm::Value* createPrintfCall(const std::string& fmt,
                                   std::vector<llvm::Value*> args);

    
    std::unordered_map<std::string, llvm::Value*> stringConstants_;
    llvm::Value* getOrCreateString(const std::string& str);

    
    std::unordered_set<std::string> hotVariables_;
};

} 
