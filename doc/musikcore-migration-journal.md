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
