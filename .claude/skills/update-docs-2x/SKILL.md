---
name: update-docs-2x
description: Update the 2.x (ESP32-S3, dev / release/2.x) documentation in one sequential, backlog-tracked pass, scoped from the last v2 tag; standalone on dev, or in release mode for /beta-release-2x and /release-2x
disable-model-invocation: true
---

# /update-docs-2x: 2.x Documentation Update Workflow

The 2.x counterpart of `/update-docs` (1.x line only). Updates the manuals (EN + NL),
the API docs and the C4 docs for the changes since the previous 2.x tag, and in
release mode also the release notes, `docs/BREAKING_CHANGES.md` and the README.

## What differs from the 1.x /update-docs

| Topic | 1.x `/update-docs` | 2.x `/update-docs-2x` |
|---|---|---|
| Base tag | `gh release view` (GitHub "Latest") | last `v2.*` tag reachable from HEAD. On dev, "Latest" is v1.7.7, which is not an ancestor of dev |
| Source map | 1.x files (`SAT.ino`, `index*.html`, ...) | dev files (`SAT*.ino`, `MQTTHaDiscovery.cpp`, `v2.*`, Platform lib, safeboot), in `scripts/docs-scope-2x.sh` |
| Version bumps | none per commit | every commit rewrites ~49 version banners: banner-only files are skipped |
| Backlog | MCP tools | `backlog` CLI only (project rule) |
| Release docs | repo root, plus `RELEASE_GITHUB_<v>.md` | `docs/releases/RELEASE_NOTES_<v>.md` only: the 2.x release skills publish that one file with `--notes-file` |
| Release mode | called from `/release` on main | called from `/beta-release-2x` (dev) or `/release-2x` (`release/2.x` worktree) |
| Staging | `git add docs/ ...` | only the files the run changed (the shared tree holds peer files) |

## Usage

```
/update-docs-2x                          # standalone on dev: scope since the last v2.* tag
/update-docs-2x --release 2.0.0-beta.2   # beta mode, called from /beta-release-2x
/update-docs-2x --release 2.0.0          # stable mode, called from /release-2x
/update-docs-2x --scope full             # all doc areas, whatever the diff says
```

A `--release` version with a `-` is beta mode; without a `-` it is stable mode.

## Writing style rules

- **Never use em dashes.** Use colons, periods, commas or parentheses.
- **Release documents in English.** Manual chapters: English and Dutch, always both.
- **No emojis.** Concise and correct over long and impressive.
- **Never add the OTGW32 board or the `esp32-otgw32` token** to the README, release
  notes, `BREAKING_CHANGES.md` or post texts. Name boards as "OTGW Classic with a
  LOLIN S3 Mini / S3 Mini Pro". Existing mentions in the manuals stay until Robert
  decides (Phase 1 reports them); a subagent does not strip or add them.
- **2.x describes ESP32-S3 only.** ESP8266 appears only as "the 1.x line" (going back
  to 1.x means putting the Wemos back). A diff-scoped run does not rewrite old ESP8266
  passages on its own; Phase 1 reports the counts.

---

## Phase 1: Scope detection

Run in the tree the docs belong to (dev, or `../wt-release-2x` for a stable release):

```bash
bash scripts/docs-scope-2x.sh                 # standalone or beta mode
bash scripts/docs-scope-2x.sh --mode stable   # stable mode
```

The script prints:

- **Base** and the commit count. Beta and standalone: the last `v2.*` tag. Stable: the
  last v2 tag without `-`; for the first stable (none exists) it falls back to the last
  beta and says that BREAKING changes and upgrade notes are measured against the 1.x line.
  It refuses (exit 2) when no v2 tag is reachable or the base is a `v1.*` tag. Never
  pass a 1.x tag with `--base`.
- **Affected doc areas**, each with its changed files and its docs. A file whose only
  change is the `Version : v...` banner is skipped and counted.
- **Unmapped changed files**: changed files under `src/`, `scripts/`, `safeboot/` or a
  build/flash script that match no area. Decide per file: a doc area (add the row to
  the table in the script, in the same commit) or no doc impact (say so in the task).
  Developer-only tooling (`ota_flash.py`, `bench_check.py`, `run_tests.py`) is usually
  ch08 at most.
- **Manual findings**: OTGW32 and ESP8266 mention counts per EN chapter. Put them in
  the task description and in the final report; they are Robert's call, not this run's.

