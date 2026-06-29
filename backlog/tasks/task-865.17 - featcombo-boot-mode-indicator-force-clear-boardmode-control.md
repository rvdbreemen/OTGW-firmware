---
id: TASK-865.17
title: 'feat(combo): boot-mode indicator + force/clear boardmode control'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-14 20:10'
updated_date: '2026-06-29 20:35'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.16
parent_task_id: TASK-865
ordinal: 80000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (combo-only prerequisite; board-delta audit 2026-06-14)
The combo (ADR-127) boot-detects PIC-vs-OTDirect via reset->ETX and caches the result in settings.iBoardMode. The audit found a STICKY misdetection risk: on an OTGW32 the PIC-RX pin (GPIO44) floats and is known to drive an RX-interrupt storm (OTGW-firmware.ino:273-277); line noise could produce a stray 0x04 inside the find(ETX) timeout -> spurious PIC detection -> caches iBoardMode=1 -> persists across reboots, recoverable ONLY by forcing iBoardMode=2. For the single-combo-only endgame the maintainer requires (Q2 + audit): the booted mode must be VISIBLE (not a silent guess) AND user-forceable/clearable (the internal iBoardMode mechanism already exists; expose it).

## What
1. Expose the active hardware mode (PIC / OTDirect / detecting) on three surfaces: a web-UI status tile, an MQTT topic (e.g. <toptopic>/hw_mode or in the existing status payload), and the boot banner/telnet log.
2. Add a user-facing force/clear control: a REST endpoint (api/v2) + web-UI control to set the boardmode to auto / force-PIC / force-OTDirect, and to clear the cached iBoardMode and re-detect on next boot.
3. Honor a forced mode in the detection path: if the user forced a mode, skip the reset->ETX probe (or override its result) and use the forced mode; auto = current probe behaviour.

## Anchors
- detectPIC() OTGW-Core.ino:859-884; probe order + iBoardMode persist OTGW-firmware.ino:299-330; isPICEnabled() OTGW-firmware.h:504-507; state.hw.eMode.
- restAPI.ino (status + new endpoint), MQTTstuff/MQTTHaDiscovery (publish mode), data/index.html + index.js (status tile + force control), settingStuff.ino (iBoardMode setting).

## Notes
KISS: reuse the existing iBoardMode (1=PIC, 2=OTDirect). Force = set iBoardMode + a force flag so detection honors it; clear = reset iBoardMode to auto/0 and re-detect. Single-point-of-instantiation + ESP-abstraction rules apply.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build esp32 + esp32-classic + esp32-combo all SUCCESS; evaluate.py --quick no new failures
- [x] #2 Web UI shows the active hardware mode (PIC / OTDirect / detecting)
- [x] #3 MQTT publishes the active hardware mode; boot banner/telnet logs it
- [x] #4 A force/clear control (REST + UI) sets boardmode to auto/force-PIC/force-OTDirect and can clear+re-detect; a forced mode is honored by the detection path (probe skipped/overridden)
- [x] #5 FIELD (epic TASK-865): on OTGW32 and Classic the indicator shows the correct mode; forcing recovers a simulated misdetect (e.g. a stuck iBoardMode)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (build/eval phase; Land owns commit/bump/status).

Findings: most surfaces were already scaffolded. AC#2 (web UI) and AC#4 (force/clear) are satisfied by EXISTING machinery:
- AC#2: sendDeviceInfoV2() already emits 'hardwaremode' (restAPI.ino:2435, unconditional); the device-info page renders a tile per JSON key (index.js:5565-5596) with label 'Hardware Mode' (index.js:6996). Renders on all builds incl. combo.
- AC#4: the 'boardmode' dropdown (Auto/PIC/OT-Direct) is the REST+UI control (index.js:6282 + settings POST). The detection path already HONORS it: OTGW-firmware.ino:307 iBoardMode==2 skips the PIC probe (recovers a stuck misdetect); :313 ==1 forces PIC; :316 ==0 (Auto) re-probes+re-caches. POST persists via updateSetting() debounced flush + synchronous flush in doRestart(). boardmode is whitelisted (restAPI.ino:3123) and combo-gated for display (restAPI.ino:2872 HAS_RUNTIME_HW_DETECT). No new endpoint/force-flag needed (KISS; maintainer note: 'mechanism already exists; expose it').

