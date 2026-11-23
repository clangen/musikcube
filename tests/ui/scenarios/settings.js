/* Exercise playback and plugin settings independently of library navigation. */
const { DOWN, RIGHT, HOME } = require('../support/catalog');

async function checkSettings(terminal, { staged, hasServer }) {
  if (staged.some(name => /^(?:lib)?supereqdsp\./.test(name))) {
    terminal.send('\x05');
    await terminal.expect('frequency bands');
    terminal.send('\t' + RIGHT);
    await terminal.expect('1 dB');
    terminal.send('z');
    await terminal.expect('frequency bands', { absent: '1 dB' });
    terminal.send('\x1b');
    await terminal.expect('playback is stopped', { absent: 'frequency bands' });
  }
  const nullPlugin = staged.find(name => /^(?:lib)?nullout\./.test(name));
  if (nullPlugin) {
    terminal.send('v');
    await terminal.expect('no visualizers found!');
    terminal.send('\r');
    await terminal.expect('playback is stopped', { absent: 'no visualizers found!' });
    await terminal.resize(28, 140);
    await terminal.waitFor(() => terminal.screen.text().split('\n')[27]?.includes('^D  quit'),
      'redrawn command bar after resizing');
    terminal.send('s');
    await terminal.expect('output driver: Null');
    const clickLabel = async label => {
      await terminal.expect(label);
      const rows = terminal.screen.text().split('\n');
      const y = rows.findIndex(row => row.includes(label));
      const x = rows[y].indexOf(label);
      const click = `\x1b[<0;${x + 1};${y + 1}`;
      terminal.send(click + 'M' + click + 'm');
    };
    await clickLabel('playback mode: gapless');
    await terminal.expect('crossfade');
    terminal.send(DOWN + '\r');
    await terminal.expect('playback mode: crossfade');
    terminal.send('\x1b[Z\r');
    await terminal.expect('preamp gain (dB):');
    await terminal.expect('replaygain: disabled');
    terminal.send('\t\r');
    await terminal.expect('replaygain mode');
    terminal.send(DOWN + '\r');
    await terminal.expect('replaygain: track');
    terminal.send('\x1b');
    await terminal.expect('playback mode: crossfade', { absent: 'preamp gain (dB):' });
    if (hasServer) {
      await terminal.expect('server setup');
      terminal.send('\t'.repeat(5) + '\r');
      await terminal.expect('audio streaming enabled');
      terminal.send('\x1b');
      await terminal.expect('playback mode: crossfade', { absent: 'audio streaming enabled' });
    }

    await terminal.expect('configure plugins');
    terminal.send('\t'.repeat(hasServer ? 7 : 11) + '\r');
    await terminal.expect('Null Output');
    /* a PTY update may end midway through the name; wait for its whole row
    before deriving a navigation index from the rendered plugin list. */
    await terminal.waitFor(() => terminal.screen.text().split('\n').some(
      row => row.includes('Null Output') && row.includes('[v')), 'complete Null plugin row');
    const pluginRows = terminal.screen.text().split('\n').filter(row => row.includes('[v'));
    const nullRow = pluginRows.findIndex(row => row.includes('Null Output'));
    terminal.send(HOME + DOWN.repeat(nullRow) + '\r');
    /* C null output intentionally has no settings schema. its row must remain
    configurable through the catalog without inventing a sample-rate setting. */
    await terminal.expect('does not provide any configurable settings.');
    terminal.send('\r');
    await terminal.waitFor(() => {
      const text = terminal.screen.text();
      return !text.includes('no configuration') && text.split('\n').some(
        row => row.includes('Null Output') && row.includes('[v'));
    }, 'plugin list restored after dismissing no-schema dialog');
    terminal.send('\x1b');
    /* the driver and catalog now share a display name; use the binary name
    to distinguish the closed plugin overlay from the settings underneath. */
    await terminal.expect('configure plugins', { absent: `(${nullPlugin})` });

    terminal.send('\t\r');
    await terminal.expect('AutoHideCommandBar');
    terminal.send('\x1b');
    await terminal.expect('configure plugins', { absent: 'AutoHideCommandBar' });

    terminal.send('\x1b[Z'.repeat(hasServer ? 11 : 10) + '\r');
    await terminal.expect('no last.fm account registered');
    terminal.send('\x1b');
    await terminal.expect('configure plugins', { absent: 'last.fm registration' });
  }
}

module.exports = { checkSettings };

if (require.main === module) require('../support/run-scenario').run('settings');
