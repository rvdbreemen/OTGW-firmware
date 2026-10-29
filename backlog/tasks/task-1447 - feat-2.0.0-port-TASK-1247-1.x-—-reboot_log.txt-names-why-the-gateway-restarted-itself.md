---
id: TASK-1447
title: >-
  feat-2.0.0: port TASK-1247 (1.x) — reboot_log.txt names why the gateway
  restarted itself
status: To Do
assignee: []
created_date: '2026-10-29T06:17:00Z'
labels:
  - port-from-1.x
  - diagnostics
  - reboot-log
  - wifi
  - low-priority
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 607000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why
On 2.0.0 every planned restart is logged in /reboot_log.txt as the same line, `reboot cause: Software reset (3)`, because doRestart() throws its reason away. A field log cannot tell a nightly restart from a WiFi-failure restart, an OTA, a safeboot handover or an operator /ReBoot. 1.x fixed this in TASK-1247 (GH #682).

## 1.x implementation (reference, ESP8266)
Commits 29c9f0834, a8c79fbf6 (capture moved to top of setup), 19a9d444c (portal-timeout chain fix). Current 1.x code:
- src/OTGW-firmware/helperStuff.ino:329-379: struct RebootReasonRtc {uint32_t magic 0x4F545252 'OTRR'; char reason[48];} stored in RTC user memory slot 100; storeRebootReason(), takeRebootReason() (reads and ALWAYS clears), captureRebootReason() (valid only when reset reason == REASON_SOFT_RESTART), storeRebootReasonUnlessPending().
- helperStuff.ino:459-463: updateRebootLog() appends `- <reason>` to the reboot-cause line, then clears g_bootRestartReasonValid.
- helperStuff.ino:723: doRestart() calls storeRebootReason(str).
- networkStuff.ino:414: config-portal timeout calls storeRebootReasonUnlessPending("WiFi: config portal timeout") before ESP.restart().
- OTGW-firmware.ino:130: captureRebootReason() at the top of setup().

Key semantics to preserve:
1. The record is consumed once per boot and cleared on every boot, so a reason is never reported twice.
2. It is only used when the current boot's reset reason is a software restart; a crash/WDT during doRestart()'s shutdown or a power-on reports no reason.
3. Capture happens at the very top of setup(), before any path that can restart (startWiFi portal timeout).
4. A restart that can fire before updateRebootLog() has run (portal timeout) passes on the taken-but-unlogged reason of the restart that started the chain instead of overwriting it; it stores its own reason only when none is pending.

## 2.0.0 target (origin/dev)
- src/OTGW-firmware/helperStuff.ino:627-659 doRestart(): add store call after the begin log line (before safebootMarkAppValid/flushSettings so a hang later still leaves the reason; the validity gate handles crashes).
- helperStuff.ino:296-380 updateRebootLog(): log_line_excpt is always empty on ESP32 (platform_esp32.h:532 platformResetExceptionInfo), so fill it with `- <reason>` when valid; keep the existing line format `... reboot cause: %s (%x) %s` and LOG_LINE_LENGTH 140 (48-char reason fits).
- networkStuff.ino:223-227 config-portal timeout: currently a bare platformRestart(); add storeRebootReasonUnlessPending("WiFi: config portal timeout").
- OTGW-firmware.ino:256 setup(): call captureRebootReason() as the first statement(s), before LittleFS mount/readSettings (315-318) and startWiFi.
- OTGW-firmware.h near :262 / :962: prototypes.
- Existing reasons that will then appear: "[nightly] scheduled restart" (OTGW-firmware.ino:905), "WiFi: too many reconnect failures" (networkStuff.ino:411), deferred reboots via helperStuff.ino:472-497 ("[OTA] Rebooting...", "[OTA] handover to safeboot", "Reboot after upgrade", /ReBoot via doRedirect).

## Platform mechanism (ESP32-S3)
No rtcUserMemory API. Use `static RTC_NOINIT_ATTR RebootReasonRtc g_rtcRebootReason;` (esp_attr.h). It survives ESP_RST_SW, panic and WDT resets and is garbage after power-on, so the magic check is mandatory; terminate reason[] before use. Validity gate: platformResetCode()/esp_reset_reason() == ESP_RST_SW. If dev still builds any ESP8266 target, keep the ESP8266 rtcUserMemory variant behind the Platform layer or #if; do NOT copy the 1.x RTC slot map comments (eboot/portal counter offsets) to ESP32.

## Risks
- Safeboot (ADR-191): an OTA handover restart boots the safeboot app first; whether RTC_NOINIT survives into the next app boot depends on safeboot's memory layout. Accept that the reason may be absent after a safeboot cycle; it must never be wrong (magic + ESP_RST_SW gate). Document the observed behaviour.
- Do not write to NVS/LittleFS in doRestart() for this (flash wear on reboot loops, and it adds latency in the shutdown path).
- ESP_RST_SW also covers a reset triggered by IDF internals (e.g. esp_restart from a library); a stale record could then be attributed, so clear the record on every boot, exactly as 1.x does.

## What NOT to port
- The 1.x TASK-1248 WiFi dead-link supervisor and its RTC counter (separate item).
- ESP8266 REASON_SOFT_RESTART constants, rtcUserMemory slot map, or ESP.getResetInfoPtr().
- Any change to the reboot_log.txt line format or LOG_LINES.

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/helperStuff.ino:627-659 doRestart(str) only prints str to telnet ([reboot] doRestart("%s") begin), then safebootMarkAppValid/flushSettings/flushDHWWaterMeter/prepareForReboot/platformRestart; the reason is not persisted anywhere
- origin/dev:src/OTGW-firmware/helperStuff.ino:296-380 updateRebootLog() builds 'reboot cause: %s (%x) %s' with log_line_excpt from platformResetExceptionInfo(); no restart-reason lookup
- origin/dev:src/libraries/Platform/src/platform_esp32.h:522-534 platformResetCode() = esp_reset_reason(); platformResetExceptionInfo() always returns an empty string on ESP32, so the trailing field of the log line is always empty on 2.0.0
- origin/dev:src/OTGW-firmware/networkStuff.ino:223-227 config portal timeout calls platformRestart() directly with no reason stored
- origin/dev:src/OTGW-firmware/networkStuff.ino:411 doRestart("WiFi: too many reconnect failures"); origin/dev:src/OTGW-firmware/OTGW-firmware.ino:905 doRestart("[nightly] scheduled restart"); origin/dev:src/OTGW-firmware/helperStuff.ino:472-497 requestDeferredReboot/performDeferredReboot -> doRestart(g_rebootReason) (OTA '[OTA] Rebooting...', '[OTA] handover to safeboot', FSexplorer 'Reboot after upgrade'): all these reasons are lost after the restart
- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:522-526 platformResetReason(lastReset) then updateRebootLog(lastReset), the insertion point for the boot side
- Searches with no hit on origin/dev: git grep -i 'restart reason|reboot reason|RTC_NOINIT|rtc_noinit|TASK-1247' over src, backlog/tasks, backlog/archive

## Platform notes

ESP32-S3 has no ESP.rtcUserMemoryRead/Write; use a static RTC_NOINIT_ATTR struct (esp_attr.h) with a magic word, which keeps its content across ESP_RST_SW/panic/WDT and is random after power-on (magic check required). Validity gate: esp_reset_reason()==ESP_RST_SW (platformResetCode() already returns it). The 1.x RTC slot map (eboot 0..31/64..95, portal counter 96..97) is ESP8266-only and must not be ported. Keep it behind the Platform layer (platform_esp32.h / platform_esp8266.h if dev still builds an ESP8266 target) or #if defined(ESP32) in helperStuff.ino. The safeboot OTA path (ADR-191) reboots into a separate safeboot app: an OTA handover restart lands in safeboot first, and RTC_NOINIT content may or may not survive into the app after safeboot finishes, depending on whether safeboot's linker places the same noinit region; the reason must be validated by magic and must not be assumed to cross that boundary. On dev the config-portal timeout uses platformRestart() directly (networkStuff.ino:227); readSettings/LittleFS mount happen at OTGW-firmware.ino:315-318, before startWiFi, so the capture call must sit at the very top of setup() before any path that can restart. The 1.x WiFi dead-link supervisor (TASK-1248) reasons do not exist on dev unless that is ported separately; this task covers only the reason plumbing.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. Every cited dev line checks out, and dev's only 'reboot reason' is the in-RAM g_rebootReason (TASK-394), which does not survive a restart. That name is similar but does not do the same job. The analyst missed one thing: dev ships no ESP8266 platform header. The draft's hedge 'if dev still builds an ESP8266 target' is therefore moot, and the RTC_NOINIT mechanism can go straight into platform_esp32.h/platform.h. Also, the 'WiFi supervisor' reason in the 1.x item comes from TASK-1248, which is not on dev. The draft correctly leaves it out of scope.

Remove the 'If dev still builds any ESP8266 target, keep the ESP8266 rtcUserMemory variant...' clause. origin/dev:src/libraries/Platform/src/ contains only platform_esp32.h, so put the store/take helpers behind the Platform layer (platform_esp32.h) or directly in helperStuff.ino. In the 'Existing reasons' list, credit g_rebootReason (helperStuff.ino:397, TASK-394) as the source of the deferred-reboot reasons. Everything else in the draft, including the file:line targets, matches origin/dev.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a doRestart("<reason>") on ESP32 (e.g. nightly restart forced via restart hour, or /ReBoot), the newest /reboot_log.txt line reads `reboot cause: Software reset (3) - <reason>`; shown on the .63 Classic+S3 bench and on the .61 OTGW32 bench, with the reboot_log.txt excerpt before (old firmware: no reason) and after (fix: reason) captured as evidence
- [ ] #2 After a power cycle and after a forced crash/WDT reset (e.g. telnet debug crash key or abort()), the newest line carries NO restart reason, even though a reason record was written earlier in RAM
- [ ] #3 A reason is reported exactly once: a second software restart without doRestart() (or a following boot) does not repeat the previous reason
- [ ] #4 Config-portal timeout chain: a boot that took over reason A and then restarts from the portal timeout leads to a log line on the next logged boot that names A, not "WiFi: config portal timeout"; a portal timeout with no pending reason logs "WiFi: config portal timeout" (verified by host harness slicing the real captureRebootReason/storeRebootReasonUnlessPending/updateRebootLog logic with a shimmed RTC struct and reset reason, red on old code, green on fix, or on the bench)
- [ ] #5 Behaviour across an OTA safeboot handover is observed on the bench and documented in the task notes (reason present or absent), and is never a wrong reason
- [ ] #6 No new NVS or LittleFS write in doRestart(); the record lives only in RTC_NOINIT memory
- [ ] #7 python build.py builds esp32, esp32-classic and esp32-combo targets clean and python evaluate.py --quick shows no new failures
<!-- AC:END -->
