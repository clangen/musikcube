/* Exercise the macOS PTY launcher with a small shell fixture. These cases catch
   the socket-stdin failure and feeder hangs without building or indexing musikcube. */

const assert = require('node:assert/strict');
const { mkdirSync, mkdtempSync, rmSync, writeFileSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { test } = require('node:test');
const { Terminal } = require('./terminal');

/* Keep each fake app and its terminal private so process/stream cleanup can be
   checked without using a real library, audio device, or user configuration. */
async function withTerminal(script, run) {
  const parent = resolve(__dirname, '../../../build/tui-driver-tests');
  mkdirSync(parent, { recursive: true });
  const runtime = mkdtempSync(join(parent, 'run-'));
  writeFileSync(join(runtime, 'musikcube'), '#!/bin/sh\n' + script, { mode: 0o755 });

  const terminal = new Terminal(runtime, { ...process.env, TERM: 'xterm-256color' }, 18, 140);
  try {
    await run(terminal);
  } finally {
    await terminal.close();
    rmSync(runtime, { recursive: true, force: true });
  }
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
