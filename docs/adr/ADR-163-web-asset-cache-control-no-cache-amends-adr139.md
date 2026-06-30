# ADR-163 Web UI Static Assets Use Cache-Control: no-cache (Always-Revalidate via ETag); Amends ADR-139's Bounded max-age

## Status

Accepted. Date: 2026-06-30.

**Decision Maker:** User: Robert (approved the no-cache redirect after the evidence-based re-diagnosis of TASK-958).

## Status History

status_history:
  - date: 2026-06-30
    status: Proposed
    changed_by: Agent
    reason: Re-diagnosis of TASK-958 ("OTA filesystem update succeeds but serves old assets") on bench hardware proved the OTA write is correct; the stale UI is the ADR-139 max-age=60 cache window. Propose amending the Cache-Control value to no-cache.
    changed_via: adr-kit
  - date: 2026-06-30
    status: Accepted
    changed_by: User
    reason: Maintainer approved the no-cache redirect (revised TASK-958 plan). Hardware-verified on OTGW32 @192.168.88.39 - warm 304 revalidations are ungated and a changed FS returns the new asset immediately. ADR-139 gains "Amended by ADR-163" for its cache-control value only.
    changed_via: adr-kit

## Context

ADR-139 made `ETag` (= filesystem hash) + `Cache-Control: public, max-age=60` the project-wide cache-busting standard for Web UI static assets (`serveVersionedAsset()` in `FSexplorer.ino`). It deliberately chose `max-age=60` over `no-cache`, reasoning that always-revalidate would impose a per-load conditional-GET burst on the then single-connection server (`bug-113`, the OTGW32 `/`-hang).

TASK-958 reported "an OTA filesystem update succeeds but the web UI still shows the old assets." Bench investigation on OTGW32 `@192.168.88.39` disproved the assumed cause (partition/size/partial-write): a complete OTA fs upload correctly REPLACES the filesystem — the served `version.hash` flipped `a46e95a -> 03591d5`, the served `ETag` updated, and telnet `checklittlefshash` logged the new FS githash. The real cause is the `max-age=60` window: after the OTA the device immediately serves the new assets with a new ETag, but the frontend's post-reboot reload (`window.location.href='/'`) lets the browser serve its cached copy for up to 60 s WITHOUT revalidating — `max-age` blinds the browser to the ETag exactly when it matters. The user sees old assets until a hard-refresh or 60 s elapse.

Two constraints from ADR-139's reasoning had to be re-checked before reversing it:
- The single-connection concern is obsolete: the 2.0.0 stack is async (ESPAsyncWebServer/AsyncTCP, concurrent). A 30-minute soak this session served 41,113 REST requests at 0% error.
- The file-serve gate (ADR-147 `webFileGateTryAdmit`, in `webSendFile`) throttles concurrent FULL file serves (200) under heap pressure, returning a cheap 503. It does NOT gate the 304 path (`webSendStatus(304)`), which is headers-only and does no LittleFS work.

## Decision

Serve Web UI static assets with `Cache-Control: no-cache` (the browser may store but MUST revalidate via `If-None-Match` before every use), keeping the existing `ETag = getFilesystemHash()`; remove the `public, max-age=60` value. Everything else in ADR-139 (ETag standard, stable URLs / no `?v=` versioning, AsyncFileResponse streaming, retired chunked rewriter, AsyncTCP task config) is unchanged. This ADR amends only ADR-139's Cache-Control value.

Implemented in `FSexplorer.ino` `serveVersionedAsset()` (the 200 path at line ~101 and the 304 path at line ~94). TASK-958 also adds two defensive fixes outside the cache surface: the post-OTA reload is cache-busted (`window.location.href='/?_ota='+Date.now()` in `updateServerHtml.h`), and `OTGW-ModUpdateServer-esp32.h` `_handleUploadEnd` now verifies `/version.hash` exists after remount so a truncated write that `Update.end(true)` finalized is surfaced as an error instead of a silent success.

