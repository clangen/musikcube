# Core C API foundation progress

Status: in progress; gate remains open. Implementation and validation tooling
are recorded in separate commits; no changes have been pushed.

## Verified locally on 2026-11-19

Host: macOS 26.6.2, Apple M4/arm64, AppleClang 21.0.0.21000101.

- Production `musikcube_tests` and `musikcube` build successfully.
- Full CTest suite: 11/11 pass, including concurrent execution with `-j6`.
- Separate, initially clean ABI-only Debug and Release builds: 9/9 tests pass
  in each configuration. Header translation units compile independently.
- Public declaration inventory matches all 57 allowed exports. Mach-O uses
  `@rpath/libmusikcore-abi0.dylib` and only system C++/C runtime dependencies.
- C and strictly header-only C++ query examples execute against the indexed
  fixture through the shared library.
- Production TUI: 10,000 generated tracks, paging/window scenario, three runs.
- `git diff --check` passes.

Final Debug/Release rebuilds include the peer-cancellation and allocation-failure
tests. Linux and Windows validation are deferred. No Linux build/test was performed.

## Implemented changes and evidence

| Area | Change | Evidence |
| --- | --- | --- |
| Contract reconciliation | Declaration inventory and current support/lifetime contract document | `musikcore.abi.inventory`, `musikcore.abi.exports`, `musikcore_public_headers` |
| C ergonomics | Fixed-width library/search enums; common callback argument/error conventions, including playlists | C header assertions, query and production/remote integration tests |
| Ownership | Reject occupied context/result/queue outputs; positive IDs for constructed results; preserve existing owners | `musikcore.abi.shared_c`, production queue tests |
| Async lifecycle | Callback shutdown/destroy rejection, callback peer cancellation, allocation-failure cleanup | `musikcore.abi.dispatch` |
| Query conversion | One retained-owner conversion for local and remote ordered results | `musikcore.abi.remote_query_transport`, `musikcore.abi.indexed_query` |
| C++ wrapper | Preserve snapshot error diagnoses; reject views from temporary pages; explicit window move deletion | Production wrong-thread snapshot/queue checks, independent headers |
| Test isolation | Fresh preferences/databases per run; staged required plugins; shared production-runtime RAII | Concurrent CTest execution, indexed and loopback fixtures |
| Binary boundary | Reject transitive links to both core implementations | `musikcore.abi.link_boundary` positive/negative configure fixtures |
| Test entry points | All test scripts consolidated in `script/test/`; correct Debug selection; ABI-only mode | Debug/Release runs and moved TUI runner |
| Performance | Reproducible synthetic API baseline and allocation-free borrowed title access | `script/test/run-baseline.js`, `musikcore_api_baseline` |

The macOS TUI launcher now uses a small `forkpty` helper because Apple's
`script` command rejects Node's socket-backed stdin. It adds no external
dependency. The existing Linux launch path is unchanged.

## Performance observations

Release ABI0, 31 samples, 10,000 synthetic tracks, 128-byte titles:

| Operation | Median | p95 |
| --- | ---: | ---: |
| Context creation | 0.042 microseconds | 0.084 microseconds |
| Idle shutdown | 0.042 microseconds | 0.042 microseconds |
| Search producing 10,000 IDs | 2,121.667 microseconds | 2,180.292 microseconds |
| Read 1,000 titles | 464.167 microseconds | 507.584 microseconds |

Borrowed access to 1,000 titles references 128,000 payload bytes, with matching
page/view pointers, zero string bytes copied, and zero change in live malloc
blocks/bytes on macOS. This measures borrowed access only. It does not measure
total allocation churn or copies during query/result construction.

Production TUI, 10,000 indexed tracks, three runs:

| Operation | Median | p95 |
| --- | ---: | ---: |
| Open | 84.5 ms | 86.8 ms |
| Filter | 399.5 ms | 401.6 ms |
| Scroll | 2.1 ms | 2.4 ms |
| Next window | 1.7 ms | 2.6 ms |
| Back to first window | 0.8 ms | 1.5 ms |
| Jump to last window | 6.0 ms | 7.4 ms |

These are observations, not regression thresholds. Context timings approach
clock resolution. Hardware load and deliberate UI debounce influence timings;
repeat measurements before assigning tolerances.

Reproduce using:

```sh
node script/test/run-tests.js --build-dir build/foundation-debug --config Debug --abi-only
node script/test/run-tests.js --build-dir build/foundation-release --config Release --abi-only
node script/test/run-baseline.js --build-dir build/foundation-release
node script/test/tui/track-search.js --tracks 10000 --windows --runs 3
```

## Remaining work

- Finish declaration-by-declaration validation/nullability and exception-barrier
  review; the inventory and general contract are not a substitute for that audit.
- Reconcile every low-level checked item against its original named acceptance
  test. Corrected obsolete constraints and phase ordering so far.
- Complete exhaustive invalid-input, field/range, playback-state, and callback
  lifecycle coverage rather than infer it from the current suite passing.
- Validate queue output preservation directly through a public playback handle.
- Determine how stale experimental binaries are identified without changing the
  fixed experimental ABI version.
- Measure total allocations and copied bytes during metadata/result creation,
  large playlist loading, queue replacement, and remote operations. Establish
  representative comparisons and regression tolerances.
- Run Windows Debug/Release DLL/import-library validation separately.
- Run Linux Debug/Release ELF/rpath validation separately, as requested.

The checklist remains authoritative. Unchecked items are not waived by this
report, and no gate should be marked complete from the local results alone.
