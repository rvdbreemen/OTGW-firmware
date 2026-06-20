---
id: TASK-875
title: >-
  fix(mqtt): onMqttMessage must gate on len==total, not just index!=0 (F4,
  ADR-131 item 8)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-15 14:29'
updated_date: '2026-06-20 10:55'
labels: []
dependencies: []
ordinal: 91000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review F4 (MEDIUM, latent). onMqttMessage (MQTTstuff.ino:1039) implements only 'if (index != 0) return;' but ADR-131 Decision item 8 mandates 'index==0 && len==total'. espMqttClient splits a PUBLISH payload on TCP read boundaries (Parser.cpp), so a payload can arrive as multiple onMessage calls even below EMC_RX_BUFFER_SIZE; the first chunk (index==0, len<total) passes the guard and is dispatched TRUNCATED, remaining chunks dropped. Latent today (all subscriptions tiny; verify-handler matches on topic name only), but a documented-contract violation and a truncation hazard for any future payload-bearing subscription. Fix: 'if (index != 0 || len != total) return;'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 onMqttMessage gates on index==0 && len==total per ADR-131 item 8
- [x] #2 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
F4: onMqttMessage now gates on (index!=0 || len!=total); removed (void)total.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
F4 fix complete and live. onMqttMessage gates on (index==0 && len==total) per ADR-131 item 8 (MQTTstuff.ino:1038, commit 1d91bbce/alpha.198, ancestor of HEAD). 3-target build green at alpha.224+cdc4ec7 (after fixing SimpleTelnet submodule drift -> 7013fdc3): esp32 SUCCESS, esp32-classic SUCCESS, esp32-combo SUCCESS (combo .ino.bin 1.91MB fits 2.375MB app slot, overflow resolved, flash.zip emitted). evaluate.py --quick green (0 fail/1 warn/98.6%). Audit wf wp0vjoo5s 2026-06-20. No field gate (pure inbound-gate logic, code-settled). Both ACs met.
<!-- SECTION:FINAL_SUMMARY:END -->
