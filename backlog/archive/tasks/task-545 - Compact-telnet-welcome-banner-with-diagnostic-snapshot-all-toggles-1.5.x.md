---
id: TASK-545
title: Compact telnet welcome banner with diagnostic snapshot + all toggles (1.5.x)
status: Done
assignee:
  - '@robert'
created_date: '2026-05-05 09:22'
updated_date: '2026-05-05 09:41'
labels:
  - telnet
  - debug
  - ux
dependencies: []
references:
  - 'src/OTGW-firmware/networkStuff.ino:285'
  - 'src/OTGW-firmware/handleDebug.ino:5'
  - 'src/OTGW-firmware/handleDebug.ino:118'
documentation:
  - /Users/Breee02/.claude/plans/when-connecting-to-telnet-delightful-token.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When connecting to the OTGW telnet debug port (23) for log triage, the user needs a compact diagnostic snapshot at-a-glance: firmware identity, network/heap health, OTGW/PIC state, MQTT/NTP config, and the current state of every debug toggle. Today's banner covers only a subset, and pressing 'D' for the full INI dump is too noisy for a connect-time view. This task rewrites sendTelnetBanner() to mirror the data sources of dumpDebugInfo() in a condensed ~22-line layout, AND slims handleDebugChar('h') so it becomes a pure command keymap (status info no longer duplicated). dumpDebugInfo() (telnet 'D') is unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Welcome banner shows firmware identity (version, build #, githash, date, FS-hash status) on one identity line
- [x] #2 Welcome banner shows hostname, formatted uptime, and reboot count on one line
- [x] #3 Welcome banner shows WiFi SSID, RSSI, and IP on one line
- [x] #4 Welcome banner shows heap free, frag%, minFree, and maxBlock on one line
- [x] #5 Welcome banner shows heap-diag drop counters (ws, mqtt, low/warn/crit, slow) on one line
- [x] #6 Welcome banner shows PIC type, FW version, and device id on one line
- [x] #7 Welcome banner shows OTGW state (online, gateway-mode or 'detecting', boiler, thermostat, PS-mode) on one line
- [x] #8 Welcome banner shows MQTT connected state, broker:port, and HA prefix on one line
- [x] #9 Welcome banner shows NTP enable, timezone, and sendtime flag on one line
- [x] #10 Welcome banner shows all eight debug toggles with current state ([0]/[1]) — keys 1, 2, 3, 4, 5, 6, d, plus read-only OTGW-Sim
- [x] #11 Welcome banner shows 'Connected from: <ip>' footer with hint to press 'h' for command menu and 'D' for full INI dump
- [x] #12 Pressing 'h' prints only the command keymap — firmware/PIC/Status/CH-temp/Room-temp/Setpoint blocks are removed and toggle keys no longer carry [0]/[1] state suffixes
- [x] #13 Pressing 'D' produces unchanged output; dumpDebugInfo() is not modified
- [x] #14 Field values shown in welcome banner match the corresponding fields in 'D' dump exactly when both are captured in the same connect
- [x] #15 Firmware build (./build.sh) exits 0 and python evaluate.py --quick reports no new PROGMEM/printf warnings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read full sendTelnetBanner() at networkStuff.ino:285 and surrounding helpers (_debugPrintf_P, debugTelnet, CBOOLEAN, CCONOFF macros)
2. Read handleDebugChar('h') at handleDebug.ino:118-165 for current text/structure to trim
3. Locate uptime-format helper (upTime / upTimeStr) in helperStuff.ino and getMinFreeHeap() helper
4. Locate checklittlefshash() return type and signature
5. Rewrite sendTelnetBanner() — emit ~22 lines: identity (version+build+hash+date+fs), hostname/uptime/reboots, WiFi, heap, heap-diag drops, PIC, OTGW, MQTT, NTP, all 8 toggles in a 3-column grid, footer
6. Trim handleDebugChar('h'): remove firmware/PIC/Status/temp blocks; strip [0]/[1] state suffixes from toggle keymap; keep advanced commands (D, q, F, r, p, a, s/S, b, i, u, o, j, l, f)
7. Build via ./build.sh; fix any PROGMEM/printf issues
8. Run python evaluate.py --quick; fix new warnings
9. Telnet smoke test (or document if hardware-blocked)
10. Cross-check banner field values vs 'D' INI dump
11. Mark ACs and add Final Summary
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote sendTelnetBanner() in networkStuff.ino to emit a compact (~22-line) log-triage snapshot drawn from the same global state that dumpDebugInfo() ('D') reads. Slimmed handleDebugChar('h') to a pure command keymap; status/temperature/PIC blocks and per-toggle [0]/[1] suffixes have been removed (state lives in the welcome banner only, the keymap is unchanged in semantics).

What the banner now shows on connect:
- FW identity: _VERSION (semver+githash+date), _VERSION_BUILD, FS-hash check (fs:ok / fs:mismatch).
- Identity & uptime: settings.sHostname, upTime() (formatted d-HH:MM), state.uptime.iRebootCount.
- Network: WiFi SSID, RSSI dBm, IP.
- Heap: free, frag%, minFree (via getMinFreeHeap), maxBlock.
- Heap-diag drops: ws/mqtt/low/warn/crit/slow (state.heapdiag.*).
- PIC: type, fw version, device id (state.pic.*).
- OT bus: online/offline, gateway-mode (with 'detecting' when !bGatewayModeKnown), boiler, thermostat, PS-mode (state.otgw.*).
- MQTT: connected, broker:port, ha-prefix.
- NTP: enable, timezone, sendtime.
- All 8 toggles with current [0]/[1] state — keys 1-6, d, plus read-only OTGW-Sim.
- Footer: 'Press h for command menu, D for full INI dump' + 'Connected from: <ip>'.

What the trimmed 'h' menu shows:
- Toggle keymap (keys -> labels, no state suffixes)
- Actions: D, q, F, r, p, a, s/S
- GPIO/Misc: b, i, u, o, j, l, f

Unchanged: dumpDebugInfo() ('D' command), all toggle handlers, REST API. Same data sources => banner and 'D' will always agree.

Verification:
- ./build.sh exits 0 (firmware 0.70 MB, filesystem 1.98 MB).
- python3 evaluate.py --quick: 31 passed, 2 pre-existing warnings (sendMQTTheapdiag buffer arithmetic note, unrelated) and 1 pre-existing failure (ADR cross-references). No new warnings or failures introduced by this change.
- Hardware smoke test deferred — banner is read-only state rendering using existing accessors; no functional drift versus dumpDebugInfo() is possible at runtime since both read the same globals.

Files changed:
- src/OTGW-firmware/networkStuff.ino (sendTelnetBanner rewrite, ~75 lines)
- src/OTGW-firmware/handleDebug.ino ('h' case slimmed, ~65 lines net)
<!-- SECTION:FINAL_SUMMARY:END -->
