---
id: TASK-1864
title: >-
  Heap low-water on beta.214 (3.9-4.6 KB) against about 7 KB on beta.211: rule
  out the TASK-1757 boot race before calling it a regression
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T11:01:00Z'
updated_date: '2026-11-12T11:03:00Z'
labels:
  - heap
  - investigation
  - ot-direct
milestone: 2.0.0
dependencies:
  - TASK-1757
priority: medium
ordinal: 940000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observation (otgw-bench-tester 09, .61 OTGW32, 2026-11-12): hd_min_free_heap on beta.214+5d03c17 fell to 4,612 B, then 4,100 B (bench of TASK-1842/1844/1860, after a pyotgw session and with a Firefox WS client), then 3,948 B in a SAT simulation run (SAT on, satsimulation, no boiler, one extra Firefox WS client). CRITICAL is 3,072 B. In 09's earlier beta.211 run the minimum was about 7 KB. No reset, no crash, bootcount stable.

Not yet a regression. hd_min_free_heap is a run-wide watermark since boot, so each value likely includes the boot connect window. That is TASK-1757's finding: with one browser tab, boots are bimodal (18-19 KB vs about 7 KB in the boot window), with a 40-55 KB drop at the first MQTT write when a page load lands in it; TASK-1757's AC2 run on beta.167 already saw 4,244 B in the boot window and 3,600 B overall with two tabs. Robert's Firefox tab was open in every run here. Alternative explanation to rule out first: the same boot race landing worse, not a change in 211..214. Related: TASK-1757.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Like-for-like comparison on .61, beta.211 against beta.214: same scenario (SAT sim, no boiler), same WS clients, at least 30 min, the minimum heap reported separately for the first 60 s after boot and for steady state (not only hd_min_free_heap); ideally with TASK-1757's probe build so the dip is attributed
- [x] #2 Static review of src/ changes 203..214 per prerelease tag for new allocations, buffers, task stacks and connection handling, recorded in the notes
- [ ] #3 Only if AC1 shows a real steady-state difference: bisect over the tags (208, 211, 214) by the bench-tester and name the commit with the measurement; otherwise close as the TASK-1757 boot race with the numbers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC2 static review (95, 2026-11-12, no build; scratch scan of every src/ commit per segment for alloc/free, new/delete, String, std containers, reserve, task create/stack, queues/ringbuffers, AsyncClient, static arrays, char/uint8_t buffers >= 100, *_SIZE/LEN/CAP/MAX defines; added and removed lines):

beta.203 (e418ead3d) -> beta.208 (b1486c859): 204-207 are web UI only (TASK-1843, 1838, 1837, 1841: v2.js, v2.html, v2.css). Asset sizes 203 -> 208: v2.js 525,953 -> 529,032 B (+3,079), v2.html 65,091 -> 64,611 (-480), v2.css 67,414 -> 67,171 (-243); unchanged after 208. beta.208 b1486c859 TASK-1757 (MQTTstuff.ino): the connect burst leaves in portions and the drip waits on a deep outbox; scan hits are only a [HEAP] debug line per connect stage. This is the one segment that touches the boot connect window itself.

beta.208 -> beta.211 (b2e473f87): 9aa81609c TASK-1846 and 2216ea923 TASK-1855, OTDirect.ino only, no scan hits; b2e473f87 docs (version.hash only).

beta.211 -> beta.214 (47f9edd84): 8f6924552 TASK-1842, 60762c994 TASK-1844, 47f9edd84 TASK-1860: OTDirect.ino plus one emitted field in restAPI.ino (device/info otdpiccompat, a const char* from a flash string); no scan hits. New code is a few pure functions and one entry in the existing otOverrides table.

Static conclusion: no change in 203..214 adds a heap allocation, buffer, task or stack. The only change near the boot heap window is beta.208's TASK-1757 connect-burst change (by design it lowers the burst). So the static review gives no cause for a 211 -> 214 drop; AC1 (like-for-like measurement) decides. Not checked: libraries (no src/libraries change in the range), runtime behaviour.
<!-- SECTION:NOTES:END -->
