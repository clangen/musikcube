# ADR-0011 — Analyzer results for a track

**Status** Accepted

## Context

Analyzers read the same decoded audio and can update the same track metadata.
One analyzer can fail after changing the in-memory track while another succeeds.

## Decision

Save analyzer changes for a track only when every participating analyzer
completes successfully. On failure or cancellation, discard all analyzer
changes for that track. Analysis continues with other tracks unless indexing
has been cancelled. If an enabled analyzer cannot be created, skip analysis
for the scan and report the failure. A factory that reports success without
an instance violates the plugin contract. Cancellation waits for a track save
that has already started, and prevents later analyzer saves.

## Alternatives and consequences

Saving each successful analyzer's changes would require isolating its metadata
writes or rolling back only the failed analyzer's edits. The shared track
record allows a simpler all-or-nothing decision. A successful analyzer's
results must be computed again if another analyzer fails on the same track.
