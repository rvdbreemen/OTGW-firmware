---
id: TASK-22
title: Release MQTT autodiscovery workspace after publish sessions
status: Done
assignee:
  - '@github-copilot'
created_date: '2026-03-18 19:45'
updated_date: '2026-03-18 21:08'
labels:
  - memory mqtt heap
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:42'
  - 'src/OTGW-firmware/MQTTstuff.ino:60'
  - 'src/OTGW-firmware/MQTTstuff.ino:1049'
  - 'src/OTGW-firmware/MQTTstuff.ino:1187'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff uses a lazy-allocated MQTTAutoConfigBuffers workspace containing line[1200], topic[200], msg[1200], and savedTopic[200], for about 2800 bytes plus allocator overhead. Compared to v1.2.0 this moved a static allocation to runtime heap, but once allocated it is kept forever. If low-heap measurements are taken after Home Assistant autodiscovery or reconnect republish, this retained block likely contributes materially to the observed drop. This task evaluates session-scoped allocation and release, or a smaller/reused workspace, while avoiding fragmentation or re-entrancy regressions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Runtime heap is recovered after MQTT autodiscovery/reconnect publish work completes
- [ ] #2 MQTT autodiscovery remains functionally correct across reconnects
- [ ] #3 No use-after-free or re-entrancy bug is introduced in autoconfig code paths
- [ ] #4 Heap fragmentation risk is assessed and documented before merging
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace copy-out parsing with in-place parsing of the mqttha.cfg line so autodiscovery keeps the raw template in a single line buffer.
2. Add streaming template helpers that first measure the rendered payload length and then write rendered chunks directly to PubSubClient, keeping only a rendered topic buffer.
3. Refactor bulk, per-msgid, and source-template paths to reuse the new helpers and remove the dedicated msg and savedTopic scratch buffers.
4. Build the firmware and verify autodiscovery code paths still compile cleanly with no new diagnostics.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented the lower-risk reduced-workspace variant rather than session-scoped free/reallocate. MQTTAutoConfigBuffers now retains only line[1200] and topic[200]; msg[1200] and savedTopic[200] were removed, and payloads are stream-rendered directly to PubSubClient.

Validation: python build.py --firmware now passes after restructuring helper signatures to avoid Arduino auto-prototype issues with custom structs.

Closed as done per user direction. Parent task originally described a full post-session heap recovery option; implemented and validated reduced retained workspace instead, with subtasks 22.1-22.3 completed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced retained MQTT autodiscovery workspace in MQTTstuff.ino by removing the dedicated message and saved-topic scratch buffers and replacing them with in-place config parsing plus stream-rendered MQTT payload publishing.

Changes:
- Added line parsing that keeps topic/message template pointers inside the shared line buffer.
- Added measured streaming template rendering for autodiscovery payloads.
- Rewired bulk discovery, per-msgid discovery, and source-template expansion to use the smaller workspace.
- Adjusted helper signatures to remain compatible with Arduino's generated prototypes.

Validation:
- python build.py --firmware

Note:
- This completes the reduced-workspace implementation path selected for TASK-22 subtasks, but it does not fully satisfy parent AC #1 if the requirement remains to reclaim the entire autodiscovery workspace after each session.
<!-- SECTION:FINAL_SUMMARY:END -->
