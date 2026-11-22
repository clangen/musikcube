/* Stage a pinned x-tools toolchain without modifying its libc sysroot. */
const { execFileSync } = require('child_process');
const { createHash } = require('crypto');
const { chmodSync, createReadStream, existsSync, mkdirSync, mkdtempSync, renameSync, rmSync } = require('fs');
const { join, resolve } = require('path');

const release = '1.1.0';
const gcc = '14';
const glibcVersion = '2.31';
const archiveBase = 'https://github.com/tttapa/docker-arm-cross-toolchain/releases/download';
const checksums = {
  'armv6-rpi-linux-gnueabihf': 'd09311dea9b01445c60f8e2a610e1dea8da03e41fd61976ef1850b2855b94b59',
  'armv8-rpi3-linux-gnueabihf': 'b61d880d1f989fdb40ea2eaec16161ada709832a7bdf3bd738f8c76596d4aac4',
};

async function verify(file, expected) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(file)) hash.update(chunk);
  if (hash.digest('hex') !== expected) throw new Error('x-tools archive checksum mismatch: ' + file);
}

async function stage(root, target) {
  if (process.platform !== 'linux' || process.arch !== 'x64') {
    throw new Error('The pinned x-tools archives require a Linux x86-64 host.');
  }
  const checksum = checksums[target];
  if (!checksum) throw new Error('Unknown x-tools target: ' + target);

  const cache = join(root, '.vendor-cache');
  const tmp = join(cache, 'tmp');
  const downloads = join(cache, 'downloads');
  const tools = join(cache, 'x-tools-' + release, target);
  const filename = `x-tools-${target}-gcc${gcc}.tar.xz`;
  const archive = join(downloads, filename);
  const prefix = join(tools, 'bin', target + '-');
  const sysroot = join(tools, target, 'sysroot');
  const cmake = join(tools, target + '.toolchain.cmake');

  for (const directory of [tmp, downloads, join(tools, '..')]) mkdirSync(directory, { recursive: true });
  const env = { ...process.env, TMPDIR: tmp };
  if (!existsSync(tools)) {
    const pending = mkdtempSync(join(tmp, 'x-tools-'));
    try {
      if (!existsSync(archive)) {
        const file = join(pending, filename);
        execFileSync('wget', ['-q', '-O', file, `${archiveBase}/${release}/${filename}`], { env, stdio: 'inherit' });
        await verify(file, checksum);
        renameSync(file, archive);
      } else {
        await verify(archive, checksum);
      }
      execFileSync('tar', ['-xJf', archive, '-C', pending], { env, stdio: 'inherit' });
      try {
        chmodSync(join(pending, 'x-tools', target), 0o755);
        renameSync(join(pending, 'x-tools', target), tools);
      } catch (error) {
        if (!existsSync(tools)) throw error;
      }
    } finally {
      execFileSync('chmod', ['-R', 'u+w', pending], { env });
      rmSync(pending, { recursive: true, force: true });
    }
  }

  for (const path of [prefix + 'gcc', prefix + 'g++', prefix + 'strip', cmake, sysroot]) {
    if (!existsSync(path)) throw new Error('Incomplete x-tools installation: ' + path);
  }
  return { prefix, sysroot, cmake };
}

module.exports = { stage, glibcVersion };

if (require.main === module) {
  const [root, target, ...extra] = process.argv.slice(2);
  if (!root || !target || extra.length) {
    console.error('Usage: node script/install-crosscompile-toolkits.js <build-root> <x-tools-target>');
    process.exitCode = 1;
  } else {
    stage(resolve(root), target).then(({ prefix }) => console.log(prefix)).catch((error) => {
      console.error(error.message);
      process.exitCode = 1;
    });
  }
}
