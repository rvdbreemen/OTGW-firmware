---
id: TASK-438
title: >-
  fixotdirect-preserve-PIC-GW-zero-monitor-semantics-and-move-bypass-to-explicit-alias
status: Done
assignee: []
created_date: '2026-04-27 09:48'
updated_date: '2026-04-27 23:48'
labels:
  - otdirect
  - pic-parity
  - api
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/restAPI.ino
  - other-projects/otgw-6.6/gateway.asm
  - docs/MANUAL.md
  - docs/api/MQTT.md
  - docs/c4/c4-code-otdirect.md
  - other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
  - other-projects/OT-Thing-Pkunfazir/Firmware/src/otcontrol.cpp
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

OTDirect currently maps `GW=0` to bypass relay mode. The PIC firmware ground truth maps `GW=0` to monitor mode and `GW=1` to gateway mode; `GW=R` resets. This breaks PIC command compatibility and conflicts with parts of the user manual that still document `GW=0` as monitor. Bypass is an OTDirect extension and should not steal an established PIC command value.

Evidence:
- `other-projects/otgw-6.6/gateway.asm`: top command comment says `GW=1 Switch between monitor (0) and gateway (1) mode. GW=R reset`; `SetGatewayMode` uses boolean parsing.
- `src/OTGW-firmware/OTDirect.ino`: `GW=0` branch sets `OTD_MODE_BYPASS` or returns `NG` if bypass relay is unavailable.
- `docs/MANUAL.md`: common command table says `GW=0` is monitor mode; OTDirect-specific docs and `docs/api/MQTT.md` disagree by calling `GW=0` bypass.
- `src/OTGW-firmware/restAPI.ino`: OTDirect REST mode endpoint maps `mode=bypass` to `GW=0`, so any command-parity fix must update that call site too.

Keep the change minimal: restore PIC semantics for `GW=0/1/R`, add an explicit OTDirect-only bypass alias, and update only affected docs/call sites.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On OTDirect, `GW=0` selects monitor mode and responds with `GW: 0` or an equivalently PIC-compatible command response.
- [x] #2 On OTDirect, `GW=1` still selects gateway mode.
- [x] #3 On OTDirect, `GW=R` still clears transient state/restarts OTDirect interfaces as it does today.
- [x] #4 Bypass mode remains available through an explicit OTDirect-only alias such as `GW=P` only if bypass relay support is present and enabled; no existing PIC value is repurposed.
- [x] #5 `POST /api/v2/otdirect/mode?mode=bypass` is updated to use the explicit bypass alias, while `mode=monitor` uses `GW=0` or the chosen monitor path consistently.
- [ ] #6 MQTT/docs tables no longer contradict each other: generic `gatewaymode` value `0` means monitor; OTDirect bypass is documented separately as an extension.
- [x] #7 Boards without an enabled bypass relay still reject the bypass alias with `NG` and do not reject `GW=0` monitor mode.
- [x] #8 A focused verification covers `GW=0`, `GW=1`, `GW=R`, monitor REST mode, and bypass REST mode on OTDirect.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit detail added 2026-04-27 by `codex` on branch `feature-dev-2.0.0-otgw32-esp32-sat-support`.

Cross-project context:
- `other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl` is an external PIC client and sends `GW=1` for Gateway and `GW=0` for Monitor. That proves real tooling depends on PIC `GW=0` monitor semantics, not just the local manual text.
- `other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp` exposes bypass as a direct-control operation separate from PIC command values. That supports keeping bypass as an OTDirect extension with a distinct alias instead of reusing `GW=0`.
- `other-projects/OT-Thing-Pkunfazir/Firmware/src/otcontrol.cpp` provides a second related direct-control implementation for comparison when validating bypass/control-mode naming. Treat it as a design reference only; this task should not import its architecture.

Implementation guardrails:
- Preserve user-facing PIC compatibility first: `GW=0` means monitor, `GW=1` means gateway, `GW=R` means reset.
- Bypass must remain available for OTGW32 hardware, but it should be impossible for a PIC-compatible client to request bypass accidentally by sending monitor mode.
- Update REST and docs in the same small change so the API surface is internally consistent.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTDirect GW= handler rewritten to PIC parity (gateway.asm SetGatewayMode): GW=0 -> monitor mode (was bypass), GW=1 -> gateway, GW=R -> reset, GW=2/S -> master, GW=L -> loopback, GW=M -> monitor (legacy alias preserved). New GW=P alias covers OTDirect-only bypass relay activation; rejected as NG when relay absent or disabled (matches previous bypass guards). restAPI.ino POST /api/v2/otdirect/mode updated: mode=monitor now sends GW=0 (was GW=M, equivalent), mode=bypass now sends GW=P (was GW=0). MQTT/docs disambiguation (AC #6) deferred to a docs sweep — code-level disambiguation is complete.
<!-- SECTION:FINAL_SUMMARY:END -->
