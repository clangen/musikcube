/* Generate deterministic WAV files and run the production C++ indexer over them.
   Integration tests and TUI scenarios share these isolated catalogs so they can
   exercise real indexed metadata without depending on a user's music collection. */

const { spawnSync } = require('child_process');
const { existsSync, mkdirSync, mkdtempSync, readdirSync, writeFileSync } = require('fs');
const { join, resolve } = require('path');
const { parseArgs } = require('util');
const { stageTestRuntime } = require('./test-runtime');

const source = resolve(__dirname, '../..');
const windows = process.platform === 'win32';
const config = 'Release';
const tracksPerAlbum = 10;
const albumsPerArtist = 10;
const genres = ['Ambient', 'Electronic', 'Jazz', 'Rock', 'Classical'];
/* These ID3 frames map to normalized metadata keys in the production tag reader. */
const extraTags = [
  ['TCOM', 'Avery Lane/Casey Reed'],
  ['TCOP', 'Fixture Records'],
  ['TENC', 'Fixture Encoder'],
  ['TEXT', 'Morgan Vale'],
  ['TOLY', 'Jordan Ellis'],
  ['TPUB', 'Fixture Publishing'],
  ['TMOO', ['Calm', 'Bright', 'Reflective']],
  ['TOPE', 'Fixture Ensemble'],
  ['TLAN', ['eng', 'fra', 'deu']],
  ['TPE3', 'Taylor Quinn'],
];

