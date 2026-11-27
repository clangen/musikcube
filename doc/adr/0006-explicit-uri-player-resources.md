# ADR-0006: own media resources independently of catalogs

Status: accepted

## Context

Applications need URI playback, byte streams, decoding, and encoding without
fabricating catalog tracks or starting an indexer. Borrowed plugin service tables
cannot safely serve retained application handles: their provider session can end
while a caller still holds them. Borrowed streams also allow callers to close an
input while a codec is using it.

## Decision and rationale

Expose explicitly owned child resources through the application C ABI, within
a context providing Plugins. URI players and application media do not require
Library or catalog Playback. They share provider discovery and session lifetime
with the context rather than introducing another media runtime.

Publish resources before blocking open operations so interruption can reach
startup. Transfer stream ownership to a codec only on successful creation;
cleanup destroys the codec before its stream. Context shutdown interrupts and
quiesces children before provider unload. Retained handles refer to stopped state
rather than unloaded provider tables. Player notifications use host dispatch.

Application PCM blocks own their samples. This copy gives callers a simple
lifetime independent of subsequent reads and context shutdown. Plugin and playback
paths keep their separate borrowed-buffer contracts.

## Alternatives considered

Synthetic catalog tracks preserve unnecessary collection coupling. A separate
player/media runtime duplicates provider selection and teardown. Returning borrowed
plugin tables cannot protect retained handles at shutdown. Shared stream ownership
while allowing arbitrary close needs attachment and rejection rules; consuming
the stream establishes one cleanup owner. Borrowed PCM avoids a copy but exports
scratch-buffer invalidation to every application caller.

## Consequences

Clients can use media without a database and can keep decoded blocks after
shutdown. Nonempty application PCM blocks incur allocation and copying. Destroying
an owned URI player stops its workers, unlike releasing a borrowed catalog
playback handle. Independent players do not promise hardware mixing or common
device clocks. Providers must still honor interruption; a published handle cannot
make an uncooperative operation safely interruptible.

[URI player contracts](../design/musikcore-c-abi-design.md#explicit-uri-players) and
[application media contracts](../design/musikcore-c-abi-design.md#advanced-audio-api)
define operations, threading, serialization, and encoder finish/abort behavior.
