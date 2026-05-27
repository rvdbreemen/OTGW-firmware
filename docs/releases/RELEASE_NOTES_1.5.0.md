# OTGW-firmware v1.5.0 Release Notes

**Release date:** 2026-05-08
**Branch:** main (from dev)
**Compare:** [v1.4.1...v1.5.0](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.4.1...v1.5.0)
**Previous release:** [v1.4.1](RELEASE_NOTES_1.4.1.md)

---

## Overview

v1.5.0 is the first stable release of the `1.5.x` long-term-support line. It runs on **Arduino Core 2.7.4**, the Core version that ran this firmware reliably for years before the move to Core 3.1.2 in `v1.4.x`. Field experience on Core 3.1.2 surfaced post-OTA reboot reliability issues and stricter PROGMEM alignment that caused `Exception (3)` crashes; Core 2.7.4 does not exhibit those classes of issue.

The `1.5.x` line carries forward the full `v1.4.x` feature set and adds a significant body of MQTT improvements, Home Assistant discovery refinements, new diagnostic capabilities, and hardened reboot logic. The separate ESP32 / SAT `v2.0.0` exploration continues independently on the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch.

---

## New features

### Sibling-suffix MQTT source topic shape (ADR-070, ADR-071)

When the `Separate Sources` setting is enabled, source-variant topics for dual-source OpenTherm message IDs now use a flat sibling-suffix shape instead of nested children:

- Old: `<topTopic>/value/<id>/TSet/thermostat` and `<topTopic>/value/<id>/TSet/boiler`
- New: `<topTopic>/value/<id>/TSet_thermostat` and `<topTopic>/value/<id>/TSet_boiler`

The old nested discovery topic shape was silently rejected by HA's `TOPIC_MATCHER` regex, so source-variant entities never actually appeared in Home Assistant under v1.4.x. The sibling-suffix shape is what makes them register for the first time.

### Worldview semantics for /thermostat and /boiler subtopics (ADR-069)

Each source-variant subtopic now reflects the correct actor perspective: `/thermostat` carries the thermostat's intent value, `/boiler` carries the boiler's echo. Previously, which actor's value appeared in which subtopic was inconsistent across message ID families.

### Human-readable HA discovery friendly names (ADR-072)

All HA MQTT discovery payloads now use human-readable Title Case names with spaces instead of raw underscore-separated identifiers. Examples:

- Old: `OTGW_TdhwSet`, `OTGW_Status_Master_Memberid_Code`
- New: `DHW Setpoint`, `Status Master MemberID Code`

The `unique_id` in every discovery payload is unchanged. Existing automations and entity-ID references are not broken by this name change. MDI icons have been added for all sensor entities.

### HA discovery for firmware and PIC diagnostic topics (TASK-540)

The PIC diagnostic topics (`otgw-pic/`) and firmware diagnostic topics (`otgw-firmware/`) now ship with full HA auto-discovery so they appear as proper HA sensors instead of raw values in MQTT Explorer. Includes reboot count, PIC firmware version, boiler ID, and diagnostic metrics.

### New REST endpoint: GET /api/v2/debug (TASK-536)

A single-call diagnostic dump endpoint for troubleshooting and field support. Returns stack high-water mark, heap state, MQTT and WebSocket stats, and the last 20 lines of the OT log in a structured JSON response.

### Compact telnet welcome banner (TASK-545)

The telnet welcome screen now shows a compact log-triage snapshot: last reset reason, heap state, MQTT connection status, and a summary of the most recently seen OpenTherm message IDs. All debug toggles are listed inline so no separate key reference is needed.

### No-Python flash scripts

`flash_otgw.sh` (macOS/Linux) and `flash_otgw.bat` (Windows) handle the full flash workflow without requiring a Python environment: automatic serial port detection, filesystem binary first, then firmware binary. `build.sh` and `build.bat` similarly wrap the build workflow.

---

## Bug fixes

### Master MQTT topic flapping (ADR-066, TASK-478, TASK-561)

The MQTT base topic for `Tr` (room temperature), `TrSet`, `TrSetCH2`, `MaxRelModLevelSetting`, and analogous master-to-slave informational write messages was oscillating between the real value and `0`. Root cause: `is_value_valid()` was widened in v1.4.1 to accept slave Write-Ack values for `OT_WRITE` and `OT_RW` messages. For those message IDs, the boiler per-spec returns an undefined Write-Ack byte, not the real value. Fix: `is_value_valid_for_master_topic()` accepts only Read-Ack for OT_READ and only Write-Data for OT_WRITE / OT_RW at the base topic.

