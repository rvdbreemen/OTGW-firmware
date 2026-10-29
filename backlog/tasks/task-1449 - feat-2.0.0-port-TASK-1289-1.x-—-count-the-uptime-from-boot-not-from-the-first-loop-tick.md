---
id: TASK-1449
title: >-
  feat-2.0.0: port TASK-1289 (1.x) — count the uptime from boot, not from the
  first loop tick
status: To Do
assignee: []
created_date: '2026-10-29T06:19:00Z'
labels:
  - port-from-1.x
  - core
  - uptime
  - low-priority
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 609000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit b7c77fd56 (1.x TASK-1289).

PROBLEM (also present on 2.0.0)
- state.uptime.iSeconds is incremented in doTaskEvery1s() (origin/dev:src/OTGW-firmware/OTGW-firmware.ino:765, `state.uptime.iSeconds++;`). That function runs from loop() through `DECLARE_TIMER_SEC(timer1s, 1, SKIP_MISSED_TICKS)` (:1126, :1151).
- So the uptime misses all time spent in setup(). That includes the blocking WiFiManager portal (networkStuff.ino:223 startConfigPortal), which can last minutes. Dev's own TASK-1130 notes (backlog task-1130 ...md:76, :122) name exactly this.
- With SKIP_MISSED_TICKS, every loop stall longer than 1 s also loses seconds. Dev has documented stalls of 2-3 s (task-1328 'maxLoopGap 1967 ms'; commit 246294edc, MQTT connect to a dead broker). The uptime therefore drifts behind real time during a session.

1.x FIX (b7c77fd56, src/OTGW-firmware/OTGW-firmware.ino:237-239, :249-253)
- `state.uptime.iSeconds = (uint32_t)(micros64() / 1000000ULL);` on every 1 s tick. The value is derived from the boot clock instead of counted.
- 1.x also added a static loopSeconds counter, because its deferred boot-command gate (TASK-1121) used uptime >= 3 as a loop-time grace. DO NOT PORT the loopSeconds part: dev sends PIC boot commands straight from setup() (origin/dev OTGW-firmware.ino:562 sendPICBootCommands()), and nothing on dev gates on a small uptime value.

WHAT TO CHANGE ON 2.0.0
1. Add a boot-clock seconds helper:
   - ESP32: (uint32_t)(esp_timer_get_time() / 1000000LL). esp_timer_get_time() is int64 microseconds since boot.
   - ESP8266 (dev still builds an esp8266 env, if any): (uint32_t)(micros64() / 1000000ULL).
   - Do NOT use arduino-esp32 micros(): it is a 32-bit value that wraps after about 71.6 minutes.
   - Preferred home: src/libraries/Platform/src/platform_esp32.h / platform_esp8266.h (e.g. platformUptimeSeconds()). Platform is a vendored library, so it needs the maintainer's OK. Without that OK, use a static inline helper with #if defined(ESP32) in OTGW-firmware.ino.
2. Replace `state.uptime.iSeconds++;` at OTGW-firmware.ino:765 with an assignment from that helper. Add a one-line comment that cites TASK-1289 (1.x).
3. Leave UptimeSection (Uptimetypes.h:19-22) as uint32_t. No REST, MQTT or JSON shape change.

CONSUMERS (behaviour check only, no code change expected)
- MQTTstuff.ino:1862-1866 sendMQTTuptime (also called from setup() at :567, now non-zero)
- helperStuff.ino:661-670 upTime() (device/info 'uptime' via restAPI.ino:3144/:3259)
- restAPI.ino:2210 runtime.uptime_sec
- OLED.ino:362
- handleDebug.ino:29
- MQTTstuff.ino:283 verifyAccessorUptimeSeconds()
- uptime > 3600 gates at OTGW-firmware.ino:897 (nightly restart) and :969 (daily discovery heal). After a long portal boot these open earlier in loop time but still one hour of real time after boot, which is the intended meaning.

