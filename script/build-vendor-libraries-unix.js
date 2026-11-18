// Build and stage the Unix dependencies used by archive-standalone-nix.sh.
const { spawnSync } = require('child_process');
const {
  closeSync,
  copyFileSync,
  existsSync,
  fstatSync,
  mkdirSync,
  mkdtempSync,
  openSync,
  readFileSync,
  readSync,
  readdirSync,
  realpathSync,
  renameSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} = require('fs');
const { basename, join } = require('path');
const { createInterface } = require('readline');
const { glibcVersion, stage: stageZigToolchain } = require('./stage-zig-toolchain');

const versions = {
  openssl: '3.5.9',
  curl: '8.23.0',
  libmicrohttpd: '1.0.5',
  ffmpeg: '8.0.3',
  ogg: '1.3.6',
  vorbis: '1.3.7',
  opus: '1.6.1',
  lame: '4.0',
  libopenmpt: '0.8.10',
  taglib: '2.3.2',
  gme: '0.6.4',
};

const mac = process.platform === 'darwin';
const buildRoot = '/build';
const buildDirectory = process.cwd();
const cache = join(buildDirectory, '.vendor-cache', 'downloads');
const scratch = join(buildDirectory, '.vendor-cache', 'tmp');
const verbose = process.argv.includes('--verbose', 2);

function run(program, args, { capture = false, ...options } = {}) {
  const logDir = !capture && !verbose ? mkdtempSync(join(scratch, 'build-log-')) : null;
  let log;

  try {
    if (logDir) log = openSync(join(logDir, 'output'), 'w+');

    const stdio = capture ? ['ignore', 'pipe', 'inherit']
      : logDir ? ['inherit', log, log] : 'inherit';
    const result = spawnSync(program, args, {
      encoding: 'utf8',
      stdio,
      env: { ...process.env, TMPDIR: scratch },
      ...options,
    });

    if (result.error) throw result.error;
    if (result.status !== 0) {
      if (logDir) {
        const size = fstatSync(log).size;
        const tail = Buffer.alloc(Math.min(size, 64 * 1024));
        const bytesRead = readSync(log, tail, 0, tail.length, size - tail.length);
        if (bytesRead) console.error(tail.toString('utf8', 0, bytesRead).trimEnd());
      }

      throw new Error(`${program} ${args.join(' ')} failed (${result.signal || result.status})`);
    }

    return capture ? result.stdout.trim() : undefined;
  } finally {
    if (log !== undefined) closeSync(log);
    if (logDir) rmSync(logDir, { recursive: true, force: true });
  }
}

async function configureZig(config, out) {
  if (mac || process.arch !== 'x64') {
    throw new Error('--compiler zig currently requires Linux x86-64.');
  }

  console.log('Staging Zig toolchain');
  const toolchain = await stageZigToolchain(buildDirectory);
  const { wrappers } = toolchain;
  const { env } = config;
  env.CC = join(wrappers, 'cc');
  env.CXX = join(wrappers, 'c++');
  env.AR = join(wrappers, 'ar');
  env.RANLIB = join(wrappers, 'ranlib');
  delete env.LD;

  /* Build zlib against the same glibc baseline as its consumers. */
  const zlib = join(out, '..', '..', 'zig-zlib');
  const zlibSource = 'zlib-1.4';
  const marker = `zig-glibc${glibcVersion}`;
  Object.assign(config, {
    marker,
    toolchain,
    variant: config.arch + '-' + marker,
    zlib,
    packages: { zlib: [zlibSource, zlibSource + '.tar.gz', `https://zlib.net/${zlibSource}.tar.gz`] },
    buildPrerequisites(sources) {
      const build = { cwd: sources.zlib, env: { ...env, CFLAGS: env.CFLAGS + ' -O2' } };
      console.log('Building static zlib');
      run('./configure', ['--static', '--prefix=' + zlib], build);
      run('make', [config.jobs], build);
      run('make', ['install'], build);
    },
    curl: ['--with-zlib=' + zlib],
    /* FFmpeg selects its compiler and archiver through configure flags. */
    ffmpeg: [
      '--cc=' + env.CC,
      '--cxx=' + env.CXX,
      '--ld=' + env.CC,
      '--ar=' + env.AR,
      '--ranlib=' + env.RANLIB,
    ],
    cmake: [
      '-DCMAKE_AR=' + env.AR,
      '-DCMAKE_RANLIB=' + env.RANLIB,
      '-DZLIB_ROOT=' + zlib,
      '-DZLIB_USE_STATIC_LIBS=ON',
    ],
    openmpt: ['CONFIG=clang', 'CC=' + env.CC, 'CXX=' + env.CXX, 'LD=' + env.CXX, 'AR=' + env.AR],
    makefileEnv: {
      ...env,
      CPPFLAGS: [env.CPPFLAGS, '-I' + join(zlib, 'include')].filter(Boolean).join(' '),
      LDFLAGS: [env.LDFLAGS, '-L' + join(zlib, 'lib')].filter(Boolean).join(' '),
      PKG_CONFIG_PATH: '',
      PKG_CONFIG_LIBDIR: join(zlib, 'lib', 'pkgconfig'),
      PKG_CONFIG_SYSROOT_DIR: '',
    },
    audit: ['--glibc-max', glibcVersion],
  });
}

