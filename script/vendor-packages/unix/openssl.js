const openssl = 'openssl-3.5.9';

exports.packages = {
  openssl: [
    openssl,
    openssl + '.tar.gz',
    `https://github.com/openssl/openssl/releases/download/${openssl}/${openssl}.tar.gz`
  ],
};

const configureOptions = [
  '--libdir=lib',
  'no-ssl3',
  'no-ssl3-method',
  'no-zlib',
];
const makeOptions = ['-j8'];
const installOptions = ['install_sw'];

exports.build = function({ sources, out, config, run }) {
  const { env } = config;
  const prefix = '--prefix=' + out;

  console.log('Building OpenSSL');
  run('perl', [
    './Configure',
    prefix,
    ...configureOptions,
    ...config.openssl
  ], { cwd: sources.openssl, env });
  run('make', makeOptions, { cwd: sources.openssl, env });
  run('make', installOptions, { cwd: sources.openssl, env });
};
