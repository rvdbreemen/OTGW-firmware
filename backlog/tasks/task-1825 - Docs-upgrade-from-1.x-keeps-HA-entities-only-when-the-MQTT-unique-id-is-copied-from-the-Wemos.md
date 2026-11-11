---
id: TASK-1825
title: >-
  Docs: upgrade from 1.x keeps HA entities only when the MQTT unique id is
  copied from the Wemos
status: To Do
assignee: []
created_date: '2026-11-11T21:52:00Z'
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
- [ ] #1 Manual h02 (NL and EN), section on upgrading from v1.x, explains that the MQTT unique id follows the MAC, so the S3 gets a new one, with the file:line facts checked against the current code
- [ ] #2 It gives both fixes: copy the unique id from the Wemos into the S3 MQTT settings to keep the same HA entities (and never run both gateways at the same time), or delete the old device in HA and rename the new entities
- [ ] #3 It says that deleting the retained topics with MQTT Explorer does not remove the entities from the HA registry
<!-- AC:END -->
