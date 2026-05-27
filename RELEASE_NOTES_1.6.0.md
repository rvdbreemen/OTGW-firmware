# OTGW-firmware v1.6.0 Release Notes

**Release date:** 2026-05-28
**Branch:** main (from dev)
**Compare:** [v1.5.0-fix2...v1.6.0](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.5.0-fix2...v1.6.0)

## Overview

v1.6.0 is a significant feature release that fixes long-standing MQTT and Home Assistant integration issues, adds static IP address configuration, introduces a bilateral OpenTherm bus support map, and systematically improves mainloop responsiveness. It ships after 25 beta builds and extensive community testing on Discord.

## Bug Fixes

**MQTT and Home Assistant**

- **HA entities no longer flap `unavailable`** (ADR-074, regression since v1.5.0): `DHW Control`, `Thermostat`, and all HA sensor entities were flapping `unavailable` whenever the boiler stopped talking. HA entity availability now reflects only the ESP-to-MQTT link (birth/LWT), not OpenTherm-bus liveness. OT-bus liveness remains on the dedicated `otgw_connected` sensor.
  - **Contract change:** consumers reading the base `<toptopic>/value/<nodeid>` topic as OT-bus liveness must migrate to the `otgw_connected` sensor.
- **HA capability-flag bits 2-5 stuck at `unknown`** (ADR-076, #614): the global MQTT status fanout rate gate suppressed per-bit publishes on subsequent MsgID 5 frames. The rate gate is dropped and per-bit publish is scoped to all three pending types so cooling, OTC active, CH2 active, and summer/winter all reach their retained topics on every status change.
- **MQTT proxy-answer (no-B) routing** (ADR-075, #599): MsgIDs without a boiler response now route to the correct worldview topic instead of going silent. Root cause behind PR #565.
- **MsgID 0 Status canonical publish** gated on boiler-side worldview so the canonical topic stops flapping on thermostat-only frames (TASK-633, #604).
- **JIT discovery stall** fixed (ADR-073): the just-in-time trigger now applies the same `hasConfig` filter as the force path so both enqueue an identical ID set, preventing phantom-ID re-pickup loops.
- **HA discovery verify** now runs an hourly first-run trigger in addition to the existing force-path, recovering any entities missed by the JIT pass without manual intervention (TASK-704).
- **Set-command debug surfacing**: silently-dropped MQTT set-commands now appear in the default debug stream (#602).

**Web UI and diagnostics**

- **LittleFS size reported as 1 MB** instead of the correct 2 MB in the device-info API and Web UI; the partition size is now read from the LittleFS descriptor (TASK-701).
- **Auto-scroll reset** when switching tabs or navigating back to the main page; scroll position is now preserved (TASK-701).
- **`/api/v2/device/info` premature 503** under moderate heap fragmentation: the over-conservative 8 KB contiguous-block precheck is reduced to the existing pbuf-sized safety threshold (TASK-723).
- **`logHeapStats` window counters**: was printing ephemeral window-reset drop counters instead of monotonic lifetime totals; now consistent with `/api/v2/heap` and MQTT stats topics (TASK-697, #642).
- **Statistics column proportions and badge styling** refined after the support-map feature landed (TASK-705, TASK-706).
- **"Update Firmware" button** hidden on touch-capable desktops; the touch-class CSS media query no longer suppresses the upload control (GitHub #575, #598).
- **Settings form labels** left-aligned with consistent flex layout across the settings tab (TASK-724, TASK-735).
- **Fixed-IP prefill** handles API failures gracefully and defaults the subnet mask to `255.255.255.0` when no current subnet can be inferred (TASK-735).

**Build and flash tools**

- `flash_otgw.sh` / `flash_otgw.bat` hardened: spec parity, SHA256 integrity verification, version-aware binary selection, COM port detection via Windows registry, auto-download when binary not found locally (#570).
- `flash_otgw.bat` now searches both the script directory and the `build\` subdirectory for firmware binaries, and correctly handles PowerShell pipe operators (#570, TASK-735).
- `build.py` auto-initialises missing git submodules so a fresh clone builds without manual `git submodule update` (#594).
- `evaluate.py` false-positive and stale-check fixes; CI gate is now meaningful again (#592).

## New Features

- **Static IP address settings** (TASK-548, TASK-709): configure a fixed IP, subnet, gateway, and up to two DNS servers in the firmware settings. Applied before WiFiManager connects. The settings page shows a "Use DHCP" toggle; unchecking it reveals the IP fields pre-filled from the current DHCP lease. Each address uses four segmented number inputs (0-255 per octet) with auto-advance, backspace navigation, paste support, ARIA labels for screen-reader accessibility, and per-field range validation before save.
- **Bilateral OT-bus support map** (TASK-683-688, #640): bitmaps tracking which OpenTherm MsgIDs are seen from the thermostat side, the boiler side, or both. Telnet view labels each data point "T / B / T+B". New `GET /api/v2/otgw/support-map` REST endpoint exposes the bitmaps. Web UI shows which data points your system is actually exchanging.
- **HA PIC-control entities** (pseudo-ID 251, #596): new `button` and `select` discovery configs expose the PIC reset and mode controls as proper Home Assistant entities.
- **Standalone HA discovery topic wiper** (TASK-611, #587): one-shot helper for cleaning stale retained discovery topics out of the broker.
- **Statistics table drag-to-resize columns** (TASK-703): grab any column header edge to resize it; preferences are saved in localStorage.
- **`/beta-prerelease` skill and GitHub Action** (#607, #612): tag-driven and workflow-dispatch-driven beta publishing with draft-first asset attachment and inline release-digest.

## Internal Improvements

- **Pure JIT MQTT discovery** (ADR-073): only non-OT pseudo-IDs queue at boot; OT MsgID discovery configs publish on first MsgID reception, reducing boot-time MQTT traffic.
- **Mainloop responsiveness audit** (TASK-651-674, #617/#626/#633/#635): all blocking `delay()` / `delayMs()` calls on the cooperative path replaced with non-blocking timer checks; `handleOTGW()` drain loops bounded at four lines per call; `String` removed from hot paths; `emergencyHeapRecovery()` is now a real recovery routine; webhook timeout tightened to 500 ms.
- **`resetgateway` hardened**: now requires payload `"1"` and is rate-limited to one PIC reset per 5 seconds.
- **Dead code removed**: inactive SAT subsystem scaffolding cleaned out of `dev` (#586, #589).
- **Telnet diagnostic noise reduced**: `onNotFound` logs accurate HTTP status; JSON API no longer mirrors to telnet; hash-match is silent (#640).
- **PROGMEM / heap discipline** pass across `OTGW-Core.ino` and FSexplorer (#637).
- **ADR-079 accepted**: `emergencyHeapRecovery()` defined as real recovery (drop OTGWstream client, skip one discovery-drip tick) instead of the previous "yield + log" no-op.
- **ADR-080 accepted**: the 15 s `MQTTclient.setSocketTimeout()` documented as a known sync-blocker bounded by the 42 s retry gate; replacing PubSubClient is out of scope for the 1.6.x line.

## Breaking Changes

1. **HA entity availability contract change**: HA entity availability (`avty_t`) now reflects the ESP-to-MQTT link only. Consumers that read the base `<toptopic>/value/<nodeid>` topic as an OT-bus liveness indicator must migrate to the `otgw_connected` binary sensor (ADR-074).
2. **`resetgateway` payload requirement**: the MQTT `resetgateway` command now requires payload `"1"`. Commands with any other payload are logged and ignored. This matches the HA-discovery `payload_press` value already in use, so HA automations created after v1.5.0 are unaffected.

## Upgrade Notes

- **HA entity availability**: after flashing, check that `otgw_connected` is in your HA sensor list. If you have automations that trigger on the base MQTT topic going unavailable as a proxy for OT-bus health, redirect them to `otgw_connected`.
- **Stale MQTT topics**: if you are upgrading from before v1.5.0, run the standalone discovery topic wiper (TASK-611) or press the "Force re-announce" button in the Web UI to clean stale retained topics from the broker.
- **Static IP (optional)**: static IP is off by default. No action required unless you want a fixed address; the DHCP default is unchanged.
- No filesystem reflash is needed. LittleFS partition layout is unchanged from v1.5.0.