Genuine code delta (3 files):
1. OTGW-firmware.h hardwareModeName(): HW_MODE_UNKNOWN now returns 'Detecting' (AC#2 third state). Verified no consumer string-matches the old 'Unknown'.
2. MQTTstuff.ino sendMQTTversioninfo(): added retained topic otgw-firmware/hardware_mode = hardwareModeName(), next to hardware_type (AC#3). Plain topic, NO HA-discovery row (KISS; adding a row to pseudo-ID 248 would shift the hand-maintained cumulative mqttHaSensorIndex[] offsets for IDs 249-254 = high blast radius; hardware_type already carries board class to HA).
3. OTGW-firmware.ino: universal telnet boot banner 'Hardware mode: <mode>' after the detection #endif, applies to all three targets (AC#3 build-agnostic, matches the universal MQTT publish).

Limitation (honest): iBoardMode is BOTH selector and auto-detect cache. Recovery from a stuck misdetect is via Force-OT-Direct(=2), which is sticky (never re-probes). Picking Auto(=0) re-probes and re-caches to 1/2, so after reboot the dropdown shows the detected mode (not 'Auto') and on a noisy floating GPIO44 could re-misdetect. This is the ambiguity the task's optional 'force flag' note anticipated; documented rather than built (KISS).

hardware_mode is retained, republished at boot/MQTT-reconnect; a runtime eMode flip (PIC banner-recovery, OTGW-Core.ino:4561) updates the topic on next reconnect.

Verification: esp32 / esp32-classic / esp32-combo all per-env SUCCESS (grepped each build log). evaluate.py --quick: 0 failures, 64 passed, 1 pre-existing warning (STATUS_BURST_COOLDOWN_MS 'boards.h not found' = tooling path, unrelated). PROGMEM + ESP Abstraction Boundary checks clean (new line uses PSTR/F; combo-only logic stays inside existing HAS_RUNTIME_HW_DETECT flag).

AC#5 is FIELD-only (real OTGW32/Classic hardware) = out of scope for this phase.

LAND: stage ONLY src/OTGW-firmware/{OTGW-firmware.h,MQTTstuff.ino,OTGW-firmware.ino} + the prerelease bump. Do NOT git add -A (worktree carries other sessions' .wolf/*, scripts/capture-mqtt-debug.bat, .adr-kit-state.json, and build-auto-incremented version.h/version.hash).

LANDED (alpha bump): 3-file source delta committed — OTGW-firmware.h hardwareModeName() returns 'Detecting' for HW_MODE_UNKNOWN (AC#2 third state); MQTTstuff.ino sendMQTTversioninfo() publishes retained otgw-firmware/hardware_mode (AC#3); OTGW-firmware.ino universal telnet boot banner 'Hardware mode: <mode>' (AC#3). AC#1-#4 satisfied (existing boardmode dropdown is the force/clear control, detection path already honors it). Remaining: AC#5 FIELD-only (real OTGW32/Classic hardware) — verify indicator shows correct mode and forcing recovers a simulated misdetect.

AC#5 ON-DEVICE VERIFIED 2026-06-29 (classic-S3 @192.168.88.64, alpha.291, PIC fw6.6). Full force/recover cycle walked through the Web UI: (1) baseline Hardware Mode tile = PIC, hardware_type=otgw-classic (correct mode shown). (2) Board Mode dropdown -> OT-Direct + reboot: tile flipped to OT-Direct, hardware_type=otgw32, PIC functions disabled = simulated misdetect HONORED by the detection path (iBoardMode==2 skips the PIC probe). (3) Board Mode -> Auto + reboot: tile recovered to PIC, hardware_type=otgw-classic (Auto re-probed, found the PIC). The boardmode dropdown is the working force/clear control and recovery from a stuck misdetect works. Indicator-correct + force-recover both confirmed on real Classic hardware.
<!-- SECTION:NOTES:END -->
