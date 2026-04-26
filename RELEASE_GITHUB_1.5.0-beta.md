**v1.5.0-beta is a development release. It is not recommended for production deployments.** A stable `v1.5.0` will follow when the line has soaked in the field.

The `1.5.x` line is the long-term-support track of OTGW-firmware on the ESP8266. It carries the `v1.4.x` feature set forward on **Arduino Core 2.7.4** instead of Core 3.1.2. Core 2.7.4 is the last Core version that ran this firmware without the post-OTA reboot reliability and PROGMEM alignment classes of issue that surfaced under Core 3.1.2 in the field.

The separate ESP32 / SAT `v2.0.0` exploration on `feature-dev-2.0.0-otgw32-esp32-sat-support` continues independently and is not affected by this LTS choice.

Full release notes: [RELEASE_NOTES_1.5.0-beta.md](RELEASE_NOTES_1.5.0-beta.md)

## What's different from v1.4.1

### Arduino Core 2.7.4 baseline
- LittleFS partition returns to 1 MB (the v1.4.x 2 MB layout is Core 3.1.2-specific)
- lwIP returns to the 2.1.x branch
- PROGMEM byte-safe helpers (`pgm_strncmp_PP`, `pgm_read_char`) stay in place; correct on both Cores
- `mqttha.cfg` archive removed (streaming HA discovery from v1.4.x supersedes it)

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

## Upgrade notes (preliminary)

A complete tested upgrade procedure from `v1.4.1` will be published with `v1.5.0` stable. The 2 MB → 1 MB LittleFS partition change between v1.4.x and 1.5.x means filesystem flashing will be required; settings should be backed up via the Web UI's settings export before upgrading. **Users staying on `v1.4.1` need no action.** `v1.4.1` continues to be the latest stable release.

## How to test

If you want to help shake out the LTS line on Core 2.7.4:

1. Flash a non-production device.
2. Run it alongside your `v1.4.1` production gateway on the same broker (different `MQTT Unique ID`).
3. Report observations on Discord `#beta-testing`. Field data is what gets the line to stable.

There is no published GitHub release for `v1.5.0-beta` at this time. Build locally with `python build.py` or pull a build from CI when offered.

## Thank you

Thanks to everyone on Discord who flagged reboot and stability issues on Core 3.1.2 in field deployments. The decision to maintain a Core 2.7.4 LTS line is a direct response to that feedback.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
