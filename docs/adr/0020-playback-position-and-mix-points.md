# ADR-0020: Playback position and mix points

Status: accepted.

## Context

An output can copy PCM into its own queue and return the original buffer before
its samples play. Buffer completion therefore establishes reuse, not an audible
playback position. The player already subtracts valid queued/device latency from
its completed-buffer position, and treats unavailable timing as zero latency.

The position shown to callers is rounded to whole seconds. Using that rounded
value for mix-point comparisons can trigger a crossfade up to half a second
before the intended point, even when the backend reports accurate latency.

## Decision

Keep the existing optional latency contract. Combine only finite, nonnegative
values whose validity flags are set. Without those values, progress and mix
points are approximate; neither buffer return nor an unknown snapshot supplies
a device clock.

Compare mix points against the unrounded latency-adjusted buffer position.
Keep whole-second rounding for the existing displayed position. Mix-point
notifications still run on the playback worker as returned buffers are processed,
not on a hardware callback. Their resolution depends on buffer processing and
the backend's available timing information.

Do not require a new media-to-device timeline or device-clock implementations
across all providers without a concrete playback problem requiring them.

## Alternatives and consequences

Rounding both values is simple but conflates presentation precision with
crossfade scheduling. Consuming played-frame timestamps throughout the player
would require mapping track offsets, seeks and overlapping outputs; the current
problem does not justify that machinery. The focused comparison change avoids
early rounding-induced mix points without changing the application/plugin ABI
or requiring additional backend measurements.
