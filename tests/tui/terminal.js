const { spawn } = require('node:child_process');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const { join } = require('node:path');
const { Screen } = require('./screen');

class Terminal {
  constructor(runtime, env, rows, columns) {
    this.screen = new Screen(rows, columns);
    this.events = new EventEmitter();
    this.transcript = Buffer.alloc(0);
    this.pidFile = join(runtime, '.tui-pid');

    /* Keep stdin open for keystrokes and return the PTY child's exit status. */
    const command = `stty rows ${rows} cols ${columns} && echo $$ > .tui-pid && exec ./musikcube`;
    const args = process.platform === 'darwin'
      ? ['-q', '/dev/null', '/bin/sh', '-c', command]
      : ['--quiet', '--flush', '--return', '--command', command, '/dev/null'];
    this.child = spawn('script', args, {
      cwd: runtime,
      env: { ...env, SHELL: '/bin/sh' },
    });

    this.child.stdout.on('data', data => {
      this.capture(data);
      this.screen.feed(data);
      this.events.emit('update');
    });
    this.child.stderr.on('data', data => this.capture(data));
    this.child.on('error', error => this.fail(error));
    this.child.stdin.on('error', error => this.fail(error));
    this.closed = new Promise(resolve => {
      this.child.once('close', (code, signal) => {
        this.result = { code, signal };
        resolve();
        this.events.emit('update');
      });
    });
  }

  capture(data) {
    this.transcript = Buffer.concat([this.transcript, data]).subarray(-4000);
  }

  fail(error) {
    this.error = error;
    this.events.emit('update');
  }

  send(keys) {
    this.child.stdin.write(keys);
  }

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

  expect(text, { absent } = {}) {
    return this.waitFor(() => {
      const screen = this.screen.text();
      return screen.includes(text) && (!absent || !screen.includes(absent));
    }, `screen text ${JSON.stringify(text)}${absent ? ` with ${JSON.stringify(absent)} absent` : ''}`);
  }

  async expectExit() {
    await this.waitFor(() => this.result, 'musikcube to exit');
    if (this.result.code !== 0) {
      throw new Error(`musikcube exited with ${this.result.signal || this.result.code}`);
    }
  }

  waitFor(predicate, description) {
    return new Promise((resolve, reject) => {
      const finish = error => {
        clearTimeout(timer);
        this.events.off('update', check);
        if (error) reject(error);
        else resolve();
      };
      const check = () => {
        if (this.error) finish(this.error);
        else if (predicate()) finish();
        else if (this.result) finish(new Error(`musikcube exited while waiting for ${description}`));
      };
      const timer = setTimeout(() => finish(new Error(`Timed out waiting for ${description}`)), 10000);
      this.events.on('update', check);
      check();
    });
  }

  async close() {
    if (!this.result) this.child.kill();
    await this.closed;
  }
}

module.exports = { Terminal };
