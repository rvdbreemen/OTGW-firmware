---
id: TASK-1049
title: 'Fix mDNS OOM crash: null-check unchecked new in LEAmDNS _readRRAnswer'
status: To Do
assignee: []
created_date: '2026-07-27 20:22'
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
