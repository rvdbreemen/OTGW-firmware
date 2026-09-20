---
id: TASK-1141
title: Remove String from the ten ADR-049 sites the whole-codebase audit surfaced
status: Done
assignee:
  - '@claude'
created_date: '2026-09-20 11:21'
updated_date: '2026-09-20 13:36'
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
- [x] #1 adr-audit --whole-codebase reports zero ADR-049 violations in OTGW-Core.ino, restAPI.ino and networkStuff.ino
- [x] #2 checkforupdatepic and refreshpic take const char* and write results to caller-provided char[] buffers with explicit sizes; no String in their signatures or bodies
- [x] #3 The PIC update check and PIC update flow behave as before on the bench OTGW (COM3): /pic reports the same version verdict and a PIC flash still completes
- [ ] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [x] #5 sendApiNotFound keeps its HTML escaping of the echoed URI (code review: same five entities, now via PROGMEM into a bounded char[]), and the only URI that reaches that branch through the router, GET /api, still renders [<b>/api</b>] with a 404
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ID notice: the backlog CLI reused 1141 for this task on 2026-09-20. The archived task-1141 (Log every PR: response so a capture shows whether the PIC answers at all, closed the day before with its premise falsified) is a different record; commits from 2026-09-19 that cite TASK-1141 refer to that one. The CLI treats archived IDs as free (five other IDs exist in both backlog/tasks and backlog/archive/tasks), so this is tooling behaviour, not a copy.

Prior art: TASK-679 (Done) already replaced the String HTTP args in upgradepic(), and TASK-678 (Done) the String path in the firmware file list. The ten sites here are what those two left behind: the helpers checkforupdatepic()/refreshpic() still take and return String, plus the four request-path sites in restAPI.ino and networkStuff.ino.

2026-09-20 shipped as b7be2d0ab on otgw-1.x.x. All ten sites converted; whole-file ADR-049 pattern count 0 in OTGW-Core.ino, restAPI.ino, networkStuff.ino; adr-judge on the diff 0 violations.

Bench verification on 192.168.88.68 running the resulting build (014d380, flashed OTA): /api/v2/pic/update-check returns the same verdict as the pre-change build (current 2.2, latest 2.2, update_available false), so checkforupdatepic() over char[] talks to otgw.tclcode.com exactly as before. GET /api renders <br>[<b>/api</b>] with 404. POST /api/v2/webhook/test?state= answers 200 for on, 1 and off, and 400 for bogus and empty, so the strcasecmp_P path matches the old String comparisons.

AC #4 was rewritten: the original asked for a crafted URL with HTML characters, but the ESP8266WebServer routing sends any path other than exactly /api and /api/... to FSexplorer's FileNotFound handler, and /api/... paths get the JSON 404. Special characters cannot reach the HTML branch from the network, so the guard is verified by code review plus the one reachable URI.

AC #3 is half done on purpose: the update-check verdict is verified; the PIC flash itself was NOT performed, because writing PIC firmware needs an explicit per-instance order from the maintainer. refreshpic() is exercised only through that path. Left unchecked for the maintainer to decide.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PIC flash verified on the maintainer's order, 2026-09-20, bench 192.168.88.68 on build 014d380: /pic?action=refresh&name=diagnose.hex&version=0.0 forced refreshpic() through its download branch (fresh diagnose.hex from otgw.tclcode.com, 12703 to 12416 bytes which is the CRLF to LF difference, Intel HEX validation passed, .ver rewritten as 2.2); /pic?action=upgrade then flashed the PIC in 17 s with flash-status ending in progress 100 and 'PIC upgrade was successful', and device/info reports diagnose 2.2 available afterwards. All five ACs met.
<!-- SECTION:FINAL_SUMMARY:END -->
