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

/* Header and linker inputs for the projectM visualizer. The GL runtime remains
provided by the desktop; these files are not copied into the standalone package. */
const projectmGraphics = {
  amd64: [
    ['pool/main/libg/libglvnd/libegl-dev_1.3.2-1_amd64.deb',
      '2847662b23487d5b1e467bca8cc8753baa880f794744a9b492c978bd5514b286'],
    ['pool/main/libg/libglvnd/libegl1_1.3.2-1_amd64.deb',
      '3a5583ebd7a9d8ad102484db9637c409561428d21345037b310c4ef2ca8e8837'],
    ['pool/main/libx/libx11/libx11-6_1.7.2-1+deb11u2_amd64.deb',
      '2b3b959cb10c07be065eb638a8577fe20f282045aaef76425dbd7310d1244b8c'],
    ['pool/main/libx/libxext/libxext-dev_1.3.3-1.1_amd64.deb',
      '0aa17565287ca8a37914e043789ee33c6e1f987acf346dac7175165009c5db7c'],
    ['pool/main/libx/libxext/libxext6_1.3.3-1.1_amd64.deb',
      'dc1ff8a2b60c7dd3c8917ffb9aa65ee6cda52648d9150608683c47319d1c0c8c'],
    ['pool/main/libx/libxkbcommon/libxkbcommon-dev_1.0.3-2_amd64.deb',
      '1202d8c64e670876b58f5b3d3f797b1848ec462f7caeef8b1e597ecea18570b6'],
    ['pool/main/libx/libxkbcommon/libxkbcommon0_1.0.3-2_amd64.deb',
      'd74d0b9f0a6641b44c279644c7ac627fa7a9b92350b7c6ff37da94352885bcfc'],
    ['pool/main/w/wayland/libwayland-client0_1.18.0-2~exp1.1_amd64.deb',
      '4baf16bb3a35823251453368ee078b6be6a14f97b05c19783b5acd4232a608ea'],
    ['pool/main/w/wayland/libwayland-cursor0_1.18.0-2~exp1.1_amd64.deb',
      '1b48d1d8e17a95b28a2876c7f2a95667ee1618a5f586d4dff05aeb09488172cb'],
    ['pool/main/w/wayland/libwayland-dev_1.18.0-2~exp1.1_amd64.deb',
      '3265bf05c0cea760d0e8f5fb5fc68b0f154911de23503e02232dfa59f6b6490c'],
    ['pool/main/w/wayland/libwayland-egl1_1.18.0-2~exp1.1_amd64.deb',
      'b98e636f08eca9e818e326fc8cd75810dbb50b1ed4e3586c2394e11248e29275'],
    ['pool/main/libg/libglvnd/libgl-dev_1.3.2-1_amd64.deb', 'a6487873f2706bbabf9346cdb190f47f23a1464f31cecf92c363bac37c342f2f'],
    ['pool/main/libg/libglvnd/libgl1_1.3.2-1_amd64.deb', 'f300f9610b5f05f1ce566c4095f1bf2170e512ac5d201c40d895b8fce29dec98'],
    ['pool/main/libg/libglvnd/libglx-dev_1.3.2-1_amd64.deb', '5a50549948bc4363eab32b1083dad2165402c3628f2ee85e9a32563228cc61c1'],
    ['pool/main/libg/libglvnd/libglx0_1.3.2-1_amd64.deb', 'cb642200f7e28e6dbb4075110a0b441880eeec35c8a00a2198c59c53309e5e17'],
    ['pool/main/libg/libglvnd/libglvnd-core-dev_1.3.2-1_amd64.deb', 'cb4f9333ec3df2a5107dc1fbf97ff4a3ac9ea3665449e263f22ae35c8c4cb079'],
    ['pool/main/libx/libx11/libx11-dev_1.7.2-1+deb11u2_amd64.deb', '9fbcda6af06d8db9cd8a48b581570aa8fee862bfc43b6f569a66503ae7d83211'],
  ],
  armhf: [
    ['pool/main/libx/libxcb/libxcb1_1.14-3_armhf.deb',
      '7b909481a169f17706eaf21da766e7564481b1c53476f204779b7f8ba27fe53e'],
    ['pool/main/libx/libxau/libxau6_1.0.9-1_armhf.deb',
      '5f9d08395ef6dd167541ccc5021b19bfe0b9fda860ac9556fdac9aee5252060e'],
    ['pool/main/libx/libxdmcp/libxdmcp6_1.1.2-3_armhf.deb',
      'ad47858edc6469f8288a13238ee53c12f5745636bf0dc12fce798db2718602bd'],
    ['pool/main/libb/libbsd/libbsd0_0.11.3-1+deb11u1_armhf.deb',
      'b1cd7014ce6c5f7305c37a6572a99a24d492fe1acce5faeb0466952c8ae3d7de'],
    ['pool/main/libm/libmd/libmd0_1.0.3-3_armhf.deb',
      'b57e87efd40daa649e1e004bb4da5f5c3360364524ef0b6a267874cdc85ac408'],
    ['pool/main/libg/libglvnd/libegl-dev_1.3.2-1_armhf.deb',
      '46335c070ad24f2d3380a704d37512f668c908e9da0445b2845592a54627f651'],
    ['pool/main/libg/libglvnd/libegl1_1.3.2-1_armhf.deb',
      'cda9c8a663dd88885136f168005b5a20128eb83ca5a17fc8abae46bb12d68e12'],
    ['pool/main/libx/libx11/libx11-6_1.7.2-1+deb11u2_armhf.deb',
      '0822c2e89b32832afd0b869202ff0eccb611e086a3b1054f2e2424c624228ca4'],
    ['pool/main/libx/libxext/libxext-dev_1.3.3-1.1_armhf.deb',
      '062b12fcf891a4ad8a1e84a01986331aea02672195d9f6a7e861a903b602739f'],
    ['pool/main/libx/libxext/libxext6_1.3.3-1.1_armhf.deb',
      '6248b14b7cda4f44be1ab8b14afff919bfafd2d285aed9479cc6055f5cf56728'],
    ['pool/main/libx/libxkbcommon/libxkbcommon-dev_1.0.3-2_armhf.deb',
      'ccdc00130fde8877699014be805a173f4d119e3e3dd2f9b20084db0b5d24449e'],
    ['pool/main/libx/libxkbcommon/libxkbcommon0_1.0.3-2_armhf.deb',
      'be427318714c3342de262f4cf8ff969463b5b649588b49efa23e9b56dea962a7'],
    ['pool/main/w/wayland/libwayland-client0_1.18.0-2~exp1.1_armhf.deb',
      '530ad65f86ce1396521ef67d17e5eb44f0216fb255fe2e240456cca7bd1e9ccc'],
    ['pool/main/w/wayland/libwayland-cursor0_1.18.0-2~exp1.1_armhf.deb',
      'd0513619949db9fa9da193ad3a1d550b897d2de309e5afdcb6eb084d259117f9'],
    ['pool/main/w/wayland/libwayland-dev_1.18.0-2~exp1.1_armhf.deb',
      '3b05df0581b0137a91b34ce6de4b949cd2fef94900128f154c521c5d17dc208f'],
    ['pool/main/w/wayland/libwayland-egl1_1.18.0-2~exp1.1_armhf.deb',
      '424321b1c66760af9dd2ecd347d283880c9136c2a8d249c1f09788da11968f45'],
    ['pool/main/libg/libglvnd/libgl-dev_1.3.2-1_armhf.deb', 'b8057b87ce48cc763328917dc7914bf287da1c06a60808595b9866134fb9a6a3'],
    ['pool/main/libg/libglvnd/libgl1_1.3.2-1_armhf.deb', '63a7d61dd76ef35b50568496860c0ca3c1a26ea7328cab055004975fb990c88d'],
    ['pool/main/libg/libglvnd/libglx-dev_1.3.2-1_armhf.deb', '788c63be76af5d86535044b8382fc856762132f4b9d55802856c2d94053d2cd2'],
    ['pool/main/libg/libglvnd/libglx0_1.3.2-1_armhf.deb', '068d78ff84c78db75f02a13c66f520394474a7dfd2282ad781651451d43cbb67'],
    ['pool/main/libg/libglvnd/libglvnd-core-dev_1.3.2-1_armhf.deb', 'ca1f859b302b9417b80f62027a42efeca9aed25af65864ea7f98638f91edf0a0'],
    ['pool/main/libx/libx11/libx11-dev_1.7.2-1+deb11u2_armhf.deb', '4bf859cb057adf53dbb0937169b2f7be79172b0c1ed1ab0cef02cf9a31f17e84'],
  ],
};
const projectmX11Headers = ['pool/main/x/xorgproto/x11proto-dev_2020.1-1_all.deb',
  'd5568d587d9ad2664c34c14b0ac538ccb3c567e126ee5291085a8de704a565f5'];

