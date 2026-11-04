---
id: TASK-930
title: >-
  Add Xiaomi MiBeacon (0xFE95) advertisement parsing to SATble for stock Mijia
  sensors
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 22:54'
updated_date: '2026-06-29 22:04'
labels: []
dependencies: []
ordinal: 144000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Path A from the 2026-06-25 BLE validation: the SATble passive-scan parser currently decodes only ATC/pvvx (service-data UUID 0x181A) and unencrypted BTHome v2 (0xFCD2). Stock Xiaomi Mijia sensors (LYWSD03MMC, MJ_HT_V1, MHO-C401, CGG1, CGDK2, ...) advertise the Xiaomi MiBeacon protocol on service-data UUID 0x195/0xFE95 and are silently ignored. Add a third parse branch so stock Xiaomi temp/hum sensors auto-populate the BLE roster like ATC/BTHome do. Reference decode: HA xiaomi_ble (xiaomi-ble lib) + ESPHome xiaomi_ble.cpp (mbedtls_ccm_auth_decrypt, packet-size checks 19 / 22-24). MiBeacon framing = object-id TLV (0x1004 temp s16/10, 0x1006 hum u16/10, 0x100A batt, 0x100D combined temp+hum). Encryption per version: v2/v3 unauthenticated AES 12-byte key, v4/v5 authenticated AES-CCM 16-byte bindkey. Phased: Phase 1 = unencrypted MiBeacon only (no key, cheap, catches MJ_HT_V1 and unencrypted devices); Phase 2 (maintainer-gated) = encrypted v4/v5 AES-CCM with a per-roster-slot bindkey setting + UI. Plan checkpoint required before code (KISS: maintainer decides whether Phase 2 / the per-sensor bindkey UX is in scope). Note: the pragmatic alternative for Telink-based models is reflashing to pvvx (already supported); this task is for keeping sensors on stock firmware or for non-reflashable models.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Phase 1: a new parse branch for Xiaomi MiBeacon service-data UUID (0xFE95) is added in the BLE scan onResult callback AFTER the ATC(0x181A) and BTHome(0xFCD2) branches, feeding the same bleFindOrAllocSlot roster path (temp/hum/battery/rssi), so a stock unencrypted Mijia sensor auto-populates a roster slot exactly like ATC/BTHome
- [x] #2 Phase 1: MiBeacon object-id TLV decoder handles the temp/hum/battery objects (0x1004,0x1006,0x100A,0x100D) for UNENCRYPTED frames; frames flagged encrypted are skipped without crashing when no key is present (mirror the existing BTHome encrypted-skip behaviour), using memcmp_P for binary compares (never strncmp_P/strstr_P on binary)
- [x] #3 Coding discipline: PROGMEM for all literals (F()/PSTR()/snprintf_P), no String in the hot scan path (ADR-004), feedWatchDog() in any added loop; builds green for esp32 and esp32-classic targets; python evaluate.py --quick shows no new failures
- [ ] #4 Phase 2 (only if the plan approves it): encrypted v4/v5 MiBeacon via mbedtls AES-CCM; a per-roster-slot bindkey setting (e.g. settings.sat.sBleBindkey[slot], 32-hex/16-byte) with serializer round-trip, REST surface, and a UI field; nonce + AAD constructed per the MiBeacon spec; packet-size validation mirrors ESPHome xiaomi_ble.cpp
- [x] #5 An ADR is drafted (Proposed) covering the architectural additions: a new advertised-format supported (MiBeacon/0xFE95) and, if Phase 2 is in scope, the new mbedtls AES-CCM crypto dependency + per-sensor secret (bindkey) storage in settings. SATble.ino header comment + the BLE-related doc updated to list 0xFE95 support
- [ ] #6 Field validation (hardware, user-gated): a real stock Xiaomi sensor (e.g. LYWSD03MMC stock or MJ_HT_V1) appears in /api/v2/sat/ble/discovery with correct temp/hum, on the live OTGW32
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Design (verified by 3-agent research, 3 primary sources: ESPHome xiaomi_ble.cpp, Bluetooth-Devices/xiaomi-ble, ble_monitor)

### Key correction to the initial assumption
Encryption is signaled PER-FRAME by **frame-control bit 3 (mask 0x0008)**, NOT by MiBeacon version. Any version can be plaintext or encrypted. Version (frctrl>>12) only selects WHICH cipher when bit3 is set. So Phase 1 (plaintext) is gated on "bit3 clear", independent of version.

### MiBeacon 0xFE95 frame layout (service-data payload, offset 0 = after the UUID)
- [0..1] frame control, u16 LITTLE-ENDIAN. bit3=encrypted, bit4=mac-included, bit5=capability-included, bit6=object-included, bits[15:12]=version.
- [2..3] product id (u16 LE); [4] frame counter (u8).
- [5..10] optional MAC (6B, byte-REVERSED) iff bit4; then optional capability (1B, +1 if cap&0x20) iff bit5; then the TLV object list iff bit6.
- Payload offset: i=5; if bit4 i+=6; if bit5 i+=1 (+1 more if cap&0x20).
- TLV walk (loop, multi-object frames exist): id u16 LE | len u8 | value[len]; advance.
- Objects: 0x1004 temp int16 LE /10.0; 0x1006 hum uint16 LE /10.0; 0x100A batt u8 percent; 0x100D combined (int16 temp/10 + uint16 hum/10).

