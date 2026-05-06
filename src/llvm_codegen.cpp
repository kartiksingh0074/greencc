#include "llvm_codegen.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

// ============================================================================
// LLVM OPTIMIZATION PASS HEADERS
// ============================================================================
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/InstCombine.h>
#include <llvm/Transforms/IPO.h>

#include <iostream>
#include <sstream>
#include <iomanip>

namespace greencc {

// ============================================================================
// CONSTRUCTOR AND DESTRUCTOR
// ============================================================================

LLVMCodeGen::LLVMCodeGen()
    : context_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>("greencc_module", *context_)),
      builder_(std::make_unique<llvm::IRBuilder<>>(*context_)),
      optLevel_(OptLevel::O2) {}

LLVMCodeGen::LLVMCodeGen(OptLevel optLevel)
    : context_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>("greencc_module", *context_)),
      builder_(std::make_unique<llvm::IRBuilder<>>(*context_)),
      optLevel_(optLevel) {}

LLVMCodeGen::~LLVMCodeGen() = default;

// ============================================================================
// TYPE AND HELPER METHODS
// ============================================================================

llvm::Type* LLVMCodeGen::getLLVMType(const std::string& typeName) {
    std::string t = typeName;
    
    if (t.substr(0, 6) == "const ") t = t.substr(6);

    if (t == "int")    return llvm::Type::getInt32Ty(*context_);
    if (t == "float")  return llvm::Type::getFloatTy(*context_);
    if (t == "double") return llvm::Type::getDoubleTy(*context_);
    if (t == "char")   return llvm::Type::getInt8Ty(*context_);
    if (t == "bool")   return llvm::Type::getInt1Ty(*context_);
    if (t == "void")   return llvm::Type::getVoidTy(*context_);
    if (t == "string") return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));

    return llvm::Type::getInt32Ty(*context_);
}

llvm::AllocaInst* LLVMCodeGen::createEntryBlockAlloca(
        llvm::Function* fn, const std::string& name, llvm::Type* type) {
    llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(),
                                  fn->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

void LLVMCodeGen::declarePrintf() {
    if (printfFunc_) return;

    auto* charPtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* printfTy = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context_), {charPtrTy}, true );
    printfFunc_ = llvm::Function::Create(
        printfTy, llvm::Function::ExternalLinkage, "printf", module_.get());
}

llvm::Value* LLVMCodeGen::getOrCreateString(const std::string& str) {
    auto it = stringConstants_.find(str);
    if (it != stringConstants_.end()) return it->second;

    auto* strConst = builder_->CreateGlobalStringPtr(str);
    stringConstants_[str] = strConst;
    return strConst;
}

llvm::Value* LLVMCodeGen::createPrintfCall(const std::string& fmt,
                                            std::vector<llvm::Value*> args) {
    declarePrintf();
    auto* fmtStr = getOrCreateString(fmt);
    args.insert(args.begin(), fmtStr);
    return builder_->CreateCall(printfFunc_, args);
}

// ============================================================================
// MAIN CODE GENERATION ENTRY POINT
// ============================================================================

std::string LLVMCodeGen::generate(Program& program) {
    program.accept(*this);

    // Run LLVM IR-level optimization passes
    runOptimizationPasses();

    std::string irStr;
    llvm::raw_string_ostream os(irStr);
    module_->print(os, nullptr);
    return irStr;
}

bool LLVMCodeGen::writeIRToFile(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream file(path, ec);
    if (ec) {
        std::cerr << "Error opening file '" << path << "': " << ec.message() << "\n";
        return false;
    }
    module_->print(file, nullptr);
    return true;
}

bool LLVMCodeGen::writeObjectFile(const std::string& path) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    module_->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        std::cerr << "Target lookup failed: " << error << "\n";
        return false;
    }

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto tm = target->createTargetMachine(targetTriple, cpu, features, opt,
                                           llvm::Reloc::PIC_);
    module_->setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Error opening file '" << path << "': " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "Target machine cannot emit object file\n";
        return false;
    }

    pass.run(*module_);
    dest.flush();
    return true;
}

