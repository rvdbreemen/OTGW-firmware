---
id: TASK-1174
title: >-
  Dev drops MsgID 56 (TdhwSet) and 57 (MaxTSet) once an OpenTherm v4.x version
  is seen: restore reserved-ID range 50-55/58-69
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 08:45'
updated_date: '2026-10-06 11:56'
labels:
  - bug
  - opentherm
  - regression
dependencies: []
priority: medium
ordinal: 297000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed defect: on dev (2.0.0), once an OpenTherm version >= 4.0 has been decoded, every live frame for MsgID 56 (TdhwSet) and 57 (MaxTSet) is logged as "Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 5x ignored)" and dropped.

OT v4.2 defines both as R/W data-ids: docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:2061-2062 and :2383-2384 ("56 R W TdhwSet f8.8", "57 R W MaxTSet f8.8"). The spec says at :2523 "All data id's not defined above are reserved for future use", and its table jumps from 57 to 70. The reserved legacy set is therefore 50-55 and 58-69.

MECHANISM (dev HEAD 3621a3830; origin/dev is identical at this line), all in src/OTGW-firmware/OTGW-Core.ino:
- :919-922 isLegacyPreV42CompatibilityId() is `return (msgid >= 50U && msgid <= 63U);` (:921), so 56 and 57 are included.
- :924-936 useV4xReservedIdRules(): in AUTO mode it returns `(OTcurrentSystemState.OpenThermVersionSlave >= 4.0f) || (OTcurrentSystemState.OpenThermVersionMaster >= 4.0f)` (:933-934). gOTSpecCompatMode is assigned only at its declaration (:916, OT_SPEC_COMPAT_AUTO) and has no setter, so users cannot opt out.
- :938-941 isMsgIdReservedInActiveProfile() ANDs the two.
- Gate sites:
  - is_value_valid() :1739 and is_value_valid_for_master_topic() :1772: `if (isMsgIdReservedInActiveProfile(OT.id)) return false;`
  - decodeAndPublishOTValue() :4628-4635 logs the "Reserved ..." line and returns before `case OT_TdhwSet: print_f88(OTcurrentSystemState.TdhwSet)` / `case OT_MaxTSet: print_f88(OTcurrentSystemState.MaxTSet)` (:4525-4526) can run.

TRIGGER: default settings, every dev target. PIC frames and OTDirect frames both decode through processOT.
- The gate follows the last valid MsgID 125 Read-Ack or MsgID 124 Write-Data value. print_f88 overwrites the version fields (:4489-4490, state write at :2647), so a v4.x thermostat or boiler keeps the gate on for the whole uptime. A reboot clears it.
- On OTGW32, OTDirect polls 125 itself (OTDirect.ino:249) and writes 56/57 for SW=/SH= (OTDirect.ino:139-140), so a v4.x boiler trips the gate there too.
- Frames decoded before the first version >= 4.0 still update.
- The PS=1 summary writer (OTGW-Core.ino:4211-4212) has no gate, but PS=1 is off by default (OTBustypes.h:26).

IMPACT while the gate is active:
- OTcurrentSystemState.TdhwSet/MaxTSet keep the 0.0 boot default, or freeze at their last pre-gate value.
- No MQTT publish of TdhwSet/MaxTSet, canonical or source subtopics.
- No setMsgLastUpdated (:5097-5099), so GET /api/v2/otgw/otmonitor omits dhwsetpoint and maxchwatersetpoint (restAPI.ino:2954-2955).
- HA discovery for 56/57 is never queued (:5104-5108).
- The HA DHW climate entity reads its target temperature from <pub>/TdhwSet (MQTTHaDiscovery.cpp:3015-3019), so it gets none.
- SW=/SH= writes still reach the boiler, but their frames never reach state or MQTT.
- SAT reads MaxTSet < 30 and falls back to SAT_MAX_SETPOINT_DEFAULT 75 C (SATcontrol.ino:4558-4559, :57). It ignores the boiler's own maximum; the system caps and the 65 C global maximum (:64) still apply.
- No crash, and no safety clamp is lost.

