/* Shared runtime-staging helper for native integration fixtures. Copy the
   executable, runtime libraries, and required plugins into a private directory
   so tests do not discover unrelated plugins from the normal app installation. */

const { copyFileSync, existsSync, mkdirSync, readdirSync } = require('fs');
const { basename, dirname, join } = require('path');

function copyRuntimeLibraries(source, destination) {
  mkdirSync(destination, { recursive: true });
  for (const entry of readdirSync(source, { withFileTypes: true })) {
    if (!entry.isDirectory() && /\.(dll|dylib|so)(\.[\d.]+)?$/i.test(entry.name)) {
      copyFileSync(join(source, entry.name), join(destination, entry.name));
    }
  }
}

/* Copy an executable and its runtime libraries into an isolated root, staging only the required test
   plugins. */
function stageTestRuntime(executable, root, includeDefaultPlugins = true) {
  const runtime = join(root, 'runtime');
  mkdirSync(join(runtime, 'plugins'), { recursive: true });

  const source = dirname(executable);
  const staged = join(runtime, basename(executable));
  copyFileSync(executable, staged);

  /* Preserve the loader paths used by the executable and its plugins. */
  copyRuntimeLibraries(source, runtime);
  const libraries = join(source, 'lib');
  if (existsSync(libraries)) {
    copyRuntimeLibraries(libraries, join(runtime, 'lib'));
  }

  /* An allowlist prevents unrelated output/device plugins from affecting tests. */
  const plugins = join(source, 'plugins');
  for (const entry of includeDefaultPlugins && existsSync(plugins) ? readdirSync(plugins, { withFileTypes: true }) : []) {
    if (!entry.isDirectory() && /^(lib)?(taglibreader|taglib_plugin|ffmpegdecoder)\.(dll|dylib|so)$/.test(entry.name)) {
      copyFileSync(join(source, 'plugins', entry.name), join(runtime, 'plugins', entry.name));
    }
  }

  return staged;
}

module.exports = { copyRuntimeLibraries, stageTestRuntime };
