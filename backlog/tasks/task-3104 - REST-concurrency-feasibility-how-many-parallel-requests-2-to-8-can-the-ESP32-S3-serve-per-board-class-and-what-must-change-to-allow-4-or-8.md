---
id: TASK-3104
title: >-
  REST concurrency feasibility: how many parallel requests (2 to 8) can the
  ESP32-S3 serve, per board class, and what must change to allow 4 or 8
status: To Do
assignee: []
created_date: '2026-11-26T11:35:00Z'
updated_date: '2026-11-26T11:50:00Z'
labels:
  - research
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1030000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-26 (session, interactive): research what is feasible for 2 to 8 parallel REST requests. Facts from code (origin/dev, T2): base REST_MAX_INFLIGHT=2 (restAPI.ino:44, ADR-165), effective cap restEffectiveInflightCap() restAPI.ino:84-95 (block<16000 gives 1; 16000..23999 gives 2; >=24000 gives restCapBase(), release = 2, prerelease restcap 2..8), static files 2 (restAPI.ino:128-139), send side WEB_MAX_TX_CONNECTIONS 4 (ADR-188), client queue 2 (ADR-184). Steady largest block on the bench unit .63 is 19444 (R1 soak 17396..22516), so the cap can never exceed 2 there; the 24000 threshold is a guess (restAPI.ino:76-82), not measured. Sweep rule of TASK-3091 counts an arm only if N was the effective cap for 95 percent of samples, which cannot happen above N=2 on .63. This task is the study that TASK-3091's sweep depends on.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Measure the steady-state largest free block and free heap of every unit class available (.63 PSRAM, .61 no PSRAM, Classic, any field data/captures with hd_min_max_block) and record the table with sources
- [ ] #2 Measure the contiguous heap each large REST response needs today (settings, info, health, sat/status, file list) and at which N the worst case sum exceeds the steady block per board class; show the arithmetic with measured sizes, not estimates
- [ ] #3 Inventory which responses can be streamed in chunks (JsonEmit, ADR-146) to cut the contiguous need, with the expected block saving per endpoint, and a ranked list of what to change first
- [ ] #4 Re-derive the clamp thresholds (16000, 24000, free heap 24000) from the measured steady blocks and state which are guesses today; propose calibrated values per board class
- [ ] #5 Redesign the TASK-3091 sweep so N above 2 can actually be the effective cap on the test unit (what must be changed for the run, prerelease only) and keep its decision rule otherwise; written plan agreed by otgw-architect
- [ ] #6 Result note for Robert: feasible N per board class (2, 4, 8) with evidence or an explicit 'not feasible because', the changes needed (streaming, thresholds, ADR superseding ADR-165, client change ADR-184 and the v2 UI limit, CLAUDE.md single-flight rule), cost and risk, and a recommendation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC3 inventory, part B: stream endpoints not yet measured (otgw-poc-dev, 2026-11-26, read-only, code read in wt-poc-merge = origin/dev + lane-poc-merge; nothing built, nothing run on a unit; every size marked EST is my estimate from the emit code, NOT a measurement). Part A (otmonitor, ble, health, presize) is otgw-architect's. Line numbers are from my tree; they may differ by a few lines from yours.

ALREADY CHUNKED, no saving from chunking: settings restAPI.ino:4208 (8634 B), device/info :3292, sat/status SATcontrol.ino:5111, debug :2220, graph history graphHistoryStuff.ino:113 (restSendChunked, jsonChunked.h:115).

1. /settings.ini (5882 B): NOT a stream. handleFile FSexplorer.ino:643-648 -> webSendFile (webServerCompat.h:648) -> WebQueuedFileResponse, file-backed, behind the FIFO file gate (webFileGateTryAdmit). No contiguous RAM body. Needs checkHttpAuth (FSexplorer.ino:614). Largest allocation: the response object plus the library file read window. Chunking: n/a. Open question (not checked): whether the file gate counts against REST_MAX_INFLIGHT.

