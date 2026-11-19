# Unit and integration testing design

Status: proposed

## Summary

musikcube should introduce automated testing from the public musikcore C API
and C SDK boundary, then fan inward toward focused implementation units and
outward toward full application, plugin, and package integration tests.

The API and SDK boundary is the highest-value starting point because it defines
the behavior that applications, language bindings, and plugins are allowed to
depend on. Boundary tests remain valid while internal C++ implementations are
refactored, and they exercise ownership, errors, concurrency, dynamic loading,
and ABI compatibility that isolated unit tests cannot cover.

This document complements the
[high-level architecture](musikcore-c-abi-hld.md),
[low-level task plan](musikcore-c-abi-lld.md), and
[detailed C API/SDK design](musikcore-c-abi-design.md). The proposed C API and
SDK should be developed contract-first: externally observable behavior is
expressed as an executable test before or with its implementation.

## Current state

The repository does not currently register automated tests with CTest and has
no dedicated test source tree. `core_c_demo` exercises portions of the existing
C API, but it is an interactive demonstration rather than a deterministic test:
it uses hard-coded media paths, sleeps, real playback, and console output for
validation.

The current build also differs by platform:

- Unix-like builds produce a shared `musikcore` library.
- Windows currently builds `musikcore` as a static library.
- Plugins are separate dynamic modules, selected by platform and detected
  dependencies.
- Build outputs and runtime assets are written into source-tree `bin`
  directories in several configurations.
- No repository CI configuration is currently present.

The testing design should improve these constraints incrementally rather than
requiring a complete build-system rewrite before the first test can run.

## Goals

- Make the public API and plugin SDK executable contracts.
- Catch ownership, lifetime, error, event-ordering, and teardown defects.
- Test the actual shared-library and dynamic-plugin boundaries.
- Make async tests deterministic and deadline-based, without arbitrary sleeps.
- Run the default suite without real audio hardware, network access, or user
  library data.
- Isolate every test from user preferences and production databases.
- Support Windows, macOS, and Linux through the same CTest interface.
- Make fast tests easy to run locally during development.
- Add integration tests for SQLite, workers, plugins, playback, indexing, and
  packaging where isolated tests would provide false confidence.
- Support sanitizers, leak detection, ABI compatibility checks, and race tests.
- Produce failures that identify a violated contract rather than merely a
  process crash or timeout.

## Non-goals

- Achieving a line-coverage percentage before useful contract coverage exists.
- Testing third-party libraries themselves.
- Requiring physical audio devices in the default suite.
- Making network-dependent or hardware-dependent tests mandatory for every
  change.
- Testing curses rendering through pixel or terminal snapshots initially.
- Preserving `core_c_demo` as the primary validation mechanism.
- Replacing every manual exploratory test with automation in the first phase.

## Strategy: start at the boundary and fan out

The test program is organized into concentric layers:

```text
                  packaged application and archives
                /                                  \
          end-to-end application and plugin integration
             /                                      \
       public C API contract  <->  C SDK contract
             \                                      /
          internal components and adapter integration
                \                                  /
                  focused C++ and C unit tests
```

Implementation begins in the center of the diagram's behavioral value: the C
API and SDK contract. Tests then fan in and out:

- **Fan inward** when a boundary failure would be diagnosed or exercised more
  precisely with a unit test. Initial targets include handle ownership, page
  arenas, event queues, cancellation state machines, function-table adapters,
  plugin loading, and buffer lifecycle.
- **Fan outward** once the boundary works in isolation. Integration tests then
  combine the real database, workers, plugin loader, playback engine, app
  startup, and packaged runtime.

This is not a traditional unit-test pyramid where implementation details are
tested first. The stable boundary drives both the design and the supporting
unit seams.

## Test taxonomy

### Contract tests

Contract tests are black-box tests of a documented ABI. They use only installed
public headers and exported symbols. They do not include private musikcore
headers or link private object libraries.

There are two contract suites:

- Public C API contract tests, written in C11 and linked to the shared library.
- C SDK contract plugins, written in C11 and loaded through the real plugin
  loader.