Six or more areas, or `--scope full`: all ten chapters are affected.

Also list the commits for the categorized summary:

```bash
git log --oneline --no-merges "$BASE..HEAD" -- src/ scripts/ safeboot/ docs/adr/ | grep -v -E "^[0-9a-f]+ (docs\(backlog\)|backlog:)"
```

**Output of Phase 1:** base, mode, affected areas in execution order, unmapped
decisions, manual findings, categorized commits. Do not wait for confirmation in
standalone mode.

---

## Phase 2: Plan as a backlog task (CLI)

```bash
backlog task create "docs: update 2.x docs for changes since <BASE>" \
  -d "<Phase 1 output verbatim>" -l docs,update-docs \
  --ac "Manual chapter <N> (<EN file>, <NL file>) updated" \
  --ac "API documentation (<files>) updated" \
  --ac "C4 docs (<files>) updated" \
  --ac "Cleanup complete"                       # standalone mode only
# release mode: title "docs: update 2.x docs for v<version> (changes since <BASE>)",
# label release, and one --ac per release document (Phase 3D)
backlog task edit <id> -s "In Progress" -a @claude
```

One AC per affected area, in execution order. Check the returned ID: the CLI can
reuse an archived ID. Every run gets a new task.

---

## Phase 3: Sequential execution

**One subagent at a time**, in the background. Subagents edit docs only: no build, no
commit, no bump, no Discord. Each reports the files it changed.

For each AC in order:

1. Spawn ONE background subagent with the matching template (3A to 3D).
2. Wait for its completion notification before the next one.
3. Read its report. On errors: `backlog task edit <id> --append-notes "..."`, retry
   once with a corrected prompt, else ask Robert.
4. On success: `backlog task edit <id> --check-ac <N> --append-notes "<one line>"`,
   and add its changed files to the run's file list.

### Chapter mapping

| Chapter | EN file | NL file |
|---|---|---|
| 1 Introduction | `docs/manuals/en/ch01-introduction.md` | `docs/manuals/nl/h01-introductie.md` |
| 2 Hardware/install/flash | `docs/manuals/en/ch02-hardware-setup.md` | `docs/manuals/nl/h02-hardware-installatie.md` |
| 3 Web interface + settings | `docs/manuals/en/ch03-web-interface.md` | `docs/manuals/nl/h03-webinterface.md` |
| 4 Home Assistant/MQTT | `docs/manuals/en/ch04-home-assistant.md` | `docs/manuals/nl/h04-home-assistant.md` |
| 5 SAT | `docs/manuals/en/ch05-sat-thermostat.md` | `docs/manuals/nl/h05-sat-thermostaat.md` |
| 6 Network | `docs/manuals/en/ch06-network.md` | `docs/manuals/nl/h06-netwerk.md` |
| 7 Troubleshooting + capture tool | `docs/manuals/en/ch07-troubleshooting.md` | `docs/manuals/nl/h07-probleemoplossing.md` |
| 8 Developer guide | `docs/manuals/en/ch08-developer-guide.md` | `docs/manuals/nl/h08-ontwikkelaarsgids.md` |
| 9 API reference | `docs/manuals/en/ch09-api-reference.md` | `docs/manuals/nl/h09-api-referentie.md` |
| 10 Appendix | `docs/manuals/en/ch10-appendix.md` | `docs/manuals/nl/h10-bijlagen.md` |

### 3A: Manual chapter subagent

> You update one chapter of the OTGW-firmware 2.x manual (ESP32-S3 line, branch
> `<branch>`). Read `[EN file]` and `[NL file]`, then the diff
> `git diff [BASE]..HEAD -- [source files of this area]`. Update both files to match the
> code as it is now; keep what is still correct, keep the heading format
> (`## Chapter N:` / `## Hoofdstuk N:`), heading levels and tables. EN in English, NL in
> idiomatic Dutch (no calques), technical terms in English in both. No em dashes, no
> emojis. Check every behaviour claim against the code (`file:line`) before writing it.
> Do not add OTGW32 or `esp32-otgw32` mentions and do not remove existing ones. Do not
> rewrite ESP8266 passages outside the changed behaviour; list any you see as findings.
> Edit only these two files. Report: sections changed and why, the `file:line` behind
> each claim, findings, and anything that needs a decision rather than a doc edit.

