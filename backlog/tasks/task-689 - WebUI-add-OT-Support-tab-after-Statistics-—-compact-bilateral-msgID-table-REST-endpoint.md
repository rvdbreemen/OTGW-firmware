---
id: TASK-689
title: >-
  WebUI: add 'OT Support' tab after Statistics — compact bilateral msgID table +
  REST endpoint
status: To Do
assignee: []
created_date: '2026-05-24 08:05'
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
- [ ] #1 GET /api/v2/otgw/ot-support returns 200 with a streamed JSON body { msgids: [ {id, label, tsR, tsW, blAR, blAW, blUR, blUW}, ... ] }. Only ids where at least one of the six bitmaps has a bit set are included. Stack buffer per row, no full-payload allocation.
- [ ] #2 index.html gains a new tab button 'OT Support' positioned immediately after the 'Statistics' button, plus a matching <div id='OTSupport' class='tab-content'> containing a table skeleton with columns MsgID, Name, Thermostat, Boiler.
- [ ] #3 index.js gains refreshOtSupport() that fetches the endpoint, renders one row per msgID into the table tbody, and is invoked from the tab-click handler. Empty response leaves the table empty (no row spam).
- [ ] #4 The Thermostat column shows 'R' if tsR, 'W' if tsW, 'R W' if both, '-' if neither (should not occur given the filter).
- [ ] #5 The Boiler column composes from the four boiler flags: 'R-ack' for acked_read, 'W-ack' for acked_write, '⚠ no read support' for unsupported_read, '⚠ rejects write' for unsupported_write. Multiple states space-separated.
- [ ] #6 Compatible across Chrome / Firefox / Safari (latest + 2 versions back). Element-existence checks before DOM access, try/catch on JSON.parse, response.ok on fetch, .catch on the promise chain — per CLAUDE.md frontend rules.
- [ ] #7 python build.py --firmware exits 0.
- [ ] #8 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->
