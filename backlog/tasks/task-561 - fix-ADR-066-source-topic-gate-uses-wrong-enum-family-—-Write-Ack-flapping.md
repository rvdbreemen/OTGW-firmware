---
id: TASK-561
title: 'fix: ADR-066 source-topic gate uses wrong enum family — Write-Ack flapping'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 13:16'
updated_date: '2026-05-07 21:55'
labels:
  - bug
  - mqtt
  - adr-066
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
publishToSourceTopic() compares rsptype (OTGW_response_type, 0..5) against OT_WRITE_ACK (OpenThermMessageType=B101=5). The numeric collision means the gate fires only for OTGW_UNDEF, never for real boiler Write-Ack frames. Result: msgids 14, 16, 23, 24, 37, 98 publish their per-spec-undefined Write-Ack data byte (~0) to <topic>_thermostat and <topic>_boiler, alternating with the Write-Data value -> visible 100->0->100 flap in HA when bSeparateSources is enabled. Fix restores the documented ADR-066 intent; no new decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Predicate at MQTTstuff.ino:1212 is replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
- [x] #2 Comment block immediately above the gate is updated so a future reader can see why OTdata.type (not rsptype) is the right field
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 Prerelease bump committed alongside the firmware change via bin/bump-prerelease.sh
- [x] #6 Field-validation note in Final Summary: with bSeparateSources=true, msgid 14 and 16 on _thermostat/_boiler no longer flap to 0 between Write-Data frames (tester sign-off via Discord; leave blocking AC if not yet confirmed)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix predicate at MQTTstuff.ino:1212 to use OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue.
2. Update preceding comment block to call out the OTGW_response_type vs OpenThermMessageType enum-family distinction so a future reader sees why OTdata.type is the right field.
3. Run bin/bump-prerelease.sh; stage version.h + data/version.hash alongside MQTTstuff.ino.
4. python build.py --firmware (exit 0).
5. python evaluate.py --quick (no new failures).
6. Commit (adr-judge + bump-check pass), push to origin/dev (auto-authorised by policy).
7. Check ACs 1-5; leave AC #6 unchecked (Discord field-validation gate); add Final Summary; leave task In Progress per CLAUDE.md autonomous-completion exception.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo, in the dev maintainer's home dir). **Field-validation gate** — implementation already shipped on dev as beta.25+5153537. AC #6 awaits Discord confirmation that msgid 14/16 stop flapping with bSeparateSources=true on a real boiler. Sibling: 2.0.0 TASK-562.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed ADR-066 source-topic Write-Ack gate; restored documented intent. No new ADR required.

## Root cause

`publishToSourceTopic()` in `src/OTGW-firmware/MQTTstuff.ino:1212` compared `rsptype` (`OTGW_response_type`, values 0..5) against `OT_WRITE_ACK` (`OpenThermMessageType`, value B101=5). The two enum families collide numerically — `rsptype == OT_WRITE_ACK` evaluates true only when `rsptype == OTGW_UNDEF`, never for a real boiler Write-Ack frame. The gate therefore never suppressed Write-Ack publication on the source-separated subtopics, and for the six MsgIDs flagged `bSlaveEchoesValue=false` (14, 16, 23, 24, 37, 98) the per-spec-undefined Ack data byte (~0) was published to `<topic>_thermostat` and `<topic>_boiler`, alternating with the Write-Data value. With `bSeparateSources=true` HA saw a 100→0→100→0 flap. The canonical (non-source-separated) topic was unaffected because `is_value_valid_for_master_topic()` correctly checks `OT.type == OT_WRITE_DATA`.

Beta.23 field report from Andre on msgid 14 (MaxRelModLevelSetting) matches this signature exactly.

## Fix

Replaced the gate predicate with one that uses the correct enum family and additionally constrains it to real boiler frames:

```cpp
if (OTdata.type == OT_WRITE_ACK
    && rsptype == OTGW_BOILER
    && !OTlookupitem.bSlaveEchoesValue) return;
```

`OTdata` is the canonical OpenThermMessageType source. Constraining `rsptype == OTGW_BOILER` ensures the gate fires only on real boiler frames (B), never on gateway-faked Answer-Thermostat frames (A) where the value is deliberately constructed and must be published.

The comment block above the gate now explains the `OTGW_response_type` vs `OpenThermMessageType` distinction so a future reader does not retrip on the same numeric collision.

## Files changed

- `src/OTGW-firmware/MQTTstuff.ino` — predicate + comment block (the fix).
- `src/OTGW-firmware/version.h`, `src/OTGW-firmware/data/version.hash` — prerelease bump beta.24 → beta.25.
- 22 other files under `src/OTGW-firmware/**` — version-string banner sweep performed by `bin/bump-prerelease.sh`.

No signature changes, no public API impact, no settings changes. Single commit; covers 26 staged files.

## Verification

- `./build.sh --firmware` — exit 0; artifact `OTGW-firmware-1.5.0-beta.25+5153537.ino.bin` (0.70 MB).
- `python3 evaluate.py --quick` — 31 passed / 2 warnings / 1 failed; baseline (before fix) was identical, so **no new failures**. The 1 fail / 2 warns are pre-existing project-wide noise unrelated to MQTT.
- Pre-commit gates: adr-judge clean (0 violations, 56 advisory llm_judge entries — informational only); prerelease bump-check passed (beta.24 → beta.25 confirmed in `git diff --cached`).

## Commit & push

- Commit: `afdc6480` on `dev` — `fix(mqtt): correct ADR-066 Write-Ack gate enum-family bug (TASK-561)`.
- Pushed to `origin/dev`: `137706c0..afdc6480`. Per project push policy, dev is auto-authorised once build + evaluator are green.

## Risk / regressions

Behavioural change is strictly narrower than the (broken) original: the gate now actually suppresses Write-Ack publication on source subtopics for the six flagged MsgIDs, and only on real boiler frames. Other code paths (canonical topic, gateway-faked answers, Read-Ack frames, the five MsgIDs where `bSlaveEchoesValue=true`) are unaffected. Change scope: one predicate.

## Field validation (AC #6 — blocking)

AC #6 requires Discord tester sign-off that with `bSeparateSources=true` msgids 14 and 16 on `_thermostat`/`_boiler` no longer flap to 0 between Write-Data frames on beta.25. This cannot be self-verified — it requires hardware-in-the-loop testing on a live OTGW with HA. Per the CLAUDE.md "Autonomous task completion" exception ("hardware-specific tester feedback"), the task remains at **In Progress** until a tester confirms the flap is gone on beta.25 in Discord `#beta-testing`. ACs #1-5 are checked; AC #6 remains the documented blocking AC.

Field validation 2026-05-07 (Andre, dev beta.25+5153537): the 6 flagged msgids (14, 16, 23, 24, 37, 98) confirmed stable in HA — values no longer flap to 0 between Write-Data frames with bSeparateSources=true. (Recorded in TASK-571 description; AC #6 closed.)
<!-- SECTION:FINAL_SUMMARY:END -->
