const mac = process.platform === 'darwin';

const libopenmpt = 'libopenmpt-0.8.10+release';

exports.packages = {
  libopenmpt: [
    mac ? libopenmpt + '.autotools' : libopenmpt,
    libopenmpt + (mac ? '.autotools' : '.makefile') + '.tar.gz',
    `https://lib.openmpt.org/files/libopenmpt/src/${libopenmpt}.${mac ? 'autotools' : 'makefile'}.tar.gz`
  ],
};

const libopenmptConfigureOptions = [
  '--disable-dependency-tracking',
  '--enable-shared',
  '--disable-openmpt123',
  '--disable-examples',
  '--disable-tests',
  '--disable-doxygen-doc',
  '--disable-doxygen-html',
  '--without-mpg123',
  '--without-ogg',
  '--without-vorbis',
  '--without-vorbisfile',
  '--without-portaudio',
  '--without-portaudiocpp',
  '--without-sndfile',
  '--without-flac',
];

const libopenmptMakeOptions = [
  'EXAMPLES=0',
  'NO_FLAC=1',
  'NO_MINIMP3=1',
  'NO_MINIZ=1',
  'NO_MPG123=1',
  'NO_OGG=1',
  'NO_PORTAUDIO=1',
  'NO_PORTAUDIOCPP=1',
  'NO_PULSEAUDIO=1',
  'NO_SDL2=1',
  'NO_SNDFILE=1',
  'NO_STBVORBIS=1',
  'NO_VORBIS=1',
  'NO_VORBISFILE=1',
  'OPENMPT123=0',
  'SHARED_LIB=1',
  'STATIC_LIB=0',
  'TEST=0',
];

const makeBuildOptions = ['VERBOSE=1'];

exports.build = function({ sources, out, config, run }) {
  const { env, jobs } = config;
  const prefix = '--prefix=' + out;

  console.log('Building libopenmpt');
  if (mac) {
    run('./configure', [
      ...libopenmptConfigureOptions,
      ...config.configure,
      prefix
    ], { cwd: sources.libopenmpt, env });
    run('make', [jobs], { cwd: sources.libopenmpt, env });
    run('make', ['install'], { cwd: sources.libopenmpt, env });
  } else {
    const options = [...libopenmptMakeOptions, 'PREFIX=' + out, ...config.openmpt];
    const make = { cwd: sources.libopenmpt, env: config.makefileEnv };
    run('make', [...options, jobs, ...makeBuildOptions], make);
    run('make', [...options, 'install'], make);
  }
};
