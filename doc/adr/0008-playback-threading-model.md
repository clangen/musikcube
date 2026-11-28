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

Each output also owns and joins its visualization audio queue. It keeps
bounded PCM copies only while a visualizer or spectrum meter needs audio, and
selects the latest audible audio window at a regular interval. Skipped windows
are discarded rather than replayed; visualization is a current picture of the
audio, not a lossless consumer. Approximate output timing is sufficient for
perceptual synchronization; sample-accurate visual presentation is not a
requirement. Returning an output buffer permits storage reuse, independently
of visualization delivery.
Seek, stop and output replacement discard pending visual frames before changing
the clock; the queue is joined before plugin factories can be released.

Output plugins expose timing through the C `mc_output_timing` contract. A
backend may use the private `OutputTimingEstimator` helper or provide a native
counter directly. Core `AudiblePlaybackPosition` owns the shared frame domain and
normalizes it once for both per-writer track position and visualization delivery.
The queue consumes an audible frame index rather than independently interpreting
device latency and native clock generations. Playback progress works without a
visualization worker when no visual consumers are active.

## Alternatives considered

A single playback thread would make selection state easy to serialize, but a
blocking open, decode or device write would also delay other work assigned to
that thread. Keeping the self-owned workers would preserve independent progress
at the cost of a second lifetime protocol for their callbacks. Session-owned
jobs preserve independent progress with a concrete join point. A backend still
has to honor interruption for shutdown to finish.

Holding decoder buffers until visualization catches up would tie decoder
capacity to visualizer progress. Dispatching only on storage completion would
make synchronization depend on each backend's copying policy. Bounded separate
copies cost memory and one worker per active output, but allow storage to return
promptly and visual delivery to follow the sound. Overload drops visual frames
instead of retaining an unbounded history. Delivering every successive audible
chunk would preserve every transient for analysis, but would also require
catch-up scheduling and coordination with producers and controls. Selecting
the current window accepts missed transients after a scheduling delay in
exchange for bounded work and timely pictures.

## Consequences

The ownership decision allows either a retained-buffer output or a write that
consumes PCM during the call. It also allows separate device outputs or a
software mixer. Introducing a mixer would require an explicit format conversion,
channel mapping, fade lookahead and playback timing policy. Those choices can
be evaluated independently.

The [media and playback design](../design/musikcore-c-abi-design.md) owns the
operation contracts and implementation details.
