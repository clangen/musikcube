/* Orchestrate native API benchmarks and collect timing/allocation measurements.
   Runs the standalone C boundary and optionally real local/loopback backends,
   then records results or compares them with a machine-specific baseline. */

const { spawnSync } = require('child_process');
const { resolve, join, dirname } = require('path');
const { readFileSync, writeFileSync } = require('fs');
const os = require('os');
const { parseArgs } = require('util');
const source = resolve(__dirname, '../..');

/* Run one isolated baseline helper; capture measurement text when requested while preserving failure
   diagnostics. */
function run(args, env = process.env, capture = false) {
  const result = spawnSync(process.execPath, args, {
    cwd: source,
    stdio: capture ? ['ignore', 'pipe', 'inherit'] : 'inherit',
    env,
    encoding: 'utf8',
  });

  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`Baseline command failed: ${result.signal || result.status}`);

  if (capture) process.stdout.write(result.stdout);
  return result.stdout || '';
}

/* Merge timing and profiling lines by operation name into the machine-readable baseline report. */
function measurements(output) {
  const values = {};

  for (const line of output.split('\n')) {
    const match = /^(?:profile )?([a-z0-9_]+): (.*)$/.exec(line);
    if (!match) continue;

    const entry = values[match[1]] ||= {};
    for (const field of match[2].matchAll(/([a-z0-9_]+)=(-?[\d.]+)/g)) {
      entry[field[1]] = Number(field[2]);
    }
  }

  return values;
}

try {
  /* Choose the build and measurement mode before launching any fixture. */
  const { values } = parseArgs({
    options: {
      'build-dir': { type: 'string', default: join(source, 'build/core-baseline') },
      config: { type: 'string', default: 'Release' },
      'production-build-dir': { type: 'string' },
      record: { type: 'string' },
      compare: { type: 'string' },
      'timing-only': { type: 'boolean', default: false },
      help: { type: 'boolean' },
    },
  });

  if (values.help) {
    console.log('Usage: node tests/scripts/run-baseline.js [--build-dir <dir>] [--config <name>] [--production-build-dir <configured dir>] [--timing-only] [--record <json>] [--compare <json>]');
  } else {
    /* Verify the public boundary first, then measure a fresh synthetic catalog. */
    const build = resolve(values['build-dir']);
    run([join(__dirname, 'run-tests.js'), '--build-dir', build, '--config', values.config, '--abi-only']);

    const binary = process.platform === 'win32'
      ? join(build, 'bin/abi0', values.config, 'musikcore_api_baseline.exe')
      : join(build, 'bin/abi0/musikcore_api_baseline');

    /* The injected helper observes dynamic allocations/copies; omit it for
       independent latency measurements without instrumentation overhead. */
    const env = { ...process.env };
    if (process.platform === 'darwin' && !values['timing-only']) {
      env.DYLD_INSERT_LIBRARIES = join(build, 'tests/bin/libmusikcore_test_profile.dylib');
    }

    let output = run([join(__dirname, 'run-isolated-test.js'), join(build, 'tests/bin/baseline'), binary, '@origin'], env, true);

    if (values['production-build-dir']) {
      /* Match configurations before measuring real local/loopback backends. */
      const production = resolve(values['production-build-dir']);
      const cache = readFileSync(join(production, 'CMakeCache.txt'), 'utf8');
      const configured = /^CMAKE_BUILD_TYPE:STRING=(.+)$/m.exec(cache)?.[1];
      if (configured && configured !== values.config) throw new Error(`Production build is ${configured}, expected ${values.config}`);

      const rebuilt = spawnSync('cmake', ['--build', production, '--config', values.config,
        '--target', 'musikcube_tests', '--parallel', '6'], { cwd: source, stdio: 'inherit' });
      if (rebuilt.error || rebuilt.status !== 0) throw new Error('Could not build production baseline fixtures');

      /* Resolve the production runtime and seed its private two-track catalog. */
      const runtime = process.platform === 'win32' ? join(source, 'bin64', values.config)
        : /^MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY:PATH=(.+)$/m.exec(cache)?.[1] || join(source, 'bin');
      const extension = process.platform === 'win32' ? '.exe' : '';

      const seeded = run([join(__dirname, 'seed-test-library.js'), '--indexer',
        join(runtime, `musikcube_seed_library${extension}`), production], process.env, true);
      const database = seeded.trimEnd().split(/\r?\n/).pop();
      const config = resolve(dirname(database), '../..');

      /* Reuse the seeded catalog for both backends and 10,000-entry playlists. */
      output += run([join(__dirname, 'run-isolated-test.js'), join(production, 'tests/bin/baseline'),
        join(runtime, `musikcore_remote_query_transport_test${extension}`), '--stage', '--config-root', config, '--parity'], env, true);
    }

    /* Store machine/build identity beside metrics so incompatible runs cannot
       accidentally be treated as performance regressions. */
    const report = {
      platform: process.platform,
      arch: process.arch,
      cpu: os.cpus()[0]?.model,
      instrumentation: process.platform === 'darwin' && !values['timing-only'] ? 'dyld-allocator-copy' : 'none',
      config: values.config,
      metrics: measurements(output),
    };

    if (values.record) writeFileSync(resolve(values.record), JSON.stringify(report, null, 2) + '\n');

    if (values.compare) {
      const previous = JSON.parse(readFileSync(resolve(values.compare), 'utf8'));
      if (previous.platform !== report.platform || previous.arch !== report.arch || previous.config !== report.config ||
          previous.cpu !== report.cpu || previous.instrumentation !== report.instrumentation) {
        throw new Error('Compare equivalent platform, architecture, and build configurations.');
      }

      /* Require every recorded measurement and accumulate failures so one run
         reports all breached budgets rather than only the first one. */
      const failures = [];
      for (const [operation, expected] of Object.entries(previous.metrics)) {
        const actual = report.metrics[operation];
        if (!actual) {
          failures.push(`${operation}: missing measurement`);
          continue;
        }

        for (const [field, value] of Object.entries(expected)) {
          /* Timing needs headroom for scheduling; allocation/copy budgets catch
             substantial accidental duplication. Zero-cost access stays zero. */
          const time = field.endsWith('_us');
          const multiplier = field === 'p95_us' ? 2 : 1.5;
          const allowance = time ? (field === 'p95_us' ? 100 : 50) : value === 0 ? 0 : 64;

          /* Dataset descriptors are recorded for context, not timing budgets. */
          if (field === 'samples' || field === 'rows' || field === 'payload_bytes') continue;

          if (!(field in actual)) {
            failures.push(`${operation}.${field}: missing measurement`);
            continue;
          }

          if (actual[field] > value * multiplier + allowance) {
            failures.push(`${operation}.${field}: ${actual[field]} exceeds ${value * multiplier + allowance}`);
          }
        }
      }

      if (failures.length) throw new Error(`Baseline regression:\n${failures.join('\n')}`);
      console.log('Baseline comparison passed.');
    }
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
