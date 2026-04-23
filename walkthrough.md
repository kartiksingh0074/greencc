# Walkthrough: New Optimization Passes

## What Was Added

Three new optimization passes were implemented in GreenCC:

### 1. Peephole Optimizations (Pass 7)
Pattern-matching simplifications applied after other passes:
- `x - x` → `0`
- `x / x` → `1`
- `x * -1` → `-x` (and `-1 * x`)
- `!!x` → `x` (double negation elimination)
- `if(true)` → keeps only then-branch
- `if(false)` → keeps else-branch or removes entirely

### 2. Function Inlining (Pass 8)
Inlines small functions (≤5 statements, non-recursive, not `main`) into call sites. Eliminates function call overhead (`COST_CALL = 15.0`).

**Strategy**: Replaces `int result = foo(args)` with a block containing parameter variable declarations initialized from arguments, cloned function body, and return value capture.

### 3. Energy-Aware Register Allocation Hints (Pass 9)
Counts variable accesses weighted by loop depth (×100 per nesting level). Variables with ≥50 weighted accesses are marked "hot" and get `!greencc.regpriority` metadata on their LLVM IR alloca instructions.

## Files Modified

| File | Changes |
|---|---|
| [optimizer.h](file:///home/chandresh/.gemini/antigravity/scratch/greencc/include/optimizer.h) | 3 new counters, 3 pass declarations, `hotVariables_` set |
| [optimizer.cpp](file:///home/chandresh/.gemini/antigravity/scratch/greencc/src/optimizer.cpp) | ~430 lines: 3 pass implementations + AST clone helpers |
| [llvm_codegen.h](file:///home/chandresh/.gemini/antigravity/scratch/greencc/include/llvm_codegen.h) | `setHotVariables()`, `hotVariables_` set |
| [llvm_codegen.cpp](file:///home/chandresh/.gemini/antigravity/scratch/greencc/src/llvm_codegen.cpp) | `setHotVariables()` impl + metadata emission |
| [main.cpp](file:///home/chandresh/.gemini/antigravity/scratch/greencc/src/main.cpp) | Integration: passes hot vars to LLVM codegen |
| [test_optimizer.cpp](file:///home/chandresh/.gemini/antigravity/scratch/greencc/tests/test_optimizer.cpp) | 3 new tests |

## Test Results

All 24 tests pass:
```
✅ All tests passed!
  - 7 lexer tests ✓
  - 8 parser tests ✓
  - 9 optimizer tests ✓ (including 3 new)
```

## Sample Output Verification

Running `./build/greencc examples/sample1.cpp` shows:
- **Functions inlined: 1** — `computeValue()` was inlined into `main()`
- **Register hints emitted: 6** — hot loop variables (`i`, `sum`, `scaled`, `multiplier`, `size_inl0`) got `!greencc.regpriority !0` metadata
- LLVM IR shows `!greencc.regpriority !0` on alloca instructions for hot variables
