---
id: TASK-637
title: >-
  Refresh README dev section and CHANGELOG Unreleased to reflect commits since
  v1.5.0-fix2
status: To Do
assignee: []
created_date: '2026-05-20 11:25'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit the 50+ commits between tag v1.5.0-fix2 (current latest public stable) and origin/dev tip; surface the user-visible items as a What's new on dev section in README.md and as additional bullets in CHANGELOG.md Unreleased. Existing entries stay; this is purely additive (plus the SAT-removal note now that dead SAT code was excised in #586 and #589).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 README has a 'What's new on dev' section between the dev-branch banner and the 'What's New in v1.5.0' anchor, listing the headline changes since v1.5.0-fix2 (ADR-073/074/075, JIT discovery, HA availability semantics, proxy-answer routing, PIC HA button/select, flash script hardening, SAT removal, build/evaluator hardening)
- [ ] #2 CHANGELOG.md Unreleased gains entries for: ADR-075 proxy-answer routing fix, MsgID 0 Status boiler-side gating, set-command debug surfacing, FSexplorer Update Firmware visibility fix, HA PIC button/select discovery (pseudo-ID 251), HA discovery topic wiper tool, flash_otgw.sh/.bat hardening, SAT subsystem removal from dev, build.py submodule auto-init, evaluate.py false-positive fixes, npx backlog fallback docs, openHAB/Domoticz integration guides, EN/NL PIC/ESP firmware-guide split
- [ ] #3 'What's New in v1.5.0' section is unchanged (historical anchor preserved per /beta-prerelease skill rule)
- [ ] #4 No em dashes in any new prose (uses colons, periods, commas, or parentheses)
- [ ] #5 Changes commit to claude/create-beta-prerelease-em9Kh and a draft PR is opened against dev
<!-- AC:END -->
