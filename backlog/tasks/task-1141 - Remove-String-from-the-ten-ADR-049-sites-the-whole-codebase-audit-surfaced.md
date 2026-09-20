---
id: TASK-1141
title: Remove String from the ten ADR-049 sites the whole-codebase audit surfaced
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-20 11:21'
updated_date: '2026-09-20 11:55'
labels:
  - tech-debt
  - adr-049
dependencies: []
priority: low
ordinal: 222000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The whole-codebase ADR audit of 2026-09-20 (adr-audit --whole-codebase, default gates) reports ten ADR-049 violations: Arduino String used as a local or return type in files the ADR's Enforcement glob protects. All ten predate the rule and none sit on the OpenTherm frame path; they are on-demand request handlers and setup code. They are violations by the letter of ADR-049, whose Decision also covers initialization functions explicitly, and they are heap-fragmentation debt on a 40 KB device.

The ten sites, by function:
- OTGW-Core.ino:5368, 5371 checkforupdatepic(String filename) returns String; 5399, 5413, 5424 refreshpic(String filename, String version). Called from upgradepic() (REST /pic, FSexplorer.ino:281) and from sendPICUpdateCheck().
- restAPI.ino:1464, 1466 sendPICUpdateCheck(); 1803 sendApiNotFound() builds an HTML-escaped copy of the URI with String.replace.
- restAPI.ino:646 handleWebhook(): httpServer.arg(F("state")) returns String by API.
- networkStuff.ino:115 startWiFi(): WiFi.hostname() returns String by API.

Where the platform API returns String (httpServer.arg, WiFi.hostname), the fix is to copy into a sized char[] immediately and let the temporary die in the same statement, not to redesign the API. The PIC-update functions take and return String by choice and should move to caller-provided char[] buffers with explicit sizes, the pattern ADR-049 prescribes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr-audit --whole-codebase reports zero ADR-049 violations in OTGW-Core.ino, restAPI.ino and networkStuff.ino
- [ ] #2 checkforupdatepic and refreshpic take const char* and write results to caller-provided char[] buffers with explicit sizes; no String in their signatures or bodies
- [ ] #3 The PIC update check and PIC update flow behave as before on the bench OTGW (COM3): /pic reports the same version verdict and a PIC flash still completes
- [ ] #4 sendApiNotFound still HTML-escapes &, <, >, and quotes in the echoed URI (reflected-XSS guard preserved), verified with a crafted 404 URL
- [ ] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ID notice: the backlog CLI reused 1141 for this task on 2026-09-20. The archived task-1141 (Log every PR: response so a capture shows whether the PIC answers at all, closed the day before with its premise falsified) is a different record; commits from 2026-09-19 that cite TASK-1141 refer to that one. The CLI treats archived IDs as free (five other IDs exist in both backlog/tasks and backlog/archive/tasks), so this is tooling behaviour, not a copy.

Prior art: TASK-679 (Done) already replaced the String HTTP args in upgradepic(), and TASK-678 (Done) the String path in the firmware file list. The ten sites here are what those two left behind: the helpers checkforupdatepic()/refreshpic() still take and return String, plus the four request-path sites in restAPI.ino and networkStuff.ino.
<!-- SECTION:NOTES:END -->