A subsequent fix (TASK-561) corrected an enum-family misclassification that was silencing valid Write-Ack publications on a related subset of message IDs.

This fix was extended (TASK-483) to the WebUI OT-log screen (WebSocket) and the stats panel (REST `/api/v2/otgw/otmonitor`) so all three layers are consistent.

### TSet bSlaveEchoesValue flip for heat-pump stability (TASK-571)

MsgID 1 (`TSet`) was incorrectly classified as having a meaningful slave echo. Heat-pump boilers that do not echo the thermostat's `TSet` write were causing the base topic to flap between the thermostat's intent value and the boiler's non-echo. `bSlaveEchoesValue` is now `false` for MsgID 1, which stabilises the base topic on heat-pump setups.

### WiFi: DHCP not acquired after first reboot post-flash (TASK-432)

On the initial `1.5.0-beta+d40c2f6` build, some devices would associate with WiFi but fail to acquire a DHCP lease after the first reboot, requiring a forced router-side reconnect. Root cause: `wifi_station_dhcpc_start()` in the WiFi reconnect path took DHCP ownership away from the SDK. Fix: removed the call, returning to the v1.2.0 pattern where the SDK manages DHCP autonomously.

### WiFi: TCP listeners re-bound on reconnect (fix(wifi))

WiFi reconnect events were calling `bind()` on already-bound HTTP and TCP listener sockets, producing port-already-in-use errors on the second and subsequent reconnects. Fix: socket state is checked before re-binding.

### GW=R queue entry never cleared (TASK-538 queue fix)

After a `GW=R` PIC reset command, the PIC responds with `SC=`/`SR=` frames. These never matched the `GW=` queue prefix, so the queue entry was never cleared. The firmware would re-issue the reset every 5 seconds, looping up to 12 times (observed in beta.5 field logs). Fix: `GW=R` is now treated as fire-and-forget; the queue entry is removed immediately after the command is sent.

### WebSocket reload-storm churn (fix(webui))

Rapid page reloads could trigger a burst of simultaneous WebSocket reconnects, saturating the ESP8266's TCP connection table. A 250 ms reconnect debounce and a `pagehide` shutdown handler prevent the storm.

### Non-monotonic timestamps in _debugBOL (fix(debug))

Debug timestamps wrapping across a one-second boundary produced out-of-order entries in the telnet log. Fixed by using a monotonic millisecond counter for intra-second ordering.

---

## Internal improvements

### Reboot reliability hardening

A series of fixes addresses the post-OTA reboot reliability issues that motivated the LTS effort:

- **Deferred reboot with lifecycle heap snapshots** captures heap and flash state at four points around an OTA-triggered reboot.
- **Explicit service cleanup before `ESP.restart()`** tears down WebSocket, telnet, HTTP, and MQTT in a defined order.
- **`ESP.reset()` fallback path** for cases where `ESP.restart()` returns to the caller.
- **`WiFi.disconnect()` removed from the reboot path** (it wipes NVRAM credentials on Core 3.1.x).
- **Nightly restart routes through `doRestart()`** so it benefits from the same cleanup and snapshot machinery.

### MQTT publish gating tightening

- msgId 0 Status fan-out gate decoupled from `iInterval` with an independent 60 s heartbeat.
- msgId 5/6/100 bit-and-byte fan-out gating with 60 s heartbeat (Scope C-min).
- Minimum 1 s spacing between gated fan-out publishes; tightened to 250 ms in beta builds.
- Force-discovery requests routed through the drip publisher with `maxBlock` throttle to prevent log flooding.
- Smart MQTT republish: `requestMQTTRepublishAll()` is gated on 5 minutes offline to avoid unnecessary topic storms on transient reconnects.

### /gateway sub-topic removed (TASK-538)

The per-message-ID `/gateway` sub-topic has been removed. The canonical base topic now carries the value that was previously published on `/gateway`. See `docs/BREAKING_CHANGES.md` for migration instructions.

### Home Assistant discovery improvements

- HA discovery extended to source-variant entities via sibling-suffix shape (first time these entities register in HA).
- HA discovery extended to `/thermostat` and `/boiler` worldview topics.
- `IS_PIC_ENTRY` flag honoured in HA discovery `stat_t` generators.
- Pseudo-ID 247 stats discovery repaired.
- `sendMQTTDataPic()` helper centralises all `otgw-pic/` publish sites.

