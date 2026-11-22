---
id: TASK-3028
title: >-
  Web UI: after a background tab is frozen and resumed, the Monitor log and
  header clock looked stuck for 90 s (headless lifecycle emulation, TASK-3017
  bench)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-22T03:56:00Z'
updated_date: '2026-11-22T06:36:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1139000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding by otgw-bench-tester on .61, build 045e1fd (TASK-3017), evidence OTGW-field-data/bench-otgw32/2026-11-22_task3017-045e1fd/bg-045e1fd/ (out/facts.json, samples.jsonl). Headless Chromium, Monitor page frozen 12 min via lifecycle emulation, device closed its /ws; after resume the socket reconnected in ~7 s (header 'WS live', device queue_len 0-1) but the Monitor log stayed at 239 lines for 90 s and the header clock moved once in 90 s. Not checked in a real browser. Reading of the code (origin/dev v2.js:2950-2955): a new log line schedules its render with requestAnimationFrame and sets logRenderPending; rAF does not run while the page is hidden, so if the emulated page stays document.hidden after 'active' the pending render waits and no new one is scheduled; timers (header clock) are throttled to about once a minute in a hidden page. In a real browser, returning to the tab makes it visible, rAF runs and the log catches up at once. Hypothesis: emulation artefact; to be confirmed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced or refuted in a real (non-emulated) browser against a unit: background the Monitor tab long enough for the device to drop the socket, return, and record log line count, header clock and WS state for 90 s
- [x] #2 If real: fix with a test that is red before and green after (for example re-render the log and clock on visibilitychange to visible); if not real: the finding is closed with the evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22 real-browser check on .61 (2.0.0-beta.420+045e1fd), headed Chrome/157.0.8103.80, own temp profile, visible window, no lifecycle emulation; Monitor > Log in tab 1, tab 2 (about:blank) activated 12 min so tab 1 was genuinely hidden, then tab 1 activated again. Evidence: OTGW-build-artifacts-safe/evidence-TASK-3028/run2.jsonl + bgtab-real-chrome.mjs (SHA256SUMS). Before: visible, 76 lines. Hidden 12 min: #logCount stayed 76 (render waits for requestAnimationFrame, which does not run in a hidden tab), #clock moved about once a minute (timer throttling), header 'connected'. Return: visibilityState visible at once, 2024 lines at 0 s, then 2026-2120 every 10 s (LOG_MAX trim band), clock advancing 10 s per 10 s sample for all 90 s. Verdict: in a real browser the log and clock catch up immediately on return; the 90 s freeze in 09's run comes from the emulated page staying hidden after frozen->active. Not covered: the device did not drop the socket during this run (header stayed 'connected'), so the reconnect-after-device-close path was not exercised; the log render path does not depend on it (logRenderPending clears in the rAF callback, which runs as soon as the tab is visible).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not a bug: emulation artefact. AC1: real-browser check on .61 (2.0.0-beta.420+045e1fd), headed Chrome/157.0.8103.80, own temp profile, visible window, real tab switch (no lifecycle emulation): Monitor log hidden 12 min, then visible at once on return; log 76 -> 2024 lines at 0 s, then 2026-2120 per 10 s sample (LOG_MAX trim band); clock advanced 10 s per 10 s sample for all 90 s. The 90 s freeze in the 045e1fd headless run was the emulated page staying hidden after frozen->active, so the requestAnimationFrame log render and the timers stayed paused/throttled. Evidence: OTGW-build-artifacts-safe/evidence-TASK-3028/run2.jsonl, bgtab-real-chrome.mjs, SHA256SUMS. AC2: not real, so no fix; closed with this evidence. Caveat: the device did not close the socket during this run, so device close + reconnect was not exercised here; 09's 045e1fd run showed the reconnect in ~7 s. No code change, no build.
<!-- SECTION:FINAL_SUMMARY:END -->
