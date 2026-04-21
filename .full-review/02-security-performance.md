# Phase 2: Security & Performance Review

**Target**: branch `1.4.1` vs `dev` (14 commits, ~20 source files)
**Threat model**: LAN-only ESP8266 behind home router, NAT-isolated from internet. Trust boundary = MQTT broker.
**Full reports**: `phase2a-security.md` (18 KB) · `phase2b-performance.md` (28 KB)

## Aggregate severity counts

| Severity | Security (2A) | Performance (2B) | Total |
|---|---|---|---|
| Critical | 0 | 1 | **1** |
| High | 0 | 2 | **2** |
| Medium | 2 | 3 | **5** |
| Low | 3 | 3 | **6** |
| Informational | 2 | — | **2** |

---

## The big three (must-address before merge)

### CRITICAL (Performance) — `sendMQTTheapdiag` JSON buffer overflow

- **Location**: `src/OTGW-firmware/MQTTstuff.ino` `sendMQTTheapdiag`, `char json[384]`
- **Math verified**: 17 fields, worst-case serialization = **465 bytes + NUL = 466 bytes** vs 384 allocated = **81-byte truncation**
- **Impact**: `snprintf_P` silently truncates, producing malformed JSON that is then published as a **retained** MQTT message. The corrupt message stays on the broker until the next hourly publish.
- **Trigger**: any counter reaching its upper range. `%lu` can be 10 digits; on a system under chronic heap pressure accumulating `mqtt_drops` over days/weeks, truncation becomes inevitable.
- **Fix** (one line):
  ```cpp
  char json[512];   // was 384: worst-case is 465 bytes + NUL
  ```
- **Note**: Phase 1B had flagged this as MEDIUM; Phase 2B promotes to CRITICAL because the math is now rigorous (not "might overflow", but "will overflow whenever any counter saturates").

### HIGH (Performance) — `STATUS_BURST_COOLDOWN_MS = 10000` permanently defers discovery drip

- **Location**: `src/OTGW-firmware/MQTTstuff.ino` `STATUS_BURST_COOLDOWN_MS` constant
- **Math**: Crashevans log shows ~3s Status cadence. Every `endStatusBurst()` resets `burstCooldownUntilMs = now + 10000`. Next burst arrives at t+3, renewing the cooldown. Gap between bursts (3s) < cooldown (10s) → **drip never gets a tick**.
- **User-visible consequence**: first-boot HA integration with any boiler at normal Status cadence → discovery drip never completes. HA sees no entities until the user manually POSTs `/api/v2/discovery/republish`. And if HIGH-2 below isn't also fixed, that won't help either.
- **The code comment near the constant acknowledges this problem** and suggests "candidates: 2500ms = fits between bursts, 5000ms = partial overlap" — but ships 10000ms as the default.
- **Fix** (one line, matches own comment):
  ```cpp
  constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 2000;  // was 10000; stays under 3s Status cadence
  ```

### HIGH (Performance + Phase 1A confirmation) — VH publishers bypass the burst quiesce

- **Location**: `src/OTGW-firmware/OTGW-Core.ino:1667-1733` (`publishMasterStatusVHState`, `publishSlaveStatusVHState`, `publishStatusVHBitMQTT`)
- **Issue**: Non-VH Status publishers correctly wrap themselves in `beginStatusBurst()`/`endStatusBurst()`. VH (ventilation) publishers do not. On VH-equipped boilers, the drip runs freely during AND immediately after VH Status fanouts — worse than the heater-only case where the drip is at least deferred during the burst itself.
- **Already covered by Phase 1A HIGH #1**. Phase 2B quantified the impact: drip never gets a gap, worst case for users of VH hardware.
- **Fix**: Same as Phase 1A recommendation — mirror the non-VH wrapping pattern.

---

## Phase 2A: Security — LAN-only calibrated

### Threat model (explicit, to calibrate reviewers)

ESP8266 on a home LAN behind a router (NAT-isolated, no port-forwarding). Design mandate: HTTP/WS only, no TLS. Realistic threat actors: (1) the MQTT broker (legitimate trust boundary), (2) an already-compromised LAN device (post-compromise, capped at Medium), (3) user misclicking (UX, not security). **Internet attackers = out of scope**. Plaintext + unauth-by-design are **not findings**.

### MEDIUM (2)

1. **Rapid-fire `/api/v2/discovery/republish` can lock out verify** — `restAPI.ino:512-524`. Post-auth LAN attacker loops POST; `countPendingDiscoveryIds() > 0` stays true permanently, which blocks `startDiscoveryVerification()`. CWE-770. **Fix**: 60-second cooldown timer on the endpoint (5 lines). Alternative: 409 when pending IDs > 0.
2. **Verify-window callback fall-through** — `MQTTstuff.ino:629-656`. When a broker publishes on `<haprefix>/<something>` without the expected 3-segment structure, the filter falls through to the command dispatcher below. A crafted retained topic under the prefix can sneak to the command path during the 15s window. CWE-20. **Fix**: make the `return` unconditional once the haprefix match succeeds, regardless of substructure parse (~10-line restructure).

