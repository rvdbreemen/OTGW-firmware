---
id: doc-4
title: sat-preset-configuration
type: other
created_date: '2026-04-09 05:24'
---
# SAT Preset Configuration Format

SAT (Smart Autotune Thermostat) supports six named temperature presets. Each preset is simply a target room temperature (°C) that is activated on demand, either by the user or by an HA automation.

---

## Preset Names

| Preset Name | MQTT Key | Default (°C) | Typical Use |
|-------------|----------|-------------|-------------|
| `comfort` | `sat/preset_comfort` | 21.0 | Normal daytime comfort temperature |
| `eco` | `sat/preset_eco` | 18.0 | Energy-saving mode, home but minimal heating |
| `away` | `sat/preset_away` | 15.0 | Nobody home, frost protection |
| `sleep` | `sat/preset_sleep` | 16.0 | Night-time temperature |
| `home` | `sat/preset_home` | 18.0 | Standard at-home temperature |
| `activity` | `sat/preset_activity` | 10.0 | Active period / window-open fallback setpoint |

---

## Storage Format

Preset temperatures are stored as individual `float` fields in the `SATSection` struct within `OTGWSettings`. They are serialized to `settings.ini` on LittleFS as JSON key-value pairs:

```json
"SATpresetcomfort": 21.0,
"SATpreseteco": 18.0,
"SATpresetaway": 15.0,
"SATpresetsleep": 16.0,
"SATpresetactivity": 10.0,
"SATpresethome": 18.5
```

There is no separate preset file. Presets are part of the main settings block, persisted with all other SAT settings via `writeSettings()`.

There is no array or list structure for presets — each preset is an independent float field. There is no concept of custom preset names or user-defined presets beyond these six.

---

## Setting Preset Values

### Via MQTT

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_comfort" -m "21.5"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_eco" -m "17.5"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_away" -m "14.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_sleep" -m "16.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_home" -m "19.0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_activity" -m "10.0"
```

### Via REST API

```bash
curl -X POST http://otgw.local/api/v2/sat/settings/preset_comfort -d "21.5"
curl -X POST http://otgw.local/api/v2/sat/settings/preset_eco -d "17.5"
```

---

## Activating a Preset

### Via MQTT

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset" -m "away"
```

Accepted preset names (case-insensitive): `away`, `eco`, `comfort`, `sleep`, `activity`, `home`

### Via REST API

```bash
curl -X POST http://otgw.local/api/v2/sat/preset -d "away"
```

### Via WebUI

The SAT dashboard shows buttons for all six presets. Clicking a preset button activates it immediately.

---

## Active Preset State

The active preset is stored as `state.sat.eActivePreset` (a `SATPreset` enum). It is **not persisted** across reboots — after a reboot, SAT reverts to using the last saved target temperature.

The firmware also tracks:
- `state.sat.fPreCustomTemp` — target temperature before the last preset was activated (allows "undo")
- `state.sat.fPreActivityTemp` — target temperature before a window-open event triggered the activity preset

---

## Preset Sync (Optional)

When preset sync is enabled, activating a preset publishes the preset name to a configurable secondary MQTT topic:

```
settings.sat.bPresetSync = true
settings.sat.sPresetSyncTopic = "home/sat/preset"
```

This allows a secondary controller, HA entity, or dashboard to track the active preset independently.

Configure via MQTT:
```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_sync" -m "true"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset_sync_topic" -m "home/sat/preset"
```

---

## Compatibility with Python SAT

The Python SAT custom component (thermo-nova branch) also supports named presets, but with some differences:

| Feature | Python SAT | OTGW-firmware SAT |
|---------|-----------|-------------------|
| Preset names | Same 6 names | Same 6 names |
| Preset temperatures | Configured in `configuration.yaml` | Configured via MQTT/REST, stored in `settings.ini` |
| Additional custom presets | Not supported | Not supported |
| Preset activation | Via HA climate entity service | Via MQTT `sat/preset` or REST `/api/v2/sat/preset` |
| Preset state persistence | HA entity state | Not persisted (resets to last target after reboot) |
| Preset sync | Not applicable | Optional via `sat/preset_sync` |

The preset temperature values themselves are compatible — the same `away`, `eco`, `comfort`, etc. names and semantics are used in both implementations. A preset set to 15°C in Python SAT means the same as in OTGW-firmware SAT.

---

## Window Open Detection and Activity Preset

When window-open detection is enabled (`sat/window_detection = true`) and a window-open signal is received (`sat/window = open`), SAT automatically activates the **activity** preset. This is designed to stop heating when a window is opened, since the activity preset defaults to 10°C.

When the window closes, SAT restores the pre-window-open target temperature (stored in `state.sat.fPreActivityTemp`).

The `activity` preset temperature is therefore effectively the "window open setpoint." Setting it to a low value (e.g., 10°C) prevents unnecessary heating while a window is open.

---

## Source Reference

- Preset struct fields: `src/OTGW-firmware/OTGW-firmware.h`, `struct SATSection` (~lines 745-750)
- Preset activation: `src/OTGW-firmware/SATcontrol.ino`, `satHandlePreset()` function
- Preset names enum: `src/OTGW-firmware/OTGW-firmware.h`, `enum SATPreset`
- Settings persistence: `src/OTGW-firmware/settingStuff.ino` (~lines 286-291)
