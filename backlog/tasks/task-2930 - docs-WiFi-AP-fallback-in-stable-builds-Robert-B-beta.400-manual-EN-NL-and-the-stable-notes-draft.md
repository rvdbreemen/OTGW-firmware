---
id: TASK-2930
title: >-
  docs: WiFi AP fallback in stable builds (Robert B, beta.400): manual EN/NL and
  the stable notes draft
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T20:10:00Z'
updated_date: '2026-11-19T22:06:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1064000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator 2026-11-19: Robert chose B, the AP fallback (was prerelease-only behind _VERSION_PRERELEASE) goes into stable; d3 builds it as beta.400 and renames the BETA: log lines. Update every 'beta builds only' AP fallback mention in the manual (ch01, ch02, ch03, ch06 6.1.2/6.1.3, ch07, ch09, ch10 and the NL chapters), the log lines, and the stable notes draft. Commit after d3's final log texts, push after d3's 400 push.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every AP fallback mention in docs/manuals EN/NL matches d3's beta.400 code (file:line), incl. 2 failed attempts, wifinoportal and the low-heap skip, and the renamed log lines
- [x] #2 Stable notes draft describes the AP fallback in stable
- [x] #3 MANUAL.md rebuilt, build_manual --check and test_manual_* green; pushed after d3's 400 via the coordinator's slot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19: written from d3's answers and verified against his lane commit d7667ae4f (task 2931, on 5844e33ff, base origin 4759e21de; version.h says beta.401, not 400, so the docs say beta.401: recheck at push). networkStuff.ino at d7667ae4f: boot path :424-431 (wifiSaved && !forcePortal, after wifiSkipPortalDecide :416 which covers ADR-211 and wifinoportal), boot log :429 'WiFi: unavailable at boot, skipping config portal -> AP fallback' (ASCII ->); decision wifiNextStateAfterFailedAttempt :656-664 (retry < 2 or noPortal -> DISCONNECTED; heap ok -> AP; else skip log :661 and FAILED at >= 10); runtime :1003-1007 log 'WiFi: retries exhausted, entering AP fallback mode'; timerWifiRetry 30 s :969, timerAPRetry 300 s :970; WIFI_FAILED doRestart 'WiFi: too many reconnect failures' :1041; AP 'WiFi AP: fallback started SSID=[...] IP=192.168.4.1 pass=otgw1234' :515. Docs changed: EN ch01:48/:83, ch02:250, ch06 6.1.2 + 6.1.3, ch07:137/:162/:237, ch09:258, ch10:258; NL h01:98 (also fixed 'drie keer' -> 'twee keer'), h02:279, h06:26 + 6.1.3 (title without '(beta)'), h07:223 (header and body now agree), h07:367, h09:419, h10:282; stable draft WiFi bullet. Gates: build_manual --check exit 0, test_manual_* 10 passed, no em dashes added.

Push check on origin 9f75390ff: d3's final commit is 47d08b117 (beta.401; only change vs d7667ae4f: uint8_t return type + cast, lines +2 from the decision fn: :658-666, skip log :663, runtime log :1007, restart :1043, timer :971); networkStuff.ino has no _VERSION_PRERELEASE left; all quoted log lines present. Added from task 2933 (09's bench of 401 on .61): the softAP closes only at the next 5-min AP check, so up to 5 min next to the restored link (6.1.3 EN/NL). Gates: build_manual --check exit 0, test_manual_* 10 passed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: every AP fallback mention in docs/manuals EN/NL (ch01/02/06/07/09/10, h01/02/06/07/09/10) now describes the AP fallback in all builds from beta.401, checked against d3's 47d08b117: boot path networkStuff.ino:416-431, decision :658-666 (2 attempts; none with wifinoportal = retry every 30 s :971, no restart; none on a starved heap = restart after 10, :663/:1043), runtime log :1007; the 5-min AP linger of task 2933 added. AC2: stable notes draft WiFi bullet (task 2931, beta.401). AC3: MANUAL.md rebuilt, build_manual --check exit 0, test_manual_* 10 passed; pushed after d3's 401 in the coordinator's slot. Not verified by me: the behaviour on a device (09 benched 401 on .61 per task 2931).
<!-- SECTION:FINAL_SUMMARY:END -->
