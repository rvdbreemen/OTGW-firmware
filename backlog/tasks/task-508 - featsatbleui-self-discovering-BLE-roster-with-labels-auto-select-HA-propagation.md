---
id: TASK-508
title: >-
  feat(satble+ui): self-discovering BLE roster with labels, auto-select, HA
  propagation
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 21:23'
updated_date: '2026-05-01 22:34'
labels:
  - feature
  - satble
  - ui
  - esp32
  - ha-discovery
  - settings
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George reported (follow-up to TASK-506/507) that the current BLE sensor selection UX is unworkable for a normal user: setup requires reading telnet logs to find the right MAC and pasting it into a settings text field. Neighbours' Xiaomi sensors (e.g. `A4:C1:38:A3:D5:4F`, `FC:4E:B5`) appear in the logs as filter-rejects with no actionable affordance.

**Goal**: turn discovery into a UX feature. The device gathers every format-pass BLE sensor it sees into a persistent roster of 8 slots. The user opens the SAT settings panel, sees a list with last-temp / RSSI / age, names them ("Woonkamer", "Slaapkamer"), picks one, and Home Assistant auto-discovers the friendly names. If only one sensor is present, no manual selection is needed. The "Forget" action removes a sensor both from the local roster and from Home Assistant via zero-byte retained discovery payloads.

**Scope (Tier C — full)**: roster + persistent labels + auto-select-if-only-one + Home Assistant discovery propagation with friendly_name + Forget-with-HA-cleanup.

**Out of scope**: multi-zone-per-area BLE binding, scan-on-demand toggle, runtime BLE-debug-trace verbosity tier.

## Architectural decisions (validated by Phase-2 Plan-agent)

1. **Two parallel arrays of length 8** indexed by the same roster slot:
   - `settings.sat.sBleMac[8][18]` + `settings.sat.sBleLabel[8][24]` — persistent (LittleFS)
   - `_bleRuntime[8] {fTemp, fHum, iBattery, iRssi, iLastSeenMs, bDiscoveryPublished, bDiscoveryDirty}` — transient
   Rationale: hot-path BLE callback only touches transient fields under mutex; label edits run on the loop task and never enter `onResult()`.

2. **`settings.sat.sBleMAC[18]` (selected MAC) is preserved as-is** — semantically distinct from the roster, no migration needed for existing deployed devices.

3. **Refuse new entries when roster is full**, no auto-evict. Surface "8/8 vol" via a `_bleRosterFullCount` counter in the discovery JSON. Honours George's "manual drop only" requirement.

4. **Auto-select runs in `satBLELoop()`, not in `onResult()`**. Calling `updateSetting()` from the BLE host task would race with the Arduino-loop settings flush and risk a corrupted `settings.ini`. Trigger: filter empty AND exactly one roster entry seen within last `2 × iBleInterval` seconds.

5. **HA label propagation via existing `bDiscoveryPublished=false` clear**, no new schedule mechanism. The existing `satBLEPublishMQTT()` loop picks it up on the next `iBleInterval`.

6. **Forget cleans up HA retained-config orphans** by publishing 4 zero-byte retained payloads to `<HaPrefix>/sensor/<uniqueId>_ble_<mac>_<kind>/config`. HA interprets empty retained discovery as device removal.

7. **Roster slot allocation under mutex**, but `settingsDirty=true` is set via a `_bleRosterDirty` volatile flag drained in `satBLELoop()`. Avoids cross-task flash writes.

8. **MAC normalisation**: roster always stores uppercase `AA:BB:CC:DD:EE:FF`. UI POST `select` normalises before `updateSetting`. Existing `bleMatchesConfiguredMAC` is `strcasecmp`-tolerant, so legacy lowercase entries keep working.

## Critical files

