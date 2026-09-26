---
id: TASK-1165
title: >-
  Resume: validate the send-buffer pre-flight fix (TASK-1164) on the bench and
  ship it
status: To Do
assignee: []
created_date: '2026-09-26 14:47'
updated_date: '2026-09-26 16:47'
labels:
  - bug
  - mqtt
  - bench
  - parked
dependencies:
  - TASK-1164
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
  - logs/task-1164-sndbuf/
priority: high
ordinal: 234000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parked 2026-09-26 at the maintainer's request. Everything needed to pick this up again is below.

## Why
TASK-1163 (Done) measured that mqttFrameFitsSndbuf() gave up after 10 yield() calls, shorter than one ACK round trip, so the ~30-publish burst of do5minevent() (OTGW-firmware.ino:411-417) lost its tail every 5 minutes: the otgw-pic/settings/* topics (3/15 arrived per block), plus the tail of the hourly heapdiag stats. OT values were not affected (484/484 arrived). Regression from TASK-1154. Reported on GH #682 (jaronbor, ~1.6 skips/min on a healthy link); the #682 comment promises the fix "in the next beta".

## Code state
- Worktree D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x, branch otgw-1.x.x.
- Fix commit d81dfdfa9 "fix(mqtt): send-buffer pre-flight waits for an ACK instead of giving up": LOCAL ONLY, NOT PUSHED (branch is ahead of origin/otgw-1.x.x). Changes src/OTGW-firmware/MQTTstuff.ino (MQTT_SNDBUF_WAIT_MS = 200 wall-clock wait with delay(1); MQTT_SNDBUF_BACKOFF_MS = 1000 skip-the-wait after a deferral) and CHANGELOG.md [Unreleased] Fixed.
- Build 1.7.6-beta.7+e7696a9 green, evaluate.py --quick 36/36. Tree carries unpublished beta.7.

## Bench state (192.168.88.68, hostname OTGW, COM3)
- OTA-flashed with the fix build on 2026-09-24 22:07 (firmware only; LittleFS still holds /otgw_simulation.log).
- OFFLINE since about 2026-09-26 16:27 (last broker message); no ping, no HTTP. Cause unknown. It runs the fix, so a link must be ruled out.
- Its MQTT broker setting still points at the test broker 192.168.88.32:1884 (the restore POSTs could not reach it), and the simulator may still be active. Until restored, Home Assistant gets no data from this unit.
- Recovery if it does not come back: RTS pulse on COM3 with DTR high (see memory reference_bench_1x_esp8266_ota_recipe). USB flashing does not work on this board; web OTA the XHR way only.

## Test rig (all in logs/task-1164-sndbuf/, gitignored)
- mosq1885.conf: mosquitto.exe on 127.0.0.1:1885, anonymous. Windows Firewall blocks mosquitto.exe inbound, so fwd1884.py (python.exe, allowed) forwards 0.0.0.0:1884 -> 127.0.0.1:1885.
- Lossless subscriber: "C:/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -p 1885 -t # -F "%I %t %p" (%U is not supported on Windows).
- Point the gateway at it with two single-field POSTs to /api/v2/settings: {"name":"mqttbroker","value":"192.168.88.32"} then {"name":"mqttbrokerport","value":1884}; the second often gets no reply, resend it. Restore with homeassistant.local / 1883. The MQTT password is never touched.
- gen_sim.py -> otgw_simulation.log (40 cycles x 16 msgids, parity-correct, every value changes each cycle, 16 min per loop) + expected.json. Upload: curl -F "file=@<windows path>;filename=otgw_simulation.log" http://192.168.88.68/upload (use a C:/ path; git-bash /c/ paths fail). Start/stop: POST /api/v2/simulate/start and /stop.
- cmp2.py <dir> <sublog>: per OT value topic, sequence-gap check against the replay ring; per 5-minute block, how many of the 15 otgw-pic/settings/* topics arrived. Validated on the pre-fix log: 0 real gaps, settings 3/15 per block.
- stall_broker.py: broker that accepts CONNECT then stops reading (for the stall bound, AC #3).
- Data: sub1163.log / counters1163.log = pre-fix baseline (TASK-1163). sub1164.log / counters1164.log = fix build; the counter loop ran only 22:08-22:11 before a host low-memory reap, and sub1164.log is only ~180 KB despite ~2 days of broker uptime, so check what it actually covers before trusting it.

## Lessons from the last attempt
- Do not leave the simulator, test broker or subscriber running unattended: a host memory reap killed the poller but not them, and HA lost data for two days.
- Run long measurements with a stop that does not depend on the poller (for example stop the sim and restore the broker in the same script, or a Monitor with a deadline).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench 192.168.88.68 is back online, its MQTT broker setting is restored to homeassistant.local:1883, the simulator is stopped, and Home Assistant receives its data again
- [ ] #2 The cause of the 2026-09-26 16:27 outage is established from /api/v2/device/crashlog, /reboot_log.txt and sub1164.log, and a link to the TASK-1164 fix is either ruled out or confirmed with evidence
- [ ] #3 sub1164.log is analysed with cmp2.py and its coverage stated (time span, number of 5-minute blocks)
- [ ] #4 Re-run with the fix build: over at least three 5-minute blocks every otgw-pic/settings/* topic arrives (15/15), OT value topics show 0 sequence gaps, and mqtt_sndbuf_skips stays flat
- [ ] #5 Against stall_broker.py the gateway stays responsive (longest REST gap no worse than the 4.9 s of TASK-1155) and mqtt_desync_drops stays 0
- [ ] #6 Test rig torn down and the bench restored at the end of the session, verified over telnet (broker line) and /api/v2/simulate
- [ ] #7 TASK-1164 ACs checked and closed; the fix commit pushed to origin/otgw-1.x.x only after the above pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-26: GH #682 mrfox7688 reports a corrupted TSet payload (10.à

2026-09-26: the unvalidated work (TASK-1164 fix, TASK-1167 write floor + status fields, ADR-066/083/097 changes) is pushed to the separate branch origin/otgw-1.x.x-pending-bench-validation (f87d93bde), NOT to origin/otgw-1.x.x. SimpleTelnet per-slot API pushed as origin/feat/per-slot-read (e8d01df) in rvdbreemen/SimpleTelnet. Local otgw-1.x.x still carries the same commits ahead of origin; merge the pending branch into otgw-1.x.x only after the bench validation passes.
<!-- SECTION:NOTES:END -->
