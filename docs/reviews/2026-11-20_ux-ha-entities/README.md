# UX review: the 2.0.0 gateway in Home Assistant, as the user sees it (2026-11-20)

Reviewer: otgw-ux-designer. Task: TASK-2970. Suggested by the coordinator. Triage: the coordinator; the owner is likely otgw-dev (discovery code), with otgw-docs for `docs/api/MQTT.md`. Nothing here is a decision.

## Scope and method

- **Read-only**, from the discovery code at origin/dev 9b6a74a85: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (the table and composers; "MHD" below) and `MQTTstuff.ino` ("MS", the dispatcher, SAT zones, PV and BLE). No broker, no Home Assistant instance and no unit were used. There are **no HA screenshots** in the field-data folder; this review sees the payloads as code, not as rendered cards.
- An Explore agent made the first inventory. I re-checked these claims in the code myself before using them:
  - no `has_entity_name`, `default_entity_id` or `object_id` field is emitted; the 5 grep hits in MHD are comments (MHD:2467, 2510, 2630, 2711, 2714);
  - the device name `OpenTherm Gateway (<hostname>)` (MHD:2440-2441);
  - the name transform `writeFriendlyName` (MHD:2359-2370);
  - the typo and the names quoted in H2;
  - the binary-sensor rows without a device class (MHD:1450-1456);
  - BLE discovery without availability or `expire_after` (MS:3733 and the 110 lines after it).
- The counts in H1 come from a small parse script over the `mqttHaSensors[]` rows (regex on each row line). They are table rows, not entities on a real boiler: discovery only announces a message ID once it is seen on the bus (`docs/api/MQTT.md:1282+`), so a real install shows fewer.
- Anything marked **[HA behaviour]** describes how Home Assistant treats a payload, from HA's documented MQTT discovery behaviour. It was not run here.

## Summary

| ID | Topic | Heuristic | Sev | Effort | Evidence |
|---|---|---|---|---|---|
| H1 | Device page overload, raw values visible by default | H8 minimalism | 3 | M | MHD:871-1436; parse: 221 rows uncategorised and enabled, 38 of them raw byte halves |
| H2 | Names are code labels, title-cased | H2 match, H4 | 2 | M | MHD:503-865, 2359-2370 |
| H3 | Binary sensors without a device class | H2 | 2 | S | MHD:1450-1468 |
| H4 | Overlapping concepts with near-identical names | H4, H5 | 2 | S | MHD:294-388 vs 503-865 |
| H5 | BLE sensors never go unavailable | H1 visibility | 2 | S | MS:3733-3840 |
| H6 | `docs/api/MQTT.md` describes five devices and old counts | H2 | 2 | S | `MQTT.md:1255-1256`, `:1535-1560` |
| H7 | Small leftovers | H4 | 1 | S | MHD:709, 815, 792, 3339-3341 |

## Findings

### H1. The device page lists hundreds of sensors, including raw byte halves (H8, sev 3)

- **What:** every entity hangs off one HA device, `OpenTherm Gateway (<hostname>)` (MHD:2440-2441). **[HA behaviour]** HA's device page groups entities by `entity_category` into Controls, Sensors, Configuration and Diagnostic, and hides disabled ones.
- **Measured (table rows):** of 405 sensor rows, 221 have no category and are enabled by default, so they land in the main Sensors list. 92 rows are raw byte halves of an OpenTherm value (names ending in `_hb`, `_lb`, `_u8`); 64 of them have no category, and 38 of those are enabled by default. Examples: "Tdhw Set UB Tdhw Set LB Value Hb", "Max TSet UB Max TSet LB Value Lb" (MHD:563, :566).
- **Why it matters:** the user opens the gateway device and sees a long list where boiler temperatures sit between byte fragments that mean nothing without the OpenTherm spec. The Energy dashboard and climate card work, but finding "the flow temperature" means scrolling or searching.
- **Proposal, one rule for the table:**
  - raw byte halves and protocol internals (status bytes, `_hb`/`_lb`/`_u8`, VH status words, boundary values) get `entity_category: diagnostic` and `enabled_by_default: false`;
  - the values a heating user reads (flow, return, DHW, room, outside, pressure, modulation, setpoints, flame, fault) stay in Sensors;
  - SAT tuning internals (PID terms, Kp/Ki/Kd, averages) get `diagnostic`.
- **Risk:** an entity that becomes disabled by default stays enabled on existing installs. **[HA behaviour]** HA applies `enabled_by_default` only when it first registers an entity. So the change only tidies new installs, and it breaks nobody's dashboard.
- **Proposed AC:** no `_hb`, `_lb` or `_u8` row and no status-byte row is uncategorised and enabled by default; the parse count of "uncategorised and enabled" sensor rows drops and is recorded in the task; a test over the table (as `tests/webui/ha-discovery-golden.test.mjs` or a host test) enforces the rule for new rows.

### H2. Entity names are code labels with capitals added (H2, H4, sev 2)

