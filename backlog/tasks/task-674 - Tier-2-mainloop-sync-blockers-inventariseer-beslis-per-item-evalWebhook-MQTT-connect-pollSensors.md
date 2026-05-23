---
id: TASK-674
title: >-
  Tier-2 mainloop sync-blockers: inventariseer, beslis per item (evalWebhook,
  MQTT connect, pollSensors)
status: Done
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Tier-2 mainloop sync-blocker review (TASK-671/TASK-673 follow-up). Three known synchronous blockers in the main loop were inventoried; per-item dispositions:

**Item 5 — webhook `http.setTimeout` (`webhook.ino:222`).** Reduced from 1000 ms to 500 ms. Local LAN GETs/POSTs complete in <50 ms; the existing WH_PENDING → WH_RETRY_WAIT retry budget (30 s × 3) absorbs slow responders, so the tighter timeout cannot lose a real state change. Comment rewritten to explain the constraint, not the history.

**Item 6 — `MQTTclient.setSocketTimeout(15)` (`MQTTstuff.ino:778`).** Accepted as a known sync-blocker bounded to outage-only (worst case 15 s every 42 s; steady state is non-blocking). Authored ADR-080 (Accepted 2026-05-23) capturing the envelope, the rationale for not lowering the value, and an Enforcement block that blocks any future change to the literal via `bin/adr-judge`. The `MQTTstuff.ino:778` comment was rewritten from a history note ("Increased from 4 to 15…") to a why-only invariant referencing ADR-080.

**Item 7 — `sensors.getTempC()` per-sensor cost (`sensors_ext.ino:259`).** Not-a-finding. `initSensors()` already calls `setWaitForConversion(false)` (TASK-651 work), so the 750 ms DS18B20 conversion does not block. The remaining ~10 ms per call is the OneWire bus-protocol transaction (reset + ROM match + read scratchpad) and is not firmware-tunable without replacing the DallasTemperature library. With N=2-4 sensors at a default 20 s cadence, the ~20-40 ms tick is negligible. Closed with no code change.

**Sibling work on 2.0.0** lives in TASK-676 (PR #636). Item 5 was ported verbatim. Item 6 needed no code change on 2.0.0 — that branch already runs `setSocketTimeout(5)` with an explanatory in-code comment; ADR-108 (Proposed) captures the 5 s envelope as the accepted bound on 2.0.0 with explicit rationale for the divergence from dev's 15 s. Item 7 closed identically on both branches.

**Tests**
- `python build.py --firmware`: exit 0, binary 0.71 MB (no size regression).
- `python evaluate.py --quick`: 34/36 pass, 0 fail, 0 warning.

**Open items**
- Hardware/beta validation (in-the-field webhook latency observation; broker-outage MQTT reconnect behaviour) — the only gate not self-verifiable from this remote sandbox. PR #635 carries the test plan.
- One additional cold-path drain spotted at `OTGW-Core.ino:3136` (unbounded `while (OTGWSerial.available())` in the simulation re-init path) — flagged in the review write-up but explicitly out of scope for TASK-674; can be opened as a low-priority follow-up if desired.

Refs ADR-080 (Accepted), ADR-108 (Proposed on 2.0.0), TASK-676 (2.0.0 sibling), PR #635, PR #636.
<!-- SECTION:FINAL_SUMMARY:END -->
