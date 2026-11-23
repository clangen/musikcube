# ADR-0009 — Playback threading model

**Status** Accepted

## Context

A decoder can block while opening a network stream. Meanwhile another track can
still be playing, a crossfade can retain a previous track, and an embedded
application can have several independent URI players. A track can leave the
active selection while its output still holds PCM from its buffer pool.

Previously a track worker detached its thread and deleted its player. Transports
counted workers and registered listeners that could be detached during a
callback. Safe shutdown depended on knowing which callbacks were still running
and finding every output that could keep a worker blocked.

## Decision

Each transport or standalone URI player owns and joins its own track jobs.
A job reports to the playback session that launched it. The receiver checks the identity of the sending
track before changing its selection. A crossfade holds a reference for as long
as it needs to adjust the track's gain. The worker owns the decoder and buffer
pool. Controls can interrupt a source and read published track information
while that worker is blocked.

Each track writes through a revocable writer. On replacement the transport can
withdraw a writer's permission to change the device while the worker finishes
recycling its previously accepted buffers. Gapless tracks can therefore share
a device queue. Terminal shutdown closes admission, interrupts all jobs in the
session, including retired tails, and joins them before their receiver or plugin
factories are released.

During a crossfade, the selected track determines whether Pause or Resume
succeeded. If its device refuses, the outgoing audio keeps its prior state.
Once it accepts, an outgoing device that cannot follow is stopped. Losing a
short fade tail on a device error is preferable to rolling back a successful
command on the track the listener selected.

Volume changes keep only the latest requested gain. If the device is busy,
the caller returns immediately and a later write applies that gain. This lets
manual controls and fade ticks share one output operation. The crossfader
stops the outgoing track if it cannot confirm silence at the end of a fade.

## Alternatives and consequences

A single playback thread would make selection state easy to serialize, but a
blocking open, decode or device write would also delay other work assigned to
that thread. Keeping the self-owned workers would preserve independent progress
at the cost of a second lifetime protocol for their callbacks. Session-owned
jobs preserve independent progress with a concrete join point. A backend still
has to honor interruption for shutdown to finish.

The ownership decision allows either a retained-buffer output or a write that
consumes PCM during the call. It also allows separate device outputs or a
software mixer. Introducing a mixer would require an explicit format conversion,
channel mapping, fade lookahead and playback timing policy. Those choices can
be evaluated independently.