function configureRpi(config, out, arm, toolchain) {
  // The toolchain's libstdc++ is newer than the oldest target system's.
  const { arch, env } = config;
  const tools = join(buildRoot, 'x-tools', toolchain);
  const sysroot = join(tools, toolchain, 'sysroot');
  const marker = 'rpi-' + arm;

  env.PATH = join(tools, 'bin') + ':' + env.PATH;
  env.LDFLAGS = [env.LDFLAGS, `--sysroot=${sysroot}/`].filter(Boolean).join(' ');
  env.CXXFLAGS += ' -static-libstdc++';
  env.PKG_CONFIG_PATH = join(out, 'lib', 'pkgconfig') + ':' +
    join(sysroot, 'usr/lib/arm-linux-gnueabihf/pkgconfig');

  Object.assign(config, {
    marker,
    variant: marker,
    cmake: [
      '--toolchain',
      join(buildRoot, 'musikcube', '.cmake', `RaspberryPiToolchain-${arm}.cmake`)
    ],
    openssl: ['-latomic', 'linux-generic32', `--cross-compile-prefix=${toolchain}-`],
    configure: [
      '--build=x86_64-pc-linux-gnu',
      `--host=${toolchain}`,
      `--with-sysroot=${sysroot}/`
    ],
    ffmpeg: [
      `--arch=${arch}`,
      '--target-os=linux',
      '--enable-cross-compile',
      `--sysroot=${sysroot}/`,
      `--cross-prefix=${toolchain}-`
    ],
    makefileEnv: {
      ...env,
      AR: `${toolchain}-ar`,
      CC: `${toolchain}-gcc`,
      CXX: `${toolchain}-g++`,
      LD: `${toolchain}-ld`,
    },
  });
}

function configureX86(config) {
  config.marker = 'x86';
  config.openssl = ['linux-x86'];
  config.env.CFLAGS += ' -I/usr/include/i386-linux-gnu/';
  config.env.CXXFLAGS += ' -I/usr/include/i386-linux-gnu/';
}

const compilers = new Map([
  ['default', () => {}],
  ['zig', configureZig],
  ['rpi-armv6', (config, out) => configureRpi(config, out, 'armv6', 'armv6-rpi-linux-gnueabihf')],
  ['rpi-armv8', (config, out) => configureRpi(config, out, 'armv8', 'armv8-rpi3-linux-gnueabihf')],
  ['x86', configureX86],
]);

function promptCompiler() {
  const suggested = process.env.CROSSCOMPILE || 'default';
  const input = createInterface({ input: process.stdin, output: process.stdout });
  const names = [...compilers.keys()].join(', ');

  return new Promise((resolve) => {
    input.once('close', () => resolve(suggested));
    input.question(`Compiler (${names}) [${suggested}]: `, (answer) => {
      resolve(answer.trim() || suggested);
      input.close();
    });
  });
}

