---
id: TASK-2923
title: >-
  Prerelease AP fallback panics in the wifi task when the heap is starved
  (NULL+0x2c in ieee80211_hostap_attach)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T16:47:00Z'
updated_date: '2026-11-19T19:01:00Z'
labels:
  - wifi
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1058000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09 bench 2026-11-19 on .61 (no PSRAM), 2.0.0-beta.393+4bb2097 with the TASK-2026 heap hold: 2 panics (17:25:08, 17:41:18), reboot_log Exception/panic (4), core dump wifi. Decode (esp_coredump 1.18.1, ELF sha256 e64fa292 matches): exccause 0x1c LoadProhibited, excvaddr 0x2c in ieee80211_hostap_attach <- wifi_softap_start <- _do_wifi_start <- wifi_set_mode_process <- ppTask; loopTask in the same dump: networkStuff.ino:996 loopWifi -> startAPFallback -> :514 WiFi.mode(WIFI_AP_STA). STA was lost (router: group/unicast key exchange timeouts from 17:23:53) and after 2 failed 30 s attempts the prerelease supervisor starts the softAP on a heap held at free < 7168 / largest block < 2048. Likely an unchecked allocation in the closed driver (inferred, not proven). Release builds do not compile the AP fallback and restart after 10 attempts. Evidence: OTGW-field-data/bench-otgw32/20261119-task2026-393/. Fix: startAPFallback only when the heap is not starved; otherwise the release path (retry, WIFI_FAILED restart at 10).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Under a starved heap the prerelease supervisor does not start the softAP, logs that it skipped the AP fallback with the largest block, and follows the release path (retry, clean restart after 10 failed attempts)
- [x] #2 Host test compiles the real decision and the call site: red on 4bb2097ed (no guard), green on the fix
- [x] #3 Bench .61 (otgw-bench-tester): the same heap hold as the 393 run gives no panic in task wifi, the skip line appears, and the unit recovers after the release or restarts cleanly with the WiFi reason
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
In WIFI_CONNECTING (prerelease) start the AP fallback only when wifiApFallbackHeapOk(free, largest block): free >= HEAP_LOW_THRESHOLD (8192) and largest block >= WIFI_STARVED_MAXBLOCK (4096), the two limits the firmware already treats as low. Otherwise log 'BETA: AP fallback skipped, heap starved (...)' and follow the release path of the same switch: WIFI_DISCONNECTED (retry), WIFI_FAILED (clean doRestart 'WiFi: too many reconnect failures') at 10 attempts. bNoPortal keeps its TASK-1645 behaviour (endless retry, no restart). Threshold: not measured (closed driver, how much hostap_attach needs is unknown); the hold sits at free < 7168 / block < 2048, both under the limits. One build with the TASK-2026 hook (394), bench .61 by 09.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
5e 2026-11-19: host test tests/test_ap_fallback_starved_heap.py compiles the REAL 'case WIFI_CONNECTING:' block of networkStuff.ino (plus wifiApFallbackHeapOk() when present) against stubs and drives 12 failed attempts. RED on 4bb2097ed: 2 failures (test_a_starved_heap_skips_the_ap_and_restarts_as_release, test_low_free_with_a_large_block_is_also_starved: softAP started, ap '1' != '0'); 2 pass (healthy heap starts the AP after 2, bNoPortal retries). GREEN on the fix: 4/4. tests/test_wifi_deadlink.py STATE_STUBS gets platformFreeHeap() (56448, healthy) and HEAP_LOW_THRESHOLD so its harness compiles the new check: test_wifi_deadlink 31 OK, test_wifi_skip_portal 38 OK (incl. the prerelease AP fallback scenarios), test_heaphold_hook 8 OK. Not verified: the real driver behaviour (bench AC3).

2026-11-19 09 (otgw-bench-tester): AC1 and AC3 NOT ticked. Bench .61 394+17c993a, two holds: no panic in task wifi, release path taken, clean restarts 18:24:08 and 18:39:08 'WiFi: too many reconnect failures', coredump unchanged; red reference 393+4bb2097 (2 wifi panics, 17:25:08 and 17:41:18). Both ACs require the skip log line (networkStuff.ino:1014 'BETA: AP fallback skipped, heap starved ...'); that line is not in the evidence because telnet is dead during the hold and the line is not persisted. Behaviour proven, log line not captured. The choice (waiver on the line, a rerun where it becomes visible, or an AC rewrite) is open with Robert via the coordinator. Archive: RvdB/OTGW-field-data/bench-otgw32/20261119-task2923-394.

2026-11-19 AC1/AC3 decided by Robert: 'A' (DM 1573043917337665547, relayed by the coordinator, reply to the question waiver on the skip log line vs a rerun that makes it visible): the skip log line is waived; AC1 and AC3 are closed on behaviour. Taken over from 5e by otgw-dev (5e is on the remote telemetry PoC only).

2026-11-19 boot-time AP fallback (networkStuff.ino:431, unguarded; otgw-docs audit) assessed by otgw-dev: it cannot meet a starved heap. (1) Every reset, also after a panic (TASK-2911), starts with a re-initialised heap, and startWiFi() runs early in setup() (OTGW-firmware.ino:395), before MQTT (:571), fsWriter/SAT (:638-639) and anything else that could fill it; the heap-hold hook and BLE are runtime state that a reset clears. Measured: the first telnet lines after the two starvation restarts on .61 (394) show free 57596 / largest block 31732 at 18:24:27 and 53732 / 31732 at 18:39:12, far above WIFI_STARVED_MAXBLOCK 4096 and HEAP_LOW_THRESHOLD 8192. (2) After a WiFi supervisor restart (the 'too many reconnect failures' path) ADR-211 skips both the portal and the boot AP fallback anyway (wifiSkipPortalDecide, networkStuff.ino:416). No follow-up task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix: the prerelease WiFi supervisor no longer starts the softAP fallback while the heap is starved; it follows the release path (retry, clean restart after 10 failed attempts), so the NULL+0x2c panic in ieee80211_hostap_attach (task wifi) under a starved heap is gone. AC2 (5e): host test compiling the real decision and call site: red on 4bb2097ed (no guard, 2 of 4 failing), green 4/4 on the fix. AC1 + AC3 (bench-tester on .61, heap-hold hook, archives OTGW-field-data/bench-otgw32): 393 = two panics in task wifi (17:25:08, 17:41:18); 394+17c993a = two holds (take 18:10:26 and 18:25:17), clean restarts 18:24:08 and 18:39:08 'WiFi: too many reconnect failures', no panic, no new core dump. The skip log line that AC1/AC3 name was NOT observed (telnet is dead during a hold); waived by Robert, 'A', DM 1573043917337665547, so AC1/AC3 are ticked on behaviour, not on the line.
<!-- SECTION:FINAL_SUMMARY:END -->
