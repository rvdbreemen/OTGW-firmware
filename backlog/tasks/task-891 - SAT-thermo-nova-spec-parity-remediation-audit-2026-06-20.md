---
id: TASK-891
title: SAT thermo-nova spec-parity remediation (audit 2026-06-20)
status: To Do
assignee: []
created_date: '2026-06-20 19:17'
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
