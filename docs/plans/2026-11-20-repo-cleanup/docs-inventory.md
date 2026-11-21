# docs/** and root *.md inventory for the repo cleanup (phase 1, read-only)

otgw-docs (95), 2026-11-20, on origin/dev 55c4146b7. Nothing was moved. Execution is phase 2, after the 20:00 beta, in small commits.

**How the columns were made**
- *last change*: `git log -1 --format=%cs` per file.
- *on otgw-1.x.x*: the same path exists in `origin/otgw-1.x.x`.
- *inbound links*: tracked text files outside the file itself that name its file name, counted per source class (README, release-notes, ADR, agent-rules/skills, backlog, docs, code/tests, other). A file name shared by several files (README.md, 00-scope.md) only counts when the reference also names its parent folder. This is a name search, not a link checker: it over-counts common names and misses links written without the file name.
- *HARD*: absolute `github.com/rvdbreemen/OTGW-firmware/blob|tree|raw/...` URLs to the file, in the body of a published GitHub release (`gh release view <tag> --json body`, 94 releases) or in a docs/releases notes file. A file with a HARD mark stays at its path: a published release body cannot follow a move.
- *purpose / current / 1.x-only / duplicate / target*: my judgment from the file heading, the folder and the links; rows marked **decide** need Robert or the owner.

**Fixed rules** (architect and project rules): never move `docs/adr/**`, `docs/sat/decisions/sergeant-d-corrections.md`, `.index.json`, `docs/sat/decisions/open-questions.md`, `docs/BREAKING_CHANGES.md`, `docs/upgrade-from-*.md`, `docs/guides/FLASH_GUIDE.md` (evaluate.py:2877 checks it).

**Rows by proposed target**: decide 920, keep 699, archive 35, move 30, delete-with-archive 1 (files).

## Summary for the plan

**Must stay at their path (HARD, `blob/dev` links in published releases or notes, 19 targets):** CONTRIBUTORS.md, docs/BREAKING_CHANGES.md, docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md, docs/guides/WIFI_FIRMWARE_UPDATE.md, docs/sat/field-run-recipe-2.0.0.md, and the release notes of beta.62, 77, 105, 118, 130, 144, 158, 162, 167, 170, 174, 203, 218 and 250 in docs/releases/. The architect counted 23; my count includes the docs/releases and docs/releases/archive files plus all 94 published GitHub release bodies, `blob/dev` only (`blob/main` points at the 1.x line and does not limit moves on dev; those are listed per row as "blob/main xN"). The difference of 4 is worth a cross-check.

**Clear moves (low risk, phase 2):**
1. `docs/plans/*` (8) and root `plan/feature-openweathermap-onboarding-1.md` -> `docs/plans/` (one plans folder). `docs/process/ps1-lean-translator-refactor-plan.md` -> `docs/plans/`.
2. `docs/archive/*` -> `docs/archive/1.x/` (one archive root); fix the merge-conflict markers in `docs/archive/mqttha.cfg` first.
3. `docs/images/` (21, 2021 screenshots of the 1.x UI, 20 without inbound link, all on otgw-1.x.x) -> `docs/archive/1.x/images/`.
4. `docs/fixes/` (1.x fix write-ups) -> `docs/archive/1.x/fixes/`; README.md:319 and CHANGELOG.md:1051 link `opentherm-v42-mqtt-breaking-changes.md`: update those links in the same commit (CHANGELOG history is not edited: leave a stub instead).
5. `docs/features/TEMPERATURE_SENSOR_*` (classic UI) -> `docs/archive/1.x/features/`.
6. `docs/api/MQTT-message-id-echo-audit.md` -> `docs/audits/`. `docs/research/deep-research-report_arduino_core_3.1.2...` stays: ADR-082 cites it and ADRs are immutable.
7. `docs/daily-issue-report.md`: tracked although ignored (.gitignore:95): `git rm --cached` plus archive copy.

**Need a decision (Robert or owner):**
- SAT simulation output: `docs/sat/sim-hp-lowload/` (762 files, 712 without inbound link), `sim-1519/` (106), `sim-1412-19/` (34): keep, zip into one archive file, or move to the field-data archive outside the repo. These are 900 of the 1685 files.
- `docs/features/` (non-classic pages: Dallas, webhook, gateway mode, data persistence, SAT): the 2.x manual covers the same subjects; archive after a per-file check, or keep. `docs/features/smart-autotune-thermostat.md` is SAT text outside the ground-truth docs (drift risk).
- `docs/design/` (mockups, 6 of 8 without inbound link), `docs/superpowers/` (skill output path), `docs/diagnostics/` vs `docs/research/`, `docs/beta/` checklist, `docs/templates/`, `docs/wiki-drafts/` (publish to the wiki or fold into manual ch07), `docs/sat/SAT_thermo_nova_Documentation.docx`, `docs/process/branch-hygiene-queue.csv`, `docs/guides/ESP_ABSTRACTION.md` (still ESP8266/ESP32 dual).
- Code inside docs/: `docs/opentherm specification/member-ids/opentherm_public_crawler.py` and `docs/sat/research/heat-loss-id/heat_loss_id.py` -> tools/ or scripts/ (owner otgw-dev).
- Folder name with a space: `docs/opentherm specification/` (75 name references): rename only with all links in one commit, or leave.

