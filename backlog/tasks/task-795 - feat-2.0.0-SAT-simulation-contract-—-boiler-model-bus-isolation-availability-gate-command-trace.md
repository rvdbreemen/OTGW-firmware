---
id: TASK-795
title: >-
  feat-2.0.0: SAT simulation contract — boiler model, bus isolation,
  availability gate, command trace
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 22:52'
updated_date: '2026-05-31 23:12'
labels:
  - sat
  - simulation
  - safety
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implements plan docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md (sections 4-7). Adds a minimal synthetic boiler model (flame state machine, modulation, flow/return temp) so SAT cycle classifier, flame health, 4h window stats, heating-curve recommendation and OPV calibration execute under simulation. Wraps SAT consumers in getters (satGetFlowTemp/satGetReturnTemp/satIsFlameOn/satGetActualModulation) routed via settings.sat.bSimulation. Enforces a tri-invariant simulation contract: (1) bus isolation - no boiler-side bytes leave the device on any of the 3 actuation paths (PIC serial, OTDirect master TX, OTGW32 translation/pass-through) while sim active; (2) boiler-absence availability gate - edge-triggered forced-disable within <=1s of the first slave frame, REST 409 / MQTT reject / Web UI hide when a boiler is present; (3) visible command trace - every would-be command surfaced on telnet, MQTT (sat/sim/last_cmd non-retained) and REST (last_blocked_cmd + age). New ADR-117 documents the contract. Three-commit split per plan section 14: (1) wrappers+migration, (2) boiler model, (3) contract. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satIsFlameOn / satGetFlowTemp / satGetReturnTemp / satGetActualModulation wrappers exist and route via settings.sat.bSimulation
- [ ] #2 All 19 call sites in plan section 7 migrated to wrappers
- [ ] #3 Synthetic flame edges drive satCycleOnFlameChange and increment iCycleCount; eLastCycleClass reaches non-NONE value
- [ ] #4 iSimModulation varies between minMod and iMaxRelModulation under varying PID error
- [ ] #5 fSimReturnTemp = fSimFlowTemp minus delta(mod), floored at fSimRoomTemp
- [ ] #6 All three boiler-side actuation paths gated when bSimulation OR bOTGWSimulation; thermostat-side replies permitted; verified on both HAS_PIC and OTGW32 builds
- [ ] #7 Command trace appears on telnet (SAT-SIM trace:), MQTT (sat/sim/last_cmd non-retained), REST (last_blocked_cmd + age); covers paths a, b, c
- [ ] #8 Edge-triggered auto-disable: satOnBoilerDetected fires within <=1s of first slave frame; full teardown (settings flipped+persisted, synthetic reset, trace cleared, MQTT OFF, telnet event); survives reboot; verified on both HAS_PIC and OTGW32
- [ ] #9 REST PATCH bSimulation:true while boiler present returns HTTP 409 with documented body
- [ ] #10 MQTT sat/simulation/set ON while boiler present rejected with telnet warning; state topic does not flip
- [ ] #11 Web UI: simulation card + diagnostic block hidden when sim_available=false; visible when true
- [ ] #12 Standalone bench (no thermostat, no boiler): full SAT loop runs; AC 3-5, 7 all observable
- [ ] #13 python build.py exits 0 for both HAS_PIC and OTGW32 targets; python evaluate.py --quick clean
- [ ] #14 Real-boiler regression (bSimulation=false): SAT OT command stream byte-identical to pre-change baseline over 10 min
- [ ] #15 ADR-117 (SAT simulation contract) authored and Accepted before merge; Enforcement block included
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Three-commit split (plan docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md):
1. Pre-flight + reading pass (plan section 13): confirm branch/clean tree/build baseline; read SATtypes.h, SATcontrol.ino, SATcycles.ino, OTGW-Core.ino, OTDirect.ino, restAPI.ino, MQTTstuff.ino, data/index.{html,js}, docs/adr/.
2. Commit 1 (section 14.1): add 4 wrappers (satGetFlowTemp/satGetReturnTemp/satIsFlameOn/satGetActualModulation) after satGetOutsideTemp; migrate 19 read sites (section 7 grep table) to wrappers. Pure pass-through while bSimulation=false. Build+eval green.
3. Commit 2 (section 14.2): add 9 SATRuntimeSection fields (section 6); tuning consts; satSimMinMod(); extend satUpdateSimulation() with flame SM + modulation + flow + return + room models (section 5.2-5.6). Build+eval green.
4. ADR-117 (section 15): author via /adr-kit:adr, grounded in 2.0.0 artifacts, Enforcement block (forbid direct Tboiler/Tret/Statusflags&0x08 reads in SAT files). Proposed; maintainer accepts before merge.
5. Commit 3 (section 14.3): satSimulationBlocksBusTx() gate helper; gate paths a (OTGW-Core PIC), b (OTDirect master TX + probeOTBus synthetic-online), c (OTGW32 translation/pass-through - MAP during reading pass, may converge with b); slave-RX edge hook -> bBoilerDetectedFlag -> satOnBoilerDetected() deferred to main loop; satBoilerHardwarePresent() (dual-signal split vs boilerOnline); runtime backstop; REST 409 + sim_available/last_blocked_cmd JSON; MQTT reject + sat/sim/last_cmd publish; MQTTHaDiscovery sat_last_blocked_cmd; Web UI data-sim-only hide + trace render. Build+eval green.
6. Per-commit: python build.py (full, grep per-env SUCCESS), evaluate.py --quick, prerelease bump, append-notes. Push after commit 3 + code ACs green (standing permission). Field-validation ACs (6,7,8,9,10,11,12,14) need maintainer beta rig - left In Progress.

