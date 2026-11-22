# Repository guidance

## Project tenets

- Keep musikcube usable across supported platforms. Consider standalone packaging and runtime dependencies when changing build inputs.
- Add dependencies only when necessary. Link them statically when practical, ship them as relocatable libraries without additional runtime requirements, or load them through optional plugins (`dlopen`, `LoadLibrary`, or the platform equivalent). Missing optional plugins must leave the app usable.
- Keep blocking I/O, allocations, and locks off real-time audio callbacks.
- Treat application and plugin interfaces as contracts. Make compatibility changes deliberate and document their impact.
- Define new public interfaces in C. Use C++ internally and generally keep C++ types and symbols out of exported interfaces.
- Surface problems with existing SDK APIs to the maintainer immediately, with the proposed contract change and compatibility impact. Do not add workarounds, adapters, or legacy compatibility by default; let the maintainer audit and decide those tradeoffs. During explicitly autonomous work, resolve necessary SDK issues within the authorized scope, then report every such decision at completion and wait for maintainer acknowledgment before proceeding beyond that work.

## Formatting

- Always brace control-flow bodies, including single-statement branches and loops.
- Attach opening braces to single-line function, method, and constructor signatures; put them on a new line for multiline signatures. Keep constructor initializer lists between the signature and brace. Use a new-line brace for multiline class inheritance lists and control-flow conditions; otherwise attach it.
- Space braced initializers as `MatchType matchType { MatchType::Substring };` and C-style casts as `(int64_t) 0`.
- Put `break;` on its own line. For a braced case, place it after and aligned with the closing brace.
- Start multiline ternary continuations with `?` and `:`, indented four spaces beyond the statement. Use continuation indentation for call arguments and adjacent string literals rather than aligning under earlier arguments. In compound conditions, put long calls on their own continuation line before wrapping arguments.
- Order class access sections as `public`, `protected`, then `private`, omitting empty sections. Within each, order types, constructors, destructor, operators, member functions, then instance variables. Separate nonempty groups and access sections with blank lines; section-label comments are optional. Public instance variables should be very rare.
- Write nested namespaces on one line: `namespace musik { namespace core { namespace audio {`. Indent the body once and close with `} } }` on one line.
- Use the root `.clang-format` incrementally on selected C/C++ files and review the diff. Manually preserve conditional brace placement, declaration order, standalone `break;`, ternary indentation, and spaced namespace closings. Watch its extra indentation for structs without access labels.
- Write comment prose in lowercase with normal punctuation; preserve symbol names verbatim. Use `/* comment */` where supported, aligning wrapped text with the opening `/*` without extra indentation. Use `//` only when necessary, and native syntax where block comments are unavailable.

## Implementation

- Explicit repository rules take precedence over legacy style. Follow surrounding conventions for unspecified details, reuse helpers, and introduce abstractions when they simplify real callers.
- Expose process-service lifecycle through namespace functions, fully qualifying calls. Keep instance calls explicit through their owner. Use `Initialize`/`Shutdown` for lifecycle and `BindServices`/`UnbindServices` for separate attachment phases.
- Use PascalCase for C++ member functions, including private members; follow surrounding conventions for file-local helpers, commonly lower camelCase. Exported C ABI functions use snake_case.
- In longer functions, add terse technical comments at phase boundaries and branching handoffs. Explain non-obvious reasons. Separate distinct steps with blank lines while keeping related statements together.
- Name meaningful public keys, limits, sentinels, and positional indices; reuse their definitions. Keep local constants near use and obvious literals inline. Group class-scoped message IDs near the top of the class declaration in its header.
- Prefer `musik::debug` for production diagnostics. Reserve direct stderr output for test assertions or boundaries that cannot depend on that logger.
- Keep platform/compiler differences near setup and pass resulting options into shared code.
- Make ownership, cleanup, and error handling explicit at resource/thread boundaries. Prefer direct ownership and references when the owner outlives the caller; use smart pointers for shared lifetimes, ownership transfer, or weak references.
- Prefer event-driven notifications and wakeups. Use polling only when necessary or significantly advantageous; document why and limit its duration.
- Keep SQL wrappers such as `db::Connection` generic: no application/query-specific types, declarations, or friendships. Add generic operations or access mechanisms when needed.
- Route library operations through typed queries on the selected backend. Reuse queries or add them as needed. Keep library SQL in local typed-query implementations so operations work with local and remote backends; reserve ad-hoc SQL for cases that require it.
- Bind every variable SQL value, including each `IN` entry. Choose identifiers and syntax from fixed fragments or allowlists. Direct execution is only for fixed SQL without variable values.

