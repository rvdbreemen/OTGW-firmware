---
id: TASK-674
title: >-
  Tier-2 mainloop sync-blockers: inventariseer, beslis per item (evalWebhook,
  MQTT connect, pollSensors)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 05:28'
updated_date: '2026-05-23 11:06'
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
- [x] #1 Item 5 (evalWebhook): trigger-condities, huidige bound (1s), alternatieven (AsyncHTTPClient, kortere timeout, queue-and-fire) gedocumenteerd; per-item beslissing genoteerd
- [x] #2 Item 6 (MQTTclient.connect blocking): trigger-condities (broker outage), bound (15s socketTimeout x 42s retry), alternatieven (kortere socketTimeout, vervangen PubSubClient) gedocumenteerd; per-item beslissing genoteerd
- [x] #3 Item 7 (pollSensors OneWire): trigger-condities, bound (~10ms x sensor count), alternatieven (twee-fase poll request->read) gedocumenteerd; per-item beslissing genoteerd
- [x] #4 Bij accepteren: ADR Proposed -> Accepted voor 'known sync-blocker surface' OF item-specifieke ADR
- [x] #5 Bij fixen: nieuwe implementation-task per item aangemaakt met expliciete scope
- [x] #6 Bij negeren: rationale in deze task's Final Summary
<!-- AC:END -->





## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Item 5 (webhook): drop http.setTimeout from 1000 to 500ms; collapse stale "was 3s" comment to a why-only line
2. Item 6 (MQTT connect): author Proposed ADR on dev capturing the 15s socketTimeout + 42s retry budget as a known sync-blocker tied to broker outage; replace MQTTstuff.ino:778 history comment with rationale; reference the ADR
3. Item 7 (pollSensors): no code change; documented in Final Summary as not-a-finding because setWaitForConversion(false) already absorbs the 750ms, and the remaining ~10ms/sensor is OneWire-protocol-inherent
4. Build python build.py --firmware exit 0, evaluate.py --quick no new failures on dev
5. Mirror to 2.0.0 in a sibling task: same Item 5 patch, sibling 2.0.0 ADR (separate numbering), same comment cleanups; build+eval there too
6. Commits land per branch; push to origin/dev (allowed by policy) and origin/feature-dev-2.0.0-otgw32-esp32-sat-support (allowed by policy)
7. Draft PR on each branch
8. Flag ADRs as Proposed; ask user to approve to Accepted before closing TASK-674
<!-- SECTION:PLAN:END -->