**Chapter 5 (SAT) adds:** read `docs/sat/sergeant-d-corrections.md` first. Describe what
the firmware port does (`src/OTGW-firmware/SAT*.ino`). Any statement about what SAT
should do or how thermo-nova does it needs a citation at the pinned commit
(`other-projects/SAT-releases-thermo-nova/custom_components/sat/<file>:<line>`), opened
in this run; without one, leave it out and list it as a finding. SAT stays described as
experimental and opt-in unless the release notes say otherwise.

### 3B: API documentation subagent

Only when the `rest`, `mqtt`, `websocket` or `sensors` area changed.

> Read the docs in `docs/api/` that belong to the changed areas (`openapi.yaml`,
> `README.md`, `MQTT.md`, `WEBSOCKET_FLOW.md`, `WEBSOCKET_QUICK_REFERENCE.md`,
> `DALLAS_SENSOR_LABELS_API.md`, `openapi-dallas-sensors.yaml`). Read
> `git diff [BASE]..HEAD -- [changed source files]`. openapi.yaml: every route in
> `kV2Routes[]` in `src/OTGW-firmware/restAPI.ino` has an entry, removed routes are
> gone, changed response fields match the JsonEmit calls. MQTT.md: topics and payloads
> match the publish calls in `MQTTstuff.ino`, `MQTTHaDiscovery.cpp` and
> `SATmqttPublish.cpp`, flat per-value topics (ADR-101). A topic or field rename is a
> BREAKING change: list it. Report endpoints and topics added, removed and changed.

### 3C: C4 docs subagent

Only when a new source file was added or three or more source files changed.

> Read the `docs/c4/c4-code-*.md` and `docs/c4/c4-component-*.md` files of the changed
> areas, and `git diff [BASE]..HEAD -- src/OTGW-firmware/ src/libraries/Platform/`.
> Targeted updates only, no rewrites; keep the C4 structure. New source files get an
> entry in their area's code doc. Report the sections touched.

### 3D: Release documents (`--release` mode only)

One AC each, sequential.

**3D-1 Changes and contributors** (one subagent):

> Categorize the Phase 1 commits: new feature, bug fix, internal, BREAKING. List ADRs
> added or changed since `[BASE]` (`git diff --name-only [BASE]..HEAD -- docs/adr/`).
> Contributors, in order: (1) merged GitHub PRs since the base date
> (`gh pr list --state merged --search "merged:>[BASE_DATE]" --json author,title`);
> (2) Discord `#alpha-testing` `1514720723980259460`; (3) `#beta-testing`
> `914498730001072149`; (4) `#dev-sat-mqtt` `1105556725714649128`, each since the base
> date, with `mcp__discord-mcp__read_messages`. Count people who reported, tested or
> sent captures. Exclude the maintainer `384411356616720384` and bots. Use Discord
> display names without trailing digits. Report a table of commits by category and the
> contributor list with what each did.

**3D-2 Release notes** (one subagent), `docs/releases/RELEASE_NOTES_<version>.md`:

> Start from the newest `docs/releases/RELEASE_NOTES_2.*.md` that was actually
> published (`gh release view v<prev>` shows it). Do NOT start from
> `RELEASE_NOTES_2.0.0.md` or `RELEASE_GITHUB_2.0.0.md` if they predate the first beta:
> they are April 2026 drafts that still describe a unified ESP8266/ESP32 build.
> Keep the structure of the last beta notes: what this release is, SAT status, changes
> since the previous tag, before you flash (USB first flash with `flash_otgw.bat` /
> `./flash_otgw.sh` for a new S3 Mini, `/update` with the zip on a 2.x unit,
> `--migrate` only for an old layout, legacy TCP port 25238 is opt-in), highlights,
> capture tool, BREAKING changes, known issues, how to report, contributors.
> Beta mode: keep the beta disclaimer and say what is still untested. Stable mode:
> remove the beta disclaimer; BREAKING changes since the 1.x line for the first stable,
> else since the previous 2.x stable. Every claim checked against code or an ADR.
> English, no em dashes, no emojis, no OTGW32. Links absolute (`https://github.com/...`),
> because relative links break on the release page.

Checks after 3D-2:

