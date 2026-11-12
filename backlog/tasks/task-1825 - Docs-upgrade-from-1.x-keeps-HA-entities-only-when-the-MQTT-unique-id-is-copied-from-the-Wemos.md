---
id: TASK-1825
title: >-
  Docs: upgrade from 1.x keeps HA entities only when the MQTT unique id is
  copied from the Wemos
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T21:52:00Z'
updated_date: '2026-11-12T20:38:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 909000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Own finding from the same support question twice (indigo_light, #nederlandse-ondersteuning msg https://discord.com/channels/812969634638725140/815561033036333076/1570193707293216903, answered in 1570194073355288748; earlier this week also indigo_light). After the swap from a Wemos (1.x) to an S3 (2.x), Home Assistant entities got a _2 suffix, and wiping the topics with MQTT Explorer did not help. Cause: the default MQTT unique id is otgw-<mac> (networkStuff.ino:1475-1479, set when empty at settingStuff.ino:638-639). The S3 has a different MAC, so the uniq_id of every discovery config changes; HA creates new entities while the old entity_ids are still taken in its registry. The manual h02 'Opmerkingen bij upgraden van v1.x' (docs/manuals/nl/h02-hardware-installatie.md ~line 284, plus the English counterpart) only says to delete orphaned entities.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Manual h02 (NL and EN), section on upgrading from v1.x, explains that the MQTT unique id follows the MAC, so the S3 gets a new one, with the file:line facts checked against the current code
- [x] #2 It gives both fixes: copy the unique id from the Wemos into the S3 MQTT settings to keep the same HA entities (and never run both gateways at the same time), or delete the old device in HA and rename the new entities
- [x] #3 It says that deleting the retained topics with MQTT Explorer does not remove the entities from the HA registry
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Facts checked against dev 52fab1fb7: getUniqueId() returns otgw-<MAC> (src/OTGW-firmware/networkStuff.ino:1475-1479); readSettings fills settings.mqtt.sUniqueid only when it is empty or 'null' (settingStuff.ino:638-639); 2.x writes and reads the key MQTTuniqueid (settingStuff.ino:309, :920); 1.x writes the same key (origin/otgw-1.x.x settingStuff.ino:246, read at :565), so restoring a 1.x settings file carries the unique id over. Web UI label: MQTT settings, group Connection, 'Unique id' (data/v2.js:3916).
Edits: docs/manuals/nl/h02-hardware-installatie.md and docs/manuals/en/ch02-hardware-setup.md, section on upgrading from v1.x: why the S3 gets a new unique id (MAC), the _2 suffix, fix 1 (restore the 1.x settings file or copy Unique id; never run both gateways at once), fix 2 (delete the old device, rename the new entities), and that wiping retained topics with MQTT Explorer leaves the entities in the HA registry. Checks: no em dashes, no OTGW32 in the added text. Docs-only, no build or evaluator gate needed (CLAUDE.md push policy).

2026-11-12 indigo_light (Discord 301077214374658048, #nederlandse-ondersteuning https://discord.com/channels/812969634638725140/815561033036333076/1570535626447782012): after swapping the Wemos D1 mini for an S3 Mini, the HACS SAT integration in Home Assistant kept sending commands to OTGW/set/otgw-<old MAC>/command (node id is derived from the MAC), so the new gateway got nothing and the Honeywell kept controlling. HA looked normal because room and outside temperature are plain entities. His fix: remove the SAT integration, delete the old otgw-<old MAC> branches in MQTT Explorer, re-add SAT with the new device; then SH= and CS= arrived on the new node. He asks to add this to the migration guide next to the _2 duplicate entities.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Manual h02 (NL) and ch02 (EN) now explain that the MQTT unique id follows the MAC (networkStuff.ino:1475-1479, settingStuff.ino:638-639), give both fixes (carry the unique id over via the 1.x settings file or the Unique id field, never both gateways at once; or delete the old device and rename), and say that wiping retained topics in MQTT Explorer does not clear the HA registry. AC1-3 ticked on those edits; facts with file:line in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