async function configuration(out, compiler) {
  const arch = run('uname', ['-m'], { capture: true });
  const env = { ...process.env, TMPDIR: scratch, CFLAGS: '-fPIC', CXXFLAGS: '-fPIC -std=c++17' };

  let jobs = '-j8';

  if (mac) {
    jobs = '-j' + run('sysctl', ['-n', 'hw.ncpu'], { capture: true });
  } else {
    const nproc = spawnSync('nproc', [], { encoding: 'utf8' });
    if (nproc.status === 0) jobs = '-j' + nproc.stdout.trim();
  }

  const config = {
    arch,
    env,
    jobs,
    compiler,
    marker: '',
    variant: arch,
    packages: {},
    buildPrerequisites: () => {},
    configure: [],
    curl: [],
    cmake: [],
    ffmpeg: [],
    openmpt: [],
    audit: [],
    openssl: [mac ? `darwin64-${arch}-cc` : `linux-${arch}`],
    makefileEnv: env,
  };

  await compilers.get(compiler)(config, out);

  return config;
}

function printConfiguration(config) {
  const { env, makefileEnv, packages, ...options } = config;
  /* Report build settings without dumping unrelated inherited variables. */
  const tools = ['CC', 'CXX', 'CPP', 'LD', 'AR', 'RANLIB', 'NM', 'STRIP', 'PKG_CONFIG'];
  const flags = ['CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS', 'ARFLAGS'];
  const variables = [
    'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH',
    'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR',
    'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PREFIX_PATH', 'PATH', 'TMPDIR',
  ];
  const environment = Object.fromEntries([
    ...tools.map((name) => [name, env[name] ?? '(build system default)']),
    ...flags.map((name) => [name, env[name] ?? '']),
    ...variables.filter((name) => env[name] !== undefined).map((name) => [name, env[name]]),
  ]);
  const makefileEnvironmentOverrides = Object.fromEntries(
    Object.entries(makefileEnv).filter(([name, value]) => value !== env[name])
  );

  console.log('Build configuration');
  console.log(JSON.stringify({ environment, makefileEnvironmentOverrides, ...options }, null, 2));
}

function download(url, file) {
  const archive = join(cache, file);
  if (existsSync(archive)) return archive;

  console.log('Downloading ' + file);
  mkdirSync(cache, { recursive: true });
  const temp = mkdtempSync(join(cache, 'download-'));
  const pending = join(temp, file);
  try {
    run('wget', ['-O', pending, url]);
    renameSync(pending, archive);
  } finally {
    rmSync(temp, { recursive: true, force: true });
  }
  return archive;
}

function fetchPackages(work, extraPackages) {
  const openssl = 'openssl-' + versions.openssl;
  const curl = 'curl-' + versions.curl;
  const libmicrohttpd = 'libmicrohttpd-' + versions.libmicrohttpd;
  const ffmpeg = 'ffmpeg-' + versions.ffmpeg;
  const ogg = 'libogg-' + versions.ogg;
  const vorbis = 'libvorbis-' + versions.vorbis;
  const opus = 'opus-' + versions.opus;
  const lame = 'lame-' + versions.lame;
  const libopenmpt = 'libopenmpt-' + versions.libopenmpt + '+release';
  const taglib = 'taglib-' + versions.taglib;
  const gme = 'game-music-emu-' + versions.gme;

  const packages = {
    openssl: [
      openssl,
      openssl + '.tar.gz',
      `https://github.com/openssl/openssl/releases/download/${openssl}/${openssl}.tar.gz`
    ],
    curl: [
      curl,
      curl + '.tar.gz',
      `https://curl.se/download/${curl}.tar.gz`
    ],
    libmicrohttpd: [
      libmicrohttpd,
      libmicrohttpd + '.tar.gz',
      `https://ftp.gnu.org/gnu/libmicrohttpd/${libmicrohttpd}.tar.gz`
    ],
    ffmpeg: [
      ffmpeg,
      ffmpeg + '.tar.bz2',
      `https://ffmpeg.org/releases/${ffmpeg}.tar.bz2`
    ],
    ogg: [
      ogg,
      ogg + '.tar.gz',
      `https://downloads.xiph.org/releases/ogg/${ogg}.tar.gz`
    ],
    vorbis: [
      vorbis,
      vorbis + '.tar.gz',
      `https://downloads.xiph.org/releases/vorbis/${vorbis}.tar.gz`
    ],
    opus: [
      opus,
      opus + '.tar.gz',
      `https://downloads.xiph.org/releases/opus/${opus}.tar.gz`
    ],
    lame: [
      lame,
      lame + '.tar.gz',
      `https://downloads.sourceforge.net/project/lame/lame/${versions.lame}/${lame}.tar.gz`
    ],
    libopenmpt: [
      mac ? libopenmpt + '.autotools' : libopenmpt,
      libopenmpt + (mac ? '.autotools' : '.makefile') + '.tar.gz',
      `https://lib.openmpt.org/files/libopenmpt/src/${libopenmpt}.${mac ? 'autotools' : 'makefile'}.tar.gz`
    ],
    taglib: [
      taglib,
      taglib + '.tar.gz',
      `https://github.com/taglib/taglib/releases/download/v${versions.taglib}/${taglib}.tar.gz`
    ],
    gme: [
      gme,
      gme + '.tar.gz',
      `https://codeload.github.com/libgme/game-music-emu/tar.gz/refs/tags/${versions.gme}`
    ],
    ...extraPackages,
  };

  const sources = {};

  for (const [name, [directory, file, url]] of Object.entries(packages)) {
    const archive = download(url, file);
    console.log('Extracting ' + file);
    copyFileSync(archive, join(work, file));
    run('tar', [file.endsWith('.bz2') ? '-xjf' : '-xzf', file], { cwd: work });
    sources[name] = join(work, directory);
  }

  return sources;
}

