/* validate both sides of the sanitizer unload correction: normal reloads pass,
but a real global overflow after reload is still rejected by AddressSanitizer. */
const { spawnSync } = require('child_process');
const [host, first, second] = process.argv.slice(2);

/* preserve diagnostics from unexpected outcomes without printing the expected
sanitizer failure during a successful test run. */
function run(extra = []) {
  const result = spawnSync(host, [first, second, ...extra], { encoding: 'utf8', timeout: 20000 });
  if (result.error) {
    throw result.error;
  }
  return result;
}

const valid = run();
if (valid.status !== 0) {
  throw new Error(`Sanitizer reload failed: ${valid.stderr}`);
}
const invalid = run(['overflow']);
if (invalid.status === 0 || !invalid.stderr.includes('AddressSanitizer: global-buffer-overflow')) {
  throw new Error(`Global overflow was not diagnosed: ${invalid.stderr}`);
}
