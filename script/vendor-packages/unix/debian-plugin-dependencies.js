const { createHash } = require('crypto');
const { mkdtempSync, readFileSync, rmSync } = require('fs');
const { basename, join } = require('path');

const archive = 'https://archive.debian.org/debian';
const extractOptions = [
  '--exclude=./usr/share',
  /* These linker symlinks belong to client libraries the plugins never use. */
  '--exclude=./usr/lib/*/libportaudiocpp.so',
  '--exclude=./usr/lib/*/libpulse-mainloop-glib.so',
];

const packages = [
  /* alsaout */
  ['pool/main/a/alsa-lib/libasound2-dev_1.1.8-1_amd64.deb',
    'efcae0522800ac0f32a72d7ac240375effde06473bb5aeabbe20d2d4057c185e'],
  ['pool/main/a/alsa-lib/libasound2_1.1.8-1_amd64.deb',
    '6cc281b4a6d1faffe4fc6d83ec71365c1af0ee6d7806fa122fef00f85a0dde62'],

  /* portaudioout */
  ['pool/main/p/portaudio19/portaudio19-dev_19.6.0-1+deb10u1_amd64.deb',
    '5f49c701a7258331e5503452020043845c53e9a0d8603852f652943c299dc0a8'],
  ['pool/main/p/portaudio19/libportaudio2_19.6.0-1+deb10u1_amd64.deb',
    '16cba5bbfc5d69b45e368c6ec03096f123462778ea046501e1af6869dd8cc278'],

  /* pulseout */
  ['pool/main/p/pulseaudio/libpulse-dev_12.2-4+deb10u1_amd64.deb',
    '6aa5973e00ac561ee1219983d78c40fa6341fe7e4ce25acc0b047e297f9a8095'],
  ['pool/main/p/pulseaudio/libpulse0_12.2-4+deb10u1_amd64.deb',
    'a7edaf4424894b0e83c81d37c26ad351daa76d74f0e01e750c6679ae74c59362'],

  /* sndioout */
  ['pool/main/s/sndio/libsndio-dev_1.5.0-3_amd64.deb',
    'f04c78a7d1de819a984a247ab2b34362d96f3f2e148d0fb7b55d7f248133db26'],
  ['pool/main/s/sndio/libsndio7.0_1.5.0-3_amd64.deb',
    'af3b9425fca1abad25be51774fec175e75f1b92f7a1909e9c27bbbf644c23550'],

  /* mpris */
  ['pool/main/s/systemd/libsystemd-dev_241-7~deb10u8_amd64.deb',
    '35b5884696b29166e9b42efa79342659ecc2a18b25dea02c86d7853ac1bc9745'],
  ['pool/main/s/systemd/libsystemd0_241-7~deb10u8_amd64.deb',
    'fadce8dbc36955ac93ece6ab2516f927c85480df9419a578c95c388834b4980e'],

  /* pipewireout uses Bullseye's PipeWire 0.3 client library for glibc 2.28. */
  ['pool/main/p/pipewire/libpipewire-0.3-dev_0.3.19-4_amd64.deb',
    'd0a65574c4ccfd3946298ba9479fb77414d26a5ccb98db823be4223b91021da2'],
  ['pool/main/p/pipewire/libpipewire-0.3-0_0.3.19-4_amd64.deb',
    'dfef17583c3b00920182d58212e03b5f3275b16f6bab596865c64d93d8011c93'],
  ['pool/main/p/pipewire/libspa-0.2-dev_0.3.19-4_amd64.deb',
    'e0703818015ff8f017596c2a13c6070a7ba9d59e02fa3cfa8a1139a04bac3567'],
];

exports.stage = function({ destination, download, run, work }) {
  console.log('Staging Debian plugin dependencies');
  const scratch = mkdtempSync(join(work, 'debian-'));

  try {
    for (const [path, expected] of packages) {
      const file = download(archive + '/' + path, basename(path));
      const actual = createHash('sha256').update(readFileSync(file)).digest('hex');
      if (actual !== expected) {
        throw new Error(`Checksum mismatch for ${file}`);
      }

      /* Extract the data archive without running package scripts. */
      const payload = run('ar', ['t', file], { capture: true })
        .split('\n').find((entry) => /^data\.tar(\.|$)/.test(entry));
      if (!payload) throw new Error(`Missing data archive in ${file}`);

      run('ar', ['x', file, payload], { cwd: scratch });
      run('tar', ['-xf', join(scratch, payload), '-C', destination, ...extractOptions]);
    }
  } finally {
    rmSync(scratch, { recursive: true, force: true });
  }
};
