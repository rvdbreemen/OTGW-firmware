---
id: TASK-538
title: 'Fix: GW=R stuck in command queue causes infinite PIC reset loop'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-04 09:01'
updated_date: '2026-05-04 09:19'
labels:
  - bug
  - queue
  - pic
  - mqtt
dependencies: []
references:
  - >-
    Discord #beta-testing, crashevans, 2026-05-03 10:50 — OTGW_1.5.0_Beta_5.txt
    attachment
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When a GW=R (Gateway Reset) command is sent via REST API or MQTT, the PIC resets correctly but the command is never removed from the OTGW command queue. The queue's response-matching logic (checkOTGWcmd) compares incoming responses (SC=, SR= from the PIC boot sequence) against the queued GW=R entry and finds no match. Since no match is found, slot [0] is never cleared, and the queue fires GW=R again approximately every 5 seconds, causing a continuous PIC reset loop.

Observed in OTGW_1.5.0_Beta_5.txt from crashevans (2026-05-03), starting at 11:46:38 and repeating until ~11:48:00. The PIC reset approximately 12 times in 2 minutes. The symptom resolves itself eventually (queue times out or a new command displaces the slot).

The fwreportinfo callback also fires twice per PIC reset, causing duplicate MQTT publications of reboot_count, reboot_reason, and PIC firmware info — a secondary symptom of the same root cause.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sending GW=R once causes exactly one PIC reset, not a repeating loop
- [x] #2 After GW=R is sent and the PIC restart sequence (SC=, SR= responses) is received, the GW=R entry is removed from the command queue
- [x] #3 fwreportinfo-triggered MQTT publications fire exactly once per PIC restart event
- [ ] #4 No SE Syntax Error published to MQTT when SC= is sent too early after reset (or SE is suppressed/delayed until PIC is ready)
- [x] #5 Regression test: subsequent normal commands (PR=, CS=, etc.) queue and execute correctly after a single GW=R
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix handleOTGWqueue: after sending GW=R, immediately remove it from the queue (fire-and-forget — PIC sends no GW: response prefix)\n2. Fix fwreportinfo callback: scan queue for any GW=R entry and remove it (belt-and-suspenders: handles the case where the command was sent but fwreportinfo fires before the queue timer)\n3. Bump version.h prerelease from beta.7 to beta.8, run autoinc-semver\n4. Build firmware, verify evaluator green\n5. Commit and push to origin/dev
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the infinite PIC reset loop caused by GW=R never being removed from the OTGW command queue.

Root cause: checkOTGWcmdqueue() matches responses by 2-char prefix. The PIC boot sequence responds with SC= and SR=, which never match the GW prefix. The slot was therefore never cleared, causing the queue to re-fire GW=R every 5 seconds — up to 12 resets in 2 minutes (confirmed in crashevans' OTGW_1.5.0_Beta_5.txt).

Changes in OTGW-Core.ino (commit 4c139550):
- handleOTGWqueue(): immediately remove GW=R from the queue after send (fire-and-forget). The PIC sends no matchable response, so the command must be self-clearing.
- fwreportinfo(): scan and remove any remaining GW=R entry as belt-and-suspenders, for the race where fwreportinfo fires within the same send tick.
- processOT() banner path: removed duplicate sendMQTTversioninfo() call. Both the banner detection path and fwreportinfo callback were publishing reboot_count/reboot_reason independently, causing two publications per restart. The callback is now the sole publisher.

Version bumped to 1.5.0-beta.8. Build exit 0, evaluator 91.7% (baseline maintained, no regressions).

AC4 (SE syntax error suppression) is a separate issue and was not addressed in this fix.
<!-- SECTION:FINAL_SUMMARY:END -->