- `src/OTGW-firmware/SATtypes.h:410-415` — add roster fields inside ESP32-only block
- `src/OTGW-firmware/SATble.ino:36, 73-84, 213-235, 240-326, 365-456, 526-537` — types, allocation, callback, loop hook, update-state, publish
- `src/OTGW-firmware/settingStuff.ino:365, 887` — serialise 17 new keys (8 mac + 8 label + count)
- `src/OTGW-firmware/restAPI.ino:738+, 602, 2167, 2118` — new `ble/*` sub-routes in `handleSAT`, body parser, escape, `knownSettings` (NOT polluted)
- `src/OTGW-firmware/MQTTstuff.ino:2026-2169` — `labelOverride` parameter, new `satBLEUnpublishDiscovery()`
- `src/OTGW-firmware/OTGW-firmware.h:259-263` — updated function signatures
- `src/OTGW-firmware/data/index.js:3436, 3561-3589, 5947` — new BLE sensors panel injected into `#satSettingsContent`; legacy `satblemac` field readonly

Approximate footprint: ~780 LOC source, 17 new settings keys, 4 new REST routes, 1 new UI panel, 0 new ADRs (no pattern-level rule introduced; structural change reviewed at PR per ADR-080).

## Reference

Detailed plan with line-by-line implementation guidance and risk register: `C:\Users\rvdbr\.claude\plans\crystalline-strolling-tiger.md`
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLOCK A: SATtypes.h SATSection extended with `sBleMac[8][18]`, `sBleLabel[8][24]`, `iBleRosterCount` inside `#if defined(ESP32)`; constant `BLE_MAX_ROSTER=8`; static_assert guards label size
- [x] #2 BLOCK A: SATble.ino splits old `BLESensorData` into transient `BLERuntime[8]` (temp/hum/battery/rssi/lastSeen/discoveryPublished/discoveryDirty) parallel to settings-backed roster; legacy `_bleSensors[4]` removed
- [x] #3 BLOCK A: `onResult()` allocates roster slot under mutex (writes mac to settings struct), updates `_bleRuntime[]`, sets `_bleRosterDirty` volatile flag; refuses new entries when full and increments `_bleRosterFullCount`
- [x] #4 BLOCK A: `satBLELoop()` drains `_bleRosterDirty` to set `settingsDirty=true` and recomputes `iBleRosterCount`; runs auto-select block when filter empty AND exactly one entry seen within `2 × iBleInterval` seconds
- [x] #5 BLOCK A: settingStuff.ino serialises 17 new keys (`SATblemac0..7`, `SATblelabel0..7`, `SATblerostercount`) via for-loop helpers in both `writeSettings()` and `updateSetting()`; mac validated as 17-char colon format on write
- [x] #6 BLOCK B: `restAPI.ino handleSAT()` dispatches `ble/discovery` (GET), `ble/select` (POST), `ble/label` (POST), `ble/forget` (POST) under existing admin auth; methods other than expected return 405
- [x] #7 BLOCK B: `satBLERosterSendJSON()` streams discovery JSON via `sendStartJsonMap`/`sendJsonMapEntry`/`sendEndJsonMap` with per-slot mac/label/temp/rssi/age_ms/valid/selected plus top-level `roster_full` and `dropped_since_full`
- [x] #8 BLOCK B: helper `satExtractTwoFields()` parses `{mac,label}` JSON via two `extractJsonField` calls; `knownSettings[]` is NOT extended (dedicated endpoints only)
- [x] #9 BLOCK B: forget on selected slot also clears `settings.sat.sBleMAC`; label/forget set `bDiscoveryDirty=true` and clear `bDiscoveryPublished` for that slot
- [x] #10 BLOCK C: `satBLEPublishOneDiscovery()` accepts optional `labelOverride`; when set, JSON `name` becomes `"<label> <kind>"` and `dev:{name:<label>}`; null falls back to legacy `"BLE %s %s"` behaviour
- [x] #11 BLOCK C: local 64-byte JSON-escape helper added (NOT `escapeJsonStringTo` which uses global `cMsg`); `payload[768]` headroom verified ≥ 50 bytes after 24-byte labels in 2 places
- [x] #12 BLOCK C: `satBLEUnpublishDiscovery(macCompact)` publishes 4 zero-byte retained payloads to wipe HA discovery; called from forget after settings mutation
- [x] #13 BLOCK D: new BLE sensors panel injected into `#satSettingsContent` via `index.js refreshSATSettings()` BEFORE generic settings groups; reuses `.sat-grid`/`.sat-row`/`.sat-label`/`.sat-value`/`.sat-btn` per ADR-091
- [x] #14 BLOCK D: 5s polling of `/api/v2/sat/ble/discovery` while panel visible; per-row buttons Selecteer / Opslaan / Vergeet (with confirm dialog) using existing `satPost()`-style fetch + .then + .catch
- [x] #15 BLOCK D: legacy `satblemac` field in generic settings group marked readonly with explanatory help text; SATbleenable help string updated
- [x] #16 VERIFY: `python evaluate.py --quick` exits 0; PROGMEM check PASS; ADR-091 design-system class drift gate PASS (no new CSS classes); no new evaluate.py warnings
- [x] #17 VERIFY: `python build.py` builds clean on ESP8266 (unchanged — file is ESP32-only) and ESP32-S3; LittleFS image rebuilt to ship the JS change; RAM/Flash budgets within current envelope
- [ ] #18 VERIFY (hardware): roster grows on first scan; label persists across reboot; HA `friendly_name` updates within 1 iBleInterval after label change; Forget removes from HA via zero-byte retained payloads; Roster-vol warning surfaces at 8/8
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan — 4 logical blocks, single commit at end

