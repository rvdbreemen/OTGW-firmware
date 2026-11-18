---
id: TASK-1470
title: >-
  Local diagnostic recording with web UI download and host replay (step 1 of
  TASK-1469, no cloud)
status: To Do
assignee:
  - '@claude'
created_date: '2026-10-29T09:03:00Z'
updated_date: '2026-11-18T12:13:00Z'
labels:
  - sat
  - diagnostics
  - replay
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 630000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer decision 2026-10-29 on TASK-1469 (gruijter's proposal): first a LOCAL recording, cloud later. Record the same event stream as the proposal (header with version/board/redacted settings, OT frames T/R/B/A with time delta, external inputs: MQTT/REST commands, weather, BLE readings, setting changes; PIC commands and responses; MQTT publishes at sendMQTTData; reboot/crash), compact binary with sequence numbers, and offer it as a download in the web UI (no cloud, no account, no outbound traffic). Budget per board (PSRAM boards larger buffer, no-PSRAM boards small ring with visible gaps); flash headroom ~196 KB. Replay: extend the OT replay (TASK-1294) with timestamp pacing, input injection and host streaming, plus a host diff tool on PIC commands and MQTT publishes. Before building: measure RAM headroom on a no-PSRAM board with the web UI open, record rate and size over 24 h, flash cost; respect the 2-concurrent-API rule for the download. Needs a Proposed ADR (new data path + replay format).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Proposed ADR (adr-kit) for the recording format, buffer budget per board and the download path
- [ ] #2 Measurements before building: RAM headroom (no-PSRAM board, web UI open), record rate and size over 24 h, flash cost; recorded in the notes
- [ ] #3 Bench demo on .63: record about 30 min under an OT replay, replay the same frames on build N and on build N+1 that changes one PIC command or MQTT publish, take a recording of each; scripts/diag_decode.py diff N vs N+1 shows the changed lines and exits 1, N vs N exits 0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Constraint from martreides (#alpha-testing msg 1565312598595993792, 2026-10-29): units may run in a restricted VLAN with all outbound internet blocked. Recording must work fully local with no cloud dependency; a later cloud upload must fail quietly (buffer/drop, no stall, no reboot). See TASK-1471 for the existing outbound paths.

Robert 2026-10-29: local first; cloud upload parked as a later feature (task-1473). Captures and recordings stay on the user's own network.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no on-device recorder; host capture exists. Needs ADR + budget decision.

2026-11-04 Robert (interactive): moved to milestone 2.1.0, out of the 2.0.0 scope.

2026-11-05 Robert's decisions on the ADR-213 open questions (AskUserQuestion in the lead session): (1) no-PSRAM boards: a 4 KB ring, allocated only while a recording runs (against the recommendation of no ring); (2) PSRAM ring 1.5 MB (~13.5 h; against the 1 MB recommendation); (3) download OPEN, not admin-protected (against the recommendation); (4) keep repeat records (~2.8 MB/day); (5) after a reboot a running recording resumes as a new recording starting with the reboot/crash record; (6) port 25238 bridge bytes not recorded in version 1. Measurements: docs/diagnostics/recording-budget-1470.md (wt-1470). AC2 still lacks the .61 heap read with UI open + BLE on.

2026-11-05: ADR-213 Accepted by Robert (AskUserQuestion, 'Accepteren, dan bouwen'); acronyms expanded at first use for the strict lint. Implementation split: TASK-1665 recorder core, TASK-1666 download/status routes, TASK-1667 host decoder + replay + UI button. AC2 still lacks the .61 heap read with UI open + BLE on (TASK-1665 AC2 covers the real cost).

Stale audit 2026-11-14 (coordinator decision): back to To Do until its children close (TASK-1665 recorder core, TASK-1666 routes, TASK-1667 decoder/replay/UI).

Triage 2026-11-18 (otgw-webui, read-only, origin/dev 90343abe1). All three children are Done: TASK-1665 recorder core (AC2 bench: .61 no PSRAM 688 appends avg 79 us max 247 us, internal heap -5.4 KB while recording; .63 PSRAM ring 384 x 4 KB = 1.5 MB, internal heap unchanged), TASK-1666 download/status routes, TASK-1667 host decoder + replay (bench .63: 8 min recording, 882 records, 0 dropped, OT frames replayed via POST /api/v2/simulate, 153/153 frames in order). The web UI button came with TASK-1700 (3a36e480a): v2.html:850-852 diagRecStart/Stop/Download, v2.js:6083-6096. Decoder tests: python -m pytest tests/test_diag_decode.py 10 passed (2026-11-18).

AC mapping. AC1 (Proposed ADR): met and exceeded: ADR-213 Accepted 2026-11-05 (docs/adr/ADR-213-*.md:4,45). AC2 (measurements before building): met: docs/diagnostics/recording-budget-1470.md (17,000 events/h, 2.79 MB/day, 140 KiB image headroom, LittleFS cannot hold a day) plus the real cost from TASK-1665 AC2. The gap named on 2026-11-05 (.61 heap with web UI open and BLE on) is not in either record; the TASK-1665 run does not say BLE was on. AC3 (a recorded day replays and the diff tool shows PIC-command/MQTT-publish differences between two firmware builds, old vs new): partly met. The diff exists (scripts/diag_decode.py:318-338, diff subcommand, exit 1 on a difference) and is host-tested only against a mutated copy of one recording (tests/test_diag_decode.py:165-177). Not done: a demonstration between two real builds. A recorded day is not possible by design: ADR-213:155 gives about 13.5 h on PSRAM and a 4 KB ring without PSRAM, so the title's 24 h is out of date.

Proposal (for the coordinator): smaller scope, then Done. (1) Tick AC1 and AC2 on the evidence above; for the BLE-on heap gap either accept the TASK-1665 numbers (recommended: BLE has its own no-PSRAM budget rule in ADR-222) or ask 09 for one /api/v2/device/info read on .61 with BLE on and the UI open while a recording runs. (2) Rewrite AC3 as a bench demo: record about 30 min on .63 under a replay, replay the same OT frames on build N and on build N+1 that changes a PIC command or a publish, and run diag_decode.py diff on the two recordings: it must show the changed lines and exit 1 (a same-build pair exits 0). (3) Retitle without 24 h (13.5 h PSRAM ring, ADR-213). Nothing here needs firmware code; AC3 needs a bench slot.

2026-11-18 AC correction (coordinator approved, milestone stays 2.1.0; a correction of an AC impossible by design, not a scope change). Old AC3 asked for 'a recorded day' to replay; ADR-213:155 sizes the PSRAM ring at '1.5 MB | ... | about 13.5 h, about 18 h without repeats' and boards without PSRAM get a 4 KB ring (ADR-213:158), so a 24 h recording cannot exist. AC3 is rewritten as a provable bench demo of the diff between two builds (the old AC's real goal), and '24 h' is dropped from the title. AC1 ticked: ADR-213 status Accepted, 2026-11-05 (docs/adr/ADR-213-*.md:4 and :45, read 2026-11-18). AC2 stays open: one /api/v2/device/info read on .61 (no PSRAM) with BLE on, the web UI open and a recording running is requested from 09 through the coordinator.
<!-- SECTION:NOTES:END -->
