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
- Let the code explain what it does. Reserve comments for longer blocks, especially those with branches, where they can mark a phase or explain why a decision was made. Keep comments terse and technical.
- Use `/* comment */` where the language supports block comments. Use `//` only when necessary and the language's native syntax where block comments are unavailable.
- Keep platform and compiler differences near setup. Pass the resulting options into shared build and application code.
- Make ownership, cleanup, and error handling explicit at resource and thread boundaries.
- Verify changes through their normal entry point. Add focused tests for new contracts and regressions that existing tests miss.

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
- When completing an ABI task, update its checklist entry and the named test in the same change. Mark a gate complete only after its acceptance criteria pass on the named platforms or the task plan records an exception.
