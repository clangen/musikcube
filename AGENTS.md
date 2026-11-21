# Repository guidance

## Project tenets

- Keep musikcube usable across its supported platforms. Consider standalone packaging and runtime dependencies when changing build inputs.
- Add new dependencies only when necessary. Each new dependency must meet one of these conditions:
  - Link it statically whenever practical.
  - Ship it as a relocatable library that runs with the app without requiring additional dependencies.
  - Load it through an optional plugin using `dlopen`, `LoadLibrary`, or the platform equivalent. If loading fails, skip the plugin and keep the app usable.
- Keep playback responsive. Keep blocking I/O, allocations, and locks off real-time audio callbacks.
- Treat application and plugin interfaces as contracts. Make compatibility changes deliberate and document their impact.
- Define new public interfaces in C. Use C++ for internal implementation and generally keep C++ types and symbols out of the exported interface.

## Coding guidelines

- Always use braces for control-flow bodies, including single-statement `if`, `else`, `for`, and `while` bodies. Do not put an unbraced statement after a condition or loop.
- For function, method, and constructor definitions, attach the opening brace when the signature fits on one line; put it on its own line when the signature spans multiple lines. Keep constructor initializer lists between the signature and opening brace. Review this distinction manually because clang-format does not support conditional function-brace placement.
- For class definitions with a multiline inheritance list, put the opening brace on its own line. Do the same for control-flow statements whose condition spans multiple lines; keep the brace attached for single-line conditions. Review class braces manually because clang-format does not support conditional class-brace placement.
- Put spaces before and inside braced initializers, such as `MatchType matchType { MatchType::Substring };`.
- Put a space after C-style casts, such as `(int64_t) 0`.
- Put `break;` statements on their own lines. When a case has a braced block, put its `break;` after the closing brace at the same indentation as that brace. Preserve this manually; clang-format joins them onto one line.
- For multiline ternary expressions, put `?` and `:` at the start of their lines and indent them one continuation level (four spaces) beyond the statement, rather than aligning them under the condition. Review nested function-call arguments manually because clang-format adds another indentation level there.
- Use continuation indentation for multiline call arguments and adjacent string literals, rather than aligning them beneath an earlier argument. In a compound condition, move a long call to its own continuation line before wrapping its arguments.
- Organize class declarations into `public`, `protected`, then `private` sections, omitting empty sections. Within each access section, order declarations as types, constructors, destructor, operators, member functions, then instance variables. Public instance variables should be very rare. Separate each nonempty group and each access section with a blank line; section-label comments are optional. Clang-format does not enforce declaration order, so preserve this structure when editing classes.
- Use the root `.clang-format` for incremental C/C++ formatting of selected files. Review the diff, including inserted braces. Preserve the spaced nested-namespace closings required below; clang-format currently compacts those closings. Its indented-access-label setting also adds an extra level to structs without explicit access labels.
- Reuse existing helpers and follow the surrounding code's conventions. Introduce an abstraction when it simplifies real callers.
- Write nested C++ namespace declarations on one line, such as `namespace musik { namespace core { namespace audio {`. Indent the body once and close with `} } }` on one line.
- Expose process-service lifecycle through namespace-level functions and fully qualify lifecycle calls at their call sites. Keep calls on owned instances explicit through their owning object.
- Use `Initialize` and `Shutdown` for service lifecycle methods. Name separate service attachment phases `BindServices` and `UnbindServices`.
- Use PascalCase for C++ member functions, including private members. Follow the surrounding code for file-local helpers, which commonly use lower camelCase. Keep exported C ABI functions in snake_case.
- Let the code explain what it does. As you write or revise longer functions, add terse, technical comments at phase boundaries and branching handoffs to make the flow easy to follow. Explain decisions where the reason is otherwise unclear.
- Use blank lines to separate distinct steps in a function. Keep closely related statements together and give transitions between phases room to stand out.
- Name constants for public keys, limits, sentinel values, and positional indices whose meaning matters. Reuse the same definition across callers. Keep local constants near their use, and leave obvious literals inline.
- Group related message IDs in one place. Declare class-scoped message IDs together near the top of the class definition in its header.
- Write comment prose in lowercase with normal punctuation. Preserve symbol names verbatim. Align wrapped comment text with the opening `/*`, without extra continuation indentation.
- Prefer `musik::debug` for production diagnostics when available. Keep direct stderr output for test assertions or boundaries that cannot depend on that logger.
- Use `/* comment */` where the language supports block comments. Use `//` only when necessary and the language's native syntax where block comments are unavailable.
- Keep platform and compiler differences near setup. Pass the resulting options into shared build and application code.
- Make ownership, cleanup, and error handling explicit at resource and thread boundaries.
- Prefer event-driven notifications and wakeups. Use polling only when necessary or when it provides a significant advantage. Document why and limit polling to the period when it is needed.
- Prefer direct ownership and borrow by reference when the owner outlives the caller. Use smart pointers when the lifetime requires shared ownership, independent transfer, or a weak reference.
- Keep SQL wrappers such as `db::Connection` generic. Do not add application-, library-, or query-specific types, declarations, or friendships to them. Expose a generic database operation or access mechanism when query implementations need additional capabilities.
- Route library operations through typed queries dispatched by the selected library backend. Reuse an existing query where possible, or add one when needed. Keep ad-hoc SQL for cases that require it, and keep library SQL inside the local typed-query implementation so the same operation can run against local and remote libraries.
- Protect all database operations from SQL injection. Use prepared statements and bind every variable value, including each value in an `IN` list. Select SQL identifiers and syntax from fixed fragments or explicit allowlists. Use direct execution only for fixed SQL with no variable values.
- Verify changes through their normal entry point. Add tests as you go for useful contracts and regressions that existing tests miss. Each test should protect behavior that matters.
- After a major C or C++ refactor, ask whether the user wants an independent critical C/C++ reviewer. Run the review when the user requests it, including on demand outside a refactor. Require concrete pros and cons, before-and-after examples, and meaningful measurements where available. Identify remaining risks and gaps in verification. For each substantive criticism, propose a practical improvement when possible and explain its tradeoffs. An overall verdict alone is insufficient.
- When completing a backlog section, capture meaningful architectural discussions and decisions as individual, numbered Markdown ADRs under `docs/adr/`, linked from the relevant design document. Explain the goal and problem that led to the decision, relevant alternatives, why the chosen approach fits, and its architectural consequences; include status. Keep implementation tasks and routine progress in the LLD/checklist. Do not split one cohesive decision into several policy notes or duplicate decisions already recorded.

