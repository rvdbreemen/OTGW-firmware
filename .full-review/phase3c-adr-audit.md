# Phase 3C: ADR Audit

Session range: `ace21a48..a61373b9` on `feature-dev-2.0.0-otgw32-esp32-sat-support`. 8 commits in scope. ADR already created during this session: ADR-092 (NimBLE adoption).

## Summary

Most session work either (a) is covered by existing ADRs, (b) is a refactor / bug fix that does not need an ADR per CLAUDE.md, or (c) was already captured in ADR-092. However, three real ADR-action items surface from this audit, and all three are about **drift in already-Accepted ADRs**, not about missing new ADRs:

1. **ADR-092 drift**: ADR-092 was written for the periodic-scan port and explicitly states "Periodic-scan model retained" with `BLE_SCAN_DURATION_SEC = 3`. TASK-494 then deleted that model and the constant. Today's code calls `_pBLEScan->start(0, false, true)` (continuous scan) inside `satBLEInit()`. The ADR no longer matches the deployed code — same release.
2. **ADR-090 sub-rule 4 drift**: ADR-090 explicitly says "no `volatile` required on ESP8266" because the model is cooperative single-threaded, and adds the foresight clause: "A future SAT/OTGW32 introduction of FreeRTOS-style threading would require revisiting this rule, but until then `volatile` is unnecessary noise". TASK-497 introduces exactly that: a `portMUX_TYPE` cross-task spinlock between the NimBLE host task (core 0) and the Arduino loop task (core 1). The "future" arrived in this session.
3. **ADR-068 drift**: ADR-068 documents `OT_CMD_QUEUE_SIZE = 8`. The code in `OTDirect.ino:268` is `12`. (This is pre-existing drift from before this session — TASK-494 only narrates it in the new comment block. Worth flagging because we are auditing this surface anyway.)

The most consequential ADR-action item is **#2 (ADR-090 amendment)**: a binding ADR's stated invariant ("cooperative single-threaded, no `volatile` needed") is now contradicted in the same source tree, and a new shape (FreeRTOS spinlock cross-task snapshot pattern) was added without naming.

A new ADR for the OTDirect `otCmdEnqueue` queue (coalesce-by-MsgID, FIFO-with-replacement, capacity-12, intentional-no-bump) is **defensible but not mandatory** under CLAUDE.md — see "New ADRs recommended" below for the trade-off.

## ADR coverage matrix

