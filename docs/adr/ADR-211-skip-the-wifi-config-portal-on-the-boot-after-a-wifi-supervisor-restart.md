---
id: "ADR-211"
title: "Skip the WiFi config portal on the boot after a WiFi supervisor restart"
status: "Proposed"
date: "2026-11-04"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "wifi"
  - "config-portal"
  - "wifimanager"
  - "wifi-recovery"
  - "restart"
  - "ap-fallback"
aliases:
  - "skip portal"
  - "portal after supervisor restart"
  - "access point outage recovery"
  - "1.x ADR-099 port"
components:
  - "Boot-time WiFi connect policy in startWiFi() (portal, AP fallback or neither)"
  - "One-boot skip-portal mark in RTC_NOINIT memory, written before a WiFi supervisor restart"
symbols:
  - "startWiFi"
  - "loopWifi"
  - "wifiMarkSkipPortal"
  - "captureWifiSkipPortal"
  - "wifiSkipPortalDecide"
  - "g_rtcWifiSkipPortal"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-211 Skip the WiFi config portal on the boot after a WiFi supervisor restart

## Status

Proposed, 2026-11-04. Guideline-level (ADR-080) with a test gate:
`tests/test_wifi_skip_portal.py`. Only the maintainer accepts it.

## Status History

```yaml
status_history:
  - date: 2026-11-04
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

This is the dev (2.x) counterpart of the 1.x ADR-099 "Skip the WiFi config
portal after a WiFi supervisor restart" on branch `otgw-1.x.x`. Dev's own
ADR-099 is a different decision (HA discovery friendly names), so the 2.x
decision gets its own number.

The WiFi supervisor in `loopWifi()` (ADR-047) restarts the gateway in two cases:

1. `WIFI_FAILED`: 10 reconnect attempts of 30 s failed,
   `doRestart("WiFi: too many reconnect failures")`. Release builds only: a
   prerelease build enters its AP fallback after 2 attempts instead.
2. The dead-link check (TASK-1450): the link stays associated but passes no
   traffic, `doRestart("WiFi: connected but no traffic")`. Both build types.

The boot after such a restart runs `startWiFi()`. It tries the saved network for
`timeOut / 2` = 120 s. When the access point is still away, a release build opens
the WiFiManager config portal for 240 s, and a prerelease build starts the beta
AP fallback (dev TASK-250). WiFiManager 2.0.17 (`platformio.ini`, the same version
as on 1.x) switches the station off while the portal runs when the station is
not connected. So a gateway that sits in the portal cannot rejoin an access point
that comes back. The portal times out, restarts the board, and only the next boot
reconnects. The final `WL_CONNECTED` wait after the portal blocks `setup()` for
up to another 240 s.

On 1.x the same chain cost 11 min 10 s of outage for an 8 min access-point
block, measured on the 1.x bench (1.x ADR-099). With the fix the 1.x bench
reconnected 7 s after the block ended, against 194 s for the old build. These
are 1.x figures. Dev has not been measured yet; that is the bench test under
Verification.

Constraints on dev: the ESP32-S3 has no `ESP.rtcUserMemory`. The existing
`platformRtcWrite()` is backed by NVS: it costs a flash write and survives a power
cycle, so a mark stored there would wrongly skip the portal after unplugging.
TASK-1447 (reboot reason) and TASK-1450 (dead-link restart count) already keep
one-restart records in `RTC_NOINIT_ATTR` memory, checked with a magic word and
`platformResetWasSoftware()`.

## Decision

A WiFi supervisor restart marks the next boot. That boot opens neither the config
portal nor the prerelease AP fallback when the saved network is still away after
the direct-connect window. `setup()` finishes without WiFi, and `loopWifi()`
retries the saved network every 30 s.

- `wifiMarkSkipPortal()` writes `{magic "OTSP", marked 1}` into
  `g_rtcWifiSkipPortal` (`RTC_NOINIT_ATTR`). It is called on the line directly
  before both supervisor `doRestart()` calls in `networkStuff.ino`, and does
  nothing while Ethernet is the active link (`state.net.eMode == NET_ETHERNET`).
- `captureWifiSkipPortal()` runs at the top of `setup()`, next to
  `captureRebootReason()` and `captureWifiDeadRestarts()`. It always clears the
  record and keeps the mark only after a software restart
  (`platformResetWasSoftware()`).
- In `startWiFi()`, `wifiSkipPortalDecide()` takes the boot flag and returns
  `marked && !wifiConnected && wifiSaved && !forcePortal`. When it is true,
  `startWiFi()` logs one line, skips the prerelease AP-fallback branch and the
  portal, still sets auto-reconnect, `persistent` and radio-awake, and skips the
  final 240 s wait.

### Prerelease builds: the mark also bypasses the boot-time AP fallback

On a prerelease build, the only supervisor restart is the dead-link restart:
`WIFI_FAILED` is never reached there. The decision is to let the mark bypass the
boot-time AP fallback too. The reasons:

- In the AP fallback, `loopWifi()` retries the saved network only every 300 s and
  disconnects in between (`WIFI_AP_FALLBACK` / `WIFI_AP_RETRY`). Without the
  fallback, the marked boot starts in `WIFI_IDLE`, sees no link and retries every
  30 s.
- The unit does not become unreachable for a person: when 2 of those retries fail
  (about 60 s), the existing prerelease rule in `loopWifi()` starts the same AP
  fallback. The bypass therefore changes only the first minute after
  `setup()`. It does not give endless 30 s retries on a prerelease build.
- The mark then means the same on both build types: rejoin the saved network
  first.

This is a maintainer decision to confirm. If the fallback should stay on the
marked boot, the prerelease branch in `startWiFi()` moves above the skip check;
`test_prerelease_marked_boot_skips_ap_fallback` names the behaviour to flip.

## Decision Contract

### Must

- Write the mark only directly before the two WiFi supervisor `doRestart()`
  calls in `networkStuff.ino` (dead link, `WIFI_FAILED`).
- Keep the mark in `RTC_NOINIT_ATTR` memory with a magic word, accept it only
  after a software restart, and clear it at every boot.
- Keep the portal for the triple reset (ADR-043), for a boot without saved
  credentials, after a power cycle and after any other restart.
- Log one line when the portal is skipped.
- Write no mark while Ethernet is the active link.

### Must Not

- Must not store the mark in NVS, LittleFS or settings (`platformRtcWrite()`,
  `Preferences`).
- Must not keep the mark across more than one boot, or across a second
  `startWiFi()` call in the same boot.
- Must not modify the vendored WiFiManager library.

### Exceptions

- None.

### Verification

- `tests/test_wifi_skip_portal.py`: compiles the real skip block and the real
  `startWiFi()` on the host (release and prerelease), and the real `loopWifi()`
  through the `test_wifi_deadlink.py` harness. Red on the old code: the marked
  boot opens the portal and restarts, and the restart sites write no mark.
- Bench (TASK-1455 AC1, AC2): MikroTik access-list deny until the supervisor
  restarts, then allow, on a release-configured build. The old build opens the
  portal SSID and rejoins only after the portal timeout; the new build never
  shows the portal SSID and rejoins within 35 s of the allow.

## Alternatives Considered

- **Shorten the portal timeout.** The outage only shrinks, and a person gets
  less time to reconfigure. Rejected, as on 1.x.
- **Keep the station on during the portal and close it on connect.** Needs a
  WiFiManager change, which the library warns can hang channel scanning and the
  soft AP. Rejected, as on 1.x.
- **Store the mark with `platformRtcWrite()` (NVS).** Costs a flash write on
  every supervisor restart, and the mark survives a power cycle, so an unplugged
  gateway would wrongly skip the portal. Rejected.
- **Keep the prerelease AP fallback on the marked boot.** Possible; slower
  rejoin (300 s retries from the start). See the prerelease section above.

## Consequences

**Positive:**

- A configured gateway whose access point was away rejoins within one 30 s
  retry after the access point returns, instead of after a portal timeout.
- The supervisor restart becomes visible: the marked boot completes `setup()`,
  so `updateRebootCount()` and `updateRebootLog()` record it, where a
  portal-timeout restart was never logged.

**Negative:**

- A gateway whose WiFi password changed gets no portal on the boot after a
  supervisor restart. A power cycle or a triple reset still opens it.
- On a release build a gateway that stays away keeps cycling: offline `setup()`,
  10 retries, restart, 120 s direct-connect window. That is the same cycle as
  1.x and is the accepted trade-off.
- `setup()` now runs without a network on this path. Telnet, HTTP, WebSocket,
  MQTT, NTP, mDNS and LLMNR start without an IP; `WIFI_RECONNECTED` restarts
  telnet, MQTT and WebSocket, but not mDNS or LLMNR. That every service comes back
  after the access point returns, `<hostname>.local` included, is not verified
  yet: it is a bench check (TASK-1455 AC6).

## Open Questions

- [ ] Robert: confirm that the skip mark also bypasses the prerelease boot-time
  AP fallback (recommended above), or keep the fallback on the marked boot.

## Related Decisions

- 1.x ADR-099 (branch `otgw-1.x.x`): the same decision on the 1.x line.
- ADR-017: WiFiManager for initial configuration. This narrows its "fall back to
  AP mode after max retries" for the boot after a supervisor restart.
- ADR-043: triple reset recovery, which still forces the portal.
- TASK-1645 (GitHub #154 point 1, #131): an opt-in setting `wifinoportal` (default off) gives the same skip on every
  boot with saved credentials, not only the one after a supervisor restart. It reuses `wifiSkipPortalDecide()`;
  no saved credentials and the triple reset still open the portal.
- ADR-047: non-blocking WiFi reconnect, which takes over after an offline setup().

## References

- `src/OTGW-firmware/networkStuff.ino`: `startWiFi()`, the TASK-1455 block,
  `handleWifiProbeVerdict()`, `loopWifi()` `WIFI_FAILED`.
- `src/OTGW-firmware/OTGW-firmware.ino`: `setup()` record captures.
- `tests/test_wifi_skip_portal.py`.
- 1.x ADR-099:
  `git show origin/otgw-1.x.x:docs/adr/ADR-099-skip-the-wifi-config-portal-after-a-wifi-supervisor-restart.md`.
- WiFiManager station-off behaviour while the portal runs:
  https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp and
  https://github.com/tzapu/WiFiManager/issues/706