void LLVMCodeGen::setHotVariables(const std::unordered_set<std::string>& hotVars) {
    hotVariables_ = hotVars;
}

// ============================================================================
// LOW-LEVEL LLVM OPTIMIZATION PASSES
// ============================================================================

void LLVMCodeGen::printOptimizationHeader() const {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║        ⚡ LLVM LOW-LEVEL OPTIMIZATION PASSES ⚡           ║\n";
    std::cout << "║                                                            ║\n";
    
    switch (optLevel_) {
        case OptLevel::O0:
            std::cout << "║  Optimization Level: O0 (No optimization)               ║\n";
            break;
        case OptLevel::O1:
            std::cout << "║  Optimization Level: O1 (Basic optimizations)          ║\n";
            break;
        case OptLevel::O2:
            std::cout << "║  Optimization Level: O2 (Standard optimizations)       ║\n";
            break;
        case OptLevel::O3:
            std::cout << "║  Optimization Level: O3 (Aggressive optimizations)     ║\n";
            break;
    }
    
    std::cout << "║                                                            ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
}

void LLVMCodeGen::printOptimizationFooter() const {
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  ✓ Optimization completed!                                 ║\n";
    std::cout << "║  Passes executed: " << std::setw(2) << passesExecuted_ 
              << "                                           ║\n";
    std::cout << "║  Time taken: " << std::fixed << std::setprecision(2) 
              << std::setw(6) << optimizationTimeMs_ << " ms                               ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

void LLVMCodeGen::runOptimizationPasses() {
    // Skip optimization at O0
    if (optLevel_ == OptLevel::O0) {
        std::cout << "\n║  O0 Selected: Skipping optimizations\n\n";
        return;
    }

    printOptimizationHeader();

    auto startTime = std::chrono::high_resolution_clock::now();

    llvm::legacy::PassManager pm;

    // Select passes based on optimization level
    switch (optLevel_) {
        case OptLevel::O1:
            addBasicPasses(pm);
            break;
        case OptLevel::O2:
            addStandardPasses(pm);
            break;
        case OptLevel::O3:
            addAggressivePasses(pm);
            break;
        default:
            addStandardPasses(pm);
            break;
    }

    // Run all passes
    pm.run(*module_);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    optimizationTimeMs_ = duration.count();

    printOptimizationFooter();
}

// ============================================================================
// PASS REGISTRATION METHODS
// ============================================================================

void LLVMCodeGen::addMem2RegPass(llvm::legacy::PassManager& pm) {
    std::cout << "║  [1/5] Mem2Reg (Promote Memory to Register)               ║\n";
    pm.add(llvm::createPromoteMemoryToRegisterPass());
    passesExecuted_++;
}

void LLVMCodeGen::addInstructionCombiningPass(llvm::legacy::PassManager& pm) {
    std::cout << "║  [2/5] InstCombine (Instruction Pattern Matching)         ║\n";
    pm.add(llvm::createInstructionCombiningPass());
    passesExecuted_++;
}

void LLVMCodeGen::addValueNumberingPass(llvm::legacy::PassManager& pm) {
    std::cout << "║  [3/5] GVN (Global Value Numbering)                       ║\n";
    pm.add(llvm::createGVNPass());
    passesExecuted_++;
}

void LLVMCodeGen::addControlFlowSimplificationPass(llvm::legacy::PassManager& pm) {
    std::cout << "║  [4/5] CFGSimplify (Control Flow Optimization)            ║\n";
    pm.add(llvm::createCFGSimplificationPass());
    passesExecuted_++;
}

// ============================================================================
// OPTIMIZATION LEVEL STRATEGIES
// ============================================================================

void LLVMCodeGen::addBasicPasses(llvm::legacy::PassManager& pm) {
    std::cout << "║                                                            ║\n";
    std::cout << "║  Basic Optimizations (O1):                                 ║\n";
    std::cout << "║  ────────────────────────────────────────────────────────  ║\n";
    addMem2RegPass(pm);
    addInstructionCombiningPass(pm);
    addControlFlowSimplificationPass(pm);
}

void LLVMCodeGen::addStandardPasses(llvm::legacy::PassManager& pm) {
    std::cout << "║                                                            ║\n";
    std::cout << "║  Standard Optimizations (O2):                              ║\n";
    std::cout << "║  ────────────────────────────────────────────────────────  ║\n";
    addMem2RegPass(pm);
    addInstructionCombiningPass(pm);
    addValueNumberingPass(pm);
    addControlFlowSimplificationPass(pm);
    
    std::cout << "║  [5/5] Advanced Passes                                     ║\n";
    pm.add(llvm::createEarlyCSEPass());
    pm.add(llvm::createReassociatePass());
    pm.add(llvm::createDeadCodeEliminationPass());
    pm.add(llvm::createCorrelatedValuePropagationPass());
    passesExecuted_ += 4;
}

void LLVMCodeGen::addAggressivePasses(llvm::legacy::PassManager& pm) {
    std::cout << "║                                                            ║\n";
    std::cout << "║  Aggressive Optimizations (O3):                            ║\n";
    std::cout << "║  ────────────────────────────────────────────────────────  ║\n";
    
    // Core passes
    addMem2RegPass(pm);
    addInstructionCombiningPass(pm);
    addValueNumberingPass(pm);
    addControlFlowSimplificationPass(pm);
    
    // Advanced passes
    std::cout << "║  [5/5] Advanced Passes:                                    ║\n";
    pm.add(llvm::createEarlyCSEPass());
    pm.add(llvm::createReassociatePass());
    pm.add(llvm::createDeadCodeEliminationPass());
    pm.add(llvm::createCorrelatedValuePropagationPass());
    passesExecuted_ += 4;
    
    // Loop optimizations
    std::cout << "║  [6/6] Loop Optimizations:                                 ║\n";
    pm.add(llvm::createLoopSimplifyPass());
    pm.add(llvm::createLICMPass());
    pm.add(llvm::createLoopUnrollPass());
    passesExecuted_ += 3;
    
    // Vectorization
    std::cout << "║  [7/7] Vectorization:                                      ║\n";
    pm.add(llvm::createSLPVectorizerPass());
    passesExecuted_++;
}

void LLVMCodeGen::printOptimizationStats() const {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          🔧 OPTIMIZATION STATISTICS 🔧                    ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Optimization Level: ";
    switch (optLevel_) {
        case OptLevel::O0: std::cout << "O0 (No optimization)"; break;
        case OptLevel::O1: std::cout << "O1 (Basic)"; break;
        case OptLevel::O2: std::cout << "O2 (Standard)"; break;
        case OptLevel::O3: std::cout << "O3 (Aggressive)"; break;
    }
    std::cout << "                 ║\n";
    std::cout << "║  Passes Executed: " << std::setw(2) << passesExecuted_ 
              << "                                       ║\n";
    std::cout << "║  Optimization Time: " << std::fixed << std::setprecision(2) 
              << std::setw(6) << optimizationTimeMs_ << " ms                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// VISITOR METHODS (AST TRAVERSAL FOR IR GENERATION)
// ============================================================================

void LLVMCodeGen::visit(Program& p) {
    module_->setModuleIdentifier("GreenCC Energy-Optimized Module");

    // First pass: declare all functions
    for (auto& decl : p.declarations) {
        if (auto* fn = dynamic_cast<FunctionDecl*>(decl.get())) {
            std::vector<llvm::Type*> paramTypes;
            for (auto& param : fn->params) {
                paramTypes.push_back(getLLVMType(param.typeName));
            }
            auto* retType = getLLVMType(fn->returnType);
            auto* fnType = llvm::FunctionType::get(retType, paramTypes, false);
            llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                    fn->name, module_.get());
        }
    }

    // Second pass: implement functions
    for (auto& decl : p.declarations) {
        decl->accept(*this);
    }
}

void LLVMCodeGen::visit(FunctionDecl& f) {
    llvm::Function* fn = module_->getFunction(f.name);
    if (!fn) return;

    currentFunction_ = fn;

    auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entryBB);

    auto savedValues = namedValues_;
    namedValues_.clear();

    unsigned i = 0;
    for (auto& arg : fn->args()) {
        arg.setName(f.params[i].name);
        auto* alloca = createEntryBlockAlloca(fn, f.params[i].name, arg.getType());
        builder_->CreateStore(&arg, alloca);
        namedValues_[f.params[i].name] = alloca;
        i++;
    }

    if (f.body) {
        for (auto& stmt : f.body->stmts) {
            stmt->accept(*this);
            if (builder_->GetInsertBlock()->getTerminator()) break;
        }
    }

    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (fn->getReturnType()->isVoidTy()) {
            builder_->CreateRetVoid();
        } else {
            builder_->CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
        }
    }

    if (llvm::verifyFunction(*fn, &llvm::errs())) {
        std::cerr << "Warning: function '" << f.name << "' verification failed\n";
    }

    namedValues_ = savedValues;
    currentFunction_ = nullptr;
}

