# Enforce the frontend consumer boundary

## Status

Implemented and verified on Linux. Native macOS verification is pending, and Windows is deferred.

## Context

Cube can start the full runtime through its public context. Daemon and plugin consumers still use parts of the original C++ interfaces. The standalone ABI0 library used in contract tests supplies the request dispatcher, but it has no production audio engine or production plugin loader.

## Decision

Build Cube against the production runtime through the C ABI, and verify the resulting executable's imports against the public export manifest. Keep private precompiled headers within the core target and keep the UI message queue in the executable. Shared header-only utilities remain usable by either side.

## Alternatives

Shipping a second production library alongside the current runtime would let the frontend link against a narrower artifact, but it would duplicate the runtime and its process-wide services. Hiding the production library's private symbols now would break daemon and provider consumers before their migration. A source-only check would miss implicit dependencies introduced by transitive compiler inputs.

## Consequences

The same production library supplies the app, existing daemon, and existing providers. The frontend contract test rejects private core headers and dynamic imports. Provider migration can later remove the remaining private exports without changing how Cube opens or drives its context. The minimal ABI0 artifact still isolates boundary contracts in tests.
