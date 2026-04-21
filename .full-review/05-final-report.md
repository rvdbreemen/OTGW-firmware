# Comprehensive Code Review — Final Report

**Target**: Branch `1.4.1` vs `dev` (14 commits, ~20 source files, +2010 / −91 lines)
**Reviewed**: 2026-04-21
**Phases executed**: 1 (Quality + Architecture), 2 (Security + Performance), 3 (Testing + Docs). Phase 4 (Framework + DevOps best practices) skipped by user choice.
**Threat model**: LAN-only ESP8266 behind home router, NAT-isolated. Broker = trust boundary.

## Executive Summary

This branch delivers a coherent set of ESP8266 heap-pressure reductions, adds a retained MQTT discovery verification mechanism with daily auto-heal, and refactors time-boundary dispatch into a single-caller contract (ADR-064). The engineering is competent — **no crash-class code defects, no platform-constraint violations, no security bugs within the LAN-only threat model** — but the release ships with **two arithmetic bugs that a five-minute static check would have caught**, and a documentation gap that hides half the user-visible changes.

The single Critical finding is code: a JSON buffer that will truncate under saturation and corrupt the retained `stats/heap` MQTT message. The sole architectural Critical is process, not code: both new ADRs cite companion ADRs (077/078/080) that do not exist in `docs/adr/`. Neither is a shipping blocker if addressed; both are straightforward fixes.

**Grade**: B. Shippable after five high-priority small fixes. Solid engineering with a cluster of fixable hygiene gaps.

## Findings by Priority

### Critical (P0 — must fix before merge)

| ID | Finding | File:line | Source | Task |
|---|---|---|---|---|
| C-1 | `sendMQTTheapdiag` JSON buffer truncates at max counter saturation (465 bytes needed, 384 allocated, 81-byte overflow); malformed retained MQTT message stays on broker until next hour | `MQTTstuff.ino:1083` | 2B | TASK-352 |
| C-2 | Both new ADRs cite non-existent companion ADRs (ADR-077/078/080); ADR-062 promises CI gates that were never implemented | `docs/adr/ADR-062,064` | 1B | TASK-355 |

### High (P1 — fix before release)

| ID | Finding | File:line | Source | Task |
|---|---|---|---|---|
| H-1 | `STATUS_BURST_COOLDOWN_MS = 10000` permanently defers drip under documented 3s Status-frame cadence — primary feature failure during first-boot HA integration | `MQTTstuff.ino:107` | 2B | TASK-353 |
| H-2 | VH (ventilation) status publishers bypass the Status-burst quiesce (`publishMasterStatusVHState`, `publishSlaveStatusVHState`, `publishStatusVHBitMQTT`) — heap-reduction benefit zero for VH boilers | `OTGW-Core.ino:1667-1732` | 1A, 2B | TASK-354 |
| H-3 | Both new ADRs carry `Status: Accepted` without explicit user approval (violates ADR governance per CLAUDE.md); plan-file path `C:\Users\...\.claude\plans\...` leaks into ADRs and 4 backlog tasks | `docs/adr/ADR-062,064` | 1B, 3B | TASK-355 |
| H-4 | Comment in `doTaskMinuteChanged` claims NTP + uptime>3600 guards are enforced inside `startDiscoveryVerification`; they are not. Comment misleads maintainers; startup verify may run before per-source discovery topics published | `OTGW-firmware.ino:316-319`, `MQTTstuff.ino:212` | 1A | TASK-359 |
| H-5 | REST `/verify` handler hard-codes `6000` where `VERIFICATION_MIN_HEAP_START` exists; two sources of truth will silently drift | `restAPI.ino:499` | 1A | TASK-358 |
| H-6 | Stale comments: `sendMQTTheapdiag` claims `doTaskEvery60s` (actually `doTaskMinuteChanged` under hourFlag post-ADR-064); drip loop comment says "3s interval" (actually 2s/10s); ADR-077 reference doesn't exist; `(void)yearFlag;` is a no-op | `MQTTstuff.ino:1071`, `OTGW-firmware.ino:409,324` | 1A | TASK-360 |
| H-7 | No release notes for 1.4.1 (RELEASE_NOTES_1.4.1.md absent, BREAKING_CHANGES ends at v1.3.5, README latest section is v1.4.0) — four user-visible changes undocumented | repo root, `docs/releases/` | 3B | TASK-365 |
| H-8 | Three new REST endpoints absent from `openapi.yaml`; new MQTT topic `stats/heap` absent from `docs/api/MQTT.md`; discovery-verification mechanism undocumented | `docs/api/` | 3B | TASK-366 |
| H-9 | Three backlog task Final Summaries misrepresent shipped behaviour (TASK-342 claims VH covered; TASK-349/351 claim NTP/uptime preconditions; TASK-346 claims old call site) | `backlog/tasks/task-342,346,349,351` | 3B | TASK-367 |

