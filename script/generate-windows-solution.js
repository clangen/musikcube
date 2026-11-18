// Generate the x64 projects under build/ and write a root solution that points to them.

const { spawnSync } = require('node:child_process');
const { readFileSync, writeFileSync } = require('node:fs');
const { join, relative, resolve } = require('node:path');

const root = resolve(__dirname, '..');
const build = join(root, 'build', 'windows-x64');
const filename = 'musikcube.sln';

const result = spawnSync('cmake.exe', ['--preset', 'windows-x64'], {
  cwd: root,
  stdio: 'inherit',
});

if (result.error) {
  console.error('Could not run CMake. Run this from an x64 Developer PowerShell for Visual Studio.');
  process.exit(1);
}
if (result.status !== 0) process.exit(result.status ?? 1);

const generated = readFileSync(join(build, filename), 'utf8');
const solution = generated.replace(
  /^(Project\("[^"]+"\) = "[^"]+", ")([^"]+\.vcxproj)"/gim,
  (_, prefix, project) => prefix + relative(root, resolve(build, project)) + '"',
);

writeFileSync(join(root, filename), solution);
console.log('Open ' + join(root, filename) + ' in Visual Studio.');
