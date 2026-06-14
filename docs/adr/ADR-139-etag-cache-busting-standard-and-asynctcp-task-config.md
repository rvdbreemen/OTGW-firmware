# ADR-139 ETag + Bounded max-age as the Project-Wide Cache-Busting Standard for Web UI Static Assets (Retire ?v= Query Versioning and the Chunked Index Rewriter); Align AsyncTCP Task Config with the EMS-ESP32 Blueprint

## Status

Proposed, 2026-06-14. Guideline-level (per ADR-080): this is a
pattern/idiom decision with no automated CI gate planned, so it is enforced at
PR review, not by `evaluate.py` or `bin/adr-judge`.

This ADR **proposes to amend ADR-132** (Accepted 2026-06-14) and **to supersede
ADR-026** (Accepted 2026-01-31):

- It **amends ADR-132** by narrowing the "Body-size routing" sub-decision: how
  the web UI static assets are served. The rest of ADR-132 (the imperative-push
  to async-pull bridge, the send-once invariant, REST routing on
  `AsyncResponseStream`) stands unchanged.
- It **supersedes ADR-026** ("Conditional JavaScript Cache-Busting for
  Firmware/Filesystem Version Mismatches"), whose entire decision was the
  `?v=<fsHash>` conditional cache-busting mechanism. ADR-139 retires `?v=`
  wholesale, so ADR-026's decision no longer holds anywhere. ADR-026's *goal* is
  preserved by the new mechanism (see Decision): ETag (= filesystem hash) +
  `max-age=60` still reloads JS after a filesystem flash and still caches
  efficiently when versions match. ADR-026's most acute motivation, Safari's
  24h+ stale-cache window, is bounded to 60 s and then ETag-revalidated.

ONLY ON ACCEPTANCE (a human maintainer action, not yet done) will the
back-references be recorded as status lines on the affected ADRs (the sanctioned
exception to immutability): ADR-132 would gain "Amended by ADR-139" and ADR-026
would gain "Superseded by ADR-139". Their decision bodies are not otherwise
edited. While this ADR is Proposed those back-references are NOT applied.

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Amend ADR-132 body-size routing and supersede ADR-026's ?v= mechanism; make ETag + bounded max-age the project-wide cache-busting standard for all web UI static assets, serve plain readable files via library-managed AsyncFileResponse streaming (no gzip), retire the chunked index rewriter; align AsyncTCP task config (core affinity) with the EMS-ESP32 blueprint
    changed_via: adr-kit

## Context

The 2.0.0 line is single-target ESP32-S3 (ADR-128 dropped ESP8266) and serves
HTTP on ESPAsyncWebServer over AsyncTCP @ 3.4.10, migrated by TASK-865.9
(ADR-132, the ADR-123 Phase-3 web move). All async-web commits landed
2026-06-14 under build `alpha.192`.

A field capture (`esp32-combo`, `alpha.192`, board mode 2 OT-Direct) showed a
browser `GET /` hanging with no HTTP response (PENDING) while the firmware,
telnet, and the cooperative loop were demonstrably alive. The user then
confirmed with `curl` that the server answers REST normally:
`curl http://otgw.local/api/v2/device/info` returned a structured JSON body.
So the hang is specific to the root page (or was transient): `/` uses a
different response path than `/api`.

Before this ADR, the root-page handler was a hand-rolled
`request->beginChunkedResponse(...)`: a pull-callback that read `index.html`
line-by-line from LittleFS, found 8 versioned asset tokens (`src="./index.js"`,
`href="components.css"`, ...) and rewrote each to carry a `?v=<fsHash>` query
before the closing quote, carrying a residual buffer across filler invocations
in a `shared_ptr<IndexEmitState>`. This was novel, complex code that performed
flash I/O and byte rewriting **on the AsyncTCP service task**, and it was the
leading suspect for the `/` hang. The 8 JS/CSS sub-resources took a different
path (`serveVersionedAsset`): plain paths, cache-busted by the `?v=` query the
shell rewriter injected, with `Cache-Control: public, max-age=86400` when the
`?v` matched the current `getFilesystemHash()` and `no-cache` otherwise. The two
halves were coupled: the chunked shell rewriter existed to feed `?v=` to the
sub-resource cache policy (the ADR-026 mechanism).

This `?v=`-immutable-URL scheme had one real virtue: **within a warm reload, the
sub-resources generated ZERO revalidation requests** (the URL was immutable for
the build, so `max-age=86400` let the browser skip the network entirely). That
matters because this device starves under parallel sub-resource load.
**bug-113** (`.wolf/buglog.json`; `#dev-sat-mqtt`, alpha.160): the OTGW32
single-threaded webserver starved when the browser fired ~8 local sub-resource
requests in parallel after `index.html`, and the `.css`/`.js` requests stalled
(PENDING, never completed) while the page document (200) and the external
echarts CDN succeeded. Same root as the documented XHR latency ramp and the
sat-slider stall (ADR-132 Context: SergeantD, `#dev-sat-mqtt`, TASK-817). So any
change that *increases* sub-resource request count is a regression on the exact
failure mode this device is known to hit.

The recorded justification for the chunked path (ADR-132 lines 137-140) was:
"never buffer the ~39 KB `index.html` whole on the fragmented S3 heap."
Measured, `index.html` is **39,791 bytes uncompressed**
(`src/OTGW-firmware/data/index.html`). The decisive observation is that this
heap guarantee does **not** require either buffering or compression:
`AsyncFileResponse` (`beginResponse(LittleFS, path)`) streams the file
incrementally. Its fill callback reads `len`-sized chunks straight into the
server's send buffer on demand:

```cpp
// .pio/libdeps/esp32-combo/ESPAsyncWebServer/src/WebResponses.cpp:817-819
size_t AsyncFileResponse::_fillBuffer(uint8_t *data, size_t len) {
  return _content.read(data, len);
}
```

`len` is bounded by the TCP window / send-buffer size (`WebResponses.cpp:453`,
`482`, `530`), so the whole file is never held in RAM. The hand-rolled chunked
filler reimplemented, fragilely, a guarantee the library already provides. gzip
was never needed for the "do not buffer 39 KB" concern.

Per maintainer directive, the LittleFS image holds **plain, readable** HTML / JS
/ CSS files, and the build phase produces **no archives or `.gz` siblings**. The
gzip build step (`prepare_gzip_assets`, `build.py:718-748`) stays disabled at its
call site (`build.py:755-761`, TASK-433); this ADR **affirms** that disable
rather than reversing it. (TASK-433 originally disabled gzip because shipping a
`.gz` while the handler also set `Content-Encoding: gzip` manually produced a
doubled header browsers rejected; with no `.gz` served, that failure mode is
out of scope entirely.)

Separately, our `platformio.ini` set **none** of the AsyncTCP task tuning flags.
The library ran at its defaults: `CONFIG_ASYNC_TCP_RUNNING_CORE = -1` (no core
affinity, the `async_tcp` task floats across cores) and a 16 KB stack. The
EMS-ESP32 project, the reference async + FreeRTOS implementation our AsyncTCP pin
is modeled on (`other-projects/EMS-ESP32-dev/`), pins all five:
`CONFIG_ASYNC_TCP_RUNNING_CORE=1`, `CONFIG_ASYNC_TCP_STACK_SIZE=6144`,
`CONFIG_ASYNC_TCP_QUEUE_SIZE=64`, `CONFIG_ASYNC_TCP_PRIORITY=10`,
`CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000` (`platformio.ini:38-42`). It serves its web
assets with `ETag` (MD5) + `Cache-Control: public,max-age=60`
(`ESP32React.cpp:7,44`), 304 on `If-None-Match`, no query-string versioning.

## Decision

**PRIMARY: ETag + bounded `max-age` is the project-wide cache-busting standard
for ALL web UI static assets, served as plain readable files.** `?v=<fsHash>`
query-string versioning is retired entirely, on both sides (the chunked shell
rewriter and `serveVersionedAsset`'s `?v=`-match logic). The concrete shape:

- **Stable plain paths.** Every web UI static asset (`index.html` and the 8
  JS/CSS sub-resources) is served from its plain path. No query versioning, no
  runtime HTML rewriting. The assets ship as plain, readable files on LittleFS;
  no gzip, no `.gz` siblings (maintainer directive; TASK-433 stays disabled).
- **ETag = filesystem hash.** The version token is the `ETag`, value =
  `getFilesystemHash()` (`helperStuff.ino:722`). One global hash means every
  asset's ETag flips together on reflash; that is correct because the LittleFS
  image ships atomically, so there is no need for independent per-asset
  versioning. (`getFilesystemHash()` is shared infrastructure reused from the
  ADR-026 era, not a surviving ADR-026 decision.)
- **`Cache-Control: public, max-age=N`, NOT `no-cache`.** The bounded `max-age`
  is the load-bearing design choice (see the trade-off below): within the window
  the browser serves from cache with **no request at all**; after it expires, a
  single cheap conditional GET returns `304` (ETag unchanged) or `200`
  (reflashed). On `If-None-Match` matching the current ETag the handler returns
  `304` with no body. Value **N = 60 s** (the EMS-ESP32 precedent).
- **Library-managed file streaming.** Assets are served with
  `beginResponse(LittleFS, path)` (`AsyncFileResponse`), which is incremental by
  construction (it reads `len`-sized pieces on demand, `WebResponses.cpp:817-819`)
  and so keeps ADR-132's "never buffer the whole file on the fragmented S3 heap"
  guarantee **without** the hand-rolled `beginChunkedResponse` code and
  **without** gzip. Idiomatic = library-managed file streaming + ETag.

This is implemented as of TASK-865.9 follow-up work:

- `serveVersionedAsset` (`FSexplorer.ino`) rewritten to ETag + `max-age=60`,
  plain file only, no `.gz` branch; a `LittleFS.exists()` -> 404 guard precedes
  the send.
- `sendIndex` (`FSexplorer.ino`) is now a thin handler:
  `webBeginRequest(request)` -> `checkHttpAuth()` ->
  `serveVersionedAsset("/index.html", ...)`. The `IndexEmitState` struct and the
  chunked `beginChunkedResponse` rewriter are deleted.
- `webSendFile` (`webServerCompat.h`) gained a `LittleFS.exists()` -> 404 guard
  before `beginResponse(LittleFS, path)` (the related-hardening note below, now
  done).

**Auth (ADR-056 preserved verbatim).** The shell stays a **thin custom handler**
that calls `checkHttpAuth()` upfront before streaming the file, **not** a bare
`server.serveStatic` registration. The upfront auth makes the browser cache
Basic-Auth credentials on the first asset (the HTML) before any `/api` call,
avoiding a mid-session credential popup. `server.serveStatic` has no auth hook,
so the shell needs the thin handler. The JS/CSS sub-resources stay
unauthenticated, exactly as before.

**SECONDARY (blueprint alignment, not the fix).** The AsyncTCP task config is
pinned to match the blueprint: `-D CONFIG_ASYNC_TCP_RUNNING_CORE=1` is added to
the GLOBAL `[env]` `build_flags` (`platformio.ini:45`), so the `async_tcp` task
has deterministic core affinity (the Arduino app core) instead of the default
float-anywhere placement. The flag is in the global `[env]`, not `[env:esp32]`,
because the field hang was on `esp32-combo`, and `esp32-classic` / `esp32-combo`
rebuild their `build_flags` from `${env.build_flags}` (the global `[env]`); they
do NOT inherit additions made to `[env:esp32]` (documented at
`platformio.ini:98-102`). Putting the flag in `[env:esp32]` would make it a
no-op on the very target that hung. Post-ADR-128 there is no ESP8266 env, so the
global `[env]` is ESP32-only and the flag is safe there. This is
deterministic-placement hygiene; it is **not** claimed as the cause of the `/`
hang. The `curl` evidence (REST works, `/` hangs) points at the response path,
not at core affinity.

## Alternatives Considered

### Alternative A: Keep `?v=` query versioning and the chunked rewriter (status quo, ADR-132 + ADR-026)

Retain the chunked shell rewriter (`sendIndex` + `IndexEmitState`), the runtime
`?v=` injection, and `serveVersionedAsset`'s `?v=`-match `max-age=86400` policy.

Rejected. The chunked rewriter is the least idiomatic ESPAsyncWebServer choice
(a hand-rolled filler that rewrites bytes, used for a static file) and the
leading suspect for the `/` hang, running line-by-line flash reads and 8-token
string rewriting on the async service task. The heap fear it addressed is moot
because `AsyncFileResponse` streams incrementally
(`WebResponses.cpp:817-819`). The two halves are also coupled: the shell
rewriter exists only to feed `?v=` to the sub-resource cache policy, so dropping
the rewriter and keeping `?v=` is not coherent.

### Alternative B: Embed all assets as gzipped PROGMEM buffers like EMS-ESP32

Compile every asset into PROGMEM byte arrays (EMS-ESP32's `WWWData.h` via
`progmem-generator.js`) and serve with `beginResponse(200, type, buffer, len)` +
`Content-Encoding: gzip` + `ETag` + 304.

Rejected on two independent grounds. (1) It abandons our LittleFS-asset model:
the UI assets live on LittleFS and are user-updatable through FSexplorer, so
moving them into PROGMEM would split the asset model and remove
user-updatability. (2) It is gzipped, which is now ruled out by the maintainer
directive that the LittleFS image hold plain, readable files with no archives.
The purest-blueprint form is therefore doubly out.

### Alternative C: Bake immutable `?v=` URLs at build time

Have `build.py` emit an `index.html` whose sub-resource URLs already carry
`?v=<hash>`, then serve everything plainly with `Cache-Control: public,
max-age=<large>` on the immutable `?v` URLs.

Viable, and the only option that preserves the **zero-revalidation** property of
the old scheme (immutable URL + long `max-age` means warm reloads make no
sub-resource requests at all). Not chosen as the primary because it keeps the
build pipeline rewriting HTML and re-introduces query-string versioning the
maintainer directive sets out to retire project-wide. Recorded as the explicit
fallback if the bounded-`max-age` revalidation burst proves to re-trigger
bug-113 on hardware: the knob is then to raise `N`, or adopt this build-time
`?v=` scheme.

### Alternative D: Stable URLs + ETag + `no-cache` (no `max-age`)

Serve stable URLs with `ETag` + `Cache-Control: no-cache`, forcing a conditional
GET on every asset on every load.

Rejected: this is the regression the bounded `max-age` exists to avoid.
`no-cache` means all ~8 sub-resources fire a conditional GET on every page load,
maximizing parallel sub-resource requests on the single-task server that already
starves (bug-113). It would be strictly worse than the old `?v=` scheme (which
made zero revalidation requests on warm reloads). The standard is bounded
`max-age`, not `no-cache`.

### Alternative E: Do nothing (keep the chunked rewriter, accept the hang risk)

Rejected. The hang was observed in the field on a release build; the chunked
rewriter is a standing maintenance and correctness liability regardless of
whether it caused this specific hang, and the idiomatic replacement is simpler
and lighter on the async task.

## Consequences

**Benefits**

- One coherent, idiomatic cache-busting standard across the whole web UI: stable
  plain-file URLs + ETag (= `getFilesystemHash()`) + bounded `max-age`, matching
  the EMS-ESP32 blueprint. No two mechanisms, no runtime HTML rewriting, no
  query versioning.
- Removes the novel chunked code (`sendIndex` rewriter + `IndexEmitState`) that
  ran flash I/O and byte rewriting on the AsyncTCP task, plus the `?v=`-match
  branch in `serveVersionedAsset`. `sendIndex` is now a 3-line thin handler.
- The heap guarantee that motivated the chunked path is met by
  `AsyncFileResponse` streaming the 39,791-byte file incrementally
  (`WebResponses.cpp:817-819`), never buffering it whole, and without gzip.
- Plain readable files on LittleFS: assets stay user-inspectable and
  user-updatable via FSexplorer; the build phase ships no archives.
- Deterministic `async_tcp` core placement matching the blueprint.
- Likely removes the leading suspect for the observed `/` hang (to be confirmed
  on hardware, see Risks).

**Trade-offs**

- **Sub-resource caching: `max-age=N` gives more revalidation than the old
  immutable scheme, less than `no-cache`.** Old: `?v` + `max-age=86400` =
  immutable URL = zero revalidation for a day. New: stable URL + `max-age=N` =
  zero requests within the window, then **one bodiless 304 per asset** after it.
  For sub-resources that is *more* revalidation than `?v=` immutable URLs gave,
  appearing as a periodic ~8-asset 304 burst on the same single-task server that
  starves (bug-113). It is bounded and bodiless (not the unbounded `no-cache`
  case), and `N` is the knob: too small -> 304 bursts on the starving device;
  too large -> longer reflash staleness. `N = 60 s` (EMS precedent) is the
  starting point, to be confirmed on hardware. Alternative C (build-time `?v=`)
  is the fallback if the burst re-triggers bug-113.
- **Shell staleness window.** The shell moves from `no-cache` to `max-age=N`, so
  a freshly reflashed device can serve a stale `index.html` from the browser
  cache for up to `N` seconds. We traded "an immediate fresh fetch on every
  visit" for stable URLs with bounded staleness; with `N = 60 s` the window is
  small and self-clears (the ETag flips on reflash, so the first conditional GET
  after the window returns `200` with the fresh shell). This is the genuine cost
  of stable URLs over the old `no-cache` shell, and the bounded version of
  ADR-026's original Safari concern.

**Risks and mitigations**

- *Risk*: the periodic `max-age=N` 304 burst re-triggers the bug-113
  sub-resource starvation on the single-task server. *Mitigation*: start at the
  EMS-precedent `N = 60 s` and field-validate the 304 burst on the OTGW32
  hardware that starves; raise `N`, or fall back to Alternative C (build-time
  immutable `?v=` URLs), if the burst stalls sub-resources. Open until
  hardware-confirmed.
- *Risk*: the `/` hang has a root cause unrelated to the retired chunked
  rewriter (e.g. a deeper async-task stall), so retiring it does not fix it.
  *Mitigation*: the decision does not assert causation; field validation on
  ESP32-S3 with the curl-probe added to
  `scripts/capture-mqtt-debug.bat:547-563` (root page + `/api/v2/device/info`)
  is required before this ADR moves to Accepted. The ADR is Proposed pending
  that confirmation.
- *Risk*: a reflashed device serves a stale shell for up to `N` seconds.
  *Mitigation*: keep `N` small (60 s start); the ETag (= `getFilesystemHash()`)
  flips on reflash, so the first conditional GET after the window returns `200`
  with the fresh shell and the cache self-clears.

## Related Decisions

- **ADR-132 (HTTP Stack on ESPAsyncWebServer with an Imperative-Push to
  Async-Pull Bridge)**: **amended by this ADR (body-size routing /
  static-asset cache-busting)**. This ADR generalizes ADR-132's `index.html`
  chunked-streaming sub-decision into a project-wide ETag + `max-age` standard;
  the bridge, send-once invariant, and REST `AsyncResponseStream` path are
  unchanged. ADR-132 stays immutable (Accepted).
- **ADR-026 (Conditional JavaScript Cache-Busting for Firmware/Filesystem
  Version Mismatches)**: **superseded by this ADR.** ADR-026's entire decision
  was the `?v=<fsHash>` conditional cache-busting, which is retired here in
  favour of ETag + bounded `max-age`. ADR-026's goal (JS reloads after a
  filesystem flash; efficient caching when versions match; Safari stale-cache
  handling) is met by the new mechanism, with the Safari window bounded to 60 s.
  ADR-026 stays immutable (Accepted); its body is not edited.
- **ADR-123 (2.0.0 Concurrency Model / Async Modernization)**: master decision;
  ADR-132 is its Phase 3 (web) and this ADR refines that phase. The AsyncTCP
  core-affinity flag tunes the single-task serialization that the whole phase
  rests on. (Disambiguated by title: there are two files claiming ADR-123 in
  `docs/adr/`; this references the concurrency-model one.)
- **ADR-056 (Protected admin endpoint security contract)**: preserved verbatim.
  The shell keeps its thin custom handler with the upfront `checkHttpAuth()`
  guard; the JS/CSS assets stay unauthenticated; no Basic-Auth / CSRF behaviour
  changes.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the single-target rationale. It makes
  the global `[env]` ESP32-only, so the AsyncTCP flag is safe to place there, and
  it is the reason the async stack is the firmware's only HTTP server.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is labeled
  guideline-level per ADR-080 because no automated gate is planned.

## References

- ADR-132, "Body-size routing" section, lines 137-140 (the amended sub-decision).
- ADR-026 (`docs/adr/ADR-026-conditional-javascript-cache-busting.md`): the
  `?v=<fsHash>` conditional cache-busting mechanism this ADR supersedes.
- Task: epic TASK-865 (2.0.0 ESP32-S3 async migration); TASK-866 (the `/`-hang
  diagnosis task). TASK-433 (the gzip disable this ADR affirms).
- bug-113 (`.wolf/buglog.json`): OTGW32 sub-resource starvation / sat-slider
  stall (alpha.160) - ~8 parallel local `.js`/`.css` requests stall on the
  single-task server. The request-count regression this ADR's bounded `max-age`
  guards against. (Documented field context: ADR-132 Context, sat-slider stall /
  XHR latency ramp, SergeantD `#dev-sat-mqtt`, TASK-817.)
- `src/OTGW-firmware/FSexplorer.ino`: `serveVersionedAsset` (ETag + `max-age=60`,
  plain file, `LittleFS.exists()` 404 guard) and `sendIndex` (thin handler:
  `webBeginRequest` -> `checkHttpAuth` -> `serveVersionedAsset`). The former
  `IndexEmitState` struct and chunked `beginChunkedResponse` rewriter, and
  `serveVersionedAsset`'s former `?v=`-match logic, are deleted.
- `src/OTGW-firmware/helperStuff.ino:722`: `getFilesystemHash()` (the ETag value
  source).
- `src/OTGW-firmware/webServerCompat.h`: `webSendFile` with the
  `LittleFS.exists()` -> 404 guard before `beginResponse(LittleFS, path)`.
- `.pio/libdeps/esp32-combo/ESPAsyncWebServer/src/WebResponses.cpp:817-819`:
  `AsyncFileResponse::_fillBuffer` reads `len`-sized chunks on demand (`len`
  bounded by the TCP send buffer, lines 453/482/530), so the file is streamed
  incrementally and never buffered whole.
- `build.py:718-748`: `prepare_gzip_assets`; `build.py:755-761`: its disabled
  call site (TASK-433), which this ADR affirms (no gzip, plain readable files).
- `src/OTGW-firmware/data/index.html`: 39,791 bytes uncompressed (measured),
  served as a plain readable file.
- `platformio.ini:45`: global `[env].build_flags` now carrying
  `-DCONFIG_ASYNC_TCP_RUNNING_CORE=1` (line 52). `platformio.ini:98-102`: the
  build_flags-rebuild note proving combo/classic do not inherit `[env:esp32]`
  additions, so the flag belongs in the global `[env]`.
- `other-projects/EMS-ESP32-dev/src/ESP32React/ESP32React.cpp:7,34-44`: the
  blueprint serving pattern (ETag MD5 + `Cache-Control: public,max-age=60` + 304
  on `If-None-Match`, no query versioning).
- `other-projects/EMS-ESP32-dev/platformio.ini:38-42`: the five
  `CONFIG_ASYNC_TCP_*` blueprint pins.
- `scripts/capture-mqtt-debug.bat:547-563`: the curl root-page +
  `/api/v2/device/info` probe used to confirm the hang root cause.

This ADR has a code surface but is enforced at PR review, not by an automated
gate: there is no clean forbid/require pattern for "all static assets are served
as plain files from stable paths with ETag + bounded max-age" (the absence of
`sendIndex`'s chunked rewriter / `IndexEmitState` and of `?v` query handling is
the only mechanically checkable signal, and ADR-132's existing `httpServer.`
gate already covers the async-API discipline). It is therefore labeled
guideline-level per ADR-080, and no `## Enforcement` block is added.
