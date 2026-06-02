---
id: TASK-811
title: 'Diagnosis: heap/MQTT/WebSocket regression analysis v1.2.0 vs v1.6.1'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-02 16:20'
updated_date: '2026-06-02 16:22'
labels:
  - docs
  - analysis
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Aggressive codebase analysis comparing dev v1.2.0 and v1.6.1 to explain a field user's MQTT instability, unexplained slowness and heap crashes. Deliverable: diagnosis document under docs/audits/ plus a ranked root-cause list with remediation paths.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Diagnosis document committed under docs/audits/ with versioned file:line and ADR/TASK evidence
- [x] #2 3-5 ranked root causes documented, each with a concrete how-to-address path
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Diagnosis doc written to docs/audits/2026-06-02-heap-mqtt-websocket-1.2.0-vs-1.6.1.md. Compared v1.2.0 vs v1.6.1 via git show/diff + 3 parallel subsystem deep-dives; directly verified two contested claims (v1.2.0 has emergencyHeapRecovery; v1.2.0 live-log was ungated). Committed 6a9ec033, pushed, draft PR #652 to dev.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added docs/audits diagnosis comparing dev v1.2.0 and v1.6.1. Establishes the WebSocket-producer / MQTT-victim heap conflict (ADR-083) as the unifying root cause, with the TASK-769 short-write desync as the acute crash trigger, halved heap thresholds, the 15s connect sync-blocker (ADR-080), and REST device/info heap churn (TASK-701/723). Includes a ranked 5-item root-cause list with remediation paths and a field diagnosis checklist. Docs-only; draft PR #652.
<!-- SECTION:FINAL_SUMMARY:END -->