- Keep ADRs focused on architectural choices and their rationale. Put transitional implementation routes, temporary workarounds, build/artifact wiring, validation exceptions, and progress in the LLD. Update an accepted ADR when the architectural decision changes, not whenever its implementation plan changes.

## Build scripts

- Keep new Unix build scripts in Node.js. Reuse `script/unix-build-config.js` for shared compiler and target settings.
- Keep test orchestration, fixture-generation scripts, build checks, and benchmark runners under `tests/scripts/`. Put TUI scenarios in `tests/ui/scenarios/` and their reusable driver code in `tests/ui/support/`.
- Keep tests top-level in this monorepo, grouped under `tests/unit/`, `tests/integration/`, and `tests/ui/`. Group unit tests by owning project and share native helpers through `tests/support/`. Give each CTest entry exactly one level label (`unit`, `integration`, or `ui`), plus owner and subsystem labels; `contract` is an additional guarantee label. Add coverage as behavior changes rather than backfilling untouched code.
- Keep compiler-specific flags and setup in compiler profiles. Use the existing vendor include and library directory lists in CMake targets.
- Keep package-specific build steps under `script/vendor-packages/`. Define fixed configure and build arguments as named constants in the recipe.
- Keep staging, caches, logs, and temporary build files under the build root. Use the existing build cache and staging directories where possible.
- Keep routine vendor build output quiet by default. Stream command output with `--verbose` and retain failure diagnostics.
- Keep the Linux x86-64 standalone build on Zig with the glibc 2.28 baseline. macOS uses the default compiler. Audit packaged binaries with `script/scan-standalone.js`.

## musikcore C ABI and tests

- Follow [the C ABI design](doc/musikcore-c-abi-design.md), [the high-level migration plan](doc/musikcore-c-abi-hld.md), [the task checklist and foundation gate](doc/musikcore-c-abi-lld.md), and [the testing design](doc/testing-design.md) when working on ABI modernization. Architectural decisions are linked from the HLD and recorded individually under `docs/adr/`.
- Complete the core C API foundation gate before adding feature families, migrating more application functionality, or beginning plugin/provider API work.
- Keep the application API and plugin SDK as separate contracts. Specify ownership and lifetime at the ABI boundary.
- Group public C declarations in subsystem headers that compile on their own. Include application-facing subsystem headers from `musikcore.h`.
- Keep the public `musikcore.hpp` wrapper strictly header-only. It depends on the public C ABI and uses no private core headers.
- Make public C++ wrapper ownership explicit. Declare copy and move operations for owning types. Make read-only accessors const and keep operations that change runtime state non-const.
- Document each public C++ wrapper class's copy and move semantics in a comment at its declaration.
- Keep the experimental ABI version fixed during development. Public signatures and layouts may change freely until the ABI freeze.
- Document every field in public ABI structs, including units, ownership, and default values where applicable. Provide an `MC_*_DEFAULT` initializer for every public struct with a `struct_size` field.
- When completing an ABI task, update its checklist entry and the named test in the same change. Mark a gate complete only after its acceptance criteria pass on the named platforms or the task plan records an exception.