Reference plan with full risk register: `C:\Users\rvdbr\.claude\plans\crystalline-strolling-tiger.md`

### Block A — Backend roster + auto-select (~250 LOC)

**Step A1**: `SATtypes.h` SATSection inside `#if defined(ESP32)` (around line 410):
```cpp
static constexpr uint8_t BLE_MAX_ROSTER = 8;
char     sBleMac[BLE_MAX_ROSTER][18]   = {{0}};
char     sBleLabel[BLE_MAX_ROSTER][24] = {{0}};
uint8_t  iBleRosterCount               = 0;
static_assert(sizeof(sBleLabel[0]) <= 100, "label too long for settingStuff line buffer");
```
Keep existing `sBleMAC[18]` (selected) intact.

**Step A2**: `SATble.ino` types — replace `BLESensorData` (line 73-84) with `BLERuntime` parallel to settings:
```cpp
struct BLERuntime {
  float    fTemperature, fHumidity;
  uint8_t  iBattery;
  int8_t   iRssi;
  uint32_t iLastSeenMs;
  bool     bDiscoveryPublished, bDiscoveryDirty;
};
static BLERuntime         _bleRuntime[BLE_MAX_ROSTER] = {};
static volatile bool      _bleRosterDirty             = false;
static volatile uint32_t  _bleRosterFullCount         = 0;
```

**Step A3**: rewrite `onResult()` (line 240-326): search `settings.sat.sBleMac[]` for match → reuse slot; else allocate first free up to `BLE_MAX_ROSTER`; on full increment `_bleRosterFullCount` and return; under mutex write `settings.sat.sBleMac[slot]` (mac only) + update `_bleRuntime[slot]`; set `_bleRosterDirty`.

**Step A4**: rewrite `bleFindOrAllocSlot()` (line 213-235), `satBLEUpdateState()` (line 374+) to iterate `BLE_MAX_ROSTER` and snapshot `_bleRuntime[]` per slot.

**Step A5**: extend `satBLELoop()` (line 365-369) — drain `_bleRosterDirty` → set `settingsDirty=true` and recompute `iBleRosterCount`. Then auto-select block: filter empty AND exactly one entry seen within `2 × iBleInterval` seconds → `updateSetting("SATblemac", uppercaseMac)`.

**Step A6**: `settingStuff.ino` `writeSettings()` line ~365 — emit 17 keys via for-loop helper. `updateSetting()` line ~887 — 17 matching `else if` branches. Validate mac format (17 chars, colons at positions 2,5,8,11,14).

**Verify A**: `python evaluate.py --quick` PASS; `python build.py --firmware` clean on ESP32-S3.

