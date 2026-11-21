# OTGW-firmware dev: file layout audit outside docs/ (read-only)

Worktree `wt-architect` at `4a9bc3c7b` (2026-11-20, matches origin/dev). `origin/otgw-1.x.x` is at `d0a5d290e`. The repo tracks 5113 files. Nothing was moved, edited or committed.

**Scope (team-lead, revised):** everything outside `docs/`. otgw-docs covers `docs/**` and the loose root `*.md` files. Where an item outside docs/ references something inside docs/, I note it as a reference only.

**Method:**
- File lists and sizes from `git ls-files`, plus `git ls-tree -r -l -z` split on tab.
- Duplicates found by grouping blob hashes.
- Tracked-but-ignored files found with `git ls-files -ci --exclude-standard`.
- References found with `git grep -F` across build.*, platformio.ini, evaluate.py, Makefile, config.py, flash_*, scripts/, tests/, test/, tools/, bin/, .github/, .githooks/, .claude/, CLAUDE.md, AGENTS.md, docs/**, README.md, CHANGELOG.md, src/ and safeboot/.
- Presence on 1.x checked with `MSYS_NO_PATHCONV=1 git cat-file -e origin/otgw-1.x.x:<path>`. An earlier run without that variable gave false "not on 1.x" results and was discarded.

**Rules used:**
- **Release-note links.** Only `blob/dev` links in release notes limit moves on dev. All 23 such targets are under docs/ or are CONTRIBUTORS.md, so they are outside this scope. The 22 `blob/main` links point at the 1.x line.
- **ADR references.** Accepted ADRs are immutable. A path an ADR cites stays where it is, or gets a stub when it moves.
- **Directories loaded by convention.** Tools load these from their location, so zero grep hits does not mean dead: `.github/skills|agents|instructions|prompts` (Copilot), `.claude/skills|agents|commands`, `.githooks`, `.wolf/hooks`. They are judged on whether the loader is in use.
- **ESP8266 mentions.** An ESP8266 mention alone does not make a file 1.x-only. The OTGW Classic PCB with an S3 module is a 2.x target.

**Classes:** KEEP, MOVE, ARCHIVE, DELETE. DELETE is used only when a copy exists elsewhere (on otgw-1.x.x, in git history, or upstream) or the file is a regenerable artifact.

**Owners:** otgw-docs (moves into docs/), otgw-dev (src, scripts, build, tests, tooling), coordinator (branches, .claude process files, decisions).

---

## (b) Layout outside docs/

| path | today | action | target | evidence (refs) | risk | owner |
|---|---|---|---|---|---|---|
| `docs/**` and the loose root `*.md` files | n/a | covered by otgw-docs | n/a | Scope cut by team-lead. Items found earlier were handed over (see the end of this file). | n/a | otgw-docs |
| `.codex` | Empty file (0 B) in the root. Last commit 2026-04-21. | DELETE | none (empty; also in git history at 731edc96d) | Committed by accident in 731edc96d "feat(tests): rewrite test_dallas_address.cpp...". Not on 1.x. The only refs are in tools/adr-kit/INSTALL.md:121-130, which describes a `.codex/` directory; this empty file would block creating it. | low | otgw-dev |
| `.claude/settings.20260421_085354.bak` | Tracked backup of settings.json, 5066 B. 2026-04-21. | DELETE | none (copy in git history, 731edc96d) | Ignored by `*.bak` (.gitignore:36) and `.claude/*.bak` (.gitignore:82). 0 refs. Same accidental commit as `.codex`. Contains no secret keywords (0 hits for token/password/secret/bearer). | low | coordinator |
| `.full-review-archive-20260421-085044/` | 4 files. 2026-04-21. | MOVE, then DELETE the dir | `state.json` to `.full-review/archive/` | 00-scope.md, 01-quality-architecture.md and 02-security-performance.md are blob-identical to the files in `.full-review/archive/`. Only state.json is unique. 0 refs. Also on 1.x. | low | otgw-docs |
| `.full-review/` (34 files) | Output of the comprehensive-review plugin. 2026-05-01. | KEEP | none | The plugin's output dir; the policy in `.external-reviews/README.md` builds on it. Referenced by docs/templates/hardware-verification-log.md:118. Content is from the 2.0 branch merge (2026-04). Also on 1.x. | n/a | otgw-docs |
| `.external-reviews/` (2 files) | 2026-04-21. | KEEP | none | Policy directory per its own README and memory feedback_external_reviews_separate. 0 code refs. Also on 1.x. | n/a | otgw-docs |
| `.copilot-tracking/research/` (2 files) | 2026-03-06. | ARCHIVE (uncertain) | `docs/archive/1.x/copilot-tracking/`, or drop it (1.x holds a copy) | Output dir of the Copilot agent `.github/agents/task-planner.agent.md:11-86`. Also on 1.x. Keep it only if the Copilot agents stay. | low | otgw-docs |
| `plan/feature-openweathermap-onboarding-1.md` | Tracked in the root although `/plan/` is ignored (.gitignore:93). 2026-05-21. | MOVE | `docs/plans/` | 0 refs. Not on 1.x, so DELETE is not allowed. | low | otgw-docs |
| `otgw_design_package/` (21 files, 440 KB) | 2026-04-29. | ARCHIVE | Archive tag, or `docs/archive/design-package-2026-04/` without the fonts and the zip | 0 refs outside backlog/archive (TASK-435, TASK-458). evaluate.py does not read it. Not on 1.x, so DELETE is not allowed. Duplicates: 3 fonts are blob-identical to `src/OTGW-firmware/data/fonts/{inter-400,inter-700,jetbrains-mono-400}.woff2`, and `SKILL.md` is blob-identical to `.claude/skills/firmware-design-handoff/SKILL.md`. `backups/data-backup-20260427-012427.zip` is a 254 KB backup. | low | otgw-docs |
| `hardware/` (6 PDFs, 8.0 MB) | Assembly manuals for the OTGW Classic V2.0, V2.3 and V7 boards, 1.1 to 1.7 MB each. 2021-12-27. | KEEP | none | Still valid for the Classic with an S3 module. ADR-001:131 and ADR-011:410 cite the `hardware/` directory, and ADRs are immutable. A move to docs/hardware/ would need a stub. docs/MANUAL.md cites `hardware/PINOUT.md`, which does not exist (handed to otgw-docs). Also on 1.x. | n/a | otgw-docs |
| `i18n/` (de, el, nl json + glossary.md) | Web UI language packs. 2026-11-20. | KEEP | none | 74 refs, among them .claude/skills/translate_webui_otgw-firmware/SKILL.md:28,51 and .claude/workflows/translate-webui-language.js:22-38. | n/a | otgw-dev |
| `include/sdkconfig.h` | PlatformIO include dir. 2026-10-04. | KEEP | none | ADR-185:92-199, ADR-225:121. | n/a | otgw-dev |
| `test/host/` (94) vs `tests/` (448) | Two test roots. | KEEP | none | The split is by design: scripts/run_tests.py:4,29 runs `tests/test_*.py` and the `test/host/test_*.py` MSVC harnesses; .gitignore:123-124. One name clash: `test/host/test_sat_pid_derivative.py` (TASK-1195 harness) and `tests/test_sat_pid_derivative.py` have different content. Confusing, but not broken. | n/a | otgw-dev |
| `tools/adr-kit/` (46 files) | Vendored copy of the adr-kit plugin repo. 2026-04-25. | ARCHIVE (uncertain: DELETE if the upstream adr-kit repo has that version) | archive tag | 0 refs outside itself. docs/adr/README.md:1071 and .claude/adr-kit-guide.md:186 say the plugin lives in `~/.claude/plugins/cache/rvdbreemen-adr-kit/`. `bin/adr-lint` is the live copy and differs from `tools/adr-kit/bin/adr-lint`. Not on 1.x. | low | otgw-dev |
| `tools/research/` (52) | SAT research scripts. | KEEP | none | Moved here by TASK-1634 (Done). Read by tests/test_sat_kd_warmup_sim_1655.py:4-5 and tests/test_sat_closedloop_sim_1412_19.py. | n/a | otgw-dev |
| `tools/check_design_system_drift.py`, `tools/generate_release_sha256sums.py`, `tools/bench/`, `tools/cloud-recorder/` | Tools. | KEEP | none | ADR-091:27,69,77; CHANGELOG.md:848 and docs/templates/hardware-verification-log.md:103; test/host/test_ot_boiler_sim.py:7-8; docs/diagnostics/minimal-ot-log-and-upload-proposal.md:362,463. | n/a | otgw-dev |
| `tools/opentherm_v42_spec_audit.py` | 2026-04-08. | KEEP (uncertain) | none | The only ref is docs/releases/archive/RELEASE_NOTES_1.2.0.md:217. The spec audit is not tied to a line. I did not verify whether anyone runs it on dev. | low | otgw-dev |
| `scripts/` (131: 35 json-golden, 24 tests, 8 ha-ingress-rig, rest tooling) | Operational tooling. | KEEP | none | Mixed tooling, but SAT research has already moved out (TASK-1634). `scripts/tests/otgw_simulation*.log` are fixtures, allowlisted on purpose (.gitignore:4-11). `scripts/test_mibeacon_decrypt.py` stays because ADR-154:27,53 cites it. | n/a | otgw-dev |
| `scripts/test_flash_automation.py` | A test file in scripts/. 2026-06-25. | MOVE or DELETE (uncertain) | `tests/`, or drop it | 0 refs, and run_tests.py does not run it. It tests flash_esp.py `--no-interactive`. I did not check whether it still runs. | low | otgw-dev |
| `bin/` (11 adr-* CLIs) | Live adr-kit CLIs. 2026-10-22. | KEEP | none | Used by the gates (CLAUDE.md: `bin/adr-judge`), and `bin/bump-prerelease.sh` is part of the version policy. `bin/adr-audit:38` lists "Makefile" as a heuristic; harmless after the Makefile is deleted. | n/a | otgw-dev |
| Root entry points: `build.{py,sh,bat}`, `evaluate.py`, `config.py`, `flash_esp.py`, `flash_otgw.{bat,sh}`, `partitions_otgw_esp32.csv`, `requirements-build.txt`, `platformio.ini`, `AUTHORS`, `LICENSE` | 13 files. | KEEP | none | Required by CLAUDE.md or read by build.py (build.py:58,92-98,2302; build.bat:121; build.sh:151). flash_esp.py also covers ESP32 and is tested by tests/test_build_safeboot.py:91 and tests/test_flash_usb_pid_filter.py:41. | n/a | otgw-dev |
| `src/libraries/{SimpleTelnet,OpenTherm}`, `other-projects` | Submodules (.gitmodules). | KEEP | none | CI initialises only the two library submodules (build.yml:41-49). | n/a | coordinator |
| `graphify-out/graph.json` (1.79 MB) and `GRAPH_REPORT.md` | Tracked by design: .gitignore:113-116 allowlists both. | KEEP (stale; I am not judging the tooling) | none | Referenced by CLAUDE.md. GRAPH_REPORT.md:1 reads "Graph Report - . (2026-07-26)": 76 files, built on `.` although CLAUDE.md says `graphify update src`. Last commit 2026-07-27 (2f7f46889); 668 src commits since. | n/a | otgw-dev |
| `.wolf/` (16) | Knowledge files tracked by design (comment at .gitignore:97-104); runtime state is ignored. | KEEP | none | Referenced by CLAUDE.md (OpenWolf). Churn: anatomy.md, buglog.json and cerebrum.md have 10-11 commits each in the last 30 days, and buglog.json is 179 KB. The main tree has uncommitted edits to all three (session snapshot). | n/a | coordinator |
| `.github/skills/`: 17 unreferenced generic skills (9.3 MB: docx, pptx, xlsx, canvas-design, pdf, theme-factory, ...) | Vendored. 2026-03-11. | ARCHIVE or DELETE (maintainer decision; uncertain because Copilot loads this dir by convention) | none (also on 1.x and upstream) | 0 refs each. The OOXML schemas and validators are stored 3 times as identical blobs (docx/pptx/xlsx, about 2 MB duplicated). Also includes `web-artifacts-builder/scripts/shadcn-components.tar.gz` and 27 font files. | low | coordinator |
| `.github/skills/adr/` (6) | Old Copilot ADR skill. | DELETE, or fix the docs | none (also on 1.x) | Contradiction: docs/adr/README.md:1071 says "There is no in-tree skill copy at `.github/skills/adr/`; that ... location is deprecated". Yet the directory exists, and .github/copilot-instructions.md:33, .github/instructions/adr.code-review.instructions.md:24,73,94, adr.coding-agent.instructions.md:13,20 and .github/PULL_REQUEST_TEMPLATE.md.example:23 point to it. | low | coordinator |
| `.github/agents/*.agent.md` (13 generic agents) | Copilot agents. 2026-02-15. | KEEP (uncertain) | none | 0 refs, but Copilot loads them by convention. Decide together with the .github/skills row. | low | coordinator |
| `.github/copilot-instructions.md`, dangling paths | Lines 585/627 cite `docs/FLASH_GUIDE.md`, `docs/BUILD.md` and `docs/EVALUATION.md`. Line 653 cites `docs/reviews/2026-01-17_dev-rc4-analysis` and line 334 cites `docs/reviews/2026-02-01_memory-management-bug-fix/...`. | KEEP + fix | Real paths: docs/guides/FLASH_GUIDE.md, docs/guides/BUILD.md, docs/process/EVALUATION.md, docs/reviews/archive/2026-Q1/... | Checked against `git ls-files`. These lines sit outside the shared AGENT-CORE block. | low | otgw-docs |
| `AGENTS.md`, dangling path | Cites `src/OTGW-firmware/platform.h`. | KEEP + fix | `src/libraries/Platform/src/platform.h` | Checked against `git ls-files`. | low | otgw-docs |
| Tracked binaries outside docs/ | `hardware/*.pdf` (6, 8 MB); `src/OTGW-firmware/data/pic16f88/*` and `pic16f1847/*` (PIC hex/ver, firmware assets); `src/OTGW-firmware/data/fonts/*.woff2` (3); `tests/fixtures/diag/*.bin` (allowlisted, .gitignore:14); `tests/webui/fixtures` (6); `tools/bench/ot-boiler-sim/data` (1); `otgw_design_package/backups/*.zip` (254 KB); `.github/skills/**` (tar.gz, fonts) | KEEP, except the zip and .github/skills (see their rows) | none | No tracked `.elf`, no stray `.bin`, no tracked logs or captures outside the allowlisted fixtures. The largest file outside docs/ is graphify-out/graph.json (1.79 MB). | n/a | otgw-dev |

---

## (c) 1.x material outside docs/

| path | today | action | target | evidence (refs) | risk | owner |
|---|---|---|---|---|---|---|
| `Makefile` | ESP8266 arduino-cli build: `PLATFORM := esp8266:esp8266`, `BOARD = $(PLATFORM):d1_mini`, core URL 2.7.4 (Makefile:14,19,31). | DELETE from dev | none (lives on otgw-1.x.x) | build.py never calls make; its legacy arduino-cli backend calls arduino-cli directly. No workflow uses the Makefile. evaluate.py:2787-2797 and 2817-2830 give INFO only ("No legacy Makefile (expected for PlatformIO-based builds)"), so deleting it causes no FAIL. Refs to fix in the same change: docs/guides/BUILD.md:43,261-283 (wrongly says the build script uses the Makefile; for otgw-docs), .github/copilot-instructions.md:59,591. CHANGELOG history stays. | low | otgw-dev |
| `.github/actions/build/action.yml`, `.github/actions/setup/action.yml` | Composite actions running `make -j` and `make filesystem`. | DELETE from dev | none (lives on otgw-1.x.x) | No workflow uses `./.github/actions/*` (grep of .github/workflows is empty). build.yml runs `pio run -e esp32-combo`. | low | otgw-dev |
| `.claude/.vscode/{arduino,c_cpp_properties,tasks,settings}.json` | Arduino IDE config for `esp8266:esp8266:nodemcuv2`, COM5 (arduino.json:3,7). 2026-03-12. | DELETE from dev | none (lives on otgw-1.x.x) | 0 refs; the 46 hits on "settings.json" are generic-name noise. c_cpp_properties.json has 23 ESP8266 lines. Odd location inside .claude/. | low | coordinator |
| `.claude/.vscode/mcp.json` | Old VS Code MCP config: backlog plus `npx mcp-discord`, token via a `promptString` input, no secret in the file. Tracked although ignored (.gitignore:57). 2026-04-02. | DELETE | none (also on 1.x; replaced by the native saseq discord-mcp per .claude/rules/discord.md) | 3 refs, all historical (backlog/docs). | low | coordinator |
| `.claude/skills/flash/SKILL.md` | `/flash`: runs `python build.py` and then `python flash_esp.py --build --no-interactive` (SKILL.md:17-20). | DELETE, or replace with a redirect stub | none (lives on otgw-1.x.x) | Conflicts with two CLAUDE.md hard rules on dev: never run `python build.py` or `python flash_esp.py`, and only the bench-tester flashes. `disable-model-invocation: true` stops automatic use, but a user `/flash` still runs it. | med (wrong flash) | coordinator |
| `.claude/skills/update-docs/SKILL.md` | 250-line 1.x docs skill; SKILL.md:11 says "1.x line only". | Replace with a redirect stub | Stub like `.claude/skills/release/SKILL.md` and `beta-prerelease/SKILL.md`, pointing to the otgw-1.x.x worktree copy | The CLAUDE.md release table names `/update-docs` for 1.x, so it must stay invocable. A full copy on dev drifts from the authoritative 1.x copy. | low | coordinator |
| `example-api/` (4 files) | Examples for the v0/v1 REST API (`/api/v1/otgw/command`, `/api/v1/sensors/labels`, ...). 2026-02-28. | ARCHIVE | `docs/archive/1.x/example-api/`, with a stub README left in `example-api/` | docs/api/README.md:45: "v2 is the only supported API version. v0 and v1 have been removed and return 410 Gone." Refs: README.md:245 (a live link to stale examples), ADR-019:307,325 (immutable, cites the dir, hence the stub), docs/api/DALLAS_SENSOR_LABELS_API.md:569-570, docs/archive/rc3-rc4-transition/README.md:14. Also on 1.x. | med | otgw-docs |
| `hardware/` | Classic PCB manuals. | KEEP | none | Classic + S3 is a 2.x target (see (b)). | n/a | otgw-docs |
| `flash_esp.py` (16 ESP8266 mentions) | Flash tool for ESP8266 and ESP32. | KEEP | none | ESP32 paths are tested (see (b)). The ESP8266 board entry (flash_esp.py:32-34) is dead weight on dev but harmless. Trimming it is an otgw-dev decision (uncertain). | low | otgw-dev |
| `.full-review/`, `.external-reviews/`, `.copilot-tracking/` | Review output from the 2.0 branch-merge era. | see (b) | none | All three are also on 1.x. | low | otgw-docs |
| `.github/skills/*`, `.github/agents/*` | Generic Copilot assets, identical on 1.x. | see (b) | none | Also on 1.x. | low | coordinator |

---

## (d) .gitignore holes and tracked-but-ignored files

`git ls-files -ci --exclude-standard` finds 8 files. One of them (`docs/daily-issue-report.md`) is under docs/ and was handed over.

| path | today | action | target | evidence (refs) | risk | owner |
|---|---|---|---|---|---|---|
| `.claude/discord_last_checked.txt`, `github_last_checked.txt`, `tweakers_last_checked.txt` | Tracked and ignored at the same time (.gitignore:76-79), so the ignore rule does nothing. Still being committed: 2cadd9925 (2026-11-14) "chore(comms): issue sweep timestamps". | Decision (coordinator): untrack (`git rm --cached`) or delete the ignore lines | none | Read and written by .claude/commands/check_otgw_issues.md:24,46,54,62,77. If sharing the timestamps through git is intended, the ignore lines are wrong; otherwise the tracking is wrong. Today both are in place. | low | coordinator |
| `.claude/.vscode/mcp.json` | Tracked; ignored at .gitignore:57. | DELETE (see (c)) | none | No secret in the file. | low | coordinator |
| `.claude/settings.20260421_085354.bak` | Tracked; ignored by `*.bak`. | DELETE (see (b)) | none | | low | coordinator |
| `plan/feature-openweathermap-onboarding-1.md` | Tracked; ignored by `/plan/`. | MOVE (see (b)) | `docs/plans/` | | low | otgw-docs |
| `other-projects` (submodule gitlink 770aa45) | Matches the `other-projects/` ignore rule. | KEEP | none | The rule is defensive on purpose (comment at .gitignore:71-74). The main-tree snapshot shows ` M other-projects` (pointer drift), which is outside this worktree's scope. | n/a | coordinator |
| `otgw_design_package/backups/*.zip` | Tracked backup archive. There is no general `*.zip` rule; only specific zip names are ignored (.gitignore:63-64). | ARCHIVE with the package, then add an ignore for `otgw_design_package/backups/` | none | 0 refs except TASK-435 (archived). | low | otgw-docs |
| `graphify-out/` | `graphify-out/*` is ignored, with graph.json and GRAPH_REPORT.md allowlisted (.gitignore:113-116). | KEEP (by design) | none | Stale, see (b). | n/a | otgw-dev |
| `.wolf/` runtime files | memory.md, cron-state, token-ledger, suggestions, designqc-* and hooks/_session.json are all ignored (.gitignore:97-104). | KEEP (by design) | none | The knowledge files are tracked and churn (see (b)). | n/a | coordinator |
| Secret-like names | `scripts/_secrets.py` is a loader; the secret store itself lives outside the repo (.gitignore:107-111). | KEEP | none | Only file names and keywords were scanned (token/password/secret/bearer); no full secret scan. Nothing found. | n/a | n/a |
| Build and cache artifacts | None tracked: no `__pycache__`, `*.pyc`, `.pio/`, `node_modules/`, `*.local.*`, `*.swp`, `*.orig`, and no `*.bin`/`*.elf` outside the allowlisted fixtures. | none | none | .gitignore covers build/, .pio, __pycache__/, .build-venv/, tests/*.out\|exe\|obj\|pdb and test/host/generated/. | n/a | n/a |
| Untracked, not ignored | wt-architect has none, and nothing is ignored either. It is a fresh, clean worktree, so this check cannot show the main tree's state. | none | none | The main-tree session snapshot shows 2 untracked, not-ignored PNGs in `docs/images/` (handed to otgw-docs; not inspected). | n/a | coordinator |

---

## Counts per action class (scope outside docs/)

A row that only points to another section is counted once.

| class | rows | items |
|---|---|---|
| KEEP | 24 | 3 of them need a fix in place: copilot-instructions.md and AGENTS.md dangling paths, and the stale graphify output (reported only). |
| MOVE | 4 | `.full-review-archive` state.json, the `plan/` file, `/update-docs` to a redirect stub, `scripts/test_flash_automation.py` (uncertain) |
| ARCHIVE | 5 | `otgw_design_package/` (with the backups zip), `tools/adr-kit/` (uncertain), `.copilot-tracking/` (uncertain), `example-api/` (with stub), the 17 generic `.github/skills` (maintainer decision) |
| DELETE | 7 | `.codex`, `.claude/settings.*.bak`, the `.full-review-archive` dir (after the move), `Makefile`, `.github/actions/{build,setup}`, `.claude/.vscode/*` (5 files), `/flash` (or a stub); plus `.github/skills/adr` (or fix docs/adr/README.md) |
| Decision only | 1 | `.claude/*_last_checked.txt`: untrack, or delete the ignore lines |

## Handed over to otgw-docs (found before the scope cut; not counted above)

- `docs/archive/mqttha.cfg` lines 4-8 contain committed merge-conflict markers. docs/MANUAL.md:1146 and :3051 cite this file.
- `docs/evidence/task1719_61_otd_control_beta145_settings_rail.png` and `task1719_63_before_beta144_settings_rail.png` are byte-identical. This is an evidence-integrity question for the TASK-1719 owner; uncertain.
- docs/MANUAL.md dangling paths: `hardware/PINOUT.md`, `src/OTGW-firmware/data/mqttha.cfg`, `tools/generate_mqttha_progmem.py`, `tools/generate_mqttha_readable.py`. docs/api/WEBSOCKET_FLOW.md:569-570 points at an archived review dir.
- `docs/guides/FLASH_GUIDE.md` must stay at its path: evaluate.py:2877-2897 gives a WARN when it is missing.
- `docs/daily-issue-report.md` is tracked although ignored (.gitignore:94-95).
- 1.x-era docs, all also on 1.x: `docs/upgrade-from-0.x.md` (1.x keeps it as `docs/archive/upgrade-from-0.x.md`), `docs/fixes/` (5 of 6; README.md:319 and CHANGELOG.md:1051 link `opentherm-v42-mqtt-breaking-changes.md`), and the dated `docs/reviews/2026-02..04_*` dirs. `docs/research/deep-research-report_arduino_core_3.1.2...` is cited by ADR-082.
