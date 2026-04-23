# mqttha.cfg generator pipeline — archived 2026-04-22

This directory contains the retired source-to-output pipeline that used to
generate Home Assistant MQTT auto-discovery definitions.

## Files in this archive

- **`mqttha.cfg`** — original human-maintained source file. Each line
  defined one HA discovery config as `<ot_msgid> ; <discovery_topic> ;
  <discovery_payload_json>`.
- **`generate_mqttha_data.py`** — main generator. Parsed `mqttha.cfg`,
  produced PROGMEM label tables, friendly-name tables, and the
  `mqttHaSensors[]` / `mqttHaBinSensors[]` arrays in
  `src/OTGW-firmware/mqtt_configuratie.cpp`.
- **`generate_mqttha_progmem.py`** — helper that emitted raw PROGMEM
  fragments for specific discovery types.
- **`generate_mqttha_readable.py`** — alternate output for
  human-readable inspection of the config tree.

## Why it is archived

From 2026-04-20 onwards, `mqtt_configuratie.cpp` diverged from
`mqttha.cfg`. Hand-written entries started landing directly in the .cpp
(notably commit `bc9bd6a2` for on-demand discovery verification and later
additions for heap statistics), and the generator was no longer run
against the cfg. Keeping the cfg and scripts in the live source tree
implied a generation step that no longer happens, which is misleading.

The files are preserved here for historical reference. The last
generator run was `2026-04-17T17:40:17Z` — the `mqttHaSensors[]` array
baseline at that timestamp was the final generated output. Everything
after that date is hand-maintained.

## Where HA discovery configs live now

**`src/OTGW-firmware/mqtt_configuratie.cpp` is the single source of
truth.** Edit that file directly to add, remove, or modify discovery
entries. Its header comment documents this explicitly.

## Do not run these scripts

`generate_mqttha_data.py` would overwrite all post-2026-04-20 hand-edits
in `mqtt_configuratie.cpp`. Do not execute it against the current source
tree. The scripts are retained only so the historical generation logic
can be studied if a future maintainer wants to restore a cfg-driven
build.
