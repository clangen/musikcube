const { spawnSync } = require('child_process');
const { join } = require('path');

/* Forward the original arguments to the platform builder. */
const platform = process.platform === 'win32' ? 'windows' : 'unix';
const script = join(__dirname, `build-vendor-libraries-${platform}.js`);
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
