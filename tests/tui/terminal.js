const { spawn } = require('node:child_process');
const { EventEmitter } = require('node:events');
const { Screen } = require('./screen');

class Terminal {
  constructor(runtime, env, rows, columns) {
    this.screen = new Screen(rows, columns);
    this.events = new EventEmitter();
    this.transcript = Buffer.alloc(0);

    /* Keep stdin open for keystrokes and return the PTY child's exit status. */
    const command = `stty rows ${rows} cols ${columns} && exec ./musikcube`;
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
