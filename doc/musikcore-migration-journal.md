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