## Testing, review, and decisions

- Verify changes through their normal entry point. Add tests alongside behavior changes for meaningful contracts or reproduced regressions that existing tests miss, rather than backfilling untouched code. Avoid coverage padding and tests that mirror implementation; use the smallest reliable setup with maintenance cost proportionate to risk.
- For a reproducible bug, write a regression test at the smallest reliable seam before fixing it. Confirm it fails for the expected reason on the unfixed code, then apply the fix and rerun it to confirm it passes. Use synchronization gates and bounded deadlines for concurrency regressions.
- For transient UI regressions, identify which region changes and compare the same interaction with a known-good build and another frontend when practical. Inspect intermediate frames and data requests during the interaction. Keep already loaded content visible while resizing or relaying out a view, retain cached rows across geometry-only changes, and fetch missing rows without clearing the overlap. Assert that existing content stays visible during the interaction as well as after it settles.
- Keep tests top-level under `tests/unit/`, `tests/integration/`, and `tests/ui/`. Group unit tests by owning project and native helpers under `tests/support/`. Each CTest entry gets exactly one level label (`unit`, `integration`, or `ui`), plus owner/subsystem labels; `contract` is an additional guarantee label.
- Keep test orchestration, fixture generation, build checks, and benchmarks in `tests/scripts/`; TUI scenarios in `tests/ui/scenarios/` and reusable drivers in `tests/ui/support/`. Keep test binaries and fixtures in shared, gitignored build-output directories. Follow [the testing design](doc/testing-design.md).
- After a major C/C++ refactor (for example, changes to ownership, threading, public contracts, or architecture), ask about an independent critical review at a natural checkpoint without blocking other authorized work. Run one whenever requested. Require concrete pros/cons, before-and-after examples, meaningful measurements where available, and remaining risks/verification gaps. Pair substantive criticisms with practical improvements and tradeoffs; a verdict alone is insufficient.
- Capture meaningful architectural decisions when completing backlog sections as individual numbered Markdown ADRs under `docs/adr/`, linked from the relevant design. Include status, context, alternatives, rationale, and architectural consequences. Keep each decision cohesive and avoid duplication. Put tasks, progress, transitional implementation routes, temporary workarounds, build/artifact wiring, and validation exceptions in the LLD. Update accepted ADRs when decisions change, not whenever implementation advances.

## Build scripts

- Write new Unix build scripts in Node.js, reusing `script/unix-build-config.js` for compiler/target settings.
- Keep compiler flags/setup in compiler profiles and use existing vendor include/library directory lists in CMake targets.
- Keep package-specific steps in `script/vendor-packages/`, with fixed configure/build arguments as named recipe constants.
- Keep staging, caches, logs, and temporary build files under the build root; reuse existing build-cache and staging directories.
- Keep routine vendor output quiet. Stream it with `--verbose` and retain failure diagnostics.
- Keep Linux x86-64 standalone builds on Zig with glibc 2.28; macOS uses the default compiler. Audit packages with `script/scan-standalone.js`.

## musikcore C ABI

- Follow [the C ABI design](doc/musikcore-c-abi-design.md), [the HLD](doc/musikcore-c-abi-hld.md), and [the LLD/task checklist and foundation gate](doc/musikcore-c-abi-lld.md). ADRs linked from the HLD record architectural decisions.
- Complete the foundation gate before adding feature families, migrating more app functionality, or beginning provider work. Core API/app work may follow a scoped maintainer-approved exception recorded in the LLD; deferred checks stay open. Provider work follows the core-first sequence in the HLD/LLD.
- Keep the application API and plugin SDK separate, with explicit boundary ownership and lifetime.
- Group public C declarations into independently compilable subsystem headers, included by `musikcore.h`.
- Keep `musikcore.hpp` strictly header-only, depending solely on the public C ABI without private core headers. For owning wrapper types, explicitly declare copy/move operations and document their semantics at the class declaration. Make read-only accessors const and state-changing operations non-const.
- Keep the experimental ABI version fixed until freeze; signatures/layouts may change freely during development.
- Document every public struct field, including units, ownership, and defaults where applicable. Give every struct with `struct_size` an `MC_*_DEFAULT` initializer.
- When completing an ABI task, update its checklist and named test in the same change. Complete gates only when named-platform acceptance passes or the task plan records an exception.
