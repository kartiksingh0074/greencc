const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

const app = express();
const PORT = process.env.PORT || 3000;

// Path to the greencc binary (one level up from web/)
const GREENCC_BIN = path.resolve(__dirname, '..', 'build', 'greencc');

app.use(cors());
app.use(express.json({ limit: '1mb' }));
app.use(express.static(path.join(__dirname, 'public')));

/**
 * Parse the greencc compiler output into structured data.
 */
function parseCompilerOutput(stdout) {
  const result = {
    optimizedCode: '',
    energyBefore: 0,
    energyAfter: 0,
    energySavings: 0,
    compilationTime: 0,
    optimizations: {
      constantsFolded: 0,
      constantsPropagated: 0,
      deadCodeEliminated: 0,
      strengthReductions: 0,
      loopInvariantsMoved: 0,
      csesEliminated: 0,
      functionsInlined: 0,
      peepholeOptimizations: 0,
      registerHintsEmitted: 0,
    },
    stages: [],
    raw: stdout,
  };

  const lines = stdout.split('\n');

  // Parse compilation stages
  for (const line of lines) {
    const stageMatch = line.match(/\[(\d)\/6\]\s+(.+?)\.\.\.\s+(.*)/);
    if (stageMatch) {
      result.stages.push({
        step: parseInt(stageMatch[1]),
        name: stageMatch[2].trim(),
        status: stageMatch[3].includes('✓') ? 'success' : 'failed',
        detail: stageMatch[3].replace('✓', '').trim(),
      });
    }
  }

  // Parse compilation time
  const timeMatch = stdout.match(/Compilation completed in ([\d.]+) ms/);
  if (timeMatch) {
    result.compilationTime = parseFloat(timeMatch[1]);
  }

  // Parse energy summary
  const beforeMatch = stdout.match(/Before optimization:\s+([\d.]+)\s+energy units/);
  const afterMatch = stdout.match(/After optimization:\s+([\d.]+)\s+energy units/);
  const savingsMatch = stdout.match(/Energy saved:\s+([\d.]+)%/);

  if (beforeMatch) result.energyBefore = parseFloat(beforeMatch[1]);
  if (afterMatch) result.energyAfter = parseFloat(afterMatch[1]);
  if (savingsMatch) result.energySavings = parseFloat(savingsMatch[1]);

  // If no explicit savings but we have both values, compute it
  if (!savingsMatch && result.energyBefore > 0) {
    const computed = (1.0 - result.energyAfter / result.energyBefore) * 100.0;
    result.energySavings = computed > 0 ? computed : 0;
  }

  // Parse optimization counts
  const optPatterns = {
    constantsFolded:       /Constants folded:\s+(\d+)/,
    constantsPropagated:   /Constants propagated:\s+(\d+)/,
    deadCodeEliminated:    /Dead code eliminated:\s+(\d+)/,
    strengthReductions:    /Strength reductions:\s+(\d+)/,
    loopInvariantsMoved:   /Loop invariants moved:\s+(\d+)/,
    csesEliminated:        /CSEs eliminated:\s+(\d+)/,
    functionsInlined:      /Functions inlined:\s+(\d+)/,
    peepholeOptimizations: /Peephole optimizations:\s+(\d+)/,
    registerHintsEmitted:  /Register hints emitted:\s+(\d+)/,
  };

  for (const [key, pattern] of Object.entries(optPatterns)) {
    const match = stdout.match(pattern);
    if (match) {
      result.optimizations[key] = parseInt(match[1]);
    }
  }

  // Extract optimized code (between Generated Output markers)
  const outputStartMarker = '── Generated Output';
  const outputEndMarker = '──────────────────────────────────────────────────';

  const startIdx = stdout.indexOf(outputStartMarker);
  if (startIdx !== -1) {
    // Find the end of the marker line
    const afterStart = stdout.indexOf('\n', startIdx);
    if (afterStart !== -1) {
      // Find the closing marker
      const remaining = stdout.substring(afterStart + 1);
      const endIdx = remaining.lastIndexOf(outputEndMarker);
      if (endIdx !== -1) {
        result.optimizedCode = remaining.substring(0, endIdx).trim();
      } else {
        result.optimizedCode = remaining.trim();
      }
    }
  }

  return result;
}

