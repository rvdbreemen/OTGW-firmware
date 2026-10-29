---
id: TASK-1455
title: >-
  feat-2.0.0: port TASK-1262 (1.x) — skip the WiFi config portal on the boot
  after a WiFi supervisor restart and finish setup() offline
status: To Do
assignee: []
created_date: '2026-10-29T06:28:00Z'
updated_date: '2026-10-29T06:30:00Z'
labels:
  - port-from-1.x
  - wifi
  - boot
  - reliability
  - esp32
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 615000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source (1.x, branch otgw-1.x.x): commits 1ccde89c1 and 508af3c7c, ADR-099 (1.x numbering), TASK-1262, GitHub #681.

The 1.x mechanism has four parts:
- networkStuff.ino:268-285 defines RTC user-memory slot 115 with magic 0x4F545350 'OTSP', and wifiMarkSkipPortal() / wifiTakeSkipPortal(). The take function reads and clears the mark, so it applies to one boot only.
- Both WiFi supervisor restart paths mark the next boot before doRestart(): networkStuff.ino:864-865 (loopWifi, 10 failed reconnects) and :717-718 (dead-link probe).
- startWiFi() takes the mark first (:291). If it is set, the saved network is not back after the direct-connect window, credentials are saved and this is not a triple reset (portalSkipped, :402), startWiFi() skips the WiFiManager portal. It also skips the final 240 s WL_CONNECTED wait (:437), so setup() finishes without WiFi and loopWifi() retries every 30 s.
- The 1.x side fixes: telnet binds with debugTelnet.begin(false) (:963), and refreshServicesAfterWifiReconnect() restarts mDNS when it is not running (:493).

Why: WiFiManager 2.0.17 switches the station off while the portal runs. A gateway whose AP was away therefore could not rejoin when the AP returned. 1.x bench result (.88.68, AP blocked 14 min): the old build opened the portal and came back 194 s after the block ended; the fix came back after 7 s. A power cycle, first install or triple reset still gets the portal.

State on origin/dev (2.0.0):
- Already present: telnet debugTelnet.begin(false) at origin/dev:src/OTGW-firmware/networkStuff.ino:631. Do not re-port.
- Missing in release builds (no _VERSION_PRERELEASE), which is what 2.0.0 GA ships:
  - loopWifi WIFI_FAILED does doRestart after 10 retries (networkStuff.ino:368-381, 410-411).
  - startWiFi() then tries a direct connect for timeOut/2 = 120 s (:171-189).
  - It then opens manageWiFi.startConfigPortal(thisAP) for 240 s (:228-235) with the station down, and restarts again on timeout.
  - On the portal path, the final WL_CONNECTED wait (:242-251) blocks for up to another 240 s.
  - Same WiFiManager version as 1.x (platformio.ini:33 WiFiManager @ 2.0.17).
- Prerelease builds (version.h:10 alpha.525) currently avoid the portal through the beta AP fallback (:211-226, dev TASK-250). That fallback is beta-only (dev TASK-314) and recovers slowly: WIFI_AP_FALLBACK retries STA only every 300 s (:336, :417-425) and calls WiFi.disconnect() between retries (:432).

What to change on dev (networkStuff.ino):
1. Add a one-boot skip-portal mark. Use an RTC_NOINIT_ATTR uint32_t compared to a magic word: it survives esp_restart() and is undefined after power-on. Write it immediately before doRestart("WiFi: too many reconnect failures") (:411). Read and clear it at the top of startWiFi() (:83). Optionally require esp_reset_reason() == ESP_RST_SW. Do NOT use NVS / platformRtcWrite: it costs a flash write and survives a power cycle, so it would wrongly skip the portal after unplugging.
2. In startWiFi(), compute portalSkipped = !wifiConnected && skipMark && wifiSaved && !forcePortal before the portal branch (:209). When it is set, log it, do not call startConfigPortal(), and skip the final WL_CONNECTED wait (:242-251). Keep WiFi.setAutoReconnect(true), WiFi.persistent(true) and platformWifiDisableSleep() so that loopWifi() (state WIFI_IDLE -> WIFI_DISCONNECTED) takes over after setup().
3. Decide and document the prerelease interaction. A supervisor-restart boot in a prerelease build today goes into the AP fallback with 5-min STA retries. Recommended: keep the AP fallback for a cold boot. A boot with the skip mark should skip the fallback too and let loopWifi retry STA every 30 s. Make this an explicit decision in the task notes.
4. mDNS: check on the bench whether arduino-esp32 3.x MDNS.begin() succeeds and answers after a setup() that ran without an IP. Only if it does not, add an mDNS restart in the WIFI_RECONNECTED state (:384-405), which today restarts telnet/MQTT/WS but not mDNS (startMDNS at :638-650).
5. Check that every other service started in setup() comes back after an offline setup(): WebSocket, MQTT, NTP, the HTTP server, and LLMNR. In particular, check anything that, like 1.x SimpleTelnet, refuses to start without WiFi.
6. OLED and status pixel: a skipped boot must not show a stale portal screen or the purple AP colour.
7. Ethernet units: do not set the mark when state.net.eMode == NET_ETHERNET. loopWifi already returns early in that case (:325-334).

