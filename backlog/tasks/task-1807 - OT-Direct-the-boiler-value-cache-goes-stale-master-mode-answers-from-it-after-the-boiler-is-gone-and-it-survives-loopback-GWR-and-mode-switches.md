---
id: TASK-1807
title: >-
  OT-Direct: the boiler value cache goes stale: master mode answers from it
  after the boiler is gone, and it survives loopback, GW=R and mode switches
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:55:00Z'
updated_date: '2026-11-15T02:26:00Z'
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
- [x] #1 In master mode the gateway does not answer from the cache while the boiler is offline (or answers only entries younger than a stated bound), so the thermostat sees the loss
- [x] #2 A mode switch, GW=R and leaving loopback clear the boiler cache, so otDirectBoilerPresent() and otIsVentSlave() reflect only real answers given after the switch
- [x] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (d3, lane-1924). AC1: handleMasterModeSlaveFrame() sends no answer while state.otBus.bOnline is false (the boiler's last MsgID 0 failed); it logs the thermostat frame ('T') only. The thermostat sees a boiler loss as from a real boiler, instead of READ_ACKs with the last cached status. Offline ends at the next good MsgID 0, about 1 s later since TASK-1802 bug 1, so a one-frame blip costs the thermostat at most a few unanswered conversations, which it retries (spec 4.5). Chosen over an age bound on cache entries: the scheduler refreshes entries at 0.8 s to 60 s intervals, so one age bound would either expire slow entries in normal running or keep fast ones too long.

AC2: resetTransientState() clears otBoilerCache, otBoilerCacheValid and otdMsgId3FromBoiler. It runs on GW=R and on every switch into or out of master, loopback or bypass. Interpretation, recorded: a gateway <-> monitor switch does NOT clear the cache. Robert's TASK-1839 answer A keeps state on that switch, and in both modes the cache is refreshed by live boiler answers, so it cannot go stale through that switch. AC2 says 'a mode switch'; this one switch is left out on purpose.

AC3 evidence (real code, host compiler, OTGW_OTD_INO = HEAD 4596aa0b4 copy for old): tests/test_otd_master_empty_slot.py new case test_no_answer_from_the_cache_while_the_boiler_is_offline: fix 6/6 OK; old FAILED, thermostat got 'id=0 type=4 data=0308', 'id=25 type=4 data=3C00' while offline. tests/test_otd_mode_change_keeps_state.py (real resetTransientState/applyOTDirectMode + real otDirectBoilerPresent/otIsVentSlave) new cases: GW=R, ->master, ->loopback, ->bypass, loopback->gateway, master->gateway clear the cache (valid3/present/vent 0/0/0), gateway<->monitor keeps it: fix 8/8 OK; old 6 subtests FAIL ('1','1','1'). Harness stubs added in test_otd_master_tset_expiry and test_otd_mode_switch_queue_reset. All 39 tests/test_otd*.py + test_outside_stamp_gateway_frames OK; evaluate.py --quick 0 failed. docs/c4/c4-code-otdirect.md updated. AC4 open: build + bench (lane slot).

Lane-1924 pushed 2026-11-15: origin/dev 5da0c91df..2f4818aa1. Build esp32-combo 2.0.0-beta.281+fe0e61a OK (build/logs/build-20261115-020302.log, 'Build completed successfully'); 'git diff fe0e61a48 2f4818aa1 -- src/' empty, so the pushed code is the benched code. evaluate.py --quick 0 failed; OTD tests 42 files, host suites, SAT suite per file green on the rebased tree. Bench 09 on .61 with beta.281: after loopback, GW=S / GW=1 / GW=R+GW=S, sim_available stays true (the synthetic MsgID 3 no longer counts as a boiler). PASS for AC2. AC1 (no answer to a thermostat while offline) is field-only, host-tested. Version beta.278.

AC4 ticked by 09 (bench part; build and evaluate are d3's lane evidence): 09 bench 2026-11-15 02:12-02:57 on .61 (OTGW32, OT-Direct, no boiler, no thermostat), 2.0.0-beta.281+fe0e61a (OTA app+FS, changed none, bootcount 700->702), evidence 09 scratchpad b281/. Leaving loopback via GW=S, GW=1 and GW=R+GW=S (20 s loopback each): /api/v2/sat/status sim_available (= !satBoilerHardwarePresent) stays true, so the synthetic cache entry no longer counts as a boiler. health/device-info boilerconnected is a separate age signal and drops ~30 s after leaving loopback (boiler_age_s > 30). NOT benched (no hardware): the master-mode answer to a real thermostat while a real boiler is offline (AC1 path), field only. All ACs ticked; Done.
<!-- SECTION:NOTES:END -->
