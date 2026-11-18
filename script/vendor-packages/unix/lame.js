const version = '4.0';
const lame = 'lame-' + version;

exports.packages = {
  lame: [
    lame,
    lame + '.tar.gz',
    `https://downloads.sourceforge.net/project/lame/lame/${version}/${lame}.tar.gz`
  ],
};

const configureOptions = [
  '--disable-dependency-tracking',
  '--disable-debug',
  '--enable-nasm',
  '--disable-frontend',
  '--disable-decoder',
];

exports.build = function({ sources, out, config, run }) {
  const { env, jobs } = config;
  const prefix = '--prefix=' + out;

  console.log('Building LAME');
  run('./configure', [
    ...configureOptions,
    prefix,
    ...config.configure
  ], { cwd: sources.lame, env });
  run('make', [jobs], { cwd: sources.lame, env });
  run('make', ['install'], { cwd: sources.lame, env });
};
