# musikcore C ABI migration journal

## Branch checkpoint

Created `clangen/musikcore-c-abi-completion` from the current SDK migration branch.

- `3c3442ba9` records the ownership and borrowing guideline.
- `f64c0230f` adds context-owned selection, playback snapshots and live subscriptions, and the local indexer contract. It includes C ABI and loopback tests.
- `26b95d0bf` migrates cube callers to those contracts, removes direct playback-service and library-backend access, and extends the Browse TUI scenario.
- The full Linux Release suite passed all 44 tests. Browse, playlists, and remote loopback passed five consecutive focused runs. Native macOS and Windows verification is pending.

## Source-list parity

The Windows core target was missing the two playback and selection sources used by the SDK bootstrap. Added both to its manifest and compared the complete Unix and Windows core C++ source lists. A native Windows build is still pending.

## Remaining work

- Finish the app-facing settings, output, and runtime APIs, keeping view and overlay behavior in its existing subsystem.
- Make core runtime ownership and selection native to the public context. Remove the legacy startup and playback/library bindings as their responsibilities move into core.
- Verify that cube uses public headers and symbols, and run consumer, switch/reconnect, playback, and native acceptance tests.

## Output selection

Added driver and device discovery and selection to the public C ABI and header-only SDK. The C entry points live in the existing output implementation. Settings and the playback chooser overlays now consume copied names and device IDs instead of holding plugin objects. Selection rejects a stale driver name before applying a device change.

Verified all 44 Linux Release tests. The headless plugin test exercises SDK driver discovery, invalid selection, and fallback while reloading playback. The standalone ABI fixture verifies argument validation without an output runtime.

## Equalizer

The existing equalizer overlay now reads and writes public band snapshots. Core reuses the same preferences and reload path for the public ABI and the plugin environment. The overlay no longer looks up the plugin or edits its preference store.

All 44 Linux Release tests passed. In three consecutive focused runs, the Browse TUI scenario loaded the real equalizer plugin, changed a band, reset the gains, and opened Settings with the headless output selected. The C ABI fixture verifies the missing-plugin and invalid-gain cases.

## Playback transport

Settings and the existing playback chooser use the public SDK to select gapless or crossfade. Core enforces the WaveOut restriction when changing either the driver or the mode. Linux tests exercise the SDK mode round trip, ABI argument validation, and a headless Settings interaction that switches to crossfade.

## Preamp and replay gain

The preamp and replay gain overlays and the transport indicator use the public playback settings API. Playback and the API share the same configured gain values. Linux coverage includes settings round trips and rejection of invalid gains, and the Browse TUI scenario selects and displays track replay gain.

## Search and browse query types

The layouts now pass public match and sort values directly. Existing session files retain their stored match values. Sort labels live in the existing sort overlay, and cube no longer includes internal query headers. All 44 Linux Release tests passed, including selecting track-title sort from the headless Search view.

Browse, category search, and the playlist overlays use public metadata and category keys. Cube no longer includes the local database schema header. All 44 Linux Release tests passed.

## Visualizers

The chooser and hotkey use the SDK for discovery, show, and hide. Playback workers read one atomic selection and borrow plugin instances held by the registry. All 44 Linux Release tests passed, including the missing-plugin dialog and the standalone ABI export checks. The shipped Windows visualizer still needs native verification.

## Playlist notifications

Browse refreshes local playlist edits through public context callbacks. Deletion publishes the list change after the transaction scope closes. All 44 Linux Release tests passed. The loopback test covers event order and discards queued local events when the remote library is selected. Browse still exercises playlist editing through the headless UI.

## Queue readiness

The core queue can wait for due work independently of dispatch. Public remote sessions use this wait instead of polling, and shutdown interrupts it. All 45 Linux Release tests passed. The new queue test covers an earlier post interrupting a deadline, delivery remaining on the dispatcher, and cancellation of the wait.

## Library identity capture

Core selection and playback now capture backend identity directly. They no longer construct SDK wrappers or allocate temporary C handles to recover the identity. All 45 Linux Release tests passed, including the backend switch and remote generation tests.

## Managed runtime

The production C API can start the configured catalog, playback engine, and plugins through context creation. The context owns the runtime queue and wakes a blocking host for both immediate and delayed work. Shutdown stops producers before releasing the queue, and retained catalogs keep their closed backend storage valid.