These are the first required tests and run on every supported platform.

### Unit tests

Unit tests exercise one algorithm, state machine, or class with controlled
dependencies. They should normally avoid the filesystem, real clocks, worker
threads, dynamic libraries, and process-global state.

Examples include:

- UTF-8 view and caller-buffer behavior.
- Error translation.
- Metadata page arena layout.
- Handle retain/destroy state.
- Event queue ordering and coalescing.
- Async request and cancellation transitions.
- Track-list editing operations.
- Duration and serialization utilities.
- Plugin version negotiation.
- Audio frame and byte-count calculations.

Unit tests are not required to use the public ABI if testing a private
implementation is materially clearer.

### Component tests

Component tests exercise a subsystem through its normal interface while
replacing external dependencies. Examples include a playback service using a
deterministic decoder and fake output, or an indexer using a temporary database
and fake source.

These tests may use threads and temporary files, but must remain deterministic
and must not use production plugin or data directories.

### Integration tests

Integration tests combine real musikcore subsystems:

- Context plus SQLite plus worker queue.
- Async library query plus metadata page creation.
- Plugin discovery plus factory registration plus provider invocation.
- Decoder plus stream plus playback transport plus fake audio output.
- Indexer plus tag reader plus database commit or rollback.
- Shared-library consumer built separately from musikcore.

Integration tests run against temporary resources and deterministic media
fixtures. They do not require a user's music collection.

### System and packaging tests

System tests launch final executables or installed artifacts. Packaging tests
validate that an archive contains a usable shared library, headers, plugins,
assets, and correct runtime paths.

These tests are slower and may be separated from the default developer suite,
but release artifacts must pass them before publication.

### Hardware and network tests

Tests requiring a real audio device, CD drive, MPRIS session, network service,
or remote HTTP endpoint are opt-in and labeled. They may run in dedicated
environments or before releases, but are not merge gates unless a reliable
runner exists.

## Test directory layout

Proposed repository structure:

```text
tests/
    CMakeLists.txt
    support/
        c_test.h
        temp_directory.cpp
        test_context.cpp
        event_waiter.cpp
        fake_clock.cpp
        fixture_paths.cpp
    api/
        context_test.c
        error_test.c
        string_test.c
        event_test.c
        library_test.c
        track_page_test.c
        track_list_test.c
        playback_test.c
        indexer_test.c
        preferences_test.c
        audio_test.c
    sdk/
        loader_test.c
        lifecycle_test.c
        decoder_test.c
        encoder_test.c
        stream_test.c
        output_test.c
        malformed_plugin_test.c
        plugins/
            valid_decoder/
            incompatible_abi/
            truncated_tables/
            failing_initialize/
            invalid_provider/
    unit/
        event_queue_test.cpp
        request_state_test.cpp
        page_arena_test.cpp
        handle_test.cpp
        plugin_negotiation_test.cpp
        track_list_test.cpp
        serialization_test.cpp
    integration/
        library_query_test.cpp
        playback_pipeline_test.cpp
        indexer_pipeline_test.cpp
        plugin_pipeline_test.cpp
        shutdown_test.cpp
    system/
        musikcube_smoke_test.cpp
        musikcubed_smoke_test.cpp
    packaging/
        installed_c_consumer/
        installed_c_plugin/
        verify_exports.cmake
        verify_archive.cmake
    fixtures/
        metadata/
        playlists/
        media/
```

Directories can be added incrementally. The initial change only needs
`support`, `api`, `sdk`, and the first SDK fixture plugin.

## Build-system integration

### CTest

CMake's CTest integration should be the common test runner on all platforms:

