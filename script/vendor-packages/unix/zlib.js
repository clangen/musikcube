const { copyFileSync, mkdirSync, readdirSync, rmSync } = require('fs');
const { join } = require('path');

const zlib = 'zlib-1.4';
const staticOptions = ['--static'];

exports.packages = {
  zlib: [zlib, zlib + '.tar.gz', `https://zlib.net/${zlib}.tar.gz`],
};

exports.build = function({ sources, config, run }) {
  const { zlib, sourceSysroot, env, jobs } = config;
  const build = { cwd: sources.zlib, env };
  console.log('Building zlib');
  run('./configure', [
    '--prefix=' + zlib,
    ...(sourceSysroot ? [] : staticOptions)
  ], build);
  run('make', [jobs], build);
  run('make', ['install'], build);

  if (sourceSysroot) {
    /* Keep vendor consumers static. The app's shared plugins use the staged .so. */
    const include = join(sourceSysroot, 'usr/include');
    const lib = join(sourceSysroot, 'usr/lib');
    mkdirSync(include, { recursive: true });
    mkdirSync(lib, { recursive: true });
    for (const file of ['zlib.h', 'zconf.h']) {
      copyFileSync(join(zlib, 'include', file), join(include, file));
    }
    copyFileSync(join(zlib, 'lib/libz.so.1'), join(lib, 'libz.so.1'));
    for (const file of readdirSync(join(zlib, 'lib')).filter((file) => /^libz\.so(?:\.|$)/.test(file))) {
      rmSync(join(zlib, 'lib', file));
    }
  }
};
