const { existsSync, readdirSync, renameSync } = require('node:fs');
const { join } = require('node:path');

/* CPack treats unmatched square brackets as list syntax in filenames. */
function sanitize(directory) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const source = join(directory, entry.name);
    const destination = join(directory, entry.name.replaceAll('[', '(').replaceAll(']', ')'));
    if (source !== destination) {
      if (existsSync(destination)) throw new Error(`Preset filename collision: ${destination}`);
      renameSync(source, destination);
    }
    if (entry.isDirectory()) sanitize(destination);
  }
}

module.exports = sanitize;
