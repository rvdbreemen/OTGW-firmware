---
id: TASK-891
title: SAT thermo-nova spec-parity remediation (audit 2026-06-20)
status: Done
assignee: []
created_date: '2026-06-20 19:17'
updated_date: '2026-07-09 21:59'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 107000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT firmware (C port, src/OTGW-firmware/SAT*.{ino,h}) audited against the thermo-nova spec (docs/sat/SAT_thermo_nova_Documentation.docx) AND the thermo-nova Python source shipped in the repo (other-projects/SAT-releases-thermo-nova/custom_components/sat/). Method: multi-reviewer fan-out + adversarial verification + primary-source cross-check.

GROUND TRUTH (George / sergeantd, SAT author, #dev-sat-mqtt 2026-06-20): 'Python always wins', with the caveat that the author overrides Python where he flags a bug. COLD_SETPOINT=22 was his bug; correct values radiators 28.2 / underfloor 21. Robert: trust George (implement COLD_SETPOINT); classifier parity must keep heap floor ~40k.

VERDICT: firmware IS a faithful thermo-nova suppression port; gaps are detail-level. Full findings register: ~/.claude/plans/in-the-docs-sat-there-mossy-rabbit.md. Child tasks track each fix group.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain: the AUDIT is complete and its remediation children are 7/8 Done (891.1 flame-off/solar-gain, 891.2 COLD_SETPOINT, 891.3 overshoot/undershoot margins, 891.5 device-status fidelity, 891.6 remove obsolete OPV, 891.7 docx corrections, 891.8 split heating-source/system enums — all Done). Only 891.4 (cycle-classifier depth parity) remains and tracks its own work independently. The audit verdict ('firmware IS a faithful thermo-nova suppression port; gaps are detail-level') and the findings register (~/.claude/plans/in-the-docs-sat-there-mossy-rabbit.md) are the deliverable of THIS parent task, and both are done. Closing the audit; 891.4 continues on its own.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT thermo-nova spec-parity audit complete: firmware verified as a faithful thermo-nova suppression port with detail-level gaps, findings register written, 8 remediation child tasks spawned (7 Done, 891.4 in flight). George's 'Python always wins' ground truth + the COLD_SETPOINT bug-override captured. Audit deliverable done.
<!-- SECTION:FINAL_SUMMARY:END -->
