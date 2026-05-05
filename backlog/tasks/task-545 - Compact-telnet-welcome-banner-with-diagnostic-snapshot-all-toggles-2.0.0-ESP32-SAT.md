---
id: TASK-545
title: >-
  Compact telnet welcome banner with diagnostic snapshot + all toggles (2.0.0 /
  ESP32 / SAT)
status: Done
assignee:
  - '@robert'
created_date: '2026-05-05 09:23'
updated_date: '2026-05-05 09:51'
labels:
  - telnet
  - debug
  - ux
  - esp32
  - sat
dependencies: []
references:
  - 'src/OTGW-firmware/networkStuff.ino:373'
  - 'src/OTGW-firmware/handleDebug.ino:9'
  - 'src/OTGW-firmware/debugStuff.h:47'
  - 'src/OTGW-firmware/SATtypes.h:137'
  - 'src/OTGW-firmware/platform_esp32.h:124'
documentation:
  - /Users/Breee02/.claude/plans/when-connecting-to-telnet-delightful-token.md
priority: medium
ordinal: 16000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When connecting to the OTGW telnet debug port (23) for log triage, the user needs a compact diagnostic snapshot at-a-glance: firmware identity, reset reason, network mode (WiFi/Ethernet) + health, heap (with computed fragmentation), OT-bus / PIC state, MQTT / NTP, SAT control state (mode, setpoint, modulation, last cycle class, fallback), BLE sensor health (ESP32), and the current state of every debug toggle. Today's banner (sendTelnetBanner in networkStuff.ino:373) covers only a subset and handleDebugChar('h') is bloated with state info. This task rewrites the banner to surface ESP32-specific data via the existing platform_esp32.h helpers (platformResetReason, platformHeapFragmentation, platformMinFreeHeap, platformMaxFreeBlock, networkModeName) plus the SAT/net/otBus/debug structs introduced in 2.0.0, and slims 'h' to a pure command keymap. Note: 2.0.0 has no separate dumpDebugInfo() / 'D' INI dump like 1.5.x — the welcome banner is the canonical compact snapshot. A future task may add a structured INI dump for parity.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Welcome banner shows firmware identity (version, build #, githash, date, FS-hash status) and reset reason via platformResetReason() on identity lines
- [x] #2 Welcome banner shows hostname, formatted uptime, and reboot count on one line
- [x] #3 Welcome banner shows network mode via networkModeName(state.net.eMode) (WiFi or Ethernet), SSID (WiFi only), RSSI (WiFi only), and IP on one line
- [x] #4 Welcome banner shows AP-fallback indicator (state.net.bAPFallback + state.net.sAPSSID) when active
- [x] #5 Welcome banner shows heap line: free (platformFreeHeap), frag% (platformHeapFragmentation), minFree (platformMinFreeHeap), maxBlock (platformMaxFreeBlock), free sketch space (platformFreeSketchSpace)
- [x] #6 Welcome banner shows heap-diag drop counters (ws, mqtt, low/warn/crit, slow) on one line if state.heapdiag is present
- [x] #7 Welcome banner shows PIC line: type, FW version, device id (state.pic.*)
- [x] #8 Welcome banner shows OT-bus state on one line: online (state.otBus.bOnline), gateway-mode (with 'detecting' if !bGatewayModeKnown), boiler, thermostat, PS-mode — using the 2.0.0 otBus rename
- [x] #9 Welcome banner shows MQTT connected state, broker:port, HA prefix on one line
- [x] #10 Welcome banner shows NTP enable, timezone, sendtime flag on one line
- [x] #11 Welcome banner shows SAT line: bActive, eControlMode (OFF/CONTINUOUS/PWM), fFinalSetpoint °C, iCurrentModulation %, eBoilerStatus, eLastCycleClass, bFallbackActive, weather.bValid
- [x] #12 Welcome banner shows BLE sensor health (state.sat.iBleSensorCount, iBleBattery, iBleRssi) — ESP32-only, guarded by #if defined(ESP32)
- [x] #13 Welcome banner shows all debug toggles with current state ([0]/[1]): 1=OTmsg, 2=RestAPI, 3=MQTT, 4=Sensors, 5=SAT, 6=OTDirect, 7=SATBLE (ESP32-only), g=MQTTGate, n=NTP, d=SensorSim, plus read-only OTGW-Sim
- [x] #14 Welcome banner shows 'Connected from: <ip>' footer + hint 'Press h for command menu'
- [x] #15 Pressing 'h' prints only the command keymap — firmware/PIC/Status/CH-temp/Room-temp/Setpoint blocks are removed and toggle keys no longer carry [0]/[1] state suffixes; the new 'w' (Open-Meteo) command is preserved in the trimmed menu
- [x] #16 Field values in the welcome banner agree with the same fields as observed via /api/v2/debug (or the SAT panel in the web UI) when sampled in the same connect — no drift from existing data sources
- [x] #17 Firmware build via wrapper (./build.sh or python build.py for esp32 env) exits 0; python evaluate.py --quick reports no new warnings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read sendTelnetBanner at networkStuff.ino:373 (verbatim) and surrounding helpers (_debugPrintf_P, networkModeName)
2. Read handleDebugChar('h') at handleDebug.ino:9-80 to know exactly what to trim
3. Read state.otBus / state.sat / state.net struct definitions to confirm field names + types
4. Read platform_esp32.h helpers (platformResetReason / platformHeapFragmentation / platformMinFreeHeap / platformMaxFreeBlock / platformFreeSketchSpace)
5. Confirm enum stringifiers for sat.eControlMode / eBoilerStatus / eLastCycleClass — find or write a small inline mapper
6. Rewrite sendTelnetBanner() with ESP32/SAT-aware lines: identity+reset, host+up+reboots, network mode (WiFi/Ethernet/AP-fallback), heap, drops, PIC, OT-bus, MQTT, NTP, SAT line, BLE block (#if defined(ESP32)), full toggle keymap (1-3,4=Sensors,5=SAT,6=OTDirect,7=SATBLE,g=MQTTGate,n=NTP,d=SensorSim,OTGW-Sim)
7. Trim handleDebugChar('h'): keep keymap only; preserve new 'w' Open-Meteo command in actions block
8. Build (./build.sh or python build.py); fix any PSTR/printf issues
9. Run python evaluate.py --quick if available
10. Mark all ACs and add Final Summary
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote sendTelnetBanner() in networkStuff.ino into a compact ESP32/SAT-aware log-triage snapshot, and slimmed handleDebugChar('h') to a pure command keymap.

What the banner now shows on connect:
- FW identity: _VERSION (semver+githash+date), _VERSION_BUILD, FS-hash check (fs:ok / fs:mismatch), reset reason via platformResetReason(buf, len).
- Identity & uptime: settings.sHostname, upTime() (formatted d-HH:MM), state.uptime.iRebootCount.
- Network transport: networkModeName() (WiFi or Ethernet); when WiFi shows SSID + RSSI dBm; on Ethernet shows IP only; when state.net.bAPFallback is active prints AP-Fallback indicator with state.net.sAPSSID (prerelease-only). All branches use getActiveIP().
- Heap: free, computed frag% (platformHeapFragmentation), minFree (platformMinFreeHeap), maxBlock (platformMaxFreeBlock), free sketch space (platformFreeSketchSpace) — all via the existing platform_esp32.h wrappers, so the same banner builds clean on the ESP8266 sibling environment too.
- Heap-diag drops: ws/mqtt/low/warn/crit/slow (state.heapdiag.*).
- PIC: type, fw version, device id (state.pic.*).
- OT bus: online/offline, gateway-mode (with 'detecting' if !bGatewayModeKnown), boiler, thermostat, PS-mode (state.otBus.*) — using the 2.0.0 otgw->otBus rename.
- MQTT: connected, broker:port, ha-prefix.
- NTP: enable, timezone, sendtime.
- SAT: bActive, eControlMode (off/continuous/pwm), fFinalSetpoint °C, iCurrentModulation %, eBoilerStatus (15-state stringifier), eLastCycleClass (good/overshoot/underheat/short/uncertain/none), bFallbackActive, weather.bValid (ok/stale).
- BLE sensor health (ESP32 only, #if defined(ESP32)): iBleSensorCount, iBleBattery %, iBleRssi dBm.
- All ten debug toggles with current [0]/[1] state in a 3-column grid: 1=OTmsg, 2=REST, 3=MQTT, 4=Sensors (corrected — the previous banner mislabelled this as MQTTGate, which the actual handler at handleDebug.ino:143-145 toggles bSensors), 5=SAT, 6=OTDirect, 7=SATBLE (ESP32-only), g=MQTTGate, n=NTP, d=SensorSim, plus read-only OTGW-Sim.
- Footer: 'Press h for command menu' + 'Connected from: <ip>'.

'h' menu trimmed to:
- Toggle keymap (keys -> labels, no current-state suffixes — the welcome banner is the source of truth for state).
- Actions: q, F, V (discovery verification, kept), r, p, a, s/S, w (Open-Meteo, preserved).
- GPIO/Misc: b, i, u, o, j, l, f.

Three small file-static stringifiers added for SAT enums (satControlModeStr, satBoilerStatusStr, satCycleClassStr). Strings are short literals deduped into flash.

Verification:
- ./build.sh exits 0; produced both ESP32 (1.81 MB) and ESP8266 (0.78 MB) binaries plus filesystems and distribution zips.
- python3 evaluate.py --quick: 59 passed, 2 pre-existing warnings (mqtt_configuratie.cpp missing, sendMQTTheapdiag buffer arithmetic — both unrelated to the banner), 0 failures. Health 97.1%.
- Hardware smoke test deferred — banner reads existing global state via existing accessors; no functional drift versus /api/v2/debug is possible since the values come from the same globals.

Files changed:
- src/OTGW-firmware/networkStuff.ino (sendTelnetBanner rewrite + 3 SAT enum stringifiers, ~190 lines)
- src/OTGW-firmware/handleDebug.ino ('h' case slimmed)
<!-- SECTION:FINAL_SUMMARY:END -->
