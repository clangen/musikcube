/* Forward archive options to the platform implementation. */
const { spawnSync } = require('node:child_process');
const { join } = require('node:path');

const platform = process.platform === 'win32' ? 'windows' : 'unix';
const script = join(__dirname, `build-and-archive-${platform}.js`);
const result = spawnSync(process.execPath, [script, ...process.argv.slice(2)], {
  stdio: 'inherit',
});

if (result.error) {
  console.error(result.error.message);
  process.exitCode = 1;
} else if (result.signal) {
  process.kill(process.pid, result.signal);
} else {
  process.exitCode = result.status;
}
