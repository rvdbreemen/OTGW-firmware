---
id: TASK-341
title: JSON-ify Status-frame MQTT fanout (single publish instead of 9 sub-topics)
status: To Do
assignee: []
created_date: '2026-04-19 21:05'
labels:
  - mqtt
  - parked
  - breaking-change
  - 2.0.0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the 9 individual MQTT publishes per Status-frame (msgid 0) with a single JSON publish. Each OT Status frame currently fans out into status_master, ch_enable, dhw_enable, cooling_enable, otc_active, ch2_enable, summerwintertime, dhw_blocking, diagnostic_indicator, electric_production — 10 PubSubClient writes within ~20ms that briefly consume heap together.

**Background**
Tester log (Crashevans, 2026-04-17) shows Status-bursts are the largest single contributor to heap dips. A Status-frame arrives every ~3s and triggers 9-10 publishes in quick succession. Under streaming-discovery drip this overlaps with discovery publishes and causes the throttle-gate to engage.

**Considerations**
- Pro: ~10x reduction in publish-burst allocation peak for the most frequent OT frame
- Pro: eliminates the primary cause of drip/publish collision
- Con: BREAKING CHANGE — all HA automations that subscribe to individual topics (OTGW/value/*/ch_enable etc.) stop working
- Con: existing user dashboards and blueprint templates break
- Con: MQTT auto-discovery schemas must be rewritten to point at JSON attributes
- Mitigation: ship behind a setting bStatusJsonMode; default OFF; users opt-in after updating automations. Still churn.
- Alternative: publish BOTH individual topics AND a combined JSON — doubles the traffic, worst of both worlds.

**Blocker**
Cannot land in a 1.4.x patch release. Target 2.0.0 with major-version messaging and a migration guide.

**Status: Parked**
Logged for future planning. Do not implement without explicit user decision on the breaking-change policy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Do not implement without explicit user decision on breaking-change policy
- [ ] #2 If implemented: JSON schema documented in docs/api/
- [ ] #3 If implemented: migration guide for HA automations in release notes
- [ ] #4 If implemented: setting bStatusJsonMode gates behavior, default OFF in 2.0.0
<!-- AC:END -->