PITFALLS (section 17): writeSettings heavy -> deferred-flag pattern; bOTGWSimulation orthogonal, keep toggleable; probeOTBus synthetic-online must NOT feed satBoilerHardwarePresent (split-signal); path c localisation riskiest - if a 3rd path found, STOP and ask; PROGMEM F()/PSTR()/snprintf_P; JS guard DOM+try/catch; MQTT shadow publish-on-change; REST 409 match sendApiError shape; keep 3 commits separate.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T01:07:26+02:00: Pre-flight done. Plan on origin (184d097c), tracked. Task scaffolded: 15 ACs + 3-commit plan. Follow-ups F1-F7 = TASK-796..802. Existing getter pattern confirmed in SATcontrol.ino ~960 (satGetRoomTemp/satGetOutsideTemp gate on settings.sat.bSimulation) — the 4 new wrappers (satGetFlowTemp/satGetReturnTemp/satIsFlameOn/satGetActualModulation) follow this shape exactly. COMMIT-1 NOT STARTED: file-read tooling was returning inconsistent output for SATcontrol.ino this session (one Read returned placeholder garbage, a re-read returned clean code). Per plan §17 pitfall #4 + project memory, the 19-site mechanical migration must be done with verified reads only (same byte-order-bug risk class as TASK-747). Deferred to a stable-tooling session rather than risk a botched multi-site edit while unattended. 593 (the in-flight SAT work that shared SATcontrol.ino/SATtypes.h) is committed a3aa6672 + pushed, so the tree is clean and ready for commit-1 to start fresh.

2026-06-01T01:12:03+02:00: COMMIT-1 RECON COMPLETE, execution deferred to stable-tooling session (interactive file-Read was fabricating code lines this session — caught it inventing 'return state.sat.fOutdoorTemp; // wait, need to see actual end' at SATcontrol.ino:1061; git show proves real line is the weather-forecast block. Editing 28 sites + inserting wrappers through a Read layer that injects garbage = the TASK-747 byte-order-bug risk class, unsafe unattended). RELIABLE GROUND TRUTH (grep -n + git show, post-593-commit a3aa6672):

WRAPPERS to add after satGetOutsideTemp() (ends ~line 1066, before next function): satGetFlowTemp()->Tboiler, satGetReturnTemp()->Tret, satIsFlameOn()->(Statusflags&0x08)!=0, satGetActualModulation()->RelModLevel. NOTE: real OT field is OTcurrentSystemState.RelModLevel (float), NOT the plan's 'RelativeModulationLevel'. Wrapper real-path: return (uint8_t)OTcurrentSystemState.RelModLevel.

PLAN INCONSISTENCY to resolve: commit-1 wrappers reference state.sat.fSimReturnTemp/bSimFlameOn/iSimModulation which do NOT exist in SATtypes.h yet (only fSimRoomTemp/fSimFlowTemp/fSimOutdoorTemp exist, lines 286-288). Plan puts these fields in commit-2 (§6), but then commit-1 cannot build. FIX: add bSimFlameOn(bool=false), iSimModulation(uint8_t=0), fSimReturnTemp(float=20.0f) in commit-1 too (inert, default-init) so commit-1 is self-building; commit-2 then adds only the remaining fields (iSimFlameOnSinceMs, iSimFlameOffSinceMs, sLastBlockedCmd[24], iLastBlockedCmdMs).

MIGRATION SITES (28 total, all confirmed READS, ZERO LHS writes -> safe for atomic sed):
- Tboiler (15): SATcontrol.ino 333,484,669,768,944,2424,2579,2678,3868 ; SATcycles.ino 515,516,526,537,613,687
- Tret (3): SATcontrol.ino 755,2679 ; SATcycles.ino 620
- Statusflags&0x08 flame (10): SATcontrol.ino 334,482,668,939,2463,2646,3010,3201,3672,4024
- RelModLevel (1): SATcontrol.ino 483
Suggested atomic approach (operates on disk, not via Read): sed -i 's/OTcurrentSystemState\.Tboiler/satGetFlowTemp()/g; s/OTcurrentSystemState\.Tret/satGetReturnTemp()/g; s/(OTcurrentSystemState\.Statusflags & 0x08) != 0/satIsFlameOn()/g' on both SAT files; targeted edit for line 483 RelModLevel->satGetActualModulation(). Then verify via git diff + build BOTH targets (esp8266+esp32, grep per-env SUCCESS) + evaluate --quick. Arduino auto-prototypes static fns so call-before-define (line 333 vs def ~1066) is fine (satGetOutsideTemp already does this).

STATUS: 593 (shared SATcontrol/SATtypes) committed+pushed a3aa6672, tree clean. F1-F7=TASK-796..802 created. Ready for a clean-tooling session to run commit-1 fast.
<!-- SECTION:NOTES:END -->
