/* Stage the pinned Zig toolchain and compiler wrappers under the build root. */
const { spawnSync } = require('child_process');
const { createHash } = require('crypto');
const { createReadStream, existsSync, mkdirSync, mkdtempSync, renameSync, rmSync, writeFileSync } = require('fs');
const { join, resolve } = require('path');

const version = '0.17.1';
/* Match Debian Buster's glibc ABI without using Buster as the build host. */
const glibcVersion = '2.28';
const target = `x86_64-linux-gnu.${glibcVersion}`;
const release = `zig-x86_64-linux-${version}`;
const checksum = '3581863e3cbcfd0fce0efc682ce7e9e93da96668a977791d356a26d2a7e9b17a';

function run(program, args, env) {
  const result = spawnSync(program, args, { env, stdio: 'inherit' });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${program} failed (${result.signal || result.status})`);
}

function quote(value) {
  return "'" + value.replace(/'/g, "'\\''") + "'";
}

async function verifyArchive(path) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  if (hash.digest('hex') !== checksum) throw new Error(`Zig archive checksum mismatch: ${path}`);
}

async function stage(root) {
  if (process.platform !== 'linux' || process.arch !== 'x64') {
    throw new Error('The Zig vendor toolchain requires a Linux x86-64 host.');
  }

  const cache = join(root, '.vendor-cache');
  const downloads = join(cache, 'downloads');
  const archive = join(downloads, release + '.tar.xz');
  const toolchain = join(cache, release);
  const zig = join(toolchain, 'zig');
  const tmp = join(cache, 'tmp');
  const globalCache = join(cache, 'zig-global');
  const localCache = join(cache, 'zig-local');
  const wrappers = join(toolchain, 'bin');
  const toolchainEnv = {
    TMPDIR: tmp,
    ZIG_GLOBAL_CACHE_DIR: globalCache,
    ZIG_LOCAL_CACHE_DIR: localCache,
  };
  const env = { ...process.env, ...toolchainEnv };

  for (const directory of [downloads, tmp, globalCache, localCache]) {
    mkdirSync(directory, { recursive: true });
  }

  if (!existsSync(zig)) {
    const pending = mkdtempSync(join(tmp, 'zig-'));
    try {
      if (!existsSync(archive)) {
        const downloaded = join(pending, release + '.tar.xz');
        run('wget', ['-O', downloaded, `https://ziglang.org/download/${version}/${release}.tar.xz`], env);
        await verifyArchive(downloaded);
        renameSync(downloaded, archive);
      } else {
        await verifyArchive(archive);
      }

      run('tar', ['-xJf', archive, '-C', pending], env);
      try {
        renameSync(join(pending, release), toolchain);
      } catch (error) {
        if (!existsSync(zig)) throw error;
      }
    } finally {
      rmSync(pending, { recursive: true, force: true });
    }
  }

  mkdirSync(wrappers, { recursive: true });
  const commands = {
    cc: `cc -target ${target} -mcpu=baseline`,
    'c++': `c++ -target ${target} -mcpu=baseline`,
    ar: 'ar',
    ranlib: 'ar s',
  };
  for (const [name, args] of Object.entries(commands)) {
    const wrapper = [
      '#!/bin/sh',
      ...Object.entries(toolchainEnv).map(([key, value]) => `export ${key}=${quote(value)}`),
      `exec ${quote(zig)} ${args} "$@"`,
      '',
    ].join('\n');
    const path = join(wrappers, name);
    const pending = mkdtempSync(join(tmp, 'wrapper-'));
    try {
      writeFileSync(join(pending, name), wrapper, { mode: 0o755 });
      renameSync(join(pending, name), path);
    } finally {
      rmSync(pending, { recursive: true, force: true });
    }
  }

  return { wrappers, target, environment: toolchainEnv };
}

module.exports = { glibcVersion, stage };

if (require.main === module) {
  const [root, ...extra] = process.argv.slice(2);
  if (!root || extra.length || root === '--help' || root === '-h') {
    console.log('Usage: node script/stage-zig-toolchain.js <build-root>');
    console.log(`Stage Zig ${version} and print the wrapper directory for ${target}.`);
    process.exitCode = root === '--help' || root === '-h' ? 0 : 1;
  } else {
    stage(resolve(root))
      .then(({ wrappers }) => console.log(wrappers))
      .catch((error) => {
        console.error(error.message);
        process.exitCode = 1;
      });
  }
}
