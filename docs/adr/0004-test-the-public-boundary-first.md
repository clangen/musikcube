# ADR-0004: start tests at the public ABI boundary and expand with changes

Status: accepted

Date: 2026-11-20

## Context

The refactor changes how applications and eventually plugins own objects,
receive async results, and load musikcore. Internal unit tests alone cannot
show that a foreign consumer can compile, load the library, release its results,
or shut down safely. An interactive app demo can hide lifecycle errors and
depends too much on user data, timing, and physical audio output.

We also want useful coverage without turning this effort into a retrospective
campaign to test every untouched part of the application.

## Decision and rationale

Begin with executable contracts at the public C boundary, then fan inward to
focused implementation tests and outward to production integration and UI tests.
Use separately compiled C consumers and the actual shared-library boundary so
private includes, accidental C++ exports, and allocator/lifetime mistakes cannot
be hidden by a test linking the same implementation directly.

Add tests for behavior we change, regressions we find, and contracts that make
the migration safe. Use musikcube and its TUI driver to validate migrated user
flows; a successful synthetic API test alone does not establish app parity.
Apply the same approach to the provider ABI when that initiative begins, with
pure-C fixture plugins before migrating production providers.

Keep default tests hermetic: isolated databases/preferences/plugin roots,
deterministic media, fake audio output, loopback dynamic ports, and deadline-based
operation predicates. Establish race ordering explicitly instead of using sleeps
to guess when work has completed.

## Organization and execution

Keep one repository-level test tree rather than a separate test root per
project. Organize by the boundary exercised, with unit ownership explicit:

```text
tests/
    scripts/                 runners, fixture generation, CMake checks
    unit/
        musikcore/
        musikcube/
    integration/
        api/
        app/
        library/
        transport/
        binary/
        performance/
    ui/
        scenarios/
        driver/              tests of the driver itself
        support/             screen, PTY, artifact and benchmark helpers
    support/                 native fixtures and shared C/C++ checks
    baselines/
```

Each CTest entry has exactly one level label: `unit`, `integration`, or `ui`.
Owner and subsystem labels describe what it covers; `contract` describes a
promised behavior, not a fourth test level. Real SQLite, socket, shared-library,
and production-service boundaries belong in integration tests even when a
component is small or uses fake audio output. A terminal parser is a unit;
a PTY launcher interacting with a child is an integration boundary; keystrokes
through musikcube's real UI are UI tests. Existing test names remain stable
where practical; labels are the selection authority.

Retain CTest as the outer runner and use Node's built-in runner for JavaScript.
Native tests currently use explicit named cases and a small C-compatible
assertion helper. This provides independent reporting without adding another
dependency during the organization work. Adopt a C++ assertion framework only
when diagnostics or test authoring show a concrete limitation; any future
framework must be pinned, test-only, and available without an implicit network
download. It does not replace CTest or turn the C11 boundary consumers into C++.

Split broad native checks and UI workflows into individually selectable cases
with fresh state. Keep a combined UI flow for end-to-end interaction coverage,
while ordinary smoke tests report search, queue, playlist, and browse behavior
separately. Run large catalogs and repeated timing comparisons explicitly;
ordinary correctness assertions do not enforce machine-specific latency limits.

A rendered screen must correspond to an input-ready view. Fix application focus
ordering when needed instead of delaying input with arbitrary sleeps. Test the
driver's parsing, cursor, erase, scroll, styling, Unicode cells, and resize
behavior separately so a driver defect cannot silently invalidate UI assertions.
The screen model covers CJK width and combining marks, not every emoji/ZWJ or
terminal-dependent glyph presentation.

Failed scenarios keep their isolated runtime, configuration, indexer log,
terminal output, visible text, and SVG under the build root, with the retained
path in the error. Successful runs clean up. Test harness failures must not
erase the evidence needed to diagnose them.

## Alternatives considered

- Broadly backfill unit tests first: delays validating the new public boundary
  and spends effort on implementation details the migration may replace.
- Only exercise the API from the app: misses external header/link/load defects
  and makes focused failures harder to diagnose.
- Only use mocked API tests: cannot expose serialization, database conversion,
  or real module-boundary defects.
- Require hardware, a personal library, or internet services: makes routine
  correctness checks environment-dependent and unsuitable for unattended runs.

## Consequences and implementation

Keep checks selectable and failure messages tied to observable contracts.
Retain the normal `node tests/scripts/run-tests.js` entry point. Select a level
with `--suite`, inspect available checks with `--list`, or pass ordinary CTest
filters after `--`. Test executables remain separately compiled at ABI seams.

Native platform boundary tests are necessary: macOS success does not establish
Windows DLL or Linux ELF behavior. Performance/allocation measurements are
separately selectable and compare equivalent environments, rather than adding
unstable timing assertions to every correctness test.

The [testing design](../../doc/testing-design.md) owns harness details, and the
[LLD foundation gate](../../doc/musikcore-c-abi-lld.md#core-c-api-foundation-gate)
owns acceptance and named checks. Test progress and counts stay there, not in
this decision record.