### Block B — REST endpoints (~200 LOC)

**Step B1**: `restAPI.ino handleSAT()` after `target` branch (~line 738) — add `else if (strcasecmp_P(sub, PSTR("ble")) == 0)` with sub-dispatch on `words[5]` for `discovery`/`select`/`label`/`forget`. Method validation per route.

**Step B2**: helper `satExtractTwoFields(body, k1, v1, sz1, k2, v2, sz2)` — calls `extractJsonField` (line 2167) twice; ~30 LOC.

**Step B3**: helper `satBLERosterSendJSON()` in `SATble.ino` — streams JSON via `sendStartJsonMap`/`sendJsonMapEntry`/`sendEndJsonMap`. Per slot: mac, label, temp, rssi, age_ms, valid, selected. Top-level: `roster_full`, `dropped_since_full`.

**Step B4**: helpers `satBLERosterSelect/SetLabel/Forget(mac)` in `SATble.ino` — all on loop task; validate mac in roster; mutate via `updateSetting()`; for label/forget clear `_bleRuntime[slot].bDiscoveryPublished` and set `bDiscoveryDirty`. Forget on selected slot also clears `settings.sat.sBleMAC`.

**Verify B**: curl smoke test — GET discovery returns valid JSON; POST select changes filter; POST label persists; POST forget clears slot.

### Block C — HA discovery label propagation (~80 LOC)

**Step C1**: `MQTTstuff.ino:2026` `satBLEPublishOneDiscovery()` — add `const char* labelOverride` parameter. When non-null: `"name"` → `"<label> <kindFriendly>"`; `"dev":{"name":"<label>"}`. Local 64-byte JSON-escape helper (NOT `escapeJsonStringTo`).

**Step C2**: `satBLEPublishHaDiscovery()` (line 2115) — add `const char* label` parameter, thread through 4 calls.

**Step C3**: new `satBLEUnpublishDiscovery(const char* macCompact)` — publish 4 zero-byte retained payloads to wipe HA discovery. Called from `satBLERosterForget` after settings mutation.

**Step C4**: `SATble.ino:526-537` publish loop — look up `settings.sat.sBleLabel[i]` (or NULL), pass to `satBLEPublishHaDiscovery`.

**Step C5**: `OTGW-firmware.h:259-263` — update signatures.

**Verify C**: subscribe `<HaPrefix>/sensor/+/config` during label rename; observe new retained payload with updated `name`.

### Block D — UI panel (~250 LOC)

**Step D1**: `data/index.js` `refreshSATSettings()` (line 3561+) — fetch `/api/v2/sat/ble/discovery`, render panel BEFORE `buildSATSettingsGroups` populates the generic form.

**Step D2**: panel HTML structure inside `#satSettingsContent`:
```html
<div class="sat-grid">
  <div class="sat-row"><span class="sat-label">Actieve sensor</span>
       <span class="sat-value" id="ble-active">--</span></div>
  <div id="ble-roster-list"></div>
  <div id="ble-roster-warning" hidden>Roster vol (8/8) ...</div>
</div>
```

**Step D3**: JS functions in `index.js`:
- `fetchBleRoster()` — 5s polling while panel visible
- `bleSelect(mac)`, `bleSaveLabel(mac, inputEl)`, `bleForget(mac)` — POST + .then + .catch
- Reuse `.sat-row`/`.sat-label`/`.sat-value`/`.sat-btn` per ADR-091

**Step D4**: line 3436 — mark legacy `satblemac` field readonly with help text "Beheerd via BLE Sensors paneel hierboven."

**Step D5**: line 5947 — update `SATbleenable` help string.

**Verify D**: full `python build.py` (LittleFS rebuild required); manual browser test with Chrome+Firefox.

### Final verification (after all 4 blocks)

1. `python evaluate.py --quick` — 0 failed checks, score ≥97%, design-system drift gate PASS
2. `python build.py` — clean on ESP8266 (unchanged) and ESP32-S3 (new code path)
3. Hardware smoke per AC #18