### Medium (P2 — plan next sprint)

| ID | Finding | Source | Task |
|---|---|---|---|
| M-1 | `/api/v2/discovery/republish` has no rate limit — post-auth LAN loop permanently locks out verify endpoint (CWE-770) | 2A | TASK-356 |
| M-2 | Verify-window callback filter falls through to command dispatcher on non-3-segment haprefix topics (CWE-20) | 2A | TASK-357 |
| M-3 | Verify heap-abort masks failure as clean pass by setting `verifyReceivedCount = expected`; telemetry can't distinguish "clean" from "aborted indeterminate" | 1A, 2B | TASK-361 |
| M-4 | `getHeapHealth()` tier-transition counters inflate on boundary chatter (no hysteresis); can reach ~1440/day false positives | 1A, 2B | not yet tasked — low stakes |
| M-5 | Four `ZonedDateTime` allocations per minute in dispatcher (refactor moved them, didn't consolidate) | 1A | not yet tasked — performance is negligible |
| M-6 | `handleDiscovery` GET endpoint uses 320-byte stack buffer for 9 vararg `snprintf_P` with no truncation check (~250B current, thin margin) | 1A | not yet tasked |
| M-7 | MQTT.md and inline rationales missing for new `stats/heap` topic, VERIFICATION_* constants, STATUS_BURST_COOLDOWN_MS default; ADR Consequences sections miss Phase 2 behaviours | 3B | TASK-366 + extensions to TASK-355/360 |
| M-8 | `runNightlyRestartCheck` lost its `minute() == 0` safety guard during refactor — defense-in-depth removed | 1A | not yet tasked |
| M-9 | Discovery verify logs use raw `DebugTln` instead of `MQTTDebugTln` (runtime-gating inconsistency) | 1A | not yet tasked |
| M-10 | Redundant `isStatusBurstActive()` double-test in `loopMQTTDiscovery`; diagnostic counters rely on call-order coincidence | 1A | not yet tasked |
| M-11 | Heap-diag JSON buffer alignment comment says "aligned with HEAP_WARNING_THRESHOLD" but values differ (3000 vs 3072) | 1A | not yet tasked |
| M-12 | VERIFICATION_MIN_HEAP_START = 6000 may be insufficient when WebSocket + concurrent sendDeviceInfoV2 both active (two-client scenario at boot) | 2B | not yet tasked |

### Low (P3 — track in backlog)

| ID | Finding | Source | Task |
|---|---|---|---|
| L-1 | 14 dead-code items (3 write-only state fields, 2 same-TU publics, stale ADR comments, redundant annotations) | 1A | TASK-362 |
| L-2 | `MQTTstuff.ino` god-object creep (+379 lines, now hosts 5 state machines); extract verify TU | 1B | TASK-363 |
| L-3 | ADR-062 CI gates (`check_discovery_counter_instrumented`, `check_publishedtopic_counter_reset`) never implemented | 1B | TASK-364 |
| L-4 | 4 additional evaluate.py regex gates + wire into CI (buffer arithmetic, cooldown bound, ADR resolve, VH wrap) | 3A | TASK-368 |
| L-5 | Orphaned `tests/test_dallas_address.cpp` — rewrite host-compilable or delete | 3A | TASK-369 |
| L-6 | Assorted LOW items from all phases (slow-mode 13-min drip documentation, DiscoverySection padding comment, positive shim pattern confirmation, etc.) | various | not individually tasked |

---

## Findings by category

| Category | Count | Severity mix |
|---|---|---|
| Code quality (1A) | 34 | 4H, 9M, 7L, +14 dead-code |
| Architecture (1B) | 13 | 1C, 3H, 5M, 4L |
| Security (2A) | 7 + 2 info | 2M, 3L, +2 informational |
| Performance (2B) | 9 | 1C, 2H, 3M, 3L |
| Testing (3A) | 9 | 4H, 2M, 3L |
| Documentation (3B) | 16 | 4H, 7M, 5L |
| **Total** | **88** | **2 Critical, 20 High, 28 Medium, 27 Low, 14 dead-code, 2 info** |

## Category insight

The branch does well on **platform correctness**: no PROGMEM-domain errors, no stack-overflow traps, no re-entrancy corruption introduced, no String class in hot paths, no HTTPS creep, static buffers respect ADR-040 lessons. It also does well on **primary goal validation**: heap-pressure thresholds were lowered with field-log evidence, and the worst-case heap envelope analysis confirms the lowered CRITICAL threshold has 1364-byte margin under the worst concurrent-allocation path modelled.

It does poorly on **process discipline**: ADR status flipped to Accepted without user approval, companion ADRs cited that don't exist, CI gates promised but not delivered, three backlog Final Summaries assert guarantees the code doesn't provide.

It does poorly on **user-facing documentation**: four user-visible changes with zero release-notes / README / openapi coverage.

And it ships with **two specific arithmetic defects** that a ten-line `evaluate.py` regex would have caught before a human reviewer saw them.

## Recommended action plan

### Before merge (blockers + high-priority hygiene)

Ordered as a single PR ("1.4.1 release gate") touching small, independent files:

1. **TASK-352** — bump `char json[384]` → `char json[512]` in `sendMQTTheapdiag` *(1 line + comment)*
2. **TASK-353** — change `STATUS_BURST_COOLDOWN_MS = 10000` → `2000` *(1 line + comment refresh)*
3. **TASK-354** — wrap VH publishers in `beginStatusBurst`/`endStatusBurst`; add `incrementStatusBurstPublishCount()` to `publishStatusVHBitMQTT` *(~15 lines across 3 functions)*
4. **TASK-355** — revert ADR-062/064 `Status: Accepted` → `Proposed`, replace ADR-077/078/080 citations (likely with ADR-044/050 or remove), strip Windows plan-file path, add Phase 2 behaviour bullets to Consequences *(doc-only)*
5. **TASK-359** — add NTP + uptime>3600 guards to `startDiscoveryVerification()` so the comment in `doTaskMinuteChanged` becomes accurate *(~4 lines)*

**Effort**: ~1-2 hours total. All items are small, independent, testable by a single manual reproduction on one boiler.

### Before release announce (documentation + audit trail)

6. **TASK-365** — `RELEASE_NOTES_1.4.1.md` + `BREAKING_CHANGES` update + README "What's new in v1.4.1"
7. **TASK-366** — `openapi.yaml` + `docs/api/MQTT.md` updates for 3 endpoints + stats/heap topic
8. **TASK-367** — erratum-append on 3 task Final Summaries + remove plan-file references from 4 task descriptions

### Short-term (~1 sprint)

9. **TASK-356, TASK-357** — the two Phase 2A MEDs (republish rate limit, verify fall-through guard)
10. **TASK-358, TASK-360** — dedupe the `6000` constant + assorted comment hygiene
11. **TASK-361** — verify outcome enum (telemetry fidelity)
12. **TASK-368** — wire `evaluate.py` into CI + add 4 regex gates

### Follow-up (not blocking)

13. **TASK-362** — dead-code cleanup (14 items)
14. **TASK-363** — extract `mqtt_discovery_verify.cpp/h` (post-merge refactor)
15. **TASK-364** — implement the ADR-062 CI gates specifically
16. **TASK-369** — Dallas test host-compilable or delete

## Review Metadata

- Review date: 2026-04-21
- Phases completed: 1A, 1B, 2A, 2B, 3A, 3B (plus consolidations)
- Phase 4 explicitly skipped by user choice (framework = Arduino/ESP8266 + CI = existing GitHub Actions; minimal value added)
- Backlog tasks created: **18** (TASK-352 through TASK-369)
- External review preserved separately: `.external-reviews/HANDOFF-claude-review-c-codebase-303Qj.md` (not merged per explicit user instruction — it has wider scope than this branch-diff review)
- Flags active: `performance_critical = true`, `strict_mode = false`
- Threat model: LAN-only ESP8266 (NAT-isolated, no internet exposure); security findings calibrated accordingly

## Artefact index

| File | Size | Purpose |
|---|---|---|
| `00-scope.md` | 4 KB | Review scope definition |
| `phase1a-code-quality.md` | 40 KB | Full code-quality + dead-code findings |
| `phase1b-architecture.md` | 26 KB | Full architecture + ADR assessment |
| `01-quality-architecture.md` | 8 KB | Phase 1 consolidation |
| `phase2a-security.md` | 18 KB | LAN-calibrated security findings |
| `phase2b-performance.md` | 28 KB | Quantitative performance validation |
| `02-security-performance.md` | 7 KB | Phase 2 consolidation |
| `phase3a-testing.md` | 23 KB | Testing strategy + evaluate.py extensions |
| `phase3b-documentation.md` | 15 KB | User-facing + inline doc gaps |
| `03-testing-documentation.md` | 8 KB | Phase 3 consolidation |
| `05-final-report.md` | this file | Executive synthesis |
| `state.json` | — | Orchestration state |

## One-line verdict

Merge after five small fixes (TASK-352, 353, 354, 355, 359 — roughly 20 lines of code plus doc edits); the rest is sprint/follow-up scope. The code is sound; the release-engineering around it is thin.
