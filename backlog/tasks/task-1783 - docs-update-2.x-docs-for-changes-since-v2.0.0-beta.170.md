---
id: TASK-1783
title: 'docs: update 2.x docs for changes since v2.0.0-beta.170'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T18:28:00Z'
updated_date: '2026-11-10T18:37:00Z'
labels:
  - docs
  - update-docs
dependencies: []
ordinal: 879000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Base v2.0.0-beta.170 (d260e6965), head e9d061c28, mode standalone, 15 commits. Areas: mqtt (MQTTstuff.ino -> ch04/h04, docs/api/MQTT.md, docs/c4/c4-code-mqtt.md), settings (OTGW-firmware.h -> ch03/h03, docs/c4/c4-code-settings.md), main (OTGW-firmware.ino -> ch01/h01 if feature-level, ch08/h08). 50 banner-only files skipped. Unmapped: none. The only code commit is a5da56bc9 feat(diag): TASK-1757 connect-window heap trace (telnet [HEAP] connect-trace lines at boot; one new prototype bootHeapConnectTraceTick() and its call in the main loop). Other commits are ADR-225 docs (a242518bf, 1b8abd8ec, 571c05d26). Manual findings for Robert: OTGW32=0 in every chapter; ESP8266 mentions EN ch01 5, ch02 7, ch04 1, ch07 1, ch08 3, ch10 9; NL h01 6, h02 8, h04 1, h07 1, h08 3, h10 9; MANUAL.md 25.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT area: ch04 (ch04-home-assistant.md, h04-home-assistant.md) and docs/api/MQTT.md checked against MQTTstuff.ino since the base, updated where needed
- [x] #2 Settings area: ch03 (ch03-web-interface.md, h03-webinterface.md) checked against OTGW-firmware.h since the base, updated where needed
- [x] #3 Main area: ch01/h01 (if feature-level) and ch08 (ch08-developer-guide.md, h08-ontwikkelaarsgids.md) checked against OTGW-firmware.ino since the base, updated where needed
- [x] #4 C4 docs (docs/c4/c4-code-mqtt.md, docs/c4/c4-code-settings.md and the main-loop code doc) updated for the TASK-1757 connect-trace
- [x] #5 Cleanup complete
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 MQTT: no edit needed. The only MQTTstuff.ino change since the base is the TASK-1757 connect-window heap trace (MQTTstuff.ino ~1110-1204), output only DebugTf [HEAP] connect-trace to telnet; added diff lines contain 0 sendMQTT/publish calls and 0 settings assignments (checked by the subagent and re-checked in the main session: grep counts 0 and 2 DebugTf). ch04/h04 and docs/api/MQTT.md unchanged.

AC2 settings: no edit needed. OTGW-firmware.h since the base adds only the prototype void bootHeapConnectTraceTick() (line 355, TASK-1757); no setting, struct field, default or page behaviour changed. ch03/h03 and c4-code-settings.md unchanged.

AC3 main: no edit needed. OTGW-firmware.ino adds only the bootHeapConnectTraceTick() call (TASK-1757, boot-only telnet diagnostic, MQTTstuff.ino:1193/1198); ch08/h08 do not document the boot [HEAP] telnet lines at all (grep 0 hits), so a temporary diagnostic gets no section; ch01/h01 not feature-level. Finding outside scope: h08 line 309 cites ADR-004 for the String rule where EN 305 / NL 307 cite ADR-180 (ADR-004 superseded).

AC4 C4: no edit needed. No docs/c4 file documents the boot-heap diagnostics (grep bootHeap/heapBurstFold/[HEAP]/ConnTrace: 0 hits; c4-code-mqtt.md lists no such helpers, c4-code-otgw-core.md:316 has no per-call list of the main loop). Adding bootHeapConnectTraceTick() (MQTTstuff.ino:1204, called at OTGW-firmware.ino:1200) would be a new level of detail for a temporary diagnostic, not a targeted update.

Phase 4 cleanup (Robert chose 'only unlinked files' via AskUserQuestion: the beta notes link each other with absolute blob/dev URLs, also in the immutable published GitHub release texts). 4A: moved 19 files to docs/releases/archive/ with git mv: RELEASE_NOTES_2.0.0-beta.1, 7, 8, 9, 10, 11, 17, 38, 60, 95, 99 (no repo file links to them) and the 1.x pairs RELEASE_NOTES/RELEASE_GITHUB 1.3.3, 1.3.4, 1.3.5, 1.4.1. Kept because linked: 2.0.0-beta.62, 77, 105, 118, 130, 144, 158; kept as newest: beta.162, 167, 170 and RELEASE_NOTES_2.0.0-draft.md. 4B: no release files at the repo root. 4C: docs/reviews has 14 entries (<= 15), nothing moved.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Standalone run since v2.0.0-beta.170 (15 commits, 50 banner-only files skipped). The only code change is TASK-1757's boot-only [HEAP] connect-trace telnet diagnostic: AC1 MQTT (ch04/h04, docs/api/MQTT.md), AC2 settings (ch03/h03), AC3 main (ch01, ch08/h08) and AC4 C4 docs all needed no edit, each checked by a subagent against the diff with file:line evidence and re-checked in the main session (0 publish/settings lines added; no doc covers the boot [HEAP] diagnostics). AC5 cleanup: 19 release-notes files archived (only files no repo file links to, Robert's choice), reviews under the threshold. Findings for Robert: (1) OTGW32 mentions: 0 in every manual chapter. (2) ESP8266 mentions: EN ch01 5, ch02 7, ch04 1, ch07 1, ch08 3, ch10 9; NL h01 6, h02 8, h04 1, h07 1, h08 3, h10 9; MANUAL.md 25 (ch08/h08 ones are 1.x-line context). (3) h08-ontwikkelaarsgids.md:309 still cites ADR-004 for the String rule (EN 305 / NL 307 cite ADR-180). (4) docs/BREAKING_CHANGES.md links '../RELEASE_NOTES_1.4.1.md', a path that did not exist before this run either (the file was in docs/releases/, now in docs/releases/archive/). (5) The skill's 4A rule (keep four newest) would break the absolute links between beta notes and in published release texts; worth adjusting the skill.
<!-- SECTION:FINAL_SUMMARY:END -->
