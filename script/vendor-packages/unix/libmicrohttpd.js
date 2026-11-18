const libmicrohttpd = 'libmicrohttpd-1.0.5';

exports.packages = {
  libmicrohttpd: [
    libmicrohttpd,
    libmicrohttpd + '.tar.gz',
    `https://ftp.gnu.org/gnu/libmicrohttpd/${libmicrohttpd}.tar.gz`
  ],
};

const configureOptions = [
  '--enable-shared',
  '--with-pic',
  '--enable-https=no',
  '--disable-curl',
];
const makeOptions = ['-j8'];

exports.build = function({ sources, out, config, run }) {
  const { env } = config;
  const prefix = '--prefix=' + out;

  console.log('Building libmicrohttpd');
  run('./configure', [
    ...configureOptions,
    prefix,
    ...config.configure
  ], { cwd: sources.libmicrohttpd, env });
  run('make', makeOptions, { cwd: sources.libmicrohttpd, env });
  run('make', ['install'], { cwd: sources.libmicrohttpd, env });
};
