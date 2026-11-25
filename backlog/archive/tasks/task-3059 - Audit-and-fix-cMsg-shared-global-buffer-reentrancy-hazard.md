---
id: TASK-1
title: Audit and fix cMsg shared global buffer reentrancy hazard
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:12'
updated_date: '2026-03-12 20:32'
labels:
  - refactor
  - safety
  - esp8266
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The global `cMsg[512]` buffer in OTGW-firmware.h is used in 16+ places across OTGW-Core.ino and other files. Under the cooperative ESP8266 scheduler, any `yield()` call inside a function using `cMsg` can let another task overwrite it mid-use — the same class of bug as the mqttAutoCfgScratch reentrancy issue already fixed. Audit every `cMsg` use site and replace hot-path or re-entrant uses with local/static buffers or function-scoped guards.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All cMsg use sites are documented with a comment indicating safe/unsafe status
- [x] #2 Hot-path and re-entrant uses of cMsg are replaced with local or static buffers
- [x] #3 No functional regressions in MQTT publish, REST API, or OT message handling
- [x] #4 cMsg retained only where it is the sole writer and no yield/callback can interrupt
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
$'Fix plan based on audit results:\n\n**CRITICAL (8 sites, OTGW-Core.ino)** — snprintf(cMsg,...) → sendEventToWebSocket/reportOTGWEvent path triggers feedWatchDog → doBackgroundTasks re-enters → cMsg overwritten mid-use.\nFix: replace cMsg with local char buf[80] at each call site.\n\nSites:\n- ~1982-1983: handleOTGWcmdqueue drop notification\n- ~3149-3174: processOT error status fields (4 sites)\n- ~3183-3184: processOT PIC restart event\n- ~3297-3298: handleOTGW serial overflow\n- ~3327-3328: handleOTGW simulation blocked\n\n**HIGH (1 site, jsonStuff.ino:831-835)** — extractJsonField() builds search key into cMsg; String::indexOf() may yield.\nFix: local char buf[MQTT_TOPIC_MAX_LEN] or similar.\n\n**MODERATE (4 sites, restAPI.ino:616-705)** — sendApiInfo/sendApiInfoMap use cMsg; HTTP handlers do not re-enter but violate ownership intent.\nFix: local char buf[32] at each site.\n\n**LOW (2 sites)** — FSexplorer.ino:195 (setup, no yield) and settingStuff.ino:149 (controlled by caller). Add safety comment, leave as-is.'
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. 16 cMsg use sites found across 5 files.\n\nOnly ONE genuine re-entrancy risk: OTGW-Core.ino line 3183 (reportOTGWEvent path). The MQTT publish inside reportOTGWEvent calls feedWatchDog() which yields; doBackgroundTasks re-enters and overwrites cMsg before the WebSocket call. Fixed with local evtBuf[60].\n\nAll sendEventToWebSocket sites are safe: the function copies msg to ot_log_buffer synchronously via AddLog() before any yield. Retained cMsg with safety comment added at the error handler block.\n\njsonStuff.ino extractJsonField: String::indexOf() does not yield. Safe with cMsg. Updated comment.\n\nrestAPI.ino and FSexplorer.ino: HTTP handler context, no re-entrancy. Safe.\n\nsettingStuff.ino: Already had safety comment. Left unchanged.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited all 16 cMsg use sites across OTGW-Core.ino, jsonStuff.ino, restAPI.ino, FSexplorer.ino, and settingStuff.ino.\n\nOnly one genuine re-entrancy bug found and fixed: OTGW-Core.ino:3183 (reportOTGWEvent path). Inside reportOTGWEvent(), sendMQTTData() calls feedWatchDog() which yields; doBackgroundTasks re-enters, processOT overwrites cMsg, and then the subsequent sendEventToWebSocket() reads corrupted data. Fixed with local char evtBuf[60] on the caller side.\n\nAll sendEventToWebSocket sites are safe (AddLog copies synchronously before yield). All restAPI/jsonStuff sites are safe (HTTP handlers don't re-enter; String::indexOf doesn't yield). Safety rationale documented via inline comments.
<!-- SECTION:FINAL_SUMMARY:END -->
