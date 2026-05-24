---
id: TASK-689
title: >-
  WebUI: add 'OT Support' tab after Statistics — compact bilateral msgID table +
  REST endpoint
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 08:05'
updated_date: '2026-05-24 08:08'
labels:
  - diagnostics
  - opentherm
  - observability
  - user-experience
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-688 persisted the bilateral OT-bus support map (thermostat sent_read/sent_write + boiler acked_read/acked_write + boiler unsupported_read/unsupported_write) but left it internal-use only. This task surfaces it in the dashboard as a dedicated tab.

Scope:
1. New REST endpoint GET /api/v2/otgw/ot-support — streams JSON, one object per observed msgID with the OT label and six boolean flags (tsR/tsW/blAR/blAW/blUR/blUW). Compact mode: only ids where at least one of the six bitmaps has the bit set.
2. New tab 'OT Support' inserted after 'Statistics' in index.html, containing one table with columns: MsgID, Name, Thermostat (R/W indicators), Boiler (R-ack/W-ack/⚠ no read support/⚠ rejects write).
3. JS fetcher invoked when the user activates the tab; renders rows into the table, sortable by msgID.
4. No new firmware RAM — the data is sourced live from the file-scope bitmaps populated in processOT().

Out of scope: showing all 134 known msgIDs even when bus has not observed them (compact mode only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/otgw/ot-support returns 200 with a streamed JSON body { msgids: [ {id, label, tsR, tsW, blAR, blAW, blUR, blUW}, ... ] }. Only ids where at least one of the six bitmaps has a bit set are included. Stack buffer per row, no full-payload allocation.
- [x] #2 index.html gains a new tab button 'OT Support' positioned immediately after the 'Statistics' button, plus a matching <div id='OTSupport' class='tab-content'> containing a table skeleton with columns MsgID, Name, Thermostat, Boiler.
- [x] #3 index.js gains refreshOtSupport() that fetches the endpoint, renders one row per msgID into the table tbody, and is invoked from the tab-click handler. Empty response leaves the table empty (no row spam).
- [x] #4 The Thermostat column shows 'R' if tsR, 'W' if tsW, 'R W' if both, '-' if neither (should not occur given the filter).
- [x] #5 The Boiler column composes from the four boiler flags: 'R-ack' for acked_read, 'W-ack' for acked_write, '⚠ no read support' for unsupported_read, '⚠ rejects write' for unsupported_write. Multiple states space-separated.
- [x] #6 Compatible across Chrome / Firefox / Safari (latest + 2 versions back). Element-existence checks before DOM access, try/catch on JSON.parse, response.ok on fetch, .catch on the promise chain — per CLAUDE.md frontend rules.
- [x] #7 python build.py --firmware exits 0.
- [x] #8 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added an "OT Support" tab to the WebUI dashboard placed between Statistics and Graph. Renders a sortable table of every msgID the OT bus has touched, with one row per id showing what the thermostat asked and what the boiler answered. Sourced live from the file-scope bitmaps populated in processOT (TASK-685/686/688) plus the persisted /ot-thermo.json + /ot-boiler.json files so reboots inherit prior bus-walk knowledge.

## Changes
- **restAPI.ino**: new branch in handleOtgw for "ot-support". Streams JSON with one `{id, label, tsR, tsW, blAR, blAW, blUR, blUW}` object per observed msgID. Compact mode — rows where all six flags are false are not included. 160-byte stack buffer per row, no full-payload allocation.
- **data/index.html**: new tab button "OT Support" between Statistics and Graph, plus #OTSupport tab-content containing the table skeleton with MsgID / Name / Thermostat / Boiler columns and an #otSupportCount footer.
- **data/index.js**: refreshOtSupport() invoked from openLogTab when the tab is activated. Composes the Thermostat cell ("R", "W", "R W", "-") and Boiler cell ("R-ack", "W-ack", "⚠ no read support", "⚠ rejects write", space-separated). Sorted by id. Empty response shows a hint line.
- The existing one-line "Boiler does not implement: ..." indicator at the Statistics-tab footer is **kept** alongside the new tab (per user direction).

## Memory
Zero net new firmware RAM. Stack buffer only during request handling.

## Verification
- python build.py --firmware exits 0.
- python evaluate.py --quick: 34 passed / 0 warnings / 0 failures (100% health).
<!-- SECTION:FINAL_SUMMARY:END -->