| Session change | Covered by | Status | Action |
|---|---|---|---|
| NimBLE-Arduino adoption (TASK-487) | ADR-092 | Covers the dependency choice; **drifts** on the scan model after TASK-494 (continuous scan replaced periodic scan) | Amend ADR-092: replace "Periodic-scan model retained" paragraph with continuous-scan model; remove `BLE_SCAN_DURATION_SEC` reference; clarify `iBleInterval` semantic shift to publish/state-update cadence |
| TT/TC remote-override state machine (TASK-466) | ADR-064 (operating modes), ADR-087 (frame bridge) | **Not covered.** ADR-064 is about Gateway/Monitor/Master/Bypass/Loopback; nothing about per-MsgID override state. ADR-087 is about the text bridge to `processOT()`. Neither addresses the TT/TC PIC-parity state machine. | Defensible no-op: TT/TC is owner-described as PIC-parity (not a new architecture decision; we are matching documented PIC `gateway.asm` behaviour). Per CLAUDE.md "Do NOT create [ADRs] for: refactors, bug fixes, minor features within existing patterns", a feature that explicitly mirrors an external reference is a candidate for "minor feature within existing patterns". Fixture row in `tests/otdirect_pic_parity_fixture.md` documents the behaviour. **No ADR required**, but the gap is real if a future maintainer asks "why do TT and TC differ in MsgID 100 bits?" — fixture answers it, owner principles do not. |
| OTDirect command queue coalesce-by-MsgID + capacity 12 (TASK-494) | (claimed) ADR-064 / ADR-087 | **Not covered.** The TASK-494 description claim that "ADR-064 / ADR-087 already cover the queue model" is **false** — both ADRs were verified to contain zero references to `otCmdQueue`, `otCmdEnqueue`, coalescing, or capacity sizing. The closest relative is ADR-068 ("OT-Direct schedule tuning constants") which documents `OT_CMD_QUEUE_SIZE = 8`, now drifted to 12 in code. ADR-016 (OpenTherm Command Queue with Deduplication) covers the **other** queue: PIC-bound text command queue `cmdqueue[]` in `OTGW-Core.ino` with 2-char prefix dedup. Two distinct queues; ADR-016 does not transfer. | **Recommended new ADR or ADR-068 amendment** (see below). The session introduces three architecturally-meaningful properties: (i) coalesce key is MsgID (not full frame, not 2-char prefix as in PIC queue), (ii) position-preserving replacement (intentional), (iii) capacity is intentional-do-not-bump owner-stated rule. None of those are written down outside one code comment + one task description. |
| Cross-task BLE concurrency `portMUX_TYPE` snapshot pattern (TASK-497) | ADR-090 (re-entrancy guard pattern, guideline-level) | **Drift.** ADR-090 explicitly anticipated this case in sub-rule 4 and the Trade-offs section: "A future SAT/OTGW32 introduction of FreeRTOS-style threading would require revisiting this rule" and "would invalidate sub-rule 4 silently. The ADR would need an amendment if that happens." It happened. | **Amend ADR-090** (or write a sibling ADR cross-referencing it). The new pattern is a **different concurrency class**: cooperative-loop re-entrancy (ADR-090) versus FreeRTOS multi-task on ESP32-S3 (this session). Mixing them in one ADR risks confusion; a cleaner split is "ADR-090: cooperative re-entrancy" + a new ADR for "FreeRTOS cross-task scratch state on ESP32". |
| Single-buffer HA discovery for BLE sensors (TASK-488) | ADR-077 (streaming MQTT HA discovery architecture) | Mostly compatible. ADR-077 prescribes the two-pass MEASURE→WRITE chunked streaming pattern for unbounded payloads. `bleSensorPublishOneDiscovery` uses the streaming primitives (`beginMqttPublish`, `writeMqttChunk`, `endPublish`) but with a 768-byte stack-local buffer and a single-pass `snprintf_P` — explicitly "skips the two-pass MEASURE-then-WRITE dance ADR-077 prescribes for unbounded payloads", as documented in the helper's banner comment. This is a bounded-payload optimisation, not a violation. | **Optional micro-amendment to ADR-077**: add a one-line "Bounded payloads ≤ 768 bytes may use single-buffer + one chunk if the helper's banner comment justifies the bound." Strictly defensible without it — the existing ADR-077 text reads unbounded-payload-prescriptive, not all-payload-prescriptive. Low-priority. |
| Continuous BLE scan (TASK-494, BLE side) | ADR-092 | **Drift** (same row as NimBLE adoption above) | Same amendment as ADR-092 row above — single edit covers both. |
| "Queue is THE channel / no side-channels / fix mechanics first" owner principles | (none) | **Not written down anywhere in the repo.** Source: TASK-494 description ("Owner architectural principles (binding)") + a one-line code comment at `OTDirect.ino:261`. Plan file (`~/.claude/plans/stateless-growing-phoenix.md`) is outside the repo. | **Optional new guideline-level ADR**. These are explicit owner-stated decisions that govern the OTDirect command-queue design (rules out queue-size-bumps and override-table side-channels in future PRs). If a future maintainer or agent proposes "let's bump the queue to 32 because we saw drops", the binding rule lives only in a Discord-equivalent informal channel. Worth capturing — but only if the owner agrees it is a stable principle (vs a TASK-494-specific rationale). |
| `evaluate.py` PROGMEM gate refinement (TASK-482) | ADR-009 (PROGMEM) | Refinement of an existing gate (skip auto-generated `*.ino.cpp` and macro continuation lines). | No ADR required. CLAUDE.md "Do NOT create [ADRs] for: refactors, bug fixes". |

## New ADRs recommended

### Option A (recommended): ADR-093 — OTDirect command-queue model and coalesce semantics

