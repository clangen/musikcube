# ADR-0004: test contracts at the public boundary

Status: accepted

Date: 2026-11-20

## Context

Internal tests cannot establish that a separately compiled foreign consumer
can load musikcore, own its results, receive callbacks, or shut down safely.
Interactive demos also hide lifecycle errors and depend on personal data and
hardware. Testing every untouched implementation first would delay proving the
new boundary.

## Decision and rationale

Start with executable contracts through the public C ABI, then expand inward
to focused implementation tests and outward to production integration and UI
flows. Use separately compiled C consumers and the real shared-library seam.
Apply the same approach to plugins, with pure-C fixture providers.

Keep routine checks hermetic and deterministic. Add coverage for changed
behavior, reproduced regressions, and meaningful ownership or lifecycle
contracts. A synthetic test alone does not establish production app parity.

## Alternatives considered

Testing only internal classes can miss exports, headers, allocators, and foreign
callback lifetimes. Testing only through the app makes failures hard to isolate.
Mock-only checks cannot prove database, socket, or native-module boundaries.
Hardware and personal libraries make routine acceptance environment-dependent.

## Consequences

Tests must prove the boundary that owns validation and lifecycle ordering;
a mock caller that behaves correctly does not establish enforcement. Platform
acceptance remains necessary because one operating system cannot prove another's
loader or calling conventions. Performance measurements remain separate from
ordinary correctness checks.

The [testing design](../design/testing-design.md) owns test organization, runners,
fixtures, and artifacts. The [LLD](../design/musikcore-c-abi-lld.md) owns acceptance
gates, named checks, progress, and validation exceptions.
