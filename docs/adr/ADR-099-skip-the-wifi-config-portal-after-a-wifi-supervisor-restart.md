---
id: "ADR-099"
title: "Skip the WiFi config portal after a WiFi supervisor restart"
status: "Accepted"
date: "2026-10-21"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-047"
  - "ADR-061"
topics:
  - "wifi"
  - "config-portal"
  - "wifimanager"
  - "wifi-recovery"
  - "restart"
aliases:
  - "portal after supervisor restart"
  - "12 minute outage"
  - "access point outage recovery"
  - "skip portal"
components:
  - "Boot-time WiFi connect policy in startWiFi() (portal or no portal)"
  - "Skip-portal marker in RTC user memory, written before a WiFi supervisor restart"
symbols:
  - "startWiFi"
  - "loopWifi"
  - "WIFI_SKIP_PORTAL_RTC_SLOT"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-099 Skip the WiFi config portal after a WiFi supervisor restart

## Status

Accepted, 2026-10-21.

## Status History

```yaml
status_history:
  - date: 2026-10-21
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-21
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-047
    changed_via: adr-kit lifecycle
  - date: 2026-10-21
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-061
    changed_via: adr-kit lifecycle
  - date: 2026-10-21
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

When the access point disappears for longer than a few minutes, a gateway stays
offline for about 11 minutes, and for up to 4 minutes after the access point is
back. The cause is the boot sequence that follows the WiFi supervisor's restart,
not the router.

The chain, measured on the bench gateway (1.7.8-beta.5, MikroTik access-list
deny from 21:50:17 to 21:58:19 on 2026-10-21, TASK-1262):

1. `loopWifi()` retries `WiFi.begin()` every 30 s and calls
   `doRestart("WiFi: too many reconnect failures")` after 10 failures
   (ADR-047, ADR-061): about 21:55:20.
2. The boot tries the saved network for `timeOut / 2` = 120 s in `startWiFi()`
   and fails, because the access point still refuses: about 21:57:20.
3. `startWiFi()` then opens the WiFiManager config portal for 240 s. The
   library switches the station (STA) side off while the portal runs when the
   station is not connected (`_disableSTAConn = true`, `WiFiManager.cpp`
   lines 719-725). The access point allowed the gateway again at 21:58:19, but
   the gateway could not rejoin.
4. The portal times out and restarts the board: about 22:01:20. The gateway
   was connected at 22:01:27. Total outage 11 min 10 s.

This matches the field reports on GH #681 (theaprilhare): outages of 12 and
24 minutes that ended in a "Software/System restart".

The intermediate restart is also invisible. `updateRebootCount()` and
`updateRebootLog()` run after `startWiFi()` (`OTGW-firmware.ino` lines
187-188), so a boot that ends in the portal timeout is never counted, and its
reason is dropped in favour of the pending supervisor reason
(`storeRebootReasonUnlessPending()`, by design since TASK-1247).

A library update does not help. The same code and the same fixed default are
in WiFiManager v2.0.17 (latest release, 2024-03-02) and in master
(2026-02-25); there is no setter for `_disableSTAConn`. Upstream issue #706
("background sta connect, reconnect to saved ap on power failure") has been
open since 2018. On #1752 (closed 2025-04-20) the library maintainer advised
handling this in user code: "User code logic, reboot counter, autoconnect loop
for 60seconds etc. reboot and retry".

ADR-017 decided: "If configured WiFi unavailable, retry for configured period.
After max retries, fall back to AP mode." That rule is right for a gateway that
was never configured or whose credentials no longer match, and wrong for a
configured gateway whose access point is briefly away.

## Decision Drivers

* A heating gateway must be back on the network as soon as the access point is
  back; minutes of avoidable outage cost Home Assistant control and data.
* A user must still be able to reach the config portal without a cable when the
  WiFi credentials are wrong.
* Minimal change surface: no change to the vendored WiFiManager library.
* The restart chain should stay readable in `reboot_log.txt`.

## Considered Options

* Option A: skip the portal on the boot that follows a WiFi supervisor restart.
* Option B: shorten the portal timeout from 240 s to 120 s.
* Option C: keep the station side on while the portal runs and close the
  portal as soon as the station connects (library change or a fork).
* Do nothing.

## Decision Outcome

Chosen option: **Option A**, because it removes the portal from exactly the
path where it does harm (a configured gateway whose access point is away), keeps
it on every path where a person may need it, and needs no library change.

Before the WiFi supervisor restarts the board, it writes a marker into RTC
(real-time clock) user memory, which survives a software restart but not a
power cycle. `startWiFi()` reads and clears the marker at the start of every
boot. When the marker is present and the saved network cannot be reached
within the direct-connect window, the boot does not open the portal; it
continues without WiFi and `loopWifi()` keeps retrying every 30 s, restarting
after 10 failures as before. The boot after a power cycle, the first boot
without credentials and the triple-reset portal request behave as before.

### Confirmation

* Bench: the same access-list deny test as in the Context, on the old build and
  on the new one. The new build rejoins within one `loopWifi()` retry interval
  (30 s) after the access point allows it again, and the log shows that the
  portal was skipped.
* Bench: a power cycle with an unreachable network still opens the portal.
* Build and `python evaluate.py --quick` green.

## Decision Contract

### Must

* Write the marker only on the WiFi supervisor's restart paths in
  `networkStuff.ino`: the `WIFI_FAILED` restart in `loopWifi()` and the
  dead-link restart.
* Read and clear the marker once per boot, before the portal decision in
  `startWiFi()`.
* Keep the portal for the triple-reset request, for a boot without saved
  credentials, and for a boot without the marker.
* Log one line when the portal is skipped.

### Must Not

* Must not modify the vendored WiFiManager library.
* Must not keep the marker across more than one boot.

### Exceptions

* None.

### Verification

* `startWiFi()` and the two restart sites in `src/OTGW-firmware/networkStuff.ino`.
* The bench test named under Confirmation, recorded in TASK-1262.

## Consequences

### Positive

* A configured gateway is back within about 30 s after its access point
  returns, instead of up to 4 minutes later.
* An outage of the access point no longer produces a restart that neither the
  boot count nor the reboot log shows: with no portal on this path there is no
  portal-timeout restart.

### Negative

* A user who changed the WiFi password does not get the portal automatically
  while the gateway is in the supervisor loop. The portal still opens after a
  power cycle (the marker does not survive it), or at once with the triple
  reset.
* A boot without WiFi now runs the rest of `setup()` without a network. The
  services already handle a later reconnect through
  `refreshServicesAfterWifiReconnect()`; the bench test must show that setup
  completes and the services come up after the reconnect.

## Pros and Cons of the Options

### Option A: skip the portal after a supervisor restart

* Good, because recovery follows the access point within one retry interval.
* Good, because the portal stays available on every path a person uses to
  reconfigure.
* Bad, because a changed password needs a power cycle or a triple reset.

### Option B: shorten the portal timeout

* Good, because it is one number.
* Bad, because the window only shrinks: about 9 minutes instead of 11.
* Bad, because a shorter portal gives a person less time to reconfigure.

### Option C: station on during the portal, close the portal on connect

* Good, because it would fix the behaviour for every path.
* Bad, because it needs a library change, which the library itself warns can
  hang channel scanning and the soft access point.
* Bad, because the library maintainer calls the auto-reconnect path unreliable
  (issue #1752).

### Do nothing

* Good, because nothing changes.
* Bad, because every access point outage longer than about 5 minutes keeps
  costing up to 4 extra minutes, plus a restart nobody can see in the logs.

## Open Questions

None.

## Related Decisions

* ADR-017: WiFiManager for initial configuration. This ADR narrows its point 3
  (fall back to AP mode after max retries) for the boot after a supervisor
  restart; the rest of ADR-017 stands.
* ADR-047: non-blocking WiFi reconnect (`loopWifi()`), which keeps retrying on
  the boots where the portal is skipped.
* ADR-061: reconnect timeout tuning (10 attempts of 30 s before the restart).

## References

* TASK-1262 bench notes (2026-10-21): access-list deny test, timeline and
  reboot log.
* GH #681 (theaprilhare): 12 and 24 minute outages ending in a self-restart.
* `src/OTGW-firmware/networkStuff.ino`: `startWiFi()`, `loopWifi()`.
* `src/OTGW-firmware/OTGW-firmware.ino` lines 187-188: boot count and reboot
  log written after `startWiFi()`.
* WiFiManager 2.0.15-rc.1 `WiFiManager.cpp` lines 719-725; same code in
  v2.0.17 and master: https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp
* https://github.com/tzapu/WiFiManager/issues/706
* https://github.com/tzapu/WiFiManager/issues/1752