function cleanLibraries(lib) {
  for (const file of readdirSync(lib)) {
    if (file.endsWith('.a') || file.endsWith('.la')) rmSync(join(lib, file));
  }

  if (mac) {
    // Keep the ABI version in the filename used by the release staging script.
    for (const [name, abi] of [['avcodec', 62], ['avformat', 62], ['avutil', 60], ['swresample', 6]]) {
      const leaf = `lib${name}-musikcube`;
      const versioned = `${leaf}.${abi}.dylib`;
      const dylib = join(lib, versioned);
      const installed = realpathSync(dylib);
      if (installed !== dylib && basename(installed) !== versioned) {
        renameSync(installed, dylib);
      }
      rmSync(join(lib, leaf + '.dylib'));
      symlinkSync(versioned, join(lib, leaf + '.dylib'));
    }
  }
}

const ffmpegOptions = [
  '--enable-rpath',
  '--disable-autodetect',
  '--enable-pthreads',
  '--disable-asm',
  '--enable-pic',
  '--enable-shared',
  '--disable-everything',
  '--disable-programs',
  '--disable-doc',
  '--disable-debug',
  '--disable-dxva2',
  '--disable-avdevice',
  '--disable-avfilter',
  '--disable-swscale',
  '--disable-ffplay',
  '--disable-network',
  '--disable-muxers',
  '--disable-demuxers',
  '--disable-zlib',
  '--disable-bzlib',
  '--disable-iconv',
  '--disable-bsfs',
  '--disable-filters',
  '--disable-parsers',
  '--disable-indevs',
  '--disable-outdevs',
  '--disable-encoders',
  '--disable-decoders',
  '--disable-hwaccels',
  '--disable-nvenc',
  '--disable-videotoolbox',
  '--disable-audiotoolbox',
  '--disable-libxcb',
  '--disable-libxcb-shm',
  '--disable-libxcb-xfixes',
  '--disable-libxcb-shape',
  '--disable-sdl2',
  '--disable-securetransport',
  '--disable-vaapi',
  '--disable-xlib',
  '--enable-libopus',
  '--enable-libvorbis',
  '--enable-demuxer=aac',
  '--enable-demuxer=ac3',
  '--enable-demuxer=aiff',
  '--enable-demuxer=ape',
  '--enable-demuxer=asf',
  '--enable-demuxer=au',
  '--enable-demuxer=avi',
  '--enable-demuxer=flac',
  '--enable-demuxer=flv',
  '--enable-demuxer=matroska',
  '--enable-demuxer=m4v',
  '--enable-demuxer=mp3',
  '--enable-demuxer=mpc',
  '--enable-demuxer=mpc8',
  '--enable-demuxer=ogg',
  '--enable-demuxer=mov',
  '--enable-demuxer=pcm_alaw',
  '--enable-demuxer=pcm_mulaw',
  '--enable-demuxer=pcm_f64be',
  '--enable-demuxer=pcm_f64le',
  '--enable-demuxer=pcm_f32be',
  '--enable-demuxer=pcm_f32le',
  '--enable-demuxer=pcm_s32be',
  '--enable-demuxer=pcm_s32le',
  '--enable-demuxer=pcm_s24be',
  '--enable-demuxer=pcm_s24le',
  '--enable-demuxer=pcm_s16be',
  '--enable-demuxer=pcm_s16le',
  '--enable-demuxer=pcm_s8',
  '--enable-demuxer=pcm_u32be',
  '--enable-demuxer=pcm_u32le',
  '--enable-demuxer=pcm_u24be',
  '--enable-demuxer=pcm_u24le',
  '--enable-demuxer=pcm_u16be',
  '--enable-demuxer=pcm_u16le',
  '--enable-demuxer=pcm_u8',
  '--enable-demuxer=wav',
  '--enable-demuxer=wv',
  '--enable-demuxer=xwma',
  '--enable-demuxer=dsf',
  '--enable-decoder=aac',
  '--enable-decoder=aac_latm',
  '--enable-decoder=ac3',
  '--enable-decoder=alac',
  '--enable-decoder=als',
  '--enable-decoder=ape',
  '--enable-decoder=atrac1',
  '--enable-decoder=atrac3',
  '--enable-decoder=eac3',
  '--enable-decoder=flac',
  '--enable-decoder=mp1',
  '--enable-decoder=mp1float',
  '--enable-decoder=mp2',
  '--enable-decoder=mp2float',
  '--enable-decoder=mp3',
  '--enable-decoder=mp3adu',
  '--enable-decoder=mp3adufloat',
  '--enable-decoder=mp3float',
  '--enable-decoder=mp3on4',
  '--enable-decoder=mp3on4float',
  '--enable-decoder=mpc7',
  '--enable-decoder=mpc8',
  '--enable-decoder=opus',
  '--enable-decoder=vorbis',
  '--enable-decoder=wavpack',
  '--enable-decoder=wmalossless',
  '--enable-decoder=wmapro',
  '--enable-decoder=wmav1',
  '--enable-decoder=wmav2',
  '--enable-decoder=wmavoice',
  '--enable-decoder=pcm_alaw',
  '--enable-decoder=pcm_bluray',
  '--enable-decoder=pcm_dvd',
  '--enable-decoder=pcm_f32be',
  '--enable-decoder=pcm_f32le',
  '--enable-decoder=pcm_f64be',
  '--enable-decoder=pcm_f64le',
  '--enable-decoder=pcm_lxf',
  '--enable-decoder=pcm_mulaw',
  '--enable-decoder=pcm_s8',
  '--enable-decoder=pcm_s8_planar',
  '--enable-decoder=pcm_s16be',
  '--enable-decoder=pcm_s16be_planar',
  '--enable-decoder=pcm_s16le',
  '--enable-decoder=pcm_s16le_planar',
  '--enable-decoder=pcm_s24be',
  '--enable-decoder=pcm_s24daud',
  '--enable-decoder=pcm_s24le',
  '--enable-decoder=pcm_s24le_planar',
  '--enable-decoder=pcm_s32be',
  '--enable-decoder=pcm_s32le',
  '--enable-decoder=pcm_s32le_planar',
  '--enable-decoder=pcm_u8',
  '--enable-decoder=pcm_u16be',
  '--enable-decoder=pcm_u16le',
  '--enable-decoder=pcm_u24be',
  '--enable-decoder=pcm_u24le',
  '--enable-decoder=pcm_u32be',
  '--enable-decoder=pcm_u32le',
  '--enable-decoder=dsd_lsbf',
  '--enable-decoder=dsd_msbf',
  '--enable-decoder=dsd_lsbf_planar',
  '--enable-decoder=dsd_msbf_planar',
  '--enable-parser=aac',
  '--enable-parser=aac_latm',
  '--enable-parser=ac3',
  '--enable-parser=cook',
  '--enable-parser=dca',
  '--enable-parser=flac',
  '--enable-parser=mpegaudio',
  '--enable-parser=opus',
  '--enable-parser=vorbis',
  '--enable-muxer=adts',
  '--enable-muxer=flac',
  '--enable-muxer=ogg',
  '--enable-muxer=opus',
  '--enable-muxer=webm',
  '--enable-muxer=webp',
  '--enable-muxer=mov',
  '--enable-muxer=mp4',
  '--enable-encoder=aac',
  '--enable-encoder=alac',
  '--enable-encoder=flac',
  '--enable-encoder=mpeg4',
  '--enable-encoder=libopus',
  '--enable-encoder=wavpack',
  '--enable-encoder=wmav1',
  '--enable-encoder=wmav2',
  '--enable-encoder=libvorbis',
];