- **Scope**: pattern-level / structural
- **CI gate (per ADR-080)**: feasible — an `evaluate.py` check that flags any addition to `OT_CMD_QUEUE_SIZE > 12` or any new producer of `otCmdEnqueue` for an MsgID outside the documented safety set. **If no CI gate is feasible / desired, the ADR Status line must say "guideline-level per ADR-080".**
- **Why**: The OTDirect ring buffer is the **single point of fan-in** for OT-direct commands across MQTT, REST, WebUI, and telnet. Three architectural properties currently exist only in code comments + one task description: (1) coalesce-by-MsgID with position-preserving replacement, (2) capacity 12 is intentional-do-not-bump, (3) drops are evidence of a producer-rate problem (no override-table side-channel rescue). These rules are owner-binding ("Owner architectural principles (binding)" in TASK-494) and govern future PRs.
- **Why not**: The behaviour is documented in `tests/otdirect_pic_parity_fixture.md`, the new TASK-466 fixture rows, and the verbose comment block in `OTDirect.ino:259-303`. A maintainer reading the file finds the rationale. CLAUDE.md "minimal change surface" + "Do NOT create [ADRs] for ... minor features within existing patterns" applies if you read the queue as part of the broader OTDirect architecture (ADR-064/068).
- **Recommendation**: **write the ADR if and only if** the owner intends the "queue is THE channel / no side-channels" principles to be binding on future PRs across multiple task cycles. Otherwise, fold the minimum into an **amendment to ADR-068** (which already covers `OT_CMD_QUEUE_SIZE`): bump documented size from 8 to 12, add a Coalesce-by-MsgID subsection, add the "intentional-do-not-bump" rationale.

### Option B (recommended): ADR-094 — FreeRTOS cross-task scratch-state pattern (ESP32 SAT BLE)

- **Scope**: pattern-level
- **CI gate (per ADR-080)**: hard to gate automatically (`portMUX_TYPE` usage is single-instance today; a regex check would be noisy). Realistic outcome: **guideline-level per ADR-080**, with the existing `_bleSensorsMux` named as the canonical exemplar — same shape ADR-090 took for itself.
- **Rationale**: ADR-090 is the right neighbour but the **wrong scope**: cooperative-loop re-entrancy on a single core is a different beast from preemptive multi-tasking across two ESP32-S3 cores. Mixing them in one ADR will confuse the next author. A sibling ADR cross-referencing ADR-090 ("see ADR-094 for FreeRTOS cross-task; ADR-090 for cooperative re-entrancy") is cleaner.
- **Alternative**: amend ADR-090 in place — add a FreeRTOS subsection, retire sub-rule 4's "no volatile required" claim, scope the rule to "cooperative re-entrancy paths only on ESP8266 and on ESP32 paths that do not cross task boundaries". This keeps one ADR per pattern but stretches ADR-090's identity. **The owner's call.**
- **Recommended**: **write ADR-094** as a sibling. Lower risk of muddying ADR-090's clear cooperative-loop story, gives a clean home for future cross-task decisions (e.g. when Ethernet ISR or a future SAT BLE GATT writer arrives).

### Option C (optional, low-priority): no new ADR

If owner wants to keep ADR count down: **amend ADR-068 + amend ADR-090** (no new ADRs). Slightly noisier ADR docs, but no new commitments.

## Existing ADRs needing amendment

### ADR-092 (NimBLE adoption) — amendment **required** to fix drift

The ADR-as-Accepted explicitly says "Periodic-scan model retained". Code as-deployed runs continuous scan. Same release. The ADR is wrong as-written.

Suggested edit (Decision section):
- Replace the "Periodic-scan model retained: a scan starts every `settings.sat.iBleInterval` seconds with a duration of `BLE_SCAN_DURATION_SEC = 3` seconds" bullet with: "**Continuous-scan model**: `_pBLEScan->start(0, false, true)` is called once in `satBLEInit()` and the scan runs forever on the BLE host task. `loop()` is never blocked. The earlier 3-s/30-s periodic model was discarded in TASK-494 because it caused a 30-s startup blackout window. `iBleInterval` is retained as a settings field but its semantics shift from scan-rate to publish/state-update cadence."
- Add to "Related" or a new "Amendment History" block: "TASK-494 (commit `805c6728`) replaced the periodic-scan model with continuous scan; `BLE_SCAN_DURATION_SEC` and `_bleLastScanMs` removed."

### ADR-090 (Re-entrancy guard pattern) — amendment **required** to fix drift

Sub-rule 4 ("No volatile required on ESP8266") and the broader scope assertion ("cooperative single-threaded model") are now contradicted by `_bleSensorsMux` in `SATble.ino`. The ADR's own foresight clause ("A future SAT/OTGW32 introduction of FreeRTOS-style threading would require revisiting this rule") triggered.

Suggested edit (sub-rule 4 + Trade-offs):
- Tighten sub-rule 4: "No volatile required for **cooperative-loop re-entrancy paths** on ESP8266 and on ESP32 paths that do not cross FreeRTOS task boundaries. For cross-task scratch state on ESP32 (e.g. NimBLE host task ↔ Arduino loop task) see [ADR-094 / sibling ADR or this ADR's FreeRTOS subsection]."
- Add reference to `_bleSensorsMux` as out-of-scope (cross-task, not cooperative re-entrancy).
- The third Trade-off bullet ("A future SAT/OTGW32 introduction of FreeRTOS-style threading invalidates sub-rule 4 silently. The ADR would need an amendment if that happens.") is the trigger that fired; either delete it (now that the amendment is in place) or update it to point at the new ADR/section.