/**
 * POST /api/optimize
 * Body: { code: string }
 * Returns: structured optimization result
 */
app.post('/api/optimize', async (req, res) => {
  const { code } = req.body;

  if (!code || typeof code !== 'string' || code.trim().length === 0) {
    return res.status(400).json({ error: 'No source code provided.' });
  }

  // Check that the greencc binary exists
  if (!fs.existsSync(GREENCC_BIN)) {
    return res.status(500).json({
      error: 'GreenCC compiler binary not found. Please build the project first (cmake + make).',
    });
  }

  // Create a temporary file for the source code
  const tmpDir = path.join(__dirname, '.tmp');
  if (!fs.existsSync(tmpDir)) {
    fs.mkdirSync(tmpDir, { recursive: true });
  }

  const fileId = crypto.randomBytes(8).toString('hex');
  const tmpFile = path.join(tmpDir, `input_${fileId}.cpp`);

  try {
    fs.writeFileSync(tmpFile, code, 'utf-8');

    // Run the compiler
    const result = await new Promise((resolve, reject) => {
      execFile(
        GREENCC_BIN,
        [tmpFile, '--emit-cpp'],
        { timeout: 10000, maxBuffer: 1024 * 1024 },
        (error, stdout, stderr) => {
          // greencc may exit 0 with output, or non-zero on errors
          if (error && !stdout) {
            reject(new Error(stderr || error.message));
          } else {
            resolve({ stdout: stdout || '', stderr: stderr || '', exitCode: error ? error.code : 0 });
          }
        }
      );
    });

    const parsed = parseCompilerOutput(result.stdout);

    // Check for compilation stage failures
    const failedStages = parsed.stages.filter(s => s.status === 'failed');
    if (failedStages.length > 0) {
      return res.json({
        success: false,
        error: `Compilation failed at stage: ${failedStages[0].name}`,
        details: result.stderr || result.stdout,
        stages: parsed.stages,
      });
    }

    return res.json({
      success: true,
      ...parsed,
    });
  } catch (err) {
    return res.status(500).json({
      success: false,
      error: err.message,
    });
  } finally {
    // Clean up temp file
    try { fs.unlinkSync(tmpFile); } catch (_) {}
  }
});

/**
 * GET /api/sample
 * Returns sample C++ code for the editor
 */
app.get('/api/sample', (req, res) => {
  const samplePath = path.resolve(__dirname, '..', 'examples', 'sample1.cpp');
  if (fs.existsSync(samplePath)) {
    const code = fs.readFileSync(samplePath, 'utf-8');
    res.json({ code });
  } else {
    res.json({
      code: `#include <iostream>

using namespace std;

int computeValue(int size) {
    int sum = 0;
    int zero = 0;
    int unused = 42;

    for (int i = 0; i < size; i++) {
        int multiplier = 2 * 3 * 4;
        int scaled = i * 8;
        int offset = i + zero;
        sum = sum + scaled + multiplier;
    }

    return sum;
}

int main() {
    const int MAX = 10;
    int result = computeValue(MAX);

    if (result > 100) {
        std::cout << "Large result: " << result << std::endl;
    } else {
        std::cout << "Small result: " << result << std::endl;
    }

    int half = result / 2;
    int quarter = result / 4;
    int fixed = 10 + 20 + 30;

    std::cout << "Half: " << half << std::endl;
    std::cout << "Quarter: " << quarter << std::endl;
    std::cout << "Fixed: " << fixed << std::endl;

    return 0;
}
`,
    });
  }
});

// Fallback to index.html for SPA
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
  console.log(`\n  🌿 GreenCC Web Editor running at http://localhost:${PORT}\n`);
});
