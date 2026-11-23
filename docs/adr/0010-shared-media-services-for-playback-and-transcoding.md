# ADR-0010 — Shared media services for playback and transcoding

**Status** Accepted

## Context

Playing a remote track can use musikcore twice. The server opens the original
file and decodes it for an encoder. The client opens the HTTP response and
decodes it for an output. A subsong provider has a similar need to open its
underlying file. Each caller must be able to choose a provider, interrupt a
blocked read, and tell a clean end of input from a failed transfer.

## Decision

The engine supplies one media service for choosing streams and decoders. A URI
selects a stream. The opened stream supplies a media type or filename extension
for choosing the decoder. Factory probes perform no I/O. Once a provider claims
a resource, its failure goes back to the caller. Trying another provider could
consume bytes from a stream that cannot rewind.

The caller creates each instance before opening it. Playback can publish a
handle to its control thread before a network operation blocks. The caller
owns the input until the decoder is destroyed. A provider that needs a child
stream uses the same service and takes ownership of that child.

The decoder writes interleaved floats into host-owned scratch. The server
passes a read-only view of those frames to an encoder before decoding again.
Playback copies the frames into its fixed output buffers, whose lifetime
extends through device completion. Each consumer owns the storage it needs
beyond the next decode.

## Alternatives and consequences

Keeping the old decoder interface behind an adapter would require translating
EOF, cancellation and ownership at every server call. Implementing separate
provider selection in the server would also make MIME matching and virtual
track support depend on which component opened the file. Using the same media
service gives all three callers the same behavior when a provider is missing,
the input ends, or a read fails.

Changing the server's input contract also changes what its encoders consume.
An encoder receives frames and a format, and a file-writing encoder receives a
writable stream. Encoder discovery and compression algorithms can evolve
independently of that handoff.
