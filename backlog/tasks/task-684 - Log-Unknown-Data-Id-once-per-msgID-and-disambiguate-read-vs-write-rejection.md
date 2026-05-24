---
id: TASK-684
title: Log Unknown-Data-Id once per msgID and disambiguate read-vs-write rejection
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 06:32'
updated_date: '2026-05-24 06:39'
labels:
  - diagnostics
  - beta.20
  - log-cleanup
  - opentherm
dependencies: []
references:
  - OTGW_1.6_beta_20.txt (crashevans
  - FW 1.6.0-beta.20+591131f
  - 'build #3380)'
  - 'Conversation: T1a/T1b suggestions from beta-20 log review'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From beta.20 telnet log (crashevans, FW 1.6.0-beta.20+591131f) the 'Unknown-Data-Id' diagnostic is both repetitive and ambiguous.

Repetition: ~145 lines in ~10 min for a deterministic set of 7-8 msgIDs that the boiler never starts supporting — once is informative, the rest are noise.

Ambiguity: the same 'Unknown-Data-Id' text covers two distinct events:
- thermostat reads an msgID the boiler doesn't implement (Read direction)
- thermostat writes an msgID and the boiler refuses (Write direction, '<ignored>' suffix in some paths)

This task does the cheapest two improvements: log Unknown-Data-Id once per msgID per session, and split the log line into two phrasings so the reader can tell which case happened without re-decoding the OT message bytes.

Out of scope: surfacing the support map on REST/stats/MQTT (that's a follow-up task), HA discovery suppression (needs an ADR).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A new per-msgID bitmap (or equivalent) records whether 'Unknown-Data-Id' has already been logged for a given msgID in this boot session. State lives in RAM only, resets on reboot.
- [x] #2 The first Unknown-Data-Id occurrence for any msgID in the current boot emits a clearly-worded line. Subsequent identical occurrences are suppressed (no log line).
- [x] #3 The log line distinguishes the two cases: a Read-direction Unknown-Data-Id reads as 'boiler does not implement msgID N (<name>)'; a Write-direction Unknown-Data-Id reads as 'boiler rejected write to msgID N (<name>=<value or -->)'.
- [x] #4 The full raw OT-message line (e.g. 'B70100500  16 Unknown-Data-Id - TrSet <ignored>') is preserved unchanged so existing log-grepping tools keep working.
- [x] #5 python build.py --firmware exits 0.
- [x] #6 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add three function-local static bitmaps in processOT() (OTGW-Core.ino):
   - lastMasterWasWrite[32]  — per-msgID bit: most-recent master frame was Write (1) vs Read (0). Updated whenever a master frame is processed.
   - unknownLoggedRead[32]   — per-msgID bit: clear-once line for read-direction Unknown-Data-Id already emitted.
   - unknownLoggedWrite[32]  — per-msgID bit: same for write-direction.
   Memory cost: 96 bytes static RAM. Indexed by (id >> 3, id & 7) — safe for full 0-255 msgID range.

2. After OTlookupitem is loaded (~line 4080 in processOT), add:
   - if master frame (masterslave==0): update lastMasterWasWrite[id] from OTdata.type.
   - if slave frame with type == OT_UNKNOWN_DATA_ID:
     * lookup direction = lastMasterWasWrite bit
     * pick the matching unknownLogged* bitmap
     * if already set → record suppressTelnetForRepeat=true (the raw line still goes to WebSocket)
     * else → set the bit and emit one OTGWDebugTf line via OTGW debug stream:
         OT_READ_DATA  → "boiler does not implement msgID N (<label>) - Unknown-Data-Id"
         OT_WRITE_DATA → "boiler does not accept writes to msgID N (<label>) - Unknown-Data-Id"

3. Around line 4185, gate the existing OTGWDebugT(skipOTLogTimestamp(ot_log_buffer)) call on !suppressTelnetForRepeat. Leave the sendLogToWebSocket(ot_log_buffer) call unchanged — the OT Monitor WebUI tab keeps showing every frame so existing tooling that watches the live stream is unaffected.

4. Build (python build.py --firmware) and evaluator (python evaluate.py --quick).
5. Commit on the existing claude/beta-20-log-review-7gnaR branch; PR #640 picks up the new commit automatically.
<!-- SECTION:PLAN:END -->