const curlOptions = [
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

function buildFfmpeg(sources, out, config) {
  const { env, jobs } = config;
  const codecPrefix = join(out, '..', '..', 'ffmpeg-codecs');
  const codecLib = join(codecPrefix, 'lib');

  /* Restrict codec discovery to the static build, including when cross-compiling. */
  const codecEnv = {
    ...env,
    PKG_CONFIG_PATH: '',
    PKG_CONFIG_LIBDIR: join(codecLib, 'pkgconfig'),
    PKG_CONFIG_SYSROOT_DIR: '',
  };

  for (const [name, options, cflags] of [
    ['ogg', [], ''],
    /* Vorbis enables fast math by default, which can introduce glibc vector-math calls. */
    ['vorbis', ['--with-ogg=' + codecPrefix], ' -fno-fast-math'],
    ['opus', ['--disable-doc', '--disable-extra-programs'], ' -O2'],
  ]) {
    console.log('Building static ' + name);
    const build = { cwd: sources[name], env: { ...codecEnv, CFLAGS: env.CFLAGS + cflags } };
    run('./configure', [
      '--prefix=' + codecPrefix,
      '--libdir=' + codecLib,
      '--disable-shared',
      '--enable-static',
      '--with-pic',
      ...options,
      ...config.configure
    ], build);
    run('make', [jobs], build);
    run('make', ['install'], build);
  }

  console.log('Building FFmpeg');
  /* Zig's old glibc stubs can expose sysctl even when the headers omit it. */
  const configurePath = join(sources.ffmpeg, 'configure');
  const configureScript = readFileSync(configurePath, 'utf8');
  const fixedConfigure = configureScript.replace(/^check_func +sysctl$/m, 'check_func_headers sys/sysctl.h sysctl');
  if (fixedConfigure !== configureScript) writeFileSync(configurePath, fixedConfigure);

  const ffmpegEnv = {
    ...codecEnv,
    LDFLAGS: [env.LDFLAGS, '-lm'].filter(Boolean).join(' '),
  };
  const build = { cwd: sources.ffmpeg, env: ffmpegEnv };
  run('./configure', [
    '--prefix=' + out,
    '--pkg-config=pkg-config',
    '--pkg-config-flags=--static',
    '--extra-ldflags=-L' + codecLib,
    ...ffmpegOptions,
    ...config.ffmpeg,
    '--build-suffix=-musikcube'
  ], build);
  run('make', [jobs], build);
  run('make', ['install'], build);
}

function buildLibraries(sources, out, config) {
  const { env, jobs } = config;
  const prefix = '--prefix=' + out;

  buildFfmpeg(sources, out, config);

  config.buildPrerequisites(sources);

  // curl uses the OpenSSL installation from this build.
  console.log('Building OpenSSL');
  run('perl', [
    './Configure',
    prefix,
    '--libdir=lib',
    'no-ssl3',
    'no-ssl3-method',
    'no-zlib',
    ...config.openssl
  ], { cwd: sources.openssl, env });
  run('make', ['-j8'], { cwd: sources.openssl, env });
  run('make', ['install_sw'], { cwd: sources.openssl, env });

  console.log('Building curl');
  run('./configure', [
    '--enable-shared',
    '--with-openssl=' + out,
    ...config.curl,
    ...curlOptions,
    ...config.configure,
    prefix
  ], { cwd: sources.curl, env });
  run('make', [jobs], { cwd: sources.curl, env });
  run('make', ['install'], { cwd: sources.curl, env });

  console.log('Building libopenmpt');
  if (mac) {
    run('./configure', [...libopenmptConfigureOptions, ...config.configure, prefix], { cwd: sources.libopenmpt, env });
    run('make', [jobs], { cwd: sources.libopenmpt, env });
    run('make', ['install'], { cwd: sources.libopenmpt, env });
  } else {
    const options = [...libopenmptMakeOptions, 'PREFIX=' + out, ...config.openmpt];
    const make = { cwd: sources.libopenmpt, env: config.makefileEnv };
    run('make', [...options, jobs, 'VERBOSE=1'], make);
    run('make', [...options, 'install'], make);
  }

  console.log('Building libmicrohttpd');
  run('./configure', [
    '--enable-shared',
    '--with-pic',
    '--enable-https=no',
    '--disable-curl',
    prefix,
    ...config.configure
  ], { cwd: sources.libmicrohttpd, env });
  run('make', ['-j8'], { cwd: sources.libmicrohttpd, env });
  run('make', ['install'], { cwd: sources.libmicrohttpd, env });

  console.log('Building LAME');
  run('./configure', [
    '--disable-dependency-tracking',
    '--disable-debug',
    '--enable-nasm',
    '--disable-frontend',
    '--disable-decoder',
    prefix,
    ...config.configure
  ], { cwd: sources.lame, env });
  run('make', [jobs], { cwd: sources.lame, env });
  run('make', ['install'], { cwd: sources.lame, env });

  for (const name of ['taglib', 'gme']) {
    console.log('Building ' + name);
    const options = name === 'gme' ? ['-DGME_ENABLE_UBSAN=OFF', '-DGME_BUILD_SHARED=ON',
      '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'] : [];
    run('cmake', [
      ...config.cmake,
      '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
      '-DCMAKE_INSTALL_PREFIX=' + out,
      '-DBUILD_SHARED_LIBS=1',
      ...options,
      '.'
    ], { cwd: sources[name], env });
    run('cmake', ['--build', '.', '--parallel', jobs.slice(2)], { cwd: sources[name], env });
    run('cmake', ['--install', '.'], { cwd: sources[name], env });
  }
}

async function main() {
  const args = process.argv.slice(2).filter((arg) => arg !== '--verbose');

  if (args.includes('--help') || args.includes('-h')) {
    console.log('Usage: node script/build-vendor-libraries.js [--verbose] [--compiler <name>]\n' +
      'Run from the standalone build directory.\n' +
      '  --verbose   Stream build command output\n' +
      `  --compiler  ${[...compilers.keys()].join(', ')}\n` +
      `              zig targets Linux x86-64 with glibc ${glibcVersion}\n` +
      '              Prompts when omitted. The suggested choice is CROSSCOMPILE or default.');
    return;
  }

  const compilerIndex = args.findIndex((arg) => arg === '--compiler' || arg.startsWith('--compiler='));
  let compiler;
  if (compilerIndex !== -1) {
    const [flag] = args.splice(compilerIndex, 1);
    compiler = flag === '--compiler' ? args.splice(compilerIndex, 1)[0] : flag.slice('--compiler='.length);
    if (!compiler || compiler.startsWith('--')) throw new Error('--compiler requires a name.');
  }

  if (args.length) throw new Error('Unknown Unix vendor build argument: ' + args[0]);
  if (!mac && process.platform !== 'linux') throw new Error('Unix vendor builds require Linux or macOS.');

  compiler = compiler ?? await promptCompiler();
  if (!compilers.has(compiler)) {
    throw new Error(`Unknown compiler "${compiler}". Choose from ${[...compilers.keys()].join(', ')}.`);
  }
  mkdirSync(scratch, { recursive: true });
  const work = mkdtempSync(join(buildDirectory, '.vendor-build-'));

  try {
    const vendor = join(work, 'vendor');
    const out = join(vendor, 'bin');
    const lib = join(out, 'lib');
    const config = await configuration(out, compiler);
    const destination = join(buildDirectory, 'vendor-' + config.variant);

    mkdirSync(vendor);
    writeFileSync(join(vendor, `.config-${config.arch}-${config.marker}`), '');

    console.log(`Building vendors for ${config.variant} (${config.jobs})`);
    printConfiguration(config);

    const sources = fetchPackages(vendor, config.packages);
    buildLibraries(sources, out, config);
    cleanLibraries(lib);
    run(process.execPath, [join(__dirname, 'relink-dynamic-libraries.js'), 'bin/lib'], { cwd: vendor });

    console.log('Checking vendor library dependencies');
    run(process.execPath, [
      join(__dirname, 'scan-standalone.js'),
      '--vendor',
      'bin/lib',
      ...config.audit,
    ], { cwd: vendor });

    const previous = join(work, 'previous');
    const replacing = existsSync(destination);
    if (replacing) renameSync(destination, previous);
    try {
      renameSync(vendor, destination);
    } catch (error) {
      if (replacing) renameSync(previous, destination);
      throw error;
    }

    console.log('Vendor libraries installed in ' + destination);
  } finally {
    rmSync(work, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
