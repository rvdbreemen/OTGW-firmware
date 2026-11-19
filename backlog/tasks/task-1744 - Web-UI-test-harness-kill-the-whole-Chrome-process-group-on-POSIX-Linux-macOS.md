---
id: TASK-1744
title: >-
  Web UI test harness: kill the whole Chrome process group on POSIX
  (Linux/macOS)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T05:08:00Z'
updated_date: '2026-11-16T10:37:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 849000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1739 by Robert 2026-11-09 (AC narrowed to Windows). stopChromeTree() in tests/webui/lib/page-harness.mjs only calls child.kill() on POSIX, so renderer/GPU children may outlive a run there. Fix candidate: spawn Chrome detached and kill the process group (process.kill(-pid)). No Linux/macOS host or CI is available here to evidence it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On Linux or macOS, no Chrome process is left after a run of the affected webui tests (process count before/after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 fix: tests/webui/lib/page-harness.mjs launchChrome() spawns Chrome with detached:true on POSIX (Chrome leads its own process group, marked child.__otgwGroup); stopChromeTree() then sends SIGKILL to -pid before child.kill(), so renderer/GPU/utility children end too. Windows unchanged (taskkill /T). Evidence on Windows only: chrome.exe count 0 before / 0 after otd-override-clear (5 pass, startPage) and graph-room-dropout (1 pass, launchChrome); harness-stall passes. AC1 NOT evidenced: it needs a Linux or macOS host with Chrome; WSL Ubuntu here has no Chromium and no node, and installing them needs sudo (not done). Leave In Progress until a POSIX run (process count before/after).

AC1 evidence (comms 92, 2026-11-16): manual workflow .github/workflows/webui-chrome-leak.yml (eb1541bcd, Node 22 in 8e29c3427) on ubuntu-latest runs tests/webui/otd-override-clear.test.mjs and graph-room-dropout.test.mjs one at a time and counts 'pgrep -c -f [c]hrome' before and 5 s after. Fix commit 8e29c3427: run https://github.com/rvdbreemen/OTGW-firmware/actions/runs/43848104184 -> before 0, after 0, tests pass 5/5 and 1/1, conclusion success. Commit before the fix 45348fc83: run https://github.com/rvdbreemen/OTGW-firmware/actions/runs/43848116785 -> before 0, after 0 too, success. So on the GitHub runner the old code does not leave Chrome either: the leak was NOT reproduced there (red not shown); the fix is shown clean on Linux. Earlier attempts: short SHAs could not be checked out; Node 20 has no global WebSocket (ReferenceError, then a hang to the 20 min timeout).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
On POSIX the web UI test harness starts Chrome detached and kills its whole process group (fix 9917092d2, tests/webui/lib/page-harness.mjs:110-112, stopChromeTree kills -pid). AC1: on Linux (ubuntu-latest, Node 22, workflow webui-chrome-leak.yml) no Chrome process is left after the two affected tests: run 43848104184, 0 before and 0 after, tests green. Windows was 0/0 before (earlier note). Not shown: the leak itself on Linux; the commit before the fix also leaves 0 on the GitHub runner (run 43848116785), so there is no red reference on this runner.
<!-- SECTION:FINAL_SUMMARY:END -->
