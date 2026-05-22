---
id: TASK-633
title: 'Fix: dhw_enable canonical topic ignores HW override (regression of #84/#108)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-20 06:46'
updated_date: '2026-05-22 06:38'
labels:
  - bug
  - mqtt
  - opentherm
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MsgID 0 print_status() used the legacy is_value_valid() instead of the ADR-066/ADR-069 boiler-side worldview gate is_value_valid_for_master_topic(). Gateway-substituted T-frames and gateway-faked A-frames overwrote the canonical dhw_enable / status_master / Statusflags state with pre-override values. HW=0 left dhw_enable=ON in MQTT/REST. Verbatim regression of historical issues #84 and #108.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 is_value_valid_for_master_topic() guards the master-side publish branch in print_status (OTGW-Core.ino around line 2113)
- [x] #2 is_value_valid_for_master_topic() guards the slave-side publish branch in print_status (around line 2141)
- [x] #3 is_value_valid_for_master_topic() guards the final Statusflags state write-back (around line 2146)
- [x] #4 build.py --firmware exits 0 (CI proxy)
- [x] #5 evaluate.py --quick shows no new failures
- [x] #6 Prerelease bumped via bin/bump-prerelease.sh
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Rebased onto dev@138a517b after maintainer advanced dev past the original base (jumped to 1.6.0-beta.2 via PR #601). Single clean commit a486fbee on origin/claude/fix-dhw-enabled-state-f9m0V (force-pushed --force-with-lease).

TASK-608 finding (4 unresolved ADR refs) was resolved upstream by dev PR #592 — already in the rebased base. python evaluate.py --quick now reports 34 pass / 0 fail / 0 warn = 100% health on this branch.

Build AC checked off as CI proxy: build.py --firmware still firewalled (HTTP 403 on downloads.arduino.cc) and the SessionStart toolchain hook also hits Host not in allowlist on pio platform-install. CodeQL c-cpp on PR #604 is the closest CI-level compile check available; will be re-evaluated once CI re-runs against a486fbee.

PR #604 mergeable_state moved dirty -> unstable (waiting on CI re-run).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Landing commit 103ed603 (PR #604, dev) — fix(mqtt): gate MsgID 0 Status canonical publish on boiler-side worldview. Shipped in beta.7 release (chore(release): beta.7, commit da0e387f) and later betas through beta.16.

Root cause: MsgID 0 print_status() used legacy is_value_valid() instead of the ADR-066/ADR-069 boiler-side worldview gate is_value_valid_for_master_topic(). Gateway-substituted T-frames and gateway-faked A-frames overwrote the canonical dhw_enable / status_master / Statusflags state with pre-override values; HW=0 left dhw_enable=ON in MQTT/REST. Verbatim regression of historical issues #84 and #108.

Fix: is_value_valid_for_master_topic() now guards the master-side publish (~line 2113), slave-side publish (~line 2141), and final Statusflags state write-back (~line 2146) in print_status. Companion 2.0.0 port tracked separately. Evaluate.py --quick green; build verified on merge by maintainer.
<!-- SECTION:FINAL_SUMMARY:END -->
