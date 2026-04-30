**v1.5.0-beta is a development release. It is not recommended for production deployments.** A stable `v1.5.0` will follow when the line has soaked in the field.

## Update 2026-04-30: v1.5.0-beta.4 — master-topic filter for MQTT, log and REST state

This build (`1.5.0-beta.4+476c34f`) is a fresh release on top of v1.5.0-beta.2. It carries two targeted fixes that together resolve the "value flapping" reports from beta testers running v1.4.1 and v1.5.0-beta.

- **ADR-066: MQTT base topic gating by source and slave-echo** (TASK-478). Some OpenTherm MsgIDs (Tr, TrSet, TrSetCH2, MaxRelModLevelSetting, TrCH2, RFsensorStatus) have a Write-Ack data byte that is per-spec undefined; the boiler does not echo back a meaningful value. Since v1.4.1, those Write-Ack frames were treated as valid for the MQTT base topic, overwriting the legitimate Write-Data value and producing apparent oscillation between the thermostat-intent value and per-spec garbage. Fix: a new `is_value_valid_for_master_topic()` helper accepts only Read-Ack for OT_READ MsgIDs and only Write-Data for OT_WRITE / OT_RW MsgIDs at the base topic. Source-separated `/boiler` subtopic is gated independently by a new `bSlaveEchoesValue` flag in the MsgID lookup table.
- **ADR-066 extension to log decode and REST state** (TASK-483). Beta.3 (TASK-478) only fixed the MQTT base topic, but the WebUI consumes two upstream tiers — the OT-log scherm (via WebSocket) and the stats panel (via REST `/api/v2/otgw/otmonitor` reading `OTcurrentSystemState`) — that kept the broader pre-fix filter and continued to flap. Fix: `print_f88`, `print_s16`, `print_s8s8`, `print_u16` now cache `is_value_valid_for_master_topic` once per call and gate both the `AddLogf` decoded value and the state write on it. Non-master-topic messages emit the label only (no misleading `= value`); the protocol event remains visible for diagnostics.

Effect: REST stats panel for Tr / TrSet / TSet is stable, OT-log shows one decoded value per Write pair, READ-only MsgIDs are unchanged, MQTT regression-free. Memory-neutral after compiler optimization (Flash 69%, RAM 71% on ESP8266 D1 mini, identical to pre-fix baseline).

Same `eesz=4M2M` partition layout as v1.5.0-beta.2; a firmware-only OTA from the previous beta keeps your settings. A full firmware + filesystem flash is also fine.

---

## Update 2026-04-26: build refreshed with a DHCP fix

This build (`1.5.0-beta+cd30617`) replaces the original `1.5.0-beta+d40c2f6` artefacts on this release. It carries one targeted fix:

- **WiFi association without DHCP/IP after first reboot post-flash** (TASK-432). On the original build, some testers reported the device would associate with WiFi but not acquire an IP from DHCP after a reboot, requiring a forced re-association at the router side to recover. Root cause: `wifi_station_dhcpc_start()` in the WiFi reconnect path took DHCP ownership away from the SDK, so subsequent `setAutoReconnect()`-driven reassociations no longer auto-restarted DHCP. Fix: removed the call, returning to the v1.2.0 baseline pattern where the SDK manages DHCP autonomously.

Same `eesz=4M2M` partition layout as the original build; a firmware-only OTA from the previous build keeps your settings. A full firmware + filesystem flash is also fine.

---

The `1.5.x` line is the long-term-support track of OTGW-firmware on the ESP8266. It carries the `v1.4.x` feature set forward on **Arduino Core 2.7.4** instead of Core 3.1.2. Core 2.7.4 is the last Core version that ran this firmware without the post-OTA reboot reliability and PROGMEM alignment classes of issue that surfaced under Core 3.1.2 in the field.

The separate ESP32 / SAT `v2.0.0` exploration on `feature-dev-2.0.0-otgw32-esp32-sat-support` continues independently and is not affected by this LTS choice.

Full release notes: [RELEASE_NOTES_1.5.0-beta.md](RELEASE_NOTES_1.5.0-beta.md)

## What's different from v1.4.1

