---
id: TASK-1049
title: 'Fix mDNS OOM crash: null-check unchecked new in LEAmDNS _readRRAnswer'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-27 20:22'
updated_date: '2026-07-27 20:27'
labels:
  - bug
  - mdns
dependencies: []
priority: high
ordinal: 168000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field reports (martreides TASK-1037 + new beta.4 reporter) show Exception (2) epc1=0x40233cba excvaddr=0x00000008: unchecked 'new stcMDNS_RRAnswer*' in LEAmDNS returns NULL under OOM, constructor writes at this+8. mDNS path is ungated (MDNS.update() runs outside heap gates), so any inbound mDNS query during low heap crashes the device. Busy LANs (Apple/Chromecast/HA multicast) trigger it; hotspots do not. Fix: null-safe allocation (new (std::nothrow) + check) at the crash site so OOM degrades to a dropped mDNS answer instead of a reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Allocation site(s) in LEAmDNS _readRRAnswer are null-safe: OOM results in graceful parse abort, not Exception (2)
- [ ] #2 python build.py --firmware exits 0
- [ ] #3 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate crash site in core 2.7.4 LEAmDNS_Transfer.cpp _readRRAnswer\n2. Patch 6 allocation sites to new (std::nothrow) + null-guard bResult\n3. Persist via idempotent patch step in build.py (core dir is gitignored)\n4. Verify: build.py --firmware exit 0, evaluate.py --quick no new failures
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patched arduino/packages/.../LEAmDNS_Transfer.cpp: 6x 'new stcMDNS_RRAnswerX' -> 'new (std::nothrow)' + '(p_rpRRAnswer) &&' guard; added #include <new>. Callers in LEAmDNS_Control.cpp verified null-safe (check pointer before use, delete-if-nonnull on failure). Core tree is gitignored -> added patch_lea_mdns_oom() to build.py install_dependencies: idempotent (std::nothrow marker), refuses partial match (<6 sites), warns+skips if core layout changes.
<!-- SECTION:NOTES:END -->
