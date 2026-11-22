/* checksum-pinned plugin linker inputs for each target. */
const amd64 = [
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

const debianArmhf = [
  /* alsaout */
  ['pool/main/a/alsa-lib/libasound2-dev_1.2.4-1.1_armhf.deb',
    '25e6feb216cc54bc1b9634604402ae80ea040f4e8dc8a92e314d9dc1c5a6062a'],
  ['pool/main/a/alsa-lib/libasound2_1.2.4-1.1_armhf.deb',
    '2b18bdd309f0f127c2ab4829028c7ad89ac27bb3bf42cfc118a5701225131528'],

  /* portaudioout */
  ['pool/main/p/portaudio19/portaudio19-dev_19.6.0-1.1_armhf.deb',
    '2cf990dedd3577fa7febc7386a2ac0e7e0977a2a337f2423dae434475dbe9625'],
  ['pool/main/p/portaudio19/libportaudio2_19.6.0-1.1_armhf.deb',
    '45b6e328bb818c347d4fdef151ebb874cd3944e468692d207eddeb1fe160e2bc'],

  /* pulseout */
  ['pool/main/p/pulseaudio/libpulse-dev_14.2-2_armhf.deb',
    'cd6f27055f6a5471573a42653ee5d9c32c2c5663f0d0fe17002f3b78f36ecf19'],
  ['pool/main/p/pulseaudio/libpulse0_14.2-2_armhf.deb',
    '495ed7c7a6765108019c1aa2e82e3b444ba0c649cb0f7b5f1b1242f068be3e9e'],

  /* sndioout */
  ['pool/main/s/sndio/libsndio-dev_1.5.0-3_armhf.deb',
    'e48c1fc83cb6e451ce4e73accb512bdaf7de4cb30fa1d045ee89f865dece7a4f'],
  ['pool/main/s/sndio/libsndio7.0_1.5.0-3_armhf.deb',
    '54e58d64051a6cb8bda95e3de21eeb1d8b7272e69bdf4b56b40343d7052ef1d7'],

  /* mpris */
  ['pool/main/s/systemd/libsystemd-dev_247.3-7+deb11u5_armhf.deb',
    '136f438ef5080edddaffd40602e59992c27a1b0d3e43db56d0a968fbc2e39a6b'],
  ['pool/main/s/systemd/libsystemd0_247.3-7+deb11u5_armhf.deb',
    '894c380fbc1d2e96d78b6a6c154a224d72b78671db9b376b3fc2ae620ae713ad'],

  /* pipewireout */
  ['pool/main/p/pipewire/libpipewire-0.3-dev_0.3.19-4_armhf.deb',
    'c902efca531411a6c6e87a165616d84517ae08f5c0dda7a9e1bc0f74ad0b0e61'],
  ['pool/main/p/pipewire/libpipewire-0.3-0_0.3.19-4_armhf.deb',
    'b0979da36ca23c29c977c6abeb62717caf338b701239f121fcdfadd3aea2a7b1'],
  ['pool/main/p/pipewire/libspa-0.2-dev_0.3.19-4_armhf.deb',
    '74f23cb82ab82556cb9ca75977c4044f1acd01b74380015eeeefd8dec0c0c363'],
];

const raspbianArmhf = [
  /* alsaout */
  ['pool/main/a/alsa-lib/libasound2-dev_1.2.4-1.1+deb11u1_armhf.deb',
    '2a600b35f0e20bc6cf3d34c83ab6b64e31b3f90c60d51f51689f58355408a60b'],
  ['pool/main/a/alsa-lib/libasound2_1.2.4-1.1+deb11u1_armhf.deb',
    'b102c8cd322fd7d2c4497146e98664acce22a32c20d6a606719fc9c5d7183410'],

  /* portaudioout */
  ['pool/main/p/portaudio19/portaudio19-dev_19.6.0-1.1_armhf.deb',
    'bd2cfbefe4025883df9dc98285a40f770c72ecabd3d2e645ee0757dc04a3bb3d'],
  ['pool/main/p/portaudio19/libportaudio2_19.6.0-1.1_armhf.deb',
    '2f9d8ad5a2c67a903def431172fc6d17f3b4a0011023712929a10df556df8e20'],

  /* pulseout */
  ['pool/main/p/pulseaudio/libpulse-dev_14.2-2_armhf.deb',
    'cd6f27055f6a5471573a42653ee5d9c32c2c5663f0d0fe17002f3b78f36ecf19'],
  ['pool/main/p/pulseaudio/libpulse0_14.2-2_armhf.deb',
    '119cf3849cecc57ba38f5455a4bb0f7bbb9df4698d50477790d32cb1b3f71e67'],

  /* sndioout */
  ['pool/main/s/sndio/libsndio-dev_1.5.0-3_armhf.deb',
    'e48c1fc83cb6e451ce4e73accb512bdaf7de4cb30fa1d045ee89f865dece7a4f'],
  ['pool/main/s/sndio/libsndio7.0_1.5.0-3_armhf.deb',
    '2234aa921b0e07abc21e1c48a3d37753ce358c82341c4c017e0f9c2aebc96d08'],

  /* mpris */
  ['pool/main/s/systemd/libsystemd-dev_247.3-7+rpi1+deb11u8_armhf.deb',
    'fff27f3ec0df7c7cf26e99ba5e6111bb4ddd292509b01bee331b1ac36aaa2675'],
  ['pool/main/s/systemd/libsystemd0_247.3-7+rpi1+deb11u8_armhf.deb',
    '8da43c2adaace329d39e31eed2b13150ca672bd04f4c5a9e4cf646203f8768ae'],

  /* pipewireout */
  ['pool/main/p/pipewire/libpipewire-0.3-dev_0.3.19-4_armhf.deb',
    'c902efca531411a6c6e87a165616d84517ae08f5c0dda7a9e1bc0f74ad0b0e61'],
  ['pool/main/p/pipewire/libpipewire-0.3-0_0.3.19-4_armhf.deb',
    '211c69ce26b103196ffef55a2024e5f2f199700e679bf3451da99597b60a0128'],
  ['pool/main/p/pipewire/libspa-0.2-dev_0.3.19-4_armhf.deb',
    '74f23cb82ab82556cb9ca75977c4044f1acd01b74380015eeeefd8dec0c0c363'],
];

const debian = 'https://archive.debian.org/debian';
const raspbian = 'https://archive.raspbian.org/raspbian';

module.exports = {
  amd64: { archive: debian, packages: amd64 },
  'debian-armhf': { archive: debian, packages: debianArmhf },
  'raspbian-armhf': { archive: raspbian, packages: raspbianArmhf, cachePrefix: 'raspbian-' },
};
