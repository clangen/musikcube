const configureOptions = ['-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DBUILD_SHARED_LIBS=1'];

module.exports = function buildCmake({ sources, out, config, run }, name, options = []) {
  const { env, jobs } = config;

  run('cmake', [
    ...config.cmake,
    ...configureOptions,
    '-DCMAKE_INSTALL_PREFIX=' + out,
    ...options,
    '.'
  ], { cwd: sources[name], env });
  run('cmake', ['--build', '.', '--parallel', jobs.slice(2)], { cwd: sources[name], env });
  run('cmake', ['--install', '.'], { cwd: sources[name], env });
};
