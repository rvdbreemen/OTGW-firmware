---
id: TASK-865.17
title: 'feat(combo): boot-mode indicator + force/clear boardmode control'
status: To Do
assignee: []
created_date: '2026-06-14 20:10'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.16
parent_task_id: TASK-865
ordinal: 80000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (combo-only prerequisite; board-delta audit 2026-06-14)
The combo (ADR-127) boot-detects PIC-vs-OTDirect via reset->ETX and caches the result in settings.iBoardMode. The audit found a STICKY misdetection risk: on an OTGW32 the PIC-RX pin (GPIO44) floats and is known to drive an RX-interrupt storm (OTGW-firmware.ino:273-277); line noise could produce a stray 0x04 inside the find(ETX) timeout -> spurious PIC detection -> caches iBoardMode=1 -> persists across reboots, recoverable ONLY by forcing iBoardMode=2. For the single-combo-only endgame the maintainer requires (Q2 + audit): the booted mode must be VISIBLE (not a silent guess) AND user-forceable/clearable (the internal iBoardMode mechanism already exists; expose it).

## What
1. Expose the active hardware mode (PIC / OTDirect / detecting) on three surfaces: a web-UI status tile, an MQTT topic (e.g. <toptopic>/hw_mode or in the existing status payload), and the boot banner/telnet log.
2. Add a user-facing force/clear control: a REST endpoint (api/v2) + web-UI control to set the boardmode to auto / force-PIC / force-OTDirect, and to clear the cached iBoardMode and re-detect on next boot.
3. Honor a forced mode in the detection path: if the user forced a mode, skip the reset->ETX probe (or override its result) and use the forced mode; auto = current probe behaviour.

## Anchors
- detectPIC() OTGW-Core.ino:859-884; probe order + iBoardMode persist OTGW-firmware.ino:299-330; isPICEnabled() OTGW-firmware.h:504-507; state.hw.eMode.
- restAPI.ino (status + new endpoint), MQTTstuff/MQTTHaDiscovery (publish mode), data/index.html + index.js (status tile + force control), settingStuff.ino (iBoardMode setting).

## Notes
KISS: reuse the existing iBoardMode (1=PIC, 2=OTDirect). Force = set iBoardMode + a force flag so detection honors it; clear = reset iBoardMode to auto/0 and re-detect. Single-point-of-instantiation + ESP-abstraction rules apply.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build esp32 + esp32-classic + esp32-combo all SUCCESS; evaluate.py --quick no new failures
- [ ] #2 Web UI shows the active hardware mode (PIC / OTDirect / detecting)
- [ ] #3 MQTT publishes the active hardware mode; boot banner/telnet logs it
- [ ] #4 A force/clear control (REST + UI) sets boardmode to auto/force-PIC/force-OTDirect and can clear+re-detect; a forced mode is honored by the detection path (probe skipped/overridden)
- [ ] #5 FIELD (epic TASK-865): on OTGW32 and Classic the indicator shows the correct mode; forcing recovers a simulated misdetect (e.g. a stuck iBoardMode)
<!-- AC:END -->
