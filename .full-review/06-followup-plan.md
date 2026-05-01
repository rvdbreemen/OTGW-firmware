# Follow-up Plan — Comprehensive Review Mediums & Lows

## Context

The comprehensive review on session `ace21a48..6537b75a` closed all 15
Highs in five fix-batch commits. **31 Mediums and 46 Lows remain
deferred** as "out of scope, follow-up sweep" in the 05-final-report.

This plan groups them by effort and risk so we can execute in batches
without scope-creep. Each batch is a self-contained TASK with builds,
fixture validation, and evaluate.py as gates.

**Total estimated effort**: ~7-9 hours of focused work split across
4 batches + 1 ops sweep + 1 polish sweep.

## Deferred findings by priority

### Quick-win Mediums (~30-60 min total) — Batch A (TASK-498)

Each is a few lines of code, files don't overlap, validated by one
build cycle.

| ID | Source | Issue | Fix |
|---|---|---|---|
| **1A-M2** | OTDirect.ino | `lastThermostatVal = 0xFFFF` sentinel collides with -0.0039 °C raw f8.8 | Add `bool bHaveLastThermostat` companion; remove sentinel-by-magic-value |
| **1B-M3** | OTGW-firmware.h, MQTTstuff.ino, SATble.ino | New exports `bleMacToCompact`, `bleSensorPublish*` break `satBLE*` naming convention | Rename to `satBLE*` form |
| **2A-M3** | SATble.ino | BTHome v2 parser bails on first unknown object ID; spec-compliant devices that prefix temperature with `0x00` packet-ID byte silently drop | Add `case 0x00: pos += 1; break;` to parser switch |
| **3A-M1** | tests/test_otdirect_override.cpp | No coverage for TASK-494 coalesce-by-MsgID semantics | Add a 4-line `otCmdEnqueue` coalesce assertion |
| **4B-M2** | OTDirect.ino, restAPI.ino | `otCmdQueueHighWater` only visible via telnet; REST/MQTT can't see it | Expose in `/api/v2/otdirect/status` JSON + PR= reporting |
| **4A-M2** | SATble.ino | `SATBLEScanCallbacks` could be `final` | One-word language polish |

**Total ~50 lines diff. One commit, one build cycle.**

### Documentation drift Mediums (~45 min total) — Batch B (TASK-499)

C4 docs and ADRs that have measurable drift after the BLE/OTDirect
changes. All doc-only, no firmware build needed.

| ID | Source | Drift | Fix |
|---|---|---|---|
| **1A-M5** | docs | BLE state-topic format duplicated in two places | Consolidate into a single canonical reference in `docs/api/MQTT.md`; cross-reference from code comments |
| **1B-M1** | ADR-077 | Bounded-buffer single-publish exception (TASK-490) not codified | One-line addendum to ADR-077 whitelisting bounded-payload single-buffer publishes |
| **3B-M1** | ADR-092 | Doesn't yet reflect TASK-497 portMUX (the TASK-494 continuous-scan was added to it; cross-task race fix is missing) | Add a brief reference + delegate to ADR-090's amendment |
| **3B-M2** | C4 docs | `c4-code-otdirect.md` and `c4-code-sat.md` have stale `BLEDevice.h`, 7-vs-8 entry override-table, pre-TASK-466 TC=/MsgID 1 mapping | Refresh both files against current code |
| **3B-M3** | MQTTstuff.ino | Block-comment 75 lines above `bleSensorPublishOneDiscovery` still claims "two-pass shape ADR-077 requires" | One-line correction |

**~80 lines of doc edits. One commit.**

### Real-issue Mediums (~2-3 hours total) — Batch C (TASK-500)

Genuine investigations or design decisions, not paperwork.

| ID | Source | Issue | Approach |
|---|---|---|---|
| **1A-M1** | OTDirect.ino setRemoteOverride | Ghost override on queue-full: state writes BEFORE enqueue, so a queue-full leaves `mode=TEMPORARY` with no frame in flight | Reverse the order: enqueue first, on success write state. Document the sequencing contract. ~10 lines + comment. |
| **1A-M3** | OTDirect.ino TT/TC handlers | Threshold for "clear" is exact zero; negative values silently set a TEMPORARY override of negative °C | After investigation: gateway.asm parity says only TT=0 clears, so non-zero (incl. negative) sets. With floatToF88 clamp now in place, clamp-to-(-40) is the documented behaviour. **Mark as not-a-bug, document in OTDirect.ino comment.** |
| **2A-M2** | SATble.ino | Default-allow MAC filter security: empty `sBleMAC` accepts ANY parseable BTHome/ATC sensor in radio range | UX decision needed. Options: (a) require explicit opt-in via a new `bAcceptAnyBleSensor` setting; (b) RSSI-floor filter (e.g. require RSSI > -80 dBm); (c) document the trust model in MANUAL.md and ship as-is. Recommend (c) for now, escalate if a field report surfaces. |
| **2B-M1** | satBLEPublishMQTT, MQTTstuff.ino | Worst-case 32-publish first-scan burst (4 sensors × (4 discovery + 4 state)) gated only by heap-tier gates | Add a small intra-burst yield (`feedWatchDog()` is already there post-TASK-496; add `delay(0)` between sensors to release the loop task). ~5 lines. |
| **2B-M3** | OTDirect.ino otCmdQueue | Queue depth 12 is borderline for new TT/TC + CS/C2 + SAT periodic stack-up | With coalesce-by-MsgID (TASK-494) effective depth is much smaller. Verify via `otCmdQueueHighWater` after Batch A exposes it via REST/MQTT. **Defer until field data available.** |
| **3A-M2** | tests/ | MAC-filter bypass scenarios untested | Add `tests/test_ble_filter.cpp` covering empty-MAC accept-all, configured-MAC strict, malformed MAC reject paths. |
| **3A-M3** | tests/ | BLE byte-layout parsers (ATC/pvvx, BTHome v2) untested | `tests/test_ble_parsers.cpp` — ~100 lines, parses fixed-byte payloads against expected float values. Closes the original 3A "honourable mention" gap. |
| **3A-M4** | tests/ | Encrypted-BTHome-flag rejection untested | Add a fixture row in `test_ble_parsers.cpp` with `flag=0x01` (encrypted bit set) — assert parser returns false. |
| **3A-M5** | tests/ | OTDirect coalesce semantics not host-tested for non-MsgID-16 cases | Extend Batch A's 3A-M1 single assertion to a 4-row coalesce table covering MsgIDs 1, 14, 100. |
| **4A-M2 follow** | OTDirect.ino | TT/TC dispatch could be a table per ADR-078 pattern | Larger refactor; defer to a clean architectural sweep. **No-op this round.** |

