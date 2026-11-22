const { createHash } = require('crypto');
const { mkdtempSync, readFileSync, rmSync } = require('fs');
const { basename, join } = require('path');

const manifests = require('./debian-plugin-manifests');
const extractOptions = [
  '--exclude=./usr/share',
  /* These linker symlinks belong to client libraries the plugins never use. */
  '--exclude=./usr/lib/*/libportaudiocpp.so',
  '--exclude=./usr/lib/*/libpulse-mainloop-glib.so',
];

exports.stage = function({ destination, download, run, work, target = 'amd64' }) {
  const { archive, packages, cachePrefix = '' } = manifests[target];
  console.log('Staging Debian plugin dependencies');
  const scratch = mkdtempSync(join(work, 'debian-'));

  try {
    for (const [path, expected] of packages) {
      const file = download(archive + '/' + path, cachePrefix + basename(path));
      const actual = createHash('sha256').update(readFileSync(file)).digest('hex');
      if (actual !== expected) {
        throw new Error(`Checksum mismatch for ${file}`);
      }

      /* Extract the data archive without running package scripts. */
      const payload = run('ar', ['t', file], { capture: true })
        .split('\n').find((entry) => /^data\.tar(\.|$)/.test(entry));
      if (!payload) throw new Error(`Missing data archive in ${file}`);

      run('ar', ['x', file, payload], { cwd: scratch });
      run('tar', ['-xf', join(scratch, payload), '-C', destination, ...extractOptions]);
    }
  } finally {
    rmSync(scratch, { recursive: true, force: true });
  }
};
