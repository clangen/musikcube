const buildCmake = require('./build-with-cmake');

const version = '0.6.4';
const gme = 'game-music-emu-' + version;

exports.packages = {
  gme: [
    gme,
    gme + '.tar.gz',
    `https://codeload.github.com/libgme/game-music-emu/tar.gz/refs/tags/${version}`
  ],
};

const configureOptions = [
  '-DGME_ENABLE_UBSAN=OFF',
  '-DGME_BUILD_SHARED=ON',
  '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
];

exports.build = function(context) {
  console.log('Building gme');
  buildCmake(context, 'gme', configureOptions);
};
