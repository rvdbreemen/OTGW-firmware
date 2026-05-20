---
id: TASK-637
title: >-
  Refresh README dev section and CHANGELOG Unreleased to reflect commits since
  v1.5.0-fix2
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-20 11:25'
updated_date: '2026-05-20 11:30'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit the 50+ commits between tag v1.5.0-fix2 (current latest public stable) and origin/dev tip; surface the user-visible items as a What's new on dev section in README.md and as additional bullets in CHANGELOG.md Unreleased. Existing entries stay; this is purely additive (plus the SAT-removal note now that dead SAT code was excised in #586 and #589).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README has a 'What's new on dev' section between the dev-branch banner and the 'What's New in v1.5.0' anchor, listing the headline changes since v1.5.0-fix2 (ADR-073/074/075, JIT discovery, HA availability semantics, proxy-answer routing, PIC HA button/select, flash script hardening, SAT removal, build/evaluator hardening)
- [x] #2 CHANGELOG.md Unreleased gains entries for: ADR-075 proxy-answer routing fix, MsgID 0 Status boiler-side gating, set-command debug surfacing, FSexplorer Update Firmware visibility fix, HA PIC button/select discovery (pseudo-ID 251), HA discovery topic wiper tool, flash_otgw.sh/.bat hardening, SAT subsystem removal from dev, build.py submodule auto-init, evaluate.py false-positive fixes, npx backlog fallback docs, openHAB/Domoticz integration guides, EN/NL PIC/ESP firmware-guide split
- [x] #3 'What's New in v1.5.0' section is unchanged (historical anchor preserved per /beta-prerelease skill rule)
- [x] #4 No em dashes in any new prose (uses colons, periods, commas, or parentheses)
- [x] #5 Changes commit to claude/create-beta-prerelease-em9Kh and a draft PR is opened against dev
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Re-read README dev banner / What's New / Latest stable sections.
2. Re-read CHANGELOG Unreleased section.
3. Insert new README section What's new on dev (since v1.5.0-fix2) between the dev banner and the v1.5.0 anchor.
4. Append missing bullets to CHANGELOG Unreleased grouped by Added / Changed / Fixed / Documentation / Removed.
5. Commit on claude branch and open draft PR against dev.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated README.md and CHANGELOG.md to reflect commits since v1.5.0-fix2.

README: new "What's new on dev (since v1.5.0-fix2)" section between the dev banner and the v1.5.0 historical anchor, grouped by MQTT/HA discovery, Web UI/diagnostics, tooling/build, code hygiene, and documentation. Banner updated to reflect 1.6.0-beta.N targeting.

CHANGELOG Unreleased: added bullets for ADR-075 proxy-answer fix, MsgID 0 Status gate, set-command debug surfacing, FSexplorer button fix, HA PIC button/select discovery, discovery topic wiper, flash script hardening, build/evaluator hardening, SAT subsystem removal, beta-prerelease tooling. Removed two stale SAT bullets that referenced 2.0.0 feature-branch work.

No em dashes in new prose. Docs-only commit; build/evaluator gates skipped per CLAUDE.md policy. Draft PR #610 opened against dev.
<!-- SECTION:FINAL_SUMMARY:END -->
