---
id: TASK-601
title: 'fix(mqtt): JIT discovery — gate on hasConfig so phantom IDs don''t stall drip'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-14 16:57'
updated_date: '2026-05-25 22:15'
labels:
  - mqtt
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-073 introduced pure JIT discovery for OT MsgIDs: the JIT trigger in processOT() (OTGW-Core.ino:4112-4116) marks the pending bit for any OT message whose is_value_valid() is true and whose done-bit isn't set yet, on the assumption that loopMQTTDiscovery() will later drain it via doAutoConfigureMsgid().

The drip-loop has an asymmetry with the F (force) path that defeats this assumption: doAutoConfigureMsgid() returns false for any OT ID that has no HA sensor or binsensor entry and is not 0/27/Dallas. When the drip sees that failure (MQTTstuff.ino:1475-1482) it intentionally leaves the pending bit set ("retain pending — next drip tick retries automatically"). On the next tick the bitmap-scan picks the same low-numbered ID, fails again, and never advances. Effectively the drip stalls on the first such "phantom" ID.

markAllMQTTConfigPending() (the F path) is immune: at MQTTstuff.ino:1339 it only sets pending for IDs where sIdx != MQTT_HA_INDEX_NONE || bIdx != MQTT_HA_INDEX_NONE. JIT does not apply that filter, so any OT-bus traffic for an ID without an HA discovery entry — but with a valid OTmap msgcmd — can poison the pending bitmap and freeze further drip progress.

This matches the field report from 1.5.1-beta.3: values publish (is_value_valid true), JIT-mark is logically called, but no entities appear in HA until the user runs F. After F, all 118 active IDs publish in ~5 minutes because F never marks phantoms.

Fix: in the JIT branch in processOT() add the same "hasConfig" check that markAllMQTTConfigPending() already uses, so JIT and F paths are symmetric and cannot enqueue an ID the drip cannot publish.

Scope: src/OTGW-firmware/OTGW-Core.ino:4109-4116 only. No change to drip loop, F path, value-publish layer, or settings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 JIT branch in processOT() (OTGW-Core.ino:4109-4116) gates setMQTTConfigPending on a hasConfig predicate (readSensorIndex != NONE || readBinSensorIndex != NONE || id == 0 || id == 27)
- [x] #2 doAutoConfigure (F path) behaviour unchanged: markAllMQTTConfigPending still publishes all known IDs
- [x] #3 Build green: python build.py --firmware exits 0
- [x] #4 Evaluator green: python evaluate.py --quick shows no new failures vs baseline
- [x] #5 Version prerelease bumped (beta.3 → beta.4) via bin/bump-prerelease.sh; version.h + data/version.hash staged
- [x] #6 Commit message follows project convention; mentions JIT phantom-ID stall + ADR-073 reference
- [x] #7 Branch claude/fix-jit-mqtt-discovery-N7Yos pushed with draft PR open
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify the suspect call site once more: OTGW-Core.ino:4109-4116 — JIT branch in processOT() after PROGMEM_readAnything(OTmap[id]).
2. Confirm readSensorIndex/readBinSensorIndex are usable from OTGW-Core.ino (inline in MQTTstuff.h, visible TU-wide after MQTTstuff.ino preprocessing).
3. Apply the gate. Mirror exactly the filter that markAllMQTTConfigPending() at MQTTstuff.ino:1336-1342 uses, plus the special pseudo-IDs (0 climate, 27 number) that markAllMQTTConfigPending() explicitly re-adds. Comment briefly that this matches the F path symmetry per ADR-073.
4. Bump prerelease beta.3 → beta.4 (bin/bump-prerelease.sh).
5. Build: python build.py --firmware. Confirm exit 0.
6. Evaluator: python evaluate.py --quick. Confirm no new failures.
7. Commit on claude/fix-jit-mqtt-discovery-N7Yos. Message: fix(mqtt): gate JIT discovery on hasConfig — stop phantom-ID drip stall (ADR-073). Stage version.h + data/version.hash alongside OTGW-Core.ino.
8. Push the branch. Create a draft PR pointing at dev.
9. Mark ACs done, write Final Summary, set status Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete. Single-point fix in OTGW-Core.ino:4109-4126 (lines extended by docstring). Pre-release bumped beta.3 -> beta.4. Committed as f6f65bf. Pushed to claude/fix-jit-mqtt-discovery-N7Yos. Draft PR #572 opened against dev.

Blocking AC3 (build green): could NOT be self-verified in this sandbox -- python build.py --firmware fails to download arduino-cli (HTTP 403 on downloads.arduino.cc from this network). The maintainer's local workstation or CI must verify exit 0 before merging. Evaluator (AC4) was self-verified: same health score 91.7% before and after the patch, with identical pre-existing failures.

AC #3 geverifieerd: python build.py --firmware exit 0 op dev (commit a82de1e4 gemerged). Evaluator 100% groen.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixes a JIT MQTT discovery stall introduced (implicitly) with ADR-073's pure-JIT model.

Root cause
The JIT trigger at OTGW-Core.ino:4112-4116 enqueued ANY OT MsgID with valid is_value_valid into the discovery pending bitmap. For an ID whose OTmap msgcmd is valid but which has no HA sensor/binsensor entry, doAutoConfigureMsgid() returns false and the drip loop intentionally keeps the pending bit set (MQTTstuff.ino:1475-1482, "retain pending"). The drip's per-tick bitmap scan picks the lowest pending bit, fails on the phantom ID, retains it, and never advances. Real-config IDs queued later in the bitmap never get their turn. Visible symptom: clean-boot device shows zero HA discovery topics until operator presses Serial F.

F worked because markAllMQTTConfigPending() (the F path) filters at enqueue: it only sets pending for IDs where readSensorIndex != NONE || readBinSensorIndex != NONE (MQTTstuff.ino:1336-1345). JIT did not apply that filter, breaking the symmetry.

Fix
Mirror the same hasConfig predicate in the JIT branch (climate ID 0 and number ID 27 added explicitly to match the pseudo-IDs that markAllMQTTConfigPending() re-adds). Both trigger paths now enqueue an identical ID set and the drip loop is guaranteed forward progress under normal traffic.

Scope discipline
- One conditional added at one site (OTGW-Core.ino:4109-4126).
- Drip-loop "retain on failure" semantics preserved -- still correct for transient publish failures; just no longer reachable from JIT enqueue under normal operation.
- F path, value-publish layer, settings, ADR-062 verify path: untouched.
- Pre-release bumped beta.3 -> beta.4 per project versioning policy (firmware-touching commit).

Field validation pending
Tester must boot clean device + clean broker on beta.4, wait 3-6 minutes without pressing F, and confirm HA entities trickle in. Build verification (python build.py --firmware exit 0) also pending -- sandbox couldn't install arduino-cli (HTTP 403). Both gates need to be cleared on a real workstation/CI before the PR moves out of draft.

Files
- src/OTGW-firmware/OTGW-Core.ino (+15 -4 lines of real change)
- src/OTGW-firmware/version.h, data/version.hash (bump-prerelease.sh output)
- 24 .ino/.h/.css/.js/.html files (cosmetic version-comment bumps from bump-prerelease.sh)
<!-- SECTION:FINAL_SUMMARY:END -->