void LLVMCodeGen::visit(Block& s) {
    for (auto& stmt : s.stmts) {
        stmt->accept(*this);
        if (builder_->GetInsertBlock()->getTerminator()) break;
    }
}

void LLVMCodeGen::visit(ExprStmt& s) {
    s.expr->accept(*this);
}

void LLVMCodeGen::visit(VarDecl& s) {
    llvm::Type* type = getLLVMType(s.typeName);

    if (s.arraySize > 0) {
        auto* arrType = llvm::ArrayType::get(type, s.arraySize);
        auto* alloca = createEntryBlockAlloca(currentFunction_, s.name, arrType);
        namedValues_[s.name] = alloca;
    } else {
        auto* alloca = createEntryBlockAlloca(currentFunction_, s.name, type);
        namedValues_[s.name] = alloca;

        // Apply energy-aware register optimization hints
        if (hotVariables_.count(s.name)) {
            auto* mdNode = llvm::MDNode::get(*context_, {
                llvm::ConstantAsMetadata::get(
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1))
            });
            alloca->setMetadata("greencc.regpriority", mdNode);
        }

        if (s.init) {
            s.init->accept(*this);
            if (lastValue_) {
                if (lastValue_->getType() != type) {
                    if (type->isIntegerTy() && lastValue_->getType()->isIntegerTy()) {
                        lastValue_ = builder_->CreateIntCast(lastValue_, type, true);
                    }
                }
                builder_->CreateStore(lastValue_, alloca);
            }
        }
    }
}