```cmake
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

`BUILD_TESTING` defaults to the CMake convention and may be disabled for
minimal release builds. Test targets should write into the selected binary
directory rather than source-tree `bin` directories.

Use the test runner with an existing CMake build, or let it configure the
checkout with the default toolchain:

```sh
node script/run-tests.js
node script/run-tests.js --build-dir build/test -- -L abi
```

The runner enables `BUILD_TESTING`, builds the `musikcube_tests` target, and
passes arguments after `--` to CTest. Use `--config Release` to select Release
with a multi-config generator such as Visual Studio.

To configure a separate Debug build manually:

```sh
cmake -S . -B build/test -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/test --parallel
ctest --test-dir build/test --output-on-failure
```

To check the ABI header in an existing build with `BUILD_TESTING=ON`:

```sh
cmake --build build/test --target musikcore_abi_header_test
ctest --test-dir build/test -R '^musikcore\.abi\.header$' --output-on-failure
```

Equivalent presets should be added for Windows, macOS, and Linux after the
basic targets work. Windows' early CMake branch must explicitly include test
targets rather than returning before test registration.

### Test labels

CTest labels keep local and CI selection predictable:

- `unit`
- `api-contract`
- `sdk-contract`
- `component`
- `integration`
- `system`
- `packaging`
- `slow`
- `network`
- `hardware`

Examples:

```sh
ctest --test-dir build/test -L 'api-contract|sdk-contract'
ctest --test-dir build/test -L unit
ctest --test-dir build/test -LE 'slow|network|hardware'
```

### Test frameworks

CTest owns test discovery, execution, labels, timeouts, and reports.

Public C API tests and pure-C plugin fixtures should use a very small C test
harness maintained under `tests/support/c_test.h`. Keeping the boundary suite
in real C is part of the test: it prevents accidental reliance on C++ syntax,
linkage, layouts, or exception behavior.

Focused C++ unit tests should use one pinned test framework with good CMake and
CTest integration. Catch2 is the initial recommendation. The framework should
be a test-only dependency and must not become part of the musikcore ABI or
release runtime. It should be acquired through the same pinned dependency
mechanism used by other build inputs, not downloaded implicitly during an
offline configure.

The framework choice is less important than maintaining a single convention.
Contract tests must remain independent of it.

### Separate compilation

At least one C consumer and every SDK fixture plugin must be built as a
separate CMake target against the installed-style include directory. Contract
targets must not inherit musikcore's private include directories or compile
definitions.

The API contract executable should link the shared library exactly as an
external application would. SDK fixture plugins should link no private
musikcore implementation library; they receive all host services through the
C SDK function table.

This constraint catches missing exports, incorrect C linkage, hidden header
dependencies, allocator leakage, and accidental C++ ABI usage.

### Shared-library requirement

Contract testing requires a shared `musikcore` target on every platform.
Windows should produce the proposed DLL in addition to any temporary static
library needed during migration. The contract suite must run against the DLL,
not silently fall back to a static target.

This requirement applies only after the minimal C ABI foundation exists. The
initial CTest and unit-test substrate must preserve the current Windows static
production linkage. The first new Windows DLL is the explicitly experimental C
façade, not a shared build of the current public C++ surface.

## Test isolation and hermeticity

### Temporary directories

Every test that creates a context receives a unique temporary root containing
explicit data, cache, preferences, plugin, and media directories. Tests must
not read or write the user's home directory or normal musikcube database.

The test context builder deletes its temporary directory on success. It may
preserve it on failure when a diagnostic environment option is enabled.

### Dependency injection

Testable boundaries should allow injection of:

- Data and plugin directories.
- Time and timers.
- Random or request-ID generation where deterministic values matter.
- Data streams.
- Decoder, encoder, and output factories.
- Event queue limits.
- Logging sinks.
- Network transports where applicable.

Injection should occur through internal constructors or context options, not
through mutable process-global test hooks in the public ABI.

### Databases

Database tests use either an in-memory SQLite database or a database under the
test's temporary root. Schema creation and fixture population happen through a
shared test utility.

Tests must not depend on execution order. Each test starts from a declared
empty or populated state and owns its transactions.

### Media fixtures

To generate a database with the production indexer and tag readers, run
`node script/seed-test-library.js [configured-build-directory]`. The script
creates two tagged WAV files, indexes them in a fresh config root under
the selected build directory, and prints the resulting `musik.db` path.
Use the database's parent directory as the ABI0 context data directory.

Run `node script/seed-test-library.js --tracks 10000` for a larger fixture.
It generates 100 artists with 10 albums per artist and 10 numbered tracks
per album, distributed across five genres.
The fixtures also embed ten extended tag keys, with a mix of missing values,
repeated values across tracks, and tracks with more than one composer.

When Node.js is available, `musikcore.abi.indexed_query` seeds the two-track
fixture before querying it through ABI0. Run it with
`node script/run-tests.js -- -R '^musikcore\.abi\.indexed_query$'`.

Fixtures should be small, deterministic, and redistributable. Prefer generating
simple PCM WAV data during the test. Commit compact binary fixtures only when
the container, tag, corruption, or codec bytes are what the test validates.

Each committed fixture should have a short manifest describing:

- Its origin or generation process.
- License.
- Expected duration, sample rate, channels, and tags.
- Why a generated fixture is insufficient.

### No arbitrary sleeps

Tests must not use sleeps to wait for async state. They wait for a specific
event, request ID, state predicate, or condition with a bounded deadline.

A shared event waiter should report queued events and current state on timeout.
Timeouts are safety nets, not synchronization mechanisms. Default tests should
normally complete far before their timeout.

### No default hardware or network dependency

The default output is a fake or null output. The default test stream is memory
or local-file backed. HTTP, platform audio, removable media, and desktop-bus
tests are explicitly labeled and disabled unless their prerequisites are
requested and detected.

## Public C API contract suite

The API suite validates externally observable behavior without knowledge of
the internal implementation.

### ABI and header tests

- The installed header compiles as C11.
- The installed header compiles as C++ with C linkage.
- Public structures have expected size and alignment for the target ABI where
  those layouts are contractual.
- Public enums use documented fixed-width representations.
- Only documented symbols are exported.
- Runtime ABI version negotiation accepts compatible versions and rejects
  incompatible versions.
- Appending fields to sized option structures preserves older callers.

### Context lifecycle

- Default and explicit context creation.
- Invalid options, missing directories, and unsupported versions.
- Repeated create/destroy cycles.
- Defined multiple-context behavior.
- Destruction with no outstanding work.
- Destruction or async shutdown with queued events and requests in flight.
- Shutdown delivers pending terminal callbacks before returning.
- No accesses occur after the context is destroyed.

### Ownership and errors

- Every owned handle can be destroyed normally and through a null
  pointer-to-pointer.
- Successful destruction clears the caller's handle.
- Borrowed views remain valid for their documented parent lifetime.
- Destroying a parent invalidates no independently retained child.
- Failure paths return the documented code and optional error information.
- Async errors are owned by their event until explicitly taken.
- C++ exceptions are translated and never cross the ABI.
- Repeated failure does not leak handles or error objects.

### Strings and slices

- Empty, ASCII, UTF-8, embedded-null input where prohibited, and long strings.
- Caller-buffer required-size behavior for zero, exact, short, and oversized
  capacities.
- String views have correct byte length and documented termination behavior.
- Page-owned views remain stable until page destruction.
- Async calls copy or retain input views before returning.

### Async requests and dispatch

- Accepted requests receive exactly one terminal callback with the original
  user data. Rejected requests receive no callback.
- Callbacks run inline on the selected dispatcher thread, including shutdown
  callbacks. Nested and concurrent dispatch return busy.
- Cancellation before start, during execution, and racing with completion
  produces one terminal outcome.
- Callbacks may submit or cancel work outside internal locks.
- Wakeups can be coalesced. Enqueue racing with a drain cannot strand work.
- Queue pressure preserves accepted completions and lossless notifications.
- Shutdown settles accepted requests and waits for in-flight wakeups.
- Successful callbacks own their payloads. Error views are borrowed for the
  callback. Polled notifications have explicit destruction.

### Library queries and metadata pages

- Empty and populated queries.
- Filtering, ordering, limits, offsets, and multiple pages.
- Requested fields appear in the declared order.
- Missing values are distinct from invalid row or field indices where needed.
- Numeric and string access agree.
- Metadata values containing Unicode and unusually long text.
- Page snapshots remain stable after the underlying database changes.
- Invalidations are reported without mutating an existing page.

### Track lists and cache windows

- IDs can be retrieved without metadata loading.
- Visible-range metadata requests complete asynchronously.
- Cache-window requests do not block the calling thread on database work.
- Loading, loaded, missing, and failed states are observable.
- Overlapping and rapidly changing ranges produce coherent completion callbacks.
- Batched edits preserve order and are atomic where promised.
- Invalid indices and duplicate IDs follow documented behavior.

### Playback and queue

- Play, pause, resume, stop, seek, next, and previous state transitions.
- Queue replacement and batch editing.
- Repeat, shuffle, volume, mute, replay gain, and transport settings.
- Playback snapshots are internally coherent.
- Decoder, stream, and output failures become documented events.
- Commands are safe during startup, draining, failure, and shutdown states.
- Reentrant application behavior is safe because events are consumed outside
  internal locks.

These tests use a deterministic decoder and fake output. They verify frames and
state rather than requiring audible playback.

### Indexer and preferences

- Indexer path operations and scan types.
- Progress coalescing and lossless terminal state.
- Cancellation commits or rolls back according to the contract.
- Typed preference defaults, updates, batches, persistence, and isolation.
- Plugin and application preference namespaces do not collide.

### Advanced audio API

- Buffer format, capacity, frame count, mapping, unmapping, and recycling.
- Invalid buffer state transitions.
- Stream partial read, EOF, seek, interruption, and unknown length.
- Decoder open, decode, seek, duration, exhaustion, and failure.
- Encoder output ownership and finalization.
- No extra PCM copy is required solely to cross the ABI.

## C SDK contract suite

The SDK suite tests musikcore as a host of separately built C plugins. Fixture
plugins deliberately include both correct and hostile-but-nonmalicious ABI
behavior.

### Loader and version negotiation

- A valid plugin loads, initializes once, and shuts down once.
- Missing entry point is rejected cleanly.
- Incompatible major version is rejected before initialization.
- Older compatible sized tables load with absent optional functions.
- A table smaller than required is rejected.
- Unknown capability bits are ignored or rejected according to the contract.
- Duplicate plugin IDs produce deterministic behavior.
- Initialization failure returns a core-owned detailed error.
- The library is not unloaded while a factory or provider instance survives.

### Host services

- Bootstrap logging and error construction are usable at the documented time.
- Other services are unavailable before initialization begins.
- Plugin-scoped preferences use the correct namespace.
- Host-created audio buffers can be mapped and unmapped by a provider.
- Optional host functions are checked through `struct_size` or feature
  discovery before use.
- Calls after shutdown are prevented by the host.

### Provider object lifecycle

- Factory and instance ownership are independent and explicit.
- Each created provider is destroyed exactly once by its plugin vtable.
- Plugin instance memory is never freed by the core allocator.
- Provider errors created through the host survive until consumed or destroyed.
- Null required functions and invalid provider results are rejected without a
  host crash.
- Exceptions from C++ convenience wrappers are caught by their C trampolines.

### Decoder, encoder, and stream fixtures

A deterministic decoder plugin should:

- Advertise one synthetic media type.
- Read a small header from a host-provided stream.
- Generate a known PCM sequence.
- Support deterministic duration and seek behavior.
- Expose controlled failure modes selected by fixture URI.

Tests verify factory selection, partial reads, buffer boundaries, exhaustion,
seek accuracy, preferred sample rate, interruption, and cleanup.

Encoder fixtures verify stream output, packet ownership, flush/finalize
behavior, and failures without returning allocator-ambiguous `char **` data.

### Output and real-time contracts

A fake output plugin records submitted formats and frames without opening a
device. Tests verify buffer state transitions, draining, pause/resume, latency,
and device snapshots.

Debug builds should optionally instrument real-time calls to detect forbidden
host allocation, logging, or blocking-service use. Timing alone is not a
reliable real-time correctness assertion.

### Malformed fixture plugins

Dedicated fixtures should cover:

- Truncated function tables.
- Unsupported versions.
- Missing required functions.
- Inconsistent capabilities.
- Factory returning an invalid instance.
- Provider returning impossible frame counts.
- Plugin attempting registration after initialization.
- Plugin retaining a transient host view.
- Shutdown requested with instances still alive.

The goal is robust rejection, not sandboxing malicious native code.

## Internal unit and component tests

After the first boundary tests identify implementation seams, unit coverage
fans inward. Priority should be based on concurrency, ownership, and defect
risk rather than file count.

Initial high-value units are:

1. ABI version and sized-table negotiation.
2. Handle state and reference ownership.
3. Error translation and error-object ownership.
4. Event queue ordering, coalescing, wakeup, and shutdown.
5. Async request state machine and cancellation races.
6. Metadata page arena and typed accessors.
7. Track-list cache-window planning.
8. Plugin registry and unload eligibility.
9. C provider to internal C++ adapters.
10. Audio buffer state machine and frame calculations.

Existing pure or mostly pure code such as duration formatting, query
serialization, metadata conversion, and track-list editing can be added where
tests make future changes safer.

Component tests should then combine these units with controlled dependencies:

- Local library plus temporary SQLite.
- Query scheduler plus fake clock and event sink.
- Playback service plus deterministic provider fixtures.
- Indexer plus fake source and tag writer.
- Preferences plus temporary persistence.

## Integration and system tests

### Core integration

Core integration tests use the real shared library, worker threads, SQLite, and
plugin loader with a temporary runtime root. They verify complete workflows:

- Create context, index generated media, query it, and inspect a metadata page.
- Build a queue, play through the deterministic output, seek, and stop.
- Load a C decoder plugin, use it from playback, destroy all instances, and
  unload cleanly.
- Cancel indexing or queries during context shutdown.
- Reopen a context and verify persisted preferences and library state.

### Application smoke tests

Application smoke tests should be deliberately small. They validate startup,
initialization, and graceful shutdown rather than terminal rendering details.

For musikcube, a pseudo-terminal test should cover:

- A valid `TERM`.
- An unset or unparseable `TERM`, exercising the documented xterm-256-color
  fallback.
- Loading a temporary empty library and deterministic plugin directory.
- Sending the quit key and observing a zero exit status.
- Detecting aborts, sanitizer failures, and unexpected stderr output.

For musikcubed, start with a temporary root, wait for a ready indication, send
the supported shutdown signal or command, and verify clean exit.

Test-only command-line options may select data/plugin directories and disable
network listeners or physical audio. They should configure normal code paths,
not bypass initialization wholesale.

### Installed consumer test

The packaging suite installs musikcore into a temporary prefix, then configures
and builds a tiny external C project using only the installed headers and
library metadata. This catches include layout, import library, runtime search
path, and missing transitive dependency problems that an in-tree test can
hide.

### Installed plugin test

A second external project builds a C decoder plugin against the installed SDK.
The installed musikcore runtime discovers and invokes it. The plugin must not
link private musikcore or C++ implementation libraries.

### Archive tests

Standalone archive validation should extend the existing dependency scanner:

- Extract or stage the archive in a path unrelated to the source checkout.
- Verify expected executables, `libmusikcore`, public headers if shipped, and
  plugins.
- Verify ELF RPATH/SONAME, Mach-O install names, or Windows DLL placement.
- Run the installed C consumer and plugin fixture.
- Run application startup/shutdown smoke tests.
- Confirm no unexpected build-machine paths or dependencies remain.

## ABI compatibility testing

Before ABI major version 1 is frozen, tests may evolve with the design. Once a
stable release is published, compatibility becomes a release requirement.

The repository should retain:

- The previous stable installed headers.
- A small consumer binary or source fixture compiled against each supported
  stable ABI baseline.
- A plugin fixture compiled against the previous stable SDK.
- An allowlist of exported public symbols.
- Recorded public structure sizes and alignments per architecture.

A newer compatible library must run older consumer and plugin fixtures without
recompilation. Symbol and ABI inspection tools may supplement these executable
tests, but do not replace them.

Incompatible changes require a new ABI major, library filename/SONAME, and
plugin negotiation version.

## Concurrency, sanitizers, and fault injection

### Sanitizers

Supported configurations should include:

- AddressSanitizer for memory errors and leaks where leak detection is
  supported.
- UndefinedBehaviorSanitizer for C and C++ undefined behavior.
- ThreadSanitizer in a separate configuration for event, cancellation, plugin,
  and shutdown races.
- Platform-native diagnostics on Windows where compiler sanitizer coverage is
  insufficient.

Sanitizer runs should use dynamically linked test paths representative of
release behavior. Suppressions must be narrow, documented, and preferably
limited to known third-party code.

### Stress tests

Repeated lifecycle and concurrency tests should cover:

- Thousands of context create/destroy cycles in a dedicated slow test.
- Rapid query start/cancel loops.
- Queue edits while playback state changes.
- Plugin instance creation during attempted shutdown.
- Queue pressure with coalescible notifications and terminal callbacks.
- Repeated map/unmap/recycle audio-buffer transitions.

Stress tests are labeled `slow` and run nightly or before releases. Fast,
bounded versions remain in the normal integration suite.

### Fault injection

Fakes should support deterministic failures at specific operations: allocation,
stream read, decoder open, database step, plugin initialization, worker enqueue,
and output submission. Tests assert cleanup and externally visible errors for
each failure point.

Fault injection belongs at explicit seams. It should not rely on global random
failure rates that make failures difficult to reproduce.

## Headless track search

From the repository root, build the Unix app and fixture indexer, then run
the PTY scenario with Node.js and the system `script` utility. The harness
uses util-linux `script` on Linux and BSD `script` on macOS.

```sh
cmake --build . --target musikcube musikcube_seed_library
node tests/tui/track-search.js
```

The scenario indexes 10,000 tracks under the build tree alongside the staged
executables and indexing plugins. It opens Track Search, filters by album,
scrolls to a result below the initial viewport, then quits through the normal
key binding. It removes the fixture and staged runtime on completion.

The runner reports fixture generation and indexing separately from keystroke
latency. Filter latency includes the app's debounce interval. The scenario
uses the existing track search implementation and runs separately from the
ABI CTest suite.

Use `--screenshots <dir>` to save SVGs of the initial, filtered, and scrolled
views. For an out-of-tree build, pass `--build-dir` and, if needed, `--bin-dir`.

## CI and developer workflows

The test commands should remain CI-provider independent. An initial matrix is:

- Linux GCC Debug: unit, API contract, SDK contract, integration.
- Linux Clang Debug with ASan/UBSan: same suites.
- macOS AppleClang Debug: unit, contracts, integration, application smoke.
- Windows MSVC Debug: unit, contracts, integration, DLL/plugin smoke.

Longer jobs run on a schedule or release candidate:

- ThreadSanitizer.
- Stress and fault-injection suites.
- Standalone package and archive validation.
- Optional network and hardware tests.
- Coverage reporting.
- Additional architectures and minimum-supported runtime environments.

The fast local and merge-gate suite should target completion in a few minutes.
The API and SDK contract subset should remain small enough to run after every
ABI change.

Failed CI should preserve CTest logs, sanitizer reports, temporary runtime
state where useful, and crash dumps as artifacts.

## Coverage and quality gates

Line coverage is reported but is not the primary acceptance criterion. Initial
quality gates are behavioral:

- Every public API function has happy-path, invalid-argument, and applicable
  lifecycle coverage.
- Every required SDK function-table entry is exercised through a loaded C
  plugin.
- Every owned public type has create/take/retain/destroy coverage as applicable.
- Every async operation has completion, cancellation, failure, and shutdown
  coverage.
- Every documented event ordering rule has an assertion.
- No default test uses arbitrary sleeps, real user state, physical audio, or
  external network access.
- Contract, unit, and integration suites pass under supported sanitizers.

Coverage reports are used to find important untested branches. A global
percentage should not encourage tests of trivial accessors while concurrency
and error paths remain untested.

## Implementation phases

### Phase 0: test foundations

- Add `include(CTest)` and `BUILD_TESTING` handling to Unix and Windows builds.
- Add `tests/`, the C harness, common temporary-directory support, labels, and
  timeouts.
- Make context data and plugin directories explicit and isolated.
- Convert `core_c_demo` scenarios into a checklist of contracts; retain the
  demo only as a sample if it remains useful.
- Preserve the current Windows static production target and Unix production
  topology; do not introduce the experimental shared façade in this phase.

### Phase 1: public ABI contract

- Add C11 header, linkage, version, lifecycle, error, ownership, and string
  tests alongside the new ABI foundation.
- Add event queue and async request tests before library-query migration.
- Add immutable metadata page and cache-window contracts.
- Build and run against the shared library on all three platforms.

### Phase 2: SDK contract

- Add the pure-C decoder fixture plugin and loader tests.
- Add version, sized-table, initialization, instance, shutdown, and unload
  contracts.
- Add deterministic stream, buffer, decoder, and failure behavior.
- Confirm the fixture is built only against installed-style SDK headers.

### Phase 3: fan inward

- Extract and test event, request, page, handle, plugin-registry, and buffer
  units where boundary tests reveal complex state.
- Add component tests for library queries and playback using deterministic
  providers.
- Run ASan/UBSan continuously and introduce a separate TSan configuration.

### Phase 4: fan outward

- Add real SQLite, worker, plugin, playback, indexer, and persistence workflows.
- Add musikcube and musikcubed startup/shutdown smoke tests.
- Add installed consumer and installed plugin projects.

### Phase 5: packaging and compatibility

- Run contract and smoke tests from staged or extracted release artifacts.
- Establish exported-symbol and public-layout baselines.
- Retain previous-release consumer and plugin fixtures for ABI compatibility.
- Make package validation a release gate.

### Phase 6: expand coverage by risk

- Add encoder, output, DSP, tag-reader, and indexer-source SDK fixtures.
- Add regression tests for every fixed production defect.
- Add opt-in platform, hardware, and network coverage where reliable runners
  exist.

## Initial milestone

The first complete boundary milestone, reached after the static-tested ABI
foundation is defined, should be deliberately narrow:

1. CTest enabled on Windows, macOS, and Linux.
2. A real C executable linked to the shared musikcore library.
3. Contract tests for ABI version, context lifecycle, errors, strings, and
   event ownership.
4. One pure-C decoder fixture loaded through the proposed C SDK.
5. Tests for plugin negotiation, initialization, decoder creation, one known
   PCM result, destruction, shutdown, and unload.
6. ASan/UBSan passing on the supported Unix-like sanitizer configuration.

This milestone validates both directions of the ABI before broader musikcube
migration. It also creates the harness needed to develop each subsequent API
and SDK feature contract-first.

## Open questions

- Which C++ test framework should be pinned for internal units?
- Should test-only fake providers be statically registered for component tests,
  or always loaded as dynamic plugins?
- Which C compiler versions form the public-header compatibility matrix?
- What is the required fast-suite runtime for local and merge-gate use?
- Which existing media formats require committed binary fixtures?
- Which CI provider and artifact retention policy should be used?
- How should Windows crash dumps and sanitizer diagnostics be collected?
- At what release should ABI compatibility baselines become immutable?
- Which hardware and network integrations have sufficiently reliable runners
  to become release gates?

## Decision summary

Testing begins at the musikcore API and SDK boundary. Public behavior is tested
through separately compiled C consumers and pure-C fixture plugins using the
actual shared library and dynamic loader. Coverage then fans inward to units
that implement difficult ownership and concurrency contracts, and outward to
real database, playback, application, and package workflows.

CTest provides one cross-platform runner. Tests use isolated temporary state,
deterministic provider fixtures, explicit event waits, and no default hardware
or network dependencies. Once ABI version 1 is stable, previous-release
consumers and plugins become executable compatibility fixtures. This strategy
lets the new API and SDK be designed contract-first while creating a practical
path from the current untested codebase to reliable cross-platform releases.
