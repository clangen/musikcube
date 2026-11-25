# ADR-0016: version the application and plugin ABIs independently

Status: accepted

## Context

The application C API serves frontends and other consumers. The plugin C ABI
serves providers such as decoders and audio outputs. Their callers, contracts,
and reasons for change differ.

## Decision

The application ABI and plugin ABI have independent versions and evolve
independently. A change to one does not by itself advance the other's version.
Each boundary must negotiate or validate its own version and document the
compatibility impact of its changes. During experimental development, existing
repository rules for ABI versions continue to apply until freeze.

## Alternatives

A single shared version would make the release number easy to communicate, but
would also suggest that changes to one boundary necessarily affect users of
the other. Independent versions express the actual compatibility boundaries.

## Consequences

Release notes and compatibility checks must identify the boundary and version
they describe. Independent ABI versions do not imply that arbitrary client and
server releases interoperate; their wire compatibility remains a separate
contract. DEC-005 in the [core LLD](../../doc/musikcore-c-abi-lld-core.md)
tracks the decision; ABI freeze and compatibility tests remain separate gates.
