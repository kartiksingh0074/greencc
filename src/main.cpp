#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "energy_model.h"
#include "optimizer.h"
#include "codegen.h"
#include "llvm_codegen.h"

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Error: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Error: cannot write to file '" << path << "'\n";
        std::exit(1);
    }
    file << content;
}

static void printBanner() {
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
    std::cout << "  ║                                                      ║\n";
    std::cout << "  ║     🌿  G r e e n C C  —  v2.0                      ║\n";
    std::cout << "  ║     Energy-Aware C++ Compiler (LLVM Backend)         ║\n";
    std::cout << "  ║                                                      ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.cpp> [options]\n";
    std::cerr << "\nOutput Modes:\n";
    std::cerr << "  --emit-llvm       Emit LLVM IR text (.ll) [default]\n";
    std::cerr << "  --emit-obj        Emit native object file (.o)\n";
    std::cerr << "  --emit-cpp        Emit optimized C++ source\n";
    std::cerr << "  --compile         Compile to native executable (via clang)\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  -o <file>         Output file path\n";
    std::cerr << "  --report          Print detailed energy report\n";
    std::cerr << "  --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    bool showReport = false;

    enum class EmitMode { LLVM_IR, OBJ, CPP, COMPILE };
    EmitMode mode = EmitMode::LLVM_IR;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "--report") {
            showReport = true;
        } else if (arg == "--emit-llvm") {
            mode = EmitMode::LLVM_IR;
        } else if (arg == "--emit-obj") {
            mode = EmitMode::OBJ;
        } else if (arg == "--emit-cpp") {
            mode = EmitMode::CPP;
        } else if (arg == "--compile") {
            mode = EmitMode::COMPILE;
        } else if (inputFile.empty()) {
            inputFile = arg;
        } else {
            std::cerr << "Error: unexpected argument '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Error: no input file specified\n";
        printUsage(argv[0]);
        return 1;
    }

    printBanner();
    auto startTime = std::chrono::high_resolution_clock::now();

    
    std::cout << "  [1/6] Lexing...             ";
    std::string source = readFile(inputFile);
    greencc::Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        std::cout << "FAILED\n";
        for (auto& e : lexer.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }
    std::cout << "✓ (" << tokens.size() << " tokens)\n";

    
    std::cout << "  [2/6] Parsing...            ";
    greencc::Parser parser(tokens);
    auto program = parser.parse();

    if (!parser.errors().empty()) {
        std::cout << "FAILED\n";
        for (auto& e : parser.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }
    std::cout << "✓ (" << program->declarations.size() << " declarations)\n";

    
    std::cout << "  [3/6] Analyzing semantics...";
    greencc::SemanticAnalyzer analyzer;
    analyzer.analyze(*program);

    if (!analyzer.errors().empty()) {
        std::cout << "FAILED\n";
        for (auto& e : analyzer.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }
    std::cout << " ✓\n";

    
    std::cout << "  [4/6] Estimating energy...  ";
    greencc::EnergyEstimator estimator;
    auto reportBefore = estimator.estimate(*program);
    std::cout << "✓ (" << reportBefore.totalEnergy << " units)\n";

    
    std::cout << "  [5/6] Optimizing...         ";
    greencc::Optimizer optimizer;
    auto optResult = optimizer.optimize(*program);
    std::cout << "✓\n";

    
    std::cout << "  [6/6] Generating code...    ";

    std::string outputCode;
    bool success = true;

    if (mode == EmitMode::CPP) {
        greencc::CodeGenerator codegen;
        outputCode = codegen.generate(*program);
        std::cout << "✓ (C++ source)\n";
    } else {
        greencc::LLVMCodeGen llvmGen;
        llvmGen.setHotVariables(optimizer.hotVariables_);
        outputCode = llvmGen.generate(*program);

        if (mode == EmitMode::OBJ || mode == EmitMode::COMPILE) {
            std::string objFile = outputFile.empty() ? "/tmp/greencc_output.o" : outputFile;
            if (mode == EmitMode::COMPILE) {
                objFile = "/tmp/greencc_output.o";
            }
            success = llvmGen.writeObjectFile(objFile);

            if (success && mode == EmitMode::COMPILE) {
                std::string binFile = outputFile.empty() ? "a.out" : outputFile;
                std::string cmd = "clang-18 " + objFile + " -o " + binFile + " -lm 2>&1";
                std::cout << "✓ (native object)\n";
                std::cout << "\n  Linking with clang... ";
                int ret = system(cmd.c_str());
                if (ret == 0) {
                    std::cout << "✓\n";
                    std::cout << "  📦 Executable: " << binFile << "\n";
                } else {
                    std::cout << "FAILED\n";
                    success = false;
                }
            } else {
                std::cout << "✓ (object file)\n";
                if (success) std::cout << "  📦 Object file: " << objFile << "\n";
            }
        } else {
            std::cout << "✓ (LLVM IR)\n";
        }
    }

    
    greencc::EnergyEstimator estimatorAfter;
    auto reportAfter = estimatorAfter.estimate(*program);

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    
    std::cout << "\n";
    std::cout << "  ──────────────────────────────────────────────────────\n";
    std::cout << "  Compilation completed in " << elapsed << " ms\n";
    std::cout << "  ──────────────────────────────────────────────────────\n";
    std::cout << "\n";

    
    double savings = 0.0;
    if (reportBefore.totalEnergy > 0) {
        savings = (1.0 - reportAfter.totalEnergy / reportBefore.totalEnergy) * 100.0;
    }

    std::cout << "  ⚡ ENERGY SUMMARY:\n";
    std::cout << "     Before optimization: " << reportBefore.totalEnergy << " energy units\n";
    std::cout << "     After optimization:  " << reportAfter.totalEnergy << " energy units\n";
    if (savings > 0) {
        std::cout << "     ✅ Energy saved:      " << savings << "%\n";
    } else {
        std::cout << "     ℹ️  No further energy savings possible\n";
    }
    std::cout << "\n";

    
    std::cout << optResult.toString();
    std::cout << "\n";

    
    if (showReport) {
        std::cout << "  BEFORE:\n" << reportBefore.toString() << "\n";
        std::cout << "  AFTER:\n"  << reportAfter.toString()  << "\n";
    }

    
    if (mode == EmitMode::CPP || mode == EmitMode::LLVM_IR) {
        if (!outputFile.empty()) {
            writeFile(outputFile, outputCode);
            std::string label = (mode == EmitMode::CPP) ? "C++ source" : "LLVM IR";
            std::cout << "  📄 " << label << " written to: " << outputFile << "\n\n";
        } else if (mode != EmitMode::COMPILE && mode != EmitMode::OBJ) {
            std::cout << "  ── Generated Output ─────────────────────────────────\n\n";
            std::cout << outputCode;
            std::cout << "\n  ──────────────────────────────────────────────────\n\n";
        }
    }

    return success ? 0 : 1;
}