/* Run a fixture tool synchronously and fail before inspecting an incomplete library database. */
function run(command, args, env = process.env) {
  const result = spawnSync(command, args, { env, stdio: 'inherit' });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} failed (${result.signal || result.status})`);
}

/* Encode a RIFF chunk with its little-endian byte length and required even-byte padding. */
function chunk(id, data) {
  const header = Buffer.alloc(8);
  header.write(id);
  header.writeUInt32LE(data.length, 4);
  return Buffer.concat([header, data, Buffer.alloc(data.length % 2)]);
}

/* Encode one ID3v2.3 text frame, including its encoding byte and big-endian payload length. */
function id3TextFrame(id, value) {
  return id3Frame(id, Buffer.concat([Buffer.alloc(1), Buffer.from(value)]));
}

function id3Frame(id, payload) {
  const header = Buffer.alloc(10);
  header.write(id);
  header.writeUInt32BE(payload.length, 4);
  return Buffer.concat([header, payload]);
}

/* Build deterministic optional metadata so indexed fixtures exercise present, absent, and multivalue tags. */
function id3Tag(album, track, lyrics, artwork) {
  const frames = [id3TextFrame('TALB', album)];
  if (lyrics) frames.push(id3Frame('USLT', Buffer.from('\0eng\0' + lyrics)));
  if (artwork) frames.push(id3Frame('APIC', Buffer.concat([
    Buffer.from('\0image/png\0\x03\0'), Buffer.alloc(40, 'a'),
  ])));
  for (const [index, [id, values]] of extraTags.entries()) {
    if ((track + index) % 4 === 0) continue;
    const value = Array.isArray(values) ? values[(track - 1) % values.length] : values;
    frames.push(id3TextFrame(id, value));
  }

  /* ID3v2.3 uses a synchsafe tag size and big-endian frame sizes. */
  const payload = Buffer.concat(frames);
  const header = Buffer.from([0x49, 0x44, 0x33, 3, 0, 0, 0, 0, 0, 0]);
  for (let i = 0; i < 4; ++i) header[9 - i] = (payload.length >>> (i * 7)) & 0x7f;
  return Buffer.concat([header, payload]);
}

/* Generate a silent WAV in memory with RIFF and ID3 tags read by the production indexer. */
function wave({ title, artist, album, genre, track, lyrics, artwork }, seconds, stereo) {
  const channels = stereo ? 2 : 1;
  const bytesPerSecond = 16000 * channels;
  const format = Buffer.alloc(stereo ? 40 : 16);
  format.writeUInt16LE(stereo ? 0xfffe : 1, 0);
  format.writeUInt16LE(channels, 2);
  format.writeUInt32LE(8000, 4);
  format.writeUInt32LE(bytesPerSecond, 8);
  format.writeUInt16LE(2 * channels, 12);
  format.writeUInt16LE(16, 14);
  if (stereo) {
    /* WAVE_FORMAT_EXTENSIBLE with the front left/right mask and PCM subtype. */
    format.writeUInt16LE(22, 16);
    format.writeUInt16LE(16, 18);
    format.writeUInt32LE(3, 20);
    Buffer.from('0100000000001000800000aa00389b71', 'hex').copy(format, 24);
  }

  /* Use RIFF INFO tags so the production tag reader supplies the metadata. */
  const info = Buffer.concat([
    Buffer.from('INFO'),
    chunk('INAM', Buffer.from(title + '\0')),
    chunk('IART', Buffer.from(artist + '\0')),
    chunk('IPRD', Buffer.from(album + '\0')),
    chunk('IGNR', Buffer.from(genre + '\0')),
    chunk('IPRT', Buffer.from(String(track) + '\0')),
  ]);
  return chunk('RIFF', Buffer.concat([
    Buffer.from('WAVE'), chunk('fmt ', format), chunk('LIST', info),
    chunk('ID3 ', id3Tag(album, track, lyrics, artwork)), chunk('data', Buffer.alloc(bytesPerSecond * seconds)),
  ]));
}

/* Write either the small named contract fixture or a larger deterministic artist/album catalog. */
function writeMedia(media, count, seconds, embeddedLyrics, stereo, embeddedArtwork) {
  if (count === 2) {
    writeFileSync(join(media, '01-morning.wav'), wave({
      title: 'Morning Light', artist: 'Fixture Artist', album: 'Fixture Album', genre: 'Ambient', track: 1,
      lyrics: embeddedLyrics ? 'Fixture lyrics for track 1' : undefined,
      artwork: embeddedArtwork,
    }, seconds, stereo));
    writeFileSync(join(media, '02-evening.wav'), wave({
      title: 'Evening Flight', artist: 'Fixture Artist', album: 'Fixture Album', genre: 'Electronic', track: 2,
    }, seconds, stereo));
    return;
  }

  /* Keep tags and directory groups aligned so browse views see the same albums. */
  for (let start = 0; start < count; start += tracksPerAlbum) {
    const albumIndex = start / tracksPerAlbum;
    const artistIndex = Math.floor(albumIndex / albumsPerArtist);
    const artist = `Artist ${String(artistIndex + 1).padStart(3, '0')}`;
    const album = `${artist} - Album ${String(albumIndex % albumsPerArtist + 1).padStart(2, '0')}`;
    const genre = genres[artistIndex % genres.length];
    const directory = join(media, artist, album);
    mkdirSync(directory, { recursive: true });

    for (let i = start; i < Math.min(count, start + tracksPerAlbum); ++i) {
      const track = i - start + 1;
      const title = `${album} - Track ${String(track).padStart(2, '0')}`;
      const lyrics = embeddedLyrics ? `Fixture lyrics for track ${track}` : undefined;
      writeFileSync(join(directory, `${String(track).padStart(2, '0')}.wav`),
        wave({ title, artist, album, genre, track, lyrics }, seconds, stereo));
    }
  }
}

/* Locate the generated musik.db under the fixture's private configuration tree. */
function findDatabase(directory) {
  const database = join(directory, 'musik.db');
  if (existsSync(database)) return database;
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    if (!entry.isDirectory()) continue;
    const found = findDatabase(join(directory, entry.name));
    if (found) return found;
  }
}

/* Validate fixture size, stage the indexer, generate media, and print the resulting database path. */
function main() {
  const { values, positionals: [buildDir, ...extra] } = parseArgs({
    options: {
      tracks: { type: 'string', default: '2' },
      seconds: { type: 'string', default: '1' },
      'embedded-lyrics': { type: 'boolean' },
      'embedded-artwork': { type: 'boolean' },
      stereo: { type: 'boolean' },
      indexer: { type: 'string' },
      'fixture-dir': { type: 'string' },
      help: { type: 'boolean', short: 'h' },
    },
    allowPositionals: true,
  });
  if (values.help) {
    console.log('Usage: node tests/scripts/seed-test-library.js [--tracks <count>] [--seconds <duration>] [--embedded-lyrics] [--embedded-artwork] [--stereo] [--indexer <executable>] [--fixture-dir <dir>] [configured-build-directory]');
    return;
  }

  if (extra.length) throw new Error('Expected a single configured build directory.');
  const count = Number(values.tracks);
  if (!/^\d+$/.test(values.tracks) || !Number.isSafeInteger(count) || count < 1) {
    throw new Error('--tracks requires a positive integer.');
  }

  const seconds = Number(values.seconds);
  if (!/^\d+$/.test(values.seconds) || !Number.isInteger(seconds) || seconds < 1 || seconds > 30) {
    throw new Error('--seconds requires an integer from 1 to 30.');
  }

  /* An explicitly supplied indexer is already built by its caller (CTest or
     the TUI runner); avoid reconfiguring that caller's build in this path. */
  const build = buildDir ? resolve(buildDir) : windows ? join(source, 'build/windows-x64') : source;
  if (!existsSync(join(build, 'CMakeCache.txt'))) throw new Error(`Configure the build first: ${build}`);
  if (!values.indexer) {
    run('cmake', ['-S', source, '-B', build]);
    run('cmake', ['--build', build, '--config', config, '--target', 'musikcube_seed_library']);
  }

  /* Give the indexer a fresh config root and deterministic local media. */
  const fixtures = values['fixture-dir'] ? resolve(values['fixture-dir']) : join(build, 'tests', 'bin', 'library-fixtures');
  mkdirSync(fixtures, { recursive: true });
  const root = mkdtempSync(join(fixtures, 'library-'));
  const media = join(root, 'media');
  mkdirSync(media);

  console.log(`Generating ${count} fixture tracks under ${root}`);
  writeMedia(media, count, seconds, values['embedded-lyrics'], values.stereo, values['embedded-artwork']);

  /* Stage plugins and redirect settings before starting production code so
     indexing cannot discover user paths or unrelated installed audio plugins. */
  const executable = values.indexer ? resolve(values.indexer) : windows
    ? join(source, 'bin64', config, 'musikcube_seed_library.exe')
    : join(build, 'bin', 'musikcube_seed_library');
  run(stageTestRuntime(executable, root), [media, String(count)], { ...process.env, APPDATA: root, XDG_CONFIG_HOME: root });

  const database = findDatabase(join(root, 'musikcube'));
  if (!database) throw new Error(`No library database created under ${root}`);

  /* Callers consume the last stdout line as the database path; diagnostics
     above it can remain readable without complicating that interface. */
  console.log(database);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
