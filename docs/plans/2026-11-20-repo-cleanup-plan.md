# Repository cleanup plan (TASK-2955)

Date: 2026-11-20. Author: otgw-architect. Status: phase 1 (audit and plan) done. Phase 2 (execution) starts after the 20:00 beta.

Robert asked for a critical review of the repository: tidy it up without losing anything, remove stale branches that have landed in `dev`, put docs, source and other files where they belong, and archive old 1.x material, old texts and logs.

This plan merges three read-only inventories made on origin/dev `4a9bc3c7b`..`55c4146b7`:

- branches, tags and worktrees (otgw-architect sub-agent);
- the layout outside `docs/`, 1.x material and `.gitignore` (otgw-architect sub-agent);
- `docs/**` and the root `*.md` files, 1685 files, one row each (otgw-docs).

The per-row inventories are in [`2026-11-20-repo-cleanup/`](2026-11-20-repo-cleanup/) (branches-audit.md, layout-audit.md, docs-inventory.md); each row carries its evidence, risk and owner. This plan carries the decisions, the order, the owners and a risk per class. Phase 2 re-checks each row against origin/dev at the moment it is executed.

## 1. Ground rules for phase 2

1. **Nothing is lost.**
   - Before the first phase-2 commit, the coordinator tags the dev tip as `archive/pre-cleanup-2026-11-20`. Every file deleted or moved after that stays reachable through that one tag, so no per-file tags are needed.
   - A branch is deleted only when one of two things holds:
     - its commits are in `dev` (or `otgw-1.x.x`) and in a published tag;
     - or it first got an `archive/<branch-name>` tag at its tip.
   - Where those archive tags go is decision D1.
2. **No history rewrite, no force-push.** `main`, `release/2.x` and published tags are not touched.
3. **Hard-keep branches stay untouched:**
   - `main`, `dev`, `otgw-1.x.x`, `release/2.x`;
   - `research/cloud-recording` and its local lanes;
   - the standing bench lane `lane-09-bench` (`wt-09-bench`);
   - every 5e telemetry lane, the `backup-5e-*` snapshots included (confirmed by the coordinator, 2026-11-20; the telemetry agent decides whether they can go);
   - every lane the coordinator holds (the list stays with the coordinator).
4. **Another session's work is never removed.** A worktree with uncommitted changes, or one owned by another session, goes to its owner through the coordinator.
   - Leftover directories move to `RvdB/OTGW-worktree-leftovers/`. They are never deleted blindly.
   - Before a worktree is removed, check its ignored directories, especially `build/` (see docs/guides/worktrees.md, "Finish a lane").
5. **Some paths cannot move.** A path that a published release note links with an absolute `blob/dev` or `tree/dev` URL stays where it is. So does a path an Accepted ADR cites; if it has to move, it gets a stub at the old path. See section 5.
6. **Small commits, one class each, in the order of section 3.** Each commit contains only its class, so the lanes in flight (webui, dev) can rebase easily. Each commit updates every reference to what it moves, in the same commit.
7. **Gates per commit:**
   - `python scripts/run_tests.py` and `python evaluate.py --quick` show no new FAIL;
   - a grep for the old path finds nothing outside CHANGELOG history and the archive;
   - docs-only commits (`*.md`, `docs/**`, `backlog/**`, `.claude/**`) skip the build, as in the push policy. C3 and C5 touch `Makefile`, `.github/**`, `tools/` and `scripts/`, so they also need an esp32-combo build and a build slot; the coordinator gives that slot after the 20:00 beta.

## 2. Findings that need a fix regardless of the cleanup

