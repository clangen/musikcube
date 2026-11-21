# ADR-0002: deliver async completions through host-controlled dispatch

Status: accepted

Date: 2026-11-20

## Context

Library work can block on databases or remote replies, while the application
must keep its UI and playback responsive. Different runtimes have different
event loops and callback-thread requirements. Invoking foreign callbacks from
arbitrary workers would force every binding to solve cross-thread delivery and
teardown independently.

Cancellation also cannot guarantee that work stops before a queued completion.
Without a terminal delivery contract, a binding cannot know when callback data
or a retained result may be released. Remote replies can arrive after a library
switch, cancellation, or context shutdown.

## Decision and rationale

Run slow work asynchronously, then deliver its completion on a host-selected
dispatcher. A nonblocking dispatch call lets a TUI, GUI, or runtime integrate
without surrendering its event loop. The wakeup hook only signals that loop;
it may run on producers and must not execute dispatch itself.

An accepted request gets exactly one terminal callback. Cancellation competes
with completion to select that outcome; it cannot retract one already selected.
A rejected submission gets no callback. Inputs are copied or retained before
admission returns, and callback data remains the caller's responsibility until
terminal delivery. This makes cleanup independent of whether success,
cancellation, failure, or shutdown wins the race.

Shutdown stops admission, settles accepted requests, joins workers, waits for
in-flight hooks, and drains completions before the host releases callback state.
Late replies cannot deliver twice or address a destroyed context. Callbacks run
without internal queue locks, and may submit or cancel work. They cannot shut
down or destroy the context currently delivering them.

Bind dispatch/shutdown to one thread. Reject owner-thread violations with
`MC_ERROR_WRONG_THREAD` and nested dispatch with `MC_ERROR_BUSY`, so callers can
distinguish a permanent calling error from temporary reentrancy.
This provides a serial delivery contract instead of making callers tolerate
reentrant teardown. Unsolicited notifications are a separate planned surface;
request completion does not depend on notification polling.

## Alternatives considered

- Synchronous calls would block UI/event-loop threads and make remote operations
  especially difficult to integrate.
- Worker-thread callbacks would export core scheduling decisions into every
  language binding and make owner destruction harder to coordinate.
- Silent cancellation would leave callback owners unable to distinguish a
  completed teardown from a lost reply.
- A mandatory runtime-specific future or scheduler would privilege one host
  model. Optional adapters can be layered over the dispatch contract instead.

## Consequences and implementation

The host must service dispatch and retain its callback data until completion.
Wakeups need coalescing and race-safe rearming. Admission reserves terminal
capacity so shutdown/cancellation can still complete accepted work under pressure.
Request IDs are context-local and nonzero; the registry selects terminal state
under synchronization, then invokes callbacks and hooks outside its locks.

Owner-thread shutdown is a real constraint, including for C++ RAII destruction.
Raw request IDs plus explicit cancellation remain preferable to a request object
that appears to retain a context but actually cannot keep it alive. The current
single-context restriction is an implementation limit, not a reason to expose
shared singleton state as an implicit public contract.

The [LLD runtime design](../../doc/musikcore-c-abi-lld.md#runtime-component-design)
records the state machines and implementation work. `musikcore.abi.dispatch` and
`musikcore.abi.remote_query_transport` exercise the races and teardown contract.