### Integration point (read from the repo)
- onResult callback: SATble.ino:290-407. ATC branch 302-307, BTHome 311-316, insert new 0xFE95 branch after BTHome fallthrough (~:316) -> same bleFindOrAllocSlot path.
- Mirror parseBLEAtcFormat (SATble.ino:153-173). New helper decl SATble.ino:143.
- Add const MIBEACON_SERVICE_UUID_16 = 0xFE95 near :47.
- mbedtls (AES-CCM) is bundled in ESP-IDF v5.x / Arduino-ESP32 3.3.5 (platformio.ini:76) but NOT currently included; Phase 2 adds #include <mbedtls/ccm.h>.

## Phase 1 - unencrypted MiBeacon (NO key, NO crypto) - the cheap, high-value half
1. Add MIBEACON_SERVICE_UUID_16 (0xFE95).
2. parseBLEMiBeaconFormat(data,len,temp,hum,batt): read frctrl LE; if bit3 set -> return false (Phase-1 skip, like the BTHome encrypted-skip at :188); compute payload offset; TLV-walk; dispatch 0x1004/0x1006/0x100A/0x100D with the divisors above; sanity-range like ATC; return parsed. char[]/byte ops only, memcmp_P for binary, no String, feedWatchDog not needed (no loop beyond the bounded TLV walk).
3. New branch in onResult after BTHome: getServiceData(0xFE95) -> parseBLEMiBeaconFormat -> existing roster path.
4. Header comment + BLE doc list 0xFE95. Build esp32 + esp32-classic green; evaluate.py --quick green.
Catches: MJ_HT_V1 and any Mijia advertising plaintext MiBeacon.

## Phase 2 - encrypted v4/v5 AES-CCM (MAINTAINER-GATED, only if the plan approves it)
- settings.sat.sBleBindkey[SAT_BLE_MAX_ROSTER][33] (SATtypes.h:532, after sBleLabel; +264B, negligible) + serializer (settingStuff.ino:404-410 loop) + deserializer prefix branch (after :1107, SATblebindkeyN, validate empty|32-hex) + whitelist (restAPI.ino:3512) + per-slot UI field.
- Port ESPHome decrypt_xiaomi_payload(): nonce(12) = revMAC(6)+prodId(2)+counter(1)+extCtr(3); AAD={0x11} len1; key 16B; tag last 4B; accepted sizes 19 or 22-24; mbedtls_ccm_auth_decrypt. On bit3 set + slot has a bindkey -> decrypt in place, clear bit3, then run the same TLV walker. Bindkey looked up by MAC->slot.
- Backward-compat: missing settings field zero-inits gracefully, no migration code.
- v2/v3 legacy (unauthenticated AES, 12-byte key) is OUT OF SCOPE (secondary-source only; ESPHome itself does not implement it; modern encrypted Xiaomi are all v4/v5 CCM).

## ADR
Draft a Proposed ADR at implementation: new advertised BLE format supported (MiBeacon 0xFE95); + if Phase 2, the new mbedtls AES-CCM crypto dependency and per-sensor secret (bindkey) storage in settings.

## Decisions for you (KISS - share complexity)
1. Phase 1 only, or Phase 1 + Phase 2 now? (Phase 2 = crypto + per-sensor bindkey UX; user must source each key via Xiaomi-cloud/pvvx.)
2. Reminder: for Telink models (LYWSD03MMC etc.) reflashing to pvvx is the zero-firmware-change alternative; this task is for stock-keeping / non-reflashable (MJ_HT_V1) sensors.

## Risks
- Nonce MAC source (advert MAC bit4 vs BLE peer address) - verify against ESPHome (uses device address reversed) in Phase 2.
- mbedtls first use in this firmware - verify link on both esp32 targets.
- ble_monitor truncates 0x1006 hum to int for the LYWSD03MMC variant; general rule /10.0.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 1 landed: e19aea63 (alpha.257). parseBLEMiBeaconFormat (plaintext 0xFE95, TLV 0x1004/0x1006/0x100A/0x100D, encrypted-bit3 skip, bounds-checked) + onResult fallthrough after BTHome. Builds green esp32 + esp32-classic; evaluate --quick 0 failed (98.7%). Adversarially reviewed; added the capability sub-field bound. ADR-153 (Proposed) + c4-code-sat + SATble header updated. AC4 (Phase 2 encrypted AES-CCM + per-slot bindkey) deferred per the approved plan, gated on maintainer go. AC6 (real stock Mijia in roster) is hardware/field-gated -> In Review. Note: device 192.168.88.39 currently has only ATC sensors (3) in range; needs a stock Mijia advertising plaintext MiBeacon to field-verify.

CLOSE 2026-06-30: Phase 1 (plaintext 0xFE95 MiBeacon TLV parsing) SHIPPED alpha.257 (e19aea63), builds green, encrypted-bit3 skipped + bounds-checked. AC#4 Phase 2 (encrypted v4/v5 AES-CCM + per-slot bindkey) was conditional ('only if the plan approves') and is NOT in scope -> descoped to a future task if a user needs encrypted Mijia. AC#6 field (a real stock Xiaomi sensor) is user-gated. Closing on the delivered Phase 1; encrypted support is a separate opt-in.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SATble parses plaintext Xiaomi MiBeacon (0xFE95) advertisements (alpha.257) so stock Mijia sensors are ingested. Encrypted v4/v5 MiBeacon descoped to a future opt-in task.
<!-- SECTION:FINAL_SUMMARY:END -->