void LLVMCodeGen::visit(IfStmt& s) {
    s.condition->accept(*this);
    llvm::Value* condV = lastValue_;

    if (!condV->getType()->isIntegerTy(1)) {
        condV = builder_->CreateICmpNE(
            condV, llvm::ConstantInt::get(condV->getType(), 0), "ifcond");
    }

    auto* fn = builder_->GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(*context_, "then", fn);
    auto* elseBB = llvm::BasicBlock::Create(*context_, "else");
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "ifcont");

    if (s.elseBranch) {
        builder_->CreateCondBr(condV, thenBB, elseBB);
    } else {
        builder_->CreateCondBr(condV, thenBB, mergeBB);
    }

    builder_->SetInsertPoint(thenBB);
    s.thenBranch->accept(*this);
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(mergeBB);

    if (s.elseBranch) {
        fn->insert(fn->end(), elseBB);
        builder_->SetInsertPoint(elseBB);
        s.elseBranch->accept(*this);
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(mergeBB);
    }

    fn->insert(fn->end(), mergeBB);
    builder_->SetInsertPoint(mergeBB);
}

void LLVMCodeGen::visit(WhileStmt& s) {
    auto* fn = builder_->GetInsertBlock()->getParent();
    auto* condBB = llvm::BasicBlock::Create(*context_, "while.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "while.body");
    auto* exitBB = llvm::BasicBlock::Create(*context_, "while.exit");

    auto* savedBreak = breakTarget_;
    auto* savedCont = continueTarget_;
    breakTarget_ = exitBB;
    continueTarget_ = condBB;

    builder_->CreateBr(condBB);

    builder_->SetInsertPoint(condBB);
    s.condition->accept(*this);
    llvm::Value* condV = lastValue_;
    if (!condV->getType()->isIntegerTy(1)) {
        condV = builder_->CreateICmpNE(
            condV, llvm::ConstantInt::get(condV->getType(), 0), "whilecond");
    }
    builder_->CreateCondBr(condV, bodyBB, exitBB);

    fn->insert(fn->end(), bodyBB);
    builder_->SetInsertPoint(bodyBB);
    s.body->accept(*this);
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(condBB);

    fn->insert(fn->end(), exitBB);
    builder_->SetInsertPoint(exitBB);

    breakTarget_ = savedBreak;
    continueTarget_ = savedCont;
}

void LLVMCodeGen::visit(ForStmt& s) {
    auto* fn = builder_->GetInsertBlock()->getParent();

    if (s.init) s.init->accept(*this);

    auto* condBB = llvm::BasicBlock::Create(*context_, "for.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "for.body");
    auto* incBB  = llvm::BasicBlock::Create(*context_, "for.inc");
    auto* exitBB = llvm::BasicBlock::Create(*context_, "for.exit");

    auto* savedBreak = breakTarget_;
    auto* savedCont = continueTarget_;
    breakTarget_ = exitBB;
    continueTarget_ = incBB;

    builder_->CreateBr(condBB);

    builder_->SetInsertPoint(condBB);
    if (s.condition) {
        s.condition->accept(*this);
        llvm::Value* condV = lastValue_;
        if (!condV->getType()->isIntegerTy(1)) {
            condV = builder_->CreateICmpNE(
                condV, llvm::ConstantInt::get(condV->getType(), 0), "forcond");
        }
        builder_->CreateCondBr(condV, bodyBB, exitBB);
    } else {
        builder_->CreateBr(bodyBB);
    }

    fn->insert(fn->end(), bodyBB);
    builder_->SetInsertPoint(bodyBB);
    s.body->accept(*this);
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(incBB);

    fn->insert(fn->end(), incBB);
    builder_->SetInsertPoint(incBB);
    if (s.update) s.update->accept(*this);
    builder_->CreateBr(condBB);

    fn->insert(fn->end(), exitBB);
    builder_->SetInsertPoint(exitBB);

    breakTarget_ = savedBreak;
    continueTarget_ = savedCont;
}

void LLVMCodeGen::visit(ReturnStmt& s) {
    if (s.value) {
        s.value->accept(*this);
        if (lastValue_) {
            auto* retType = currentFunction_->getReturnType();
            llvm::Value* retVal = lastValue_;
            if (retVal->getType() != retType) {
                if (retType->isIntegerTy() && retVal->getType()->isIntegerTy()) {
                    retVal = builder_->CreateIntCast(retVal, retType, true);
                }
            }
            builder_->CreateRet(retVal);
        } else {
            builder_->CreateRetVoid();
        }
    } else {
        builder_->CreateRetVoid();
    }
}

void LLVMCodeGen::visit(BreakStmt&) {
    if (breakTarget_)
        builder_->CreateBr(breakTarget_);
}

void LLVMCodeGen::visit(ContinueStmt&) {
    if (continueTarget_)
        builder_->CreateBr(continueTarget_);
}

void LLVMCodeGen::visit(LiteralExpr& e) {
    switch (e.kind) {
        case LiteralExpr::INT:
            lastValue_ = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(*context_), std::stoll(e.value), true);
            break;
        case LiteralExpr::FLOAT: {
            std::string val = e.value;
            if (val.back() == 'f' || val.back() == 'F') val.pop_back();
            lastValue_ = llvm::ConstantFP::get(
                llvm::Type::getDoubleTy(*context_), val);
            break;
        }
        case LiteralExpr::BOOL:
            lastValue_ = llvm::ConstantInt::get(
                llvm::Type::getInt1Ty(*context_), e.value == "true" ? 1 : 0);
            break;
        case LiteralExpr::CHAR: {
            char c = e.value.length() >= 3 ? e.value[1] : '\0';
            if (c == '\\' && e.value.length() >= 4) {
                switch (e.value[2]) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case '0': c = '\0'; break;
                    case '\\': c = '\\'; break;
                    default: c = e.value[2];
                }
            }
            lastValue_ = llvm::ConstantInt::get(
                llvm::Type::getInt8Ty(*context_), c);
            break;
        }
        case LiteralExpr::STRING: {
            std::string str = e.value.substr(1, e.value.size() - 2);
            lastValue_ = getOrCreateString(str);
            break;
        }
    }
}

