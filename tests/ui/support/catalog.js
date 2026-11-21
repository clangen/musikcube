const FILTER = 'Artist 001 - Album 02';
const FIRST_ALBUM = 'Artist 001 - Album 01';
const FIRST_TRACK = `${FIRST_ALBUM} - Track 01`;
const SECOND_TRACK = `${FIRST_ALBUM} - Track 02`;
const FILTERED_FIRST = `${FILTER} - Track 01`;
const FILTERED_LAST = `${FILTER} - Track 10`;
const FIRST_WINDOW_MISS = `${FIRST_ALBUM} - Track 10`;
const DISTANT_TRACK = 'Artist 001 - Album 10 - Track 10';
const DOWN = '\x1bOB';
const UP = '\x1bOA';
const RIGHT = '\x1bOC';
const END = '\x1bOF';
const HOME = '\x1bOH';
const PAGE_DOWN = '\x1b[6~';
const DISTANT_PAGE = 12;

module.exports = { FILTER, FIRST_ALBUM, FIRST_TRACK, SECOND_TRACK, FILTERED_FIRST, FILTERED_LAST,
  FIRST_WINDOW_MISS, DISTANT_TRACK, DOWN, UP, RIGHT, END, HOME, PAGE_DOWN, DISTANT_PAGE };
