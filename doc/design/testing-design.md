# Unit and integration testing design

Status: active foundation; provider and package validation remain planned

## Summary

musikcube tests start at the public musikcore C API boundary, then fan inward
toward focused units and outward through production integration and the TUI.
Apply the same approach to the C SDK after the core API is integrated into the
app; provider implementation remains deferred by the HLD/LLD sequence.

The rationale, alternatives, and scope of that choice are recorded in
[ADR-0004](../adr/0004-test-the-public-boundary-first.md). This document
specifies the harness and test layers; it does not maintain a second decision
record.

This document complements the
[high-level architecture](musikcore-c-abi-hld.md),
[low-level task plan](musikcore-c-abi-lld.md), and
[detailed C API/SDK design](musikcore-c-abi-design.md). The proposed C API and
SDK should be developed contract-first: externally observable behavior is
expressed as an executable test before or with its implementation.

## Starting point

Before the migration, validation relied in part on `core_c_demo`, an
interactive example with hard-coded media paths, sleeps, and real playback.
The demo and its C API have been retired. The indexed-library fixture now
uses the internal C++ indexer and the ABI contract tests run through CTest.

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
- Isolate every test from user settings and production databases.
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
- Requiring whole-screen golden snapshots for ordinary UI behavior.
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

Implementation begins with the C API contract. Apply the same process to the
SDK only after core application integration. Tests then fan in and out:

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

Use three execution levels. Contract coverage and optional platform/package
checks describe guarantees or environments, not additional parallel levels.

### Unit tests

Exercise one algorithm, state machine, or class with controlled dependencies.
Normally avoid databases, external processes, dynamic libraries, and global
runtime initialization. Examples include error/string helpers, queue ordering,
callback validity, and the terminal screen model. Tests can use a private
implementation when that gives clearer evidence than the public boundary.

### Integration tests

Exercise real boundaries: the shared library, SQLite and typed queries,
workers/dispatch, loopback transport, or production playback/library services.
Component checks with fake audio or other external dependencies also belong
here. They stay deterministic, use private roots and ports, and require no
user collection or hardware. Installed-consumer/package checks are binary
integration tests; provider integration remains deferred with the SDK.

### UI tests

Send real input through musikcube in a PTY and assert visible state, ordering,
selection, and outcomes. Use independent small correctness workflows for routine
runs and explicit larger/repeated commands for performance observations. A fake
child validating PTY plumbing is an integration test of the driver, not proof
of application UI behavior.

### Contract coverage

Black-box public ABI tests use only public headers and exported symbols, with
separately compiled C11 consumers crossing the real shared boundary. They carry
`contract` alongside their execution level. Tests that intentionally inspect
private adapters must not be described as external consumers. Pure-C SDK plugins
will apply the same rule when provider work begins.

### Optional environments

Physical audio, CD drives, desktop services, external endpoints, sanitizers,
and staged release packages require explicit environment/selection policies.
They do not replace the normal hermetic suite or imply native platform coverage
from a different host's results.

## Test directory layout

The boundary-first rationale is in
[ADR-0004](../adr/0004-test-the-public-boundary-first.md). This section owns
the test organization.
Tests live at the monorepo root under `tests/unit/`, `tests/integration/`, and
`tests/ui/`; unit directories identify their owning project. Shared native
fixtures and `check.h` live in `tests/support/`. Runners and build checks live
in `tests/scripts/`; TUI scenarios and driver support stay together in `tests/ui/`.
Captured comparison baselines belong in `tests/baselines/`.

