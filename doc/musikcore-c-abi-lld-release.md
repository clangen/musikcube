# C ABI LLD: release and cleanup gates

[LLD index](musikcore-c-abi-lld.md) · [SDK removal design](musikcore-sdk-removal-design.md)

## Release tasks

### Phase 9: system, packaging, and compatibility

- [x] **SYS-001** Add musikcube pseudo-terminal startup/shutdown smoke test.
  `node tests/ui/scenarios/track-search.js` starts the app in a PTY and quits through
  its normal key binding.
- [x] **SYS-002** Test valid, unset, and unparseable `TERM` behavior in a PTY.
  `App::InitCurses()` falls back to `xterm-256color` when `setupterm()` fails.
  `musikcube.ui.term` runs the production TUI with all three cases and checks
  clean shutdown. It passes on Linux and fails if the fallback is removed.
  The maintainer accepts macOS behavior without a new native test run.
- [x] **SYS-003** Add musikcubed startup/readiness/shutdown smoke test.
  `musikcubed.integration.lifecycle` covers concurrent starts, foreground
  scan completion, background readiness and startup failure, and shutdown.
  Native macOS Debug/Release runs pass.
- [x] **PKG-001** Install headers, shared library, platform import library,
  and package metadata into a temporary prefix. Unix and Windows supply the
  relocatable `musikcore::musikcore` CMake target. On Windows,
  `musikcore.integration.api.installed_sdk` moves the prefix and independently
  builds and runs the installed C and C++ consumers, including managed
  runtime and playback against the installed DLL and plugins.
- [x] **PKG-002** Build external C and C++ consumers against the installed prefix
  on native Linux, macOS, and Windows. `musikcore.integration.api.installed_sdk`
  relocates the prefix,
  builds both query examples independently, and runs them against indexed media.
  A public-only C consumer switches from a remote catalog with a withheld reply
  to the local fixture. It checks retained track origin, dispatcher thread
  ownership, self-unsubscription, rejected callback shutdown and destruction,
  and cancellation during owner-thread shutdown.