EVIDENCE (checked read-only, 2026-09-30):
1. Live 2.0.0 alpha.288 capture: PIC 6.6, "otgwsimulation":false at transcript :2535. File: OTGW-logs/s3-mini-alpha288/20260629-075953/transcript-20260629-075953-2.0.0-alpha.288+b11d255-(29-06-2026)-OTGW-otgw-AC276ECBEB50.txt
   - :596 "B407D0400 125 Read-Ack > OpenThermVersionSlave = 4.00"
   - :333 "BC0395300 57 Read-Ack Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 57 ignored)" (the boiler reports MaxTSet 83 C)
   - :1109 "BC0383700 56 Read-Ack Reserved ... ID 56 ignored" (TdhwSet 55 C)
   - The file has 26 such lines and zero TdhwSet/MaxTSet decode lines.
2. The dev coverage baseline scripts/tests/baseline_coverage.json records the bug as the expected output. It was recorded in ca55863bb on "the bench S3 (Classic + PIC 6.6, alpha.354+a7e06f8)".
   - All 15 MsgID 56/57 keys render "Reserved ... ignored", e.g. :1384 "B|Read-Ack|56".
   - There is no TdhwSet or MaxTSet MQTT topic.
   - On the same fixture, the 1.x baseline (wt-otgw-1.x.x/scripts/tests/baseline_coverage.json:1852-1860) renders "TdhwSet = 43.00 C" / "MaxTSet = 75.00 C" and publishes TdhwSet and MaxTSet plus the _boiler/_thermostat source topics.
3. TASK-1071, task file line 77 (OTGW32 bench, alpha.371): 56/57 decoded in loop 1 after a fresh flash, then flipped to "Reserved" after the replayed "OpenThermVersionSlave = 4.00". The note reads this as "AUTO profile works as designed"; it is this regression.