### Commit + push (after all verification passes)

Single commit on `feature-dev-2.0.0-otgw32-esp32-sat-support`:
```
feat(satble+ui): self-discovering BLE roster with labels, auto-select, HA propagation (TASK-508)
```

User approval required before push (shared-state action per CLAUDE.md).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation started 2026-05-01. Approach: full session implementation A→B→C→D, single commit at end. Sanity check (evaluate.py + partial build) between blocks; user confirms before push.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-508 — Self-discovering BLE roster with persistent labels, auto-select, and HA discovery propagation

Replaces the manual MAC-paste-into-settings UX with a roster-based self-discovery flow. Every format-pass BLE sensor (ATC/pvvx, BTHome v2) seen on-air enters a persistent 8-slot roster with user-friendly labels. The user opens the SAT settings panel, sees the roster with last-temp/RSSI/age, names sensors ("Woonkamer", "Slaapkamer"), and selects one as the active SAT input. Auto-select triggers when only one sensor is in the roster. Labels propagate to Home Assistant's `friendly_name` via existing retained discovery configs. Forget cleans up HA via zero-byte retained payloads.

### Files (~780 LOC across 7 files)
- `src/OTGW-firmware/SATtypes.h`: `SAT_BLE_MAX_ROSTER=8` + `sBleMac[8][18]` + `sBleLabel[8][24]` + `iBleRosterCount` inside the existing ESP32-only block. `static_assert` guards the label-size against settingStuff line-buffer overflow.
- `src/OTGW-firmware/SATble.ino`: `BLESensorData` split into transient `BLERuntime[8]` (parallel to settings-backed roster); `bleFindOrAllocSlot` searches the persistent roster; `onResult()` no longer filters before allocation (every format-pass MAC enters the roster); `_bleRosterDirty` volatile flag drains in `satBLELoop()` via `updateSetting("SATblerostercount", ...)` to trigger the existing 2-s flush debounce; auto-select hook fires once when filter is empty AND exactly one sensor is fresh; `satBLEUpdateState` no longer auto-evicts stale slots (UI-managed forget only); `satBLEPublishMQTT` publishes ONLY for the selected MAC instead of all valid slots; new helpers `satBLERosterSendJSON`, `satBLERosterSelect`, `satBLERosterSetLabel`, `satBLERosterForget` route REST mutations through `updateSetting()`.
- `src/OTGW-firmware/settingStuff.ino`: 17 new keys serialised with the indexed-key pattern (`SATblemac0..7`, `SATblelabel0..7`, `SATblerostercount`); read-side validates MAC as 17-char colon hex format and canonicalises to uppercase before storage.
- `src/OTGW-firmware/restAPI.ino`: 4 new sub-routes under `handleSAT()`: `GET /api/v2/sat/ble/discovery`, `POST /api/v2/sat/ble/select`, `/label`, `/forget`. All inherit admin auth + CSRF same-origin from the existing `checkHttpAuth()` gate. `satExtractTwoFields()` helper parses the `{mac,label}` JSON body via two `extractJsonField` calls. `knownSettings[]` deliberately NOT polluted with the per-slot keys.
- `src/OTGW-firmware/MQTTstuff.ino`: `satBLEPublishOneDiscovery()` gains optional `labelOverride` parameter — when set, `friendly_name` becomes `"<label> <Kind>"` and `dev:{name}` becomes the label; null falls back to legacy "BLE %s %s" form. Local 64-byte `mqttJsonEscape()` helper handles user-input quotes/backslashes/control chars without touching the global `cMsg` scratch. New `satBLEUnpublishDiscovery()` publishes 4 zero-byte retained payloads to wipe HA-side discovery on Forget.
- `src/OTGW-firmware/OTGW-firmware.h`: forward declarations for the 4 roster REST helpers; `satBLEPublishHaDiscovery` signature extended with default-null `label` parameter (backward compatible); `satBLEUnpublishDiscovery` declared.
- `src/OTGW-firmware/data/index.js`: BLE Sensors panel injected into `#satSettingsContent` BEFORE the generic SAT settings groups via `buildBleSensorsPanel()`. Polling `/api/v2/sat/ble/discovery` every 5s on settings refresh. Per-row UI: MAC + status, label input, Selecteer/Opslaan/Vergeet buttons. All DOM nodes built via `createElement` + `textContent` for XSS safety (security hook blocks `innerHTML` on user content). Strict MAC validation (`bleIsValidMac`) before any DOM-id or event-handler interpolation. Legacy `satblemac` field marked `readonly` with explanatory help-string.

