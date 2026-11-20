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
Retain CTest as the build/test entry point; choosing another assertion framework
is a separate tooling decision, not required by this architecture.

Native platform boundary tests are necessary: macOS success does not establish
Windows DLL or Linux ELF behavior. Performance/allocation measurements are
separately selectable and compare equivalent environments, rather than adding
unstable timing assertions to every correctness test.

The [testing design](../../doc/testing-design.md) owns harness details, and the
[LLD foundation gate](../../doc/musikcore-c-abi-lld.md#core-c-api-foundation-gate)
owns acceptance and named checks. Test progress and counts stay there, not in
this decision record.