Do not keep executable test cases in a directory called `fixtures`. The remote
transport suite lives in `tests/integration/transport/`; its reusable host and
fake-transport helpers live in `tests/support/`. Optional future provider and
package suites belong under the appropriate integration boundary and do not
require another parallel test hierarchy.

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
node tests/scripts/run-tests.js --suite unit
node tests/scripts/run-tests.js --build-dir build/test --config Release --suite integration
node tests/scripts/run-tests.js --build-dir build/test --config Release --suite ui
node tests/scripts/run-tests.js --build-dir build/test --config Release --list
node tests/scripts/run-tests.js --build-dir build/test --config Release -- -L musikcore
```

The runner enables `BUILD_TESTING`, builds `musikcube_tests`, and passes
arguments after `--` to CTest. Without `--build-dir`, it uses
`build/tests-<configuration>`; `--jobs` bounds build parallelism. `--list`
configures and lists tests without compiling them. Use `--config Release` for
Release, including multi-config generators such as Visual Studio. `--abi-only`
omits production service/UI dependencies while retaining the standalone ABI
and lightweight unit/driver checks.

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

Each registered test has exactly one level (`unit`, `integration`, or `ui`),
an owner (`musikcore` or `musikcube`), and useful subsystem labels such as
`api`, `library`, `runtime`, or `terminal`. Add `contract` for a documented
promise; it does not replace the level. Tests involving SQLite, real sockets,
or the shared library are integration checks. Future opt-in hardware/network
checks must be selected explicitly.

```sh
ctest --test-dir build/test -L '^unit$'
ctest --test-dir build/test -L '^integration$'
ctest --test-dir build/test -L '^ui$'
ctest --test-dir build/test -R '^musikcore.integration.api.production.playback_commands$'
```

### Test frameworks

CTest owns discovery, selection, timeouts, and reports. Keep public consumers
in real C11. `tests/support/check.h` reports the expression and source location
without dictating whether a case returns or jumps to resource cleanup.

C++ fixtures expose named cases through their existing harnesses; broad wrapper
contracts use `--case`, and CTest registers public-only and production-bound
cases independently. JavaScript uses Node's built-in test runner. No new native
framework is required for this organization. If authoring or diagnostics later
justify one, pin it as a test-only dependency with offline configuration support;
keep it out of the public boundary and shipped runtime.

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
explicit data, cache, settings, plugin, and media directories. Tests must
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
`node tests/scripts/seed-test-library.js [configured-build-directory]`. The script
creates two tagged WAV files, indexes them in a fresh config root under
the selected build directory, and prints the resulting `musik.db` path.
Use the database's parent directory as the ABI0 local library locator.

Run `node tests/scripts/seed-test-library.js --tracks 10000` for a larger fixture.
It generates 100 artists with 10 albums per artist and 10 numbered tracks
per album, distributed across five genres.
The fixtures also embed ten extended tag keys, with a mix of missing values,
repeated values across tracks, and tracks with more than one composer.

When Node.js is available, `musikcore.abi.indexed_query` seeds the two-track
fixture before querying it through ABI0. Run it with
`node tests/scripts/run-tests.js -- -R '^musikcore\.abi\.indexed_query$'`.

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

### Indexer and settings

- Indexer path operations and scan types.
- Progress coalescing and lossless terminal state.
- Cancellation commits or rolls back according to the contract.
- Typed setting defaults, updates, batches, persistence, and isolation.
- Plugin and application setting namespaces do not collide.

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
- Plugin-scoped settings use the correct namespace.
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
- Settings plus temporary persistence.

## Integration and system tests

### Core integration

Core integration tests use the real shared library, worker threads, SQLite, and
plugin loader with a temporary runtime root. They verify complete workflows:

- Create context, index generated media, query it, and inspect a metadata page.
- Build a queue, play through the deterministic output, seek, and stop.
- Load a C decoder plugin, use it from playback, destroy all instances, and
  unload cleanly.
- Cancel indexing or queries during context shutdown.
- Reopen a context and verify persisted settings and library state.

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

Unix Clang/GCC Debug builds default to AddressSanitizer and
UndefinedBehaviorSanitizer for all repository C/C++ targets, including the
reduced ABI library, SQLite, direct dispatch fixtures, applications, and plugins.
Configuration happens before targets are created. Instrumentation is Debug-only;
Release builds and standalone release packages stay uninstrumented. Prebuilt
vendor archives are not rebuilt by this option.

Select `MUSIKCUBE_SANITIZERS=address,undefined`, `address`, `undefined`, or `none`
when configuring CMake. The test runner forwards `--sanitizers` and preserves a
cached selection when that argument is omitted:

```sh
node tests/scripts/run-tests.js --build-dir build/tests-abi-sanitizers --config Debug --abi-only --sanitizers address,undefined
node tests/scripts/run-tests.js --build-dir build/tests-abi-plain --config Debug --abi-only --sanitizers none
```

Both sanitizers fail the process on a detected error. The
`musikcore.integration.binary.sanitizers.c` and `.cpp` cases prove this using
ordinary execution followed by deliberate heap overflow and signed overflow in
child processes. These cases are registered only for instrumented Debug builds.
On macOS, `musikcore.integration.binary.sanitizer_reload` additionally checks
repeated module unload/reload and a real global overflow after reload.
AppleClang 21 needs narrowly scoped image-global unregister cleanup to avoid
stale shadow-memory reports after `dlclose`; this keeps global checks enabled.

Windows sanitizer configuration and native execution remain deferred. This
profile rejects explicitly requested unsupported compiler/platform combinations
instead of silently claiming coverage. ThreadSanitizer remains a separate future
configuration; it cannot be combined with AddressSanitizer.

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
node tests/ui/scenarios/track-search.js
```

