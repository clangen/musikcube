const buildCmake = require('./build-with-cmake');

const version = '2.3.2';
const taglib = 'taglib-' + version;

exports.packages = {
  taglib: [
    taglib,
    taglib + '.tar.gz',
    `https://github.com/taglib/taglib/releases/download/v${version}/${taglib}.tar.gz`
  ],
};

exports.build = function(context) {
  console.log('Building taglib');
  buildCmake(context, 'taglib');
};
