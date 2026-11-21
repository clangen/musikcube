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
  const text = Buffer.from(value);
  const header = Buffer.alloc(11);
  header.write(id);
  header.writeUInt32BE(text.length + 1, 4);
  return Buffer.concat([header, text]);
}

/* Build deterministic optional metadata so indexed fixtures exercise present, absent, and multivalue tags. */
function id3Tag(album, track) {
  const frames = [id3TextFrame('TALB', album)];
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
function wave({ title, artist, album, genre, track }, seconds) {
  const format = Buffer.alloc(16);
  format.writeUInt16LE(1, 0);
  format.writeUInt16LE(1, 2);
  format.writeUInt32LE(8000, 4);
  format.writeUInt32LE(16000, 8);
  format.writeUInt16LE(2, 12);
  format.writeUInt16LE(16, 14);

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
    chunk('ID3 ', id3Tag(album, track)), chunk('data', Buffer.alloc(16000 * seconds)),
  ]));
}

/* Write either the small named contract fixture or a larger deterministic artist/album catalog. */
function writeMedia(media, count, seconds) {
  if (count === 2) {
    writeFileSync(join(media, '01-morning.wav'), wave({
      title: 'Morning Light', artist: 'Fixture Artist', album: 'Fixture Album', genre: 'Ambient', track: 1,
    }, seconds));
    writeFileSync(join(media, '02-evening.wav'), wave({
      title: 'Evening Flight', artist: 'Fixture Artist', album: 'Fixture Album', genre: 'Electronic', track: 2,
    }, seconds));
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
      writeFileSync(join(directory, `${String(track).padStart(2, '0')}.wav`),
        wave({ title, artist, album, genre, track }, seconds));
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
      indexer: { type: 'string' },
      'fixture-dir': { type: 'string' },
      help: { type: 'boolean', short: 'h' },
    },
    allowPositionals: true,
  });
  if (values.help) {
    console.log('Usage: node tests/scripts/seed-test-library.js [--tracks <count>] [--seconds <duration>] [--indexer <executable>] [--fixture-dir <dir>] [configured-build-directory]');
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
  writeMedia(media, count, seconds);

  /* Stage plugins and redirect preferences before starting production code so
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
