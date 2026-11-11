---
id: TASK-1807
title: >-
  OT-Direct: the boiler value cache goes stale: master mode answers from it
  after the boiler is gone, and it survives loopback, GW=R and mode switches
status: To Do
assignee: []
created_date: '2026-11-11T06:55:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 900000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report E, E-cand 1 and E-cand 3. The PIC has no boiler value cache it answers from: StoreValue (gateway.asm:3574-3607) only fills the PS=1 summary, a PIC reset clears it, and its presence signal is a live timer (BoilerAlive, set in HandleResponse :2432). OT-Direct keeps otBoilerCache[128] + otBoilerCacheValid[128] (OTDirect.ino:296-297) with no timestamp. E-cand 1: in master mode handleMasterModeSlaveFrame answers READ_ACK from the cache with no age or online check (:2715-2716); bOnline goes false only after a failed MsgID 0 (:1686-1692) and nothing invalidates the cache then; bEnableSlave defaults to true (OTDirecttypes.h:114). ADR-066:90 bounds staleness to < 2 s only while the poll runs. Scenario: master mode with a thermostat on the slave port, the boiler loses power: the thermostat keeps getting READ_ACK with the last status (flame on, CH active) and never sees the timeout a real boiler loss gives. OTGW-Core.ino:2857-2858 already notes the symptom for the DHW total. E-cand 3: resetTransientState (:2498-2577), called by applyOTDirectMode (:2582-2640) and GW=R, never clears otBoilerCache*; loopback fills valid[3] with a synthetic 0x000B (:1246-1247, :1541). otDirectBoilerPresent() is !loopback && valid[3] (:324-327), and satBoilerHardwarePresent() (SATcontrol.ino:2769-2771) uses it to gate the SAT simulation (REST 409, MQTT enable-reject, edge auto-disable). Scenario: a bench unit leaves loopback for gateway or master mode with nothing on the boiler terminals: presence stays true and the simulation cannot be enabled until a reboot; in the field an unplugged boiler keeps 'present' true. Related: TASK-1138 (loopback presence), TASK-1759 (ADR-224 cache-answered MsgID 28 slot, which adds a cache stamp; report E lists notes for it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In master mode the gateway does not answer from the cache while the boiler is offline (or answers only entries younger than a stated bound), so the thermostat sees the loss
- [ ] #2 A mode switch, GW=R and leaving loopback clear the boiler cache, so otDirectBoilerPresent() and otIsVentSlave() reflect only real answers given after the switch
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
