MQTT and Home Assistant reliability overhaul, static IP address support, bilateral OT-bus diagnostics, and 25 beta builds of field-tested polish.

Full release notes: [RELEASE_NOTES_1.6.0.md](RELEASE_NOTES_1.6.0.md)
README and setup: [README.md](README.md)
API documentation: [docs/api/](docs/api/)

## Bug Fixes

- **HA entities no longer flap `unavailable`** (ADR-074, regression since v1.5.0): availability now reflects the MQTT link, not OpenTherm-bus liveness. OT-bus liveness moves to the dedicated `otgw_connected` sensor. See [upgrade notes](#upgrade-notes) for the contract change.
- **HA capability-flag bits 2-5 (cooling, OTC active, CH2 active, summer/winter) stuck at `unknown`** (ADR-076, #614): global MQTT rate gate dropped; per-bit publish now scoped correctly.
- **MQTT proxy-answer (no-B) routing** (ADR-075, #599): MsgIDs without a boiler response now route to the correct worldview topic.
- **JIT discovery stall** fixed (ADR-073): `hasConfig` filter aligned on both trigger paths; no more phantom-ID re-pickup loops.
- **LittleFS size fixed**: Web UI and API were reporting 1 MB instead of 2 MB (TASK-701).
- **`/api/v2/device/info` premature 503** under moderate heap fragmentation corrected (TASK-723).
- **`logHeapStats` now prints lifetime drop totals** instead of ephemeral window counters (TASK-697, #642).
- **"Update Firmware" button** visible again on touch-capable desktops (#575, #598).
- **Flash scripts hardened**: `flash_otgw.sh` / `flash_otgw.bat` now verify SHA256 integrity, auto-select the correct binary version, and support auto-download (#570).
- **Settings form labels** left-aligned with consistent flex layout (TASK-724, TASK-735).
- **Fixed-IP subnet prefill** now defaults to `255.255.255.0` and handles API failures gracefully (TASK-735).

## New Features

- **Static IP address settings** (TASK-548, TASK-709): configure fixed IP, subnet, gateway, and DNS servers. Applied before WiFiManager connects. Smart prefill from current DHCP lease with segmented octet inputs, ARIA accessibility, and per-field validation.
- **Bilateral OT-bus support map** (TASK-683-688, #640): see which MsgIDs your thermostat and boiler are actually exchanging, with "T / B / T+B" labels in telnet and `GET /api/v2/otgw/support-map`.
- **HA PIC-control entities** (#596): `button` and `select` discovery configs for PIC reset and mode controls under pseudo-ID 251.
- **Standalone HA discovery topic wiper** (#587): one-shot helper for cleaning stale retained topics from the broker after an upgrade.
- **Statistics table drag-to-resize columns** (TASK-703): column width preferences saved in localStorage.

## Internal Improvements

- **Mainloop responsiveness**: four rounds of audits replaced all blocking `delay()` / `delayMs()` calls on the cooperative path; `handleOTGW()` drain loops bounded at four lines per call; `String` removed from hot paths; `emergencyHeapRecovery()` is now real recovery (TASK-651-674).
- **Pure JIT MQTT discovery** (ADR-073): OT MsgID configs publish on first MsgID reception, not on connect.
- `resetgateway` now requires payload `"1"` and is rate-limited to one reset per 5 seconds.

## Upgrade Notes

1. **HA availability contract change**: consumers using the base MQTT topic as an OT-bus liveness proxy must migrate to the `otgw_connected` binary sensor. HA automations created after v1.5.0 are unaffected.
2. **`resetgateway` payload**: must now send payload `"1"`. Matches the HA-discovery `payload_press` value, so HA button automations are unaffected.
3. No filesystem reflash required. LittleFS partition layout unchanged from v1.5.0.

## Thank You

Special shoutout to **andrebrait** for their relentless testing of the static IP settings UI across five beta iterations! Their detailed bug reports drove beta.22 through beta.25 and shaped the final form of the fixed-IP configuration page.

Thanks to everyone who contributed to this release through bug reports, testing, and feedback:

- **rkuijer** (GitHub) - found and reported the "Update Firmware" button hidden on touch-capable desktops (#575)
- **andrebrait** (Discord) - extensive fixed IP UI testing and feedback through multiple beta cycles
- **simontemplar6623** (Discord) - reported the LittleFS 1 MB display bug, the statistics column drag feature request, and the auto-scroll reset issue
- **crashevans** (Discord) - beta testing with detailed logs
- **renehonig** (GitHub) - reported MQTT publishing intervals behaviour
- **tjfsteele** (GitHub) - feature request for password protection settings

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped test and diagnose issues across 25 beta builds deserve enormous credit for making this release rock-solid.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support, discussion, and beta testing.
