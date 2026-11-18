const curl = 'curl-8.23.0';

exports.packages = {
  curl: [
    curl,
    curl + '.tar.gz',
    `https://curl.se/download/${curl}.tar.gz`
  ],
};

const curlOptions = [
  '--enable-shared',
  '--with-pic',
  '--enable-optimize',
  '--enable-http',
  '--enable-proxy',
  '--enable-ipv6',
  '--disable-rtsp',
  '--disable-ftp',
  '--disable-ftps',
  '--disable-gopher',
  '--disable-gophers',
  '--disable-pop3',
  '--disable-pop3s',
  '--disable-smb',
  '--disable-smbs',
  '--disable-smtp',
  '--disable-telnet',
  '--disable-tftp',
  '--disable-hsts',
  '--disable-imap',
  '--disable-mqtt',
  '--disable-dict',
  '--disable-ldap',
  '--without-librtmp',
  '--without-zstd',
  '--without-brotli',
  '--without-libidn2',
  '--without-nghttp2',
  '--without-ngtcp2',
  '--without-nghttp3',
  '--without-quiche',
  '--without-libpsl',
];

exports.build = function({ sources, out, config, run }) {
  const { env, jobs } = config;
  const prefix = '--prefix=' + out;

  console.log('Building curl');
  run('./configure', [
    '--with-openssl=' + out,
    ...config.curl,
    ...curlOptions,
    ...config.configure,
    prefix
  ], { cwd: sources.curl, env });
  run('make', [jobs], { cwd: sources.curl, env });
  run('make', ['install'], { cwd: sources.curl, env });
};