### LOW (3) + Informational (2)

- `iVerifyRunCount` increments at start, not on clean close — inflates under broker flaps.
- `handleDebugChar('V')` is telnet-unauth — by-design surface, flagged for completeness only.
- `6000` magic number duplicated in REST endpoint (same as Phase 1A HIGH #3).
- INFO: Hostile broker flood of the verify window is **self-terminating** (bounded by 1024-byte reused buffer + 4500 heap-abort + `verifyReceivedCount >= expected` early-close).
- INFO: New POST endpoints inherit `checkHttpAuth()` — consistent with existing v2 routes.

### Surfaces verified clean (11 dimensions)

Auth consistency, CSRF, MQTT topic NUL-termination (PubSubClient guarantees it at `libraries/PubSubClient/src/PubSubClient.cpp:399` — Phase 1A LOW finding **downgraded**), PROGMEM pointer safety, stack buffer sizes, secrets in logs, supply chain (no dependency changes), integer wraparound, re-entrancy of `handleMQTTcallback`, Status-burst flag 500ms self-heal, OWASP Top 10 (scoped out — not applicable to LAN-only ESP8266).

### Security merge-readiness

No Critical, no High. Both Mediums are realistic-impact Low but touch the broker trust boundary. Both have cheap fixes (< 15 lines total). **Security alone does not block merge**, but both Mediums are recommended before merge.

---

## Phase 2B: Performance — primary claim validated

### Validation of the branch's primary claim

**Claim**: heap pressure is reduced during HA discovery drip.
**Verdict**: TRUE for the common case, FALSE for three concrete conditions (HIGH-1, HIGH-2, and for systems with chronic chatter around tier boundaries per Phase 1A MED on hysteresis).

### Key metric deltas (vs `dev`)

| Parameter | dev | 1.4.1 | Delta |
|---|---|---|---|
| `HEAP_CRITICAL_THRESHOLD` | 2048 | 1536 | −512 B (−25 %) |
| `HEAP_WARNING_THRESHOLD` | 4096 | 3072 | −1024 B (−25 %) |
| `HEAP_LOW_THRESHOLD` | 6144 | 5120 | −1024 B (−17 %) |
| `DISCOVERY_INTERVAL_NORMAL` | 1s | 2s | +100 % |
| `DISCOVERY_INTERVAL_SLOW` | 30s | 10s | −67 % |
| Slow-mode trigger tier | HEAP_WARNING | HEAP_LOW | fires earlier |
| `STATUS_BURST_COOLDOWN_MS` | — | 10000 ms | NEW (**problematic default, HIGH-1**) |
| `VERIFICATION_WINDOW_MS` | — | 15000 ms | NEW |
| New static RAM (BSS) | 0 | ~222 B | +222 B |
| `OTGWState` new sections | 0 | 60 B | DiscoverySection 24 + HeapDiagSection 36 |

### Heap envelope — worst-case analysis

Starting at `freeHeap = 6000` (minimum for verify start), worst-case concurrent path:

| Allocation | Bytes | Cumulative free |
|---|---|---|
| Start | 0 | 6000 |
| `setBufferSize(1024)` realloc net | +640 | 5360 |
| Status-burst TX pbufs (9 publishes × ~90B) | +800 | 4560 |
| TCP RX pbuf (retained flood) | +1460 | 3100 |
| Callback stack frame | +200 | **2900** |

At 2900 bytes: above `HEAP_CRITICAL = 1536` (margin 1364). Already inside `HEAP_WARNING` band. `VERIFICATION_MIN_HEAP_ABORT = 4500` fires at 3100 → heap-abort triggered → **MEDIUM-1 false-negative** (Phase 2B) fires.

VH case adds ~600 B more TX pbufs → margin compresses to 764 B above CRITICAL. Still safe, but tight on systems with existing fragmentation.

### MEDIUM findings (3)

1. **Heap-abort during verify creates false-negative** — `tickDiscoveryVerification` at line ~315 sets `verifyReceivedCount = expected` to avoid "missing" republish, which also suppresses genuine missing-config detection under pressure. Phase 1A MED already flagged this with enum-class fix suggestion.
2. **`getHeapHealth()` transition counter inflation** — no hysteresis. Field counter `iEnteredLowCount` can reach ~1440/day from boundary chatter alone. Telemetry accuracy issue, not performance.
3. **`VERIFICATION_MIN_HEAP_START = 6000` may be insufficient** when WebSocket client + concurrent `sendDeviceInfoV2` HTTP chunk-stream is active. Two-concurrent-client scenario realistic at boot-time when user opens UI.

### LOW findings (3)

1. Slow-mode drip completion = 13 minutes (80 IDs × 10s) — user-visible during broker reconnects. No code change needed; document worst-case.
2. `getHeapHealth()` called twice per gated publish path — 12 µs per drip tick, negligible.
3. Verify window 15s may be insufficient for slow brokers with large retained backlogs — consider configurable window.

### CPU cost summary

- `ZonedDateTime` refactor: **net improvement** — nightly-restart reduces from 60 calls/hour (old minute-poll) to 1 call/hour (hourChanged-gated). Phase 1A MED on "4 allocations/min" is the per-minute cost, which is ~600 µs total = 10 µs/sec = negligible at 80 MHz.
- `markAllMQTTConfigPending()`: 25-125 µs; no O(N²). Not a performance concern.
- `handleMQTTcallback` verify-filter: ~0.3 µs/msg, bounded by `VERIFICATION_MAX_NODE_SEGMENT_LEN = 64` strncmp cap. DoS-bounded.
- `sendMQTTheapdiag`: hourly, cost irrelevant (but **see CRITICAL on buffer size**).
- `getHeapHealth()`: ~0.1 µs healthy, ~6 µs LOW/WARNING (includes `getMaxFreeBlockSize()` walk).

### Scalability

- **VH boilers**: HIGH-2 unresolved → heap-reduction claim fails for all VH users.
- **Multi-month uptime**: daily verify realloc 1024→384 → umm_malloc coalesces cleanly. `getMaxFreeBlockSize < 1280` precheck at start refuses verify on fragmented heap → silent skip, not crash. Accumulating skips visible in `disc_verify_runs`.
- **Slow brokers**: LOW-3 unresolved.
- **Rapid-fire `/republish` amplification**: second-to-tenth POSTs are no-ops on the bitmap (all bits already set). No amplification concern. (The **deferred-verify** concern from Phase 2A MED-1 is orthogonal — different symptom.)

---

## Consolidated findings cross-referenced across all phases (1+2)

### Confirmed by multiple reviewers (priority for merge)
- **VH publishers missing burst quiesce** — Phase 1A HIGH #1 + Phase 2B HIGH-2 (quantified impact)
- **`sendMQTTheapdiag` buffer too small** — Phase 1B MED → Phase 2B **CRITICAL** (math rigour added)
- **Hard-coded `6000` in REST** — Phase 1A HIGH #3 + Phase 2A LOW (security angle on source-of-truth drift)
- **Verify heap-abort masks failure** — Phase 1A MED + Phase 2B MED-1 (both agree fix needs outcome enum)

### Raised only in Phase 2
- **CRITICAL**: `sendMQTTheapdiag` buffer math (Phase 2B)
- **HIGH**: `STATUS_BURST_COOLDOWN_MS = 10000` permanently defers drip (Phase 2B)
- **MEDIUM**: `/republish` rate-limiting missing (Phase 2A)
- **MEDIUM**: Verify-callback fall-through on non-structured haprefix topics (Phase 2A)

### Phase 1 LOW downgraded in Phase 2
- **Verify callback topic NUL-termination** — PubSubClient library guarantees it; Phase 2A verified this against `libraries/PubSubClient/src/PubSubClient.cpp:399`. Finding removed.

---

## Merge-readiness snapshot (through Phase 2)

**Must fix before merge:**
1. `sendMQTTheapdiag` buffer → 512 bytes (Phase 2B CRITICAL; one line)
2. `STATUS_BURST_COOLDOWN_MS` → 2000 ms (Phase 2B HIGH-1; one line)
3. VH publishers burst-quiesce wrapping (Phase 1A HIGH + Phase 2B HIGH-2)
4. ADR governance: flip `Accepted → Proposed` until user signs off (Phase 1B HIGH)
5. ADRs cite non-existent ADR-077/078/080 (Phase 1B CRITICAL)

**Strongly recommended before merge:**
6. `/republish` cooldown / 409 (Phase 2A MED-1)
7. Verify callback fall-through guard (Phase 2A MED-2)
8. False comment on `startDiscoveryVerification` preconditions (Phase 1A HIGH #2)
9. Stale hourly-publish location comment (Phase 1A HIGH #4)

**Follow-up / post-merge:**
- Dead-code cleanup (14 items from Phase 1A)
- MQTTstuff.ino extraction (Phase 1B HIGH)
- ADR-062 CI gates (Phase 1B HIGH)
- ADR-051 struct migration for straggler statics (cross-phase, tech-debt)
