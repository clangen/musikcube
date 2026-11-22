const { readFileSync, writeFileSync } = require('fs');
const { join } = require('path');

const ffmpeg = 'ffmpeg-8.0.3';
const ogg = 'libogg-1.3.6';
const vorbis = 'libvorbis-1.3.7';
const opus = 'opus-1.6.1';

exports.packages = {
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
};

const ffmpegOptions = [
  '--pkg-config=pkg-config',
  '--pkg-config-flags=--static',
  '--build-suffix=-musikcube',
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

const staticCodecOptions = ['--disable-shared', '--enable-static', '--with-pic'];

const codecBuilds = (prefix) => [
  ['ogg', [], ''],
  /* Vorbis enables fast math by default, which can introduce glibc vector-math calls. */
  ['vorbis', ['--with-ogg=' + prefix], ' -fno-fast-math'],
  ['opus', ['--disable-doc', '--disable-extra-programs'], ''],
];

function patchVorbisConfigure(source) {
  if (process.platform !== 'darwin') return;

  /* libvorbis replaces the caller's CFLAGS on Darwin and adds this obsolete
   * linker option. It was accepted by older Apple linkers, but was removed
   * from the linker shipped with current Command Line Tools. */
  const configure = join(source, 'configure');
  const script = readFileSync(configure, 'utf8');
  const fixed = script.replaceAll(' -force_cpusubtype_ALL', '');
  if (fixed !== script) writeFileSync(configure, fixed);
}

exports.build = function({ sources, out, config, run }) {
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

  /* Build the codec archives before configuring FFmpeg. */
  for (const [name, options, cflags] of codecBuilds(codecPrefix)) {
    console.log('Building static ' + name);
    if (name === 'vorbis') patchVorbisConfigure(sources[name]);
    const extra = name === 'opus' ? config.opus ?? [] : [];
    const build = { cwd: sources[name], env: { ...codecEnv, CFLAGS: env.CFLAGS + cflags } };
    run('./configure', [
      '--prefix=' + codecPrefix,
      '--libdir=' + codecLib,
      ...staticCodecOptions,
      ...options,
      ...extra,
      ...config.configure
    ], build);
    run('make', [jobs], build);
    run('make', ['install'], build);
  }

  console.log('Building FFmpeg');
  /* Zig's old glibc stubs can expose sysctl even when the headers omit it. */
  const configurePath = join(sources.ffmpeg, 'configure');
  const configureScript = readFileSync(configurePath, 'utf8');
  const fixedConfigure = configureScript.replace(
    /^check_func +sysctl$/m,
    'check_func_headers sys/sysctl.h sysctl'
  );
  if (fixedConfigure !== configureScript) writeFileSync(configurePath, fixedConfigure);

  const ffmpegEnv = {
    ...codecEnv,
    LDFLAGS: [env.LDFLAGS, '-lm'].filter(Boolean).join(' '),
  };
  const build = { cwd: sources.ffmpeg, env: ffmpegEnv };
  run('./configure', [
    '--prefix=' + out,
    '--extra-ldflags=-L' + codecLib,
    ...ffmpegOptions,
    ...config.ffmpeg
  ], build);
  run('make', [jobs], build);
  run('make', ['install'], build);
};
