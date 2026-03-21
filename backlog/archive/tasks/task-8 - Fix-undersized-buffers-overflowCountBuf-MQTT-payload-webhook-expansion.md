---
id: TASK-8
title: 'Fix undersized buffers: overflowCountBuf, MQTT payload, webhook expansion'
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:52'
updated_date: '2026-03-12 21:47'
labels:
  - bug
  - safety
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-Core.ino:3346'
  - 'src/OTGW-firmware/MQTTstuff.ino:419'
  - 'src/OTGW-firmware/webhook.ino:186'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Several buffers are too small for their maximum possible content, leading to silent truncation or potential overflow:

1. **overflowCountBuf[7]** (OTGW-Core.ino:3346): Used with `utoa(OTcurrentSystemState.errorBufferOverflow, ...)` where errorBufferOverflow is a uint32_t. A 32-bit unsigned can be up to 4294967295 (10 digits + null = 11 bytes). Buffer is only 7 bytes — overflows if counter exceeds 999999. Fix: increase to `char overflowCountBuf[12]`.

2. **msgPayload[50]** (MQTTstuff.ino:419): Incoming MQTT payload buffer in `handleMQTTcallback()`. Home Assistant status messages and OTGW command payloads could exceed 50 bytes. If payload > 49 bytes, it silently truncates via `copyMQTTPayloadToBuffer()`, causing commands to be corrupted. Fix: increase to 128 bytes and/or add length validation.

3. **expandedPayload[201]** (webhook.ino ~line 186): Webhook payload template expansion. Template can contain many `{variable}` placeholders that each expand to multi-character values. Dense templates can easily exceed 201 bytes after expansion. `snprintf` silently truncates, causing webhook receiver to get incomplete JSON. Fix: increase to 512 bytes or add truncation warning.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 overflowCountBuf increased to at least 12 bytes to safely hold any uint32_t value
- [ ] #2 MQTT incoming payload buffer increased to at least 128 bytes
- [ ] #3 Webhook expandedPayload buffer increased to at least 384 bytes
- [ ] #4 No new stack pressure issues from increased buffer sizes (verify total stack usage in each function)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed three undersized buffers:
1. overflowCountBuf[7] → [12] in OTGW-Core.ino (uint32_t needs up to 10 digits + null)
2. msgPayload[50] → [128] in MQTT callback (MQTTstuff.ino) — accommodates longer command payloads
3. expandedPayload[201] → [384] in webhook.ino — room for expanded template variables

Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