void LLVMCodeGen::visit(VarExpr& e) {
    auto it = namedValues_.find(e.name);
    if (it != namedValues_.end()) {
        auto* alloca = it->second;
        lastValue_ = builder_->CreateLoad(alloca->getAllocatedType(), alloca, e.name);
    } else {
        lastValue_ = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
    }
}

void LLVMCodeGen::visit(BinaryExpr& e) {
    e.left->accept(*this);
    llvm::Value* L = lastValue_;
    e.right->accept(*this);
    llvm::Value* R = lastValue_;

    if (!L || !R) { lastValue_ = nullptr; return; }

    if (L->getType() != R->getType()) {
        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
            if (L->getType()->getIntegerBitWidth() < R->getType()->getIntegerBitWidth())
                L = builder_->CreateIntCast(L, R->getType(), true);
            else
                R = builder_->CreateIntCast(R, L->getType(), true);
        }
    }

    bool isFloat = L->getType()->isFloatingPointTy();

    switch (e.op) {
        case TokenType::PLUS:
            lastValue_ = isFloat ? builder_->CreateFAdd(L, R, "addtmp")
                                  : builder_->CreateAdd(L, R, "addtmp");
            break;
        case TokenType::MINUS:
            lastValue_ = isFloat ? builder_->CreateFSub(L, R, "subtmp")
                                  : builder_->CreateSub(L, R, "subtmp");
            break;
        case TokenType::STAR:
            lastValue_ = isFloat ? builder_->CreateFMul(L, R, "multmp")
                                  : builder_->CreateMul(L, R, "multmp");
            break;
        case TokenType::SLASH:
            lastValue_ = isFloat ? builder_->CreateFDiv(L, R, "divtmp")
                                  : builder_->CreateSDiv(L, R, "divtmp");
            break;
        case TokenType::PERCENT:
            lastValue_ = builder_->CreateSRem(L, R, "modtmp");
            break;
        case TokenType::LSHIFT:
            lastValue_ = builder_->CreateShl(L, R, "shltmp");
            break;
        case TokenType::RSHIFT:
            lastValue_ = builder_->CreateAShr(L, R, "shrtmp");
            break;
        case TokenType::AMPERSAND:
            lastValue_ = builder_->CreateAnd(L, R, "andtmp");
            break;
        case TokenType::PIPE:
            lastValue_ = builder_->CreateOr(L, R, "ortmp");
            break;
        case TokenType::CARET:
            lastValue_ = builder_->CreateXor(L, R, "xortmp");
            break;
        case TokenType::EQ_EQ:
            lastValue_ = isFloat ? builder_->CreateFCmpOEQ(L, R, "eqtmp")
                                  : builder_->CreateICmpEQ(L, R, "eqtmp");
            break;
        case TokenType::BANG_EQ:
            lastValue_ = isFloat ? builder_->CreateFCmpONE(L, R, "netmp")
                                  : builder_->CreateICmpNE(L, R, "netmp");
            break;
        case TokenType::LT:
            lastValue_ = isFloat ? builder_->CreateFCmpOLT(L, R, "lttmp")
                                  : builder_->CreateICmpSLT(L, R, "lttmp");
            break;
        case TokenType::GT:
            lastValue_ = isFloat ? builder_->CreateFCmpOGT(L, R, "gttmp")
                                  : builder_->CreateICmpSGT(L, R, "gttmp");
            break;
        case TokenType::LT_EQ:
            lastValue_ = isFloat ? builder_->CreateFCmpOLE(L, R, "letmp")
                                  : builder_->CreateICmpSLE(L, R, "letmp");
            break;
        case TokenType::GT_EQ:
            lastValue_ = isFloat ? builder_->CreateFCmpOGE(L, R, "getmp")
                                  : builder_->CreateICmpSGE(L, R, "getmp");
            break;
        case TokenType::AND_AND: {
            L = builder_->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));
            R = builder_->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
            lastValue_ = builder_->CreateAnd(L, R, "andtmp");
            break;
        }
        case TokenType::OR_OR: {
            L = builder_->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));
            R = builder_->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
            lastValue_ = builder_->CreateOr(L, R, "ortmp");
            break;
        }
        default:
            lastValue_ = nullptr;
            break;
    }
}

