# ADR-0021: Remote playback metadata

Status: accepted.

## Context

Playback needs a track's URI and replay gain before opening its stream. The
legacy playback service synchronously requested remote metadata while processing
playback events on its owner thread. An indexing server or slow network could
hold the frontend for the five-second request timeout. A short track could end
before its successor was prepared.

## Decision

Request remote metadata asynchronously for initial playback and next-track
preparation. Continue on the playback owner queue when the matching response
arrives. Getters consult already loaded records and never initiate a remote
metadata wait. Reuse the list cache alongside the current playing record.

Use `TrackList::GetAsync()` to fetch into the existing track cache. Each list
owns one pending lookup; replacement, explicit cancellation, clearing the cache,
or destruction expires its continuation before a late reply can touch the list.
Playback cancels the lookup on Stop, replacement, library change, disconnect,
or shutdown. Check the queue revision, track identity, and originating library
session before using a response. A cancelled response
cannot start or prepare audio later.

The default metadata deadline is 30 seconds, configurable through
`PlaybackTrackQueryTimeoutMs`. Expiry is a delayed owner-queue message, not a
blocking wait. Expiry retires the continuation and logs the failed operation.
Already playing audio continues independently. If metadata arrives after a short
track ends, the transport can start the successor once it is prepared.

## Alternatives and consequences

Increasing the synchronous timeout would make UI freezes longer. Moving the
entire playback service to another thread would require changing owner-thread
application and plugin contracts. Asynchronous requests preserve those contracts
and use the backend's existing response delivery.

A successful play submission can precede stream startup. Callers observe playback
events instead of assuming that the stream is open when the command returns.
Metadata and audio buffering are separate: a responsive UI does not guarantee
that a slow server supplies the next track without an audible gap.