## Alternatives Considered

### Alternative A: keep `max-age=60` (do nothing)

Leaves the user-reported bug: a successful OTA shows old assets for up to 60 s, and a same-recent-load reload reproduces it reliably. Rejected — it is the bug.

### Alternative B: version-pinned immutable URLs (`index.js?v=<fsHash>`, `max-age=immutable`)

Eliminates warm-load requests entirely and busts on fs change. Rejected for now: it reverses ADR-139's deliberate "no `?v=` query versioning" decision and requires build-time injection of the hash into every asset reference in `index.html`/`v2.html` (the kind of templating ADR-139 retired). Larger change; revisit if `no-cache` 304 traffic ever proves costly.

### Alternative C: raise the ADR-147 file-serve gate

Not needed: the 304 revalidation path is already ungated. Raising the gate would weaken the heap/PCB protection it exists for. Rejected.

## Consequences

**Benefits**

- A successful OTA filesystem update shows the new assets on the very next load — no stale window. Verified on hardware: a browser holding the old ETag `"a46e95a"` against a device now serving `03591d5` gets `HTTP 200` (fresh content) on revalidation, not a stale `304`.
- No per-load file-serve burst from this change: 8 concurrent warm revalidations with matching ETags returned 24/24 `304` with zero `503` (the 304 path is ungated by ADR-147). Unchanged assets still cost only a headers-only 304.

**Trade-offs**

- Every asset load now performs one conditional GET (`If-None-Match`) instead of serving blindly from cache for up to 60 s. On the trusted LAN a 304 is a few hundred bytes and sub-millisecond; negligible.

**Risks and mitigations**

- *Risk*: a COLD load (empty cache) or a post-OTA load fetches several full 200 bodies concurrently and can trip the ADR-147 gate (some 503). *Mitigation*: this is pre-existing (cold loads always fetched full bodies) and bounded by the browser's ~6-connection-per-host limit, which matches the gate's healthy-heap allowance; `no-cache` does not worsen it because warm revalidations are 304 (ungated). A separate follow-up may right-size the gate vs the classic UI's asset count.

## Related Decisions

- **ADR-139 (ETag + Bounded max-age Cache-Busting Standard)**: Amended by ADR-163 — only the `Cache-Control` value changes (`public, max-age=60` -> `no-cache`); ADR-139's ETag standard, stable URLs, no-`?v=`, and streaming model remain in force.
- **ADR-147 (File-serve admission gate)**: relied upon — the gate lives in `webSendFile` (200 path) and does not throttle the 304 revalidation path, which is what makes `no-cache` cheap here.
- **ADR-029 / ADR-134 (OTA flash path)**: unchanged — the OTA write was always correct; this ADR only fixes how the freshly written assets are revalidated by the browser.

## References

- TASK-958 (OTA fs update succeeds but serves old assets).
- `src/OTGW-firmware/FSexplorer.ino` `serveVersionedAsset()` (Cache-Control no-cache, 200 + 304 paths).
- `src/OTGW-firmware/updateServerHtml.h` (`redirectToHome` / `onTimeout` post-OTA reload cache-bust).
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h` `_handleUploadEnd` (post-write `/version.hash` verification).
- Hardware verification: OTGW32 @192.168.88.39 (alpha.297) — served `Cache-Control: no-cache` + `ETag`; matching-ETag burst 24/24 `304`; stale-ETag -> `200`.

## Enforcement

```json
{
  "version": 1,
  "rules": [
    {
      "id": "asset-cache-no-cache",
      "type": "forbid_pattern",
      "glob": "src/OTGW-firmware/FSexplorer.ino",
      "pattern": "Cache-Control\"\\), F\\(\"public, max-age",
      "message": "ADR-163: Web UI static assets must use Cache-Control: no-cache (always-revalidate via ETag), not public, max-age. See serveVersionedAsset()."
    }
  ]
}
```
