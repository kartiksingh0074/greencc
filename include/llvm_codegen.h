#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>

#include "visitor.h"
#include "ast.h"

namespace greencc {

// ============================================================================
// OPTIMIZATION LEVEL ENUM
// ============================================================================
enum class OptLevel {
    O0,  // No optimization
    O1,  // Basic optimizations (Mem2Reg, InstCombine, CFGSimplify)
    O2,  // Standard optimizations (O1 + GVN, EarlyCSE, Reassociate)
    O3   // Aggressive optimizations (O2 + Loop/Vectorization passes)
};

// ============================================================================
// LLVM CODE GENERATION WITH LOW-LEVEL OPTIMIZATION PASSES
// ============================================================================
class LLVMCodeGen : public ASTVisitor {
public:
    LLVMCodeGen();
    explicit LLVMCodeGen(OptLevel optLevel);
    ~LLVMCodeGen();

    // Set optimization level
    void setOptLevel(OptLevel level) { optLevel_ = level; }
    OptLevel getOptLevel() const { return optLevel_; }

    // Generate LLVM IR (with optimizations)
    std::string generate(Program& program);

    // Get the LLVM module
    llvm::Module* getModule() { return module_.get(); }

    // Write LLVM IR to file
    bool writeIRToFile(const std::string& path);

    // Write object file
    bool writeObjectFile(const std::string& path);

    // Set hot variables for energy-aware optimization
    void setHotVariables(const std::unordered_set<std::string>& hotVars);

    // Get optimization statistics
    void printOptimizationStats() const;
    double getOptimizationTime() const { return optimizationTimeMs_; }

    // Visitor methods for AST traversal
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
    // LLVM objects
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module>      module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;

    // Compilation context
    llvm::Function* currentFunction_ = nullptr;
    llvm::Value*    lastValue_        = nullptr;

    // Symbol table
    std::unordered_map<std::string, llvm::AllocaInst*> namedValues_;

    // Control flow targets
    llvm::BasicBlock* breakTarget_    = nullptr;
    llvm::BasicBlock* continueTarget_ = nullptr;

    // I/O functions
    llvm::Function* printfFunc_  = nullptr;
    llvm::Function* putsFunc_    = nullptr;

    // String constants cache
    std::unordered_map<std::string, llvm::Value*> stringConstants_;
    llvm::Value* getOrCreateString(const std::string& str);

    // Hot variables for energy-aware optimization
    std::unordered_set<std::string> hotVariables_;

    // Optimization level
    OptLevel optLevel_ = OptLevel::O2;

    // Optimization statistics
    double optimizationTimeMs_ = 0.0;
    unsigned int passesExecuted_ = 0;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    // Type conversion
    llvm::Type* getLLVMType(const std::string& typeName);
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
                                              const std::string& name,
                                              llvm::Type* type);
    void declarePrintf();
    llvm::Value* createPrintfCall(const std::string& fmt,
                                   std::vector<llvm::Value*> args);

    // ========================================================================
    // LOW-LEVEL OPTIMIZATION PASS METHODS
    // ========================================================================

    // Main optimization orchestrator
    void runOptimizationPasses();
    
    // Individual pass registration methods
    void addMem2RegPass(llvm::legacy::PassManager& pm);
    void addInstructionCombiningPass(llvm::legacy::PassManager& pm);
    void addControlFlowSimplificationPass(llvm::legacy::PassManager& pm);
    void addValueNumberingPass(llvm::legacy::PassManager& pm);
    void addBasicPasses(llvm::legacy::PassManager& pm);
    void addStandardPasses(llvm::legacy::PassManager& pm);
    void addAggressivePasses(llvm::legacy::PassManager& pm);

    // Optimization pass utilities
    void printOptimizationHeader() const;
    void printOptimizationFooter() const;
};

} // namespace greencc
