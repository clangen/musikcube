# ADR-0014 — Playback remote command lifetime

**Status** Accepted

## Context

A playback remote can submit commands from a network or platform worker. The
playback session executes them on its owner thread, which may run after the
originating client disconnects. Runtime shutdown also has to stop remote
workers before their plugin can be unloaded.

## Decision

Once the host accepts a playback command, a client disconnect does not cancel
it. The remote keeps its completion state alive until the command completes and
discards any response whose client has disconnected. This policy does not
promise that a command was unexecuted when a client stops waiting.

During runtime shutdown, the host rejects new commands and completes queued
commands with cancellation before joining remote workers and unloading their
plugins. Commands are bounded per remote; rejected submissions have no callback.
Completion runs on the owner thread and may race the submitting worker.
For operations that need the full playback or queue API, a remote may request
an owned playback handle through the same bounded owner-thread dispatch. The
remote releases the handle when finished; cancellation supplies no handle.
The callback must not wait for another owner-thread completion. Some playback
operations can themselves wait for remote metadata, so a server must keep its
network thread available while those operations run.

The server sends playback requests to that owner without waiting on its network
thread. Responses can arrive out of order if a client sends several requests
without waiting; the owned clients wait for snapshot creation before using it.
This keeps the server available when playback needs metadata from that server.

If a remote fails to start, stop and join that remote while leaving healthy
remotes running. A remote must detach external callbacks and wake its own
workers without depending on other remotes stopping. Its workers may wait for
its own accepted host commands: the host cancels those before joining.

## Alternatives and consequences

Per-client cancellation would require the host to track additional request
identities and coordinate cancellation with execution and disconnect races.
Allowing accepted commands to finish keeps the control path smaller, but a user
may hear a playback change after the controlling client disconnects. Callers
that need a response must keep their callback state alive until completion.
Keeping healthy remotes available requires each failed remote to honor its
independent stop contract; a plugin that fails to wake its own workers can
otherwise block startup or shutdown.
