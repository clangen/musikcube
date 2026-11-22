/* Exercise Windows console input, output, and resize through the real app. */
const fs = require('node:fs');
const { dirname, join, resolve } = require('node:path');
const { parseArgs } = require('node:util');
const { Terminal } = require('../support/terminal');
const { withRuntime } = require('../support/artifacts');
const { stageTestRuntime } = require('../../scripts/test-runtime');

async function main() {
  const { positionals, values } = parseArgs({ allowPositionals: true, options: {
    screenshots: { type: 'string' },
  } });
  if (positionals.length !== 3) throw new Error('Expected ConPTY launcher, console executable, and build directory.');
  const [windowsHost, executable, build] = positionals.map(value => resolve(value));
  const artifacts = join(build, 'tests', 'ui');
  fs.mkdirSync(artifacts, { recursive: true });
  const root = fs.mkdtempSync(join(artifacts, 'console-'));
  await withRuntime(root, async () => {
    const runtime = dirname(stageTestRuntime(executable, root));
    for (const directory of ['locales', 'themes']) {
      fs.cpSync(join(dirname(executable), directory), join(runtime, directory), { recursive: true });
    }
    const profile = join(root, 'profile');
    fs.mkdirSync(join(profile, 'musikcube'), { recursive: true });
    fs.writeFileSync(join(profile, 'musikcube', 'settings.json'), JSON.stringify({
      FirstRunSettingsDisplayed: true,
      SyncOnStartup: false,
      AutoUpdateCheck: false,
    }));

    const terminal = new Terminal(runtime, { ...process.env, APPDATA: profile, USERPROFILE: profile },
      26, 100, { windowsHost });
    try {
      await terminal.expect('^D  quit');
      if (values.screenshots) terminal.saveArtifacts(join(values.screenshots, 'before'));
      await terminal.resize(34, 120);
      await terminal.waitFor(() => terminal.screen.text().split('\n')[33]?.includes('^D  quit'),
        'the quit shortcut on the resized terminal bottom row');
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