The scenario indexes 10,000 tracks under the build tree alongside the staged
executables and indexing plugins. It opens Track Search, filters by album,
scrolls to a result below the initial viewport, then quits through the normal
key binding. Successful runs remove the fixture and staged runtime; failures
retain them with diagnostics.

The runner reports fixture generation and indexing separately from keystroke
latency. Filter latency includes the app's debounce interval. The scenario
uses the production Track Search implementation. Small correctness workflows
are registered with CTest; larger repeated benchmark runs remain explicit.

Use `--screenshots <dir>` to save SVGs of the initial, filtered, and scrolled
views. For an out-of-tree build, pass `--build-dir` and, if needed, `--bin-dir`.
Use `--runs 20` to reuse one fixture across fresh app launches and report
median and p95. Pass `--tracks 30000` to check how the baseline scales.

Build the ABI0-only pilot with `BUILD_TESTING` enabled to compare both search
views against the same fixture.

```sh
cmake --build . --target musikcube_abi_search_pilot
node tests/ui/scenarios/track-search.js --compare --runs 20
```

To compare two pilot builds, save the control executable and its matching
`libmusikcore-abi0` shared library together before rebuilding. Pass the
directory containing both files to `--pilot-control`.

```sh
node tests/ui/scenarios/track-search.js --pilot-control build/search-control --runs 20
```

Both variants use the same fixture and alternate launch order. The driver
keeps the track-search actions and assertions. Other TUI benchmarks can
reuse `tests/ui/support/terminal.js` for the PTY and process samples, and
`tests/ui/support/benchmark.js` for repeated runs and median/p95 reporting.

On macOS, the terminal driver launches the system `script` utility through a
Bash pipe bridge. Node's subprocess stdin is socket-backed; macOS `script`
accepts ordinary pipes but fails its terminal probe on that socket. A `cat`
feeder supplies a real pipe and is stopped when the app exits. The launcher
preserves the app's status and closes its process group on scenario failure.
`node --test tests/ui/driver/terminal.test.js` exercises input, exit status, and
cleanup; CTest registers it as `musikcube.ui.terminal_driver` on macOS.

