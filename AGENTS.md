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

- Reuse existing helpers and follow the surrounding code's conventions. Introduce an abstraction when it simplifies real callers.
- Write nested C++ namespace declarations on one line, such as `namespace musik { namespace core { namespace audio {`. Indent the body once and close with `} } }` on one line.
- Use PascalCase for C++ member functions, including private members. Follow the surrounding code for file-local helpers, which commonly use lower camelCase. Keep exported C ABI functions in snake_case.
- Let the code explain what it does. As you write or revise longer functions, add terse, technical comments at phase boundaries and branching handoffs to make the flow easy to follow. Explain decisions where the reason is otherwise unclear.
- Use blank lines to separate distinct steps in a function. Keep closely related statements together and give transitions between phases room to stand out.
- Name constants for public keys, limits, sentinel values, and positional indices whose meaning matters. Reuse the same definition across callers. Keep local constants near their use, and leave obvious literals inline.
- Use `/* comment */` where the language supports block comments. Use `//` only when necessary and the language's native syntax where block comments are unavailable.
- Keep platform and compiler differences near setup. Pass the resulting options into shared build and application code.
- Make ownership, cleanup, and error handling explicit at resource and thread boundaries.
- Route library operations through typed queries dispatched by the selected library backend. Reuse an existing query where possible, or add one when needed. Keep ad-hoc SQL for cases that require it, and keep library SQL inside the local typed-query implementation so the same operation can run against local and remote libraries.
- Protect all database operations from SQL injection. Use prepared statements and bind every variable value, including each value in an `IN` list. Select SQL identifiers and syntax from fixed fragments or explicit allowlists. Use direct execution only for fixed SQL with no variable values.
- Verify changes through their normal entry point. Add tests as you go for useful contracts and regressions that existing tests miss. Each test should protect behavior that matters.

## Build scripts

- Keep new Unix build scripts in Node.js. Reuse `script/unix-build-config.js` for shared compiler and target settings.
- Keep compiler-specific flags and setup in compiler profiles. Use the existing vendor include and library directory lists in CMake targets.
- Keep package-specific build steps under `script/vendor-packages/`. Define fixed configure and build arguments as named constants in the recipe.
- Keep staging, caches, logs, and temporary build files under the build root. Use the existing build cache and staging directories where possible.
- Keep routine vendor build output quiet by default. Stream command output with `--verbose` and retain failure diagnostics.
- Keep the Linux x86-64 standalone build on Zig with the glibc 2.28 baseline. macOS uses the default compiler. Audit packaged binaries with `script/scan-standalone.js`.

## musikcore C ABI and tests

- Follow [the C ABI design](doc/musikcore-c-abi-design.md), [the high-level migration plan](doc/musikcore-c-abi-hld.md), [the task checklist](doc/musikcore-c-abi-lld.md), and [the testing design](doc/testing-design.md) when working on ABI modernization.
- Keep the application API and plugin SDK as separate contracts. Specify ownership and lifetime at the ABI boundary.
- Group public C declarations in subsystem headers that compile on their own. Include application-facing subsystem headers from `musikcore.h`.
- Keep the public `musikcore.hpp` wrapper strictly header-only. It depends on the public C ABI and uses no private core headers.
- Make public C++ wrapper ownership explicit. Declare copy and move operations for owning types. Make read-only accessors const and keep operations that change runtime state non-const.
- Document each public C++ wrapper class's copy and move semantics in a comment at its declaration.
- Keep the experimental ABI version fixed during development. Public signatures and layouts may change freely until the ABI freeze.
- Document every field in public ABI structs, including units, ownership, and default values where applicable. Provide an `MC_*_DEFAULT` initializer for every public struct with a `struct_size` field.
- When completing an ABI task, update its checklist entry and the named test in the same change. Mark a gate complete only after its acceptance criteria pass on the named platforms or the task plan records an exception.