void LLVMCodeGen::visit(UnaryExpr& e) {
    e.operand->accept(*this);
    llvm::Value* V = lastValue_;
    if (!V) return;

    switch (e.op) {
        case TokenType::MINUS:
            if (V->getType()->isFloatingPointTy())
                lastValue_ = builder_->CreateFNeg(V, "negtmp");
            else
                lastValue_ = builder_->CreateNeg(V, "negtmp");
            break;
        case TokenType::BANG:
            if (!V->getType()->isIntegerTy(1))
                V = builder_->CreateICmpNE(V, llvm::ConstantInt::get(V->getType(), 0));
            lastValue_ = builder_->CreateNot(V, "nottmp");
            break;
        case TokenType::TILDE:
            lastValue_ = builder_->CreateNot(V, "bnottmp");
            break;
        case TokenType::PLUS_PLUS: {
            auto* operandVar = dynamic_cast<VarExpr*>(e.operand.get());
            if (operandVar) {
                auto it = namedValues_.find(operandVar->name);
                if (it != namedValues_.end()) {
                    auto* one = llvm::ConstantInt::get(V->getType(), 1);
                    auto* newVal = builder_->CreateAdd(V, one, "inctmp");
                    builder_->CreateStore(newVal, it->second);
                    lastValue_ = e.prefix ? newVal : V;
                }
            }
            break;
        }
        case TokenType::MINUS_MINUS: {
            auto* operandVar = dynamic_cast<VarExpr*>(e.operand.get());
            if (operandVar) {
                auto it = namedValues_.find(operandVar->name);
                if (it != namedValues_.end()) {
                    auto* one = llvm::ConstantInt::get(V->getType(), 1);
                    auto* newVal = builder_->CreateSub(V, one, "dectmp");
                    builder_->CreateStore(newVal, it->second);
                    lastValue_ = e.prefix ? newVal : V;
                }
            }
            break;
        }
        default:
            break;
    }
}

