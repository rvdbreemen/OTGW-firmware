# Phase 1: Code Quality & Architecture Review

**Target**: branch `1.4.1` vs `dev` (14 commits, ~20 source files)
**Themes**: heap-pressure reduction, cumulative heap diagnostics, MQTT discovery verification, time-boundary dispatcher refactor
**Full reports**: `phase1a-code-quality.md` (40 KB) · `phase1b-architecture.md` (26 KB)

## Aggregate severity counts

| Severity | Code Quality (1A) | Architecture (1B) | Total |
|---|---|---|---|
| Critical | 0 | 1 | **1** |
| High | 4 | 3 | **7** |
| Medium | 9 | 5 | **14** |
| Low | 7 | 4 | **11** |
| Dead code | 14 | — | **14** |

Overall: branch is in solid shape. No crash-class code issues. The single Critical is an **ADR integrity** problem (not a code-level coupling problem). The HIGH findings cluster around three roots: (a) the Status-burst quiesce isn't applied to VH publishers, (b) comments have drifted from the refactor, (c) the ADR governance process wasn't followed cleanly.

---

## 1A: Code Quality Highlights

### HIGH (4)
1. **VH ventilation publishers bypass the Status-burst quiesce** — `OTGW-Core.ino:1688-1732`. The whole point of TASK-342/347 is to suppress the drip during the 9-publish Status fanout; VH boilers get the pre-refactor behaviour because `beginStatusBurst`/`endStatusBurst` and `incrementStatusBurstPublishCount()` are missing in `publishMasterStatusVHState`, `publishSlaveStatusVHState`, and `publishStatusVHBitMQTT`. Functional gap, not cosmetic.
2. **False comment on `startDiscoveryVerification` preconditions** — `OTGW-firmware.ino:316-319`. Comment claims NTP + uptime>3600 are enforced; they aren't. Either add the guards or rewrite the comment. (First option preferred — matches stated contract.)
3. **REST `/verify` hard-codes `6000` instead of referencing `VERIFICATION_MIN_HEAP_START`** — `restAPI.ino:499`. Two sources of truth; UX also duplicates guards that already live inside `startDiscoveryVerification()`.
4. **Stale comment on hourly heap-diag publish path** — `MQTTstuff.ino:1071-1074` still says "called from `doTaskEvery60s`", but after ADR-064 it's `doTaskMinuteChanged` under `hourFlag`. Trap for the next maintainer.