**Findings (verified by me, from the architect's handover and this run):**
- `docs/archive/mqttha.cfg` lines 4-8: committed merge-conflict markers.
- `docs/evidence/task1719_61_otd_control_beta145_settings_rail.png` and `task1719_63_before_beta144_settings_rail.png` are byte-identical (same sha256): evidence question for the TASK-1719 owner.
- `docs/daily-issue-report.md` is tracked and ignored at the same time.
- docs/MANUAL.md (generated) names `tools/generate_mqttha_data.py` and the archived mqttha.cfg; `docs/api/WEBSOCKET_FLOW.md` points at a review folder that moved to docs/reviews/archive/2026-Q1 (architect); docs/adr/README.md says `.github/skills/adr` does not exist, it does (architect).

**Keep without discussion:** docs/adr/**, docs/sat ground-truth files, docs/manuals, docs/MANUAL.md, docs/c4, docs/api (except the audit), docs/releases (+archive), docs/reviews (published and ADR links), docs/BREAKING_CHANGES.md, docs/upgrade-from-0.x.md, docs/guides/FLASH_GUIDE.md and BUILD.md (evaluate.py:2877).

## (root)

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `AGENTS.md` | Agent rules (Codex) | yes | no | yes | 2026-11-15 | AGENT-CORE block shared with CLAUDE.md | 40 (backlog 24, docs 5, agent-rules/skills 4, code/tests 4, ADR 3) |  | keep | read by tests/test_adr_governance.py |
| `CHANGELOG.md` | Changelog of both lines | yes | mixed | yes | 2026-11-20 |  | 33 (backlog 11, code/tests 9, docs 5, other 4, agent-rules/skills 3, ADR 1) | blob/main x16 (1.x) | keep | history, not edited |
| `CLAUDE.md` | Agent rules (Claude) | yes | no | yes | 2026-11-15 | AGENT-CORE block shared with AGENTS.md | 166 (backlog 86, docs 32, ADR 14, code/tests 13, agent-rules/skills 12, other 9) |  | keep | read by tests/test_adr_governance.py |
| `CONTRIBUTORS.md` | Contributors list (TASK-2947) | yes | no | no | 2026-11-20 |  | 4 (agent-rules/skills 1, backlog 1, docs 1, release-notes 1) | notes:RELEASE_NOTES_2.0.0-draft.md | keep | updated every public stable release |
| `README.md` | Project README, landing page on GitHub | yes | no | yes | 2026-11-20 |  | 0 | blob/main x32 (1.x) | keep | linked from published release notes |

## docs/(top)

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/BREAKING_CHANGES.md` | Breaking changes log per core version | yes | mixed | yes | 2026-11-19 |  | 50 (release-notes 21, backlog 15, docs 9, agent-rules/skills 3, other 2) | gh:v2.0.0-beta.162, gh:v2.0.0-beta.167, gh:v2.0.0-beta.170, gh:v2.0.0-beta.174 +33 | keep | never moves (update-docs-2x rule) |
| `docs/MANUAL.md` | Single-file manual, generated from docs/manuals/en | yes | no | no | 2026-11-20 | docs/manuals/en/* (generated) | 62 (backlog 47, code/tests 6, docs 4, other 3, agent-rules/skills 1, ADR 1) |  | keep | gated by tests/test_manual_generated.py FINDING: generated text names tools/generate_mqttha_data.py and the archived mqttha.cfg; fix in the EN chapters, then regenerate (never edit MANUAL.md). |
| `docs/daily-issue-report.md` | One daily issue report of 2026-07-11 | historical | yes | yes | 2026-07-11 |  | 1 (backlog 1) |  | archive -> docs/archive/1.x/ | one-off output of the issue sweep FINDING: tracked although .gitignore:95 ignores it (verified with check-ignore --no-index). |
| `docs/upgrade-from-0.x.md` | Upgrade guide 0.9.x/0.10.y to 1.3.x | historical | yes | no | 2026-04-06 |  | 2 (README 1, backlog 1) |  | keep | never moves (update-docs-2x rule: docs/upgrade-from-*.md); 1.x users still land here |

## docs/adr/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/adr/` (273 files) | Architecture Decision Records + index/README + adr-kit files | yes | mixed | 66/273 | 2026-01-28..2026-11-20 |  | 792 total, 3 files with none |  | keep | never move: adr-kit, evaluate.py and tests read this folder |

## docs/api/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/api/DALLAS_SENSOR_LABELS_API.md` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-03-28 |  | 8 (agent-rules/skills 2, backlog 2, README 1, release-notes 1, docs 1, code/tests 1) |  | keep |  |
| `docs/api/MQTT-message-id-echo-audit.md` | Audit of MQTT MsgID echo behaviour (v4.2) | historical | no | yes | 2026-05-07 |  | 7 (backlog 2, code/tests 2, other 1, ADR 1, docs 1) |  | move -> docs/audits/ | an audit, not a reference |
| `docs/api/MQTT.md` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-11-20 |  | 140 (backlog 96, docs 20, other 8, ADR 8, agent-rules/skills 4, code/tests 3, README 1) |  | keep |  |
| `docs/api/MQTT_DISCOVERY_CHECK.md` | Regression check procedure for HA discovery | yes | no | no | 2026-04-19 |  | 3 (backlog 1, ADR 1, docs 1) |  | keep |  |
| `docs/api/README.md` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-11-20 |  | 46 (backlog 25, docs 7, release-notes 5, ADR 4, code/tests 2, agent-rules/skills 1, other 1, README 1) | blob/main x4 (1.x) | keep |  |
| `docs/api/WEBSOCKET_FLOW.md` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-10-15 |  | 12 (agent-rules/skills 4, docs 4, backlog 2, README 1, code/tests 1) |  | keep | FINDING (architect): lines 569-570 point at a review folder that moved to docs/reviews/archive/2026-Q1. |
| `docs/api/WEBSOCKET_QUICK_REFERENCE.md` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-10-15 |  | 2 (agent-rules/skills 1, code/tests 1) |  | keep |  |
| `docs/api/openapi-dallas-sensors.yaml` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-03-28 |  | 3 (agent-rules/skills 1, backlog 1, docs 1) |  | keep |  |
| `docs/api/openapi.yaml` | REST, MQTT and WebSocket reference for 2.x | yes | no | yes | 2026-11-20 |  | 78 (backlog 50, docs 11, ADR 6, code/tests 4, agent-rules/skills 2, other 2, release-notes 2, README 1) |  | keep |  |

## docs/archive/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/archive/MQTT_old.md` | Older archive: MQTT_old, release generation guide, mqttha generator, rc3-rc4 notes | historical | yes | yes | 2026-03-28 |  | 0 |  | move -> docs/archive/1.x/ | one archive root instead of two names; keep the subfolder names |
| `docs/archive/RELEASE_GENERATION_GUIDE.md` | Older archive: MQTT_old, release generation guide, mqttha generator, rc3-rc4 notes | historical | yes | yes | 2026-03-28 |  | 0 |  | move -> docs/archive/1.x/ | one archive root instead of two names; keep the subfolder names |
| `docs/archive/mqttha-generator/README.md` | Older archive: MQTT_old, release generation guide, mqttha generator, rc3-rc4 notes | historical | yes | yes | 2026-04-22 |  | 0 |  | move -> docs/archive/1.x/ | one archive root instead of two names; keep the subfolder names |
| `docs/archive/mqttha.cfg` | Older archive: MQTT_old, release generation guide, mqttha generator, rc3-rc4 notes | historical | yes | no | 2026-04-18 |  | 0 |  | move -> docs/archive/1.x/ | one archive root instead of two names; keep the subfolder names FINDING: committed merge-conflict markers at lines 4-8 (verified); docs/MANUAL.md and ch08 cite this path. Fix the markers in phase 2 before any move. |
| `docs/archive/rc3-rc4-transition/README.md` | Older archive: MQTT_old, release generation guide, mqttha generator, rc3-rc4 notes | historical | yes | yes | 2026-02-06 |  | 2 (docs 2) |  | move -> docs/archive/1.x/ | one archive root instead of two names; keep the subfolder names |

## docs/audits/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-05-21 |  | 2 (ADR 1, code/tests 1) |  | keep | dated, cited from tasks; fine as is |
| `docs/audits/2026-05-28-esp-abstraction-leak-audit.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-05-29 |  | 8 (backlog 3, agent-rules/skills 2, code/tests 2, docs 1) |  | keep | dated, cited from tasks; fine as is |
| `docs/audits/2026-06-13-adr-123-phase1-single-writer-map.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-06-13 |  | 2 (backlog 1, ADR 1) |  | keep | dated, cited from tasks; fine as is |
| `docs/audits/2026-06-26-v2-mockup-alignment-audit.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-06-27 |  | 2 (backlog 1, docs 1) |  | keep | dated, cited from tasks; fine as is |
| `docs/audits/2026-07-01-v2-design-update-audit.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-07-02 |  | 2 (backlog 2) |  | keep | dated, cited from tasks; fine as is |
| `docs/audits/2026-11-04-outbound-connections.md` | Dated audits (HA flags, ESP abstraction leaks, ADR-123 map, v2 mockup, outbound connections) | historical | no | no | 2026-11-04 |  | 2 (backlog 2) |  | keep | dated, cited from tasks; fine as is |

## docs/beta/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/beta/2.0.0-beta-hardware-validation-checklist.md` | 2.0.0 beta hardware validation checklist (NL) | stale | no | no | 2026-06-28 | docs/templates/hardware-verification-log.md (overlap) | 0 |  | archive -> docs/archive/2.0.0-beta/ | 0 inbound links; superseded by the bench-tester flow |

## docs/c4/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/c4/` (20 files) | C4 architecture docs (context, container, component, code) | yes | no | 0/20 | 2026-10-21..2026-11-20 |  | 209 total, 0 files with none |  | keep | updated in TASK-2950 |

## docs/design/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/design/` (8 files) | Web UI design concepts (PNG mockups, HTML concepts, v2 navigation concept) | historical | no | 0/8 | 2026-06-27..2026-10-23 |  | 12 total, 6 files with none |  | decide | 6 of 8 files have no inbound link; keep as design history or archive |

## docs/diagnostics/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/diagnostics/minimal-ot-log-and-upload-proposal.md` | Diagnostics research (recording budget, OT stream encoding, rekey on a starved heap) | yes | no | no | 2026-11-16 | docs/research/ (same genre) | 1 (backlog 1) |  | move -> docs/research/ | or keep; same kind of content as docs/research |
| `docs/diagnostics/ot-stream-encoding-study.md` | Diagnostics research (recording budget, OT stream encoding, rekey on a starved heap) | yes | no | no | 2026-11-14 | docs/research/ (same genre) | 5 (backlog 2, other 1, docs 1, code/tests 1) |  | move -> docs/research/ | or keep; same kind of content as docs/research |
| `docs/diagnostics/recording-budget-1470.md` | Diagnostics research (recording budget, OT stream encoding, rekey on a starved heap) | yes | no | no | 2026-11-05 | docs/research/ (same genre) | 6 (ADR 3, backlog 2, docs 1) |  | move -> docs/research/ | or keep; same kind of content as docs/research |
| `docs/diagnostics/task-2026-group-rekey-starved-heap.md` | Diagnostics research (recording budget, OT stream encoding, rekey on a starved heap) | yes | no | no | 2026-11-19 | docs/research/ (same genre) | 2 (backlog 2) |  | move -> docs/research/ | or keep; same kind of content as docs/research |

## docs/evidence/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/evidence/2026-07-06_session_verification_summary.md` | Session verification summary 2026-07-06 | historical | no | no | 2026-07-08 |  | 0 |  | archive -> docs/archive/2.0.0-dev/ |  |
| `docs/evidence/open_tasks_validation_report.html` | Open tasks validation report (HTML) | historical | no | no | 2026-07-06 |  | 0 |  | delete-with-archive | generated report, no inbound link |
| `docs/evidence/task-1310/` (5 files) | Bench/test evidence files per task | historical | no | 0/5 | 2026-10-23..2026-10-23 |  | 0 total, 5 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1311/` (2 files) | Bench/test evidence files per task | historical | no | 0/2 | 2026-10-23..2026-10-23 |  | 0 total, 2 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1312/` (5 files) | Bench/test evidence files per task | historical | no | 0/5 | 2026-10-23..2026-10-23 |  | 0 total, 5 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1322/` (4 files) | Bench/test evidence files per task | historical | no | 0/4 | 2026-10-23..2026-10-23 |  | 0 total, 4 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1326/` (3 files) | Bench/test evidence files per task | historical | no | 0/3 | 2026-10-23..2026-10-23 |  | 0 total, 3 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1358/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-01..2026-11-01 |  | 2 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1390-outside-missing-hint-alpha565.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-10-31..2026-10-31 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1520-bench-alpha568-boot-tc.txt/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-10-31..2026-10-31 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1520-gruijter-alpha567-follow-gap.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-10-31..2026-10-31 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task-1521-bench-alpha569.txt/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-10-31..2026-10-31 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1475/` (24 files) | Bench/test evidence files per task | historical | no | 0/24 | 2026-10-30..2026-10-30 |  | 19 total, 8 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1716_61_beta149_otdmode_selector.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_61_otd_control_beta145_device_info.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_61_otd_control_beta145_settings_rail.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_63_after_beta145_device_info.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_63_after_beta145_settings_rail.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_63_before_beta144_device_info.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task1719_63_before_beta144_settings_rail.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-11-08..2026-11-08 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task956_heap_soak_2h_verdict.md/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 2 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task959_update_page_firmware_hidden.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task972_01_before_flash.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task972_02_armed.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task972_03_flashing_started.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task972_04_flash_final.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task972_05_retry_final.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task978_static_asset_retry_verify.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task989_css_bundle_verify.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 1 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task994_ble_dram_hypotheses.md/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-10-03..2026-10-03 |  | 2 total, 0 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task996_01_ble_roster_before.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task996_02_ble_roster_after_save.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task_fontfix_clean.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task_fontfix_final.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task_fontfix_verify.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task_fontfix_verify2.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |
| `docs/evidence/task_fontfix_verify3.png/` (1 files) | Bench/test evidence files per task | historical | no | 0/1 | 2026-07-06..2026-07-06 |  | 0 total, 1 files with none |  | keep | evidence for Done tasks; task notes link some of them |

## docs/features/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/features/TEMPERATURE_SENSOR_DIAGRAM.md` | Diagram for the 1.x temperature sensor graph | historical | yes | yes | 2026-03-28 |  | 0 |  | archive -> docs/archive/1.x/features/ | classic UI |
| `docs/features/TEMPERATURE_SENSOR_FINAL_SUMMARY.md` | Implementation summary of the 1.x temperature graph | historical | yes | yes | 2026-03-28 |  | 1 (ADR 1) |  | archive -> docs/archive/1.x/features/ | classic UI |
| `docs/features/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md` | Implementation notes of the 1.x temperature graph | historical | yes | yes | 2026-03-28 |  | 5 (docs 3, ADR 1, release-notes 1) |  | archive -> docs/archive/1.x/features/ | classic UI |
| `docs/features/dallas-temperature-sensors.md` | Feature descriptions from the 1.x era (Dallas, webhook, gateway mode, data persistence, SAT, temperature graph) | stale | mixed | yes | 2026-02-15 | docs/manuals/en/ch03-ch05 (2.x manual covers these) | 4 (README 1, backlog 1, release-notes 1, docs 1) |  | decide | partly 1.x; compare per file with the manual before archiving |
| `docs/features/data-persistence.md` | Feature descriptions from the 1.x era (Dallas, webhook, gateway mode, data persistence, SAT, temperature graph) | stale | mixed | yes | 2026-01-28 | docs/manuals/en/ch03-ch05 (2.x manual covers these) | 1 (release-notes 1) |  | decide | partly 1.x; compare per file with the manual before archiving |
| `docs/features/gateway-mode-detection.md` | Feature descriptions from the 1.x era (Dallas, webhook, gateway mode, data persistence, SAT, temperature graph) | stale | mixed | yes | 2026-03-28 | docs/manuals/en/ch03-ch05 (2.x manual covers these) | 0 |  | decide | partly 1.x; compare per file with the manual before archiving |
| `docs/features/smart-autotune-thermostat.md` | Old SAT feature page | stale | no | no | 2026-10-24 | docs/manuals/en/ch05-sat-thermostat.md, docs/sat/firmware-sat-spec.md | 4 (backlog 2, release-notes 2) |  | decide | SAT text outside the ground-truth docs; risk of drift |
| `docs/features/webhook.md` | Feature descriptions from the 1.x era (Dallas, webhook, gateway mode, data persistence, SAT, temperature graph) | stale | mixed | yes | 2026-08-24 | docs/manuals/en/ch03-ch05 (2.x manual covers these) | 5 (ADR 2, README 1, backlog 1, docs 1) |  | decide | partly 1.x; compare per file with the manual before archiving |

## docs/fixes/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/fixes/CI_BUILD_FIX.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-03-28 |  | 0 |  | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |
| `docs/fixes/README.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-04-07 |  | 0 |  | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |
| `docs/fixes/SAFARI_FLASH_FIX.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-03-28 |  | 2 (ADR 2) |  | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |
| `docs/fixes/mqtt-auth-analysis-v0.10.3-vs-v1.0.0.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-02-10 |  | 0 |  | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |
| `docs/fixes/mqtt-whitespace-auth-fix.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-02-10 |  | 1 (release-notes 1) |  | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |
| `docs/fixes/opentherm-v42-mqtt-breaking-changes.md` | Write-ups of individual 1.x fixes (CI, Safari flash, MQTT auth, v4.2 MQTT breaking) | historical | yes | yes | 2026-09-30 |  | 5 (release-notes 2, other 1, README 1, backlog 1) | blob/main x2 (1.x) | archive -> docs/archive/1.x/fixes/ | check inbound links (one hard link) |

## docs/guides/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/guides/BUILD.md` | User and agent guides | yes | mixed | yes | 2026-11-14 |  | 17 (backlog 9, docs 3, ADR 2, agent-rules/skills 1, README 1, code/tests 1) |  | keep | per-file rows below for 1.x-only guides evaluate.py:2877 expects it: keep the path. .github/copilot-instructions.md cites docs/BUILD.md (dangling, outside docs scope). |
| `docs/guides/DOMOTICZ.md` | User and agent guides | yes | mixed | yes | 2026-05-22 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/ESP_ABSTRACTION.md` | ESP8266/ESP32 platform abstraction reference | stale | mixed | no | 2026-11-05 | .claude/rules/firmware-coding.md (platform rules) | 1 (backlog 1) |  | decide | dev is ESP32-S3 only; check against the Platform library |
| `docs/guides/FLASH_GUIDE.md` | ESP8266 flashing guide | historical | yes | yes | 2026-10-17 |  | 3 (agent-rules/skills 1, README 1, backlog 1) |  | keep | evaluate.py:2877 requires this path; 1.x users; exists on otgw-1.x.x evaluate.py:2877 expects BUILD.md and FLASH_GUIDE.md in docs/guides: keep the path. |
| `docs/guides/MIGRATION_FROM_OTTHING.md` | User and agent guides | yes | mixed | no | 2026-05-07 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/MQTT_API_CONTRACT.md` | User and agent guides | yes | mixed | no | 2026-05-08 |  | 0 |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/MQTT_LWT.md` | User and agent guides | yes | mixed | yes | 2026-10-29 |  | 3 (docs 2, other 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/MQTT_STALE_TOPICS_CLEANUP.md` | User and agent guides | yes | mixed | yes | 2026-05-18 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/MQTT_STALE_TOPICS_CLEANUP_NL.md` | NL version of the stale topics cleanup guide | yes | no | yes | 2026-05-18 | docs/guides/MQTT_STALE_TOPICS_CLEANUP.md (EN) | 0 |  | keep | pair EN/NL |
| `docs/guides/OPENHAB.md` | User and agent guides | yes | mixed | yes | 2026-05-22 |  | 1 (docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/OT_THING_FEATURE_PARITY.md` | User and agent guides | yes | mixed | no | 2026-11-07 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md` | User and agent guides | yes | mixed | yes | 2026-10-25 |  | 48 (release-notes 37, docs 6, backlog 2, other 1, README 1, code/tests 1) | gh:v2.0.0-beta.105, gh:v2.0.0-beta.118, gh:v2.0.0-beta.130, gh:v2.0.0-beta.144 +56; blob/main x1 (1.x) | keep | per-file rows below for 1.x-only guides |
| `docs/guides/WEBSOCKET_LOGGING.md` | User and agent guides | yes | mixed | yes | 2026-03-28 |  | 1 (docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/WIFI_FIRMWARE_UPDATE.md` | User and agent guides | yes | mixed | no | 2026-11-15 |  | 59 (release-notes 37, backlog 9, docs 8, code/tests 3, other 1, README 1) | gh:v2.0.0-beta.105, gh:v2.0.0-beta.118, gh:v2.0.0-beta.130, gh:v2.0.0-beta.144 +56 | keep | per-file rows below for 1.x-only guides |
| `docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md` | User and agent guides | yes | mixed | yes | 2026-11-14 |  | 8 (docs 4, backlog 2, ADR 1, release-notes 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/agent-build-and-bench.md` | User and agent guides | yes | mixed | no | 2026-11-15 |  | 9 (backlog 5, agent-rules/skills 2, docs 2) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/backlog-cli.md` | User and agent guides | yes | mixed | no | 2026-11-05 |  | 9 (backlog 5, other 1, agent-rules/skills 1, docs 1, code/tests 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/bench-check-example.json` | Example input for bench_check.py | yes | no | no | 2026-10-31 |  | 1 (docs 1) |  | keep |  |
| `docs/guides/bench-check.md` | User and agent guides | yes | mixed | no | 2026-11-11 |  | 2 (backlog 1, code/tests 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/browser-debug-console.md` | User and agent guides | yes | mixed | yes | 2026-05-16 |  | 1 (release-notes 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/discord-mcp-server.md` | User and agent guides | yes | mixed | no | 2026-11-11 |  | 7 (agent-rules/skills 2, backlog 2, docs 2, code/tests 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/pr-checklist.md` | User and agent guides | yes | mixed | no | 2026-10-22 |  | 23 (backlog 15, docs 4, other 2, agent-rules/skills 2) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/second-machine-setup.md` | User and agent guides | yes | mixed | no | 2026-11-05 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/test-runner.md` | User and agent guides | yes | mixed | no | 2026-11-08 |  | 4 (backlog 2, docs 1, code/tests 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/versioning-policy.md` | User and agent guides | yes | mixed | no | 2026-11-05 |  | 3 (backlog 2, agent-rules/skills 1) |  | keep | per-file rows below for 1.x-only guides |
| `docs/guides/worktrees.md` | User and agent guides | yes | mixed | no | 2026-11-12 |  | 9 (agent-rules/skills 4, backlog 4, docs 1) |  | keep | per-file rows below for 1.x-only guides |

## docs/hardware/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/hardware/PINOUT.md` | Pinouts and the ESP32-S3 datasheet | yes | mixed | no | 2026-06-28 |  | 17 (docs 6, backlog 5, ADR 4, code/tests 2) |  | keep | datasheet PDF is a vendor file: consider a link instead of the binary |
| `docs/hardware/combo-esp32-s3-pinout.md` | Pinouts and the ESP32-S3 datasheet | yes | mixed | no | 2026-06-13 |  | 4 (backlog 2, docs 1, code/tests 1) |  | keep | datasheet PDF is a vendor file: consider a link instead of the binary |
| `docs/hardware/esp32-s3_datasheet_en.pdf` | Pinouts and the ESP32-S3 datasheet | yes | mixed | no | 2026-06-13 |  | 0 |  | keep | datasheet PDF is a vendor file: consider a link instead of the binary |

## docs/history/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/history/2026-10-15-retired-classic-web-ui-features.md` | Retired classic web UI features (2.0.0 line) | historical | no | no | 2026-10-15 |  | 9 (docs 4, ADR 2, agent-rules/skills 1, other 1, backlog 1) |  | keep | explains ADR-192; cited from docs |

## docs/images/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/images/` (21 files) | Screenshots and pinouts of the 1.x web UI and ESP8266 boards (2021) | historical | yes | 21/21 | 2026-01-28..2026-01-28 |  | 1 total, 20 files with none |  | archive -> docs/archive/1.x/images/ | 20 of 21 files have no inbound link; all exist on otgw-1.x.x |

## docs/manuals/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/manuals/` (20 files) | The 2.x manual, EN and NL chapters | yes | no | 0/20 | 2026-11-19..2026-11-20 |  | 111 total, 0 files with none |  | keep | source of docs/MANUAL.md |

## docs/opentherm specification/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/opentherm specification/` (23 files) | OpenTherm specs (v2.2, 2.3b, v4.2), member IDs, Remeha ELGA manuals | yes | no | 14/23 | 2026-02-28..2026-05-05 |  | 75 total, 11 files with none |  | keep | folder name has a space: rename only with every link updated; vendor PDFs are third-party material |

## docs/pic/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/pic/pic-firmware-6.8-spec.md` | PIC firmware 6.8 reference specification | yes | no | no | 2026-11-12 |  | 5 (backlog 5) |  | keep |  |

## docs/plans/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/plans/CPP_REFACTORING_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | yes | 2026-03-12 | docs/plans/ (same purpose, other name) | 0 |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/HAL_2_0_0_DESIGN_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-05-22 | docs/plans/ (same purpose, other name) | 1 (docs 1) |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/OTGW32_INTEGRATION_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-04-13 | docs/plans/ (same purpose, other name) | 2 (ADR 1, docs 1) |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/SAT_SIMULATION_CONTRACT_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-05-31 | docs/plans/ (same purpose, other name) | 1 (ADR 1) |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-06-03 | docs/plans/ (same purpose, other name) | 2 (ADR 1, docs 1) |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/SAT_TASK_SEPARATION_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-05-29 | docs/plans/ (same purpose, other name) | 0 |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/SETTINGS_STREAMING_REFACTOR_PLAN.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | yes | 2026-03-28 | docs/plans/ (same purpose, other name) | 0 |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |
| `docs/plans/adr-kit-governance-plan.md` | Older plans (C++ refactoring, HAL, OTGW32 integration, SAT simulation, settings streaming, adr-kit governance) | historical | mixed | no | 2026-07-04 | docs/plans/ (same purpose, other name) | 10 (backlog 8, agent-rules/skills 1, code/tests 1) |  | move -> docs/plans/ | one plans folder; OTGW32 plan name stays internal (not public text) |

## docs/plans/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/plans/2026-11-20-mqtt-burst-pacing-analysis.md` | Plans (MQTT burst pacing, JsonEmit) | yes | no | no | 2026-11-20 | docs/plans/ (same purpose) | 2 (backlog 1, ADR 1) |  | keep | target folder for docs/plans/* and root plan/* |
| `docs/plans/PLAN-streaming-json-jsonEmit.md` | Plans (MQTT burst pacing, JsonEmit) | yes | no | no | 2026-06-18 | docs/plans/ (same purpose) | 2 (backlog 1, ADR 1) |  | keep | target folder for docs/plans/* and root plan/* |

## docs/process/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/process/EVALUATION.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | yes | 2026-03-28 |  | 4 (agent-rules/skills 1, README 1, backlog 1, docs 1) |  | keep | per-file rows below .github/copilot-instructions.md cites docs/EVALUATION.md (dangling, outside docs scope). |
| `docs/process/RELEASE_PROCESS.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | yes | 2026-11-20 |  | 4 (backlog 2, agent-rules/skills 1, docs 1) |  | keep | per-file rows below |
| `docs/process/agentic-improvements-handoff-2026-11-04.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | no | 2026-11-04 |  | 14 (backlog 13, docs 1) |  | keep | per-file rows below |
| `docs/process/agentic-setup.html` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | no | 2026-11-04 |  | 3 (docs 2, backlog 1) |  | keep | per-file rows below |
| `docs/process/agentic-setup.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | no | 2026-11-12 |  | 16 (backlog 14, docs 2) |  | keep | per-file rows below |
| `docs/process/agentic-throughput-2026-11-05.md` | Throughput measurement 2026-10-06..11-04 | historical | no | no | 2026-11-05 | docs/process/agentic-waits-2026-11-05.md (next version) | 2 (backlog 2) |  | archive -> docs/archive/process/ | superseded the same day by the waits report |
| `docs/process/agentic-waits-2026-11-05.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | no | 2026-11-05 |  | 2 (backlog 1, docs 1) |  | keep | per-file rows below |
| `docs/process/branch-hygiene-queue.csv` | Branch hygiene work queue | stale | no | yes | 2026-02-26 |  | 3 (code/tests 2, docs 1) |  | decide | data file; check whether the hygiene script still uses it |
| `docs/process/branch-hygiene-status.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | yes | 2026-02-26 |  | 1 (docs 1) |  | keep | per-file rows below |
| `docs/process/directive-changelog.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | no | 2026-11-16 |  | 5 (backlog 4, agent-rules/skills 1) |  | keep | per-file rows below |
| `docs/process/ps1-lean-translator-refactor-plan.md` | PS=1 translator refactor plan | historical | no | yes | 2026-03-08 |  | 0 |  | move -> docs/plans/ | a plan |
| `docs/process/release-workflow.md` | Process docs (evaluation, release process, agentic setup, directive changelog, branch hygiene) | yes | mixed | yes | 2026-02-26 |  | 1 (docs 1) |  | keep | per-file rows below |

## docs/releases/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/releases/RELEASE_NOTES_2.0.0-beta.105.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-06 |  | 9 (release-notes 8, backlog 1) | gh:v2.0.0-beta.118, gh:v2.0.0-beta.130, gh:v2.0.0-beta.144, gh:v2.0.0-beta.158 +12 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.118.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-07 |  | 7 (release-notes 7) | gh:v2.0.0-beta.130, gh:v2.0.0-beta.144, gh:v2.0.0-beta.158, gh:v2.0.0-beta.162 +10 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.130.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-07 |  | 7 (release-notes 6, backlog 1) | gh:v2.0.0-beta.144, gh:v2.0.0-beta.158, gh:v2.0.0-beta.162, gh:v2.0.0-beta.167 +8 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.144.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-08 |  | 6 (release-notes 5, backlog 1) | gh:v2.0.0-beta.158, gh:v2.0.0-beta.162, gh:v2.0.0-beta.167, gh:v2.0.0-beta.170 +6 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.158.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-09 |  | 6 (release-notes 4, backlog 2) | gh:v2.0.0-beta.162, gh:v2.0.0-beta.167, gh:v2.0.0-beta.170, gh:v2.0.0-beta.174 +4 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.162.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-09 |  | 3 (release-notes 3) | gh:v2.0.0-beta.167, gh:v2.0.0-beta.170, gh:v2.0.0-beta.174, notes:RELEASE_NOTES_2.0.0-beta.167.md +2 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.167.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-10 |  | 2 (release-notes 2) | gh:v2.0.0-beta.170, gh:v2.0.0-beta.174, notes:RELEASE_NOTES_2.0.0-beta.170.md, notes:RELEASE_NOTES_2.0.0-beta.174.md | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.170.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-10 |  | 2 (release-notes 2) | gh:v2.0.0-beta.174, notes:RELEASE_NOTES_2.0.0-beta.174.md, notes:RELEASE_NOTES_2.0.0-draft.md | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.174.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-11 |  | 2 (backlog 1, release-notes 1) | gh:v2.0.0-beta.203, notes:RELEASE_NOTES_2.0.0-beta.203.md | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.203.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-12 |  | 2 (backlog 1, release-notes 1) | gh:v2.0.0-beta.218, notes:RELEASE_NOTES_2.0.0-beta.218.md | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.218.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-12 |  | 1 (release-notes 1) | gh:v2.0.0-beta.224, notes:RELEASE_NOTES_2.0.0-beta.224.md | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.224.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-13 |  | 1 (release-notes 1) | blob/main x3 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.237.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-14 |  | 1 (release-notes 1) | blob/main x3 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.250.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-14 |  | 10 (release-notes 9, backlog 1) | gh:v2.0.0-beta.294, gh:v2.0.0-beta.323, gh:v2.0.0-beta.339, gh:v2.0.0-beta.351 +9; blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.294.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-15 |  | 0 |  | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.323.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-15 |  | 0 |  | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.339.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-16 |  | 1 (backlog 1) |  | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.351.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-16 |  | 1 (backlog 1) |  | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.360.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-17 |  | 1 (backlog 1) | blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.374.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-18 |  | 1 (backlog 1) | blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.382.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-19 |  | 1 (backlog 1) | blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.392.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-19 |  | 1 (backlog 1) | blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.401.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-20 |  | 1 (README 1) | blob/main x1 (1.x) | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.62.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-04 |  | 12 (release-notes 12) | gh:v2.0.0-beta.105, gh:v2.0.0-beta.118, gh:v2.0.0-beta.130, gh:v2.0.0-beta.144 +20 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-beta.77.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-04 |  | 12 (release-notes 11, backlog 1) | gh:v2.0.0-beta.105, gh:v2.0.0-beta.118, gh:v2.0.0-beta.130, gh:v2.0.0-beta.144 +18 | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/RELEASE_NOTES_2.0.0-draft.md` | Release notes of 2.x betas and the stable draft | yes | no | no | 2026-11-20 |  | 9 (backlog 8, agent-rules/skills 1) |  | keep | do NOT archive: published notes link beta.250 and others by absolute blob/dev URL (see hard column) |
| `docs/releases/archive/` (33 files) | Archived release notes 1.x and early 2.0.0 betas | historical | mixed | 16/33 | 2026-04-12..2026-11-10 |  | 45 total, 15 files with none |  | keep | already the archive |

## docs/research/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/research/2.0.0-async-modernization.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-06-13 |  | 3 (ADR 2, backlog 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-06-03-ha-core-opentherm-device-routing.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-06-03 |  | 1 (docs 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-07-03-ble-memory-hypotheses.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-07-04 |  | 1 (docs 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-07-03-ble-memory-implementation-proposal.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-03 |  | 1 (backlog 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-07-03-ble-on-otgw32-no-psram-report.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-07-04 |  | 0 |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-14-4mb-ota-asset-options.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-15 |  | 3 (ADR 2, backlog 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-15 |  | 4 (ADR 2, backlog 1, docs 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-15-classic-vs-v2-gap-audit.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-15 |  | 17 (backlog 14, docs 2, ADR 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-15-crash-loop-rollback-feasibility.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-15 |  | 2 (backlog 1, ADR 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-24-esp32s3-native-https-prototype.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-10-24 |  | 13 (backlog 13) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-25-boiler-capability-discovery.md/` (1 files) | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | 0/1 | 2026-10-25..2026-10-25 |  | 1 total, 0 files with none |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-25-ha-ingress-bench-2.0.0/` (18 files) | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | 0/18 | 2026-10-25..2026-10-25 |  | 21 total, 1 files with none |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-10-25-home-assistant-ui-integration.md/` (1 files) | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | 1/1 | 2026-10-25..2026-10-25 |  | 9 total, 0 files with none |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-11-07-web-ui-languages-research.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-11-08 |  | 5 (ADR 4, backlog 1) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/2026-11-08-usb-serial-otmonitor-outlet.md` | Research reports (async, BLE memory, 4MB OTA, HTTPS, languages, USB serial) | historical | no | no | 2026-11-08 |  | 2 (backlog 2) |  | keep | dated research; one folder ha-ingress bench with evidence |
| `docs/research/deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md` | Research on OTA reboot failures after the ESP8266 core change (NL) | historical | yes | no | 2026-04-24 |  | 2 (backlog 1, ADR 1) |  | keep | 1.x core; ADR-082 cites it (ADRs are immutable) |

## docs/reviews/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/reviews/2026-02-11_codebase-improvements/` (5 files) | Dated code reviews and audits | historical | mixed | 5/5 | 2026-02-15..2026-02-28 |  | 1 total, 4 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-02-13_codebase-review/` (2 files) | Dated code reviews and audits | historical | mixed | 2/2 | 2026-02-16..2026-02-16 |  | 3 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-02-15_opentherm-v42-compliance/` (3 files) | Dated code reviews and audits | historical | mixed | 3/3 | 2026-02-22..2026-02-28 |  | 1 total, 2 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-02-16_restful-api-evaluation/` (2 files) | Dated code reviews and audits | historical | mixed | 2/2 | 2026-02-16..2026-02-16 |  | 2 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-02-20_issue-143-source-separation/` (1 files) | Dated code reviews and audits | historical | mixed | 1/1 | 2026-02-22..2026-02-22 |  | 2 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-03-16_gpio-ota-postmortem/` (2 files) | Dated code reviews and audits | historical | mixed | 2/2 | 2026-03-16..2026-03-16 |  | 1 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-03-19_critical-review-refactoring/` (1 files) | Dated code reviews and audits | historical | mixed | 1/1 | 2026-03-21..2026-03-21 |  | 0 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-03-20_v1.2.0-to-v1.3.0-beta-review/` (2 files) | Dated code reviews and audits | historical | mixed | 2/2 | 2026-03-21..2026-03-21 |  | 0 total, 2 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-07_issue-525-sdk-dhcp-analysis/` (1 files) | Dated code reviews and audits | historical | mixed | 1/1 | 2026-04-07..2026-04-07 |  | 1 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-07_opentherm-spec-deep-audit/` (4 files) | Dated code reviews and audits | historical | mixed | 4/4 | 2026-04-08..2026-04-08 |  | 2 total, 2 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-18_ultrareview_post_merge_2.0.0/` (1 files) | Dated code reviews and audits | historical | mixed | 0/1 | 2026-04-18..2026-04-18 |  | 1 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-24_feature-branch-handoff/` (1 files) | Dated code reviews and audits | historical | mixed | 0/1 | 2026-04-24..2026-04-24 |  | 0 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-24_v1.4.1-to-dev-handoff/` (1 files) | Dated code reviews and audits | historical | mixed | 1/1 | 2026-04-24..2026-04-24 |  | 0 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-04-27_otdirect-pic-parity-audit/` (1 files) | Dated code reviews and audits | historical | mixed | 0/1 | 2026-04-27..2026-04-27 |  | 3 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-11-11_otdirect-pic-parity-audit/` (5 files) | Dated code reviews and audits | historical | mixed | 0/5 | 2026-11-11..2026-11-11 |  | 4 total, 1 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/2026-11-12_pic68-otdirect-parity-audit/` (1 files) | Dated code reviews and audits | historical | mixed | 0/1 | 2026-11-12..2026-11-12 |  | 27 total, 0 files with none |  | keep | do NOT move the 2026-02 reviews: ADR-035/ADR-040 and 1.x release notes link them |
| `docs/reviews/archive/` (49 files) | Archived 2026-Q1 review set | historical | mixed | 0/49 | 2026-04-12..2026-04-12 |  | 66 total, 23 files with none |  | keep | already the archive |

## docs/sat/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/sat/SAT_thermo_nova_Documentation.docx` | Word document about thermo-nova SAT | historical | no | no | 2026-06-20 | docs/sat/thermo-nova-spec.md (cited spec) | 2 (backlog 2) |  | decide | binary; the cited spec replaces it as ground truth 3 |
| `docs/sat/bench-simulator-1412-20.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-16 |  | 4 (code/tests 3, backlog 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/evidence/` (3 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/3 | 2026-10-28..2026-10-28 |  | 8 total, 0 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/field-run-recipe-2.0.0.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-19 |  | 7 (backlog 3, docs 3, release-notes 1) | notes:RELEASE_NOTES_2.0.0-draft.md | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/firmware-sat-spec.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-19 |  | 56 (backlog 45, docs 4, ADR 3, agent-rules/skills 2, code/tests 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/decisions/open-questions.md` | Queue of questions to Sergeant D | yes | no | no | 2026-11-20 |  | 74 (backlog 37, docs 17, ADR 9, code/tests 7, agent-rules/skills 2, other 2) |  | keep | read by scripts/sat_open_questions.py: never move |
| `docs/sat/research/` (6 files) | Heat-loss identification research and its script | yes | no | 0/6 | 2026-11-13..2026-11-13 |  | 4 total, 4 files with none |  | keep | script heat_loss_id.py lives in docs/; could move to tools/ with the study |
| `docs/sat/sat-commissioning-profiles-2026-11-01.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-01 |  | 4 (backlog 3, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-commissioning-profiles-2026-11-01.second-read.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-01 |  | 2 (backlog 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-comparison.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-19 |  | 37 (backlog 24, docs 7, ADR 3, agent-rules/skills 2, code/tests 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-control-algorithm-research-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-29 |  | 15 (backlog 14, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-execution-plan-2026-11-03.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-03 |  | 0 |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-ha-entity-audit.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-30 |  | 3 (backlog 2, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-instrumentation-inventory-2026-10-31.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-31 |  | 1 (backlog 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-log-format.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-16 |  | 17 (backlog 10, ADR 3, docs 2, other 1, release-notes 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-agent-handoff-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-28 |  | 8 (backlog 4, agent-rules/skills 2, docs 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-execution-backlog-2026-10-28.json` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-28 |  | 1 (backlog 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-execution-backlog-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-28 |  | 12 (backlog 12) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-remediation-plan-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-29 |  | 30 (backlog 28, docs 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-requirements-crosswalk-2026-10-31.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-01 |  | 3 (backlog 1, docs 1, code/tests 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-requirements-crosswalk-2026-10-31.second-read.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-01 |  | 1 (docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-resolution-index-2026-10-29.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-03 |  | 3 (docs 2, backlog 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-load-verification-plan-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-19 |  | 33 (backlog 28, code/tests 3, docs 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-low-temperature-control-comparison-2026-10-28.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-28 |  | 15 (backlog 14, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-port-audit-2026-10-24.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-26 |  | 17 (backlog 12, ADR 3, docs 2) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-state-machine.html` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-05 |  | 2 (backlog 1, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sat-state-machine.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-05 |  | 2 (backlog 1, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/decisions/sergeant-d-corrections.index.json` | Provenance index of the corrections | yes | no | no | 2026-11-20 |  | 5 (code/tests 3, backlog 1, docs 1) |  | keep | generated by scripts/sat_corrections_index.py: never move |
| `docs/sat/decisions/sergeant-d-corrections.md` | Sergeant D's recorded SAT decisions (ground truth 1) | yes | no | no | 2026-11-20 |  | 157 (backlog 75, ADR 28, docs 24, code/tests 21, agent-rules/skills 9) |  | keep | read by scripts/sat_corrections_index.py and the SAT rules: never move |
| `docs/sat/sim-1412-16/` (3 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/3 | 2026-11-05..2026-11-05 |  | 8 total, 1 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sim-1412-19/` (34 files) | SAT simulation output TASK-1412.19 | historical | no | 0/34 | 2026-11-05..2026-11-05 |  | 10 total, 30 files with none |  | decide | 30 of 34 without inbound link |
| `docs/sat/sim-1519/` (106 files) | SAT simulation output TASK-1519 (106 files) | historical | no | 0/106 | 2026-10-31..2026-11-11 |  | 81 total, 83 files with none |  | decide | 81 without inbound link; generated output |
| `docs/sat/sim-1655/` (3 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/3 | 2026-11-05..2026-11-05 |  | 16 total, 0 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sim-1784/` (5 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/5 | 2026-11-10..2026-11-10 |  | 9 total, 0 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sim-1933/` (2 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/2 | 2026-11-14..2026-11-14 |  | 7 total, 0 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/sim-hp-lowload/` (762 files) | SAT heat-pump low-load simulation output (762 files) | historical | no | 0/762 | 2026-10-31..2026-11-05 |  | 133 total, 714 files with none |  | decide | 712 files without inbound link; generated sim output; candidate for an archive zip or the field-data archive outside the repo |
| `docs/sat/sim-stock/` (3 files) | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | 0/3 | 2026-11-02..2026-11-05 |  | 6 total, 0 files with none |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/thermo-nova-pin-vs-develop-2026-11-01.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-01 |  | 2 (backlog 1, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/thermo-nova-spec.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-10-22 |  | 17 (backlog 8, agent-rules/skills 3, ADR 3, docs 2, code/tests 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/transport-budgets-1412-5.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-16 |  | 2 (backlog 1, docs 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |
| `docs/sat/vtherm-comparison-2026-11-13.md` | SAT ground truth (Sergeant D corrections, open questions, thermo-nova spec), firmware spec, comparisons, plans | yes | no | no | 2026-11-13 |  | 12 (backlog 7, docs 3, other 1, code/tests 1) |  | keep | never move sergeant-d-corrections.md, .index.json, open-questions.md (scripts and rules read them) |

## docs/superpowers/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/superpowers/` (11 files) | Plans and specs written by the superpowers skill (May-July 2026) | historical | no | 0/11 | 2026-05-26..2026-07-06 | docs/plans/ (same genre) | 14 total, 4 files with none |  | move -> docs/plans/superpowers/ | or keep: the skill writes to this path by default |

## docs/templates/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/templates/hardware-verification-log.md` | Hardware verification log template | stale | no | no | 2026-05-01 |  | 3 (other 2, backlog 1) |  | decide | 3 inbound links; check whether the bench flow still uses it |

## docs/wiki-drafts/

| path | purpose | current? | 1.x-only? | on otgw-1.x.x | last change | duplicate of | inbound links | HARD (published notes) | proposed target | note |
|---|---|---|---|---|---|---|---|---|---|---|
| `docs/wiki-drafts/PIC-not-detected-on-Classic-with-S3-Mini.md` | Draft wiki page: PIC not detected on a Classic with S3 Mini | yes | no | no | 2026-11-10 |  | 1 (backlog 1) |  | decide | publish to the GitHub wiki or move into the manual ch07 |