HISTORY (git log -L on the function):
- 83da1aed1 introduced 50..63.
- 818d11817 (#538, "Fix MaxTSet and TdhwSet suppressed in OpenTherm v4.x mode (HA shows 0°C)") and 3edac84c3 (#540, extends the range to 58-69) fixed it, both 2026-04-07. Both are ancestors of dev.
- Dev commit 7ef9608eb ("feat: update version to 1.4.0-beta across all relevant files"; authored 2026-03-31, committed 2026-04-13, single parent 4dc2ec012) replaced the fixed body with `(msgid >= 50U && msgid <= 63U)` and deleted the explanatory comment. No later commit touches the line.
- The dates suggest a stale working tree was committed on top of the fixes. That is inference; nothing records it.
- The same revert also dropped 64-69 from the reserved set. Those IDs are OT_UNDEF (OTGW-Core.h:438-443), so on dev they log "Unknown message [6x]" instead of "Reserved ... ignored". Only the log text differs.

WHY NOTHING CAUGHT IT:
- tools/opentherm_v42_spec_audit.py:277-284 only string-checks that the guard exists, not which IDs it covers. Its own expected range at :361-362 is correct.
- The coverage baseline recorded the buggy output as expected.

DOCS ARE INCONSISTENT:
- docs/BREAKING_CHANGES.md:149 and docs/fixes/opentherm-v42-mqtt-breaking-changes.md (:9, :24-25, :55, :84, :102, :128) say 50-55 and 58-63.
- The spec rule, 1.x and docs/reviews/2026-04-07_opentherm-spec-deep-audit/AUDIT_REPORT_EN.md:283 say 50-55 and 58-69.
- The dev code comment at OTGW-Core.ino:915 says "notably IDs 50-63".

1.x STATUS: not affected.
- wt-otgw-1.x.x (otgw-1.x.x HEAD 8ba6f7ef9) and origin/otgw-1.x.x have OTGW-Core.ino:496 `return (msgid >= 50U && msgid <= 55U) || (msgid >= 58U && msgid <= 69U);`, with the explanatory comment at :482-493.
- 7ef9608eb is not an ancestor of otgw-1.x.x.
- The fix is dev-only, so no cross-line master plan is needed.
- No 2.0.0 field report is known. The alpha.288 capture is the real-world evidence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Code: in src/OTGW-firmware/OTGW-Core.ino, isLegacyPreV42CompatibilityId() returns true for exactly MsgIDs 50-55 and 58-69 and false for 56 and 57, matching otgw-1.x.x OTGW-Core.ino:496 `return (msgid >= 50U && msgid <= 55U) || (msgid >= 58U && msgid <= 69U);`. The profile comment (currently :913-915, 'notably IDs 50-63') and the function header state that 56 (TdhwSet) and 57 (MaxTSet) are valid in OpenTherm v4.2 and not reserved. Nothing else in the reserved-ID gate changes (:924-941, :1739, :1772, :4628-4635).
- [x] #2 Host regression test, old vs fix, no hardware: a stdlib test under tests/ reads the real OTGW-Core.ino text and does NOT re-implement the predicate. It extracts isLegacyPreV42CompatibilityId / useV4xReservedIdRules / isMsgIdReservedInActiveProfile verbatim and either compiles them with a host C++ compiler against a stub OTcurrentSystemState, or mechanically evaluates the extracted return expression for msgid 0..255. It asserts this matrix: with both versions 0.0, no ID is reserved; with OpenThermVersionSlave = 4.0, and separately OpenThermVersionMaster = 4.2, IDs 48, 49, 56, 57, 70, 124 and 125 are NOT reserved and IDs 50-55 and 58-69 ARE reserved. The task notes include the pasted output for three inputs: `git show 3621a3830:src/OTGW-firmware/OTGW-Core.ino` FAILS (on 56, 57 and 64-69), the fixed file PASSES, and wt-otgw-1.x.x/src/OTGW-firmware/OTGW-Core.ino PASSES.
- [x] #3 Bench reproduction, old vs fix on the same rig and replay: use the bench S3 Classic + PIC, the rig that recorded scripts/tests/baseline_coverage.json. Replay scripts/tests/otgw_simulation_coverage.log with scripts/tests/run_coverage_test.py (telnet plus broker subscription). OLD is the current dev build and FIX is the same tree plus only this change; flash both app-only and start both runs the same way, right after the flash reboot. Look at the MsgID 56/57 frames decoded after the fixture's MsgID 125 = 4.00 frame (fixture :231), i.e. replay loop 2 onward. In the OLD telnet transcript they show 'Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 56 ignored)' / '... ID 57 ignored'. In the FIX transcript they show the TdhwSet/MaxTSet decode (for B Read-Ack frames, 'TdhwSet = <value>' / 'MaxTSet = <value>') and no 'Reserved' line for 56 or 57. Both transcripts are saved as transcript-<host>-<id>-<hw>-<datetime>.txt and cited in the Final Summary.
- [x] #4 End-to-end on FIX after the version frame has been seen, in the same bench run: the broker receives the canonical value topics TdhwSet and MaxTSet. GET /api/v2/otgw/otmonitor lists dhwsetpoint and maxchwatersetpoint, and their epoch advances between two reads taken one replay loop apart. On OLD, neither topic is published after the version frame, and the otmonitor entries are absent or their epoch is frozen. Source subtopics (_boiler/_thermostat) are NOT required: the dev baseline has none for any ID.
- [x] #5 The coverage-gate drift is exactly the expected set. Running run_coverage_test.py on FIX against the committed baseline reports:
- CHANGED on exactly the 15 MsgID 56/57 ot keys (decodes instead of 'Reserved');
- CHANGED on the 12 MsgID 64-69 ot keys (B|Read-Ack|64..69 and T|Read-Data|64..69, from 'Unknown message [6x] ...' to 'Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 6x ignored)');
- NEW MQTT topics TdhwSet and MaxTSet;
- and nothing else.
Any other drift is explained in the task notes before recording. The 1.x renderings in wt-otgw-1.x.x/scripts/tests/baseline_coverage.json (:1214-1223, :1852-1860, :2572-2574, :3372-3374) serve as a reference only, not as required dev strings.
- [x] #6 The baseline is re-recorded and deterministic. Re-record scripts/tests/baseline_coverage.json with --record on FIX, on the same bench, after the drift above has been reviewed. A later gate run on FIX then PASSES in two runs: one from a fresh boot (started right after the flash reboot) and one on a warm device that has already decoded MsgID 125. The fixture layout supports this: all 15 MsgID 56/57 frames sit at :126-140, and every 50-55/58-69 frame sits at :301-342, after the version frame at :231.
- [x] #7 Docs match the code: docs/BREAKING_CHANGES.md:149 and docs/fixes/opentherm-v42-mqtt-breaking-changes.md (:9, :24-25, :55, :84, :102, :128) state the suppressed legacy IDs as 50-55 and 58-69, with 56/57 valid. `grep -n "58-63"` over those two files returns nothing, and `grep -n "50-63"` over src/OTGW-firmware/OTGW-Core.ino returns nothing.
- [x] #8 TASK-1071 is corrected with `backlog task edit 1071 --append-notes`. The note records that the 15 CHANGED MsgID 56/57 keys in its 2026-09-23 OTGW32 run (task file line 77) were this regression, not 'AUTO profile works as designed', and references this task.
- [x] #9 Release hygiene:
- The change ships under its own prerelease tag via bin/bump-prerelease.sh, in the same commit.
- build.bat (esp32-combo) reports SUCCESS with fresh firmware.bin and littlefs.bin timestamps.
- python evaluate.py --quick shows no new FAIL.
- The commit title is descriptive, not a task ID.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from the backlog triage (workflow wf_1c568508-5f2) after an adversarial verification (workflow wf_4f2df5e8-285): 3 independent verifiers (trace / refute / impact) all voted real; judge severity medium.

FIX PLAN:
1. src/OTGW-firmware/OTGW-Core.ino:913-922. Restore the 1.x text, which reverts the 7ef9608eb hunk:
- Profile comment: "then applies v4.x reserved-ID rules (IDs 50-55 and 58-69). Note: IDs 56 (TdhwSet) and 57 (MaxTSet) are valid in OpenTherm v4.2 and are NOT reserved".
- Function header: copy it from wt-otgw-1.x.x OTGW-Core.ino:490-493.
- Body: `return (msgid >= 50U && msgid <= 55U) || (msgid >= 58U && msgid <= 69U);`.

Nothing else in the gate changes. 7ef9608eb also removed the WRITE_ACK clauses, but those are already back (:1742-1743), and :1739, :1772 and :4628-4635 stay as they are. The change adds no strings (so PROGMEM does not apply), no String, no platform #ifdef and no new shared state; the predicate is only evaluated inside processOT.

Why the exact 1.x range and not 50-55/58-63:
- It is the spec's reserved set (spec :2523 "All data id's not defined above are reserved"; the table jumps from 57 to 70).
- It restores parity with 1.x and with the 1.x baseline.
- The extra 64-69 change only affects log text, because those IDs are OT_UNDEF (OTGW-Core.h:438-443).

2. Add the host regression test (AC 2) under tests/, reading the real source text. Two things let this regression slip in through a version-bump commit: tools/opentherm_v42_spec_audit.py:277-284 only checks that the guard exists, and the coverage baseline recorded the buggy output as expected. This test closes that gap cheaply. Optionally, the audit tool could also parse the range, since its expected range at :361-362 is already correct; that is noted here, not in scope.

3. Docs: in docs/BREAKING_CHANGES.md:149 and docs/fixes/opentherm-v42-mqtt-breaking-changes.md (:9, :24-25, :55, :84, :102, :128), change 58-63 to 58-69. Keep the existing statement that 56/57 are valid.

4. Run bin/bump-prerelease.sh, build.bat (esp32-combo) and python evaluate.py --quick.

5. Bench old vs fix, following the repro plan. Review the drift, then re-record scripts/tests/baseline_coverage.json on FIX and re-run the gate from a fresh boot and from a warm device.

6. Append the correction note to TASK-1071 with the backlog CLI.

7. Make a single commit on dev containing the change and the bump. Push to origin/dev per policy once build and evaluator are green. The fix is dev-only: otgw-1.x.x already has it (OTGW-Core.ino:496), so no port or cross-line plan is needed.

REPRO PLAN:
Nothing below was executed for this ruling: the session was read-only and a bench test is running.

A. HOST, no hardware (the fastest proof, AC 2): run the new tests/ check against three inputs.
- `git show 3621a3830:src/OTGW-firmware/OTGW-Core.ino`: expect FAIL. With slave = 4.0, 56 and 57 are reserved, and 64-69 are not.
- The fixed file: expect PASS.
- wt-otgw-1.x.x/src/OTGW-firmware/OTGW-Core.ino: expect PASS.
This proves the predicate only. Step B closes the gap to state, REST and MQTT.

B. BENCH: bench S3 Classic + PIC (the Classic-S3 bench board), esp32-combo or esp32-classic, with the MQTT broker reachable. Do not touch the PIC itself.
1. Save settings: curl http://<ip>/api/v2/settings.
2. OLD run:
   - Flash the current dev build app-only: flash_otgw.bat --update --app <bin>. This keeps settings, and the reboot clears the RAM-held version state.
   - Right after boot, run python scripts/tests/run_coverage_test.py --host <ip> (at least 694 s, telnet plus broker).
   - Expect: if no version frame has been decoded since boot, loop 1 renders 56/57 as decodes. From loop 2 on they render "Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 5x ignored)". This is the pattern already seen in TASK-1071 line 77.
   - Expect no TdhwSet/MaxTSet publish after the first "OpenThermVersionSlave = 4.00".
   - Expect GET /api/v2/otgw/otmonitor to lack dhwsetpoint/maxchwatersetpoint, or to show a frozen epoch.
3. FIX run: flash the fixed build app-only and repeat the same procedure. Expect:
   - no "Reserved" line for 56 or 57 in any loop;
   - TdhwSet/MaxTSet decode lines in every loop;
   - canonical TdhwSet and MaxTSet topics on the broker;
   - otmonitor epochs that advance;
   - gate drift of exactly 15 CHANGED (56/57) + 12 CHANGED (64-69) + NEW TdhwSet/MaxTSet topics, and nothing else.
4. Record the baseline with --record on FIX. Then rerun the gate on FIX twice, from a fresh boot and on a warm device; both must PASS.

C. OPTIONAL field confirmation: on a system with a v4.x boiler, the alpha.288 pattern changes as follows.
- Transcript :333 "BC0395300 57 Read-Ack Reserved ..." becomes "BC0395300 57 Read-Ack > MaxTSet = 83.00".
- Transcript :1109 "BC0383700 56 Read-Ack Reserved ..." becomes "... > TdhwSet = 55.00".

D. OPTIONAL OTDirect: OTGW32 replay has worked since alpha.371 (TASK-1071, In Review), and the same fixture shows the same loop-1 vs loop-2 flip there. It is not required: both producers share processOT, and the host test covers the predicate.

DISSENT / CORRECTIONS:
No verdict refutes the bug: 3 of 3 say real, all with high confidence. I re-checked every decisive line myself: the range at :921, no setter for gOTSpecCompatMode, the three gate sites, ancestry of 7ef9608eb and of both fixes, the spec rows, both baselines and the live alpha.288 capture. None of it refutes the bug.

The verifiers differ only on narrowings, which are folded into the task:
1. "Never update" is too strong. Frames decoded before the first version >= 4.0 still update. The gate also does not latch: it follows the last valid 124/125 value, because print_f88 overwrites it. A v4.x device keeps it on for the whole uptime, and a reboot clears it.
2. The PS=1 summary writer (:4211-4212) has no gate, but PS=1 is off by default.
3. The impact verifier says the 1.x field report behind #538 (GH #445) came from an OT v3.00 system and was attributed to pyotgw, so it is not evidence for this path. I did not re-read #445 myself. No 2.0.0 field report is known; the alpha.288 capture is the real-world evidence.
4. Docs. One verifier said no doc change is needed, but docs/BREAKING_CHANGES.md:149 and docs/fixes/opentherm-v42-mqtt-breaking-changes.md say 58-63, while the spec rule (:2523), 1.x and the dev audit report say 58-69. The doc correction is therefore included.
5. The 64-69 half of the revert changes log text only, because those IDs are OT_UNDEF. That is 12 baseline keys, confirmed by grep.
6. Reading 7ef9608eb as an accidental stale-tree commit is inference from its author and commit dates only.

2026-09-30 implementation + host evidence (alpha.378).
- AC#1: OTGW-Core.ino isLegacyPreV42CompatibilityId() now returns (50..55) || (58..69), identical to otgw-1.x.x OTGW-Core.ino:496; the profile comment says 50-55 and 58-69, and a function header cites OT spec v4.2 (48, 49, 56, 57, then 70). No other line of the reserved-ID gate changed.
- AC#2: test/host/test_ot_reserved_range.py slices the enum, gOTSpecCompatMode and the three functions from OTGW-Core.ino by anchor (not re-implemented), compiles them with MSVC against a stub OTcurrentSystemState and checks all ids 0..255 for versions 3.00, slave 4.00 and master 4.20 against the spec set (so 48, 49, 56, 57, 70, 124, 125 free and 50-55/58-69 reserved under v4.x). The harness lives in test/host/ next to the existing MSVC slicing harness, not in tests/.
  * --rev 3621a3830: FAIL (16 mismatches: 56(reserved) 57(reserved) 64-69(free) for both 4.x inputs); 'MsgID 56 TdhwSet SUPPRESSED, MsgID 57 MaxTSet SUPPRESSED'.
  * working tree (fix): PASS (0 mismatches); 'MsgID 56 TdhwSet decoded, MsgID 57 MaxTSet decoded'.
  * --file wt-otgw-1.x.x/src/OTGW-firmware/OTGW-Core.ino: PASS (0 mismatches).
  Full output: %LOCALAPPDATA%/OTGW-capture/task1174-host.txt.
- AC#7: docs/BREAKING_CHANGES.md (2x) and docs/fixes/opentherm-v42-mqtt-breaking-changes.md (7x) now say 58-69; grep '58-63' over both returns nothing, grep '50-63' over OTGW-Core.ino returns nothing.
- AC#8: TASK-1071 note appended.
- AC#9: bin/bump-prerelease.sh alpha.377 -> alpha.378 in the same commit; build.bat --target esp32-combo [SUCCESS] firmware (194.9 s) and filesystem, fresh .pio firmware.bin 10:55:18 / littlefs.bin 10:55:47 (alpha.378+e421570); evaluate.py --quick 69 passed, 0 warnings, 0 failed.
OPEN: AC#3-#6 need the bench replay (S3 Classic + PIC rig per AC#3; the OTGW32 can replay too since TASK-1071, but the committed baseline was recorded on the Classic rig).

2026-10-06: maintainer chose to run AC#3-#6 on the OTGW32 (COM4) instead of the Classic-S3 PIC rig, which is not attached; the baseline therefore moves to the OTGW32. Method on that rig: OLD = dev HEAD fa2b89d with only the predicate reverted to 'msgid >= 50U && msgid <= 63U' (built with pio, image task1174/firmware_old.bin); FIX = dev HEAD fa2b89d as is (build.bat, alpha.418+fa2b89d). Both app-only, both runs started right after a reboot, run_coverage_test.py --seconds 820 (the default 694 s window gave 1.94 loops once, below the runner's 2-loop floor).
AC#3: OLD record run (task1174/run-old-record.log): after the version frame the 56/57 frames log 'Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 56 ignored)' / '... ID 57 ignored' (11 such lines); loop 1 before MsgID 125 still decodes them. FIX run (task1174/run-fix-vs-old.log): every 56/57 frame decodes ('TdhwSet = 43.00 C', 'MaxTSet = 75.00 C'), no Reserved line for 56/57.
AC#4: FIX, two reads of /api/v2/otgw/otmonitor one replay loop apart, both after the version frame: 12:16:22 dhwsetpoint 43 C epoch 469, maxchwatersetpoint 75 C epoch 475; 12:19:25 epochs 787 / 793 (+318 s, one loop). Broker: TdhwSet and MaxTSet topics present. OLD after its run: both otmonitor entries ABSENT. On OLD the TdhwSet/MaxTSet topics are also present, published in loop 1 before the version frame, so the 'neither topic after the version frame' check rests on the telnet decode and otmonitor, not on topic presence.
AC#5: FIX gated against the OLD baseline recorded on the same rig (task1174/baseline_old_otgw32.json): 33 differences = 15 CHANGED MsgID 56/57 keys (Reserved line dropped, decode kept; exactly the expected 15) + 12 CHANGED MsgID 64-69 keys (B|Read-Ack and T|Read-Data 64..69, 'Unknown message [6x] ...' -> 'Reserved in OpenTherm v4.x profile (legacy pre-v4.2 ID 6x ignored)'; exactly the expected 12) + 6 capture artefacts, explained: B|Read-Ack|204, B|Read-Ack|87, B|Unknown-Data-Id|240 and T|Read-Data|62 changed only because the OLD capture holds a second, truncated copy of the same line ('f8.8 [', 'LB=', 'legacy pre-v'); B|Read-Ack|29 and T|Read-Data|30 are MISSING because their frames do not appear at all in the FIX telnet capture (0 lines) while OLD has them: telnet loss, IDs unrelated to the 50-69 predicate. The expected 'NEW MQTT topics TdhwSet and MaxTSet' do not show as drift on this rig because OLD already publishes them in loop 1 from a fresh boot.

2026-10-06 AC#6 on the OTGW32 (FIX = alpha.418+fa2b89d):
- Recorded with --record from a fresh boot (run-fix-record.log, 2.42 loops). That single capture lost whole frames (MsgID 3/5 keys) and kept one truncated copy (T|Read-Data|43 'f8.8 [0.0'), so a single-run record was not deterministic.
- Tooling made robust (host scripts only, no firmware change): coverage_baseline.drop_truncated_copies() drops a rendering that is a strict prefix of another rendering of the same key (the 42/43/62/87/204/240 artefacts); SPLICE_RX also cuts at 'OTGW-SIM trace:' (a simulator trace spliced into a decode line without its timestamp prefix); set_debug_flag() keeps toggling until an echo confirms the wanted state instead of failing when one echo is lost (3 of 7 runs had died on a lost toggle echo before any capture). 3 new unit tests; test_run_coverage_test.py 12/12 OK.
- Committed baseline = ot union of the two FIX captures run-fix-record + run-fix-gate-warm (369 keys, 143 MsgIDs; mqtt half from the broker-observed record run, 230 topics). Those two captures are not used as evidence below.
- Gate runs (--seconds 1000, ~3 loops), independent captures: FRESH (reboot, 12:57) PASS, 369 keys; WARM (no reboot after it, 13:37) PASS, 369 keys. One earlier warm run (13:17, before the splice fix) failed on 3 telnet artefacts: 2 whole frames lost (A|Unknown-Data-Id|127, T|Read-Data|200) and the 68 splice; none in the 50-69 decode, 68 rendered 'Reserved' correctly. Whole-frame telnet loss remains possible on any run (the ADR-175 limitation: telnet is a lossy source); the longer window makes a full miss less likely.
- satsimulation is false; the 'OTGW-SIM trace' lines are the replay's own trace (SATcontrol.ino:1149).
Artifacts: %LOCALAPPDATA%/OTGW-capture/task1174/ (OLD/FIX images and ELFs, all run logs, baseline_old_otgw32.json, baseline_coverage.classic.json = the previous Classic baseline).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MsgID 56 (TdhwSet) and 57 (MaxTSet) are no longer suppressed once an OpenTherm v4.x version is seen: isLegacyPreV42CompatibilityId() covers 50-55 and 58-69 (as on otgw-1.x.x), with a host regression test that reads the real OTGW-Core.ino. Bench (maintainer chose the OTGW32 rig because the Classic-S3 is not attached; the coverage baseline moved with it): OLD (dev with only the predicate reverted) logs 'Reserved ... ID 56/57 ignored' after the version frame and has no dhwsetpoint/maxchwatersetpoint in otmonitor; FIX decodes every 56/57 frame, and otmonitor shows dhwsetpoint 43 C / maxchwatersetpoint 75 C with epochs advancing 469 -> 787 over one replay loop. FIX gated against the OLD baseline from the same rig: exactly the 15 MsgID 56/57 keys and the 12 MsgID 64-69 keys changed, plus 6 explained telnet artefacts. The new baseline (OTGW32, union of two FIX captures) passes a fresh-boot and a warm gate run. The coverage tooling now ignores truncated duplicate lines and simulator-trace splices and survives a lost toggle echo. Docs, TASK-1071 note and release hygiene were done earlier (alpha.378).
<!-- SECTION:FINAL_SUMMARY:END -->