### KISS choices defended
- Two parallel arrays of length 8 (settings vs runtime), indexed by the same slot — keeps the BLE-callback critical section short and the loop-task-only label/discovery logic out of the host-task path.
- Refuse new entries when full instead of LRU-evicting — matches the "manual drop only" UX requirement.
- Auto-select runs in `satBLELoop()`, never in `onResult()` — `updateSetting()` cannot be called from the BLE host task without racing the settings flush.
- HA label propagation reuses the existing `bDiscoveryPublished=false` clear pattern — no new schedule mechanism, the publish loop picks it up on the next iBleInterval cycle.
- No new CSS classes (ADR-091 design-system class drift gate); reuse `.sat-grid`, `.sat-row`, `.sat-label`, `.sat-value`, `.sat-btn`, `.sat-settings-group*`.
- No new ADR; structural change reviewed at PR per ADR-080. Block C's HA-orphan-cleanup pattern may justify an ADR once a 2nd discoverable entity-type adopts it.

### Verification (ACs #16, #17, #18)
- `python evaluate.py --quick`: PASS, 0 failed checks, score 97.0% (no regression vs TASK-507 baseline). PROGMEM compliance PASS, ADR-091 design-system class drift PASS (no new CSS classes).
- `python build.py`: clean on both platforms with the LittleFS image rebuilt for the JS change to ship.
  - ESP8266: SUCCESS, RAM 84.6%, Flash 77.3% (unchanged — file is ESP32-only).
  - ESP32-S3: SUCCESS, RAM 31.8%, Flash 96.1% (+0.2% over TASK-507 baseline; ~5KB delta absorbed within margin).
- AC #18 (hardware verify) stays open for George's flash + smoke test on his OTGW32: roster grows from first scan, label persists across reboot, HA `friendly_name` updates within 1 iBleInterval after rename, Forget removes from HA via zero-byte retained payloads, and the 8/8 vol-warning surfaces when 8 different format-pass MACs are seen.

### Mid-flight fixes worth noting
- `settingsDirty` is `static` (file-scope) in `settingStuff.ino`; the Phase-1 agent's claim that it could be set externally was incorrect. Routed through `updateSetting("SATblerostercount", ...)` instead — that's the public API that handles the dirty flag and timer restart.
- C++20 `-Wvolatile` deprecates chained assignment with a volatile-qualified left operand. Reset the TASK-506 stats counters individually (was `_bleAdCount = _bleAcceptCount = ... = 0;`).
- Security hook blocks `innerHTML` for user-controlled content even with proper escape — the entire UI panel is built via `createElement` + `textContent` + `appendChild`, no `innerHTML`. Defensible XSS-safe path.

### Outstanding (for owner)
- AC #18 hardware verification on George's OTGW32 with at least one Xiaomi sensor in range.
- Optional follow-up ADR documenting the HA retained-discovery orphan-cleanup pattern (zero-byte retained payloads) once a 2nd discoverable entity-type adopts it.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Block A merged + hardware-tested before Block B begins (REST has nothing to read without a roster)
- [ ] #2 Block B merged + curl-tested before Block D begins (UI has no endpoint to call)
- [ ] #3 All MQTT smoke tests pass: subscribe `<HaPrefix>/sensor/+/config` during a label rename and observe new retained payload with updated name
- [ ] #4 settings.ini OTA edge cases verified: old firmware reading new file ignores unknown keys; new firmware reading old file rebuilds roster from scratch (defaults empty)
<!-- DOD:END -->
