---
id: TASK-391
title: 'Fix 1.4.2 webui boot-lag: render hotpath + lower restore cap to 10k'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-23 20:02'
updated_date: '2026-04-23 20:11'
labels: []
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Initial page load with a full log buffer (restored from localStorage) freezes the browser for 30-40 seconds. Console shows hundreds of rAF handlers taking 85-135ms plus Forced reflow bursts; WS watchdog timeout and REST timeouts cascade from the blocked main thread.

Root causes:
1. renderLogDisplay assigns an ~150KB escaped string to innerHTML, which re-parses the HTML even though the content is plain text. scrollHeight read immediately after forces sync layout every frame.
2. Normal-mode log cap climbs to ~16.777 lines based on storage quota, so restore has to chew through 8.5 MB before the UI settles.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 renderLogDisplay uses textContent (no HTML parse, no escapeHtml) for the log body
- [x] #2 scrollTop assignment is deferred via a follow-up rAF so scrollHeight read does not force layout in the same frame as the write
- [x] #3 Normal-mode cap (calculateOptimalMaxLines) is lowered from 200k ceiling to 10k for the 1.4.2 timeframe; capture-mode behaviour unchanged
- [x] #4 Build succeeds; browser console shows <50ms rAF handlers at steady state on a full 10k buffer restore
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Scope change to two spots: renderLogDisplay (~line 2120) and calculateOptimalMaxLines (~line 1019)
2. Render path: slice -> string build with plain concatenation -> container.textContent; skip if unchanged; move scrollTop into a second rAF so scrollHeight does not force layout in the same paint
3. Restore cap: clamp ceiling at 10000 in calculateOptimalMaxLines normal mode (cap only, capture mode untouched); leave 5000 floor alone
4. Verify CSS .ot-log-content has white-space preserving styling so textContent line breaks render
5. Run python build.py in background; user will load on device to verify rAF timing and watchdog behaviour
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation done:

- index.js:849-850: renamed lastRenderedLogHtml -> lastRenderedLogText; added scrollToBottomScheduled flag
- index.js:2149-2172 (renderLogDisplay): switched from escapeHtml + innerHTML to plain-string build + textContent; moved scrollTop assignment into a follow-up rAF guarded by scrollToBottomScheduled to avoid forced reflow in the same frame as the write
- index.js:1030-1032 (calculateOptimalMaxLines): clamped normal-mode ceiling from 200000 to 10000; capture mode unchanged
- CSS verified: .ot-log-content has white-space: pre so textContent line breaks render identically to the previous innerHTML output
- Build kicked off (background). Browser verification AC (#4) requires device flash + load, pending build success.

Runtime verification (AC#4) confirmed on device:

- Restored 10000 log entries from localStorage (was 16777)
- Normal mode cap reported consistently as 10.000 across 3 recalculations
- Zero rAF-handler violations (was ~400+ at 85-135ms)
- Zero Forced-reflow violations (was hundreds)
- WS connect #1 stays OPEN, no watchdog timeout, no reconnect cascade
- REST timeouts on /api/v2/device/time and /api/v2/otgw/otmonitor gone
- Side-effect observation: PIC now detected (picavailable: true) during initial load, suggesting the previous run was mostly dying under browser-induced ESP load
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduce 1.4.2 webui boot-lag by fixing the render hotpath and capping the restore buffer.

## Why

Initial page load with a full restored log buffer froze the browser for 30-40 seconds. Console showed hundreds of rAF handlers at 85-135 ms plus large Forced-reflow bursts. Cascading effects: WS watchdog timed out (main thread blocked, incoming WS frames not processed in time), REST calls hit ERR_CONNECTION_TIMED_OUT.

## Changes (src/OTGW-firmware/data/index.js)

- renderLogDisplay: switched from `escapeHtml(line) + innerHTML` to plain string concatenation + `textContent`. The log container already has `white-space: pre` (index.css:873), so `
` separators render identically while skipping the HTML parser and per-line escaping.
- Scroll-to-bottom deferred to a follow-up rAF guarded by `scrollToBottomScheduled`. The scrollHeight read + scrollTop write no longer force layout in the same frame as the textContent write.
- Renamed `lastRenderedLogHtml` to `lastRenderedLogText` to match reality.
- calculateOptimalMaxLines normal-mode ceiling clamped from 200_000 to 10_000. The 5_000 floor stays. Capture mode is untouched and still scales with memory.

## Impact

- Restore cap drops from ~16.777 (storage-quota-driven) to 10.000. localStorage restore reads less data and the initial render chews through ~40% fewer entries.
- Per-render work drops substantially: no HTML parse, no escape pass, and the forced reflow per render is eliminated.
- Expected downstream effect: WS watchdog no longer trips on first load, REST timeouts during boot disappear (main thread stays responsive so fetches start on time).

## Tests

- `python build.py`: clean build, no warnings, artefacts produced (0.70 MB firmware + 1.98 MB littlefs).
- No C/C++ changes, so evaluate.py not applicable.
- Runtime verification (AC#4 second half) requires flashing to a live OTGW and observing the browser console; this is left for the user to confirm on device.

## Follow-ups

- ESP8266 heap during boot (freeheap 11k, maxfreeblock 6.7k, fragmentation 39%) is still tight; the REST timeouts on fresh boot may have an ESP-side component independent of the browser render fix. Track separately if they persist after this change.
- If 10k proves too low for capture workflows, revisit the ceiling; capture mode already provides the escape hatch.
<!-- SECTION:FINAL_SUMMARY:END -->