What NOT to port:
- The 1.x dead-link probe restart (handleWifiProbeVerdict, TASK-1248). Dev has no equivalent restart path.
- ESP.rtcUserMemory slot numbering and the helperStuff.ino slot-map comment.
- LEAmDNS-specific code (MDNS.isRunning()) unless step 4 shows it is needed.
- 1.x commit a1d756077 (TASK-1301, SDK auto-connect race). It is a separate change with its own evaluation.

Risks:
- A gateway whose saved credentials are genuinely wrong, and which hits a supervisor restart, gets no portal on that boot. The next power cycle or a triple reset restores it; that is the same accepted trade-off as 1.x ADR-099.
- An ADR is needed on the dev numbering, mirroring 1.x ADR-099, because it changes the boot/portal contract (ADR-043 triple reset, ADR-047 nonblocking reconnect).

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/networkStuff.ino:631 already has debugTelnet.begin(false) (telnet part ported)
- origin/dev:src/OTGW-firmware/networkStuff.ino:209-238 release-build path (no _VERSION_PRERELEASE): !wifiConnected -> manageWiFi.startConfigPortal(thisAP), and platformRestart() on timeout. No skip-portal mark exists
- origin/dev:src/OTGW-firmware/networkStuff.ino:211-226 prerelease-only (#if defined(_VERSION_PRERELEASE)): wifiSaved && !forcePortal -> startAPFallback() and return, so current alpha builds avoid the portal by a different route (dev TASK-250)
- origin/dev:src/OTGW-firmware/networkStuff.ino:368-381 loopWifi: release build -> WIFI_FAILED after 10 retries -> doRestart (line 411); prerelease -> AP fallback after 2 retries, then WIFI_AP_FALLBACK retries only every 300 s (timerAPRetry line 336) and WiFi.disconnect() between retries (line 432)
- origin/dev:src/OTGW-firmware/networkStuff.ino:242-251 final WL_CONNECTED wait of timeOut (240 s) runs unconditionally on the portal-success path
- origin/dev:src/OTGW-firmware/networkStuff.ino:638-650 startMDNS has no isRunning/restart logic; WIFI_RECONNECTED (lines 384-405) restarts telnet/MQTT/WS but not mDNS. Whether arduino-esp32 3.x MDNS.begin() fails without an IP is NOT verified
- origin/dev:src/OTGW-firmware/version.h:10 _VERSION_PRERELEASE alpha.525 (the AP fallback is what runs today)
- origin/dev:backlog/tasks/task-314 '[SEC-L2] Randomize or remove AP fallback password before 2.0.0 GA': the AP fallback is beta-only; GA builds fall back to the portal path
- origin/dev:platformio.ini:33 WiFiManager @ 2.0.17 (same library version the 1.x commit message names for the STA-off-in-portal behaviour)
- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:95-130 shouldForceWifiConfigPortal persists through readWifiPortalResetState/platformRtcWrite (NVS-backed per the comment at line 79-80); no RTC_NOINIT_ATTR usage found in src/OTGW-firmware
- Searched origin/dev backlog/tasks and docs/adr for ADR-099, skip portal and supervisor restart: no sibling task found (only TASK-250 AP fallback, TASK-314)

## Platform notes

ESP32-S3 has no ESP.rtcUserMemory. Use an RTC_NOINIT_ATTR uint32_t in RTC slow memory instead. It survives esp_restart() and is undefined after power-on, so compare it to a magic word and clear it on read. Optionally gate it on esp_reset_reason() == ESP_RST_SW. Do NOT use the NVS-backed platformRtcWrite path: it costs a flash write per restart and survives a power cycle, which would wrongly skip the portal after unplugging.

Restart paths on dev: loopWifi WIFI_FAILED (networkStuff.ino:410-411) in release builds. Dev has no dead-link probe restart (TASK-1248 not on dev), so there is only one restart path to mark. The prerelease AP-fallback branch (networkStuff.ino:211-226) already returns without the portal. Decide explicitly whether a supervisor-restart boot in prerelease builds should also skip the AP fallback, so that STA keeps retrying every 30 s.

Ethernet (HAS_ETH_CAPABLE) units: loopWifi returns early when NET_ETHERNET is active, so do not mark a skip when the Ethernet transport is active.

mDNS: arduino-esp32 3.x MDNS (ESP-IDF mdns component) may well work without an IP and announce on IP_EVENT. Verify on the bench before porting the isRunning restart. The 1.x LEAmDNS fix may simply not apply.

OLED and status pixel: configModeCallback is never called on a skipped boot. Make sure the OLED or pixel shows a 'no WiFi, retrying' state, not a stale portal screen.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. Every cited dev mechanism is real. Some line numbers are off by a few lines: prerelease branch 205-220 (not 211-226), portal 222-229 (not 228-235), final wait 241-250 (not 242-251), prerelease/release retry split 369-385.

The analyst left one point implicit. In prerelease builds, loopWifi never reaches WIFI_FAILED, because it enters the AP fallback after 2 retries (:369-378). So on today's alpha builds no supervisor restart happens at all, and the skip mark only has an effect in release (GA) builds. This makes the red/green AC depend on a release-configured build. The prerelease decision is about whether the boot-time AP fallback should be kept, not about the restart mark.

Calling it partial is fair: the telnet begin(false) part is already on dev. The rest has not been ported, and no dev task covers it.

1) Correct the dev line refs in the description: prerelease AP-fallback branch is origin/dev networkStuff.ino:205-220, portal :222-229, final WL_CONNECTED wait :241-250, retry split :369-385, WIFI_RECONNECTED :389-408.
2) Change 'mirroring 1.x ADR-099' to name it as the 1.x ADR-099 (WiFi portal skip). Dev's own ADR-099 is the HA discovery friendly-name ADR, so the dev ADR needs a new number and must not be confused with it.
3) Add a note: in prerelease builds loopWifi never reaches WIFI_FAILED (it switches to AP fallback after 2 retries), so the skip mark has no effect there. Step 3 therefore concerns only the boot-time AP-fallback branch (:205-220).
4) AC 2 must state that it runs on a build without _VERSION_PRERELEASE. Otherwise the 'old' side never restarts and never shows the portal.
5) AC 4 cites a 'nightly restart'. Check that dev has such a restart path, or use a REST/settings-triggered reboot instead.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A boot that follows a 'WiFi: too many reconnect failures' restart, where the saved AP is still unreachable, does not open the WiFiManager config portal. setup() completes, and loop()/loopWifi() run within the direct-connect window (about 120 s) plus normal setup time. Verified on the bench (.63 Classic+S3 and .61 OTGW32) with the AP blocked on the MikroTik router; the telnet log shows the skip message and loopWifi retry attempts.
- [ ] #2 Red/green bench proof on the same reproduction (AP access-list deny long enough to trigger the supervisor restart, then allow), run against a release-configured build (no _VERSION_PRERELEASE) or with the prerelease path made equivalent: the old build opens the portal SSID (laptop WiFi scan) and rejoins only after the portal timeout; the fix build never shows the portal SSID and rejoins within 35 s after the allow. Times recorded in the task notes.
- [ ] #3 A power cycle with the AP unreachable, a first install without saved credentials, and a triple reset still open the config portal. Bench-verified at least for the power cycle and the triple reset.
- [ ] #4 The skip mark applies to exactly one boot: a software restart that was not a WiFi supervisor restart (for example nightly restart or a settings-triggered restart) with the AP unreachable behaves as before. Verified by log.
- [ ] #5 The skip mark is held in RTC memory (RTC_NOINIT_ATTR), not in NVS; no NVS/flash write is added on the restart path (code review plus grep).
- [ ] #6 After the AP returns on a boot that ran setup() offline: telnet port 23, HTTP port 80, WebSocket, MQTT (online LWT and data published) and NTP time all work again without a further restart, and <hostname>.local resolves through mDNS. If mDNS did not resolve without a fix, the fix is added and shown red/green.
- [ ] #7 The prerelease AP-fallback interaction is decided and documented in the task notes (skip mark also bypasses the AP fallback, or not). The chosen behaviour is bench-verified on an alpha build.
- [ ] #8 Ethernet-active units (HAS_ETH_CAPABLE, NET_ETHERNET) do not set the skip mark and their boot is unchanged.
- [ ] #9 A dev-numbered ADR mirroring 1.x ADR-099 is drafted via adr-kit as Proposed and accepted only by the maintainer.
- [ ] #10 build.py / build.bat green for esp32, esp32-classic and esp32-combo, and python evaluate.py --quick shows no new failures.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Do together with TASK-1450 (both touch the WiFi supervisor), per the port evaluation.
<!-- SECTION:NOTES:END -->