The historical Linux production baseline comes from a 20-run comparison at
30,000 tracks. Both variants used the same fixture, search API, and renderer.
The comparison measured median peak RSS
of 32,224 KiB with pages and 32,190 KiB with the legacy row cache.
Median CPU time was 70 ms in both modes. A jump to the last viewport
took 4.6 ms with pages and 1.2 ms with the legacy cache. The page-backed
view shortens the curses input wait only while an async page is pending.
Use `node tests/ui/scenarios/track-search.js --windows --tracks 30000 --runs 20`
to measure the current production path against those recorded numbers.

The viewport scenario widens the search to 100 results, advances past the first
viewport, returns to it, and jumps to the end. On Linux, the runner also
reads the application's peak RSS before exit. It excludes the fixture
indexer and PTY runner. On Linux it also records user plus system CPU time
across all application threads, both from first screen to last result and
from process start to last result. These samples exclude teardown and are
quantized to the kernel's process accounting tick. The standalone pilot
starts less application state than the production TUI. Its process RSS,
CPU time, and render latency include that difference.

The pilot defaults to three cached viewports, one read in flight, and
prefetching one viewport in the direction of travel. Use
`--cache-pages <count>` and `--prefetch-pages <count>` with `--compare`
to test another policy. A prefetch distance of zero fetches only on demand.
The distance must be smaller than the cache size. For direct pilot launches,
set `MUSIKCUBE_SEARCH_CACHE_PAGES` and `MUSIKCUBE_SEARCH_PREFETCH_PAGES`.
The benchmark verifies whether Page Down hits the prefetch and times it
after the adjacent page is ready when prefetching is enabled.

The first Linux x86-64 Release run used 20 launches per variant. Each pair
shared its seeded database. Times are keystroke to visible result, and the
filter includes the 300 ms debounce. Values are median / p95.

| Tracks | Variant | Open (ms) | Filter (ms) | Next window (ms) | Peak RSS (KiB) |
|---:|---|---:|---:|---:|---:|
| 10,000 | Legacy | 79.1 / 79.4 | 376.8 / 377.0 | 0.1 / 0.1 | 29,336 / 29,520 |
| 10,000 | ABI0 pilot | 6.8 / 7.1 | 306.6 / 306.9 | 0.6 / 0.7 | 9,190 / 9,336 |
| 30,000 | Legacy | 83.8 / 84.6 | 377.0 / 378.0 | 0.1 / 0.1 | 31,354 / 31,464 |
| 30,000 | ABI0 pilot | 15.6 / 16.3 | 315.4 / 316.4 | 0.6 / 0.8 | 10,564 / 10,648 |

With default prefetch enabled, a later 20-launch comparison at 10,000
tracks measured search-scenario CPU at 30 / 30 ms for legacy and
10 / 20 ms for the pilot (median / p95). The accounting tick was 10 ms.

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

## Runner and failure artifacts

Use `node tests/scripts/run-tests.js` as the normal entry point. `--suite` selects
a test level, `--list` lists checks, and arguments after `--` pass through to CTest.
Native ABI consumers remain separately compiled; CTest remains the outer runner
and C11 consumers stay C11. Framework selection is described above.

Keep broad native and UI workflows independently selectable with fresh state;
a combined UI flow supplements individual search, queue, playlist, and browse
checks. Large-catalog and repeated timing comparisons run explicitly. Fix focus
ordering rather than adding input sleeps when a rendered view is not ready.
The terminal driver tests parsing, cursor/erase/scroll, styling, resize, CJK width,
and combining marks; emoji/ZWJ presentation is not a universal width guarantee.

Failed scenarios retain isolated configuration/runtime, indexer logs, terminal
output, visible text, and SVG under the build root and report their location.
Successful runs clean up; harness failures must preserve diagnostic evidence.

## Execution and remaining work

