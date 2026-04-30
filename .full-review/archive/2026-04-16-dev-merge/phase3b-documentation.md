# Phase 3B: Documentation Findings

Branch `1.4.1` vs `dev`, 14 commits. Phase 1B already covered the ADR governance findings (Accepted→Proposed, ghost ADR-077/078/080 citations, Windows plan-file path, missing `streamDallasSensorDiscovery`). Those are not re-raised here. This phase concentrates on documentation that ships alongside code: release notes, README, REST/MQTT user docs, inline comments, backlog task fidelity, and C4 docs.

## Summary

- Critical: 0
- High: 4
- Medium: 7
- Low: 5

## Coverage matrix

| Dimension | Status | Gap summary |
|---|---|---|
| Changelog / release notes | missing | No `RELEASE_NOTES_1.4.0.md` or `RELEASE_NOTES_1.4.1.md`; `BREAKING_CHANGES.md` stops at v1.3.5; `docs/releases/` has no 1.4.x file |
| README | missing 1.4.1 | README has a "What was new in v1.4.0" block but no 1.4.1 entry; feature list omits auto-verify, heap-diag topic, new REST endpoints |
| REST API docs | missing | `docs/api/openapi.yaml`, `docs/api/README.md` have zero mention of `/api/v2/discovery*`; only ADR-062 mentions them |
| MQTT docs | missing | `docs/api/MQTT.md` has no entry for `otgw-firmware/stats/heap`; no mention of the `MQTTdiscoveryAutoVerify` setting; discovery-verification mechanism undocumented |
| Inline code comments | partial | New state sections well commented; magic constants lack inline rationale (4 of 5 `VERIFICATION_*` constants); one stale location comment (Phase 1A HIGH #4 echo) |
| ADR supplementary (non-1B) | partial | Tuning tables accurate, task cross-refs present, but Consequences sections miss two Phase 2 behaviours: cooldown default deferring drip, and heap-abort false-negative |
| Backlog task fidelity | 3 inaccuracies | TASK-342 claims "ALL three call sites" (VH path missing); TASK-346/349/351 Final Summaries assert preconditions (NTP, uptime>3600) and location (doTaskEvery60s) that don't match shipped code |
| C4 docs | absent | `docs/c4/` directory does not exist on disk, despite being referenced in session memory as mandatory session-start reading |
| Leak scan | pre-existing only | No new Windows paths; "plan file expressive-growing-yao" reference leaks into 4 backlog task descriptions (beyond the ADR-flagged site) |

## Findings

### [HIGH] No release notes or changelog entry for 1.4.1

- **Where**: no `RELEASE_NOTES_1.4.0.md` or `RELEASE_NOTES_1.4.1.md` at repo root; `docs/releases/` ends at `RELEASE_GITHUB_1.3.5.md` and `RELEASE_NOTES_1.3.4.md`; `docs/BREAKING_CHANGES.md` stops at v1.3.5.
- **What's missing**: the branch ships four user-visible changes that need a release-notes entry:
  1. New MQTT topic `<topTopic>/otgw-firmware/stats/heap` (retained, hourly), 17-field JSON diagnostic blob.
  2. New setting `MQTTdiscoveryAutoVerify` (default `true`) that triggers a daily retained-discovery verify on the day-flip boundary.
  3. Three new REST endpoints: `GET /api/v2/discovery`, `POST /api/v2/discovery/verify`, `POST /api/v2/discovery/republish`.
  4. Behavioural shifts the user will notice: discovery drip default 1 s → 2 s (boot discovery takes ~2x longer), slow-mode 30 s → 10 s (faster recovery from pressure), nightly restart now fires at the wall-clock hour via `hourChanged()` instead of polling `minute() == 0`.
- **Fix**: create `RELEASE_NOTES_1.4.1.md` at repo root mirroring the structure of `RELEASE_NOTES_1.3.5.md`. Minimum sections: "Heap pressure reduction" (TASK-338..347), "Discovery verification and republish" (TASK-349/351), "Time-boundary dispatcher refactor" (TASK-350). Under each, state the user-visible effect in one paragraph and link the ADR. Add a `## 🛑 v1.4.0` and `## 🛑 v1.4.1` block to `docs/BREAKING_CHANGES.md` stating "no breaking changes" (the new setting is additive with a safe default).

### [HIGH] README has no "What's new in v1.4.1" entry

- **Where**: `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\README.md:7-18`
- **What's missing**: the README has a "What's New in v1.4.0" section that describes SAT/ESP32 but not the 1.4.1 heap-pressure and discovery-verification work. Users browsing the repo first will see a description of a version older than what they flash.
- **Fix**: add a new section ahead of the 1.4.0 section:
  ```markdown
  ## What's New in v1.4.1

  Version 1.4.1 focuses on ESP8266 heap robustness during Home Assistant MQTT auto-discovery and adds an automatic retained-discovery self-heal mechanism. Full release notes: [RELEASE_NOTES_1.4.1.md](RELEASE_NOTES_1.4.1.md).

  - Slower, heap-aware discovery drip (2 s normal, 10 s slow-mode) with post-Status-burst cooldown.
  - Retained-discovery verification: node-scoped wildcard subscribe, counts retained configs, re-announces on mismatch. Exposed via REST (`/api/v2/discovery`), telnet (V key) and MQTT telemetry. Opt-in daily auto-heal via `MQTTdiscoveryAutoVerify` (default on).
  - Hourly retained MQTT heap diagnostic at `otgw-firmware/stats/heap`.
  - Unified time-boundary dispatcher — hour/day/year triggers are now wall-clock aligned. See [ADR-064](docs/adr/ADR-064-time-boundary-single-caller-contract.md).
  ```

### [HIGH] Three new REST endpoints are not in openapi.yaml

- **Where**: `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\docs\api\openapi.yaml` (unmodified on this branch); `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\docs\api\README.md` (unmodified)
- **What's missing**: `GET /api/v2/discovery`, `POST /api/v2/discovery/verify`, `POST /api/v2/discovery/republish`. The handler in `restAPI.ino:468-527` returns rich JSON (`verification.*`, `counters.*`, `settings.auto_verify`) that clients cannot discover without reading C code.
- **Fix**: add three `paths:` entries under `/v2/discovery`, `/v2/discovery/verify`, `/v2/discovery/republish`. Mirror the schema of the existing `/v2/otgw/discovery` entry at `docs/api/openapi.yaml:963-993`. Include the 503/409 error cases (heap, MQTT disconnected, concurrent verify). Add a short section to `docs/api/README.md:384` ("Trigger MQTT Autodiscovery") explaining the relationship: `/v2/otgw/discovery` publishes all configs; `/v2/discovery/verify` checks what the broker retained; `/v2/discovery/republish` marks all pending without an explicit verify pass.

### [HIGH] Backlog Final Summaries misrepresent shipped behaviour

Three task Final Summaries assert guarantees the code does not provide. A future reviewer using the task as the canonical record will draw wrong conclusions.

1. **TASK-342** (`backlog/tasks/task-342 - Quiesce-MQTT-discovery-drip-during-Status-frame-burst.md:85`): claims the wrap around `publishMasterStatusState`/`publishSlaveStatusState` "covers all three Status-frame call sites automatically". It does not cover the VH (ventilation) publishers at `OTGW-Core.ino:1688-1732` — `publishMasterStatusVHState`, `publishSlaveStatusVHState`, `publishStatusVHBitMQTT` have no `beginStatusBurst()/endStatusBurst()` calls. Verified by grep: the six `beginStatusBurst`/`endStatusBurst`/`incrementStatusBurstPublishCount` call sites in `OTGW-Core.ino` are all in the non-VH paths.
   - **Fix**: append an erratum line via `backlog task edit 342 --append-final-summary "Erratum: the wrap does NOT cover VH publishers (publishMasterStatusVHState, publishSlaveStatusVHState, publishStatusVHBitMQTT at OTGW-Core.ino:1688-1732). VH-equipped boilers continue to run the drip through the VH Status fanout. Follow-up task to extend the wrap."`.
2. **TASK-349** (`backlog/tasks/task-349...md:60`) and **TASK-351** (`backlog/tasks/task-351...md:49`): both claim preconditions "(NTP sync, uptime>3600, heap>=6000, no pending drip, MQTT connected) are enforced inside startDiscoveryVerification()". Only three of those are actually enforced. `startDiscoveryVerification()` (MQTTstuff.ino:212-221) checks: `verifyActive`, `bConnected`, `!isFlashing()`, `countPendingDiscoveryIds() == 0`, `getFreeHeap() >= 6000`, `getMaxFreeBlockSize() >= 1280`. There is no NTP-sync check (`time() > 946684800`) and no `uptime > 3600` check.
   - **Fix (option A)**: add the two missing guards to `startDiscoveryVerification()` so the claim becomes true. This matches the quality bar TASK-345 already set for the nightly-restart block.
   - **Fix (option B)**: append an erratum to both task Final Summaries noting only three guards are enforced, and file a follow-up task. Phase 1A HIGH #2 recommends option A; the Final Summary needs to stop asserting option A's outcome until option A ships.
3. **TASK-346** (`backlog/tasks/task-346...md:74`): Final Summary says "Hourly via hourChanged() hook in doTaskEvery60s. 24 publishes/day, ~4.8 KB/day extra traffic." After TASK-350 refactor, the call moved to `doTaskMinuteChanged`. Traffic claim is still accurate; location claim is stale.
   - **Fix**: append a correction `--append-final-summary "Post-TASK-350: sendMQTTheapdiag() moved from doTaskEvery60s to doTaskMinuteChanged under if(hourFlag). Wall-clock aligned instead of boot-relative 60s drift."`.

### [MEDIUM] MQTT topic `stats/heap` is undocumented

- **Where**: `docs/api/MQTT.md` has no mention of the new retained topic. The topic is only visible in source (`MQTTstuff.ino:1108`) and in ADR-062 (indirectly via the heapdiag schema).
- **What's missing**: users integrating the diagnostic into HA or Grafana cannot discover the topic, its retention semantics, its 17 JSON fields, or the fact that counters reset on reboot. The full topic path (after `MQTTPubNamespace` prepend) is `<topTopic>/value/<nodeId>/otgw-firmware/stats/heap`.
- **Fix**: add a new subsection to `docs/api/MQTT.md` after the "Home Assistant discovery" section (around line 405) titled "Heap diagnostic telemetry". List the full topic path, retained=true, publish cadence (hourly on minute 00 wall-clock), and enumerate the 17 fields with their meaning (mirror the struct field comments in `OTGW-firmware.h:267-277`). Note explicitly that `drip_burst_skip` / `drip_cooldown_skip` / `drip_slowmode` are session counters, reset on reboot, cumulative while the firmware is running.

### [MEDIUM] Discovery-verification mechanism is undocumented outside ADR-062

- **Where**: `docs/api/MQTT.md:367-403` describes HA discovery using the pre-1.4.1 model (two paths, `homeassistant/status` trigger). There is no mention of the verification pass or the `MQTTdiscoveryAutoVerify` setting.
- **What's missing**: users troubleshooting "HA shows entity as unavailable" have no documentation of:
  - The 15-second verification window (what it does, what is visible on telnet).
  - The `/api/v2/discovery/verify` endpoint as a diagnostic tool.
  - The `MQTTdiscoveryAutoVerify` setting, its default, and how to disable (relevant for users on a shared broker with high wildcard traffic).
  - What `disc_last_orphan` means and why OTGW does NOT delete orphans.
- **Fix**: add a new "Retained discovery verification" subsection to `docs/api/MQTT.md` between the existing "JIT discovery" paragraph (line 390) and the "Discovery reset triggers" list (line 401). Describe the mechanism in two paragraphs, then list the three REST endpoints, the telnet `V` key, and the setting. Cross-link to ADR-062 for the design rationale.

### [MEDIUM] Inline rationale missing for `VERIFICATION_*` constants

- **Where**: `src/OTGW-firmware/MQTTstuff.ino:186-189`
- **What's missing**: four constants on consecutive lines with no per-line rationale:
  ```cpp
  constexpr unsigned long VERIFICATION_WINDOW_MS      = 15000;
  constexpr uint16_t      VERIFICATION_BUFFER_BYTES   = 1024;
  constexpr uint32_t      VERIFICATION_MIN_HEAP_START = 6000;
  constexpr uint32_t      VERIFICATION_MIN_HEAP_ABORT = 4500;
  ```
  Only the fifth one (`VERIFICATION_MAX_NODE_SEGMENT_LEN = 64`) has a rationale comment. The ADR has a tuning table but a reader debugging the code won't follow the ADR link back.
- **Fix**: add one-line rationale trailers matching the ADR-062 tuning table:
  ```cpp
  constexpr unsigned long VERIFICATION_WINDOW_MS      = 15000; // allows slow brokers; early-close when received>=expected && >500ms
  constexpr uint16_t      VERIFICATION_BUFFER_BYTES   = 1024;  // worst realistic discovery config ~900B; default 384 is too small
  constexpr uint32_t      VERIFICATION_MIN_HEAP_START = 6000;  // must clear the 1024B buffer-resize with comfortable margin above 4500B abort
  constexpr uint32_t      VERIFICATION_MIN_HEAP_ABORT = 4500;  // aligned with post-TASK-344 HEAP_LOW=5120; abort suppresses false-missing republish
  ```

### [MEDIUM] Inline rationale missing for `STATUS_BURST_COOLDOWN_MS = 10000`

- **Where**: `src/OTGW-firmware/MQTTstuff.ino:107`
- **What's missing**: the surrounding block header (lines 94-98) already documents the known trade-off (cooldown overlaps ~3s Status cadence). That's in the right place for a reader scanning the module. But the constant itself ships at the "problematic" value per Phase 2B HIGH (drip stalls under normal Status cadence). The code comment says "candidates: 2500ms = fits between bursts, 5000ms = partial overlap" but does not explain why 10000 was chosen as the shipped default.
- **Fix**: change the trailing comment on the constant line to reflect the deliberate choice:
  ```cpp
  constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 10000; // conservative default; lower to 2500 if iDripCooldownSkipCount grows without drip progress (see block comment above)
  ```
  If the Phase 2B HIGH recommendation lands (2000 ms), update the comment accordingly. Either way, the shipped value needs a reason in-place.

### [MEDIUM] Stale comment on hourly heap-diag publish path

- **Where**: `src/OTGW-firmware/MQTTstuff.ino:1071-1074`
- **What's wrong**: comment says "Called from the hourly tick (doTaskEvery60s gated by hourChanged)". After TASK-350 this is incorrect — the call is now in `doTaskMinuteChanged` under `if (hourFlag) { … sendMQTTheapdiag(); }`. Already noted as Phase 1A HIGH #4; restated here because it matters equally for doc hygiene.
- **Fix**:
  ```cpp
  /*
  Publish cumulative heap-pressure and drop diagnostics as a single retained JSON
  blob to <namespace>/otgw-firmware/stats/heap. Called from doTaskMinuteChanged
  under if(hourFlag) (ADR-064 single-caller contract; previously doTaskEvery60s
  which was boot-relative and drifted).
  ...
  */
  ```

### [MEDIUM] ADR-062 Consequences section does not acknowledge Phase 2 findings

- **Where**: `docs/adr/ADR-062-retained-discovery-verification.md:84-103` (Consequences).
- **What's missing**: Phase 2B quantified two behaviours that the ADR should own honestly:
  - **Heap-abort false-negative**: `tickDiscoveryVerification()` sets `verifyReceivedCount = expected` to suppress a false-missing republish (MQTTstuff.ino:315). The ADR calls out the `< 4500` abort threshold but does not say the outcome masks as "clean pass" from the perspective of `iLastMissingCount`. Users watching the telemetry cannot tell "no missing" from "heap-aborted".
  - **Boot-time first-minute dispatch**: at boot, `hourChanged()` / `dayChanged()` both fire true on the first minute because their `lastX` sentinels are `-1`. This means a fresh boot triggers `sendMQTTheapdiag` with zero counters AND the daily auto-verify on the first minute after NTP sync, not at the wall-clock day boundary. Phase 1B flagged; the ADR Consequences section should document the behaviour so it's not a surprise to a future reader.
- **Fix**: add two bullets under "Limits":
  - "A heap-abort during the window is indistinguishable in telemetry from a clean pass (`iLastMissingCount = 0`). If `iLastVerifyEpoch` advances but `iRepublishTriggeredCount` never does, check the debug log for `[verify] heap-abort` to distinguish."
  - "At boot, the `dayChanged()` helper's `lastX = -1` sentinel fires true on the first post-NTP-sync minute. With `MQTTdiscoveryAutoVerify = true`, a verify pass runs within one minute of reaching NTP sync. Intentional: covers the case where HA was restarted while OTGW was offline."

### [MEDIUM] ADR-064 Consequences section omits the same boot-time behaviour

- **Where**: `docs/adr/ADR-064-time-boundary-single-caller-contract.md:105-118`
- **What's missing**: the same first-minute dispatch issue applies to `hourFlag` (`runNightlyRestartCheck` / `sendMQTTheapdiag`). The `runNightlyRestartCheck` guards itself with `uptime > 3600`, so the nightly restart is protected. `sendMQTTheapdiag` is not. Boot-time first heap-diag publish lands with fresh-zero counters; the retained message on the broker is then stuck at zeros until the next real hour boundary. The ADR-064 Consequences do not mention this.
- **Fix**: add under "Benefits" or "Costs":
  - "All three flags (hourFlag/dayFlag/yearFlag) fire `true` on the first post-NTP-sync dispatcher tick because `lastX = -1` sentinels. Downstream consumers must defend against boot-time first-minute fires. `runNightlyRestartCheck` does so via `uptime > 3600`; `sendMQTTheapdiag` publishes a near-zero snapshot (acceptable; overwritten on the next real hour). New consumers added to `if (hourFlag) { ... }` must consider this."

### [LOW] OpenAPI spec has no entry for discovery-verify REST endpoints

- See HIGH finding on REST API docs. Low-severity echo: the OpenAPI `paths:` list is the machine-readable contract. Documentation-first clients (Postman, Swagger UI, HA REST integrations) won't see the endpoints at all until the YAML is updated.

### [LOW] "Plan file expressive-growing-yao" reference leaks into 4 backlog task descriptions

- **Where**: `backlog/tasks/task-348*.md`, `task-349*.md`, `task-350*.md`, `task-351*.md` — each Description ends with "See plan file expressive-growing-yao" (or similar).
- **What's wrong**: the plan file is a private task-planner artefact not checked into the repo. A future reader looking up the plan gets no result. Phase 1B flagged the full Windows path inside the two ADRs; the backlog task descriptions leak the same reference in a shorter form.
- **Fix**: edit each Description via `backlog task edit <id> -d "..."` and remove the "See plan file" trailing sentence. The ADRs and cross-task links are sufficient primary-source documentation.

### [LOW] TASK-355 exists and documents the revert — task itself is correct, needs visibility

- **Where**: `backlog/tasks/task-355 - choreadr-revert-ADR-062-064-to-Proposed-and-resolve-ghost-ADR-citations.md`
- **What's present**: the follow-up task for the Phase 1B CRITICAL / HIGH already exists on disk with the right scope (revert to Proposed, resolve ghost ADRs, remove plan-file path). This is good.
- **Note**: not a defect, just confirmation that the gap identified in Phase 1B already has a tracked remediation. Flagged as LOW so it is not lost in the Phase 3B consolidation.

### [LOW] `docs/c4/` directory referenced in session memory does not exist

- **Where**: `C:\Users\rvdbr\.claude\projects\...\memory\MEMORY.md` line "Always read docs/c4/ at session start"; `docs/c4/` does not exist on disk.
- **What's wrong**: pre-existing condition, not introduced by 1.4.1. But two new code surfaces this branch adds would fit naturally into a C4 hierarchy: the verification state machine (component-level) and the time-boundary dispatcher (code-level). With no directory, there is nowhere to file them.
- **Fix**: either create `docs/c4/` with initial scaffolding (`context.md`, `container.md`, `component.md`, `code/`) under a follow-up task, or delete the memory reference. Do NOT block 1.4.1 on this — pre-existing; mentioned for completeness because the system-reminder context asks about it.

### [LOW] REST GET /api/v2/discovery does not include `auto_verify` documentation

- **Where**: `src/OTGW-firmware/restAPI.ino:474-487` — the GET response JSON includes `settings.auto_verify`, but TASK-351 AC#8 is unchecked ("REST GET /api/v2/discovery exposes auto_verify boolean"). The AC is actually met; the checkbox is wrong. Minor backlog hygiene.
- **Fix**: `backlog task edit 351 --check-ac 8` (run manually; the field is present in the handler at `restAPI.ino:479`).

### [LOW] `sendMQTTheapdiag` JSON blob is 17 fields but the struct comment shows 9 counters

- **Where**: `OTGW-firmware.h:267-277` documents the 9 HeapDiag fields; `MQTTstuff.ino:1084-1107` emits 17 JSON keys (heap-diag + discovery + live free-heap/max-block/frag-pct). A reader looking at the struct definition and expecting the MQTT payload to match will be confused.
- **Fix**: add a comment above the `HeapDiagSection` struct listing the additional fields the retained MQTT blob includes (`free_heap`, `max_block`, `frag_pct`, `disc_*`) and note they come from live calls / `state.discovery`. Prevents next maintainer from thinking the struct is authoritative for the wire format.

## Leak scan

- **No machine-specific paths introduced this branch** outside the two ADR "Plan file: C:\Users\rvdbr\.claude\plans\..." lines already flagged by Phase 1B.
- **Backlog task descriptions** (TASK-348/349/350/351) reference "plan file expressive-growing-yao" by name without a path. Unresolvable to readers; flagged above as LOW.
- **No TODO(robert), FIXME, or HACK personal tags** found in `src/OTGW-firmware/` or `docs/` on this branch.
- **No credentials or PII** found in example configs. `mqttha.cfg` is config only; `settings.mqtt.sUniqueid` default is `otgw-<chipId>`, not PII.
- **No Discord or GitHub usernames** in ADRs or backlog tasks (checked against the session memory mentions of `sergeantd` — not present in repo).

## Consolidation notes for the reviewer merging Phase 3A + 3B

Phase 3B's HIGH findings are documentation omissions that should land before or with the 1.4.1 release, not merge-blockers for code. The three must-do items are:

1. Create `RELEASE_NOTES_1.4.1.md` + update `BREAKING_CHANGES.md`.
2. Update `docs/api/openapi.yaml` and `docs/api/MQTT.md` for the new REST endpoints and MQTT topic.
3. Add the README "What's new in v1.4.1" section.

The task-fidelity HIGH (TASK-342/349/351 Final Summaries) should be fixed at the task level via `backlog task edit --append-final-summary` — takes five minutes and preserves audit trail. The Phase 1A HIGH #2 root cause (missing NTP/uptime guards in `startDiscoveryVerification`) is the right permanent fix, but the task-record correction is independent.

The two MEDIUM ADR-Consequences additions are cheap edits on Proposed ADRs — land them during the Phase 1B-mandated "revert to Proposed" commit (TASK-355) rather than as a separate change.
