---
id: TASK-694
title: 'feat-2.0.0: port TASK-689 — WebUI OT Support tab showing bilateral msgID map'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 08:19'
updated_date: '2026-05-24 08:39'
labels:
  - port-from-dev
  - diagnostics
  - opentherm
  - observability
  - webui
dependencies: []
priority: medium
ordinal: 62000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-689 / commit 2adc06a9. Adds 'OT Support' tab between Statistics and Graph in the WebUI dashboard, showing a sortable table of every observed msgID with what the thermostat asked and what the boiler answered. New REST endpoint GET /api/v2/otgw/ot-support streams compact JSON (only rows where at least one of six bitmaps is set). Keeps the one-line 'Boiler does not implement: ...' footer on the Statistics tab. Depends on TASK-693 port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/otgw/ot-support returns 200 with streamed JSON { "msgids": [ {id, label, tsR, tsW, blAR, blAW, blUR, blUW}, ... ] }. Compact: only ids where at least one of the six bitmaps has the bit set. 160-byte stack buffer per row, no full-payload allocation.
- [x] #2 data/index.html gains a new tab button 'OT Support' positioned immediately after the Statistics button in the .ot-log-tabs nav, plus a matching <div id='OTSupport' class='tab-content'> with a table skeleton (columns: MsgID / Name / Thermostat / Boiler) and footer (#otSupportCount + #otSupportEmpty hint).
- [x] #3 data/index.js gains refreshOtSupport() that fetches /api/v2/otgw/ot-support, renders one <tr> per msgID, and is invoked from openLogTab when currentTab === 'OTSupport'.
- [x] #4 Thermostat column composes from tsR/tsW: 'R', 'W', 'R W', or '-'. Boiler column composes from blAR/blAW/blUR/blUW as space-separated tokens: 'R-ack', 'W-ack', '⚠ no read support', '⚠ rejects write'.
- [x] #5 Defensive coding per CLAUDE.md frontend rules: response.ok check, .catch on the promise chain, element-existence guards before DOM access, escapeHtml on user-supplied label/id strings. Empty msgids array shows the otSupportEmpty hint.
- [x] #6 The existing Statistics-tab footer line 'Boiler does not implement: ...' (from TASK-692 port) is preserved as-is.
- [x] #7 python build.py --firmware exits 0.
- [x] #8 python evaluate.py --quick shows no new failures vs current 2.0.0 baseline.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. restAPI.ino: new branch in handleOtgw for 'ot-support'. Streams JSON with one compact row per observed msgID using PROGMEM_readAnything(OTmap[id]) for label.
2. data/index.html: new 'OT Support' tab button between Statistics and Graph, plus #OTSupport tab-content div with table skeleton (MsgID/Name/Thermostat/Boiler) and otSupportCount + otSupportEmpty footer.
3. data/index.js: refreshOtSupport() invoked from openLogTab when OTSupport activated. Builds rows from JSON, sorted by id, composes Thermostat cell ('R', 'W', 'R W', '-') and Boiler cell ('R-ack', 'W-ack', '⚠ no read support', '⚠ rejects write').
4. Bump prerelease (alpha.61 -> alpha.62).
5. Build + eval.
6. Commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-689 (commit 2adc06a9 / PR #640) to 2.0.0: new 'OT Support' tab between Statistics and Graph showing the bilateral OT-bus support map. Sourced live from file-scope bitmaps populated in processOT plus the persisted /ot-thermo.json + /ot-boiler.json so reboots inherit prior bus-walk knowledge.

## Changes
- restAPI.ino: new branch in handleOtgw for 'ot-support'. Streams JSON with one {id, label, tsR, tsW, blAR, blAW, blUR, blUW} object per observed msgID; compact mode (rows where all six flags are false are skipped). 160-byte stack buffer per row.
- data/index.html: new tab button 'OT Support' inserted in the .ot-log-tabs nav between Statistics and Graph (with role='tab' / aria-selected / data-tab attrs matching the 2.0.0 nav convention). New #OTSupport tab-content div with table skeleton (MsgID / Name / Thermostat / Boiler) and footer with #otSupportCount + #otSupportEmpty hint.
- data/index.js: refreshOtSupport() added before refreshBoilerSupport(). Invoked from openLogTab when currentTab === 'OTSupport'. Composes Thermostat cell ('R'/'W'/'R W'/'-') and Boiler cell ('R-ack', 'W-ack', '⚠ no read support', '⚠ rejects write', space-separated). Sorted by id. Empty response shows the hint line. Defensive coding: response.ok, .catch, element-existence guards, escapeHtml on user data.
- The existing one-line 'Boiler does not implement: ...' indicator at the Statistics-tab footer (from the prior port) is preserved.

## Notes vs dev
- 2.0.0's tab nav uses role='tablist' + role='tab' + aria-selected + data-tab attributes; the new tab follows this richer convention (dev's plain tab buttons don't have aria attrs). data-tab value 'otsupport' (lowercase) follows the 2.0.0 pattern.
- 2.0.0 already has escapeHtml available (defined at line 3060); no change needed there.
- Zero net new firmware RAM. Stack buffer only during request handling.

## Verification
- python build.py --firmware --target esp8266 exits 0 (2.0.0-alpha.62).
- python evaluate.py --quick: 60/1/0 (98.5%, unchanged from baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