External C decoder build/load checks (PKG-003/PKG-004) are tracked with the
[application media API work](musikcore-c-abi-lld-plugins.md#application-media-apis).
- [x] **PKG-005** Validate ELF SONAME/RPATH and symbol versions. Linux Release
  installed with the expected `libmusikcore.so` SONAME and relative runtime
  paths. The complete runtime passed `script/scan-standalone.js` at glibc 2.28.
- [x] **PKG-006** Audit packaged Mach-O install names, RPATHs, and dependencies.
  Archive staging and SDK installation share `FixupMachORpaths.cmake`, which
  removes absolute build paths, supplies relative search roots, and re-signs
  changed binaries. `musikcore.integration.binary.macos_package` rejects stale
  absolute paths, escaping relative paths, and absolute install names; it checks
  repeated fixup and signature validity. `musikcore.integration.api.installed_sdk`
  rejects absolute RPATHs in copied libraries. The regression failed on the
  original FFmpeg dependency before the fix and passes afterward. The extracted
  app passes `script/scan-standalone.js`; all 52 app/SDK Mach-O signatures and
  install-name/RPATH checks pass. Export allowlists pass in the full suite.
  Debug installed consumers use the SDK's sanitizer selection and explicitly
  link the compiler runtime; they cannot rely on an absolute toolchain RPATH
  inside the relocated library. The installed-SDK regression also passes with
  native macOS ASan/UBSan after this test-build correction.
- [x] **PKG-007** Validate Windows DLL/import-library/plugin placement.
  Visual Studio 2026 x64 Debug and Release stage `musikcore.dll` beside both
  frontends, the import library under the configuration's build library directory,
  and plugins in the runtime's `plugins` directory. The
  `musikcube.integration.windows_core_dll` test verifies frontend imports and
  the C ABI export allowlist. Both staged configurations passed the Windows
  dependency audit.
- [x] **PKG-008** Run consumer, SDK 23 plugin, and application smoke tests from
  staged standalone archives. Native macOS extracted app startup/search/queue/
  shutdown passes three times; daemon lifecycle, competing starts, rescan, and
  background failure checks pass. External C/C++ queries and URI players plus
  Rust tests and five doctests pass against the extracted SDK with its original
  install prefix unavailable. URI players use shipped FFmpeg/null-output plugins;
  physical audio-device behavior is outside this check. Fresh Linux/Windows
  archive runs are maintainer-deferred; pure-C provider smoke remains separately
  deferred with PKG-003/PKG-004 and does not block core consumer acceptance.
- [x] **PKG-009** Verify consumer packages have no build-machine loader paths or
  unexpected dependencies. The native macOS app/SDK load-command and package
  metadata checks pass after RPATH cleanup. The package scanner rejects absolute
  and out-of-package RPATHs even when a working fallback exists. The existing
  explicit host-provided Homebrew PortAudio exception remains limited to the
  optional output plugin. This checks runtime/package paths, not arbitrary
  diagnostic source strings embedded in vendor binaries. New Linux/Windows
  checks remain maintainer-deferred; GATE-900 stays open for combined acceptance.
- [ ] **CI-001** Add the fast contract/unit/integration matrix for Windows,
  macOS, GCC, and Clang.
- [ ] **CI-002** Add ASan/UBSan jobs.
- [ ] **CI-003** Add a separate TSan job.
- [ ] **CI-004** Add scheduled stress, fault-injection, and package jobs.

#### Phase 9 gate

- [ ] **GATE-900** Installed and archived artifacts pass external consumer,
  external plugin, application smoke, dependency, and runtime-path tests on all
  supported platforms.

### Phase 10: migration cleanup and ABI v1 freeze

- [ ] **FIN-001** Complete the final frontend private-dependency audit. Unix
  musikcube/musikcubed pass their `integration.core_boundary` import checks;
  `musikcube.integration.windows_core_dll` checks production Windows PE exports
  and console/GUI imports. Preserve this evidence through final cleanup.
  `musikcore.integration.binary.exports.*` checks the shipped Unix core,
  executable, and plugin export lists, including symbols pulled in by static
  libraries. Linux frontend boundary and export checks passed during FIN-004.
  Fresh macOS Release audit on 2026-11-25 (code commit `84938c075`): both frontend
  boundary checks, the retired-SDK source check, all 16 shipped core/frontend/
  plugin export checks, the Mach-O package regression, and installed SDK/provider
  checks passed (22 CTests). A full install was archived and extracted at a new
  prefix. Both archived frontends import `mc_context_create` and only public
  `mc_*` core operations; the archived core's 186 exports match the allowlist.
  Source inspection found only public musikcore headers in the two frontends.
  No implementation fixes were needed. Repeat final acceptance on native Linux
  and Windows before closing this task.
- [ ] **FIN-002** Confirm every shipped plugin has left the C++ SDK ABI.
  Source discovery now uses only C entry points, and old plugin interfaces and
  hooks are removed. The retired `src/musikcore/sdk` directory is absent; the
  earlier SDK-directory utility note is obsolete. Linux export and
  installed-provider checks passed during FIN-004.
  Fresh macOS Release audit on 2026-11-25 (code commit `84938c075`): all 13
  rebuilt plugins in the relocated install archive export only
  `mc_plugin_get_api` and import no private musikcore C++ symbols or unlisted
  `mc_*` operations. The independently rebuilt installed providers and C/C++
  consumers passed, including application media shutdown/retained-handle tests.
  The extracted runtime passed `script/scan-standalone.js`, and all 29 shipped
  Mach-O signatures verified. The existing optional Homebrew PortAudio
  dependency allowance remains unchanged.
  Local evidence is retained under `tests/bin/final-artifact-audit/` (archive,
  extracted install and symbol/signature report), with CTest and scan logs in
  `tests/bin/final-artifact-*.log`. No new Linux/Windows artifact acceptance is
  claimed; their final build/export/package checks remain open.
- [x] **FIN-003** Remove or archive `musikcore_c.h`, `c_context.cpp`, and
  `c_interface_wrappers.cpp` after their consumers are gone. The fixture
  seeder uses the public indexer and is exercised by `musikcore.abi.indexed_query`.
- [x] **FIN-004** Remove obsolete C++ SDK interfaces from installed/public
  include paths. Full installs no longer copy `src/musikcore/sdk/*.h`; in-tree
  users can continue to use those private helpers until SDK-REMOVE-002 moves
  them. `musikcore.integration.api.installed_sdk` checks the full install for
  the retired header directory and builds consumers against the supported SDK.
  Linux passed; native-platform package checks remain under GATE-900.
- [x] **FIN-005** Replace `core_c_demo` with maintained C and C++ examples.
  `musikcore.abi.indexed_query` runs both query examples against an indexed fixture.
- [x] **FIN-006** Retired: no `musikcore_legacy` target was introduced.
  FIN-003 removed the old C boundary; current private test archives are not
  legacy public libraries and remain isolated by `link_boundary`.
- [x] **FIN-007** Retired as a release-cleanup requirement by maintainer
  decision. Production uses `musikcore`; the reduced `musikcore_abi0` target
  remains available to ABI contract tests and the test-only TUI pilot. This
  entry does not claim the target or its fixtures have been removed.
- [ ] **FIN-008** Assign ABI major version 1 and platform SONAME/install-name/DLL
  versioning. Complete the [library query scope adendum](musikcore-c-abi-lld-library-scope.md)
  and its SCOPE-014 acceptance gate before application ABI v1 freeze. The scope
  feature gate is complete under the maintainer-approved 2026-11-25 macOS
  acceptance exception. Before freeze, rerun `musikcore.integration.api.library_scope`,
  `musikcore.integration.api.backend_parity`, `musikcore.integration.api.managed_remote`,
  `musikcore.integration.api.remote_audio`, `musikcore.integration.api.installed_sdk`,
  `musikcore.integration.library.playlist_source_schema`, and
  `musikcore.integration.runtime.indexer_provider_cancel` on native Linux and
  Windows, together with public-header, exported-symbol and installed C/C++
  consumer checks. No new Linux/Windows scope acceptance is claimed by that exception.
  Native Arch Linux x86-64, code commit `e0e63ae38` plus the corrected
  `remote_audio` test expectation, passed all seven named scope tests on
  2026-11-26, along with the public-header, ABI export and installed SDK tests.
  The initial `remote_audio` run exposed a stale `Files` label expectation;
  it passed after updating the expectation to `Music files`. This build used
  the system compiler with no explicit CMake build type; Windows and final
  release artifact acceptance remain open.
- [ ] **FIN-009** Freeze and publish the export allowlist and public layout
  baseline after the library query scope contract and acceptance are complete.
  Include the renamed plugin service tables and indexer/playback service layout
  changes from [ADR-0018](../docs/adr/0018-library-layers-and-plugin-services.md).
  LIBLAY-PLATFORMS was closed by maintainer acceptance on 2026-11-25 without
  additional native Linux/Windows runs. Broader provider and release-platform
  gates retain their own requirements; no new platform results are claimed.
- [ ] **FIN-010** Preserve a v1 C consumer binary/source fixture.
- [ ] **FIN-011** Preserve a v1 C plugin binary/source fixture.
- [ ] **FIN-012** Test older v1 fixtures against every newer compatible v1
  build.
- [ ] **FIN-013** Publish API, SDK, ownership, threading, and example
  documentation with release artifacts.
- [x] **FIN-014** Audit `MC_CALL` in internal musikcore C++ code and remove it
  from ordinary C++ methods. Keep it on exported C functions and on functions
  or static thunks assigned to C ABI callback tables. The source audit found no
  non-static member declarations marked with `MC_CALL`; remaining member
  definitions are static callback thunks. The maintainer accepts platform
  validity without a new native Windows build; no such build is claimed here.

#### Legacy binding cleanup inventory

Cube no longer has library or playback attachment bindings. Playback tests
inspect public queue snapshots. The native fault-injection fixture opens
catalogs through a probe compiled only in test builds. Standalone builds omit
the probe.

#### Final gate

- [ ] **GATE-1000** ABI v1 is released only after all prior gates pass and the
  previous-boundary implementations are absent from shipped artifacts.

## Historical next steps

The following list records the earlier migration checkpoint and is not the current work queue. Use the [LLD index](musikcore-c-abi-lld.md) and task checkboxes instead.

Core consumer acceptance and packaging are complete in the recorded scope on
`clangen/musikcore-c-abi-completion`. The active branch rebuilds the plugin
architecture using the six-step order in Phase 4. Replacing plugin dependencies
must preserve the public application C ABI and its ownership/threading contracts;
the former output-first test deferrals were restored under PLG-EMPTY-002.

1. Keep shared subscription scope and URI lifecycle regressions green. The
   subscription fix centralizes delivery depth in the exported C implementation;
   it does not move typed callback storage into C or make callback-local
   disconnect wait for all admitted calls. `musikcore.unit.subscriptions` checks
   cross-library C/C++ disconnect and ordinary external-thread draining.
2. Preserve TST-008/API-013 instrumentation and fault-detection probes when
   adding targets. Debug now covers direct ABI and production paths; Release
   stays uninstrumented. Linux/Windows execution remains separately owned.
3. Preserve CORE-RC-001/CORE-RC-002 acceptance with the independently maintained
   Rust consumer and installed C/C++ examples. Reuse their existing tests rather
   than adding another foreign-runtime harness.
4. Preserve macOS archive acceptance under PKG-006/PKG-008/PKG-009, including
   shared staging/install fixup, strict loader-path scanning, extracted app/daemon
   runs, and relocated external C/C++/Rust consumers. The native Release suite
   passes 110/110 after packaging changes; the focused UI/SDK/package rerun also
   passes after separating cold-start and interaction deadlines.
5. Preserve recorded native Windows DLL, installed-SDK, playback, and ConPTY
   checks and the existing Linux results. Validate subsequent changes in their
   native platform sessions; the macOS sanitizer change does not establish new
   Windows/Linux coverage. Finish the remaining release gates without reopening
   foundation checks that already have native evidence.
6. Finish PLG-CONTRACT-001/002/003: the high-level family shapes are approved;
   audio/output headers, opaque buffers, the session-owned registry, and a
   boundary-tested loadable `nullout` plugin are implemented. Materialize the remaining family
   headers and reconcile engine callers, then disconnect the old production path
   and select the null factory through the replacement engine. Keep the standalone
   and runtime plugin regressions green. Empty-engine acceptance and the first
   stream/decoder playback proof remain open; GATE-400 is historical bootstrap
   evidence, not completion of this replacement plan.

Deferred expansion includes polling/owned unsolicited events (API-014/API-015),
general metadata invalidation and atomic batch edits (LIB-008/LIB-009), generic
numeric metadata access and capability discovery, and advanced consumer
stream/codec APIs (PUB-008). Packed page storage (LIB-002) is measurement-driven.
These items are not prerequisites for existing application parity. Source-list
consolidation (BLD-001) and CI jobs remain explicit build/test backlog; neither
is silently completed by a passing local suite. TST-008/API-013 are complete
in the stated native macOS scope, with other platform execution still deferred.
Source-list cleanup and a packed arena need not precede external-consumer
ergonomics work.

Historical phase numbers and completed implementation boxes do not override
this dependency order or imply an unchecked cross-platform gate has passed.

## Progress reporting

When completing a task:

1. Check its box in this document in the same commit as the implementation.
2. Add or update the named contract/unit/integration test.
3. Record meaningful architectural decisions as individual ADRs in `docs/adr/`,
   linked from the HLD, with status,
   context, rationale, and consequences; link the affected task. Do not add a
   progress diary or duplicate an existing decision record.
4. Do not check a gate until its acceptance statement passes on every named
   platform or the document explicitly records a temporary exception.

Implementation commits should reference task IDs in their messages where
practical, for example:

```text
ABI-005: add UTF-8 views and buffer-copy semantics
SDK-009: add pure-C fixture plugin
```

This keeps the checklist useful as an execution plan rather than a retrospective
status summary.
