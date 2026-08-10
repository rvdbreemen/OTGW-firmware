Long-run stability release for the 1.x (ESP8266) line: two genuine heap leaks fixed, both of which drained long-running devices to an out-of-memory reboot after roughly 1 to 1.5 hours. v1.7.0 fixed heap *fragmentation*; this release fixes the *leaks* that were still left. No breaking changes versus v1.7.1.

If your gateway reboots on a suspiciously regular schedule, especially an uptime-locked reboot around 90 minutes, this is the release to install.

Full notes: [RELEASE_NOTES_1.7.2.md](https://github.com/rvdbreemen/OTGW-firmware/blob/v1.7.2/RELEASE_NOTES_1.7.2.md) . README: [README.md](https://github.com/rvdbreemen/OTGW-firmware/blob/v1.7.2/README.md) . Compare: [v1.7.1...v1.7.2](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.1...v1.7.2)

## Bug fixes

- **Heap leak: MQTT discovery-verify retry storm.** The automatic discovery-verify subscribed to `homeassistant/+/<node>/#` to count retained configs, read back only a fraction of them under the reduced PubSubClient buffer, falsely declared the rest missing, and triggered a full republish that re-armed every hour. That loop leaked until the device ran out of memory. The verify readback is gone; the daily auto-heal is now an unconditional, heap-gated drip republish, and the hourly retry is deleted. Bench-confirmed: 5 hour soak with free heap flat, where the previous build died at about 80 minutes. (TASK-1037, TASK-1048, ADR-087)
- **Heap leak: DHCP-supplied NTP servers (option 42).** On networks whose DHCP server advertises an NTP server (Pi-hole, several D-Link routers), every lease renewal leaked memory, because the prebuilt lwIP2 in core 2.7.4 is built with `LWIP_DHCP_GET_NTP_SRV=1` and pushed the router's server into the SDK SNTP module. The firmware runs its own NTP client, so those servers were never wanted: `sntp_servermode_dhcp(0)` is now called at the top of `setup()`. Field signature was an uptime-locked onset at the first lease renewal, typically around 90 minutes. (TASK-1050)
- **Crash on an mDNS answer arriving with an exhausted heap** (`Exception (2) epc1=0x40233cba excvaddr=0x8`). Plain `new` in core 2.7.4 returns NULL but still runs the constructor, so the six `stcMDNS_RRAnswer` allocation sites dereferenced NULL. They now use `new (std::nothrow)` with a null guard, so out-of-memory drops the answer instead of crashing. Applied to the core at build time, like ADR-084. (TASK-1049)
- **Gateway-mode and OTGW-connected discovery entities** are queued at boot, so they publish once and self-heal instead of appearing only after a mode change. (TASK-1035)

## Changes

- NTP resyncs once per day instead of every 30 minutes, cutting periodic allocation churn on long-running devices. (TASK-1046)
- The web UI ticks its clock locally and polls the device less often. Server-side, `/api/v2/otgw/otmonitor` and `/api/v2/device/time` are rate-limited to 1 request per second, so a couple of open dashboard tabs stop adding up to a continuous HTTP load. (TASK-1043, TASK-1044, ADR-086)

## New

- Per-second heap sampling in the leak-onset window, off by default. It is what made both leaks visible in field captures. (TASK-1037)

## Behaviour changes

No breaking changes. One thing to know if you wrote your own tooling: a caller exceeding the 1 req/s budget on `/api/v2/otgw/otmonitor` or `/api/v2/device/time` now gets HTTP `429` with an RFC 9457 `application/problem+json` body. All other endpoints are unaffected, and Home Assistant users are not affected because that path is MQTT, not REST polling. (ADR-086)

## Upgrade notes

Flash **both** firmware and filesystem. The filesystem matters here: the reduced polling lives in the web assets, so a firmware-only flash leaves the old behaviour in your browser. OTA via the web UI, or the merged binary over USB. Settings are preserved and no migration runs.

To confirm the fix, watch the uptime sensor. Where the leak previously produced a reboot at a repeatable 1 to 1.5 hours, uptime should now keep climbing.

## Thank you

Special shoutout to **martreides**, whose three telnet captures in Discord `#nederlandse-ondersteuning` are the reason this release exists. Reporting "it reboots every 1 to 1.5 hours" is useful. Capturing it three times, across two firmware versions, with NTP disabled in one run to rule it out, is what turns a vague complaint into a findable bug. Both root causes were traced from those captures.

Thanks to everyone who helped through reports, testing, and feedback:
- **martreides** (Discord) reported the reboot cycle and provided the captures that pinned both leaks
- **geo83_44083** (Discord) provided sustained stability testing and captures across the 1.7.x line
- **crashevans**, **ties7944** and **richard_ha_** (Discord) ran validation logs through the 1.7.x beta cycle

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
