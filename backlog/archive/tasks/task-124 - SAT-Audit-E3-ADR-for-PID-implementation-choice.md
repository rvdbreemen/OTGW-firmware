---
id: TASK-124
title: 'SAT Audit E3: ADR for PID implementation choice'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-5
  - adr
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the PID implementation choice in SAT firmware: discrete PID variant, anti-windup approach, sampling interval and tuning strategy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR describes all considered PID variants
- [x] #2 Anti-windup strategy documented
- [x] #3 ADR published in backlog/decisions/ with correct number
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-069-sat-pid-v3-implementation.md documenting:
- Why standard PID with fixed gains was rejected
- Auto-gain derivation from heating curve coefficient
- Fixed 60s integration window convention
- Deadband-based integral/derivative asymmetry
- Temperature-based derivative (not error-based) with adaptive alpha LPF
- Solar gain freeze mechanism
- Anti-windup: reset outside deadband, clamp to [0, curveValue], hard cap at 20C
- All PID state as file-scope statics in SATpid.ino
<!-- SECTION:FINAL_SUMMARY:END -->
