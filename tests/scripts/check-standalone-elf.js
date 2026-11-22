/* Exercise the readelf audit through its CLI with a relocatable dependency. */
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const { copyFileSync, mkdirSync, mkdtempSync, renameSync, rmSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');

const [compiler, scanner, staging] = process.argv.slice(2);
mkdirSync(staging, { recursive: true });
const root = mkdtempSync(join(staging, 'elf-audit-'));
const lib = join(root, 'lib');
const plugins = join(root, 'plugins');
const source = join(root, 'main.c');
const dependency = 'libaudit-fixture.so.1';

function compile(...args) {
  const result = spawnSync(compiler, ['-O2', ...args], {
    encoding: 'utf8',
    env: { ...process.env, TMPDIR: root }
  });
  assert.equal(result.status, 0, result.stderr);
}

function audit(target, ...args) {
  return spawnSync(process.execPath, [scanner, root, '--target', target, ...args], { encoding: 'utf8' });
}

try {
  mkdirSync(lib);
  mkdirSync(plugins);
  writeFileSync(source, 'int fixture(void) { return 0; }\n');
  compile('-shared', '-fPIC', '-Wl,-soname,' + dependency, source, '-o', join(lib, dependency));
  copyFileSync(join(lib, dependency), join(root, 'libmusikcore.so'));
  copyFileSync(join(lib, dependency), join(plugins, 'libfixture.so'));

  writeFileSync(source, 'int fixture(void); int main(void) { return fixture(); }\n');
  compile(source, join(lib, dependency), '-Wl,-rpath,$ORIGIN/lib', '-o', join(root, 'musikcube'));
  copyFileSync(join(root, 'musikcube'), join(root, 'musikcubed'));

  const good = audit('x86_64');
  assert.equal(good.status, 0, good.stderr);
  const wrongTarget = audit('armv6');
  assert.equal(wrongTarget.status, 1);
  assert.match(wrongTarget.stderr, /wrong ELF architecture/);
  const oldGlibc = audit('x86_64', '--glibc-max', '2.0');
  assert.equal(oldGlibc.status, 1);
  assert.match(oldGlibc.stderr, /GLIBC_\d/);

  const alsa = join(root, 'libasound.so.2');
  const output = join(plugins, 'libalsaout.so');
  writeFileSync(source, 'int fixture(void) { return 0; }\n');
  compile('-shared', '-fPIC', '-Wl,-soname,libasound.so.2', source, '-o', alsa);
  writeFileSync(source, 'int fixture(void); int output(void) { return fixture(); }\n');
  compile('-shared', '-fPIC', source, alsa, '-o', output);
  rmSync(alsa);
  assert.equal(audit('x86_64').status, 0);
  renameSync(output, join(plugins, 'libunrelated.so'));
  const unexpectedHostDependency = audit('x86_64');
  assert.equal(unexpectedHostDependency.status, 1);
  assert.match(unexpectedHostDependency.stderr, /libasound\.so\.2 \(not found on packaged search paths\)/);
  rmSync(join(plugins, 'libunrelated.so'));

  renameSync(join(lib, dependency), join(plugins, dependency));
  const wrongPath = audit('x86_64');
  assert.equal(wrongPath.status, 1);
  assert.match(wrongPath.stderr, /libaudit-fixture\.so\.1 \(not found on packaged search paths\)/);

  renameSync(join(plugins, dependency), join(lib, dependency));
  writeFileSync(source, 'int fixture(void); int main(void) { return fixture(); }\n');
  compile(source, join(lib, dependency), '-Wl,-rpath,' + lib, '-o', join(root, 'musikcube'));
  const absolutePath = audit('x86_64');
  assert.equal(absolutePath.status, 1);
  assert.match(absolutePath.stderr, /non-relocatable search path/);
} finally {
  rmSync(root, { recursive: true, force: true });
}
