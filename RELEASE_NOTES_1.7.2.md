# OTGW-firmware v1.7.2 Release Notes

**Release date:** 2026-07-30
**Branch:** main (from otgw-1.x.x)
**Compare:** [v1.7.1...v1.7.2](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.1...v1.7.2)

## Overview

A long-run stability release for the 1.x (ESP8266) line. v1.7.0 fixed heap *fragmentation*; this release fixes two genuine heap *leaks* that were still draining long-running devices to an out-of-memory reboot, typically after 1 to 1.5 hours of uptime. Both were found by tracing field captures to their root cause, and both are now confirmed on the bench.

If your gateway has been rebooting on a suspiciously regular schedule (an uptime-locked reboot around 90 minutes is the classic signature), this is the release to install.

Alongside the leak fixes, the periodic allocation churn on a long-running device is reduced: NTP resyncs once per day instead of every 30 minutes, and the web UI polls the device far less aggressively.

No settings migration is required. See "Behaviour changes" below for the one item worth reading if you poll the REST API from your own scripts.

## Bug fixes

- **Heap leak: the MQTT discovery-verify retry storm (the "true leak").** Long-running devices died of genuine memory exhaustion, distinct from the fragmentation the 1.7.0 gates already handle. Root cause: the automatic discovery-verify (ADR-062) subscribed to `homeassistant/+/<node>/#` to count retained discovery configs. Under the reduced PubSubClient buffer it read back only a fraction of them, falsely concluded the rest were missing, and triggered a full discovery republish that re-armed every hour. Verify to false-missing to republish leaked heap until the device ran out and the external watchdog reset it. The verify readback is removed entirely. The daily auto-heal is now an unconditional, heap-gated drip republish of the retained configs (guarded on MQTT connected, no drip already in progress, and a healthy largest-contiguous block), and the hourly first-run retry is deleted. No wildcard subscribe, no count, no false-missing, no retry storm. Bench-confirmed: a 5 hour soak against a real broker holds free heap flat where the previous build died at about 80 minutes. (TASK-1037, TASK-1048, ADR-087)
- **Heap leak: DHCP-supplied NTP servers (option 42).** On networks whose DHCP server advertises an NTP server (Pi-hole and several D-Link routers do this by default), every DHCP lease renewal leaked memory. The prebuilt lwIP2 in Arduino core 2.7.4 is compiled with `LWIP_DHCP_GET_NTP_SRV=1`, so each renewal pushed the router's NTP server into the SDK's SNTP module and leaked on the way. The field signature is an uptime-locked onset at the first lease renewal (commonly around 90 minutes) followed by a reset, which looks like the leak above but is a separate cause. The firmware runs its own NTP client, so DHCP-supplied servers were never wanted: `sntp_servermode_dhcp(0)` is now called at the top of `setup()`, before the persistent-WiFi auto-connect can complete DHCP. (TASK-1050)
- **Crash when an mDNS answer arrived on an exhausted heap.** `Exception (2) epc1=0x40233cba excvaddr=0x8`. Plain `new` in ESP8266 core 2.7.4 returns NULL on failure but still runs the constructor, so the six `stcMDNS_RRAnswer` allocation sites in `_readRRAnswer()` dereferenced NULL. They now use `new (std::nothrow)` with a null guard, so an out-of-memory moment drops the mDNS answer instead of crashing the device; the callers already handled a NULL answer. The Arduino core tree is gitignored, so `build.py` re-applies this patch idempotently after every core install, the same mechanism as ADR-084. This is a safety net, not a leak fix: it was the visible crash at the end of the leaks above. (TASK-1049)
- **Gateway-mode and OTGW-connected discovery entities missing until a mode change.** Both Home Assistant discovery entities are now queued at boot, so they publish once and self-heal instead of appearing only after the gateway mode changed. (TASK-1035)

## Changes

- **NTP resyncs once per day instead of every 30 minutes.** The half-hourly resync was pure periodic allocation churn on a device that has no meaningful clock drift over a day. (TASK-1046)
- **Web UI polls the device much less.** The dashboard clock now ticks locally instead of asking the device for the time, and the poll rates are cut. Server-side, the two endpoints the UI drives (`/api/v2/otgw/otmonitor` and `/api/v2/device/time`) are rate-limited to 1 request per second. A couple of open dashboard tabs no longer add up to a continuous HTTP load on the ESP. (TASK-1043, TASK-1044, ADR-086)

## New

- **Per-second heap sampling in the onset window**, off by default. A diagnostic aid for leak hunting: it samples free heap every second inside the window where a leak onset is expected, which is what made the two leaks above visible in field captures. (TASK-1037)

## Internal improvements

- `scripts/capture-heap-soak.bat`: a browser-free, low-perturbation capture preset for confirming heap stability over a 24 hour soak on a fixed build. It complements `capture-heap-onset.bat`, which hunts the leak onset on an unfixed build. The distinction matters: the instrumentation itself perturbs the heap, so measuring a fix needs a quieter preset than finding a bug does. (TASK-1037, TASK-1041, TASK-1042, TASK-1045)

## Behaviour changes

**No breaking changes versus v1.7.1.** No MQTT topic renames, no REST API removals, no settings-format changes, no migration on upgrade.

One behaviour change is worth knowing about if you wrote your own tooling:

- `/api/v2/otgw/otmonitor` and `/api/v2/device/time` are rate-limited to 1 request per second. A caller that exceeds the budget receives HTTP `429` with an RFC 9457 `application/problem+json` body instead of the data. Every other REST endpoint is unaffected. If you poll either of these two endpoints faster than once per second from a script or a Home Assistant REST sensor, slow it to 1 second or handle the `429`. (ADR-086)

Home Assistant users are not affected: the integration path is MQTT, not REST polling.

## Upgrade notes

Flash both the firmware and the filesystem. The filesystem matters this time: the reduced web-UI polling lives in the web assets, so a firmware-only flash leaves the old polling behaviour in your browser. OTA via the web UI, or the merged binary over USB.

Nothing else is required. Settings are preserved, and the discovery drip republish heals the Home Assistant entities on its own within a day of running (or immediately, if you trigger a republish yourself).

To confirm the fix on your device, watch the uptime sensor. Where the leak previously produced a reboot at a repeatable 1 to 1.5 hours, uptime should now simply keep climbing.

## Thank you

Special shoutout to **martreides**, whose three telnet captures in Discord `#nederlandse-ondersteuning` are the reason this release exists. Reporting "it reboots every 1 to 1.5 hours" is useful. Capturing it three times, on two different firmware versions, with NTP disabled in one run to rule it out, is what turns a vague complaint into a findable bug. Both root causes were traced from those captures.

Thanks to everyone who contributed through reports, testing, and feedback:
- **martreides** (Discord) reported the reboot cycle and provided the captures that pinned both leaks
- **geo83_44083** (Discord) provided sustained stability testing and captures across the whole 1.7.x line, including the earlier reports that first pointed at long-run heap behaviour
- **crashevans**, **ties7944** and **richard_ha_** (Discord) ran validation logs through the 1.7.x beta cycle

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify.
