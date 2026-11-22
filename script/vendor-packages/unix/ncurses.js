const { readFileSync, writeFileSync } = require('fs');
const { join } = require('path');

const ncurses = 'ncurses-6.6';

exports.packages = {
  ncurses: [ncurses, ncurses + '.tar.gz', `https://ftp.gnu.org/gnu/ncurses/${ncurses}.tar.gz`],
};

const configureOptions = [
  '--prefix=/usr',
  '--enable-widec',
  '--without-shared',
  '--with-normal',
  '--with-termlib=tinfo',
  '--without-debug',
  '--without-ada',
  '--without-cxx',
  '--without-cxx-binding',
  '--without-tests',
  '--without-manpages',
];

const installTargets = ['install.libs', 'install.includes', 'install.data'];

exports.build = function({ sources, config, run }) {
  console.log('Building ncurses and terminfo');
  const configure = join(sources.ncurses, 'configure');
  /* Drop GNU ld statistics and pass libc through the Zig driver. */
  const script = readFileSync(configure, 'utf8').replace(/,-stats,-lc/g, ' -lc');
  writeFileSync(configure, script);

  const build = { cwd: sources.ncurses, env: config.env };

  run('./configure', [...configureOptions, ...config.configure], build);
  run('make', [config.jobs], build);
  run('make', ['DESTDIR=' + config.staticSysroot, ...installTargets], build);
};
