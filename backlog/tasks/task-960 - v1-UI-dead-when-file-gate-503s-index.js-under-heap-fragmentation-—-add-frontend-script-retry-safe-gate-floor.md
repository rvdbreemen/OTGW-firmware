---
id: TASK-960
title: >-
  v1 UI dead when file gate 503s index.js under heap fragmentation — add
  frontend script-retry + safe gate floor
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-30 20:22'
updated_date: '2026-06-30 21:06'
labels: []
dependencies: []
ordinal: 172000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported with screenshot: classic v1 UI shows [hostname]/[version]/[00:00:00] placeholders, 'Wait for it...', empty log. Diagnosed on .39 (alpha.298): REST API is FINE (device/info 5/5 -> 200). Real failure: index.js itself returns 503 from the file-serve gate (webFileGateTryAdmit, ADR-147, restAPI.ino:83). Evidence (telnet): 'WEBFILE BUSY: 4 in-flight (cap 1) => 503 (freeheap=34920 maxblock=7668)'. The v1 UI loads 8 JS/CSS concurrently; the burst fragments the heap (maxBlk 40948 idle -> 7668 under load, frag 55%); the gate clamps cap to 1 (maxBlock<16000) or 2 (<24000); 4+ in-flight -> excess 503'd incl index.js. A 503'd <script> = dead UI, and a 503'd asset never caches so it keeps 503ing (feedback death-spiral). no-cache (ADR-163) is not the root cause (503s are on 200-serves; 304s are ungated) but removed the max-age blind-serve mask that hid it on warm reloads. Fix chosen (user): frontend script-retry on 503 (UI self-heals) + a SAFE gate floor tweak. CAUTION: cap=1 at maxBlock<16000 is allocation-bound (TASK-879 PCB-hang prevention) - do not raise it past what can safely allocate 2 concurrent LittleFS serves; the frontend retry is what makes a low cap acceptable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Frontend: index.html loads critical scripts (index.js/graph.js/sat.js) with retry on 503/network failure (bounded retries, small backoff), so a transient gate 503 self-heals instead of killing the UI
- [x] #2 Backend gate: add Retry-After to the 503 and/or a safe floor adjustment that does NOT risk the TASK-879 alloc-hang (verify 2 concurrent LittleFS serves fit at the chosen maxBlock floor)
- [x] #3 Verified on .39 under heap pressure: v1 UI populates hostname/version/uptime + log loads, no permanent dead-UI; evaluate clean; build green; prerelease bumped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
FIXED + verified on .39 (alpha.298, Playwright real-browser). Root cause confirmed live: index.js got 503'd by the file-serve gate during the 8-asset cold-load burst (telnet: 'WEBFILE BUSY: 4 in-flight (cap 1) => 503, maxblock=7668'); a 503'd <script> killed the UI ([hostname]/[version] placeholders). Fixes: (1) index.html loads the 6 app scripts SEQUENTIALLY via an inline loader with retry-on-error (8 tries, linear backoff) - serializing removes the concurrent burst that fragments the heap, so the gate stops clamping; retry is the backstop. (2) index.js init made load-safe (readyState check + setTimeout deferral) since the loader can finish after window 'load'. (3) Removed a redundant inline 'window.onload=initMainPage' at end of index.html body that, with the now-async index.js, ran BEFORE initMainPage was defined -> 'initMainPage is not defined' ReferenceError (caught + fixed; PAGE ERRORS now (none)). (4) webServerCompat.h: Retry-After:1 on the file 503 (cooperative; cap kept - it is allocation-bound at maxBlock<16000, raising it risks the TASK-879 hung-PCB hazard). BONUS (second bug found while testing the switch): the v1->v2 'Try New UI' (index.html) and v2->v1 'Classic UI' (v2.js gotoClassic) handlers reloaded UNCONDITIONALLY (.catch{}.finally(reload)) without checking response.ok, so a 503 on the ui_usev2 settings POST silently dropped the switch -> reloaded to the same UI. Both now retry the POST on 503/network and reload ONLY after a confirmed 200. Proof: real button clicks classic->v2->classic, 0 console errors, both UIs fully populated (screenshots).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix v1 (classic) Web UI dying when the file-serve gate 503s index.js under heap fragmentation, plus an unrelated silent UI-switch failure found while verifying. Reported: classic UI stuck on [hostname]/[version] placeholders, empty log. Root cause: index.js (main script) 503'd by webFileGateTryAdmit (ADR-147) during the 8-asset cold-load burst that fragments the heap (maxBlock 40948->7668, gate clamps cap to 1); a 503'd <script> = dead UI (REST API itself is fine). Fixes (all data assets + one firmware header): index.html serializes the 6 app scripts via an inline retry-on-503 loader (removes the burst + self-heals); index.js init made load-safe (readyState + deferral); removed a redundant inline window.onload=initMainPage that broke under async loading; webServerCompat.h adds Retry-After:1 to the file 503 (cap kept, allocation-bound). Also fixed both UI-switch buttons (index.html #tryV2, v2.js gotoClassic) which reloaded blind without checking response.ok, silently dropping the switch when the ui_usev2 POST 503'd - now retry-then-reload-on-200. Verified live on OTGW32 .39 (alpha.298): classic UI fully populates, button-driven classic<->v2<->classic switch works with 0 console errors (screenshots). 3-target build + evaluate green.
<!-- SECTION:FINAL_SUMMARY:END -->