const debian = 'https://archive.debian.org/debian';
const raspbian = 'https://archive.raspbian.org/raspbian';

module.exports = {
  'projectm-libffi-amd64': { archive: debian, packages: [
    ['pool/main/libf/libffi/libffi-dev_3.3-6_amd64.deb', 'ca2c71d9c68b1944b689606f12acf8023bad1b5083e8413894fd41ad0b977d20'],
    ['pool/main/libf/libffi/libffi7_3.3-6_amd64.deb', '30ca89bfddae5fa6e0a2a044f22b6e50cd17c4bc6bc850c579819aeab7101f0f'],
  ] },
  'projectm-libffi-armhf': { archive: debian, packages: [
    ['pool/main/libf/libffi/libffi-dev_3.3-6_armhf.deb', 'beaee29c2f9dc7149549f39938abede8f230ebb9c16ec25a7fdc6a7e790eba0f'],
    ['pool/main/libf/libffi/libffi7_3.3-6_armhf.deb', 'c18d4ad579a4e88f3da20eb523ecc5a850a61bf37091b316c9d3f668cf9838fa'],
  ] },
  'projectm-libffi-raspbian-armhf': { archive: raspbian, cachePrefix: 'raspbian-', packages: [
    ['pool/main/libf/libffi/libffi-dev_3.3-6_armhf.deb', 'c31745d547be78fab5ac58af4e6367233b7f8c6a63c4871e1073662b291fef8d'],
    ['pool/main/libf/libffi/libffi7_3.3-6_armhf.deb', '0b7208c2864ec8d523839b2fd89b5dc3a1bf03ce9a53cb464b88656cf5866053'],
  ] },
  'projectm-amd64': { archive: debian, packages: [...projectmGraphics.amd64, projectmX11Headers] },
  'projectm-armhf': { archive: debian, packages: [...projectmGraphics.armhf, projectmX11Headers] },
  amd64: { archive: debian, packages: amd64 },
  'debian-armhf': { archive: debian, packages: debianArmhf },
  'raspbian-armhf': { archive: raspbian, packages: raspbianArmhf, cachePrefix: 'raspbian-' },
};
