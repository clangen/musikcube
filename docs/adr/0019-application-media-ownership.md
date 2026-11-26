# ADR-0019: Application media ownership

Status: accepted.

## Context

Applications need byte IO, decoding and encoding without creating a playback
session. Plugin media and scratch tables borrow provider-session state and can
become invalid during context shutdown. Returning those tables to applications
would let retained handles call unloaded code. Borrowing a caller's stream for a
codec would also let the caller close it while the codec still uses it.

## Decision

Expose opaque `mc_media_stream`, `mc_media_decoder`, `mc_media_encoder` and
`mc_media_pcm` handles through the application C ABI. The header-only C++ owners
call that ABI. Plugin service tables and plugin PCM types remain separate.

Stream creation selects a provider without IO on the context owner thread.
Opening is a separate worker operation, allowing callers to publish the handle
for cancellation before blocking. Decoder and encoder creation consume an
already opened stream only on success. Their separate open operations permit the
same cancellation sequence. Codec cleanup always precedes stream cleanup.

Ordinary operations on each resource are serialized; concurrent attempts return
`BUSY`. Interruption can race an operation and latches cancellation. Decoders
interrupt both codec and input. Encoders interrupt output IO; CPU work must
return before cleanup can complete, as the encoder SDK has no interrupt callback.
Destroy and ownership transfer require synchronization with callers using the
same public handle.

A context-owned session admits calls and retains the roots of media ownership
chains. Shutdown closes admission, interrupts roots, waits for admitted calls,
then destroys codecs before their streams and before provider unload. Replacing
a stream root with a codec root is atomic with shutdown. Retained handles own
stopped state and return `NOT_FOUND` without accessing provider tables.

Each decoded PCM block owns a copy of its samples. Borrowed views last until that
block is destroyed, including after another read or context shutdown. Stream
metadata strings similarly live with their application handle.

Encoding writes returned streaming blocks to the owned output, accounting for
partial writes. File encoders receive that output directly. Explicit finish
flushes, finalizes, destroys the codec and closes output, returning the first
failure. Finish caches its result; a failed write is never retried implicitly.
Destroying an unfinished encoder aborts without flushing/finalizing. Expected IO
errors remain ordinary results; a provider exception crossing C or a resource
escaping native cleanup terminates the process.

## Alternatives

Returning borrowed plugin services has the smallest implementation but cannot
make retained application handles safe at shutdown. Reference-counting a stream
while leaving close available to borrowers requires additional attachment rules
and rejection paths. Consuming it makes the safe ownership sequence the natural
one. Borrowed decoder scratch avoids a copy but invalidates samples on each
read and during shutdown; owned PCM has a simpler application contract.

## Consequences

Applications can use media independently of playback, indexing and catalogs.
They must keep blocking calls off event/audio threads and finish encoders when
completion errors matter. PCM incurs an allocation and copy per nonempty block;
playback and plugin services retain their existing borrowed-buffer paths.
The implementation needs one session root list and admission counter so cleanup
cannot race provider unload. No plugin ABI layout or version changes are needed;
the experimental application ABI version stays fixed until freeze.