- **What:** `writeFriendlyName()` turns `_` into a space and capitalises the first letter of each word, nothing else (MHD:2359-2370). So the HA name is the code label: "Max Capacity Min Mod Level HB U8" (MHD:531), "Tdhw Set UB Tdhw Set LB Value Hb" (MHD:563), "Water Pressure In CH Circuit" (MHD:535), "Status VH Master", "S0 Power Kw" (MHD:705). Abbreviations and protocol words (VH, TSP, FHB, RBP, HB/LB, Pct, Sec) show as they are written.
- **[HA behaviour]** No `has_entity_name` is sent, so HA (2023.8 and later) treats the `name` as the entity name and puts the device name in front. The user sees "OpenTherm Gateway (otgw2) Water Pressure In CH Circuit".
- **Proposal:** a display-name column for the rows a user sees (after H1, about the Sensors and Controls lists), in plain words with sentence case: "CH water pressure", "Maximum boiler capacity", "Hot water setpoint upper limit". Keep the label for topics and `uniq_id`, so nothing else changes. Units go in `unit_of_measurement`, never in the name ("S0 power" with unit kW).
- **Safe for existing installs:** **[HA behaviour]** the entity registry is keyed on `uniq_id`, so a new `name` changes the shown name but not the `entity_id`, and a user's own rename still wins.
- **Proposed AC:** every row that stays in Sensors or Controls after H1 has a curated display name without code abbreviations or units; topics and `uniq_id` are unchanged (golden test on the topics passes).

### H3. Binary sensors carry no device class (H2, sev 2)

- **What:** the OpenTherm binary rows leave `devCls` empty (MHD:1450-1456 onward); only SAT rows use `problem` or `window`. **[HA behaviour]** Without a device class HA shows "On / Off" and a generic icon. With one, it shows words that fit, for example "Problem / OK" or "Connected / Disconnected".
- **Proposal:** fault rows → `problem`; "Boiler Connected" and "Thermostat Connected" → `connectivity`; flame, pump and CH/DHW active → `running`; the rest stay without.
- **Proposed AC:** fault, connected and running-type binary rows carry those device classes; the golden discovery test covers three examples.

### H4. Overlapping concepts look alike (H4, H5, sev 2)

- **What:** the same physical quantity appears more than once, with names that do not say which one to use:
  - room temperature: "Room Temperature" (the thermostat's MsgID 24), "SAT Room Temp" (what SAT controls on) and "SAT BLE Temp";
  - CH pressure: "Water Pressure In CH Circuit" and "SAT CH Pressure";
  - flame: "Flame" (binary) and "SAT Flame Status" (sensor);
  - "Thermostat": the climate entity, the " Thermostat" variant suffix and "Thermostat Connected".
- **Why it matters:** a user building an automation picks one at random. "Room Temperature" is the thermostat's reading, not the one SAT uses (ADR-248 routes the climate entity to SAT while SAT is on).
- **Proposal:** names that say the role: "Room temperature (thermostat)", "Room temperature used by SAT", "Room temperature (BLE sensor)"; and drop the SAT duplicate of a plain OpenTherm value where it is the same number (pressure), or name it "… (SAT input)". Which duplicates hold different values is a question for otgw-dev.
- **Proposed AC:** no two enabled entities with the same quantity have names that differ only by "SAT"; each name states its source or role.

### H5. BLE sensors never turn unavailable (H1, sev 2)

- **What:** the BLE discovery (`satBLEPublishOneDiscovery`, MS:3733 and the next 110 lines) sends no `availability` and no `expire_after`, while the main entities do have availability (11 hits in MHD). **[HA behaviour]** When a BLE sensor's battery dies, HA keeps showing its last temperature as if it were current.
- **Proposal:** `expire_after` of about 3 times the expected report interval on the BLE temperature and humidity entities, plus the gateway's availability topic, so they go unavailable when either the sensor or the gateway is silent.
- **Proposed AC:** BLE entities carry the gateway availability and an `expire_after`; a host test checks the payload.

### H6. The MQTT docs describe a device layout that no longer exists (H2, sev 2)

- **What:** `docs/api/MQTT.md:1535-1560` still describes five HA devices (ADR-122), and `:1255-1256` lists 400 sensors and 109 binary sensors. The code makes one gateway device plus one per BLE sensor (MHD:2440-2441, MS:3780-3825), with 405 sensor and 113 binary rows (MHD:869, 1443).
- **Proposed AC:** `MQTT.md` describes one gateway device plus one device per BLE sensor, with counts that a test or a script derives from the table.

### H7. Small leftovers (H4, sev 1)

- The typo `VH_free_ventliation_status` gives "VH Free Ventliation Status" (MHD:815).
- "Cooling Configs" (`Cooling_configs`, MHD:792) should be singular.
- A placeholder name `%sensor_id%` sits in the table (MHD:709). The Dallas composer builds its own name, so it may never reach HA; it is still dead text in the table.
- Override sensors such as "TSet Override" skip `writeFriendlyName` and keep raw OT casing (MHD:3339-3341).
- **Proposed AC:** typo and plural fixed; the placeholder row is removed or documented; override names follow the same naming rule as the others.

## What works well

- **One source of truth:** the discovery table replaced `mqttha.cfg` (MHD:1-27), which makes H1 to H3 table edits instead of scattered code.
- **Icons everywhere:** every table row sets an `mdi:` icon.
- **The per-ID drip:** entities appear only for message IDs the boiler actually answers, so a real install is much smaller than the table.
- **Many rows already disabled by default** (93 sensor rows) and a clear diagnostic category for 114 sensors: H1 extends a pattern that exists.
- **BLE sensors get their own device** with `via_device` to the gateway, so they show as children of the gateway in HA (MS:3780-3825).

## Not verified

- No Home Assistant instance and no screenshots: how the device page, cards and names actually render is described from HA's documented behaviour, not observed.
- Counts are table rows from a regex parse; the number on a real boiler depends on which message IDs it answers.
- Whether the SAT duplicates in H4 carry different values from the plain ones.
- The PIC-only and OT-Direct-only row variants were not split out in the counts.
