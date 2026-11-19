# Local and remote library routing

Status — proposal

## One context, explicit library handles

The app owns one `mc_context` for request IDs, cancellation, UI wakeups,
completion dispatch, and shutdown. Library selection returns an opaque
`mc_library` that binds to a local database or a remote connection. The
context stays alive when the app switches libraries. The C++ wrapper owns
the handle through a move-only `Library`.

Only library discovery, connection settings, and switching need to know
the backend type. Search, browse, metadata reads, and library edits take
a library handle or a handle derived from it. They expose the same input,
result, and completion types for both backends.

## Search data flow

1. The app submits search options and a library handle through the app
   context. The C API copies the inputs and builds an internal typed query.
2. The handle submits that query to the selected library. `LocalLibrary`
   executes it through the local query service. `RemoteLibrary` serializes
   it, and the server's query registry reconstructs the same query type
   for local execution. The remote backend decodes the reply into the
   query result before the C API delivers it.
3. Both routes produce a search result with a stable order and count.
   The app asks the result for a bounded window of metadata and group
   information. An uncached window uses an async typed range query.
   Remote reads carry a server snapshot token and the same range and
   projection as local reads. The snapshot stores ordered IDs and album
   boundaries. Page reads use those IDs, and no read transaction stays
   open between requests. Bound snapshot storage per connection and
   expire idle snapshots.
4. The dispatcher delivers the same owned page on the UI thread. The
   view renders and caches it without branching on backend type. The
   page includes the identity of each row and enough group information
   to render a jump without fetching preceding pages.

The C API and `SearchTrackListQuery` now share the typed search executor.
The C API uses a typed projected-page executor for local reads. Route
these operations through the selected library handle, with server
snapshots and projected pages for remote reads.

The current result exposes synchronous ID and album access by row.
Keep synchronous access on rows already in an owned page. Move arbitrary
position reads to the async range API before the ABI freezes. During
migration, the core can retain a bounded eager identity list for
existing actions that require the whole list.
[Current library API](../include/musikcore/library.h)
[Local backend](../src/musikcore/library/LocalLibrary.cpp)
[Remote backend](../src/musikcore/library/RemoteLibrary.cpp)
[Query registry](../src/musikcore/library/QueryRegistry.cpp)

## Lifetime and failures

Accepted requests retain their inputs and backend through completion.
A result binds to its originating library and search generation. A track
reference binds to its originating library and identity. Switching views
never changes the target of an existing handle. A deleted track keeps
its slot in an existing search and resolves as missing.

The dispatcher settles each accepted request once, including when a
reply, cancellation, and disconnect race. The remote backend asks the
server to stop work when possible and discards late replies. The view
applies results only to its current search generation. Disconnect
settles affected requests and invalidates
server snapshots. Materialized pages remain readable. Reconnect verifies
library identity before reusing track references. The app opens a fresh
search when a snapshot expires. Shutdown closes transports and drains
callbacks before releasing wakeup state.
[Context contract](../include/musikcore/context.h)

## What the current app already protects

Legacy remote search sends the ordered ID list to the client. Its row
cache fetches metadata by ID, so deleting a track cannot shift later
rows. The view checks that a query completion belongs to its current
query. On a library switch, the app stops playback and rebuilds the
library view.
[Legacy row cache](../src/musikcore/library/track/TrackList.cpp)
[Library switching](../src/musikcube/app/layout/MainLayout.cpp)

The new API makes those guarantees hold for callers that retain a result
or track reference across a library switch. Legacy playback resolves
media through the active library. An API track reference resolves media
through its originating library.
[Legacy playback](../src/musikcore/audio/PlaybackService.cpp)

## Routing by operation

Catalog reads, library edits, and indexing target the library handle.
Server capabilities determine which operations are available. Use an
internal compatibility path for older servers where their query protocol
can preserve the same contract.

Playback uses the client queue, decoder, output, and DSP. A remote track
resolves to a stream from its originating library. Controlling playback
on another machine targets an explicit playback session, independent of
the library supplying the track.
[Existing media routing](../src/musikcore/library/RemoteLibrary.cpp)

## Migration and tests

1. Move the context to the app and route local Track Search through a
   library handle. Covered by `musikcore.abi.library_switch` with two local databases.
2. Give remote Track Search the same result and page contract. Negotiate
   snapshot support on connect and test older-server behavior.
3. Run parity tests against a real loopback server with an indexed
   fixture. Cover order, grouping, field projection, deletion, distant
   jumps, cancellation, disconnect, and reconnect. Verify metadata and
   media separately over their respective listeners. Hold a page reply
   until after a switch or cancellation and verify that the stale page
   cannot update the view or change which track an action targets.
4. Switch production remote Track Search after those tests pass. Measure
   first viewport, cold and warm scrolling, request count, and bytes
   transferred before migrating browse, edits, indexing, and queue actions.
