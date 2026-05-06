/* ═══════════════════════════════════════════════════════
   GreenCC Web Editor — Application Logic
   ═══════════════════════════════════════════════════════ */

(function () {
  'use strict';

  // ── DOM References ──────────────────────────────────
  const $inputEditor   = document.getElementById('input-code');
  const $outputEditor  = document.getElementById('output-code');
  const $btnOptimize   = document.getElementById('btn-optimize');
  const $btnSample     = document.getElementById('btn-load-sample');
  const $dashboard     = document.getElementById('dashboard');
  const $errorBanner   = document.getElementById('error-banner');
  const $errorTitle    = document.getElementById('error-title');
  const $errorDetails  = document.getElementById('error-details');
  const $outputPlaceholder = document.getElementById('output-placeholder');
  const $inputInfo     = document.getElementById('input-info');
  const $outputInfo    = document.getElementById('output-info');
  const $outputPanel   = document.getElementById('output-panel');

  // Dashboard elements
  const $pipeline      = document.getElementById('pipeline');
  const $compileTime   = document.getElementById('compile-time');
  const $totalOpts     = document.getElementById('total-opts');
  const $energyBefore  = document.getElementById('energy-before');
  const $energyAfter   = document.getElementById('energy-after');
  const $energySavings = document.getElementById('energy-savings');
  const $savingsGauge  = document.getElementById('savings-gauge-fill');
  const $statsGrid     = document.getElementById('stats-grid');

  // ── CodeMirror Instances ────────────────────────────
  const cmOptions = {
    mode: 'text/x-c++src',
    theme: 'material-darker',
    lineNumbers: true,
    matchBrackets: true,
    autoCloseBrackets: true,
    styleActiveLine: true,
    tabSize: 4,
    indentWithTabs: false,
    scrollbarStyle: 'simple',
    lineWrapping: false,
  };

  const inputCM = CodeMirror.fromTextArea($inputEditor, {
    ...cmOptions,
    autofocus: true,
    placeholder: '// Paste your C++ code here...',
  });

  const outputCM = CodeMirror.fromTextArea($outputEditor, {
    ...cmOptions,
    readOnly: true,
    cursorBlinkRate: -1,
  });

  // Hide the output CodeMirror initially
  outputCM.getWrapperElement().style.display = 'none';

  // Update line count in info bar
  inputCM.on('change', () => {
    const lines = inputCM.lineCount();
    $inputInfo.textContent = `${lines} line${lines !== 1 ? 's' : ''} · C++`;
  });

  // ── Default sample code ─────────────────────────────
  const DEFAULT_CODE = `#include <iostream>

using namespace std;

// Compute a value with energy-wasteful patterns
int computeValue(int size) {
    int sum = 0;
    int zero = 0;
    int unused = 42;

    for (int i = 0; i < size; i++) {
        // Constant expression that could be folded
        int multiplier = 2 * 3 * 4;

        // Strength reduction candidate: x * 8 → x << 3
        int scaled = i * 8;

        // Redundant add with zero
        int offset = i + zero;

        sum = sum + scaled + multiplier;
    }

    return sum;
}

// Demonstrate branching and I/O
int main() {
    const int MAX = 10;

    int result = computeValue(MAX);

    if (result > 100) {
        std::cout << "Large result: " << result << std::endl;
    } else {
        std::cout << "Small result: " << result << std::endl;
    }

    // Energy-wasteful: divide where shift would work
    int half = result / 2;
    int quarter = result / 4;

    // Constant expression
    int fixed = 10 + 20 + 30;

    std::cout << "Half: " << half << std::endl;
    std::cout << "Quarter: " << quarter << std::endl;
    std::cout << "Fixed: " << fixed << std::endl;

    return 0;
}`;

  // ── Optimization Stat Definitions ───────────────────
  const STAT_DEFS = [
    { key: 'constantsFolded',       name: 'Constants Folded',       icon: '📐', cssClass: 'fold' },
    { key: 'constantsPropagated',   name: 'Constants Propagated',   icon: '📡', cssClass: 'prop' },
    { key: 'deadCodeEliminated',    name: 'Dead Code Eliminated',   icon: '🗑️', cssClass: 'dead' },
    { key: 'strengthReductions',    name: 'Strength Reductions',    icon: '💪', cssClass: 'strength' },
    { key: 'loopInvariantsMoved',   name: 'Loop Invariants Moved',  icon: '🔄', cssClass: 'licm' },
    { key: 'csesEliminated',        name: 'CSEs Eliminated',        icon: '🔗', cssClass: 'cse' },
    { key: 'functionsInlined',      name: 'Functions Inlined',      icon: '📦', cssClass: 'inline' },
    { key: 'peepholeOptimizations', name: 'Peephole Optimizations', icon: '🔍', cssClass: 'peep' },
    { key: 'registerHintsEmitted',  name: 'Register Hints',         icon: '🎯', cssClass: 'reg' },
  ];

  // ── Helpers ─────────────────────────────────────────

  function showError(title, details) {
    $errorTitle.textContent = title;
    $errorDetails.textContent = details || '';
    $errorBanner.classList.add('visible');
  }

  function hideError() {
    $errorBanner.classList.remove('visible');
  }

  function setLoading(loading) {
    if (loading) {
      $btnOptimize.classList.add('btn-optimize--loading');
      $btnOptimize.disabled = true;
    } else {
      $btnOptimize.classList.remove('btn-optimize--loading');
      $btnOptimize.disabled = false;
    }
  }

  /**
   * Animate a number counting up from 0 to target.
   */
  function animateCounter(element, target, duration = 800, suffix = '') {
    const start = performance.now();
    const initial = 0;

    function update(now) {
      const elapsed = now - start;
      const progress = Math.min(elapsed / duration, 1);
      // Ease out cubic
      const eased = 1 - Math.pow(1 - progress, 3);
      const current = initial + (target - initial) * eased;

      if (Number.isInteger(target)) {
        element.textContent = Math.round(current) + suffix;
      } else {
        element.textContent = current.toFixed(1) + suffix;
      }

      if (progress < 1) {
        requestAnimationFrame(update);
      }
    }

    requestAnimationFrame(update);
  }

  // ── Render Functions ────────────────────────────────

  function renderPipeline(stages) {
    $pipeline.innerHTML = '';

    stages.forEach((stage, i) => {
      const stageEl = document.createElement('div');
      stageEl.className = `pipeline__stage pipeline__stage--${stage.status}`;
      stageEl.innerHTML = `
        <span class="pipeline__stage-icon">${stage.status === 'success' ? '✅' : '❌'}</span>
        <span>${stage.name}</span>
      `;
      $pipeline.appendChild(stageEl);

      if (i < stages.length - 1) {
        const arrow = document.createElement('span');
        arrow.className = 'pipeline__arrow';
        arrow.textContent = '→';
        $pipeline.appendChild(arrow);
      }
    });
  }

  function renderStats(optimizations) {
    $statsGrid.innerHTML = '';

    let totalOpts = 0;

    STAT_DEFS.forEach((def, idx) => {
      const count = optimizations[def.key] || 0;
      totalOpts += count;

      const card = document.createElement('div');
      card.className = 'stat-card';
      card.style.animationDelay = `${idx * 60}ms`;
      card.innerHTML = `
        <div class="stat-card__icon stat-card__icon--${def.cssClass}">${def.icon}</div>
        <div class="stat-card__content">
          <div class="stat-card__name">${def.name}</div>
          <div class="stat-card__count" data-target="${count}">0</div>
        </div>
      `;
      $statsGrid.appendChild(card);

      // Animate the counter after a staggered delay
      const countEl = card.querySelector('.stat-card__count');
      setTimeout(() => {
        animateCounter(countEl, count, 600);
        countEl.classList.add('animate-count');
      }, 200 + idx * 80);
    });

    // Total optimizations counter
    setTimeout(() => {
      animateCounter($totalOpts, totalOpts, 800);
    }, 300);
  }

  function renderDashboard(data) {
    // Show dashboard
    $dashboard.classList.add('visible');

    // Pipeline
    if (data.stages && data.stages.length > 0) {
      renderPipeline(data.stages);
    }

    // Compilation time
    $compileTime.textContent = data.compilationTime.toFixed(2) + ' ms';

    // Energy cards
    animateCounter($energyBefore, data.energyBefore, 1000);
    animateCounter($energyAfter, data.energyAfter, 1000);
    animateCounter($energySavings, data.energySavings, 1200, '%');

    // Savings gauge bar
    setTimeout(() => {
      $savingsGauge.style.width = Math.max(0, Math.min(100, data.energySavings)) + '%';
    }, 400);

    // Optimization stats
    renderStats(data.optimizations);
  }

  function showOutput(code) {
    $outputPlaceholder.style.display = 'none';
    outputCM.getWrapperElement().style.display = '';
    outputCM.setValue(code);
    outputCM.refresh();
    $outputPanel.classList.add('has-output');

    const lines = outputCM.lineCount();
    $outputInfo.textContent = `${lines} line${lines !== 1 ? 's' : ''} · Optimized C++`;
  }

  function resetOutput() {
    $outputPlaceholder.style.display = '';
    outputCM.getWrapperElement().style.display = 'none';
    outputCM.setValue('');
    $outputPanel.classList.remove('has-output');
    $outputInfo.textContent = '—';
    $dashboard.classList.remove('visible');
    $savingsGauge.style.width = '0%';
  }

  // ── API Calls ───────────────────────────────────────

  async function optimizeCode() {
    const code = inputCM.getValue().trim();
    if (!code) {
      showError('No code provided', 'Please write or paste C++ source code in the input editor.');
      return;
    }

    hideError();
    setLoading(true);
    resetOutput();

    try {
      const response = await fetch('/api/optimize', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ code }),
      });

      const data = await response.json();

      if (!response.ok) {
        showError('Server Error', data.error || 'An unexpected error occurred.');
        return;
      }

      if (!data.success) {
        showError(
          data.error || 'Compilation Failed',
          data.details || ''
        );
        // Still render pipeline if available
        if (data.stages && data.stages.length > 0) {
          $dashboard.classList.add('visible');
          renderPipeline(data.stages);
        }
        return;
      }

      // Success — show optimized code and dashboard
      if (data.optimizedCode) {
        showOutput(data.optimizedCode);
      } else {
        showOutput('// No optimized output generated');
      }

      renderDashboard(data);

    } catch (err) {
      showError('Connection Error', 'Could not reach the GreenCC server. Make sure it is running on port 3000.');
    } finally {
      setLoading(false);
    }
  }

  async function loadSample() {
    try {
      const response = await fetch('/api/sample');
      const data = await response.json();
      if (data.code) {
        inputCM.setValue(data.code);
        resetOutput();
        hideError();
      }
    } catch (err) {
      // Fallback to embedded sample
      inputCM.setValue(DEFAULT_CODE);
      resetOutput();
      hideError();
    }
  }

  // ── Event Bindings ──────────────────────────────────

  $btnOptimize.addEventListener('click', optimizeCode);
  $btnSample.addEventListener('click', loadSample);

  // Ctrl+Enter / Cmd+Enter shortcut to optimize
  document.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      optimizeCode();
    }
  });

  // ── Initialize ──────────────────────────────────────

  // Load sample code on startup
  inputCM.setValue(DEFAULT_CODE);
  inputCM.refresh();

  // Trigger info update
  const lines = inputCM.lineCount();
  $inputInfo.textContent = `${lines} line${lines !== 1 ? 's' : ''} · C++`;

})();
