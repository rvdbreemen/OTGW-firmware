---
id: TASK-213
title: 'SAT: Investigate and fix sat/current_modulation topic mismatch in mqttha.cfg'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 05:35'
labels:
  - audit-fix
  - sat
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mqttha.cfg line 522 defines a HA discovery entry for sensor sat_current_modulation with stat_t: sat/current_modulation. However, searching SATcontrol.ino and SATble.ino finds no sendMQTTData() call for 'sat/current_modulation'. The topic may be:\n\n1. Published under a different name (e.g., sat/modulation or sat/relative_modulation)\n2. Intended to use an existing OT bus topic like RelModLevel or CHFsupTemp that publishes to a different topic\n3. A planned topic that was never implemented\n\nIf the topic is never published, the HA entity sat_current_modulation will always show as unavailable. Either:\n- Add a sendMQTTData() for sat/current_modulation in the SAT publish loop (using OTcurrentSystemState.RelModLevel or similar)\n- Or remove the mqttha.cfg entry for sat_current_modulation\n\nSource: src/OTGW-firmware/data/mqttha.cfg line 522, src/OTGW-firmware/SATcontrol.ino (all sendMQTTData calls)."
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed sat_current_modulation HA entity being permanently unavailable. Root cause: mqttha.cfg line 522 defined stat_t: sat/current_modulation but SATcontrol.ino never called sendMQTTData() for that topic — iCurrentModulation was only emitted via the WebSocket JSON map. Fix: added sendMQTTData(F("sat/current_modulation"), ...) in satPublishMQTT() (SATcontrol.ino, near modulation_reliable publish). The mqttha.cfg entry was correct; the firmware publish was missing.
<!-- SECTION:FINAL_SUMMARY:END -->
