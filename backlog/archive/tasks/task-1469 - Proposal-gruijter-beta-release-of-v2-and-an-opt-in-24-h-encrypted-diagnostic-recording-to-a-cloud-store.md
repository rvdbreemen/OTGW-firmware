---
id: TASK-1469
title: >-
  Proposal (gruijter): beta release of v2 and an opt-in 24 h encrypted
  diagnostic recording to a cloud store
status: Done
assignee: []
created_date: '2026-10-29T08:49:00Z'
updated_date: '2026-10-29T12:46:00Z'
labels:
  - proposal
  - tester
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 629000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester proposal, gruijter, #alpha-testing 2026-10-29 08:36 UTC (msg 1565283161800183861), addressed to Robert and Sergeant D, attachment OTGW_cloud_recordings_proposal.md (written by his agent; copy at %TEMP%\discord-attach\OTGW_cloud_recordings_proposal.md).
(1) Release v2 as beta "in the next few weeks" for a broader test group.
(2) Opt-in "share a 24-hour diagnostic recording" checkbox: the firmware streams a compact binary event log (OT frames, external inputs, PIC commands/responses, MQTT publishes, reboot/crash, optional debug text) in encrypted batches (X25519 per session + AES-GCM/ChaCha20, plain HTTP POST from its own FreeRTOS task, so no TLS on the device per ADR-003) to a Cloudflare free-tier Worker + Durable Object + R2 store; only the developers' offline key decrypts; device-side redaction of secrets and rounded lat/lon; consent text; 30-day retention; replay through the existing OT replay (TASK-1294) extended with timestamp pacing, input injection and host streaming. Budget: ~150-200 B/s, 13-17 MB/day uncompressed (extrapolated, not measured); flash headroom ~196 KB; boards without PSRAM get a ~16 KB buffer. His own open items: RAM headroom on a no-PSRAM board, timing impact of the POST task, real record rate, flash cost, who operates the account and owns the key, consent and retention text.
Points for the decision: privacy (data leaves the house, GDPR), an outbound internet dependency, crypto code and flash budget, account ownership and cost, and release timing (beta) which only Robert decides.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decided on (1) beta timing and (2) the recording feature, recorded in the notes; follow-up tasks filed if approved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 Robert decided (interactive, in session): (1) beta only after field proof: a green field run on a real boiler (gruijter) plus the open PID tasks (TASK-1414..1418, TASK-1423) and the TASK-1412 bench/field gates. (2) Recording: first local, cloud later: TASK-1470 (local recording, web UI download, host replay). The cloud part waits until the local step works and Robert has settled account and key ownership.

Constraint from martreides (#alpha-testing msg 1565312598595993792, 2026-10-29): units may run in a restricted VLAN with all outbound internet blocked. Recording must work fully local with no cloud dependency; a later cloud upload must fail quietly (buffer/drop, no stall, no reboot). See TASK-1471 for the existing outbound paths.

Robert 2026-10-29: local first; cloud upload parked as a later feature (task-1473). Captures and recordings stay on the user's own network.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decided by Robert 2026-10-29: beta after field proof; recording first local (TASK-1470), cloud later.
<!-- SECTION:FINAL_SUMMARY:END -->
