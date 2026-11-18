/* Generate the x64 projects under build/ and write a root solution that points to them. */

const { spawnSync } = require('node:child_process');
const { existsSync, readFileSync, writeFileSync } = require('node:fs');
const { join, relative, resolve } = require('node:path');

const root = resolve(__dirname, '..');
const build = join(root, 'build', 'windows-x64');

const result = spawnSync('cmake.exe', ['--preset', 'windows-x64'], {
  cwd: root,
  stdio: 'inherit',
});

if (result.error) {
  console.error('Could not run CMake. Run this from an x64 Developer PowerShell for Visual Studio.');
  process.exit(1);
}
if (result.status !== 0) process.exit(result.status ?? 1);

/* Use the solution format emitted by the installed CMake generator. */
const filename = ['musikcube.slnx', 'musikcube.sln'].find(name => existsSync(join(build, name)));
if (!filename) {
  console.error('CMake did not generate a musikcube solution in ' + build);
  process.exit(1);
}

const generated = readFileSync(join(build, filename), 'utf8');
const project = filename.endsWith('.slnx')
  ? /(<Project Path="|<BuildDependency Project=")([^"]+\.vcxproj)"/gi
  : /^(Project\("[^"]+"\) = "[^"]+", ")([^"]+\.vcxproj)"/gim;
/* SLNX dependencies refer to projects by path, so rebase those references too. */
const solution = generated.replace(project, (_, prefix, path) =>
  prefix + relative(root, resolve(build, path)) + '"',
);

writeFileSync(join(root, filename), solution);
console.log('Open ' + join(root, filename) + ' in Visual Studio.');
