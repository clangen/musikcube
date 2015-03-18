# Introduction #

A list of features mC2 should support. We should cherry-pick features from here to help define the [Roadmap](Roadmap.md).

# Details #

## Basic playback ##
  * Playback from library (now playing)
  * Add/remove now playing songs
  * Volume control
  * Start/stop/pause
  * Next/previous

## Media formats ##
  * Add/fix decoders (files only): MP3, Ogg, FLAC, WAV, WMA (non-DRM?), WavPack?, APE?, MP4/AAC?
  * Remove decoder libs with conflicting licenses
  * Tag reading of supported audio types

## Tag & file handling ##
  * Write tags (single file)
  * Move files (path & filename from tags?)
  * Support for new tags (not in standard TagLib?): disc number, multiple artists, lyrics, etc

## Playlists ##
  * Smart/dynamic playlists (auto add & remove tracks from now playing)
  * Define default queries
  * User friendly query builder?

## Now playing II ##
  * Advanced queueing: add album, add to end, add as next, ...
  * Reorder playlist (also for multiple files at once)
  * Repeat: track, album, playlist

## Audio control ##
  * Output device configuration
  * Cross-fading
  * Gapless playback?
  * Replaygain?
  * Position slider: get/set position in file
  * Mixer/audio profiles?

## Preferences ##
  * General preferences/Defaults
  * Plugin configuration

## GUI ##
  * Commands & menu structure
  * (Global) hotkeys
  * Graphics for buttons, sliders, ...
  * Translations
  * Minimize to tray

## Media formats II ##
  * Streaming audio decoder (which formats?)

## Plugins ##
  * Stabilize APIs
  * Last.fm
  * MSN Now playing
  * Scoring & track selection (IMMS?)

## Future ##
  * Visualization?
  * Multiple libraries
  * Remote libraries
  * Streaming output
  * Cross-platform (requires new GUI)?
  * Different DB backends?