### ADR-068 (OT-Direct schedule tuning constants) — amendment **recommended** to fix drift

ADR-068 documents `OT_CMD_QUEUE_SIZE = 8`. Code is `12`. Pre-existing drift (not introduced by this session — the change-source pre-dates `ace21a48`), but the audit surface includes the queue and the discrepancy is worth closing.

Suggested edit:
- Update the OT_CMD_QUEUE_SIZE row in the constants table from `8 frames` to `12 frames`.
- Update the `OT_CMD_QUEUE_SIZE = 8` Alternatives subsection to reflect the move to 12 (presumably justified by additional producers since 2026-04-09: SAT control loop on MsgID 1, MsgID 99 dripfeed, MsgID 100 RemoteOverrideFunction). If the rationale is unclear, run `git log -S "OT_CMD_QUEUE_SIZE" -- src/OTGW-firmware/OTDirect.ino` to find the bump commit.
- Add a Coalesce-by-MsgID subsection (or fold into Option A's new ADR-093).

### ADR-077 (Streaming MQTT HA discovery) — amendment **optional**

The ADR text reads as if the two-pass MEASURE-WRITE pattern is the only acceptable shape. The new BLE discovery helper uses a single-buffer single-pass shape for bounded payloads (≤ 768 bytes). The implementation is internally consistent with the helper's banner comment, but a reader of ADR-077 alone might think the BLE code violates it.

Suggested edit (Decision section, end):
- Add: "**Bounded-payload exception**: helpers whose maximum payload is statically known and fits in a single ≤ 1 KB stack buffer may use single-pass `snprintf_P` + a single `writeMqttChunk` call instead of the MEASURE/WRITE two-pass dance. The helper's banner comment must justify the bound. Example: `bleSensorPublishOneDiscovery` (768-byte buffer, four entity types per BLE MAC). The two-pass dance remains mandatory for unbounded or runtime-sized payloads."

Low-priority. Skippable.

## Existing ADRs that look stale or contradicted

- **ADR-092** (covered above) — drift-as-of-this-session, fix in same release.
- **ADR-090 sub-rule 4** (covered above) — drift-as-of-this-session, fix in same release.
- **ADR-068 OT_CMD_QUEUE_SIZE row** (covered above) — drift pre-existing, surfaced by this audit.
- **ADR-016** (PIC text command queue) — *not* contradicted: it correctly describes the PIC-bound `cmdqueue[]` text-string queue with 2-char-prefix dedup. Worth a one-line cross-reference note in any new ADR-093 (or ADR-068 amendment) clarifying that the two queues exist for the two different OT data paths (PIC text path versus OT-direct frame path).

No other Accepted ADR was found to be contradicted by the session's changes.

## Verdict

- **Mandatory fixes**: 2 ADR amendments (ADR-092, ADR-090). Both are drift in Accepted ADRs, same release as the drift-causing code. Leaving them untouched is maintenance debt that contradicts ADR-080's spirit (binding rules must match deployed code).
- **Recommended fixes**: 1 ADR amendment (ADR-068, queue size 8→12 + coalesce subsection) — pre-existing drift, low-risk.
- **Decision-needed**: 1 new ADR. Either **ADR-094 (FreeRTOS cross-task pattern)** as a sibling to ADR-090, OR fold the FreeRTOS scope into ADR-090 directly. Sibling is cleaner; in-place is denser. Owner picks.
- **Defensible no-op**: TT/TC state machine (TASK-466) — PIC-parity feature documented in fixture; no ADR strictly needed unless owner wants the architectural-decision audit-trail. New ADR-093 (OTDirect queue model) is also defensible no-op if the ADR-068 amendment is accepted instead.

**Total ADR-action items**: 3 mandatory edits (ADR-090, ADR-092 amendments + decision on ADR-094 vs in-place ADR-090 expansion) + 1 recommended (ADR-068) + 1 optional (ADR-077 micro-amendment).

**Top one**: **ADR-090 amendment for FreeRTOS cross-task drift**. ADR-090's own foresight clause anticipated this case by name; leaving the ADR unamended while shipping `_bleSensorsMux` is the most directly contradictory state in the corpus right now.
