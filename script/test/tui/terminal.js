/* Reusable TUI driver. Owns the pseudo-terminal process, sends keystrokes, and
   waits for screen or exit conditions. Feeds app output into Screen and keeps
   a raw transcript tail so scenario failures can be diagnosed. */

const { spawn } = require('node:child_process');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const { join } = require('node:path');
const { Screen } = require('./screen');

/* macOS script accepts ordinary pipes but rejects Node's socket-backed stdin.
   Bash process substitution gives it a real pipe through cat. Stop that feeder
   when script exits so Node can observe closure while its input is still open. */
const MACOS_LAUNCHER = [
  'exec 3<&0', /* Keep Node's original stdin open as descriptor 3. */
  'exec 4< <(exec cat <&3)', /* Start cat reading descriptor 3; open its real pipe output as descriptor 4. */
  'feeder=$!', /* Save the background cat process ID so cleanup can stop it. */
  /* On shell exit, stop and reap cat. Ignore errors if it has already exited. */
  'trap \'kill "$feeder" 2>/dev/null; wait "$feeder" 2>/dev/null\' EXIT',
  /* Run argument 1 in a PTY, reading from descriptor 4. -q hides status banners,
     -F flushes output immediately, and /dev/null discards the session recording. */
  '/usr/bin/script -qF /dev/null /bin/sh -c "$1" <&4',
  'exit $?', /* Return script's exit status; cleaning up cat must not replace it. */
].join('\n'); /* Pass the statements to bash -c as separate shell commands. */

class Terminal {
  /* Launch musikcube in a sized PTY and feed stdout into an incrementally updated screen model. */
  constructor(runtime, env, rows, columns) {
    this.screen = new Screen(rows, columns);
    this.events = new EventEmitter();
    this.transcript = Buffer.alloc(0);
    this.pidFile = join(runtime, '.tui-pid');
    this.isProcessGroup = process.platform === 'darwin';

    /* Keep stdin open for keystrokes and return the PTY child's exit status. */
    const command = `stty rows ${rows} cols ${columns} && echo $$ > .tui-pid && exec ./musikcube`;
    const args = process.platform === 'darwin'
      ? ['-c', MACOS_LAUNCHER, 'musikcube-tui', command]
      : ['--quiet', '--flush', '--return', '--command', command, '/dev/null'];
    const host = process.platform === 'darwin' ? '/bin/bash' : 'script';
    this.child = spawn(host, args, {
      cwd: runtime,
      env: { ...env, SHELL: '/bin/sh' },
      detached: this.isProcessGroup,
    });

    this.child.stdout.on('data', data => {
      this.capture(data);
      this.screen.feed(data);
      this.events.emit('update');
    });

    /* Diagnostics belong in the transcript, not the rendered cell grid:
       stderr may contain launcher/library messages rather than terminal output. */
    this.child.stderr.on('data', data => this.capture(data));
    this.child.on('error', error => this.fail(error));
    this.child.stdin.on('error', error => this.fail(error));

    /* Wait for close rather than exit so the final PTY output has been consumed
       before a scenario checks the exit status or releases its runtime. */
    this.closed = new Promise(resolve => {
      this.child.once('close', (code, signal) => {
        this.result = { code, signal };
        resolve();
        this.events.emit('update');
      });
    });
  }

  /* Retain a bounded raw output tail for diagnosing terminal parsing or scenario failures. */
  capture(data) {
    this.transcript = Buffer.concat([this.transcript, data]).subarray(-4000);
  }

  /* Publish asynchronous process or pipe errors to any pending screen/exit expectation. */
  fail(error) {
    this.error = error;
    this.events.emit('update');
  }

  /* Write raw keystrokes to the PTY child without bypassing the app's real input handling. */
  send(keys) {
    this.child.stdin.write(keys);
  }

  /* Read Linux peak RSS and CPU ticks for the actual app process, excluding the terminal launcher. */
  processStats() {
    if (process.platform !== 'linux') return undefined;

    const pid = fs.readFileSync(this.pidFile, 'utf8').trim();
    const status = fs.readFileSync(`/proc/${pid}/status`, 'utf8');
    const peak = status.match(/^VmHWM:\s+(\d+) kB$/m);
    if (!peak) throw new Error('Could not measure peak resident memory');

    /* The command field may contain spaces or parentheses. Fields 14 and 15 follow it. */
    const stat = fs.readFileSync(`/proc/${pid}/stat`, 'utf8');
    const fields = stat.slice(stat.lastIndexOf(')') + 2).trim().split(/\s+/);
    const userTicks = Number(fields[14 - 3]);
    const systemTicks = Number(fields[15 - 3]);
    if (!Number.isSafeInteger(userTicks) || !Number.isSafeInteger(systemTicks)) {
      throw new Error('Could not measure process CPU time');
    }

    return { peakRssKiB: Number(peak[1]), cpuTicks: userTicks + systemTicks };
  }

  /* Wait for required visible text and optional disappearance of text from the previous state. */
  expect(text, { absent } = {}) {
    return this.waitFor(() => {
      const screen = this.screen.text();
      return screen.includes(text) && (!absent || !screen.includes(absent));
    }, `screen text ${JSON.stringify(text)}${absent ? ` with ${JSON.stringify(absent)} absent` : ''}`);
  }

  /* Wait for process closure and require a clean exit rather than treating any termination as success. */
  async expectExit() {
    await this.waitFor(() => this.result, 'musikcube to exit');
    if (this.result.code !== 0) {
      throw new Error(`musikcube exited with ${this.result.signal || this.result.code}`);
    }
  }

  /* Observe screen/process updates until the predicate succeeds, the app exits, or a ten-second deadline
     expires. */
  waitFor(predicate, description) {
    return new Promise((resolve, reject) => {
      /* Every completion path removes its listener/timer; leaking either can
         make a later scenario observe stale state or keep Node alive. */
      const finish = error => {
        clearTimeout(timer);
        this.events.off('update', check);
        if (error) reject(error);
        else resolve();
      };

      /* Check on output/process changes so failures terminate promptly instead
         of waiting out every screen deadline after the app has already exited. */
      const check = () => {
        if (this.error) finish(this.error);
        else if (predicate()) finish();
        else if (this.result) finish(new Error(`musikcube exited while waiting for ${description}`));
      };

      const timer = setTimeout(() => finish(new Error(`Timed out waiting for ${description}`)), 10000);
      this.events.on('update', check);

      /* Check after subscribing: readiness may already be visible, and a
         producer update between subscription and this call must not be lost. */
      check();
    });
  }

  /* Terminate a still-running launcher and await stream closure before releasing scenario resources. */
  async close() {
    if (!this.result) {
      /* A failed scenario must stop the shell, feeder, and script together;
         leaving a feeder alive would keep Node's captured streams open. */
      try {
        if (this.isProcessGroup) process.kill(-this.child.pid, 'SIGTERM');
        else this.child.kill();
      } catch (error) {
        if (error.code !== 'ESRCH') throw error;
      }
    }

    await this.closed;
  }
}

module.exports = { Terminal };
