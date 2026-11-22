/* Build-and-test entry point. Configures the selected CMake build, builds its
   test targets, and invokes CTest. Individual native tests contain the assertions. */

const { spawnSync } = require('child_process');
const { resolve } = require('path');
const { parseArgs } = require('util');

const source = resolve(__dirname, '../..');

/* Run a tool with inherited diagnostics, optionally capturing a test listing
   so unsupported or empty selections cannot report a false successful run. */
function run(command, args, cwd, capture = false) {
  const result = spawnSync(command, args, {
    cwd, stdio: capture ? ['ignore', 'pipe', 'inherit'] : 'inherit', encoding: 'utf8',
  });

  if (result.error) throw result.error;
  if (result.signal) process.kill(process.pid, result.signal);
  if (result.status !== 0) {
    if (capture && result.stdout) {
      process.stderr.write(result.stdout);
    }
    process.exit(result.status ?? 1);
  }
  return result.stdout;
}

/* Configure the selected build, compile test targets, then forward selection arguments to CTest. */
function main() {
  const { values, positionals } = parseArgs({
    options: {
      'build-dir': { type: 'string' },
      config: { type: 'string', default: 'Debug' },
      'abi-only': { type: 'boolean', default: false },
      suite: { type: 'string', default: 'all' },
      jobs: { type: 'string', default: '6' },
      list: { type: 'boolean', default: false },
      help: { type: 'boolean', short: 'h' },
    },
    allowPositionals: true,
  });

  /* Help exits before configuring a build or changing its cached options. */
  if (values.help) {
    console.log('Usage: node tests/scripts/run-tests.js [--build-dir <path>] [--config <name>] [--abi-only] [--suite unit|integration|ui|all] [--jobs <count>] [--list] [-- <ctest args>]');
    return;
  }

  if (!['unit', 'integration', 'ui', 'all'].includes(values.suite)) {
    throw new Error('Choose suite unit, integration, ui, or all');
  }
  if (values['abi-only'] && values.suite === 'ui') {
    throw new Error('UI scenarios require a full build; omit --abi-only');
  }
  const jobs = Number(values.jobs);
  if (!Number.isSafeInteger(jobs) || jobs < 1) {
    throw new Error('Use a positive integer for --jobs');
  }
  const build = values['build-dir'] === undefined
    ? resolve(source, 'build', `tests-${values.config.toLowerCase()}`)
    : resolve(values['build-dir']);

  /* Keep Debug/Release selection and runtime placement consistent across
     configure, build, and CTest; ABI-only mode avoids production dependencies. */
  const preset = process.platform === 'win32' ? ['--preset', 'windows-x64'] : [];
  run('cmake', [...preset, '-S', source, '-B', build, '-DBUILD_TESTING=ON',
    `-DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY=${resolve(build, 'bin')}`,
    `-DCMAKE_BUILD_TYPE=${values.config}`, `-DMUSIKCORE_ABI_ONLY=${values['abi-only'] ? 'ON' : 'OFF'}`], source);

  const selection = values.suite === 'all' ? [] : ['-L', `^${values.suite}$`];
  if (values.suite !== 'all') {
    const listing = run('ctest', ['-C', values.config, '--show-only', ...selection], build, true);
    if (/Total Tests:\s*0\s*$/m.test(listing)) {
      throw new Error(`No ${values.suite} tests are available in this build`);
    }
  }

  if (!values.list) {
    const target = values.suite === 'all' ? 'musikcube_tests' : `musikcube_${values.suite}_tests`;
    run('cmake', ['--build', build, '--parallel', String(jobs), '--config', values.config, '--target', target], source);
  }

  /* Level labels are mutually exclusive; component/contract labels remain
     available through ordinary CTest arguments after --. */
  run('ctest', ['-C', values.config, '--output-on-failure', ...selection,
    ...(values.list ? ['--show-only'] : []), ...positionals], build);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
