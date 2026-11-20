/* Exercise the macOS PTY launcher with a small shell fixture. These cases catch
   the socket-stdin failure and feeder hangs without building or indexing musikcube. */

const assert = require('node:assert/strict');
const { mkdirSync, mkdtempSync, writeFileSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { test } = require('node:test');
const { Terminal } = require('../support/terminal');
const { withRuntime } = require('../support/artifacts');

/* Keep each fake app and its terminal private so process/stream cleanup can be
   checked without using a real library, audio device, or user configuration. */
async function withTerminal(script, run) {
  const parent = process.env.MUSIKCUBE_TEST_ARTIFACT_ROOT || resolve(__dirname, '../../../build/tui-driver-tests');
  mkdirSync(parent, { recursive: true });
  const runtime = mkdtempSync(join(parent, 'run-'));
  writeFileSync(join(runtime, 'musikcube'), '#!/bin/sh\n' + script, { mode: 0o755 });

  await withRuntime(runtime, async () => {
    const terminal = new Terminal(runtime, { ...process.env, TERM: 'xterm-256color' }, 18, 140);
    try {
      await run(terminal);
    } catch (error) {
      terminal.saveArtifacts(join(runtime, 'failure'));
      throw error;
    } finally {
      await terminal.close();
    }
  });
}

/* Keep Node's input open after the fake app exits. A plain cat | script pipeline
   can hang here, even though it accepts keystrokes and renders output correctly. */
test('macOS PTY accepts input and exits without closing Node stdin', {
  skip: process.platform !== 'darwin', timeout: 15000,
}, async () => {
  await withTerminal('printf "READY\\n"\nread value\nprintf "RECEIVED:%s\\n" "$value"\n', async terminal => {
    await terminal.expect('READY');
    terminal.send('hello\n');
    await terminal.expect('RECEIVED:hello');
    await terminal.expectExit();
    assert.equal(terminal.child.stdin.writableEnded, false);
  });
});

/* Feeder cleanup must preserve the app's failure status; a successful shell or
   cleanup command must not make a crashing/failing UI scenario appear to pass. */
test('macOS PTY preserves the application exit status', {
  skip: process.platform !== 'darwin', timeout: 15000,
}, async () => {
  await withTerminal('exit 7\n', async terminal => {
    await assert.rejects(terminal.expectExit(), /musikcube exited with 7/);
    assert.equal(terminal.result.code, 7);
  });
});

/* Changing only the model would hide resize failures. Ask the child itself
   for its actual PTY dimensions after the driver updates the window size. */
test('macOS PTY resize reaches the child and updates the screen model', {
  skip: process.platform !== 'darwin', timeout: 15000,
}, async () => {
  await withTerminal('printf "READY\\n"\nread value\nstty size\n', async terminal => {
    await terminal.expect('READY');
    await terminal.resize(24, 100);
    terminal.send('size\n');
    await terminal.expect('24 100');
    await terminal.expectExit();
    assert.equal(terminal.screen.rows, 24);
    assert.equal(terminal.screen.columns, 100);
  });
});

/* A failed assertion closes a still-running app. Its shell/feeder must not keep
   captured streams open and prevent the test process from finishing. */
test('macOS PTY cleanup stops a running scenario', {
  skip: process.platform !== 'darwin', timeout: 15000,
}, async () => {
  await withTerminal('printf "READY\\n"\nexec sleep 60\n', async terminal => {
    await terminal.expect('READY');
    await terminal.close();
    assert.equal(terminal.result.signal, 'SIGTERM');
  });
});
