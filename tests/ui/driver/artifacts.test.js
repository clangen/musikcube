/* Failure artifacts are part of the test harness contract: cleanup must not
   erase the evidence a developer needs to diagnose an unattended failure. */

const assert = require('node:assert/strict');
const { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { test } = require('node:test');
const { withRuntime } = require('../support/artifacts');
const { Screen } = require('../support/screen');
const { Terminal } = require('../support/terminal');

/* Keep all generated files below the build root, including deliberate failures. */
function createRuntime() {
  const parent = process.env.MUSIKCUBE_TEST_ARTIFACT_ROOT || resolve(__dirname, '../../../build/test-artifacts');
  mkdirSync(parent, { recursive: true });
  return mkdtempSync(join(parent, 'run-'));
}

/* Passing scenarios must not accumulate catalogs or staged app copies. */
test('successful runtime cleanup happens after scenario completion', async () => {
  const runtime = createRuntime();
  const result = await withRuntime(runtime, async () => {
    writeFileSync(join(runtime, 'setup.log'), 'ready');
    return 42;
  });
  assert.equal(result, 42);
  assert.equal(existsSync(runtime), false);
});

/* A failed assertion must keep existing evidence and explain where it lives. */
test('failed runtimes retain files and the original diagnostic', async () => {
  const runtime = createRuntime();
  try {
    writeFileSync(join(runtime, 'indexer.log'), 'indexed');
    const failure = new Error('expected selected row');
    await assert.rejects(withRuntime(runtime, async () => { throw failure; }), error => {
      assert.equal(error.cause, failure);
      assert.ok(error.message.includes(runtime));
      return true;
    });
    assert.equal(readFileSync(join(runtime, 'indexer.log'), 'utf8'), 'indexed');
    assert.match(readFileSync(join(runtime, 'failure.log'), 'utf8'), /expected selected row/);
  } finally {
    rmSync(runtime, { recursive: true, force: true });
  }
});

/* Screen state and raw output answer different questions; save both without
   requiring a live PTY just to test the diagnostic writer. */
test('terminal artifacts contain visible text, SVG and raw output', () => {
  const runtime = createRuntime();
  try {
    const terminal = Object.create(Terminal.prototype);
    terminal.screen = new Screen(2, 12);
    terminal.screen.feed(Buffer.from('selected'));
    terminal.transcript = Buffer.from('\x1b[Hselected');
    terminal.saveArtifacts(runtime);
    assert.match(readFileSync(join(runtime, 'screen.txt'), 'utf8'), /selected/);
    assert.match(readFileSync(join(runtime, 'screen.svg'), 'utf8'), /<svg/);
    assert.deepEqual(readFileSync(join(runtime, 'terminal.log')), terminal.transcript);
  } finally {
    rmSync(runtime, { recursive: true, force: true });
  }
});
