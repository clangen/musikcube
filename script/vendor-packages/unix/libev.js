const libev = 'libev-4.33';

exports.packages = {
  libev: [libev, libev + '.tar.gz', `https://dist.schmorp.de/libev/${libev}.tar.gz`],
};

const configureOptions = [
  '--prefix=/usr',
  '--disable-shared',
  '--enable-static',
  '--with-pic',
];

exports.build = function({ sources, config, run }) {
  console.log('Building libev');
  const build = { cwd: sources.libev, env: config.env };

  run('./configure', [...configureOptions, ...config.configure], build);
  run('make', [config.jobs], build);
  run('make', ['DESTDIR=' + config.staticSysroot, 'install'], build);
};