| # | finding | evidence | owner |
|---|---|---|---|
| F1 | `docs/archive/mqttha.cfg` has merge-conflict markers committed at lines 4-8 | read in this session; `docs/MANUAL.md` cites the file (via the EN chapters) | otgw-docs |
| F2 | `/flash` skill runs `python build.py` and `python flash_esp.py` | `.claude/skills/flash/SKILL.md:17-20`, read in this session; breaks two hard rules: build through `build.bat`/`build.sh` only, and only the bench-tester flashes | coordinator |
| F3 | 8 files are tracked although `.gitignore` ignores them | `git ls-files -ci --exclude-standard`, run in this session | see C2 |
| F4 | Dangling paths in agent and doc files | `.github/copilot-instructions.md` (BUILD.md, EVALUATION.md, FLASH_GUIDE.md, two moved review dirs); `AGENTS.md:97` (`src/OTGW-firmware/platform.h`, outside the AGENT-CORE block, so only AGENTS.md changes); MANUAL EN chapters (`tools/generate_mqttha_*.py`). The `PINOUT.md` link is not dangling: `build_manual.py` rebases it to `docs/hardware/PINOUT.md`, which exists (otgw-docs, 2026-11-20).; `docs/api/WEBSOCKET_FLOW.md:569-570` | otgw-docs |
| F5 | `docs/adr/README.md:1071` says `.github/skills/adr/` does not exist, but it does, and 4 Copilot files point at it | layout audit | coordinator (decision D4) |
| F6 | Two TASK-1719 evidence PNGs, a "before" and an "after", are byte-identical | same sha256, checked by both audits | owner of TASK-1719: check which screenshot is missing. otgw-webui did not do TASK-1719; the coordinator asks otgw-bench-tester |
| F7 | The local `dev` in the main tree is 328 commits behind origin/dev, and its tree is dirty (`.wolf` files, one task file, the `other-projects` pointer) | branch audit | coordinator (main-tree owner) |
| F8 | `graphify-out/` was built on `.` on 2026-07-26; 668 src commits have landed since | `GRAPH_REPORT.md:1` | otgw-dev: `graphify update src` (not part of the cleanup) |

## 3. Phase-2 commits, in order

### C1: branches and worktrees (coordinator)

Risk: low for the delete list (every commit is on dev and in a published tag); low for the archive list (the tag keeps the tip); the owner-must-decide rows carry the real risk and are never acted on without their owner.

- **Delete (26 local branches), landed in dev and in v2.0.0-beta.401.** Evidence: `git cherry` shows 0 ahead or patch-equivalent; where it does not, `git range-diff` shows only banner lines.
  - ci-combo-only, fix/ba-2918-iw, lane-1882, lane-1890, lane-1890-pre-rebase2, lane-1995n, lane-2019n, lane-2026, lane-2026-pre-rebase;
  - lane-2028, lane-2040, lane-2920, lane-2921, lane-2921b, lane-2922b, lane-2923, lane-2924, lane-apstable, lane-busy503, lane-docs-1119;
  - lane-review, lane-simport, lane-wxprefetch, lane-wxprefetch-pre, lane/ba-1889, push-397.
- **Moved from delete to archive by this plan:** `backup/ba-1965-pre-restack` has one task-file hunk that differs from dev. Rule 1 says: archive.
- **Archive (13 local branches): `archive/<name>` tag, then delete.**
  - backup/ba-1919-pre-restack, backup/ba-1965-pre-restack;
  - lane-1689tls (the NAS backup remote already holds a copy);
  - lane-1759a-pre-takeover, lane-1882-pre, lane-1890-pre-rebase, lane-2026-pre-rebase3;
  - lane-410: its SAT spec wording differs from dev. Its owner checks which wording is current before the tag.
  - lane-95-docs, lane-comms-pre-309, lane-comms-pre-330b, lane/ba-vcenv, parked/ba-ui-1843-1838-1837.
- **Keep for now (landed but not yet in a tag, or recent).** They are re-checked after the next beta.
  - docs-2026, lane-architect, lane-2019t, lane-2026-ctr, park/task-2019-pace.