The public-only integration test started playback through Null output, delivered a delayed reload on the owner thread, and restarted the runtime while retaining a catalog handle. It passed five consecutive runs. Cube startup still uses the legacy entry point pending the equalizer notification cutover.

Cube now also uses the managed context for startup and core shutdown. The equalizer overlay subscribes to context dispatch, allowing the core and UI queues to run separately. All 46 Linux Release tests passed. The managed runtime, Browse, and playlist cases each passed six consecutive focused runs.

The managed loopback test exposed a no-op when reconnecting to an unchanged address. Reconnect now replaces that connection. The test proves playback rejects a reference after switching libraries, reads still use the retained origin while its connection remains open, and reconnect invalidates the old generation. All 47 Linux Release tests passed.

## Direct catalog selection

The managed context selects concrete local or remote catalogs. Playback switches its queue and caches to the selected catalog, invalidates old queue snapshots, and clears delayed work tied to the previous catalog. Plugin metadata and indexing use the local catalog. `MasterLibrary` remains in older integration fixtures.

All 47 Linux Release tests passed. The managed loopback, runtime, and Browse cases each passed four additional runs, including the new stale queue snapshot checks.

Removed `MasterLibrary` and the legacy selection source after migrating the integration fixtures. The remote transport contract now exercises the managed context directly, and playlist notification ordering remains covered in the dispatcher contract. All 47 Linux Release tests passed.

## Typed preferences

Settings and Session now support typed reads and atomic in-memory batch writes through the C ABI, with explicit persistence. Search and Browse use the header-only preferences wrapper and retain their existing on-disk keys. The public runtime test covers scope isolation, text ownership, and invalid-batch rollback. All 47 Linux Release tests passed.

Startup, session navigation, theme, and remote settings now use public preferences. The remote editor applies endpoint fields as a batch and keeps the active connection when the endpoint is unchanged. The managed loopback test configures the endpoint through the public store. All 47 Linux Release tests passed.

## Server settings

The server overlay uses the public configuration API. Core and the provider share preference keys and defaults, and provider preference storage remains alive through shutdown. The native test verifies a configuration round trip with both listeners disabled. The Browse TUI test opens and dismisses the server overlay. All 47 Linux Release tests passed.

## Plugin catalog and settings

Cube uses public copied metadata and typed schema values for plugin discovery, enablement, and configuration. The existing schema overlay also renders advanced app preferences through the SDK. All 47 Linux Release tests passed. The headless Browse scenario edits the real Null plugin's sample rate and opens advanced settings.

Hotkeys now load and save through their public preference scope. The Browse scenario opens search with a fixture-specific override. Cube no longer includes the private preferences, plugin factory, or schema headers. All 47 Linux Release tests passed.

## Playback ownership

The managed runtime owns one playback engine directly. The engine opens its own public handles, and one registry tracks both handle validity and playback event subscriptions. Removed the private playback attachment and its duplicate lifetime token. All 47 Linux Release tests passed, including thread ownership, subscription lifecycle, and stale handle checks.

Library selection, remote reconnect, and local indexing now use the context-owned runtime directly. Removed the startup callbacks that attached these services to the context. All 47 Linux Release tests passed, including managed loopback and the four TUI scenarios.

## Shared preferences and paths

Cube uses public constants for settings shared with core and reads its application, home, and data directories through the C ABI. Persisted setting names are unchanged. The managed runtime test covers directory resolution and invalid arguments. All 47 Linux Release tests passed.

Theme reads, directory browsing, and hotkey backups use application-owned filesystem operations. All 47 Linux Release tests passed, including the Browse TUI scenario.

## Locale resources

Cube loads and selects the existing locale resources through the public SDK. Queries and UI share the same synchronized translation store, and C callbacks receive a snapshot outside its lock. All 47 Linux Release tests passed. The isolated managed-runtime fixture stages the English locale to check enumeration, selection, and fallback values.

## UI message queue

The application and core compile the same generic queue implementation, with a local copy in the Unix frontend. Cube no longer imports message queue methods from libmusikcore. All 47 Linux Release tests passed, including the queue unit contracts and four headless UI scenarios.

## Diagnostics

The managed runtime owns file logging and optional Piggy output. It delivers console logs on the context dispatcher through public callbacks. Cube's console consumes those records directly. All 47 Linux Release tests passed. The managed-runtime test checks delivery of a worker-thread log on the owner thread, and Browse opens the console and verifies a real library log is visible.
