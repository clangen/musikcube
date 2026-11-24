/* verify C++ static destructors run when a Zig-built shared library unloads. */
const { copyFileSync, mkdirSync, mkdtempSync, writeFileSync, rmSync } = require('fs');
const { join, resolve } = require('path');
const { spawnSync } = require('child_process');

const compiler = resolve(process.argv[2]);
const cCompiler = resolve(process.argv[3]);
const root = resolve(process.argv[4]);
mkdirSync(root, { recursive: true });
const dir = mkdtempSync(join(root, 'zig-finalizers-'));
function run(program, args) {
  const result = spawnSync(program, args, { encoding: 'utf8' });
  if (result.status !== 0) throw new Error(`${program}: ${result.stderr || result.signal}`);
  return result.stdout;
}
try {
  writeFileSync(join(dir, 'plugin.cpp'), `#include <cstdio>\nstruct Value { ~Value() { puts("finalized"); } };\nextern "C" void initialize() { static Value value; }\n`);
  writeFileSync(join(dir, 'host.c'), `#include <dlfcn.h>\n#include <stdio.h>\nint main(int argc, char **argv) { void *a = dlopen(argv[1], RTLD_NOW); void *b = dlopen(argv[2], RTLD_NOW); if (!a || !b) return 1; void (*ia)(void) = (void (*)(void)) dlsym(a, "initialize"); void (*ib)(void) = (void (*)(void)) dlsym(b, "initialize"); if (!ia || !ib) return 2; ia(); ib(); if (dlclose(a)) return 3; puts("closed first"); if (dlclose(b)) return 4; puts("closed second"); return 0; }\n`);
  run(compiler, ['-shared', '-fPIC', join(dir, 'plugin.cpp'), '-o', join(dir, 'plugin.so')]);
  copyFileSync(join(dir, 'plugin.so'), join(dir, 'plugin2.so'));
  run(cCompiler, [join(dir, 'host.c'), '-ldl', '-o', join(dir, 'host')]);
  const output = run(join(dir, 'host'), [join(dir, 'plugin.so'), join(dir, 'plugin2.so')]);
  if (output !== 'finalized\nclosed first\nfinalized\nclosed second\n') throw new Error(`Unexpected unload order: ${JSON.stringify(output)}`);
  console.log('Zig shared library finalizers passed');
} finally {
  rmSync(dir, { recursive: true, force: true });
}