```bash
grep -n -E "TODO|TBD|XXX|—|OTGW32|otgw32" docs/releases/RELEASE_NOTES_<version>.md
grep -o -E "\]\([^)]+\)" docs/releases/RELEASE_NOTES_<version>.md | grep -v -E "\(https?://|\(#"
```

Both must print nothing.

**3D-3 `docs/BREAKING_CHANGES.md`** (one subagent):

> Read the file. The 2.x line has one section per core version (`## 2.0.0`). Beta mode
> and the first stable: update the existing `## <major.minor.patch>` section with the
> BREAKING changes from 3D-1 that it lacks; do not add a section per beta. A new core
> version (2.0.1, 2.1.0): prepend a new section that says "None" or lists them. Match
> the existing format (what changed, who is affected, migration).

**3D-4 README** (one subagent):

> Beta mode: the "What's New in v<major.minor.patch>" section and the beta disclaimer
> match this beta; nothing is demoted. Stable mode: remove the beta disclaimer; for a
> new core version demote the previous "What's New in v<prev>" to "What was new in
> v<prev>" and add "What's New in v<version>" with four to six bullets from the release
> notes. No OTGW32.

The CHANGELOG is not part of this skill: the release skills roll `[Unreleased]` over.

---

## Phase 4: Cleanup (standalone mode on dev only)

Skip in release mode: moves on `release/2.x` make the cherry-pick back to dev messy, and
a beta tag should carry only the release docs.

- **4A Archive old release notes.** Keep the four newest in `docs/releases/`, move the
  rest to `docs/releases/archive/` with `git mv`. Never move the notes of the current
  or the previous published release:
  ```bash
  ls docs/releases/RELEASE_NOTES_*.md docs/releases/RELEASE_GITHUB_*.md | sort -V | head -n -4
  ```
  Show the list in the task notes before moving.
- **4B Misplaced files.** `ls RELEASE_NOTES_*.md RELEASE_GITHUB_*.md 2>/dev/null` at
  the repo root: move them to `docs/releases/`.
- **4C Reviews.** More than fifteen entries in `docs/reviews/`: group those older than
  ninety days under `docs/reviews/archive/<YYYY>-Q<N>/`, keep the ten newest.

Each move is listed in the task notes. `docs/BREAKING_CHANGES.md` and
`docs/upgrade-from-*.md` never move.

---

## Phase 5: Verify, finalize, commit

1. Changed files of this run = the subagents' reported files plus the cleanup moves.
   Cross-check with `git status --short -- docs/ README.md scripts/docs-scope-2x.sh`;
   a file in git status that is not in the run's list belongs to someone else: leave it.
2. Checks over the changed files:
   ```bash
   grep -n "—" <changed files>                        # no em dashes
   grep -n -i "otgw32" README.md docs/releases/RELEASE_NOTES_<version>.md   # release mode: none
   python tests/test_adr_governance.py                # when CLAUDE.md/AGENTS.md changed
   ```
3. `backlog task edit <id> --final-summary "<areas updated, per AC the evidence, findings for Robert>"`,
   then `-s Done` when every AC is checked.
4. **Standalone mode:** stage only the run's files plus the task file, never `git add -A`
   or `git add docs/`:
   ```bash
   git add <changed files> "backlog/tasks/task-<NNN> - "*.md
   git commit -m "docs: update 2.x docs for changes since <BASE> (TASK-<NNN>)"
   ```
   Docs-only, so no bump. Push: ask the 2.0.0 lead session first (`ListAgents`), and
   never while a build for a push is running, since a push forces a rebuild for the
   post script. Then `git pull --rebase origin dev` and `git push origin HEAD:dev`.
5. **Release mode:** no commit. Hand the file list to the calling skill: `/beta-release-2x`
   commits it with the release narrative on dev, `/release-2x` commits it on
   `release/2.x` at its CHECKPOINT 2 and cherry-picks it to dev in its Phase 7.

No changes after Phase 3: no commit, report "no documentation changes", and close the
task with a final summary that says so.

---

## Important rules

- One subagent at a time; every doc area is its own AC; one new task per run.
- Read before writing; scope discipline: only what changed since the base.
- Accuracy over completeness. A claim without a code or ADR source stays out.
- openapi.yaml matches `kV2Routes[]` in `restAPI.ino`.
- Never touch `main` or `otgw-1.x.x` from this skill.
- Robert decides on the OTGW32 and ESP8266 manual findings; report them, every run.
