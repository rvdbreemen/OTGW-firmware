---
id: TASK-1640
title: 'GitHub #692: serve LittleFS web assets pre-compressed with gzip on 2.0.0'
status: Done
assignee: []
created_date: '2026-11-04T23:51:00Z'
updated_date: '2026-11-11T05:29:00Z'
labels:
  - github
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 766000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/692 by andrebrait. Proposal: ship pre-compressed .gz web assets (about 73% smaller fs text, v2.js first load 266 KB to 74 KB), ETag caching (ADR-139/163) unchanged; reverses an earlier gzip drop, so an ADR. Needs Robert's go.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): parked. Keep the label.

Robert 2026-11-05 (session, AskUserQuestion): decided together with PR #697 (queue vs single gzipped file), after andrebrait's gzip+ETag check; no separate work until then.

2026-11-05 andrebrait (#alpha-testing 1568021809486372925, 1568024271396278284, 1568024836993982465; PR #697 comment 6670567252): gzip+ETag verified for a single inlined v2 page (/v2.html.gz 170,558 B: 200+ETag 1.17 s, reload 304 0.13 s, stale ETag 200). He proposes the same for FSexplorer and the regular firmware update page. Facts: the update page is already one self-contained page in flash (updateServerHtml.h:17, UpdateServerIndex PROGMEM, ~55 KB source, no external assets): only gzip-in-flash would apply; FSexplorer still loads fsexplorer.css separately and must stay independent of v2 assets. Decision with PR #697 (Robert).

Robert 2026-11-06 ~00:00 (AskUserQuestion, lead session), after Andre's gzip+ETag measurement: 'Beide: eerst queue, gzip later'. The PR #697 request queue (ADR-212) goes first; the single gzipped page is follow-up TASK-1668 with its own ADR (supersedes ADR-192), after measuring today's multi-file load time.

2026-11-06 implemented by PR #698 (André Brait), landed via TASK-1674 with the review fixes; ADR-215 Accepted.

2026-11-11 GitHub #692 closed as completed with Robert's OK: comment https://github.com/rvdbreemen/OTGW-firmware/issues/692#issuecomment-6787920267 (PR #698 shipped in beta.111, ADR-215).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented by PR #698 (André Brait), landed as 14d304af9 (beta.111) with the TASK-1674 review fixes; ADR-215 Accepted 2026-11-06 by Robert. Bench evidence in TASK-1674. GitHub issue #692 still open (closing it was not in the approval).
<!-- SECTION:FINAL_SUMMARY:END -->
