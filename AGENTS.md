# Repository guidance

## Project tenets

- **Fail fast on fatal errors.** When an ownership violation, broken invariant, or failed cleanup means execution cannot safely continue, emit a clear diagnostic and terminate before further mutation or resource release. Enforce this in Debug and Release; a debug-only assertion is insufficient. Do not swallow the failure, convert it to an ordinary status, or pretend cleanup succeeded. Expected user, device, and I/O failures remain recoverable errors. Catch exceptions at ABI boundaries only when containment or rollback leaves a valid state.

- **The most important rule of API design is to make APIs easy to use correctly and hard to use incorrectly.** Aim for a pit of success: the natural call should also be the correct and safe one. Apply this to application APIs, internal interfaces, and plugin SDKs. Use types, named operations, and validation to prevent mistakes with identifiers, units, options, ownership, and lifetimes. Keep necessary flexibility, such as custom metadata keys, behind explicit opt-ins. Review how callers could misuse each new or revised API and test meaningful safeguards. [Scott Meyers on interface design](https://www.oreilly.com/library/view/97-things-every/9780596809515/ch55.html) · [Rico Mariani on the pit of success](https://ricomariani.medium.com/the-pit-of-success-cfefc6cb64c8)
- Keep public API surfaces as small as practical: expose only operations and types needed by real callers. Keep implementation helpers, intermediate results, and bookkeeping private. Prefer operations that complete a caller's task over exposing internal steps that callers must sequence correctly. Apply this to C++ classes as well as exported APIs and SDKs; a smaller surface helps create a pit of success.
- Keep musikcube usable across supported platforms. Consider standalone packaging and runtime dependencies when changing build inputs.
- Add dependencies only when necessary. Link them statically when practical, ship them as relocatable libraries without additional runtime requirements, or load them through optional plugins (`dlopen`, `LoadLibrary`, or the platform equivalent). Missing optional plugins must leave the app usable.
- Keep blocking I/O, allocations, and locks off real-time audio callbacks.
- Treat application and plugin interfaces as contracts. Make compatibility changes deliberate and document their impact.
- Define new public interfaces in C. Use C++ internally and generally keep C++ types and symbols out of exported interfaces.
- C++ plugins in this repository may use the public header-only C++ RAII and convenience wrappers when they simplify ownership or calls; plugin ABI boundaries remain C.
- Surface problems with existing SDK APIs to the maintainer immediately, with the proposed contract change and compatibility impact. Do not add workarounds, adapters, or legacy compatibility by default; let the maintainer audit and decide those tradeoffs. During explicitly autonomous work, resolve necessary SDK issues within the authorized scope, then report every such decision at completion and wait for maintainer acknowledgment before proceeding beyond that work.

## Documentation

- Keep repository documentation under `doc/`: design documents and implementation
  plans in `doc/design/`, architectural decision records in `doc/adr/`, and build
  guides, migration history, and other documentation in the `doc/` root. Update
  links and references when moving documents.

## Formatting

- Always brace control-flow bodies, including single-statement branches and loops.
- Attach opening braces to single-line function, method, and constructor signatures; put them on a new line for multiline signatures. Keep constructor initializer lists between the signature and brace. Use a new-line brace for multiline class inheritance lists and control-flow conditions; otherwise attach it.
- Space braced initializers as `MatchType matchType { MatchType::Substring };` and C-style casts as `(int64_t) 0`.
- Use designated initializers (`.field = value`) for struct aggregate initialization and assignment where the language standard supports them. In C++, keep fields in declaration order; retain positional initialization in public C++17 wrappers. Put one field per line and the closing brace on its own line, including nested structs, with a trailing comma after the final field so clang-format preserves the layout. This does not require expanding scalar braced initializers.
- Put `break;` on its own line. For a braced case, place it after and aligned with the closing brace.
- Start multiline ternary continuations with `?` and `:`, indented four spaces beyond the statement. Use continuation indentation for call arguments and adjacent string literals rather than aligning under earlier arguments. In compound conditions, put long calls on their own continuation line before wrapping arguments.
- Order class access sections as `public`, `protected`, then `private`, omitting empty sections. Within each, order types, constructors, destructor, operators, member functions, then instance variables. Separate nonempty groups and access sections with blank lines; section-label comments are optional. Public instance variables should be very rare.
- Write nested namespaces on one line: `namespace musik { namespace core { namespace audio {`. Indent the body once and close with `} } }` on one line.
- Indent class and struct members one four-space level; align `public`, `protected`, and `private` labels with the class braces.
- Use the root `.clang-format` incrementally on selected C/C++ files and review the diff. Manually preserve conditional brace placement, declaration order, standalone `break;`, ternary indentation, and spaced namespace closings.
- Write comment prose in lowercase with normal punctuation; preserve symbol names verbatim. Refer to functions and methods by their exact names with a `()` suffix, wrapped in backticks (for example, `Stop()`). Use `/* comment */` where supported, aligning wrapped text with the opening `/*` without extra indentation. Use `//` only when necessary, and native syntax where block comments are unavailable.
- Keep APIs and SDKs readable by humans, including generated bindings and plugin declarations. Wrap long signatures at argument boundaries, leave blank lines between function declarations, and separate related groups with whitespace. Avoid dense tables of long single-line declarations; apply the same care to code examples in design documents.
- Keep C++ plugin registration consistent across plugin types: use similarly named registration helpers and calling conventions, and keep adapter ownership and cleanup inside the SDK whenever practical.

## Implementation

- Treat new nested bookkeeping types, especially those collected in containers and guarded by locks, as a warning sign during refactors. Trace the underlying need and prefer a simpler design that addresses it at its source; add the machinery only when its benefits justify the complexity.
- Explicit repository rules take precedence over legacy style. Follow surrounding conventions for unspecified details, reuse helpers, and introduce abstractions when they simplify real callers.
- Expose process-service lifecycle through namespace functions, fully qualifying calls. Keep instance calls explicit through their owner. Use `Initialize`/`Shutdown` for lifecycle and `BindServices`/`UnbindServices` for separate attachment phases.
- Use PascalCase for C++ member functions, including private members; follow surrounding conventions for file-local helpers, commonly lower camelCase. Exported C ABI functions use snake_case.
- Write terse, plain-English comments that explain the concrete situation and why the code handles it that way. Describe what could happen and the consequence being avoided, rather than narrating statements or using abstract lifecycle terminology. For example: "a factory can start workers before reporting failure. stop them before freeing the object they might still be using." In longer functions, place these comments at meaningful phase boundaries and branching handoffs. Separate distinct steps with blank lines while keeping related statements together.
- Name meaningful public keys, limits, sentinels, and positional indices; reuse their definitions. Keep local constants near use and obvious literals inline. Group class-scoped message IDs near the top of the class declaration in its header.
- Prefer `musik::debug` for production diagnostics. Reserve direct stderr output for test assertions or boundaries that cannot depend on that logger.
- Keep platform/compiler differences near setup and pass resulting options into shared code.
- Validate inputs at the boundary that owns them, then let internal code and thin adapters rely on documented preconditions. Keep lifecycle sequencing with the caller that owns the lifecycle. Tests must prove the boundary rejects invalid inputs before downstream code runs and establishes required lifecycle order; merely documenting preconditions or testing valid inputs is insufficient. Do not duplicate validation across layers. If an enforcement boundary is not implemented yet, record its tests as an open gate rather than claiming coverage from a mock caller.
- Make ownership, cleanup, and error handling explicit at resource/thread boundaries. Prefer direct ownership and references when the owner outlives the caller; use smart pointers for shared lifetimes, ownership transfer, or weak references. When a class defines an alias for its own `std::shared_ptr`, name it `Shared` so callers can see the shared ownership in `Type::Shared`.
- Prefer event-driven notifications and wakeups. Use polling only when necessary or significantly advantageous; document why and limit its duration.
- Keep SQL wrappers such as `db::Connection` generic: no application/query-specific types, declarations, or friendships. Add generic operations or access mechanisms when needed.
- Route library operations through typed queries on the selected backend. Reuse queries or add them as needed. Keep library SQL in local typed-query implementations so operations work with local and remote backends; reserve ad-hoc SQL for cases that require it.
- Bind every variable SQL value, including each `IN` entry. Choose identifiers and syntax from fixed fragments or allowlists. Direct execution is only for fixed SQL without variable values.

## Testing, review, and decisions

- Verify changes through their normal entry point. Add tests alongside behavior changes for meaningful contracts or reproduced regressions that existing tests miss, rather than backfilling untouched code. Avoid coverage padding and tests that mirror implementation; use the smallest reliable setup with maintenance cost proportionate to risk.
- For a reproducible bug, write a regression test at the smallest reliable seam before fixing it. Confirm it fails for the expected reason on the unfixed code, then apply the fix and rerun it to confirm it passes. Use synchronization gates and bounded deadlines for concurrency regressions.
- For transient UI regressions, identify which region changes and compare the same interaction with a known-good build and another frontend when practical. Inspect intermediate frames and data requests during the interaction. Keep already loaded content visible while resizing or relaying out a view, retain cached rows across geometry-only changes, and fetch missing rows without clearing the overlap. Assert that existing content stays visible during the interaction as well as after it settles.
- Keep tests top-level under `tests/unit/`, `tests/integration/`, and `tests/ui/`. Group unit tests by owning project and native helpers under `tests/support/`. Each CTest entry gets exactly one level label (`unit`, `integration`, or `ui`), plus owner/subsystem labels; `contract` is an additional guarantee label.
- Keep test orchestration, fixture generation, build checks, and benchmarks in `tests/scripts/`; TUI scenarios in `tests/ui/scenarios/` and reusable drivers in `tests/ui/support/`. Keep test binaries and fixtures in shared, gitignored build-output directories. Follow [the testing design](doc/design/testing-design.md).
- Run independent objective reviews only when the maintainer requests one or agrees after it is suggested. For a major C/C++ refactor, suggest one at a natural checkpoint without blocking other authorized work. Require concrete pros/cons, before-and-after examples, meaningful measurements where available, and remaining risks/verification gaps. Challenge incidental or accidental complexity explicitly, especially new nested bookkeeping objects collected in containers, their checks, and associated locks: trace it to its root cause, assess the system as a whole, and propose simpler designs and their tradeoffs instead of accumulating local fixes. Pair substantive criticisms with practical improvements and tradeoffs; a verdict alone is insufficient.
- When an unusual failure would require substantial recovery machinery, bring the user a simpler safe policy and its observable tradeoff before implementing it. Record agreed architectural tradeoffs in the relevant ADR and test the promised behavior. Review against that decision and reopen it when new evidence changes the tradeoff.
- Capture meaningful architectural decisions when completing backlog sections as individual numbered Markdown ADRs under `doc/adr/`, linked from the relevant design. Include status, context, alternatives, rationale, and architectural consequences. Keep each decision cohesive and avoid duplication. Put tasks, progress, transitional implementation routes, temporary workarounds, build/artifact wiring, and validation exceptions in the LLD. Update accepted ADRs when decisions change, not whenever implementation advances.

## Build scripts

- Write new Unix build scripts in Node.js, reusing `script/unix-build-config.js` for compiler/target settings.
- Keep compiler flags/setup in compiler profiles and use existing vendor include/library directory lists in CMake targets.
- Keep package-specific steps in `script/vendor-packages/`, with fixed configure/build arguments as named recipe constants.
- Keep staging, caches, logs, and temporary build files under the build root; reuse existing build-cache and staging directories.
- Keep routine vendor output quiet. Stream it with `--verbose` and retain failure diagnostics.
- Keep Linux x86-64 standalone builds on Zig with glibc 2.28; macOS uses the default compiler. Audit packages with `script/scan-standalone.js`.

## musikcore C ABI

- Follow [the C ABI design](doc/design/musikcore-c-abi-design.md), [the HLD](doc/design/musikcore-c-abi-hld.md), and [the LLD/task checklist and foundation gate](doc/design/musikcore-c-abi-lld.md). ADRs linked from the HLD record architectural decisions.
- Complete the foundation gate before adding feature families, migrating more app functionality, or beginning provider work. Core API/app work may follow a scoped maintainer-approved exception recorded in the LLD; deferred checks stay open. Provider work follows the core-first sequence in the HLD/LLD.
- Keep the application API and plugin SDK separate, with explicit boundary ownership and lifetime.
- Group public C declarations into independently compilable subsystem headers, included by `musikcore.h`.
- Keep `musikcore.hpp` strictly header-only, depending solely on the public C ABI without private core headers. For owning wrapper types, explicitly declare copy/move operations and document their semantics at the class declaration. Make read-only accessors const and state-changing operations non-const.
- Keep the experimental ABI version fixed until freeze; signatures/layouts may change freely during development.
- Document every public struct field, including units, ownership, and defaults where applicable. Give every struct with `struct_size` an `MC_*_DEFAULT` initializer.
- Give every public C struct a terse type-level comment explaining what it represents, why it exists, and when callers use it. Establish that purpose before listing restrictions; put detailed units, ownership, threading, and lifetime rules beside the relevant fields or operations.
- When completing an ABI task, update its checklist and named test in the same change. Complete gates only when named-platform acceptance passes or the task plan records an exception.
