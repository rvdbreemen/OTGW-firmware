---
id: TASK-1457
title: >-
  feat-2.0.0: port TASK-1442 (1.x) — report a command the PIC refuses
  (NG/BV/...) on command_dropped
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:03:00Z'
updated_date: '2026-10-29T20:15:00Z'
labels:
  - port-from-1.x
  - mqtt
  - pic
milestone: 2.0.0
dependencies:
  - TASK-1439
priority: low
ordinal: 617000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The PIC refuses a command with an error code instead of an echo: "ZZ: NG" for an unknown command, "PR: BV" for a register an older PIC lacks. Codes: NG SE BV OR NS NF OE (otgw.tclcode.com/firmware.html).
Problem on dev: checkCommandResponse() in OTGW-Core.ino (after f3b1351e4, ~3890-3930) matches such a reply on its two letters, so the command is removed as confirmed, not reported, and clearCommandDropped() sets command_retry_exceeded OFF.
1.x fix (otgw-1.x.x 11c169021, OTGW-Core.ino): isPicErrorReply(code) with a PROGMEM list; reportCommandRejected(cmd, code) -> reportCommandDropped("<cmd> (<code>)"); in the match loop, before the PR register check, on an error reply report/remove the first queued command with the same two letters and break; no retry; flag not cleared.
Dev differences: there is also a PM= match (atoi on the id): catch the error reply before that check too, as before the PR check. OT-Direct has no queue, so a synthesised "XX: NG" finds nothing (fine, note it). SAT's CS= goes through the queue too, so a refused SAT CS= is reported as well (wanted).
Depends on TASK-1439 (Done). Draft from the 1.x session, 2026-10-29.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 test/host/test_cmd_queue_drop.py gains the 1.x R1-R3 cases (ZZ: NG -> 'ZZ=1 (NG)', flag ON, sent once; SB: OR does not clear the flag; PR: BV -> 'PR=Q (BV)' after one send) plus PM: NG -> 'PM=3 (NG)'; red on HEAD, green on fix; existing D/C cases stay green
- [x] #2 A normal confirmation still sets the flag OFF; PR= and PM= do not clear it (existing behaviour)
- [ ] #3 Bench on a PIC board (Classic .63) when available: ZZ=1 via MQTT gives command_dropped 'ZZ=1 (NG)'; otherwise recorded as not run
- [x] #4 esp32-combo build and evaluate.py --quick green; MQTT.md and CHANGELOG as on 1.x
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: implemented in dca69c850 (alpha.535), pushed to origin/dev. isPicErrorReply()/reportCommandRejected() in OTGW-Core.ino; the error reply is caught before the PR= register and PM= id matches.
- python test/host/test_cmd_queue_drop.py --old-rev origin/dev (6c34b58ad) -> RESULT: PASS: old fails R1 (ZZ: NG), R2 (SB: OR keeps flag ON), R3 (PR: BV), R4 (PM: NG); passes D1-D4, C1, C2; fix passes all 10.
- esp32-combo build from HEAD dca69c8: SUCCESS, 'Build completed successfully', 21:13 (OTGW_BUILD_JOBS=1, the rule at start time). evaluate.py --quick 79 passed, 0 failed.
- AC#3 (bench .63) not run: no PIC board available (.63 offline).
- Discord post via scripts/discord-post-build.sh was refused by the Claude Code auto-mode classifier (external write); not posted, reported to the 2.0.0 lead and to Robert.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A command the PIC refuses with an error code (NG, SE, BV, OR, NS, NF, OE) is reported on otgw-firmware/command_dropped as '<command> (<code>)' (e.g. 'ZZ=1 (NG)', 'PM=3 (NG)'), counts in cmd_dropped, latches command_retry_exceeded ON and is removed without retry; a refusal never clears the flag. Port of 1.x TASK-1442 (11c169021, bench-proven there on .68). The refusal is caught before the PR= register and PM= id matches; SAT PM=/CS= refusals are reported too; OT-Direct has no queue and is unchanged.

Tests: test_cmd_queue_drop.py old-vs-fix PASS (old fails R1-R4, controls green, fix 10/10); combo only (maintainer 2026-10-29): esp32-combo alpha.535+dca69c8 green; evaluate 79/0.

Not run: bench (.63 offline, no PIC board). Not posted to Discord (classifier refused the post).
<!-- SECTION:FINAL_SUMMARY:END -->
