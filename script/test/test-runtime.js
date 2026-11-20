const { copyFileSync, mkdirSync, readdirSync } = require('fs');
const { basename, dirname, join } = require('path');

/* Stage only test-required plugins; never discover unrelated installed plugins. */
function stageTestRuntime(executable, root) {
  const runtime = join(root, 'runtime');
  mkdirSync(join(runtime, 'plugins'), { recursive: true });
  const source = dirname(executable);
  const staged = join(runtime, basename(executable));
  copyFileSync(executable, staged);
  for (const entry of readdirSync(source, { withFileTypes: true })) {
    if (!entry.isDirectory() && /\.(dll|dylib|so)(\.[\d.]+)?$/.test(entry.name)) {
      copyFileSync(join(source, entry.name), join(runtime, entry.name));
    }
  }
  for (const entry of readdirSync(join(source, 'plugins'), { withFileTypes: true })) {
    if (!entry.isDirectory() && /^(lib)?(taglibreader|ffmpegdecoder)\.(dll|dylib|so)$/.test(entry.name)) {
      copyFileSync(join(source, 'plugins', entry.name), join(runtime, 'plugins', entry.name));
    }
  }
  return staged;
}
module.exports = { stageTestRuntime };