### WebUI design system

- Self-hosted Inter and JetBrains Mono fonts (no external CDN dependency).
- Design system tokens centralise colours, spacing, and typography across light and dark themes.
- Dark theme: scrollbar styling, placeholder colour, `.input-changed` contrast, `color-scheme` declaration.
- Light theme: input contrast, mobile header toggle overlap.
- WebUI log render hotpath: restore buffer capped at 10 000 entries.

### Boot and loop diagnostics

- `logBootSignature()` boot telemetry (reset reason, SDK version, sketch size, free heap at earliest `setup()`).
- `BGTRACE` per-phase timing in `doBackgroundTasks` and the main loop (off by default in stability builds).
- `processOT` sub-trace with per-phase heap and time deltas (off by default in stability builds).

### Library bumps

- **SimpleTelnet** submodule updated to `25a0250` (printf stack raised to 256 bytes).

---

## Breaking changes

Three breaking changes vs v1.4.1. See `docs/BREAKING_CHANGES.md` for full migration instructions.

1. **MQTT source-topic shape changed to sibling-suffix** (ADR-070, ADR-071): `<msgid>/thermostat` and `<msgid>/boiler` are now `<msgid>_thermostat` and `<msgid>_boiler`.
2. **`/gateway` sub-topic removed** (TASK-538): the canonical base topic replaces it.
3. **HA discovery entity names changed to Title Case** (ADR-072): display names update automatically; `unique_id` and entity IDs are unchanged.

---

## Upgrade notes

`v1.4.1` and `v1.5.0` share the same `eesz=4M2M` flash and partition layout. The upgrade **does not require a filesystem partition reformat**. The `WARNING: flash filesystem FIRST` rule from the v1.4.1 notes does not apply here.

Recommended procedure:

1. Flash the **firmware binary** (`*.ino.bin`) via the Web UI update page. Existing settings are preserved.
2. Flash the **filesystem binary** (`*.littlefs.bin`) via the same page. This brings the updated WebUI font hosting and removes `mqttha.cfg`. Export your settings beforehand; the filesystem image is a fresh content bundle.

After flashing, open the Web UI and trigger a discovery republish (Settings page or `POST /api/v2/mqtt/republish`) to push the updated entity names and new discovery configs to HA.

If `bSeparateSources` is enabled, also clear the old retained discovery topics at the nested paths using `mosquitto_pub`. See `docs/BREAKING_CHANGES.md` for the exact commands.

---

## Architecture Decision Records

New ADRs accepted in this release cycle (ADR-065 through ADR-072). See `docs/adr/` for details.

| ADR | Decision |
|-----|----------|
| ADR-065 | OTGW PIC MQTT subtree layout |
| ADR-066 | MQTT publish gating by source and slave echo |
| ADR-067 | HA discovery state reconciliation on OTA upgrade |
| ADR-068 | `bSeparateSources` mutually exclusive base and source variants |
| ADR-069 | MQTT source topic worldview semantics |
| ADR-070 | MQTT source topic sibling-suffix shape |
| ADR-071 | MQTT discovery topic sibling-suffix shape (supersedes ADR-070) |
| ADR-072 | HA discovery friendly name format |

---

## Thank you

Special shoutout to **andrebrait** for being the relentless beta tester who was there from day one: reporting the DHCP/WiFi regression that became a targeted fix, catching the friendly name inconsistencies and driving them to Title Case perfection, providing HA integration screenshots throughout, and confirming that the Core 2.7.4 reboot path is solid. This release is materially better because of that level of engagement.

Thanks to everyone who contributed through testing, logs, and feedback:

- **crashevans** (Discord) — systematic stress testing across beta.2, beta.5, beta.11, and beta.20 with detailed long-term logs that confirmed stability between iterations
- **_reuzenpanda_** (Discord) — reported the source-topic flapping issue and provided telnet logs that pinpointed the ADR-071 fix; confirmed the sibling-suffix topics in HA
- **fuzzyduck** (Discord) — raised the entity naming compatibility question and confirmed the Title Case transition works without breaking existing automations
- **simontemplar** (Discord) — spotted the partition size documentation contradiction in the initial beta build
- **@andrebrait** (GitHub) — filed the WiFi/DHCP reconnect issue with enough reproduction detail to diagnose it immediately

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose, test, and verify.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
