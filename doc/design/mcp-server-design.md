# musikcube MCP server design

Status: proposed future initiative

## Summary

musikcube should expose a Model Context Protocol (MCP) server that allows an
authorized agent to understand a user's local music collection and create
useful, tailored playlists from natural-language requests.

The product goal is not merely protocol support. It is to enable experiences
such as:

- "What are some interesting patterns in my music collection?"
- "Show me highly rated music I have not played in a year."
- "Create a 60-minute playlist for exercise."
- "Build a dinner playlist from music I already own, but research the artists
  and favor mellow recordings."
- "Make a road-trip playlist with familiar songs and a few things I have not
  listened to recently."

The musikcube MCP server supplies grounded collection data, safe aggregate
queries, stable local track references, playlist validation, and atomic
playlist mutations. The agent performs natural-language interpretation and may
use separate web-search tools for external research. The server itself does not
contain an LLM and does not browse the web.

This is a separate initiative that begins after the C ABI and C SDK
modernization is complete. It must use the new supported musikcore boundaries
rather than current private C++ SDK interfaces or raw database access.

## Product vision

The user should be able to describe intent rather than database predicates.
An agent translates that intent into a transparent workflow:

```text
User request
    |
    v
Agent interprets mood, activity, duration, novelty, and constraints
    |
    +--> musikcube: inspect collection profile and metadata coverage
    |
    +--> musikcube: query or sample plausible local tracks
    |
    +--> optional web search: research tempo, mood, artist, era, or context
    |
    +--> musikcube: resolve researched candidates to actual local tracks
    |
    +--> musikcube: prepare and validate playlist draft
    |
    +--> user reviews or authorizes mutation
    |
    +--> musikcube: atomically create playlist
```

musikcube remains the source of truth for what the user owns and what can be
added to a playlist. External research is advisory. An agent cannot create a
playlist entry for a song that musikcube cannot resolve to the local
collection.

## Dependency on the C modernization

Implementation should begin only after the new C surfaces provide:

- Stable context and lifecycle behavior.
- Async library queries and immutable metadata pages.
- Structured filters, paging, and aggregate-friendly metadata access.
- Stable track and playlist identities.
- Atomic playlist creation and editing.
- Playback and queue operations if they are later exposed through MCP.
- A supported way for the server plugin to consume scoped library services
  through the C SDK host or public consumer surface.
- Boundary contract and integration tests on Windows, macOS, and Linux.

The server plugin must not create a second musikcore context in the same
process. It consumes the existing host's scoped services or, if the server is
eventually a standalone process, owns one public C API context.

The C modernization may be adjusted to support efficient aggregates and stable
track references, but MCP-specific JSON-RPC, schemas, authorization, and agent
concepts do not belong in the core C ABI.

## Goals

- Let agents answer meaningful questions about the user's collection without
  downloading the entire library.
- Let agents find and select local tracks using structured, bounded queries.
- Support optional research using independent web-search tools.
- Safely match external research results to tracks that actually exist in the
  local collection.
- Create playlists through a previewable, idempotent, atomic workflow.
- Protect collection privacy and require explicit authorization for all access.
- Default to read-only behavior and loopback-only network exposure.
- Keep filesystem paths, raw SQL, and unrestricted internal metadata private.
- Provide structured output schemas that are efficient for agent context
  windows.
- Make every mutation auditable and attributable to a client identity.
- Share one transport-independent domain service with the existing remote API
  where practical.

## Non-goals

- Embedding an LLM or web-search client inside musikcube.
- Automatically uploading the user's collection to an external service.
- Exposing raw SQLite access or the existing raw-query remote command.
- Making arbitrary tag or database modifications.
- Streaming audio through MCP.
- Replacing the existing audio and thumbnail HTTP endpoints.
- Providing playlist deletion, library reindexing, output configuration, or
  other administrative tools in the initial release.
