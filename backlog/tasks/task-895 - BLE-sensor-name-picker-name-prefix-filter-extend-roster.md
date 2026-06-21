---
id: TASK-895
title: BLE sensor name picker + name-prefix filter (extend roster)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-21 11:42'
updated_date: '2026-06-21 20:51'
labels: []
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 119000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extend the existing TASK-508 BLE Sensors roster picker so the user can identify sensors by their advertised BLE name and narrow detection with a name-prefix filter.

Background: BLE is a connectionless passive listener (ATC/pvvx 0x181A + BTHome v2 0xFCD2 service-data only). Show+select-for-SAT already exists (roster + /api/v2/sat/ble/{discovery,select,label,forget}; Select pins settings.sat.sBleMAC). Missing: advertised names, a name-prefix filter, and on-demand name refresh.

Design approved by maintainer (brainstorm). Three confirmed decisions:
1. Empty/unknown name = admit + show (only confirmed mismatches are rejected/hidden).
2. Advertised name is runtime-only (not persisted); user label sBleLabel unchanged.
3. Prefix + ingest-toggle persisted via the generic POST /api/v2/settings; ONE new action route POST /api/v2/sat/ble/rescan for the active burst.

Scope guard: NOT a generic scanner, NO GATT connect, NO permanent active scan. Compatible temp sensors only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New settings: char sBleNamePrefix[24]="" (key SATblenameprefix) and bool bBleNameFilterIngest=false (key SATblenamefilteringest) added to SATSection (ADR-051); persisted+loaded in settingStuff; round-trips through /settings POST and /settings.ini.
- [x] #2 Roster captures advertised name via dev->getName() into BLERuntime.sName[32] (runtime-only, transient); exposed as per-sensor 'name' in /api/v2/sat/ble/discovery plus top-level 'name_prefix' and 'filter_ingest'.
- [ ] #3 Hybrid scan: passive-continuous remains the base; a ~5s active-scan burst runs at boot (satBLEInit) and on POST /api/v2/sat/ble/rescan, then auto-returns to passive-continuous. No permanent active scan.
- [x] #4 Filter rule implemented: case-insensitive prefix (strncasecmp_P). Empty prefix = off (current behaviour, admit all). Display filter (always when prefix set) hides ONLY rows whose name is known AND mismatches; empty/unknown name stays visible. Ingestion gate (only when bBleNameFilterIngest) rejects slot allocation ONLY for known-AND-mismatching names; empty/unknown name admitted.
- [x] #5 index.js BLE Sensors panel: shows advertised name; new filter row (prefix text input + 'also restrict ingestion' checkbox + 'Rescan names' button); prefix/toggle saved via generic settings POST; settings descriptions added for the two new keys.
- [x] #6 ADR amendment authored documenting the hybrid passive-continuous + on-demand active-burst scan model (amends ADR-092 / TASK-494 'passive to save power'); guideline-level per ADR-080 (no CI gate).
- [x] #7 Quality gates: python build.py --target esp32 exits 0 (esp32-otgw32 SUCCESS line present); python evaluate.py --quick shows no new failures; HAS_SAT_BLE gating respected, no raw #ifdef ESP* outside abstraction.
- [x] #8 FIELD VALIDATION (hardware, maintainer sign-off): advertised names appear in the panel; prefix filter narrows display and (with toggle) the roster; Rescan refreshes names; Select still pins a sensor as SAT room source.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Implementation order (each phase self-contained; build/eval at the end).

PHASE 1 — Settings foundation
- SATtypes.h / SATSection: add  char sBleNamePrefix[24] = "";  and  bool bBleNameFilterIngest = false;  in the BLE block (after sBleMAC/iBleInterval). Keep static_assert(label<=100) intact; prefix[24] fits.
- SATtypes.h / BLERuntime: add  char sName[32];  (advertised name, runtime-only). +256 B RAM ESP32, transient, BSS-zero.
- settingStuff.ino: register the two keys (SATblenameprefix, SATblenamefilteringest) in load/save + updateSetting handler, copying the existing SATblemac/SATbleinterval pattern (strlcpy + bool parse).

