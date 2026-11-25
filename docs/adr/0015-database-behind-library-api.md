# ADR-0015: keep database access behind the library API

Status: accepted

## Context

The application API serves libraries through typed operations that work with
local and remote backends. Exposing database handles or arbitrary SQL would
couple consumers to a particular storage implementation and schema.

## Decision

The public application API does not expose raw database handles or arbitrary
SQL, including through an unstable extension. Consumers use library operations;
new use cases are expressed as typed operations on the selected backend. The
database implementation and schema may evolve independently of the public API
as long as its documented behavior remains intact.

## Alternatives

An unstable raw-database extension could support storage-specific tools, but it
would create a second contract tied to implementation details and would not
naturally work with remote libraries. We do not add that surface without a new
decision and a concrete use case.

## Consequences

Storage changes must preserve public behavior, and new library operations need
local and remote backend implementations and appropriate boundary tests. This
decision does not prevent the core from using SQL internally. DEC-007 in the
[core LLD](../../doc/musikcore-c-abi-lld-core.md) records its implementation status.