- **Owner must decide (dirty worktree or another session's work):**
  - wt-comms (untracked `sat_tune_test.json`), wt-ux (uncommitted task file), wt-ba-2023 (71 entries, some staged);
  - wt-tlspoc (uncommitted TLS PoC), wt-ci-combo / review-sat-tasks (uncommitted `.full-review/`);
  - wt-2026: uncommitted changes in `src/libraries/espMqttClient`, which needs the library-change rule. It also has a 122-entry `build/`.
  - the Codex `pr701` worktree: its content is on research/cloud-recording; its `build/` is checked first.
  - another session's scratchpad worktree `devcheck`.
- **Retire worktrees (clean, landed): wt-1995, wt-1890, wt-410.** Each moves to `RvdB/OTGW-worktree-leftovers/` after its `build/` check, and only once the owning session has ended. Their branches are in the delete list.
- **Remote refs:**
  - `refs/remotes/pr/699` is an orphan ref to merged PR 699: `git update-ref -d`. Local only; GitHub is not touched.
  - `origin/task1757-staged-connect` ("kept off dev", Robert 2026-11-10): archive tag, then delete on GitHub. This is a push to GitHub, so it needs decision D2.
  - `origin/claude/funny-franklin-ybf9jh` is active today: keep.
  - The `backup/*` NAS mirror refs stay. Refreshing them is outside this cleanup.
- **Tags:** none are deleted.
  - 27 existing snapshot tags use `backup/*`, `bak-*`, `safety/*` and one-off names. Folding them into `archive/*` is optional (D1).
  - 31 v1.7.x tags have no GitHub release, among them the stable v1.7.3. This is informational: tags without a release are allowed, and the 1.x line owns them.

### C2: tracked-but-ignored and accidentally committed files (otgw-dev, one commit)

Risk: low; none of these files is read by the build, the tests or the gates (layout audit, section d), and the archive tag keeps them.

| path | action | why |
|---|---|---|
| `.codex` (empty, root) | delete | accidental, in 731edc96d; it also blocks a `.codex/` directory |
| `.claude/settings.20260421_085354.bak` | delete | accidental, in 731edc96d; ignored by `*.bak`; no secret keywords in it |
| `.claude/.vscode/mcp.json` | delete | ignored; old VS Code MCP config replaced by the native discord-mcp |
| `.claude/{discord,github,tweakers}_last_checked.txt` | decision D5 | tracked and ignored at the same time; still committed by the issue sweep (2cadd9925) |
| `docs/daily-issue-report.md` | `git rm --cached` | ignored; one-off output of 2026-07-11; the archive tag keeps it |
| `plan/feature-openweathermap-onboarding-1.md` | move to `docs/plans/` (see C6) | the root `/plan/` is ignored |

### C3: ESP8266 build and tool leftovers on dev (otgw-dev; all of them exist on otgw-1.x.x)

Risk: low for the deletes (no workflow or script on dev uses them; evaluate.py gives INFO only); medium for the `/flash` stub (a user typing `/flash` gets the redirect instead of a flash, which is the intent).

| path | action | references fixed in the same commit |
|---|---|---|
| `Makefile` (arduino-cli, d1_mini, core 2.7.4) | delete | `docs/guides/BUILD.md:43,261-283` (it wrongly says the build script uses the Makefile); `.github/copilot-instructions.md:59,591`. `evaluate.py:2787-2830` reports it at INFO level only, so the delete adds no FAIL. |
| `.github/actions/build/`, `.github/actions/setup/` | delete | no workflow uses them; `build.yml` runs `pio run -e esp32-combo` |
| `.claude/.vscode/{arduino,c_cpp_properties,tasks,settings}.json` | delete | Arduino IDE config for nodemcuv2; 0 references |
| `.claude/skills/flash/SKILL.md` | replace with a stub pointing to the bench-tester flow (docs/guides/agent-build-and-bench.md) | F2 |
| `.claude/skills/update-docs/SKILL.md` (a 250-line 1.x copy) | replace with a stub to the otgw-1.x.x worktree copy, like `/release` | the CLAUDE.md release table keeps `/update-docs` as the 1.x entry |
| `flash_esp.py` ESP8266 board entry | keep | it is harmless and the ESP32 paths are tested; trimming it is a separate otgw-dev choice |

### C4: duplicate dot-directories (otgw-docs)

- `.full-review-archive-20260421-085044/`: 3 of its 4 files are blob-identical to `.full-review/archive/`. Move `state.json` there, then delete the directory.
- `.full-review/` and `.external-reviews/` stay: both are policy directories.
- `.copilot-tracking/research/` (2 files, on 1.x) goes together with the Copilot decision D4.

### C5: unused trees that are not on 1.x (otgw-dev; the archive tag of rule 1 keeps them)

- `otgw_design_package/` (21 files, 440 KB, 0 references):
  - 3 fonts duplicate `src/OTGW-firmware/data/fonts/`;
  - its `SKILL.md` duplicates `.claude/skills/firmware-design-handoff/`;
  - it holds a 254 KB backup zip.
  
  Delete it from dev.
- `tools/adr-kit/` (46 files, 0 references): a stale vendored copy. The live CLIs are in `bin/`, and the plugin lives in the plugin cache. Delete it from dev.
- `scripts/test_flash_automation.py`: 0 references and not run by `scripts/run_tests.py`. otgw-dev runs it once: if it passes, move it to `tests/`; if not, delete it.

### C6: docs moves (otgw-docs; one commit per numbered item)

1. **One plans folder.** `docs/plans/*` (8 files), the root `plan/` file and `docs/process/ps1-lean-translator-refactor-plan.md` go to `docs/plans/`.
2. **The existing `docs/archive/` stays as it is.** It holds 2.x archives too: ADR-077 cites `docs/archive/mqttha.cfg` (a 2.x snapshot, not on otgw-1.x.x) and ADR-065 cites `docs/archive/mqttha-generator/`. F1 is fixed in place. New 1.x material from this plan goes under a new `docs/archive/1.x/`. No ADR cites a path under `docs/fixes/` or `docs/images/` (`git grep` of docs/adr, this session). ADR-020:504 names `docs/wemosd1mini-pinout-ds18b20.png`, a path that has not existed for a long time; the move changes nothing there (otgw-docs).
3. **1.x UI screenshots.** `docs/images/` holds 21 screenshots from 2021; 20 have no inbound link, and all are on otgw-1.x.x. They move to `docs/archive/1.x/images/`, and the one link is updated.
4. **1.x fix write-ups.** `docs/fixes/` moves to `docs/archive/1.x/fixes/`. `README.md:319` links one of them: update it. `CHANGELOG.md:1051` links one too, but CHANGELOG history is not edited, so that file keeps a stub (rule 5).
5. **Classic-UI feature pages.** `docs/features/TEMPERATURE_SENSOR_*` move to `docs/archive/1.x/features/`.
6. **The v0/v1 API examples.** `example-api/` (v0/v1 return 410 Gone on 2.x) moves to `docs/archive/1.x/example-api/`, with a stub README at `example-api/` because ADR-019 cites it. The link at `README.md:245` is updated.
7. **One audit.** `docs/api/MQTT-message-id-echo-audit.md` moves to `docs/audits/`, with a stub at the old path. ADR-097 (:65, :129) cites it, and two src comments do too (MQTTstuff.ino:2551, OTGW-Core.ino:2036). Those comments stay as they are, because changing them would need a prerelease bump (otgw-docs, 2026-11-20).
8. **Code inside docs/.** `docs/opentherm specification/member-ids/opentherm_public_crawler.py` and `docs/sat/research/heat-loss-id/heat_loss_id.py` move to `tools/research/`. otgw-dev does this one, with its references.

### C7: dangling-path fixes (otgw-docs, one commit)

- F4: change each path to its real location.
- F5: depends on decision D4.
- MANUAL.md is regenerated from the EN chapters, never edited by hand (`tests/test_manual_generated.py`).

### C8: `.gitignore` (otgw-dev, last)

- Add `otgw_design_package/` if it is not deleted.
- Add whatever D5 decides.
- Verify: `git ls-files -ci --exclude-standard` returns only `other-projects`, which is intentional (`.gitignore:71-74`).

## 4. Stays where it is (checked, no action)

- **docs, process and SAT ground truth:**
  - `docs/adr/**`, `docs/sat/sergeant-d-corrections.md` with its `.index.json`, `docs/sat/open-questions.md`;
  - `docs/manuals/`, `docs/MANUAL.md`, `docs/c4/`, `docs/api/` (except the one audit), `docs/releases/` (with its archive), `docs/reviews/`;
  - `docs/BREAKING_CHANGES.md`, `docs/upgrade-from-0.x.md`, `docs/guides/FLASH_GUIDE.md` (`evaluate.py:2877` warns if it moves), `docs/guides/BUILD.md`.
- **Hardware and build inputs:**
  - `hardware/` (Classic PCB manuals; still valid for the Classic with an S3 module; ADR-001 and ADR-011 cite it);
  - `i18n/`; `include/sdkconfig.h` (ADR-185); `tools/research/`, `tools/bench/`, `tools/cloud-recorder/`; `bin/`.
- **Tooling with a fixed layout:**
  - `test/host/` next to `tests/`, by design (`scripts/run_tests.py:4,29`);
  - `.wolf/` and `graphify-out/` (tracked by design, see F8);
  - the root entry points (`build.*`, `evaluate.py`, `config.py`, `flash_esp.py`, `flash_otgw.*`, `platformio.ini`, `partitions_otgw_esp32.csv`, `requirements-build.txt`).
- **Large binaries:** none to remove. No `.elf`, logs or captures are tracked outside the fixtures that `.gitignore` allowlists.

## 5. Paths fixed by published links

Absolute `blob/dev` links in the 94 published GitHub release bodies and in `docs/releases/` pin these paths. otgw-docs counts 19 targets:

- `CONTRIBUTORS.md`;
- `docs/BREAKING_CHANGES.md`;
- `docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md`, `docs/guides/WIFI_FIRMWARE_UPDATE.md`;
- `docs/sat/field-run-recipe-2.0.0.md`;
- the release notes of beta.62, 77, 105, 118, 130, 144, 158, 162, 167, 170, 174, 203, 218 and 250.

The layout audit counted 23 because it also searched README.md, CHANGELOG.md and the rest of docs/. Its 4 extra targets are linked only from repo files, not from release notes: `docs/api/MQTT.md` (from openapi.yaml), `docs/manuals/en/ch07-troubleshooting.md` and `docs/manuals/nl/h07-probleemoplossing.md` (from a wiki draft), and `docs/releases/RELEASE_NOTES_2.0.0-beta.401.md` (from README.md). Reconciled: the union of 23 stays at its path. The 4 extras are not hard pins (their links can be updated), but no move in C6 touches them either.

`blob/main` links point at the 1.x line and do not constrain dev.

## 6. Decisions for Robert (through the coordinator)

**Decided 2026-11-20:** Robert answered "A" (Discord DM 1573349163083505787): every recommendation below, as listed. F2 is tracked separately as TASK-2962.

| # | question | options and consequences | recommendation |
|---|---|---|---|
| D1 | Where do the per-branch `archive/<name>` tags go? (The one `archive/pre-cleanup-2026-11-20` tag on the dev tip goes to GitHub in every option: it only marks a dev commit that is public already.) | A: the per-branch tags go only to the NAS `backup` remote and stay local. The public tag list stays clean; GitHub has no copy of those 13 branch tips. B: push them to GitHub too. Off-site twice, but 13 more public tags. | **A.** The NAS remote already holds the off-site branch copies. Folding the 27 old snapshot tags into `archive/*` at the same time is optional. |
| D2 | Delete the GitHub branch `task1757-staged-connect` (kept off dev on 2026-11-10)? | A: archive tag, then delete. One less stale branch on GitHub. B: keep it. | **A**, if the TASK-1757 notes on dev already carry what it found. |
| D3 | SAT simulation output in `docs/sat/sim-*`: 900 files, 712 without an inbound link | A: keep as is. Nothing changes, but it is half the docs tree. B: move it to the field-data archive outside the repo. git history keeps it and the repo becomes lighter to read, but links from task notes break. C: `docs/archive/sat-sim/`. Tidier, but the size stays. | **A for 2.0.0.** History already keeps it, and moving 900 files is churn just before the stable release. Revisit after 2.0.0. |
| D4 | Copilot assets: 17 generic skills (9.3 MB; the OOXML schemas are stored three times), `.github/skills/adr/` and 13 generic agents | A: delete the generic skills from dev, and keep `adr/` and the agents but fix `docs/adr/README.md:1071` to say `adr/` exists. B: keep everything. C: delete everything not referenced. | **A.** All of it is on 1.x and upstream. The generic skills are unrelated to this firmware; the ADR skill is still referenced. |
| D5 | `.claude/*_last_checked.txt`: untrack, or drop the ignore lines? | A: untrack (`git rm --cached`). No more timestamp commits; each tree keeps its own state. B: drop the ignore lines. Sharing through git stays, along with its churn. | **A.** It is machine-local state of one sweep; the git noise (2cadd9925) has no reader. |
| D6 | The folder name with a space, `docs/opentherm specification/` (75 references) | A: leave it. B: rename it, with every link in one commit. | **A.** The risk is larger than the gain. |
| D7 | Leftover 1.x-flavoured docs that need a per-file check: `docs/features/` (non-classic pages), `docs/design/`, `docs/wiki-drafts/`, `docs/guides/ESP_ABSTRACTION.md` | A: otgw-docs checks each file against the 2.x manual and archives what the manual already covers. B: leave them. | **A, after the 2.0.0 stable release.** It is not urgent and needs reading time. |

## 7. Out of scope

- Rewriting git history to remove large files: not needed (there are no large stray binaries) and not allowed.
- Lanes the coordinator holds: listed with the coordinator only.
- Trimming the ESP8266 entry from `flash_esp.py`, and refreshing graphify: separate tasks if wanted.
