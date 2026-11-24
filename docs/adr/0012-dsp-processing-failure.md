# ADR-0012 — DSP processing failure

**Status** Accepted

## Context

A DSP modifies audio in place and can report an error after changing part of a
buffer. Playback cannot determine which samples are valid after that failure.

## Decision

Discard the affected buffer and stop playback with the plugin's error. Earlier
audio already accepted by the output follows the existing playback failure path.

## Alternatives and consequences

Playing the buffer could expose partially processed audio. Keeping a backup of
every buffer would add copies and buffering solely to recover from a plugin
failure. Stopping playback keeps the data path simple and makes the failure
visible to the caller.