### MEDIUM (9, summarised)
- `getHeapHealth()` tier-transition counters inflate on boundary chatter (no hysteresis).
- `sendMQTTheapdiag` writes `iLastPublishedEpoch` before publish (and the publish can silently drop).
- `MQTT_DISCOVERY_HEAP_MIN = 3000` comment claims alignment with `HEAP_WARNING_THRESHOLD = 3072` — they're 72 bytes apart.
- Four `ZonedDateTime` allocations per minute in the dispatcher (refactor moved them, didn't consolidate).
- Verify-window heap-abort masks failure as a clean pass (`verifyReceivedCount = expected`).
- `handleDiscovery` GET endpoint uses a 320-byte stack buffer for 9 vararg `snprintf_P` with no truncation check.
- Discovery verify logs use raw `DebugTln` instead of `MQTTDebugTln` (runtime-gating inconsistency).
- Redundant `isStatusBurstActive()` double-test in `loopMQTTDiscovery`.
- `runNightlyRestartCheck` lost the `minute() == 0` safety guard during refactor.

### LOW (7, summarised)
- Stale "3s interval" drip comment.
- Non-existent ADR-077 referenced in code.
- `(void)yearFlag;` after `sendtimecommand(dayFlag, yearFlag)` — no-op, variable is already read.
- Status-burst cooldown 10s can permanently defer drip under ~3s Status cadence (no adaptive backoff).
- Verify callback topic filter has no bounds check on NUL-termination.
- `MQTT_DISCOVERY_HEAP_MIN` gate is redundant with `getHeapHealth()` / adaptive interval.

### Dead Code & Cleanup Candidates (14)
**High severity (clearly obsolete):**
- `state.discovery.bVerificationActive` — written 3×, never read. Single source of truth is `isDiscoveryVerificationActive()`.
- `state.heapdiag.iLastPublishedEpoch` — written on publish, never consumed anywhere.
- `endDiscoveryVerification` and `tickDiscoveryVerification` in public header but only used in same TU. Make `static`.

**Medium severity:**
- `isDripDeferred()` in public header but called once in own TU.
- `(void)yearFlag;` dead cast (see LOW finding above).
- Stale comments on `sendMQTTheapdiag` location and ADR-077 reference.
- REST handler duplicates 4 preconditions already in `startDiscoveryVerification()`.
- Pre-existing: `setMQTTConfigPending` iterates full 256-bit namespace when ~50 IDs are actually in use (latent; this branch added more iterators).
- Pre-existing: `mqttAutoConfigInProgress` + `MQTTAutoConfigSessionLock` = two symbols for one job.

**Low severity:**
- Four `// ADR-064: single caller` comment repeats where one block header suffices.
- `HEAP_FRAG_PROMOTE_MAXBLOCK` is `#define` where the rest of the branch uses `constexpr`.
- "Log heap statistics every minute" comment rot.

---

## 1B: Architecture Highlights

### ADR-062 Assessment (retained discovery verification) — strong
- Concrete context with 5 named Gap-A failure modes + separate Gap-B observability argument.
- Tuned-parameter table with per-row rationale (exemplary).
- Honest trade-off section: coarse per-reset-cycle tracking rejected with cost argument.
- Orphan non-deletion rationale has a real safety argument.
- Preconditions match code 1:1 at `MQTTstuff.ino:217-226`.

Weaknesses: missing companion ADR references (see CRITICAL below); stream-helper binding list omits `streamDallasSensorDiscovery`; boot-time `dayChanged()==true` interaction with ADR-064 not addressed.

**Verdict**: Accept with minor edits.

### ADR-064 Assessment (time-boundary single-caller contract) — strong
- Call-site census table IS the argument for the rule (correct form).
- Silent-failure characterisation is honest.
- Alternatives-considered is substantively better than average in this repo.
- CI gate is actually implemented and resilient to comment-line false positives.

Weaknesses: "exactly one call site" is slightly overstated given the comment-line skip logic; boot-time "every helper fires true" behaviour not documented in Consequences; `(void)yearFlag;` anti-pattern; plan-file path leak.

**Verdict**: Accept with minor edits.

### CRITICAL (1)
**New ADRs reference non-existent companion ADRs ADR-077, ADR-078, ADR-080.**
`ls docs/adr | grep -E "07[78]|080"` returns zero matches; highest pre-existing ADR is ADR-061. This undermines the ADR system's traceability contract. Three knock-on problems:
1. Readers cannot verify the claimed companion decisions.
2. Future PRs propagate the ghost citations.
3. One of the two CI gates ADR-062 promises (`check_discovery_counter_instrumented`, `check_publishedtopic_counter_reset`) is **not implemented**, which matches the pattern of a loose obligation rather than enforced policy.

**Fix**: rewrite "Related" sections to cite only existing ADRs (e.g., ADR-050 "centralized API route dispatch" in place of ADR-078), or write the missing ADRs before merge. Under no circumstance land with vapour citations.

### HIGH (3)
1. **Status field says "Accepted" but should be "Proposed"** — per CLAUDE.md, ADRs only become Accepted after explicit user sign-off. Both files carry `Status: Accepted` on disk; the scope lists them as Proposed. With other findings still requiring edits, Accepted would freeze them. Revert to Proposed unless approval is confirmed.
2. **ADR-062 binding rule claims 2 CI gates; only ADR-064's is implemented.** `check_discovery_counter_instrumented` and `check_publishedtopic_counter_reset` are not in `evaluate.py`. Unenforced rule = silent-bleed bug class: a future `streamSomeNewDiscovery` missing `incPublishedTopicCount()` causes daily false-missing republishes of all 80 topics.
3. **God-object creep in `MQTTstuff.ino`** — +379 lines on this branch; now hosts 5 state machines (publish, drip, Status-burst, discovery-verify, heap-diag). Re-entrancy contract between `doBackgroundTasks`, `handleMQTTcallback`, `tickDiscoveryVerification`, `loopMQTTDiscovery` is held together by author discipline. This is the same smell pattern as the ADR-040 stack overflow. Extract discovery-verify to `mqtt_discovery_verify.cpp/h` (state is already file-local statics; external symbols are a handful).

### MEDIUM (5)
- `mqtt_configuratie.cpp` stream-helper enumeration in ADR-062 omits `streamDallasSensorDiscovery`.
- `sendMQTTheapdiag` JSON buffer 384 bytes under worst-case saturation of 17 counters ≈ 464 bytes — precedent-breaking with the 1616-byte stack crash lesson.
- Boot-time first-minute dispatch fires all hourly/daily tasks at once (sentinels are `-1`); works because inner preconditions hold, but not documented.
- Discovery wildcard subscription crosses into PubSubClient internals via shared `handleMQTTcallback` — couples command dispatcher with verify counter.
- Plan-file path `C:\Users\rvdbr\.claude\plans\...` leaks local filesystem into repo.

### LOW (4)
- REST `/api/v2/discovery` duplicates precondition logic against start function.
- `sendMQTTheapdiag` topic double-brands "OTGW/otgw-firmware/stats/heap" when `sTopTopic` default prepends.
- `DiscoverySection` "3 bytes padding" comment suggests load-bearing padding where none exists.
- `incPublishedTopicCount()` shim pattern works well — flagged as positive pattern confirmation, no action.

---

## Critical issues for Phase 2 (Security & Performance)

Consolidated from both reviews, de-duplicated:

1. **Discovery verify DoS surface** — `setBufferSize(1024)` realloc + 1024-byte RX + 15s window. Hostile broker could flood `<haprefix>/+/<shortNodeId>/#`. Quantify sustainable rate vs `getFreeHeap() < 4500` abort. Also: weekly-verify × multi-week uptime: fragmentation accumulation?
2. **REST `/api/v2/discovery/republish` amplification** — one POST triggers 80 publishes over 2 min. Verify rapid-fire POST cannot DoS the drip.
3. **Heap threshold tuning risk** — `HEAP_LOW` 6144→5120, `HEAP_WARNING` 4096→3072, combined with verify `MIN_HEAP_START=6000` / `MIN_HEAP_ABORT=4500`. Model worst-case concurrent-allocation (Status-burst + WS client + drip + verify window) vs `HEAP_CRITICAL=1536`.
4. **`isStatusBurstActive()` 500ms auto-clear** — audit OTGW-Core.ino:1563+ for early returns after `beginStatusBurst` that skip `endStatusBurst` (esp. exception/flash-operation paths).
5. **VH Status-burst gap** (1A HIGH #1) — confirm heap-pressure reduction claim also holds for VH-equipped boilers.
6. **`getHeapHealth()` transition counter inflation** (1A MED) — Phase 2 telemetry interpretation will mislead; field reports inflate.
7. **NTP `ZonedDateTime` allocation cost per minute × 4** — measure CPU footprint; ADR-064's consolidation goal not fully realised.
8. **Verify callback topic filter** — no NUL-termination bound; `strchr` could walk malformed topic.
9. **`sendMQTTheapdiag` json[384] saturation** — add truncation check or bump to 512.
10. **`sendDeviceInfoV2` growth** — +15 JSON map entries this branch; verify cumulative size vs HTTP chunk-stream assumptions in frontend.
11. **First-minute boot dispatch** of `sendMQTTheapdiag` publishes retained JSON with zero counters — intentional or should wait for real hour boundary?

## Quick "merge readiness" assessment

Safe to merge **after**:
- Both ADRs flipped to Proposed until user approval.
- Companion ADR citations resolved (ADR-077/078/080 replaced or written).
- VH publishers wrapped in Status-burst quiesce (1A HIGH #1).
- Two stale/false comments fixed (1A HIGH #2 and #4).

Nice-to-have before merge (can also be follow-up tasks):
- REST `/verify` deduplication (1A HIGH #3).
- Dead-code cleanup (especially the 3 write-only state fields).
- Heap-diag CI gate implementation (1B HIGH #2).

Likely follow-up tasks (not blocking):
- `MQTTstuff.ino` extraction into `mqtt_discovery_verify.cpp` (1B HIGH #3).
- `ZonedDateTime` consolidation (1A MED).
- `getHeapHealth()` hysteresis (1A MED).
