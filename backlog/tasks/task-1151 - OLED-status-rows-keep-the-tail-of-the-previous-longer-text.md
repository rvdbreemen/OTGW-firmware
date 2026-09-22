---
id: TASK-1151
title: 'OLED status rows keep the tail of the previous, longer text'
status: To Do
assignee: []
created_date: '2026-09-22 06:37'
labels: []
dependencies: []
ordinal: 286000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on Discord #dev-sat-mqtt 2026-06-12 by tjfs: 'When I plug in the Ethernet into otgw32 I get "Ethernet6617823" (the number is I think left over from my SSID) and the IP address when I unplug is 0.0.0.0.1.150, again I think left over, need to clear the rest of the line on the oled when updating info'

Verified against dev HEAD 2a2e5fba3 (2026-09-22). Still applicable; an adversarial pass confirmed every citation verbatim.

NOTHING CHANGED SINCE THE REPORT. git log -G on OLED.ino returns only three commits, all BEFORE 2026-06-12: 54b77945d (flame bitmap), d27e0c12b (TASK-751 ethernet failover), 94d5244d4 (OLED driver rewrite).

ROOT CAUSE: the display is cleared only on a page change (OLED.ino:596-599, stated in the header comment at OLED.ino:25-26). Individual rows are rewritten in place, so redrawing a row with SHORTER text leaves the tail of the previous text on screen. tjfs saw exactly the two worst cases: row 2 writes 'Ethernet' (8 chars) over 'WiFi: ' + up to 15 chars of SSID (OLED.ino:224-238), and row 3 writes a shorter IP over a longer one.

THIS IS A CLASS, NOT TWO LINES. The same hazard exists on the page 2 flame row (OLED.ino:294-304), the page 4 CH/DHW flag row (OLED.ino:391-394), and every fmtFloatOrDash() call site. Conditionally skipped rows are a second variant: the RSSI row (OLED.ino:245-254) is not drawn at all on Ethernet, so whatever was there stays. There is already a manual-padding workaround at OLED.ino:311-314, which shows the hazard was known but handled ad hoc rather than uniformly.

ADR NOTE: ADR-067 records 'Accepted: at 1 Hz refresh with clear() first', which does not match the shipped clear-on-page-change strategy. Reconcile the ADR with the implementation as part of the fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Redrawing any OLED status row with shorter text leaves no characters of the previous text visible
- [ ] #2 Plugging Ethernet into an OTGW32 shows exactly 'Ethernet' on row 2, with no SSID remnant
- [ ] #3 Unplugging Ethernet shows exactly the new IP on row 3, with no remnant of the previous address
- [ ] #4 The fix covers the whole class: page 2 flame row (OLED.ino:294-304), page 4 CH/DHW flags (OLED.ino:391-394) and every fmtFloatOrDash() call site
- [ ] #5 Conditionally skipped rows are blanked rather than left stale, specifically the RSSI row (OLED.ino:245-254) on Ethernet transport
- [ ] #6 No visible flicker at the 5 s refresh cadence (OLED_REFRESH_MS) and no loop-timing regression from extra I2C traffic
- [ ] #7 The manual-padding workaround at OLED.ino:311-314 is removed in favour of the uniform mechanism, or documented as deliberate
- [ ] #8 ADR-067's clear() consequence is reconciled with the shipped clear-on-page-change strategy
- [ ] #9 python build.py --target esp32 exits 0 with a fresh firmware.bin; python evaluate.py --quick shows no new FAILs
- [ ] #10 Field-verified on the bench OTGW32 with an Ethernet cable plugged and unplugged
<!-- AC:END -->
