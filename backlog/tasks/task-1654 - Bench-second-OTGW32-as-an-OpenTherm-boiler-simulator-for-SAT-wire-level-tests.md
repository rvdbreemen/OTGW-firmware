---
id: TASK-1654
title: 'Bench: second OTGW32 as an OpenTherm boiler simulator for SAT wire-level tests'
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-05T07:00:00Z'
updated_date: '2026-11-05T17:10:00Z'
labels:
  - sat
  - bench
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 778000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 (TASK-1412.20 R-1): a second OTGW32 runs the vendored OpenTherm library in slave mode as a programmable boiler, wired to the bench unit's boiler side, so ID25/ID28 Data-Invalid, Unknown-DataID, no-answer, ignition delays and DHW can be driven on the wire (replay cannot reach the flow/return feeds).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Simulator firmware or sketch answers MsgID 0/1/3/17/25/28 with scripted values and faults, driven by a scenario file
- [ ] #2 Bench unit sees the simulator as a boiler on the wire (B frames from the bus, not replay), shown in a capture
- [ ] #3 TASK-1412.20 SB05-SB09 and SB11 run on the wire with the expected SAT reactions recorded
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 (6c, worktree wt-6c-1654 on origin/dev c0d471ebf): AC1 host side built under tools/bench/ot-boiler-sim/ (separate PlatformIO project, no src/ change). Engine src/boiler_sim.cpp is pure C++ and reacts to the master: ID0 echoes master flags; CH active follows CH enable; flame needs heat demand (CH + written TSet >= demand_min 20 C) and lights ignition_delay s later, because SAT's PWM OFF keeps CH=1 and sends CS=10 (SATcontrol.ino:1601). Per-ID Data-Invalid/Unknown/no-answer, silence, fault, DHW draw come from .scn files; export_scenarios.py writes SB05-SB09 and SB11 from scripts/sat_bench_scenarios_1412_20.py (generator unchanged, 1412.20-v1, sha256 per file in data/index.txt); SB11 wire variant ignition_delay=120. Other IDs the bench unit polls (OTDirect.ino otSchedule[]) are answered, so no unscripted Unknown-DataID confounds a run. Evidence: python test/host/test_ot_boiler_sim.py -> 17/17 PASS (MSVC; frames encoded by the generator's frame()); mutants red: Data-Invalid ignored, ID0 HB not echoed, ignition delay ignored, no-answer ignored, demand ignored. EOL bug found and fixed (.gitattributes eol=lf; --check stale -> up to date). evaluate.py --quick 83 passed / 0 failed. NOT verified: src/main.cpp sketch not compiled (pio run would bypass the build lock, TASK-1488; question to 6b), nothing ran on a board; AC1 left unchecked. Classic PIC: whether it originates requests without a thermostat is not checked. Commits 6b6f163bc, 19fae5d2a, 133b8072b (local, handed to 6b).

Full sweep scripts/run_tests.py in wt-6c-1654: 153 passed / 7 failed. Fixed: tests/test_sat_kd_warmup_sim_1655.py (TASK-1656 moved the port to 0.063; commit 09d9ce606). Also failing on clean c0d471ebf: test_boiler_unsupported_origin, test_local_frame_consumers, test_ot_support_migration. Not re-run clean: test_capture_tool, test_refresh_storm, test_sat_lowload_contract (diff adds files only). test/host/test_ot_boiler_sim.py PASS in the sweep. Asked 6b about compiling via pio run (build lock).

2026-11-05: pushed to origin/dev e5578aa52..29fbac698 (6b's go), 4 commits rebased (simulator, EOL attributes, heat-demand flame, TASK-1655 baseline fix). Push gate slip: my chained command piped the host test into tail, so a FAIL (committed_scn_files_match_export) did not stop the push. Cause checked: working copy only. git ls-files --eol shows i/lf w/crlf attr eol=lf; the rebase replayed commit 1 before .gitattributes existed. After deleting and re-checking out data/: --check 'up to date', test/host/test_ot_boiler_sim.py exit 0. The pushed blobs are correct. AC1 firmware-compile half stays open: compiling tools/bench/ot-boiler-sim needs a lock-aware build.py hook for tools/bench or Robert's OK (6b raises it with Robert; 6b declined a direct pio run, TASK-1488). AC2/AC3 need a second OTGW32, which does not exist yet.

Robert 2026-11-05 (session): parked to 2.1.0: the wire-level bench (second OTGW32 as boiler simulator) is not a 2.0.0 condition; replay plus field validation suffice for now. Delivered so far stays (1412.20 AC3 procedure + fixtures; 1654 host-side simulator code).

2026-11-05 ~18:30 6c: AC1 compile half done, with Robert's one-time OK for a direct pio run of tools/bench/ot-boiler-sim. The lock was kept: the wrapper took build.py acquire_build_lock() first, waited for the wt-1451 build, then checked commit charge (81.0% < 85%). Built with pio run -j 4, PLATFORMIO_CORE_DIR D:/DevData/platformio, worktree wt-6c-1654 at 29fbac698. Result: [SUCCESS] Took 117.17 s, exit 0. RAM 10.3% (33744 / 327680 B), Flash 29.1% (381715 / 1310720 B). One warning, in the vendored library and not in our code: OpenTherm.cpp:174 -Wvolatile. Lock released, worktree clean. Not verified: the answers on the wire; that is AC2. Classic+S3 as the sim board was researched and rejected: the ESP has only the PIC UART (boards.h:165-171, HAS_DIRECT_OT 0), gateway PIC firmware can only override responses per ID (SR=), and interface.hex would need a PIC flash, which is forbidden. AC2/AC3 need a second OTGW32. Status and milestone stay To Do / 2.1.0, as Robert set them (via 6b).
<!-- SECTION:NOTES:END -->
