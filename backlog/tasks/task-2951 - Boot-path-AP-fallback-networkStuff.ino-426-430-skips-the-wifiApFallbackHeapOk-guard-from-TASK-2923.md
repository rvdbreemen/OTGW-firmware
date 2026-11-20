---
id: TASK-2951
title: >-
  Boot-path AP fallback (networkStuff.ino:426-430) skips the
  wifiApFallbackHeapOk() guard from TASK-2923
status: To Do
assignee: []
created_date: '2026-11-20T11:53:00Z'
labels:
  - firmware
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1079000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-docs (95) during TASK-2950, 2026-11-20. The starved-heap skip of the AP fallback (TASK-2923, after the ieee80211_hostap_attach crash on a starved heap) runs only in the runtime path: wifiNextStateAfterFailedAttempt() at networkStuff.ino:658 calls wifiApFallbackHeapOk(). The boot path ('WiFi: unavailable at boot, skipping config portal -> AP fallback', networkStuff.ino:426-430) calls startAPFallback() directly without that guard. Risk estimate: small, not measured (the heap is usually healthy at boot). Docs (manual 6.1.3 EN/NL, stable notes) say 'at runtime' until this is fixed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test red on the current boot path with a starved heap (startAPFallback reached), green after the guard
- [ ] #2 Boot with a normal heap unchanged (AP fallback still starts when WiFi is unreachable at boot)
<!-- AC:END -->
