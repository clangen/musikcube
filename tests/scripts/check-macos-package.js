/* exercise the package scanner with real Mach-O load commands, including paths
containing spaces. no vendor rebuild or user runtime is needed. */
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const { join, resolve } = require('node:path');

const [parent, compiler, cmake] = process.argv.slice(2);
const source = resolve(__dirname, '../..');
fs.mkdirSync(parent, { recursive: true });
const root = fs.mkdtempSync(join(resolve(parent), 'macos package '));

/* keep tool failures distinct from the scanner rejections asserted below. */
function run(program, args) {
  const result = spawnSync(program, args, { encoding: 'utf8', timeout: 30000 });
  assert.ifError(result.error);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return result.stdout;
}

/* a matching diagnostic proves rejection came from the intended loader rule. */
function scan(expected) {
  const result = spawnSync(process.execPath, [join(source, 'script/scan-standalone.js'), root],
    { encoding: 'utf8', timeout: 30000 });
  assert.ifError(result.error);
  assert.equal(result.status, expected ? 1 : 0, result.stdout + result.stderr);
  if (expected) {
    assert.match(result.stdout + result.stderr, expected);
  }
}

let passed = false;
try {
  fs.mkdirSync(join(root, 'lib'));
  fs.mkdirSync(join(root, 'plugins'));
  const input = join(root, 'fixture.c');
  fs.writeFileSync(input, 'int main(void) { return 0; }\n');
  run(compiler, [input, '-Wl,-headerpad_max_install_names', '-o', join(root, 'musikcube')]);
  fs.copyFileSync(join(root, 'musikcube'), join(root, 'musikcubed'));
  run(compiler, [input, '-dynamiclib', '-Wl,-headerpad_max_install_names', '-Wl,-install_name,@rpath/libmusikcore.dylib', '-o', join(root, 'libmusikcore.dylib')]);
  scan();

  /* an unused absolute fallback still violates relocatability. */
  const stale = join(root, 'old build directory');
  run('install_name_tool', ['-add_rpath', stale, join(root, 'libmusikcore.dylib')]);
  scan(/non-relocatable search path/);

  /* the same staging helper serves archive creation and SDK installation. */
  run(cmake, ['-DMUSIKCORE_RUNTIME=' + root, '-P', join(source, '.cmake/FixupMachORpaths.cmake')]);
  scan();
  run(cmake, ['-DMUSIKCORE_RUNTIME=' + root, '-P', join(source, '.cmake/FixupMachORpaths.cmake')]);
  run('codesign', ['--verify', join(root, 'libmusikcore.dylib')]);

  /* dependencies may be relative even when the library's own identity is stale. */
  run('install_name_tool', ['-id', join(root, 'old install/libmusikcore.dylib'), join(root, 'libmusikcore.dylib')]);
  scan(/non-relocatable install name/);
  run('install_name_tool', ['-id', '@rpath/libmusikcore.dylib', join(root, 'libmusikcore.dylib')]);

  /* relative spellings must not escape the extracted package either. */
  run('install_name_tool', ['-add_rpath', '@loader_path/../outside', join(root, 'libmusikcore.dylib')]);
  scan(/non-relocatable search path/);
  passed = true;
} finally {
  if (passed) {
    fs.rmSync(root, { recursive: true });
  } else {
    console.error(`Mach-O package fixture retained at ${root}`);
  }
}