void LLVMCodeGen::visit(AssignExpr& e) {
    e.value->accept(*this);
    llvm::Value* val = lastValue_;
    if (!val) return;

    auto it = namedValues_.find(e.target);
    if (it == namedValues_.end()) return;

    auto* alloca = it->second;
    auto* allocaType = alloca->getAllocatedType();

    if (e.op != TokenType::EQ) {
        auto* current = builder_->CreateLoad(allocaType, alloca, e.target);
        if (val->getType() != current->getType() &&
            val->getType()->isIntegerTy() && current->getType()->isIntegerTy()) {
            val = builder_->CreateIntCast(val, current->getType(), true);
        }
        switch (e.op) {
            case TokenType::PLUS_EQ:  val = builder_->CreateAdd(current, val); break;
            case TokenType::MINUS_EQ: val = builder_->CreateSub(current, val); break;
            case TokenType::STAR_EQ:  val = builder_->CreateMul(current, val); break;
            case TokenType::SLASH_EQ: val = builder_->CreateSDiv(current, val); break;
            default: break;
        }
    }

    if (val->getType() != allocaType) {
        if (allocaType->isIntegerTy() && val->getType()->isIntegerTy()) {
            val = builder_->CreateIntCast(val, allocaType, true);
        }
    }

    builder_->CreateStore(val, alloca);
    lastValue_ = val;
}

