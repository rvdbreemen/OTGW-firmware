---
id: TASK-1755
title: >-
  Docs: SAT Known limitations wrongly say the wall thermostat still decides CH
  enable (ADR-208, ADR-199)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T12:08:00Z'
updated_date: '2026-11-09T12:11:00Z'
labels:
  - docs
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 857000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Known limitations of the SAT chapter say: SAT controls the flow temperature setpoint, but the wall thermostat still controls whether heating is enabled or disabled. Wrong while SAT holds its control setpoint: SAT's CH= sets the CH enable bit the boiler sees. PIC boards: the PIC CH= override rule (gateway.asm:53, :2700-2743; sergeant-d-corrections entry 60 capture). OT-Direct gateway mode: bit 0 of the thermostat's relayed MsgID 0 is rewritten (ADR-208, applyOverrides/otdRelayedStatusFrame in OTDirect.ino). Where SAT sends CH=0: ADR-199 point 2. Owner of MsgID 1 while SAT is enabled: ADR-179. Found in docs/MANUAL.md:1178, docs/manuals/en/ch05-sat-thermostat.md:673, docs/manuals/nl/h05-sat-thermostaat.md:727. Same wrong claim was corrected in public by another session (coordinator request 2026-11-09). Sweep also checked FAQ (no FAQ page in the repo; TASK-1748 is a wiki FAQ item), v2.js wizard and dial texts (correct), other docs (correct: ch05:587, h05:629 already say the thermostat's CH enable does not decide).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The three Known limitations lines state that while SAT is enabled SAT's CH= decides the CH enable bit the boiler sees, on PIC and on OT-Direct, citing ADR-208, ADR-199 and ADR-179
- [x] #2 The text says the thermostat controls CH enable again when SAT is off or tripped, and that the OT-Direct field capture with a live thermostat is still open (ADR-208 Verification)
- [x] #3 A repo grep on the old wording returns no hit outside backlog and ADRs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix 2026-11-09: replaced the line in docs/MANUAL.md (5.10), docs/manuals/en/ch05-sat-thermostat.md (5.22) and docs/manuals/nl/h05-sat-thermostaat.md (5.21). Sources checked this session: ADR-208 Decision and Verification (field capture open), ADR-199 point 2 (CH=0 paths), ADR-179 (SAT owns MsgID 1 while enabled), gateway.asm:53 ';CH=1 Override the CH enable bit when CS != 0', OTDirect.ino:600-612 applyOverrides calls otdRelayedStatusFrame (:822) for a gateway-mode READ_DATA MsgID 0. Grep for the old EN and NL wording outside backlog/ADRs/other-projects: no hit (grep exit 1). Side finding for the coordinator, not fixed here: CHANGELOG TASK-1701 removed the gateway's own MsgID 0 while a thermostat is connected, which is option A that ADR-208 lists under Must Not; ADR-208 is not superseded.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/AC2: the three Known limitations lines now say SAT's CH= decides the CH enable bit while SAT is enabled (PIC: CH= override; OT-Direct gateway: relayed MsgID 0 bit 0 rewrite), the thermostat controls it again when SAT is off or tripped, and the OT-Direct live-thermostat capture is open; sources ADR-208, ADR-199, ADR-179, verified against gateway.asm:53 and OTDirect.ino:600-612/:822. AC3: grep for the old EN/NL wording outside backlog, ADRs and other-projects returns nothing (exit 1). Not verified: rendering on the wiki (no wiki copy of these lines checked).
<!-- SECTION:FINAL_SUMMARY:END -->
