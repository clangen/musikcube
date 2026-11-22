/* Exercise Windows console input, output, and resize through the real app. */
const fs = require('node:fs');
const { execFileSync } = require('node:child_process');
const { dirname, join, resolve } = require('node:path');
const { parseArgs } = require('node:util');
const { Terminal } = require('../support/terminal');
const { withRuntime } = require('../support/artifacts');
const { stageTestRuntime } = require('../../scripts/test-runtime');
const { FIRST_TRACK } = require('../support/catalog');

async function main() {
  const { positionals, values } = parseArgs({ allowPositionals: true, options: {
    screenshots: { type: 'string' },
  } });
  if (positionals.length !== 4) throw new Error('Expected ConPTY launcher, console executable, seeder, and build directory.');
  const [windowsHost, executable, seeder, build] = positionals.map(value => resolve(value));
  const artifacts = join(build, 'tests', 'ui');
  fs.mkdirSync(artifacts, { recursive: true });
  const root = fs.mkdtempSync(join(artifacts, 'console-'));
  await withRuntime(root, async () => {
    const runtime = dirname(stageTestRuntime(executable, root));
    for (const directory of ['locales', 'themes']) {
      fs.cpSync(join(dirname(executable), directory), join(runtime, directory), { recursive: true });
    }
    const output = execFileSync(process.execPath, [
      join(__dirname, '../../scripts/seed-test-library.js'), '--tracks', '25', '--seconds', '1',
      '--indexer', seeder, '--fixture-dir', root, build,
    ], { encoding: 'utf8', timeout: 15000 });
    const profile = dirname(dirname(dirname(output.trimEnd().split(/\r?\n/).pop())));
    fs.writeFileSync(join(profile, 'musikcube', 'settings.json'), JSON.stringify({
      FirstRunSettingsDisplayed: true,
      SyncOnStartup: false,
      AutoUpdateCheck: false,
    }));

    const terminal = new Terminal(runtime, { ...process.env, APPDATA: profile, USERPROFILE: profile },
      26, 100, { windowsHost });
    try {
      await terminal.expect('^D  quit');
      terminal.send('t');
      await terminal.expect(FIRST_TRACK);
      if (values.screenshots) terminal.saveArtifacts(join(values.screenshots, 'before'));
      for (const rows of [34, 25, 32, 27]) {
        await terminal.resize(rows, 120);
        await terminal.waitFor(() => terminal.screen.text().split('\n')[rows - 1]?.includes('^D  quit'),
          'the quit shortcut on the resized terminal bottom row');
        if (!terminal.screen.text().includes(FIRST_TRACK)) {
          throw new Error(`Loaded track disappeared after resizing to ${rows} rows`);
        }
      }
      if (values.screenshots) terminal.saveArtifacts(join(values.screenshots, 'after'));
      terminal.send('\x04');
      await terminal.expectExit();
    } catch (error) {
      terminal.saveArtifacts(root);
      throw error;
    } finally {
      await terminal.close();
    }
  });
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
