---
id: TASK-684
title: Log Unknown-Data-Id once per msgID and disambiguate read-vs-write rejection
status: To Do
assignee: []
created_date: '2026-05-24 06:32'
labels:
  - diagnostics
  - beta.20
  - log-cleanup
  - opentherm
dependencies: []
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
- [ ] #1 A new per-msgID bitmap (or equivalent) records whether 'Unknown-Data-Id' has already been logged for a given msgID in this boot session. State lives in RAM only, resets on reboot.
- [ ] #2 The first Unknown-Data-Id occurrence for any msgID in the current boot emits a clearly-worded line. Subsequent identical occurrences are suppressed (no log line).
- [ ] #3 The log line distinguishes the two cases: a Read-direction Unknown-Data-Id reads as 'boiler does not implement msgID N (<name>)'; a Write-direction Unknown-Data-Id reads as 'boiler rejected write to msgID N (<name>=<value or -->)'.
- [ ] #4 The full raw OT-message line (e.g. 'B70100500  16 Unknown-Data-Id - TrSet <ignored>') is preserved unchanged so existing log-grepping tools keep working.
- [ ] #5 python build.py --firmware exits 0.
- [ ] #6 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->
