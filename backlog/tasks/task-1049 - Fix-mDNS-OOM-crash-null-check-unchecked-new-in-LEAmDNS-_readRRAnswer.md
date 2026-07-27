---
id: TASK-1049
title: 'Fix mDNS OOM crash: null-check unchecked new in LEAmDNS _readRRAnswer'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-27 20:22'
updated_date: '2026-07-27 20:30'
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
- [x] #1 Allocation site(s) in LEAmDNS _readRRAnswer are null-safe: OOM results in graceful parse abort, not Exception (2)
- [x] #2 python build.py --firmware exits 0
- [x] #3 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate crash site in core 2.7.4 LEAmDNS_Transfer.cpp _readRRAnswer\n2. Patch 6 allocation sites to new (std::nothrow) + null-guard bResult\n3. Persist via idempotent patch step in build.py (core dir is gitignored)\n4. Verify: build.py --firmware exit 0, evaluate.py --quick no new failures
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patched arduino/packages/.../LEAmDNS_Transfer.cpp: 6x 'new stcMDNS_RRAnswerX' -> 'new (std::nothrow)' + '(p_rpRRAnswer) &&' guard; added #include <new>. Callers in LEAmDNS_Control.cpp verified null-safe (check pointer before use, delete-if-nonnull on failure). Core tree is gitignored -> added patch_lea_mdns_oom() to build.py install_dependencies: idempotent (std::nothrow marker), refuses partial match (<6 sites), warns+skips if core layout changes.

Verified: build.py --firmware exit 0, fresh bin build/OTGW-firmware-1.7.2-beta.4+44f1198.ino.bin (764752B, 22:29) with patched LEAmDNS compiled in (7x nothrow in file). patch_lea_mdns_oom() idempotency confirmed by direct invocation ('already applied'). evaluate.py --quick: 1 pre-existing ADR-reference fail (dirty ADR-087, predates this change), no new failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Null-safe mDNS RRAnswer allocation under OOM (field crash epc1=0x40233cba excvaddr=0x8).

Changes:
- ESP8266 core 2.7.4 LEAmDNS_Transfer.cpp _readRRAnswer(): 6 allocation sites changed from plain new to new (std::nothrow) with a null-guard before the per-type reader, plus #include <new>. Core 2.7.4 plain new returns NULL on OOM but still runs the constructor (write at this+8 = the observed Exception 2). nothrow makes the compiler emit the null-check first; callers in LEAmDNS_Control.cpp already handle a NULL answer pointer, so OOM now degrades to a dropped mDNS answer instead of a reboot.
- build.py patch_lea_mdns_oom() added to install_dependencies: the core tree lives under gitignored arduino/**, so the patch is re-applied on every build. Idempotent via std::nothrow marker; refuses partial application (<6 sites) if a future core layout changes.

Tests: python build.py --firmware exit 0 with fresh binary; patch idempotency verified; evaluate.py --quick shows no new failures.

Risks/follow-ups: fixes the crash site only, not any heap pressure that gets a device to OOM; field diagnosis of the beta.4 reporter continues separately (heap trend + reboot interval questions on Discord). MDNS.update() remains ungated by design for now.
<!-- SECTION:FINAL_SUMMARY:END -->
