/* Exercise playback and plugin settings independently of library navigation. */
const { DOWN, RIGHT, END, HOME } = require('../support/catalog');

async function checkSettings(terminal, { staged, hasServer }) {
  if (staged.some(name => name.startsWith('libsupereqdsp.'))) {
    terminal.send('\x05');
    await terminal.expect('frequency bands');
    terminal.send('\t' + RIGHT);
    await terminal.expect('1 dB');
    terminal.send('z');
    await terminal.expect('frequency bands', { absent: '1 dB' });
    terminal.send('\x1b');
    await terminal.expect('playback is stopped', { absent: 'frequency bands' });
  }
  if (staged.some(name => name.startsWith('libnullout.'))) {
    terminal.send('v');
    await terminal.expect('no visualizers found!');
    terminal.send('\r');
    await terminal.expect('playback is stopped', { absent: 'no visualizers found!' });
    await terminal.resize(28, 140);
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
    await terminal.expect('Null IOutput');
    const pluginRows = terminal.screen.text().split('\n').filter(row => row.includes('[v'));
    const nullRow = pluginRows.findIndex(row => row.includes('Null IOutput'));
    terminal.send(HOME + DOWN.repeat(nullRow) + '\r');
    await terminal.expect('default_sample_rate');
    terminal.send(DOWN + '\r');
    await terminal.expect('4096 - 192000');
    terminal.send(END + '\x7f'.repeat(5) + '44100\r');
    await terminal.expect('44100', { absent: '4096 - 192000' });
    terminal.send('\x1b');
    await terminal.expect('Null IOutput', { absent: 'default_sample_rate' });
    terminal.send('\x1b');
    await terminal.expect('configure plugins', { absent: 'Null IOutput' });

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