- Guaranteeing the truth of metadata obtained through an external web search.
- Supporting unauthenticated collection browsing, even in read-only mode.
- Implementing every historical MCP protocol revision.

## Architectural overview

### Agent-side composition

MCP is one tool source available to the agent. External research is performed
through a separate, independently authorized capability:

```text
                       +--------------------+
                       | Agent / MCP client |
                       +----------+---------+
                                  |
                 +----------------+----------------+
                 |                                 |
       +---------v----------+             +--------v--------+
       | musikcube MCP      |             | Web search or   |
       | local collection   |             | knowledge tools |
       +---------+----------+             +-----------------+
                 |
       +---------v----------+
       | musikcore supported|
       | C/C++ boundary     |
       +--------------------+
```

The MCP server never silently invokes a web service. This separation makes
privacy and cost visible to the user's agent host and lets the user disable web
research while retaining local collection features.

### Server-side composition

The current WebSocket server combines transport parsing with domain operations.
Before adding MCP, extract a transport-independent service:

```text
WebSocket transport ----+
                         |
MCP HTTP transport ------+--> RemoteMusicService
                         |          |
Future local API --------+          v
                              supported musikcore
                              consumer/host surface
```

`RemoteMusicService` performs authorization-aware domain operations and returns
typed internal results. It does not know about JSON-RPC, WebSocket message
names, or MCP content blocks.

The existing audio-file HTTP path remains separate because it has streaming,
range, and transcoding requirements unrelated to agent tools.

### Suggested implementation layout

```text
src/plugins/server/
    remote/
        RemoteMusicService.h
        RemoteMusicService.cpp
        CollectionQueries.h
        PlaylistDrafts.h
        PlaylistDrafts.cpp
    mcp/
        McpHttpServer.h
        McpHttpServer.cpp
        McpProtocol.h
        McpProtocol.cpp
        McpCatalog.h
        McpCatalog.cpp
        McpTools.h
        McpTools.cpp
        McpResources.h
        McpResources.cpp
        McpAuthorization.h
        McpAuthorization.cpp
        McpSchemas.h
        McpAuditLog.h
        McpAuditLog.cpp
```

Exact files depend on the selected MCP implementation or SDK. Protocol,
authorization, domain service, and tool behavior should remain separable for
testing.

## Protocol baseline

At the time this design was written, the current MCP protocol revision is
`2026-07-28`. It uses stateless HTTP requests, explicit protocol/method/name
headers, optional discovery, cacheable catalog results, and explicit handles
for workflows that carry state across calls.

Implementation must revalidate the current specification and client ecosystem
when this initiative begins. The server should initially support one deliberate
modern revision rather than accumulating partially tested compatibility paths.

Relevant protocol features include:

- Streamable HTTP at an `/mcp` endpoint.
- Structured tool input and output using JSON Schema.
- Resources and resource templates for readable snapshots.
- Tool annotations describing read-only, destructive, idempotent, and
  open-world behavior.
- Explicit multi-round-trip input requests where supported for confirmation.
- Optional Tasks extension for genuinely long-running work; not required for
  initial collection queries or playlist creation.

Tool annotations are descriptive hints and are not an authorization mechanism.
All access control is enforced by the HTTP boundary and domain service.

References:

- [MCP 2026-07-28 overview](https://blog.modelcontextprotocol.io/posts/2026-07-28/)
- [Tool annotation security model](https://blog.modelcontextprotocol.io/posts/2026-03-16-tool-annotations/)
- [MCP authorization guidance](https://ruby.sdk.modelcontextprotocol.io/server/authorization/)

## Domain model

### Track references

MCP does not expose mutable database row IDs as the primary identity. Every
returned track has an opaque `track_ref` minted by musikcube and resolved by
the server.

The internal representation may include source and external IDs, but clients
must not construct or interpret it. This permits database rebuilds and future
remote library sources without changing tool schemas.

A track summary may contain:

```json
{
  "track_ref": "opaque",
  "title": "Example",
  "artist": "Artist",
  "album": "Album",
  "album_artist": "Artist",
  "genre": "Electronic",
  "year": 2024,
  "duration_seconds": 238,
  "bpm": 128,
  "rating": 4,
  "play_count": 7,
  "last_played": "2026-10-01T18:22:00Z",
  "date_added": "2025-04-12T03:10:00Z"
}
```

Fields that are absent or unknown remain absent or null. The server reports
metadata coverage so an agent does not mistake missing BPM or play history for
zero.

Local filenames, absolute paths, arbitrary tag keys, and source credentials are
excluded from normal results.

### Playlist references

Playlists use opaque `playlist_ref` values plus a revision. Mutations that
modify an existing playlist use optimistic concurrency by requiring the
expected revision.

### Cursors and snapshots

Pagination cursors are opaque, bounded, and expire when appropriate. Query
results carry a library revision or snapshot identifier when needed to detect
stale multi-step workflows.

### Playlist drafts

A playlist draft is an opaque, expiring handle containing or referring to:

- Ordered validated track references.
- Collection revision.
- Total track count and duration.
- Duplicate handling outcome.
- Missing or stale references.
- Optional agent-supplied rationale for display or audit, not execution.
- Expiration time.

The handle may be a signed self-contained token or a server-side record. It is
explicit because the MCP transport is stateless; it is not hidden in an HTTP
session.

## Collection intelligence

Natural-language insight requires more than returning tracks by keyword. The
server should provide bounded aggregate operations that let an agent reason
about the collection without fetching every row.

### Collection profile

`library_get_profile` returns a compact overview:

- Track, album, artist, album-artist, genre, and playlist counts.
- Total known duration.
- Earliest/latest year and date-added range.
- Played, never-played, rated, and unrated counts.
- Metadata coverage percentages for BPM, year, genre, rating, play count,
  last played, and date added.
- Supported query dimensions, measures, filters, and sort modes.

Coverage is important to agent planning. For example, if only five percent of
tracks have BPM, the agent should not interpret a BPM query as the full
exercise-music candidate set. It can use genre and artist candidates, then
optionally research tempo externally.

### Safe aggregation

`library_aggregate` exposes an allowlisted analytical query rather than SQL.

Supported dimensions may include:

- Artist and album artist.
- Album.
- Genre.
- Year or decade.
- Rating.
- Date-added period.
- Last-played period.
- Played versus never played.

Supported measures may include:

- Track count.
- Total duration.
- Total play count.
- Average rating over rated tracks.
- Earliest/latest play or add time.
- Metadata coverage.

Every query has dimension, result-count, execution-cost, and time limits.
Unknown SQL expressions are impossible to supply.

This supports questions such as:

- Which genres dominate my collection?
- Which artists have I added most recently?
- How much highly rated music have I never played?
- Which decades are underrepresented?
- Which albums have I not heard in the longest time?

### Structured track queries

`library_query_tracks` accepts allowlisted filters and sorts:

- Text query.
- Artist, album artist, album, and genre.
- Year or date-added ranges.
- BPM and duration ranges where metadata exists.
- Rating range.
- Play-count range.
- Last-played before/after or never played.
- Track references to include or exclude.
- Stable sort or deterministic random sample with a supplied seed.
- Cursor and bounded page size.

The result declares which predicates operated on incomplete metadata.

### Candidate sampling

Agents often need diversity rather than the first page of a sorted query.
`library_sample_tracks` returns a deterministic, seed-controlled sample from a
structured filter. It can support per-artist and per-album caps so playlist
generation does not accidentally select one artist's entire album.

Sampling is computed by musikcube rather than by transferring thousands of
tracks to the agent.

### External candidate matching

`library_match_tracks` maps externally researched descriptors back to the
local collection. Input entries may contain title, artist, album, duration, and
year. Results include:

- Exact or fuzzy match classification.
- Confidence score.
- Resolved opaque `track_ref`, if unambiguous.
- A small bounded set of alternatives when ambiguous.
- A reason for no match.

Only resolved track references can enter a playlist draft. Matching behavior
must be deterministic, normalized for case and Unicode, and conservative about
low-confidence matches.

External metadata is not automatically written into the musikcube database.

## Proposed MCP resources

Initial read-only resources and templates:

```text
musikcube://library/profile
musikcube://playlists
musikcube://playlists/{playlist_ref}
musikcube://tracks/{track_ref}
musikcube://playback
musikcube://queue
```

Resources return bounded snapshots with cache metadata appropriate to their
volatility. Large playlists use pagination or a tool rather than an unbounded
resource response.

The playback and queue resources may be deferred if the initial product scope
focuses exclusively on insights and playlist creation.

## Proposed MCP tools

### Initial read tools

| Tool | Purpose |
| --- | --- |
| `library_get_profile` | Summarize collection size, ranges, and metadata coverage. |
| `library_aggregate` | Compute safe, bounded collection insights. |
| `library_query_tracks` | Retrieve tracks through structured filters and paging. |
| `library_sample_tracks` | Produce diverse deterministic candidate samples. |
| `library_get_tracks` | Resolve known track references to current summaries. |
| `library_match_tracks` | Match externally researched descriptors to local tracks. |
| `playlist_list` | List playlist identities and summary metadata. |
| `playlist_get` | Read one playlist with bounded paging. |

Read tools are annotated as read-only and closed-world, but their output is
still treated as untrusted user-controlled data.

### Initial mutation tools

| Tool | Purpose |
| --- | --- |
| `playlist_prepare` | Validate, normalize, summarize, and preview a proposed ordered track list. |
| `playlist_create` | Atomically create a playlist from a valid draft. |

`playlist_create` requires:

- A valid unexpired draft handle.
- A bounded playlist name.
- A client-supplied idempotency key.
- `playlist.write` authorization.
- Confirmation when the configured policy requires it.

Creating a playlist is additive and reversible, but it is still a mutation.
The tool is not marked read-only or inherently idempotent; server idempotency is
provided for repeated calls carrying the same key.

### Later mutation tools

- `playlist_append`, with expected playlist revision.
- `playlist_replace`, with stronger confirmation.
- `queue_enqueue` and `queue_replace`.
- `playback_play`, `playback_pause`, and `playback_skip`.

Playlist deletion, reindexing, settings changes, and raw queries remain outside
the normal agent surface unless a later design provides a compelling, safely
scoped user experience.

## Playlist-building workflow

### Example: "Create a playlist for exercise"

The agent can follow this workflow:

1. Call `library_get_profile` to learn collection size and BPM/genre coverage.
2. Interpret "exercise" into candidate traits such as energy, tempo, genre,
   familiarity, target duration, and desired variety.
3. Query tracks with known high BPM, relevant genres, strong ratings, or recent
   play history according to the user's likely settings.
4. If BPM or mood coverage is poor, sample plausible candidates and use an
   independently authorized web-search tool to research tempo or style.
5. Call `library_match_tracks` with researched titles to verify which candidates
   exist locally.
6. Apply diversity constraints such as per-artist and per-album caps.
7. Order tracks and call `playlist_prepare`.
8. Present the playlist duration, track count, selection rationale, unmatched
   candidates, and any metadata limitations to the user.
9. After authorization, call `playlist_create` with the draft and idempotency
   key.
10. Return the created playlist reference and a concise summary.

The server does not define what "exercise" means. The agent reasons about the
request, while musikcube provides facts, bounds, matching, and mutation safety.

### Example: collection insight without web research

For "What are some highly rated albums I have neglected?" the agent can:

1. Inspect rating and last-played coverage.
2. Aggregate or query tracks with high ratings and old or absent play history.
3. Group candidates by album.
4. Explain that the result is based entirely on local ratings and playback
   history.

No external information or mutation is required.

## Privacy and trust boundaries

### Local collection privacy

The music collection may reveal identity, interests, location, language,
religion, health context, or other sensitive information. All MCP operations
require authentication, including read-only resources.

Default results omit:

- Local filesystem paths.
- Network source URLs and credentials.
- Arbitrary metadata keys and comments.
- Raw lyrics.
- Server configuration and plugin secrets.

The user or agent host decides whether collection-derived queries are sent to a
web-search provider. musikcube itself does not transmit them.

### Untrusted library metadata

Titles, tags, comments, and lyrics are user-controlled content and may contain
text resembling agent instructions. The server:

- Returns metadata only in structured data fields.
- Never interpolates metadata into tool descriptions or server instructions.
- Does not interpret metadata as commands.
- Bounds and UTF-8-validates returned text.
- Excludes high-risk free-form fields from default tool output.

The MCP client remains responsible for prompt-injection defenses when combining
local data with other tools.

### Untrusted web research

Web search results are not authoritative. `library_match_tracks` treats them as
candidate descriptors, not executable instructions or database updates. A
low-confidence match cannot silently enter a draft.

## Authentication and authorization

### Default local mode

- MCP disabled until explicitly enabled.
- Separate listener bound to loopback by default.
- Separate randomly generated bearer credential; do not reuse the historical
  empty/default media password.
- Read-only scopes enabled first.
- Playlist mutations disabled until explicitly authorized.
- No environment-variable bypass in normal release operation.

A separate listener is preferred initially because the existing media HTTP
server may intentionally bind to a LAN interface and uses a different Basic
authentication model. The implementation may later share a listener only when
binding, TLS, and authorization policies are equivalent.

### Remote mode

Remote access requires:

- TLS directly or through a trusted reverse proxy.
- MCP-compatible OAuth resource-server behavior.
- Bearer-token validation and protected-resource discovery.
- Correct HTTP `401` and `403` challenges before tool execution.
- Host and Origin validation.
- Issuer and credential binding required by the supported MCP revision.

### Scopes

Initial scopes:

```text
library.read
playlist.read
playlist.write
```

Later optional scopes:

```text
playback.read
playback.control
library.admin
```

`library.admin` is intentionally not required by ordinary insight or playlist
workflows.

### Authorization enforcement

Authorization is checked twice:

1. At the HTTP/MCP boundary before dispatch.
2. In `RemoteMusicService` before executing a domain operation.

Tool annotations never grant access. A transport bug must not bypass domain
authorization.

## Mutation safety

### Read-only by default

The server can run in a mode that advertises no mutation tools. This should be
the default on first enablement.

### Prepare before commit

The server validates a complete playlist draft before mutation. The preview
reports:

- Name proposed by the agent.
- Track count and total duration.
- Ordered track summaries.
- Duplicate removals or warnings.
- Missing, stale, or ambiguous tracks.
- Collection revision.

### Idempotency

Every create call carries an idempotency key scoped to the authenticated client.
Retrying the same key returns the original result. Reusing the key with
different arguments is rejected.

### Optimistic concurrency

Future append or replace operations require the expected playlist revision so
an agent cannot silently overwrite changes made by the user or another client.

### Confirmation

Where supported, MCP multi-round-trip input can request confirmation. Because
client behavior varies, server policy does not rely solely on a confirmation
hint. Administrators can require a confirmation artifact or disable mutation
tools entirely.

### Atomicity

Playlist creation is one database transaction. Partial playlists are not
published. Failure releases the draft or leaves it retryable according to the
documented result.

## Limits and abuse resistance

Initial limits should include:

- Maximum request body size.
- Maximum page and aggregation result count.
- Maximum candidate descriptors passed to `library_match_tracks`.
- Maximum draft track count and total serialized size.
- Maximum playlist name length.
- Query execution timeout and cost budget.
- Per-client concurrent request and rate limits.
- Draft expiration and per-client draft quota.

Limits are applied before expensive database work. Error responses explain
which bound was exceeded without revealing internal SQL or paths.

## Observability and audit

Every MCP request receives a correlation ID. Structured operational logs
record:

- Authenticated client identity.
- Tool or resource name.
- Result code and duration.
- Number of rows or tracks returned.
- Mutation target and created playlist reference.
- Idempotency outcome.

Logs should not include bearer tokens, entire request bodies, filenames, or
complete collection results by default.

The application should eventually expose a small "recent agent activity" view
showing playlist mutations and failed authorization attempts.

Metrics may include request counts, latency, error rate, query limit rejection,
draft creation/expiration, and playlist commit outcomes.

## Implementation approach

### Protocol implementation decision

At implementation time, evaluate:

1. A maintained C or C++ MCP SDK compatible with the selected revision.
2. A focused native implementation over libmicrohttpd.
3. A separately packaged sidecar using a Tier 1 MCP SDK and the existing remote
   service.

The current ecosystem does not make this a permanent decision today. A native
implementation avoids another runtime but assumes responsibility for protocol
versioning, header validation, JSON-RPC errors, schemas, authorization, and
conformance. A sidecar simplifies protocol maintenance but complicates
packaging, lifecycle, and access to the live musikcore instance.

Regardless of transport choice, `RemoteMusicService` and its tests remain the
same.

### HTTP handling

If implemented in the existing C++ server process, the MCP listener must add:

- POST body accumulation with a strict size limit.
- Content-Type and protocol header validation.
- JSON-RPC parsing and error mapping.
- MCP method/name header validation for the selected revision.
- Stateless request context.
- Authentication middleware before body dispatch where possible.
- Bounded response serialization.
- Graceful shutdown with in-flight request tracking.

The existing audio GET router should not grow a second monolithic command
switch. MCP dispatch uses a catalog of typed handlers and generated or
centrally defined schemas.

## Testing strategy

### Domain-service tests

- Collection profile and metadata coverage calculations.
- Allowlisted aggregate dimensions and measures.
- Query filtering, paging, sampling, and cost limits.
- Stable/opaque reference resolution.
- Exact, fuzzy, ambiguous, and failed external candidate matching.
- Draft validation, duration, ordering, duplicates, and expiration.
- Atomic playlist creation and idempotent retries.
- Optimistic concurrency for later edits.

### MCP contract tests

- Protocol version and header validation.
- Tool/resource discovery and schema validity.
- Structured output conformity.
- Pagination and bounded responses.
- Tool annotation accuracy.
- JSON-RPC error mapping.
- Request body and rate limits.
- Stateless draft-handle workflow.

### Authorization tests

- Every read operation requires `library.read` or `playlist.read` as defined.
- Every mutation requires `playlist.write`.
- Missing, invalid, expired, and insufficient-scope tokens.
- Loopback and remote bind policies.
- Host and Origin validation.
- Transport authorization cannot bypass service authorization.
- Audit entries contain identity and action without secrets.

### Privacy tests

- Normal outputs never contain filenames, local paths, credentials, or raw
  arbitrary metadata.
- A malicious title such as an embedded agent instruction remains an inert
  structured field.
- External candidate text cannot change a tool name, query structure, or
  authorization decision.

### Scenario tests

Use a deterministic fixture library to test complete workflows without an
actual model:

- Exercise playlist from BPM-rich metadata.
- Exercise playlist with missing BPM plus recorded external candidate
  descriptors.
- Highly rated but neglected collection insight.
- Diverse playlist with per-artist and per-album caps.
- Ambiguous web candidate requiring agent/user selection.
- Playlist create retry returning the original result.
- Concurrent user edit causing a stale revision rejection.

Web search itself is outside the musikcube test boundary. Recorded descriptors
exercise the matching and playlist workflow deterministically.

### Conformance and compatibility

- Run the selected MCP conformance suite where available.
- Test at least two independent MCP clients.
- Preserve tool schema snapshots and version changes deliberately.
- Revalidate supported protocol revisions before every major release.

## Delivery phases

### Phase 0: prerequisite validation

- Confirm C ABI/C SDK modernization and relevant compatibility gates are
  complete.
- Confirm the server plugin uses supported musikcore surfaces.
- Inventory available collection statistics and identify any missing public
  aggregate operations.
- Revalidate the current MCP specification and SDK landscape.

### Phase 1: transport-independent remote service

- Extract collection queries and playlist operations from WebSocket transport
  code.
- Add scoped authorization to service methods.
- Port existing WebSocket operations to the service without changing client
  behavior.
- Add domain-service unit and integration tests.

### Phase 2: read-only MCP insights

- Add the loopback MCP listener and authentication.
- Implement profile, aggregate, query, sample, track lookup, matching, and
  playlist-read tools/resources.
- Add schemas, limits, audit logging, privacy tests, and MCP conformance tests.
- Ship behind an experimental configuration flag in read-only mode.

### Phase 3: playlist drafts and creation

- Implement draft handles, validation, expiration, and preview.
- Add idempotency and atomic playlist creation.
- Add `playlist.write` authorization and confirmation policy.
- Add end-to-end natural-language workflow fixtures without embedding an LLM.

### Phase 4: optional remote deployment

- Add TLS/reverse-proxy documentation.
- Add OAuth protected-resource behavior and scope enforcement.
- Add host/origin validation and operational rate limits.
- Test independent remote MCP clients.

### Phase 5: playback and richer workflows

- Evaluate queue and playback control tools.
- Add subscriptions for playlist, queue, or playback changes where useful.
- Evaluate long-running task support for explicitly approved operations.
- Add an in-application agent activity and authorization view.

## Success criteria

The initial initiative succeeds when:

- An authenticated agent can summarize meaningful collection characteristics
  without retrieving the full library.
- The agent can query and sample bounded local candidates using play history,
  ratings, dates, genres, years, duration, and BPM where available.
- The agent can research candidates externally without musikcube sending data
  to the web itself.
- External candidates are conservatively resolved to actual local tracks.
- The user can preview and atomically create a tailored playlist.
- Retries cannot create duplicate playlists.
- Read-only mode is useful and is the default.
- No MCP response exposes raw SQL, local paths, credentials, or unbounded
  collection data.
- Authorization and mutation behavior pass independent contract tests.
- The server works with at least two MCP clients on supported desktop
  platforms.

## Open questions

- Which MCP protocol revision and compatibility window should the first release
  support when implementation begins?
- Should the initial listener always use a separate port, or may it share the
  media HTTP listener under strict equivalent policy?
- Which maintained SDK or protocol implementation should be used?
- What opaque track-reference format survives local and remote library sources?
- Which aggregate dimensions and measures provide the best insight without
  becoming a general query language?
- How much fuzzy matching is acceptable before user confirmation is mandatory?
- Should playlist drafts be signed self-contained tokens or server-side
  records?
- Should a playlist store agent rationale or only expose it in the audit log?
- What confirmation behavior is compatible across target MCP clients?
- Should listening-history and date-added fields require an additional privacy
  scope beyond general library read access?
- Which application UI should manage tokens, scopes, recent agent actions, and
  mutation enablement?
- Should playlist append/replace ship before any playback-control tools?

## Decision summary

The musikcube MCP server is a post-C-modernization initiative focused on user
outcomes: collection insight and natural-language playlist creation. It exposes
bounded, structured facts and carefully scoped mutations while leaving language
reasoning and optional web research to the user's agent.

The server uses a transport-independent `RemoteMusicService`, provides compact
collection profiles and safe aggregations, maps external research back to
opaque local track references, and creates playlists through an explicit
prepare/commit workflow. It is authenticated, loopback-only, and read-only by
default. Raw database access, filesystem paths, unbounded results, and automatic
external transmission are outside the design.