RISKS
- Low. This is a pure clock-source change.
- The only visible effect is that the uptime is larger by the setup() duration plus any lost ticks.
- Scripts that detect a reboot by an uptime drop (capture-mqtt-debug, heap_soak_driver) keep working, because the value still restarts near 0 at reset.

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:765 still has `state.uptime.iSeconds++;` inside doTaskEvery1s()
- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:1126 `DECLARE_TIMER_SEC(timer1s, 1, SKIP_MISSED_TICKS);` and :1151 `if (DUE(timer1s)) doTaskEvery1s();` mean setup() time and loop stalls are not counted
- origin/dev:backlog/tasks/task-1130 ...md:76 records the defect on dev: 'state.uptime.iSeconds counts only from loop() (doTaskEvery1s, SKIP_MISSED_TICKS), so the blocking startConfigPortal() time in setup() is not in it.' (also :122)
- origin/dev:src/OTGW-firmware/networkStuff.ino:223 startConfigPortal() blocks in setup() on dev as well
- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:562 sendPICBootCommands() is called directly in setup(), so dev has no uptime-based boot-command gate and does not need the 1.x loopSeconds counter
- Searched with git grep on origin/dev src/OTGW-firmware: no micros64, esp_timer_get_time, millis64 or other boot-clock-based uptime. The only esp_timer mention is a Ticker comment at OTGW-firmware.ino:589
- Searched origin/dev backlog for 1289, 'uptime.*boot' and esp_timer_get_time: no sibling task. `git log origin/dev --grep=uptime -i`: no commit changes the counter source. PR #668 'otgw-uptime-sensor' is about the HA uptime sensor, not about how the counter is computed
- Consumers on dev that read iSeconds: MQTTstuff.ino:1863-1866 (otgw-firmware/uptime), helperStuff.ino:661-670 upTime(), restAPI.ino:2210 and :3144/:3259 (device/info uptime), OLED.ino:362, handleDebug.ino:29, OTGW-firmware.ino:897 and :969 (uptime>3600 gates), MQTTstuff.ino:283 verifyAccessorUptimeSeconds()

## Platform notes

ESP32-S3 / arduino-esp32 has no micros64(). The equivalent is esp_timer_get_time(), which returns int64_t microseconds since boot and does not wrap in practice. Do not use arduino-esp32 micros(): it is a 32-bit cast that wraps after about 71.6 min. millis() wraps at 49.7 days. Per the 2.0.0 platform-abstraction rule, put the clock read behind a helper, for example platformUptimeSeconds() in src/libraries/Platform/src/platform_esp32.h (esp_timer_get_time()/1000000) and platform_esp8266.h (micros64()/1000000). Platform is a vendored library, and the memory rule 'No library changes without approval' may apply. If maintainer approval is not given, use a local static inline helper with #ifdef ESP32 in OTGW-firmware.ino. iSeconds stays uint32_t, so no struct or JSON change is needed. The setup()-time sendMQTTuptime() at OTGW-firmware.ino:567 now publishes the true seconds since boot instead of 0, which is fine. The uptime>3600 gates (nightly restart at :897, daily discovery heal at :969) open slightly earlier after a long portal boot. They are still more than one hour of real time after boot, so this is acceptable, but note it in the change.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

Verdict holds. All cited dev lines are real and dev does exactly the same per-tick ++ behind a SKIP_MISSED_TICKS timer. No dev task or code ports it under another name. The analyst was right that the loopSeconds part does not apply, because dev sends the PIC boot commands from setup(). One factual slip: the draft assumes dev may still build an ESP8266 env and says to add the helper to platform_esp8266.h. Dev has neither: platformio.ini has only the esp32, esp32-classic and esp32-combo envs, and the Platform library has only platform_esp32.h next to platform.h.

In WHAT TO CHANGE step 1, drop the ESP8266/micros64() branch and the reference to platform_esp8266.h. Neither exists on origin/dev: the Platform lib has only platform.h, platform_esp32.h and boards.h, and the only envs are esp32, esp32-classic and esp32-combo. Put the helper in src/libraries/Platform/src/platform_esp32.h as (uint32_t)(esp_timer_get_time()/1000000LL), since platform.h:8 says all platform differences live there. This needs maintainer approval because Platform is a vendored lib. Without it, use a local static inline helper in OTGW-firmware.ino. AC1 should say 'derived from esp_timer_get_time()' with no ESP8266 clause. Everything else checks out: the targets (:765, :1126, :1151, :562, :567, :897, :969) are correct, and the red/green ACs are fine.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 state.uptime.iSeconds on 2.0.0 is derived from a boot clock (esp_timer_get_time() on ESP32, micros64() on ESP8266) instead of a per-tick ++ in doTaskEvery1s(). arduino-esp32 micros() is not used.
- [ ] #2 Red/green bench on .61 OTGW32 or .63 Classic+S3: reset by RTS/EN with laptop timestamps, then compare GET /api/v2/device/info runtime.uptime_sec against the time since reset. Old build is behind by the full setup() duration. New build is within about 2 s. Record both numbers in the task notes.
- [ ] #3 Red/green stall check: induce a loop stall of 2 s or more (e.g. MQTT broker pointed at an unreachable host, per commit 246294edc). Over 5 minutes the old build's uptime falls behind wall time and the new build's does not. Record the drift on both.
- [ ] #4 The 1.x loopSeconds boot-command gate is NOT ported. sendPICBootCommands() is still called from setup() unchanged.
- [ ] #5 otgw-firmware/uptime MQTT, device/info 'uptime', the OLED device page and the telnet uptime line show the same new value. No API or MQTT payload shape changes.
- [ ] #6 Build for esp32, esp32-classic and esp32-combo succeeds, and python evaluate.py --quick is green.
<!-- AC:END -->
