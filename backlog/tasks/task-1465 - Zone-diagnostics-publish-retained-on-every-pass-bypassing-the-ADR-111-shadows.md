---
id: TASK-1465
title: Zone diagnostics publish retained on every pass, bypassing the ADR-111 shadows
status: To Do
assignee: []
created_date: '2026-10-29T07:23:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 625000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. sat/zone/<n>/{active,output,error} go out via sendMQTTData on every pass (SATcontrol.ino:2313-2343), not through the publish-if-changed shadows (ADR-111).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Zone diagnostics go through the shadows (publish on change), or the exception is recorded in ADR-111 with the reason
<!-- AC:END -->