### Arduino Core 2.7.4 baseline
- Partition layout retained at `eesz=4M2M` (4 MB flash, 2 MB LittleFS) from v1.4.x; **v1.4.1 → 1.5.x needs no filesystem partition reformat**
- lwIP returns to the version shipped with Core 2.7.4 (the 2.2.0 update was Core 3.1.2-specific)
- PROGMEM byte-safe helpers (`pgm_strncmp_PP`, `pgm_read_char`) stay in place; correct on both Cores
- `mqttha.cfg` archive removed from the filesystem image (streaming HA discovery from v1.4.x supersedes it)

### Reboot reliability hardening
- **Deferred reboot with lifecycle heap snapshots** at 4 points around OTA-triggered reboot
- **Explicit service cleanup** before `ESP.restart()` (WebSocket, telnet, HTTP, MQTT torn down in defined order)
- **`ESP.reset()` fallback** for cases where `ESP.restart()` returns to caller (Core 3.1.x failure mode)
- **`WiFi.disconnect()` removed from reboot path** (it wipes NVRAM credentials on Core 3.1.x)
- **Nightly restart routes through `doRestart()`** so it benefits from the same cleanup
- Safety-tail delay restored after `ESP.restart()` so auto-reset window fires reliably

### MQTT publish gating tightening
- msgId 0 Status fan-out gate decoupled from `iInterval`, independent 60 s heartbeat
- msgId 5/6/100 bit-and-byte fan-out gating with 60 s heartbeat (Scope C-min)
- Minimum 1 s spacing between gated fan-out publishes
- Latest beta tightens to 250 ms spacing; `BGTRACE`/`OTTRACE` disabled by default in stability builds

### Home Assistant discovery for stats topics
- Discovery configs for `otgw-firmware/stats/*` metrics (heap levels, fragmentation, tier counters, discovery state)
- Pseudo-ID 247 stats discovery repaired
- `IS_PIC_ENTRY` flag honoured in HA discovery `stat_t` generators
- Legacy `mqttha.cfg` archive pipeline removed
- New `sendMQTTDataPic()` helper centralises `otgw-pic/` publish sites

### WebUI design system and dark/light hardening
- Self-hosted Inter and JetBrains Mono (no external font CDNs)
- Design system tokens centralise colours, spacing, typography
- Dark theme: scrollbar, placeholder, `.input-changed` contrast, `color-scheme` declaration
- Light theme: input contrast, mobile header toggle overlap
- Cross-browser hardening for theme toggling (Chrome, Firefox, Safari)
- Log render hotpath: WebUI restore buffer capped at 10k entries
- Per-message console logs gated behind `otgwDebug.verbose`

### Boot and loop diagnostics
- `logBootSignature()` boot telemetry (reset reason, SDK version, sketch size, free heap)
- `BGTRACE` per-phase timing in `doBackgroundTasks` and main loop
- `processOT` sub-trace with per-phase heap/time deltas
- All compiled in but off by default in stability builds

### Library bumps
- SimpleTelnet submodule to `25a0250` (printf stack 256)

## Upgrade notes

`v1.4.1` and `1.5.x` share the same `eesz=4M2M` partition layout, so the upgrade does **not** require a filesystem partition reformat. A firmware-only OTA (`*.ino.bin` via the Web UI) keeps existing settings untouched. A full OTA (firmware + filesystem) brings the WebUI design system updates as well; export your settings beforehand if you flash the filesystem image, since the new image is a fresh content bundle. The exact recommended procedure will be confirmed with `v1.5.0` stable. **Users staying on `v1.4.1` need no action.** `v1.4.1` continues to be the latest stable release.

## How to test

If you want to help shake out the LTS line on Core 2.7.4:

1. Flash a non-production device.
2. Run it alongside your `v1.4.1` production gateway on the same broker (different `MQTT Unique ID`).
3. Report observations on Discord `#beta-testing`. Field data is what gets the line to stable.

There is no published GitHub release for `v1.5.0-beta` at this time. Build locally with `python build.py` or pull a build from CI when offered.

## Thank you

Thanks to everyone on Discord who flagged reboot and stability issues on Core 3.1.2 in field deployments. The decision to maintain a Core 2.7.4 LTS line is a direct response to that feedback.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
