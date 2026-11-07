---
id: TASK-1685
title: >-
  Remove the 26 dead firmware functions found by TASK-1683 (keep the Platform
  shims)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T09:28:00Z'
updated_date: '2026-11-07T10:35:00Z'
labels:
  - cleanup
  - firmware
dependencies: []
priority: low
ordinal: 806000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 chose: remove the dead firmware functions, keep platformWiFiClientSetSync and platformTaskExit (Platform API). List and evidence in TASK-1683 notes: 23 zero-use functions (all absent from the beta.119 elf) plus 3 transitive (formatBytes, restSendP, restFlushContent). Also drop the lifted bleMatchesConfiguredMAC copy and its cases from tests/test_ble_parsers.cpp. Start only after the coordinator's 'beta.118 tag out' (build freeze).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 26 functions and their prototypes are removed; platformWiFiClientSetSync and platformTaskExit stay
- [x] #2 The lifted bleMatchesConfiguredMAC copy in tests/test_ble_parsers.cpp is removed; python scripts/run_tests.py passes
- [ ] #3 esp32-combo build succeeds with its own prerelease bump; the app image is the same size or smaller than beta.119's
- [ ] #4 evaluate.py --quick shows no new FAIL; an OTA bench flash boots and serves the web UI and MQTT
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Work in own lane wt-1685 (branch lane-1685 from origin/dev 80b304f; the main dev tree had another session's build churn). Removed 23 zero-use + 3 transitive functions, their prototypes, the empty MQTTHA_PGM_HELPERS_DEFINED guard and five comments that named removed code (sendMQTTData now named as the heap-gate owner, restFinalize as the stream sender). Platform shims kept. tests/test_ble_parsers.cpp: lifted bleMatchesConfiguredMAC, its strcasecmp shim, the g_sBleMAC mock, cases 11-12 and the now-unused <cctype> removed. Net: 12 firmware files + 1 test, about 300 lines removed. Prerelease bump beta.119 -> beta.120.

Removal-script bug caught in review: the first brace counter counted braces inside string literals and swallowed the live sendJsonMapEntry overloads with sendStartJsonMap; reverted, fixed (string/comment aware, prints every span), re-run, diff reviewed hunk by hunk (logged as bug in .wolf/buglog.json). No reference to any removed name is left in src/ or tests/ (git grep; only the separate JS formatBytes in HTML pages and a past-tense comment in restAPI.ino).

Evidence: evaluate.py --quick 84 passed, 0 warn, 0 fail. scripts/run_tests.py -j4: 170 files, 159 passed, 8 failed, 2 timeouts. The 5 genuine failures (test_adr_governance ADR-216 index, test_sat_corrections_index, test_static_integrity CLI, test_boiler_unsupported_origin, test_local_frame_consumers) fail identically on clean origin/dev (stash run: 0/5 passed); 3 more are environment (WinError 225 Defender on test_otd_status_relay / test_sat_requested_vs_transmitted, no host C++ compiler for test_web_file_scheduling). Timeouts rerun alone: test_dhw_water_meter PASS (670 s); test_sat_lowload_contract LL-021 error + LL-023 fail, baseline run on clean origin/dev in progress.

test_sat_lowload_contract baseline on a clean origin/dev worktree: LL-010 fail, LL-021 error, LL-023 fail (624 s). Lane: LL-021 error, LL-023 fail, LL-010 pass. No new failure from the removal. Remaining: AC3 build + image size vs beta.119 and AC4 OTA bench, both after the coordinator's 'beta.118 tag out'; then rebase onto origin/dev and push via the lead.
<!-- SECTION:NOTES:END -->