### Operational Mediums (~2 hours total) — Batch D (TASK-501)

CI/DevOps work that needs scripting and external account access.

| ID | Source | Issue | Approach |
|---|---|---|---|
| **4B-M1** | OTGW-ModUpdateServer-esp32.h | "Safe OTA rollback" claim contradicts single-`ota_0` partition reality. No `esp_ota_mark_app_valid_cancel_rollback` calls anywhere | Add the call after first successful HTTP response (post-boot) in `setup()` or a post-boot hook. Plus document partition layout truthfully — there IS no second app slot, the rollback is implicit-via-bootloader-anti-rollback only. ~15 lines + doc fix. |
| **4B-M3** | repo / CI | No dependency-vulnerability scan on `lib_deps` | Add a GitHub Actions step using `npm audit`-equivalent for PlatformIO: `pio pkg outdated` + manual CVE check. ~20 lines YAML. |
| **4B-M4** | release artefacts | No SHA256SUMS published with releases | Add a `release.yml` workflow step that hashes each artefact and uploads `SHA256SUMS.txt`. ~10 lines YAML. |
| **4B-M5** | docs | No formal hardware-verification log template for AC #11 gates | Create `docs/templates/hardware-verification-log.md` with a fillable matrix. ~30 lines markdown. |

### Lows — Batch E (TASK-502, polish sweep)

46 Lows across all phases. None are bugs; all are style / consistency
/ comment-quality / minor cleanup. Group them as one "polish sweep"
commit covering all the easy ones.

Examples per phase:
- **1A-L1..L8**: cumulative naming, comment, minor style nitpicks.
- **1B-L1..L4**: documentation cross-references and naming consistency.
- **2A-L1..L2**: spec-compliance audit-trail notes.
- **2B-L1..L6**: minor allocation, comment, doc observations.
- **3A-L1..L4**: minor fixture additions and docstring suggestions.
- **3B-L1..L5**: minor inline comment touch-ups.
- **4A-L1..L10**: SemVer pin tightening, enum-class consistency, static_cast-over-C-cast, named BLE constants, RAII over manual portENTER/EXIT_CRITICAL, named OT-frame accessors, ADR-078-style dispatch table for OTDirect commands.
- **4B-L1..L7**: smaller CI/ops nitpicks.

**Strategy for Lows**: do one polish-sweep PR cherry-picking the trivial ones (~20 of 46), defer the rest to next major work cycle.

## Suggested execution order

1. **Batch A** (Quick-win Mediums, ~50 min) — visible improvements, shipping safety nets.
2. **Batch B** (Doc drift, ~45 min) — unblocks PRs that reference these ADRs/docs.
3. **Batch C** (Real-issue Mediums, ~3 hours) — substantive code/test additions.
4. **Batch D** (Operational, ~2 hours) — CI/release/security infrastructure.
5. **Batch E** (Lows polish sweep, ~2 hours) — pure polish, low priority but cumulative quality lift.

Total: ~7-9 hours focused work, distributed across 5 separate commits
on `feature-dev-2.0.0-otgw32-esp32-sat-support`. None block merge to
`dev` once owner hardware verification (TASK-487/488/494/466) lands.

## Out of scope

- Date-gated tasks (TASK-409 release-gate to 2.3.0/3.0.0, TASK-421
  date-gate week of 2026-05-23, TASK-480 release-cycle gate).
- Hardware-pending verification (TASK-487, TASK-488, TASK-494 AC #11,
  TASK-466 AC #12, TASK-496 ACs 3-4).
- Field-report investigations (TASK-431, 432, 484, 486 — those need
  reporter input or lab repro).
- Larger architectural refactors flagged as "defer" in the review
  output (ADR-078-style dispatch table, full SATBLEScanCallbacks
  extract-to-free-function, OTA rollback wiring with anti-rollback
  fuse — each its own design decision).

## Decision points for the owner

1. **Approve Batch A** for immediate execution? It's the lowest-risk
   group with the highest visibility-per-effort ratio.
2. **Approve Batches B+C** sequentially after A? They depend on no
   Batch A artefacts but should land in stable-build cadence.
3. **Defer Batch D + E** to a separate sprint, or include them?
   Batch D involves CI/secrets/release tooling; Batch E is pure
   polish. Both are deferable without blocking 2.0.0.
4. **2A-M2 (default-allow MAC filter)** decision: option (a) explicit
   opt-in setting / (b) RSSI floor / (c) document and ship as-is.
   Owner's call — this changes user-visible behaviour.
5. **3A-M3 + M4 (BLE parser tests)** worth ~90 min — yes, do them
   now while context is fresh, or batch into next test-coverage
   sweep?

Choose your batches; I execute, build, commit, push.