void LLVMCodeGen::visit(CallExpr& e) {
    llvm::Function* callee = module_->getFunction(e.callee);
    if (!callee) {
        std::cerr << "Warning: unknown function '" << e.callee << "'\n";
        lastValue_ = nullptr;
        return;
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < e.args.size(); i++) {
        e.args[i]->accept(*this);
        llvm::Value* argVal = lastValue_;
        if (i < callee->arg_size() && argVal) {
            auto* paramType = callee->getArg(i)->getType();
            if (argVal->getType() != paramType) {
                if (paramType->isIntegerTy() && argVal->getType()->isIntegerTy()) {
                    argVal = builder_->CreateIntCast(argVal, paramType, true);
                }
            }
        }
        if (argVal) args.push_back(argVal);
    }

    if (callee->getReturnType()->isVoidTy()) {
        builder_->CreateCall(callee, args);
        lastValue_ = nullptr;
    } else {
        lastValue_ = builder_->CreateCall(callee, args, "calltmp");
    }
}

void LLVMCodeGen::visit(ArrayAccessExpr& e) {
    auto it = namedValues_.find(e.name);
    if (it == namedValues_.end()) {
        lastValue_ = nullptr;
        return;
    }

    e.index->accept(*this);
    llvm::Value* idx = lastValue_;

    auto* alloca = it->second;
    auto* elemType = alloca->getAllocatedType();
    llvm::Value* ptr;

    if (elemType->isArrayTy()) {
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
        ptr = builder_->CreateGEP(elemType, alloca, {zero, idx}, "arridx");
        lastValue_ = builder_->CreateLoad(elemType->getArrayElementType(), ptr, "arrval");
    } else {
        ptr = builder_->CreateGEP(elemType, alloca, idx, "arridx");
        lastValue_ = builder_->CreateLoad(elemType, ptr, "arrval");
    }
}

void LLVMCodeGen::visit(IOExpr& e) {
    declarePrintf();

    if (e.isOutput) {
        for (auto& op : e.operands) {
            if (auto* var = dynamic_cast<VarExpr*>(op.get())) {
                if (var->name == "endl") {
                    createPrintfCall("\n", {});
                    continue;
                }
            }

            op->accept(*this);
            if (!lastValue_) continue;

            if (lastValue_->getType()->isIntegerTy(32)) {
                createPrintfCall("%d", {lastValue_});
            } else if (lastValue_->getType()->isIntegerTy(1)) {
                auto* ext = builder_->CreateZExt(lastValue_,
                    llvm::Type::getInt32Ty(*context_));
                createPrintfCall("%d", {ext});
            } else if (lastValue_->getType()->isFloatingPointTy()) {
                if (lastValue_->getType()->isFloatTy()) {
                    lastValue_ = builder_->CreateFPExt(lastValue_,
                        llvm::Type::getDoubleTy(*context_));
                }
                createPrintfCall("%f", {lastValue_});
            } else if (lastValue_->getType()->isPointerTy()) {
                createPrintfCall("%s", {lastValue_});
            } else if (lastValue_->getType()->isIntegerTy(8)) {
                createPrintfCall("%c", {lastValue_});
            }
        }
    }

    lastValue_ = nullptr;
}

} // namespace greencc
