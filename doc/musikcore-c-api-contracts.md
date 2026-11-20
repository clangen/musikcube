# Current experimental core C API contracts

Status: active, ABI0; no binary compatibility promise between development builds.

The declaration inventory is in [musikcore-c-api-inventory.md](musikcore-c-api-inventory.md).
Regenerate it with `node script/test/audit-core-api.js`; CTest checks the exact
57-symbol declaration/allowlist correspondence. The header and library names
remain `mc_*`, `include/musikcore`, and `musikcore-abi0` for this development phase.

## Available surfaces

| Surface | Standalone ABI0 | Production-bound core |
| --- | --- | --- |
| Context, dispatch, cancellation, shutdown, errors | Supported | Supported |
| Local library identity, queries, search, result/range reads | Supported against `musik.db` | Supported through selected typed backend |
| Remote library identity | Can be created; does not connect | Internal adapter binds a live connection generation |
| Remote search and projected result reads | Unsupported without adapter | Supported; projection limited as documented in `library.h` |
| Category/directory selection and playlists | Unsupported without adapter | Supported through typed local/remote queries |
| Playback and queue snapshots/commands | No public session factory | App supplies a handle to its existing session |
| Plugin loading, notifications, preferences, indexing | Not implemented by this API | Remain private implementation services |

`plugin_directory` is currently copied but does not start a plugin loader.
Context creation does not open a library or start audio. Do not infer feature
support solely from successful context or library-handle creation. Unsupported
operations reject submission with `MC_ERROR_UNSUPPORTED` and no callback.

## Ownership and errors

Required handles and output slots must be non-null and valid for each call.
Initialize a handle output slot to null. Creation functions reject occupied
slots without changing them. Destruction accepts null and clears the slot.
Never share a mutable output slot across concurrent calls.

IDs are positive and scoped to their originating library. Zero is the missing
ID sentinel. Queue row absence is `SIZE_MAX`. Missing metadata has null data;
an explicit empty string has non-null data and zero length. A zero album
duration may mean either no group at that row or a zero/unknown duration; use
`starts_album` when the distinction matters.

All async inputs are copied or retained during submission. Accepted requests
retain their identities and result storage independently of caller handles.
Pages own strings. Result handles own stable membership, ordering, duplicates,
and album grouping, while subsequent page reads retrieve current metadata.
Removed tracks preserve result positions with no metadata; direct reference
reads report `MC_ERROR_NOT_FOUND`.

Callbacks receive status, payload, borrowed error, and user data in that order.
Array payloads include their count immediately after the pointer. Status is
authoritative. Success has no error. Failure supplies error details when memory
allows; applications must tolerate a null error on allocation failure. Errors
and their messages last only through the callback. Page/result handles transfer
to the callback; playlist arrays/names are borrowed for the callback. Playlist
write callbacks receive an ID value and own no payload.

No C++ exception may escape an ABI entry point. Scalar accessors and handle
destructors use nonthrowing operations; allocating/dispatching calls use the
common exception barrier. Callback exceptions are contained by dispatch, but
applications should not throw from callbacks: no public diagnostic channel
currently reports a swallowed callback exception.

## Dispatcher and request lifecycle

The first dispatch or shutdown call binds the dispatcher thread. Concurrent or
nested dispatch/shutdown returns `MC_ERROR_BUSY`. A completion callback may
submit or cancel requests. It cannot shut down or destroy its own context.
Destroy leaves the handle intact when shutdown is busy. C++ `Context` must be
destroyed on its dispatcher thread outside callbacks.

Each accepted request has one terminal callback. Cancellation selects a result
only before a terminal result is ready. It cannot retract an already selected
success/failure. IDs are nonzero and monotonic within a context; never use an
ID with another context. Callback data must remain alive until completion,
including cancellation or shutdown delivery.

Wakeups only signal the host loop. They can run on producer threads, must
return promptly, and must not dispatch/shut down/destroy the context. Wakeups
coalesce until dispatch rearms them. Shutdown stops admission, selects
cancellation for unfinished requests, joins the worker, waits for hooks to
return, and drains terminal callbacks. External replies may arrive later but
cannot deliver another callback or dereference the destroyed context.

The current process supports one context. Another create returns
`MC_ERROR_BUSY`. The context slot is released only by successful destruction.

## C++ wrapper policy

Every wrapper stays strictly header-only and uses public C declarations only.
Construction and async admission failures throw `api::Error`; asynchronous
completion uses status, owned value, and a borrowed message. Synchronous
commands expose the C status directly. Playback snapshot getters return
`nullopt` only for an ended/unavailable session and throw for other failures,
including wrong-thread access.

Views intentionally borrow storage rather than retain/copy strings. A page
move transfers the same storage, so existing views remain valid while its new
owner retains it. Destroying/replacing the final page invalidates its views.
Page view-producing accessors reject temporary owners. `TrackView::Values`
allocates a vector of views; it does not copy their string bytes.

Retain raw `mc_request_id` for now. A request wrapper would have to borrow the
nonmovable context and still could not prevent it being destroyed first.
Explicit IDs and `Context::Cancel` match the C lifetime model without implying
an additional retained context or automatic cancellation contract.

Playlist-list wrappers intentionally copy borrowed names into owned C++
strings so callers can retain the returned vector. Error messages and metadata
views remain borrowed. Futures/coroutines, if later added, must be optional
header-only adapters using the same C lifecycle.

## Examples and reproducible validation

`examples/core-query.c` and `examples/core-query.cpp` read a supplied local
database directory. Their host event adapter is in `tests/support/host-wakeup`;
it models an application's sleeping event loop and is not a musikcore service.
Both examples are built with public includes only and run against the generated
indexed fixture in `musikcore.abi.indexed_query`.

Run from a clean build directory:

```sh
node script/test/run-tests.js --build-dir build/core-debug --config Debug --abi-only
node script/test/run-tests.js --build-dir build/core-release --config Release --abi-only
```

For the production integration suite, configure its normal dependencies and run:

```sh
node script/test/run-tests.js --build-dir build/core-full --config Release
```

The ABI-only mode preserves the production target topology and avoids requiring
audio/curses/vendor dependencies to validate the standalone boundary. It does
not replace the full production integration suite.

`script/test/run-isolated-test.js` creates fresh test configuration and database
directories, removes successful runs, and retains failures for diagnosis.
Production fixtures stage their executable and only the two required tag-reader
and decoder plugins in a private runtime directory. They use fake playback
transport and loopback servers with dynamically allocated ports.

## Remaining validation limits

Mac builds and Mach-O checks do not establish Windows DLL or ELF behavior.
Those platform gates remain unchecked. Fixed ABI0 versioning also cannot
identify arbitrary stale development binaries with identical version numbers;
consumers must rebuild against the same revision pending a build-identity policy.
