---
id: TASK-931
title: 'BLE MiBeacon follow-ups: per-field roster merge (b) + v2 bindkey UI field (a)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-25 05:02'
updated_date: '2026-06-26 22:32'
labels: []
dependencies: []
ordinal: 145000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two follow-ups to TASK-930 (Xiaomi MiBeacon). (b) PER-FIELD MERGE: stock LYWSD03MMC emits single-object adverts (temp / humidity / battery in separate frames). Today onResult overwrites temp+hum+batt unconditionally from the parser out-params, so a humidity-only frame would clobber temperature to 0 (which is why parseBLEMiBeaconFormat requires gotTemp and a hum-only frame is dropped). Fix: init the onResult locals to sentinels (temp/hum = NAN, batt = 0xFF), have each parser write only the fields it actually decoded, and merge ONLY the non-sentinel fields into _bleRuntime[slot] (so temperature never flickers to 0 and humidity/battery populate from their own adverts). This is a strict improvement for BTHome too (a temp-only BTHome frame no longer zeroes humidity). MiBeacon registers on any decoded field; ATC/BTHome return semantics unchanged. (a) v2 BINDKEY UI: add a minimal per-slot bindkey input to the v2 BLE roster card so the user can paste a 32-hex key and POST /api/v2/sat/ble/bindkey {mac,key}; show the has_key state from discovery; never display the key. Asset-only (data/), no new firmware API (route already exists from TASK-930 Phase 2).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per-field merge: onResult inits temp/hum to NAN and batt to 0xFF; the runtime update writes a field to _bleRuntime[slot] ONLY when it is non-sentinel; temperature is never overwritten with 0 by a humidity-only or battery-only advert
- [x] #2 parseBLEMiBeaconFormat registers a frame on ANY decoded field (temp OR humidity OR battery), not temperature-only; ATC and BTHome parser return semantics are unchanged; a humidity-only stock-Mijia advert now updates the slot humidity while keeping the last temperature
- [x] #3 v2 BLE roster card shows per-slot has_key state and offers a minimal bindkey input that POSTs {mac,key} to /api/v2/sat/ble/bindkey; the key is never rendered back (write-only); empty input clears
- [x] #4 Builds green esp32 + esp32-classic; evaluate.py --quick no new failures; known-answer test scripts/test_mibeacon_decrypt.py still passes
- [ ] #5 Field validation (hardware, user-gated): a stock Mijia sending separate temp + humidity adverts shows BOTH values in /api/v2/sat/ble/discovery without temperature flicker, on the live OTGW32
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Per-field BLE merge + v2 bindkey UI verified done. AC#1: onResult inits temp/hum=NAN, batt=0xFF (SATble.ino:471) and merges only non-sentinel fields (:592 'if(!isnan(temp)) ...fTemperature=temp'), so a humidity-only Mijia frame never clobbers temperature. AC#2: parseBLEMiBeaconFormat returns gotTemp||gotHum||gotBatt (:409); ATC/BTHome return semantics unchanged. AC#3: v2 renderBleCard offers a per-slot 32-hex bindkey input POSTing {mac,key} to /api/v2/sat/ble/bindkey, write-only. AC#4: scripts/test_mibeacon_decrypt.py PASS (humidity=46.7 KAT); esp32 builds green; evaluate.py clean. AC#5 (live Mijia separate temp+hum frames) = hardware field-validation sign-off.
<!-- SECTION:FINAL_SUMMARY:END -->
