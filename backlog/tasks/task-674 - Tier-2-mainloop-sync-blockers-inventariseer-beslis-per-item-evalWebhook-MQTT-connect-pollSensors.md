---
id: TASK-674
title: >-
  Tier-2 mainloop sync-blockers: inventariseer, beslis per item (evalWebhook,
  MQTT connect, pollSensors)
status: To Do
assignee: []
created_date: '2026-05-23 05:28'
labels:
  - mainloop
  - responsiveness
  - research
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Drie bekende synchronous blockers in main loop, geidentificeerd in tweede mainloop-review na TASK-671.

Doel: per item documenteren wat de huidige bound is, welke alternatieven bestaan, en per item beslissen: accepteren (known limitation in ADR), gericht fixen, of negeren.

Geen code-werk in deze task -- alleen analyse en beslissingen. Implementatie volgt in afzonderlijke tasks per gekozen fix.

De drie items:
- Item 5: webhook.ino:222 attemptSendWebhook HTTPClient setTimeout(1000) -- tot 1s -- state-change van trigger-bit
- Item 6: MQTTstuff.ino:808/813 MQTTclient.connect() setSocketTimeout(15) -- tot 15s -- broker outage
- Item 7: sensors_ext.ino:259 sensors.getTempC() per sensor -- ~10ms/sensor -- elke poll tick
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Item 5 (evalWebhook): trigger-condities, huidige bound (1s), alternatieven (AsyncHTTPClient, kortere timeout, queue-and-fire) gedocumenteerd; per-item beslissing genoteerd
- [ ] #2 Item 6 (MQTTclient.connect blocking): trigger-condities (broker outage), bound (15s socketTimeout x 42s retry), alternatieven (kortere socketTimeout, vervangen PubSubClient) gedocumenteerd; per-item beslissing genoteerd
- [ ] #3 Item 7 (pollSensors OneWire): trigger-condities, bound (~10ms x sensor count), alternatieven (twee-fase poll request->read) gedocumenteerd; per-item beslissing genoteerd
- [ ] #4 Bij accepteren: ADR Proposed -> Accepted voor 'known sync-blocker surface' OF item-specifieke ADR
- [ ] #5 Bij fixen: nieuwe implementation-task per item aangemaakt met expliciete scope
- [ ] #6 Bij negeren: rationale in deze task's Final Summary
<!-- AC:END -->
