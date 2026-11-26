/* exercise staging setup without compiling or replacing an installed SDK. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');
const root = path.resolve(__dirname, '../..');
const script = path.join(root, 'script/build-and-stage-sdk-macos.js');

test('macOS SDK staging uses its checkout and the wrapper build/output paths', () => {
    const build = path.join(root, 'build/test-sdk-build');
    const output = path.join(build, 'stage-test');
    const calls = [];
    const finished = new Error('reached installation');
    const fakeFs = { ...fs, mkdirSync() {}, writeFileSync() {}, appendFileSync() {},
        rmSync() {}, readdirSync: () => [], existsSync: () => false, readFileSync: () => '' };
    assert.throws(() => vm.runInNewContext(fs.readFileSync(script, 'utf8'), {
        __dirname: path.dirname(script), console: { log() {} },
        process: { platform: 'darwin', execPath: process.execPath,
            env: { MUSIKCORE_SDK_BUILD: build, MUSIKCORE_SDK_OUTPUT: output } },
        require(name) {
            if (name === 'fs') return fakeFs;
            if (name === 'path') return path;
            if (name === path.join(root, 'script/unix-build-config')) {
                return { buildArchiveConfiguration: () => ({ env: { TMPDIR: build }, cmake: [] }) };
            }
            if (name === 'child_process') return { spawnSync(command, args, options) {
                calls.push({ command, args, options });
                if (args.includes('--install')) throw finished;
                return { status: 0, stdout: '', stderr: '' };
            } };
            throw new Error(`unexpected module: ${name}`);
        },
    }), error => error === finished);
    assert.equal(calls[0].options.cwd, root);
    assert.equal(calls[0].args[calls[0].args.indexOf('-B') + 1], build);
    const install = calls.at(-1).args;
    assert.equal(install[install.indexOf('--prefix') + 1], output);
});