The [LLD](musikcore-c-abi-lld-foundation.md#test-infrastructure) tracks completed harness
work and remaining native-platform acceptance. Do not maintain a second progress
checklist here. Extend unit, integration, and UI coverage alongside the behavior
being changed; there is no backfill requirement for untouched code and no rule
that each change needs all three levels.

Provider fixtures, sanitizer/CI matrices, installed artifacts, and package
compatibility remain future work in the core migration plan. A provider test
milestone does not precede the current core API/app integration goal.

## Open questions

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

## Correctness UI scenarios and driver checks

The full `musikcube_tests` target builds the app and required fixture plugins on
Unix. CTest registers separate search, queue, playlists, browse, settings,
and locale scenarios with small isolated catalogs. The capture fixture also
enables separate transport scenarios for gapless and crossfade.
Windows requires a native PTY backend before these app scenarios can be enabled;
registration on macOS does not establish Windows/Linux acceptance.

The native PCM capture fixture can hold a buffer completion at a known boundary.
The transport tests use this to force overlapping seeks, stop and output reload
with buffers outstanding, and replacement of a prepared next track. Other cases
stop a job during its last notification, interrupt a blocked retired fade,
destroy a paused crossfade, and cancel a source while its provider is opening it. These tests exercise
the production player and decoder with a synthetic output clock.

Run `cmake --build <build-dir> --target musikcube_playback_benchmark` for the
headless Linux/macOS playback baseline. Each sample runs in a fresh process
and plays two local WAV tracks gaplessly. CPU time covers playback and worker
cleanup. Peak RSS is the process high-water mark, including startup. The
capture sink counts rendered frames without retaining the PCM history.
Results are saved to `<build-dir>/playback-benchmark/results.json`. Use
`tests/scripts/benchmark-playback.js` with `--baseline` to compare medians from
the same machine and build configuration. Optional `--max-cpu-increase` and
`--max-rss-increase` percentages make a comparison fail when it exceeds a budget.
For small changes, pass `--compare-binary` and `--compare-plugin` for the
reference build. The runner alternates the order of the two builds and reports
the median and range of paired changes. Comparing a build with itself first
shows how much the metric moves under the same workload.

Run `node tests/scripts/benchmark-indexer.js <build-dir> --baseline <file>`
to compare incremental indexing against a saved baseline. It creates batches
of 5,000 WAV files with distinct comments, uses an isolated catalog and the
production indexer, and records each scan under `<build-dir>/indexer-benchmark`.
Each timing includes a three-second idle wait and rechecks the existing files.
Save a baseline locally and compare on the same machine, accounting for
filesystem cache and background load.
Use `--workload gme` for 1,000 synthetic NSF containers with 16 tracks each
per batch, or `--workload openmpt` for 2,500 two-subsong MOD containers per
batch. These workloads isolate the respective source and can be compared with
separately captured baselines.

```sh
node tests/ui/scenarios/playlists.js --build-dir build/test --bin-dir build/test/bin
node tests/ui/scenarios/standalone.js --build-dir build/test --bin-dir build/test/bin --tracks 1000 --windows
node --test tests/ui/driver/*.test.js
```

Each scenario launches a fresh app and catalog. `track-search.js` defaults to
search and paging measurements. `standalone.js` runs the combined acceptance
workflow. Queue, playlist, and transport scenarios use Search to select a fixture
track and skip the search benchmark. Focus transitions use
visible state with no arbitrary focus-delay sleeps; the app activates overlays
created by dispatch before drawing them.

Failed UI runs retain their runtime under the selected build root, including
`failure.log`, indexer logs, settings, `failure/screen.txt`, `failure/screen.svg`,
and the bounded `failure/terminal.log` transcript tail. The error names the
retained root. Successful runs remove their private runtime. Optional
`--screenshots` remains available for captures of successful runs.

`Terminal.resize()` changes the real PTY through `stty` and updates the screen
model. Driver tests verify the child observes the new dimensions. The screen
suite checks chunked UTF-8/CSI/OSC parsing, cursor/erase/scroll operations,
CJK/combining cells, styles/SVG escaping, and resizing. Emoji grapheme/ZWJ and
terminal-specific ambiguous widths are not treated as a layout oracle.