2. /api/listfiles apilistfiles FSexplorer.ino:496 (restBeginStream, unpresized, starts at 1460 B). Bound: MAX_FILES_IN_LIST = 40 (FSexplorer.ino:53) + 1 summary object. Per entry {"name":"<<=31>","size":N,"type":"file"} about 45-75 B, so full file system EST 40 x 65 + 90 = 2.7 KB, worst EST 3.1 KB (escapes). Largest contiguous: the cbuf staircase from 1460 B up to about 3 KB (transient about 2x). Chunking: poor fit (a LittleFS directory walk per window, order is stable but it is file I/O on async_tcp each pass, 2-3 passes). Better: presize class 3584 (effort S, risk low). Volatile fields: none except the file list itself.

3. /api/v2/firmware/files apifirmwarefilelist FSexplorer.ino:370-427 (restBeginStream, unpresized). Entry {"name","version","size"} about 70 B, count = .hex files in /<deviceid>: EST a handful (PIC images), unbounded in code but under 1 KB in practice. NOT chunkable as written: the loop opens .ver files and WRITES them (LittleFS.open(verfile,"w"), FSexplorer.ino:~406) while emitting, so a re-run per window would repeat file I/O and writes: breaks the determinism contract. Leave as is (effort of a change: M, gain: none).

4. /api/v2/device/crashlog sendDeviceCrashLog restAPI.ino:3718. summary char[160] + details char[REBOOT_LOG_LINE_MAX=240] (OTGW-firmware.h:245), EST worst 0.9 KB with escaping, so one 1460 B cbuf is enough: no growth. Chunking: no saving. Leave.

5. handleSAT: (a) BLE roster GET restAPI.ino:1219 (restBeginStream, not the presized satBLERosterSendJSON): 8 slots x {idx, mac, label, has_bindkey} EST 90-130 B each plus header, EST 0.8-0.9 KB (sBleLabel is char[24], SATtypes.h:680), so it fits the first 1460 B cbuf: no growth, leave. (b) weather needs-setup restAPI.ino:1583: about 30 B. Leave. (c) satSendHealthJSON restAPI.ino:1061 (1021): not in your list; 51 begin/field calls were counted for sendHealth, not this; body size not estimated.

6. handleNetwork wifi scan restAPI.ino:2507-2537 (restBeginStream, unpresized): per AP {"ssid" up to 32, rssi, channel, secured, connected} about 90-110 B, count = scan result (unbounded in code, EST typical 10-30 APs = 1-3 KB, dense area 50 APs about 5 KB). This is the one endpoint here that can pass 4096 B on a crowded site. NOT chunkable without refactor: the handler calls WiFi.scanDelete() right after restFinalize (restAPI.ino:~2540), but a chunked body is produced AFTER the handler returns, so the results would already be gone: needs a frozen snapshot (array of ~50 x 40 B = 2 KB of heap) or a deferred scanDelete. Effort M-L, risk HIGH. Cheaper: cap the emitted APs (for example strongest 24) and presize 3072 (effort S).

7. PIC settings sendPICsettings restAPI.ino:3930: 15 short strings from state.picSettings (sizes of the char arrays not read); EST 0.5-0.8 KB, fits 1460 B. Volatile: yes (triggerPICsettingsReadout updates them while serving), so chunking would need a snapshot; no saving. Leave.

SUMMARY for the 2-to-8 change: of the unmeasured ones only the wifi scan (EST up to 5 KB) and listfiles (about 3 KB) can outgrow the first cbuf; the BLE roster GET and PIC settings fit it. None needs chunking; presize plus a cap on the scan list is the low-risk path. NOT CHECKED: measured body lengths (restStreamSizeReport only covers the three classed endpoints), the sizes of sBleLabel and picSettings arrays, wifi scan result cap in the WiFi driver, and anything at 8 in flight on a unit.
<!-- SECTION:NOTES:END -->
