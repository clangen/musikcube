# ADR-0018: Library layers and plugin services

Status: accepted.

## Context

Library operations passed through similarly named types with overlapping roles.
`LibraryInstance` retained separate `PlaybackOrigin`, `AsyncTrackReader` and
`LibraryQueries` views, often pointing to the same object. C entry points chose
between those views. `LibraryHost` looked like another application library even
though it implemented plugin callbacks. The native factory exposed a general
catalog registry although production only requested its two default catalogs.

Plugin services also used inconsistent host/API names, and general indexer
scheduling was embedded in the playback service table.

## Decision

Keep four explicit roles:

1. `api::Library` is the public header-only C++ owner of a C handle.
2. The application C API validates inputs and forwards operations to the core.
3. `musik::core::library::Library` owns instance scope and executes asynchronous
   operations. Handles, results and references share that same object.
4. `LibraryBackend`, `LocalLibraryBackend` and `RemoteLibraryBackend` are private
   catalog/connection implementations and typed-query schedulers.

The C++ wrapper is a caller of the C API, not an extra layer beneath it. Frontends
obtain their library from the context and do not choose native implementations.

Merge the consumer state, origin checks and operation interfaces into the core
`Library`. Local and remote executors inherit shared query construction and
result conversion at compile time, without another runtime delegate. Standalone
catalog reads retain their SQLite reader and existing supported operation subset.
Native reads remain ordered with catalog writes; remote requests retain their
connection generation. Scope snapshots, cancellation and result lifetimes remain
as specified in ADR-0017.

Replace the factory object, library vector and lookup map with namespace lifecycle
functions and accessors for the two defaults. Preserve persisted names/IDs,
conflicting-ID repair, lazy initialization, worker shutdown order and retained
message-queue ownership.

Plugin-facing adapters are separate from application wrappers. Name the existing
C++ owners `PluginLibraryService` and `PluginMediaService`; create no C++ class
merely to mirror a C table. Use these C names consistently:

| Service | Type |
| --- | --- |
| Library | `mc_plugin_library_service` |
| Playback | `mc_plugin_playback_service` |
| Indexer | `mc_plugin_indexer_service` |
| Media factories | `mc_plugin_media_service` |
| Preferences | `mc_plugin_preferences_service` |
| Audio scratch | `mc_plugin_audio_scratch_service` |

Use `indexer` and `playback` in `mc_plugin_services`. Keep `mc_host_api` for the
separate bootstrap/registration lifetime. Keep provider capability IDs and
provider factory/vtable names unchanged.

Move general scan scheduling to `mc_plugin_indexer_service::schedule_async`.
Source plugins receive only `request_rescan`, bound to their own source; general
scheduling remains null there. Playback plugins granted general scheduling retain
the same shared command queue, owner-thread completion and shutdown cancellation.

## Alternatives and rationale

Renaming all the old interfaces would leave the delegate graph intact. Merging
application and plugin APIs would mix context dispatch, plugin callback guards
and bootstrap lifetimes. Keeping a registry for hypothetical additional catalogs
would expose unsupported sequencing and ownership choices. The selected design
removes redundant interfaces and state while retaining real backend and ABI
boundaries.

## Consequences

Application C declarations and public C++ ownership behavior are unchanged.
Private C++ names and object layout change. The plugin table names, initializer
macros and two bundle field names are source-breaking changes. Moving scheduling
also changes the playback/indexer service layouts. Rebuild plugins with the new
experimental SDK; no compatibility aliases or old-layout adapters are provided.
Experimental ABI versions remain fixed until freeze.

The core library still translates typed queries into local or remote execution.
Those distinctions stay inside the core. Native-platform and dormant consumer
verification limits are recorded in the implementation checklist, not inferred
from the naming changes.
