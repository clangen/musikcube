# C ABI LLD adendum: library query scopes

Status: **complete; native macOS acceptance passed, with the maintainer-approved platform exception below**.

[LLD index](musikcore-c-abi-lld.md) · [C ABI design](musikcore-c-abi-design.md) ·
[Release gates](musikcore-c-abi-lld-release.md#phase-10-migration-cleanup-and-abi-v1-freeze)

## Goal

Provide a reusable, explicit way to restrict library browsing and queries by
indexer source ID before the application C ABI is frozen. Adding another
common filter later should not require adding a separate argument to every
query function. The mechanism must work with local and remote libraries and
must not expose SQL or database implementation details. A user must be able
to change the selected source set while the application is running, without
restarting the application or rescanning the library.

The initial scope contains only a set of source IDs. Visibility, directory,
related categories, playlist membership, rating, time and availability are
possible future filters and are outside this adendum. Do not add them to the
initial implementation or turn this into a general-purpose query language.
The shared mechanism must be extensible: adding a later filter should not
require changing every public query signature, replacing library ownership,
or inventing a separate filtering API.

## Starting point

Library operations already receive an opaque `mc_library` or derive a library
identity from a result or track reference. They are not all parameterized the
same way: track search and page query use different structs, while category,
category, album, directory and playlist operations use other arguments. Their
local implementations use several typed queries; remote operations serialize
those queries to the server. `MC_TRACK_FIELD_SOURCE_ID` exposes a track's
source ID, but there is no public source-listing operation.

Source ID zero identifies ordinary file-indexed tracks; indexer providers use
catalog IDs derived from their plugin ID and stable source key. Source IDs
belong to a catalog, not to all libraries globally. The indexer's scan-source
sentinels do not define the public filtering contract.

## Contract and decisions

Accepted lifecycle and playlist decisions are recorded in
[ADR-0011](../adr/0011-library-scope-query-lifetime.md).

Attach an optional, mutable scope to the library instance exposed by the existing
`mc_library` handle. Handles referring to the same instance share its scope;
independently created instances do not share scope merely because they address
the same catalog. Provide
explicit operations to set, replace and clear that scope while the application
is running; an unscoped library keeps its current behavior. Existing query
entry points continue to receive `mc_library`. Do not introduce a separate
public scoped-library type. A sized C scope struct carries the selected source
IDs and leaves room for future filter fields, and the header-only C++ wrapper
may provide matching convenience methods.

The sized C scope struct contains nullable pointers to typed filters. Every
non-null filter applies; the source filter owns an ID pointer/count for the
call, copied on update. A null source filter is unrestricted, while a present
empty filter matches no sources. No enabled-filter bitmask is used.

Older-sized scope inputs default absent trailing pointers to null. Reject
structs larger than the implementation understands, both for the scope and
its filter structs: accepting unknown appended filters could silently widen a
query. This deliberately trades forward-sized acceptance for strict filtering
semantics. The remote scope object likewise rejects unknown filter keys, and
uses a null `sources` field for unrestricted selection or an array for a source
selection. Validate sizes before reading pointer or count fields.

Each accepted scope-filtered request captures an immutable scope snapshot at
submission. Never retarget accepted work to a new selection. An effective scope
change invalidates pending scope-filtered requests from the previous selection:
they complete through the existing cancellation contract, never as successful
stale results. Interrupt backend work where practical; invalidation must still
suppress stale success when interruption cannot stop that work. Playlist-content requests are unaffected; playlist discovery is scope-filtered.

Already delivered results retain their original snapshot, including subsequent
page reads and track references derived from them. Completion admission and scope
replacement are ordered under the same lock. A completion admitted first may
finish after the update; otherwise it completes with cancellation. Invalidation
cancels requests promptly and discards pending remote queries without waiting for
replies. Scope-change notifications follow invalidation. Snapshot
capture, pending-request registration and invalidation must agree on that ordering
so a concurrent submission cannot escape invalidation. Callers need not manage
public scope revisions; an internal generation may enforce this contract.

The current `mc_library` stores a shared immutable `musik::core::library::Library`; preserve
backend ownership, remote routing, context selection and result/track lifetimes
when adding instance-owned mutable scope state. Operations using the context's
selected library observe that instance's scope. Different library instances and
catalogs must not share scope state accidentally.

A successful scope change must publish a library scope-changed event so
multiple API consumers can requery without polling. Delivery must occur
after the new scope is visible to subsequent queries. Reuse existing event or
subscription primitives where their ownership and threading semantics fit;
define the typed event, its originating library, callback thread and lifetime,
unsubscribe/shutdown behavior, and behavior for rejected or unchanged updates.
Pending-query invalidation prevents successful stale completions; observers
refresh on the event without managing public scope revisions. Specify how registration and the initial
scope read avoid missing a concurrent change. Do not call observers while
holding a lock they could need to query or update the scope.

The default must preserve today's unfiltered behavior. The contract must
separately represent no restriction, an explicitly empty source selection,
and selection of source ID zero. An explicitly empty selection returns no
matches for scope-filtered operations; clearing the restriction selects all
sources, including newly added sources. Playlist contents remain unaffected; empty selection lists only global playlists.
Treat source IDs as a set: order and duplicates do not change the selection,
invalidate requests or publish events. Validate IDs,
counts and pointer/count pairs at the owning boundary and copy caller-owned
arrays before returning from a scope update or query submission. Specify supported
ID range and a bounded selection size.

Define how a scope combines with existing search, category and directory
predicates. Source selections normally mean any of the selected sources;
other query conditions narrow that result. Apply restrictions before grouping,
counts, album-boundary calculations, sorting, offset and limit. A category or
album shared by several sources must reflect only matching tracks without
splitting its catalog identity or double-counting it.

Define semantics explicitly for each operation, especially:

- Searches, category lists, albums, directories and track pages.
- Playlist discovery shows global playlists plus those owned by selected
  sources; unrestricted shows all and an empty selection shows only global.
  Contents remain unfiltered and preserve order and duplicates. Creation must
  explicitly choose global or source ownership; source-owned playlists only
  accept tracks from that source. Reject mixed invalid batches atomically.
  Replacement and append preserve ownership. Scope updates invalidate discovery
  requests but not playlist-content reads.
- Direct track references, caller-supplied IDs and artwork: whether each is
  scope-limited or explicitly catalog-wide, including missing/out-of-scope IDs.
- Metadata mutations: whether they are catalog-wide, rejected on
  scoped libraries, or have another explicit contract. Filtering must not silently
  change or delete entries outside the user's intended operation.
- Result and track-reference lifetimes, equality/identity, cancellation, backend
  changes and shutdown. Existing accepted operations keep their original scope
  snapshot; pending scope-filtered requests are invalidated when the scope changes.

A scope is a query facility, not an authorization boundary. Server access
control remains independent. Do not infer visibility, offline availability,
source ownership or write permission from a source filter.

## Operation inventory and source discovery

The operation classifications below apply to both local and remote backends.

| Public operation family | Scope behavior | Existing typed path |
| --- | --- | --- |
| Track search | filtered | `TrackSearchQuery`, local `TrackReadBackend`, remote `SearchTrackListQuery` |
| Category and directory track results | filtered | `LibraryQueries::OpenSelection()` |
| Category values | filtered before grouping | `FilterCategoriesByName()` / `GetRelatedCategories()` |
| Albums | filtered before grouping | `LibraryQueries::ListAlbums()` |
| Metadata field discovery | catalog-wide schema | `LibraryQueries::ListCategoryFields()` |
| Direct page query | title browsing filtered; explicit ID projection catalog-wide | `TrackPageQuery` / `AsyncTrackReader::ReadPage()` |
| Playlist listing | global plus selected source owners | `LibraryQueries::ListPlaylists()` |
| Playlist contents and writes | contents unfiltered; source-owned writes enforce matching sources | `LibraryQueries` playlist operations |
| Existing result pages and derived references | retain original membership/snapshot; playlist pages remain catalog-wide | `mc_track_result_read_async()`, `mc_track_result_get_ref()` |
| Caller-supplied result IDs | catalog-wide explicit selection | `mc_library_make_track_result()` |
| Track metadata reads and rating writes | catalog-wide explicit identity operations | `mc_track_ref_read_async()`, `mc_track_set_rating_async()` |
| Artwork by cover ID | catalog-wide explicit identity | `LibraryQueries::ReadThumbnail()` |

The public API must let the UX enumerate sources with catalog-stable IDs and
human-readable labels. Discovery is catalog-wide regardless of the attached
scope, so deselected sources remain available to turn back on. It must use the
same library/context selection and asynchronous completion conventions as other
library lists, with callback-borrowed entries and explicit error/cancellation
semantics. The C++ wrapper must expose the same operation. Carry discovery
through a typed local query and the server query registry for remote catalogs.

Source zero represents ordinary files; its current display name is "Music files".
As agreed during
review, do not add a separate source metadata table. Discovery combines immutable
source descriptions from loaded plugins with IDs referenced by tracks and
playlists. Loaded providers include empty sources. An absent provider's referenced
IDs remain discoverable with a fallback label (`Source N`); its historical display
label and never-populated unreferenced sources are not persisted. Stable source
IDs continue to derive from plugin ID and source key, never from labels. Source
descriptions live on the local backend, independently of indexer shutdown.

## Implementation progress and acceptance evidence

The C API and header-only C++ wrapper expose instance-owned scope mutation,
snapshots, notifications, source discovery and explicit playlist ownership.
Local and remote executors share query construction/result adapters; execution
and transport stay in separate files. Typed queries carry copied scope values
and apply bound source predicates before grouping and pagination. Explicit IDs,
existing results, artwork, playlist contents and writes remain catalog-wide.

Schema version 12 adds nullable playlist ownership and membership triggers;
existing playlists migrate as global. Typed mutations validate entire batches,
retain ownership on replace/append and report query-specific rejection results.
The server exposes session-local get/set/clear scope operations and notifications.
Both typed queries and legacy browsing handlers apply the session selection.

Named acceptance tests:

- `musikcore.integration.api.library_scope`: public validation, copied storage,
  normalized updates, aliases, independent instances, observers, queued
  cancellation, completion admission and source-zero/empty selection.
- `musikcore.abi.typed_track_queries`: local predicates, shared album identities,
  source-zero/unknown/empty selections, hidden tracks, ordering and pagination.
- `musikcore.integration.library.playlist_source_schema`: fresh/version-11
  migration, ownership triggers, duplicate preservation and source wire data.
- `musikcore.integration.api.backend_parity`: local/remote scoped browsing and
  owned playlist creation/discovery/rejection.
- `musikcore.integration.api.managed_remote`: cancellation before held replies,
  prompt reuse of a bounded remote request slot, and rejected host playlist writes.
- `musikcore.abi.remote_query_transport`: transport cancellation and teardown.
- `musikcore.integration.api.remote_audio`: actual server browsing under an empty
  session scope, typed queries, restoration and rejection of malformed scope objects.
- `musikcore.integration.runtime.indexer_provider_cancel`: source discovery after
  indexer shutdown retains descriptions of loaded providers with no catalog rows.

Native macOS validation also includes `musikcore.abi.header`,
`musikcore.abi.shared_c`, `musikcore.abi.exports`, and the C++ query/search
consumer tests. Independently compiled public C/C++ headers build successfully.
`musikcore.integration.api.installed_sdk` verifies the relocated SDK and both
scoped example consumers. The final 2026-11-25 native macOS run passed all 16
selected CTest checks, including runtime services and indexer cancellation.
The three affected remote suites also passed after the final timeout-lifetime
review fix; accepted test callbacks cannot outlive borrowed state on timeout.
New regressions for rejected host playlist writes and source discovery after
shutdown fail when their respective fixes are temporarily removed.

The final acceptance additions cover:

- malformed creation options rejected before backend admission; separate catalogs
  with overlapping IDs; retained result pages and references after scope changes;
- concurrent submission/replacement, subscription registration during delivery,
  reentrant observer removal while another callback runs, and context shutdown;
- invalidation before refresh notifications and reentrant refresh queries;
- shared artists/albums and scoped grouped durations/categories on both backends;
- mixed-source and missing-track create/append/replace batches, source zero and
  provider ownership, duplicate ordering, unchanged contents and playlist counts,
  and absence of mutation notifications after rejected writes;
- playlist contents surviving scope changes while discovery is cancelled;
- actual server clients with independent selections, unchanged/rejected updates,
  correctly addressed broadcast notifications, reconnect reset, catalog-wide
  source discovery, and legacy playlist pagination under empty scope;
- independently installed C/C++ examples querying empty and source-zero scopes.

The new real-server regression failed because scope notifications lacked the
standard broadcast type and message ID. Those fields are now present; only the
originating session receives the committed update.

### WebSocket ordering and ownership

`get_library_scope`, `set_library_scope` and `clear_library_scope` operate on the
authenticated connection's selection. New/reconnected sessions start unrestricted.
The options object uses `sources: null` or an array of nonnegative IDs; absent
sources means unrestricted. Unknown keys, non-object options, negative IDs and
oversized selections are rejected without changing scope or publishing events.
Equivalent normalized selections publish nothing.

An effective update sends a `library_scope_changed` broadcast to that connection,
with the standard message ID/type and the committed scope in `options`. Other
connections receive neither that selection nor its scope event. A typed request
with an explicit scope owns that snapshot; otherwise the handler copies the
current session selection before query submission. Legacy browsing uses that
same selection. Explicit IDs and playlist contents remain catalog-wide.

Server handlers serialize query admission and session updates on the I/O thread.
The synchronous typed-query response is admitted before a subsequent session
update; the update cannot retarget that query. Client library scope replacement
uses normal cancellation/discard for requests still pending on the client, with
late replies ignored. This matches the completion admission rule in ADR-0011.

### Accepted platform exception — 2026-11-25

Native macOS builds, public C/C++ header compilation, installed/relocated consumer
checks and the named scope tests pass. No native Linux/Windows run is claimed.
Those runners are unavailable in this workspace.

The maintainer explicitly approved closing this feature LLD on native macOS
acceptance evidence. Native Linux/Windows revalidation of the same named scope
and installed-SDK tests remains mandatory before ABI v1 freeze under FIN-008,
FIN-009 and GATE-1000. This closes SCOPE-011 and SCOPE-014 for the accepted feature
scope; it does not freeze the experimental ABI or waive release-platform checks.
The release LLD records the remaining native-platform requirement.

There are no frontend implementation or UI test tasks in this LLD; the removed
SCOPE-012 identifier is intentionally not reused.

## Implementation checklist

- [x] **SCOPE-001** Inventory the public library operations and their typed
  local/remote query paths. Classify each as scope-filtered, catalog-wide, or
  unsupported on scoped libraries; resolve the operation semantics above before
  changing public declarations. Record the decision and alternatives in an
  individual numbered ADR, linked from the HLD and this adendum.
- [x] **SCOPE-002** Define the application C library-attached scope contract,
  including set, replace, clear, initialization, source ID zero, unrestricted
  versus empty, input limits, validation, copying, aliasing and lifetime.
  Add only fields required for the initial source filter; design extension
  through the existing sized-struct rules. Use nullable typed-filter pointers; default absent fields to null and
  reject oversized unknown scope/filter layouts instead of silently widening
  results. Keep ABI versions experimental.
- [x] **SCOPE-003** Provide a usable catalog-scoped source-discovery operation
  or identify an existing public operation that supplies stable IDs and human
  labels. Specify ordinary file tracks, disabled or absent providers, sources
  with no tracks, and remote catalogs. Do not infer IDs from display labels.
- [x] **SCOPE-004** Implement runtime source-selection changes and propagate
  immutable scope snapshots through library identities, async submissions,
  results and track references. Update the existing library handle; do not
  require creating another library type. A change applies to subsequent
  operations without restarting or rescanning. Specify and test how separately
  obtained handles and the context-selected library observe updates. Publish
  a scope-changed event after each effective change. Invalidate pending
  scope-filtered requests atomically with that change; equivalent selections
  (including reordered or repeated source IDs) cause no event or invalidation.
- [x] **SCOPE-005** Apply the filter to local typed queries for track search,
  projections, category and album browsing, directory reads and any
  other scope-filtered operations from SCOPE-001. Share predicate construction
  where it simplifies callers; bind every source ID, including each `IN` value,
  and choose SQL identifiers from fixed fragments. Do not push application
  query types into the generic database wrapper.
- [x] **SCOPE-006** Implement the decided playlist, direct-ID, artwork and
  mutation behavior. Preserve playlist order and duplicates, missing-row
  behavior, track identity and explicit catalog-wide operations as specified.
- [x] **SCOPE-007** Carry scopes through remote typed queries and the existing
  server query registry. Validate inputs on the boundary that owns them;
  unsupported/malformed filters must fail explicitly, never silently broaden a
  request. Leave a structured extension point for future filter fields without
  requiring a new wire operation for each filter. Clients and servers update in
  lockstep for 4.0; do not add an old
  server fallback or expose raw SQL.
- [x] **SCOPE-008** Update the header-only C++ convenience API for callers
  that require source filtering. Keep ownership explicit and wrappers dependent
  only on the public C ABI. Do not add unrelated filters to this task.
- [x] **SCOPE-009** Add boundary tests for invalid sizes, ID/count limits,
  null/count mismatches, empty and unrestricted selection, source zero, copied
  caller storage, ownership, cancellation and shutdown. Test older-sized input
  against a newer library and a larger unknown scope/filter struct against the
  current library, including truncated present filter fields. Assert
  rejection occurs before backend/provider work and scoped requests cannot
  escape their catalog.
- [x] **SCOPE-010** Add local and real remote parity tests with multiple sources,
  shared artists/albums, duplicate playlist entries, hidden or missing tracks,
  distinct library handles, scope updates, grouped counts, ordering and page
  boundaries.
  Test the explicitly decided ID, artwork and mutation semantics. Gate and
  reverse request completions while changing sources to prove older responses
  complete with cancellation rather than successful stale results. Cover queued
  completions, concurrent submission and update, and already delivered result
  paging. Verify playlist-content requests remain unaffected and playlist discovery is invalidated.
- [x] **SCOPE-011** Update public header/default-initializer checks, exported
  symbols, ABI-only and installed C/C++ consumer tests, wire fixtures, examples
  and contract documentation. Validate named-platform builds and tests under
  the normal project gates; keep missing platform evidence open rather than
  assuming it from another platform.
- [x] **SCOPE-013** Implement and test library scope-changed notifications.
  Cover multiple observers, update ordering, unchanged and rejected updates,
  reentrant queries, subscription lifetime and shutdown, registration racing
  an update, and pending-query invalidation before refresh delivery. Verify callbacks run
  outside internal locks and no observer sees an event for an uncommitted scope.
- [x] **SCOPE-014** Complete the scope acceptance gate: all applicable tasks
  above are complete, the ADR and public contract match implementation, and
  the maintainer has reviewed any SDK/API contract decisions. Only then may the
  application ABI v1 freeze proceed under FIN-008/FIN-009 and GATE-1000.

## Additional public and remote requirements

- [x] **SCOPE-015** Add explicit playlist ownership to the public C creation API
  and playlist discovery results. Use a sized creation-options struct with a
  default initializer; distinguish global ownership from source zero. Do not
  infer ownership from the current scope. Preserve ownership on append/replace.
  Define matching C++ convenience operations and remote wire fields. Existing
  creation callers continue to create global playlists.
- [x] **SCOPE-016** Enforce same-source membership for source-owned playlists at
  the API mutation boundary and in catalog constraints. Reject whole invalid
  create/append/replace batches without changes or success events. Test local
  and real remote behavior, missing tracks, source zero, duplicates and mixed
  sources; global playlists continue to accept mixed sources.
- [x] **SCOPE-017** Add WebSocket get/set/clear library-scope operations and
  committed scope-change notifications. Scope state is client-session-local,
  not shared mutable state on the server catalog. Validate the same sized/typed
  filter contract and reject unsupported filters. Define ordering with typed
  query snapshots and pending invalidation, disconnect/reconnect reset behavior,
  and tests proving one client's selection cannot affect another. Retain scope
  snapshots on each typed request so queued work cannot be retargeted by a later
  server-side selection. Include source discovery and playlist ownership fields.

The acceptance gate includes SCOPE-015 through SCOPE-017. Checked tasks record
implemented contracts and the named executable acceptance evidence above. The
accepted platform exception is explicit and does not imply ABI v1 freeze approval.
