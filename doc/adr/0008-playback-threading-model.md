# ADR-0008: own playback workers within their sessions

Status: accepted

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
A job reports to the playback session that launched it. The receiver checks the
sending track's identity before changing its selection. A crossfade holds a reference for as long
as it needs to adjust the track's gain. The worker owns the decoder and buffer
pool. Controls can interrupt a source and read published track information
while that worker is blocked.

Each track writes through a revocable writer. On replacement the transport can
withdraw a writer's permission to change the device while the worker finishes
recycling its previously accepted buffers. Gapless tracks can therefore share
a device queue. Terminal shutdown closes admission, interrupts all jobs in the
session, including retired tails, and joins them before their receiver or plugin
factories are released.

## Alternatives considered

A single playback thread would make selection state easy to serialize, but a
blocking open, decode or device write would also delay other work assigned to
that thread. Keeping the self-owned workers would preserve independent progress
at the cost of a second lifetime protocol for their callbacks. Session-owned
jobs preserve independent progress with a concrete join point. A backend still
has to honor interruption for shutdown to finish.

## Consequences

The ownership decision allows either a retained-buffer output or a write that
consumes PCM during the call. It also allows separate device outputs or a
software mixer. Introducing a mixer would require an explicit format conversion,
channel mapping, fade lookahead and playback timing policy. Those choices can
be evaluated independently.

The [media and playback design](../design/musikcore-c-abi-design.md) owns the
operation contracts and implementation details.
