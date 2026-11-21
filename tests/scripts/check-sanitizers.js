/* run deliberate faults in child processes: a passing Debug test must prove
that its compiler/runtime detects errors, not merely that flags were configured. */
const { spawnSync } = require('child_process');
const [probe, ...sanitizers] = process.argv.slice(2);

/* bound each child and retain unexpected diagnostics without printing the
expected sanitizer reports during a successful test run. */
function run(mode, expected) {
  const result = spawnSync(probe, [mode], { encoding: 'utf8', timeout: 10000 });
  if (result.error) {
    throw result.error;
  }

  const passed = expected
    ? (result.status !== 0 || result.signal) && result.stderr.includes(expected)
    : result.status === 0 && !result.signal;

  if (!passed) {
    throw new Error(`${mode}: unexpected sanitizer result (${result.status ?? result.signal})\n${result.stderr}`);
  }
}

run('valid');
if (sanitizers.includes('address')) {
  run('address', 'AddressSanitizer: heap-buffer-overflow');
}
if (sanitizers.includes('undefined')) {
  run('undefined', 'runtime error: signed integer overflow');
}