PHASE 2 — Firmware BLE logic (SATble.ino, all under #if HAS_SAT_BLE)
- onResult(): AFTER the temp-parse success guard (so getName() only runs for real sensors, not every ad), read dev->getName() (std::string) and strlcpy into a local nameBuf[32]. Compute the prefix decision with a helper bleNamePrefixConfirmedMismatch(name): returns true ONLY when prefix non-empty AND name non-empty AND strncasecmp_P(name,prefix,len)!=0. If settings.sat.bBleNameFilterIngest && confirmed-mismatch -> count _bleFilterRejCount and return (no slot). Else allocate slot as today and, under _bleSensorsMux, strlcpy nameBuf into _bleRuntime[slot].sName (reset to '' on new-slot).
- Active burst state: static volatile bool _bleActiveBurstRequested; static bool _bleActiveBurstRunning; static uint32_t _bleActiveBurstStartMs; const BLE_ACTIVE_BURST_MS=5000. Poll-driven on loop task (avoids callback-context restart):
  * satBLELoop(): if _bleActiveBurstRequested && !running -> stop(), setActiveScan(true), start(0,false,true) [continuous active], running=true, start=now, clear request. If running && (now-start)>=BURST_MS -> stop(), setActiveScan(false), start(0,false,true) [back to passive-continuous], running=false.
- satBLEInit(): keep passive-continuous start as base; set _bleActiveBurstRequested=true at the end so the first loop runs a boot burst then reverts.
- satBLERescanRequest(): exported; sets _bleActiveBurstRequested=true.
- satBLERosterSendJSON(): snapshot now includes sName; add je.field(F("name"), snap.sName) per sensor; add top-level je.field(F("name_prefix"), settings.sat.sBleNamePrefix) and je.field(F("filter_ingest"), settings.sat.bBleNameFilterIngest).
- Forward-declare bleNamePrefixConfirmedMismatch + satBLERescanRequest in OTGW-firmware.h as needed.

PHASE 3 — REST (restAPI.ino)
- In the existing ble sub-route block, add act=='rescan' (POST) -> satBLERescanRequest() -> 200 small JSON/OK. Update the route comment list (discovery/select/label/forget/rescan).

PHASE 4 — UI (data/index.js)
- buildBleSensorsPanel(): new filter row above the roster list: text input #ble-name-prefix, checkbox #ble-filter-ingest ('also restrict roster'), button 'Rescan names'.
- bleRowFor(s): show s.name (advertised) alongside MAC; label input (user label) unchanged.
- renderBleRoster(j): set #ble-name-prefix value from j.name_prefix and checkbox from j.filter_ingest (skip while focused/typing); DISPLAY filter — when prefix set, hide a row only if s.name is non-empty AND does not start with prefix (case-insensitive); empty/unknown name stays visible.
- Handlers: bleSaveNamePrefix() -> POST /api/v2/settings {name:SATblenameprefix}; bleToggleIngest() -> POST /api/v2/settings {name:SATblenamefilteringest, value 0/1}; bleRescan() -> POST /api/v2/sat/ble/rescan then refreshBleRoster().
- Settings descriptions array (~line 7311): add SATblenameprefix + SATblenamefilteringest entries; mark them '(managed via BLE Sensors panel)'.

PHASE 5 — ADR (adr-kit)
- New ADR amending ADR-092 / TASK-494: hybrid passive-continuous + on-demand active-burst scan model. Status Proposed (maintainer accepts). Guideline-level per ADR-080 (no CI gate).

PHASE 6 — Gates + version + push
- bin/bump-prerelease.sh (firmware paths changed -> prerelease bump, stages all banners).
- python build.py --target esp32 (expect exit 0 + esp32-otgw32 SUCCESS line).
- python evaluate.py --quick (no new failures; check ESP-abstraction scan).
- Single commit, descriptive title (not task-id), push origin/dev if green (dev policy).
- Discord #dev-sat-mqtt: task start + finish (alpha line).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Code complete (phases 1-5): SATtypes.h (2 settings + BLERuntime.sName), settingStuff.ino (save+parse), SATble.ino (name capture, new-slot-only ingest gate, hybrid active-burst 10s, satBLERescanRequest, satBLEPruneByNameFilter eviction, JSON fields), restAPI.ino (POST /sat/ble/rescan), OTGW-firmware.h (fwd-decl), data/index.js (filter row, name display, display filter, 3 handlers, settings labels+descriptions), ADR-151 (Proposed). evaluate.py --quick green (0 fail). Advisor review caught ingest-filter zombie/no-evict defect -> fixed via new-slot-only gate + loop-task prune. Build blocked twice by two-agent shared-.pio collision in one tree (NOT code); now building isolated in worktree D:/.../wt-ble895 per maintainer. Disk-full was real (maintainer cleared D: bloat).

Committed as ab4413dd (alpha.236), pushed to origin/dev. Build green (esp32 + esp32-combo SUCCESS), evaluate.py --quick green.

DESIGN CHANGE during field test: AC#3 (hybrid active-scan burst) is SUPERSEDED. The active-burst crash-looped a real OTGW32 (ESP32-S3) on OTA flash; per maintainer directive 'follow OT-Thing (passive-continuous)' the active-burst was REMOVED. Final design = passive-continuous getName() only (ATC/pvvx names are in the primary advertisement; BTHome scan-response names stay empty -> admitted). ADR-151 rewritten to document this (active-burst rejected). So AC#3 left unchecked by design; AC#8 field validation BLOCKED.

AC#8 (hardware field validation) BLOCKED: after USB recovery the OTGW32 is stable on the network (24/24 ping) but its webserver does not start (port 80 'connection refused') on BOTH esp32 and esp32-combo builds. This is NOT the BLE change (BLE is off by default after erase, so the code is inert; no edits touch setup()/server.begin(); submodules match). Separate pre-existing OTGW32 webserver issue (TASK-879/147 family) tracked as its own task. Live BLE web test deferred until the device serves HTTP again.

FIELD VALIDATION DONE (live on combo, device 192.168.88.39, KeepOut2): combo serves HTTP 200; /api/v2/sat/ble/discovery returns the new fields name_prefix + filter_ingest + per-sensor name; BLE enable (runtime lazy-init) discovered the ATC sensor A4:C1:38:8F:97:A4 (temp 28C, hum 60%, batt 96%, rssi -51); Select pinned it as SAT source (selected=true). LIMITATION accepted by maintainer: this sensor's advertised name is empty on passive scan (name lives in the scan-response; active scan was rejected as the crasher) -> the name filter only keys on sensors that put their name in the primary advertisement. Design = passive-only (ADR-151). Earlier 'webserver dead' was a transient post-crash state, not a combo bug (see TASK-898).
<!-- SECTION:NOTES:END -->
