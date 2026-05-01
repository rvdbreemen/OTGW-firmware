---
name: update-docs
description: Update all OTGW-firmware documentation in one sequential, backlog-tracked workflow
disable-model-invocation: true
---

# /update-docs : Documentation Update Workflow

Update all project documentation in one well-tracked sequential pass. Can be invoked standalone or as part of a release.

## Why sequential and backlog-tracked

This workflow used to spawn one subagent per affected manual chapter in parallel. Fast in theory, but on releases that touched four or more chapters it tripped Claude API concurrency limits and the run failed mid-flight. The new model has two properties:

- **Exactly one subagent runs at a time.** Slower wall-clock, no rate-limit hits, and a deterministic order of operations.
- **Every doc area is an AC on a single backlog task.** The run is replayable from the task on its own; progress is visible in `mcp__backlog__task_view` while the workflow runs in the background; partial failures leave a clear breadcrumb trail.

## Usage

```
/update-docs                       # Standalone: scope from git changes
/update-docs --release 2.0.1       # Release mode: also generate release documents
/update-docs --scope full          # Force-update all docs regardless of git diff
```

## Writing style rules

- **Never use em dashes.** Use colons, periods, commas, or parentheses.
- **All release documents in English** (international audience).
- **Manual chapters: English version always, Dutch version always.**
- **No emojis** in technical documentation.
- **Concise and correct over long and impressive.**

---

## Phase 1: Scope detection

Determine what changed since the last release and which documentation is affected.

```bash
PREV_TAG=$(gh release view --json tagName --jq '.tagName' 2>/dev/null || git describe --tags --abbrev=0)
git diff --name-only $PREV_TAG..HEAD
```

Categorize using this mapping:

| Changed files match | Subsystem | Docs affected |
|---|---|---|
| `networkStuff.ino`, `EthernetESP32.ino` | Network | ch06/h06, c4-code-network.md |
| `MQTTstuff.ino`, `mqttha.cfg` | MQTT | ch04/h04, docs/api/MQTT.md, OpenAPI |
| `restAPI.ino` | REST API | ch09/h09, docs/api/openapi.yaml, docs/api/README.md |
| `OTGW-Core.ino`, `OTDirect.ino` | OpenTherm core | ch08/h08, c4-code-otgw-core.md |
| `SAT.ino` | SAT thermostat | ch05/h05, c4-component-smart-thermostat.md |
| `sensorStuff.ino` | Sensors | docs/api/DALLAS_SENSOR_LABELS_API.md |
| `settingStuff.ino`, `OTGW-firmware.h` | Settings/State | ch08/h08, c4-code-settings.md |
| `data/index*.html`, `data/*.js`, `data/*.css` | Web UI | ch03/h03, c4-code-web-assets.md |
| `platformio.ini`, `build.py`, `flash_esp.py` | Build system | ch08/h08 (developer guide) |
| `OTGW-firmware.ino` | Main/boot | ch01/h01 if feature-level |

If `--scope full` or six or more subsystems changed, treat all docs as affected.

**Output of Phase 1:** an ordered list of affected doc areas, the PREV_TAG hash, and a categorized commit summary. Pass this to Phase 2 verbatim. Do NOT wait for user confirmation in standalone mode.

---

## Phase 2: Plan as a backlog task

Before any documentation is written, capture the run as one backlog task.

**Create the task** via `mcp__backlog__task_create`:

- `title`: `docs: update for changes since <PREV_TAG>` (in `--release` mode add the version: `docs: update for v<version> (changes since <PREV_TAG>)`)
- `status`: `In Progress`
- `assignee`: `["@claude"]`
- `labels`: `["docs", "update-docs"]` (add `"release"` in --release mode)
- `description`: paste the Phase 1 output verbatim (PREV_TAG, categorized commits, list of affected areas)
- `acceptanceCriteria`: one entry per affected doc area, in execution order:
    1. `Manual chapter <N> (<EN file>, <NL file>) updated`  (one AC per affected chapter)
    2. `API documentation (openapi.yaml, README.md, MQTT.md, ...) updated`  (only if API changed)
    3. `C4 architecture docs (<files>) updated`  (only if structural change)
    4. `Cleanup phase complete (archive old releases, move misplaced files, reorg reviews)`
    5. (--release only) `Release documents generated (RELEASE_NOTES, RELEASE_GITHUB, BREAKING_CHANGES, README What's New)`

**Record the assigned `TASK-NNN` ID** returned by `task_create`. Phase 5's commit message uses it; the commit-msg hook will block the commit if the task file is not staged.

---

## Phase 3: Sequential execution

**Exactly one subagent runs at a time.** When it finishes, update the backlog task and start the next.

For each AC in order:

1. Spawn ONE subagent in the background (`run_in_background=true`) with the relevant prompt template (3A / 3B / 3C / 3D below).
2. Wait for the completion notification. Do not spawn the next subagent before this notification arrives.
3. Read the agent's summary. If it reports errors, `mcp__backlog__task_edit --append-notes` with the failure detail and either retry once with a corrected prompt or escalate to the user.
4. On success: `mcp__backlog__task_edit --check-ac <N>` to tick the AC, and `--append-notes` with a one-line summary of what was changed.
5. Move to the next AC.

### 3A: Manual chapter subagent

Used for each affected manual chapter. One subagent per chapter, sequential.

**Chapter mapping:**

| Chapter | EN file | NL file |
|---|---|---|
| 1 (Introduction/features) | `docs/manuals/en/ch01-introduction.md` | `docs/manuals/nl/h01-introductie.md` |
| 2 (Hardware/install) | `docs/manuals/en/ch02-hardware-setup.md` | `docs/manuals/nl/h02-hardware-installatie.md` |
| 3 (Web interface) | `docs/manuals/en/ch03-web-interface.md` | `docs/manuals/nl/h03-webinterface.md` |
| 4 (Home Assistant/MQTT) | `docs/manuals/en/ch04-home-assistant.md` | `docs/manuals/nl/h04-home-assistant.md` |
| 5 (SAT thermostat) | `docs/manuals/en/ch05-sat-thermostat.md` | `docs/manuals/nl/h05-sat-thermostaat.md` |
| 6 (Network) | `docs/manuals/en/ch06-network.md` | `docs/manuals/nl/h06-netwerk.md` |
| 7 (Troubleshooting) | `docs/manuals/en/ch07-troubleshooting.md` | `docs/manuals/nl/h07-probleemoplossing.md` |
| 8 (Developer guide) | `docs/manuals/en/ch08-developer-guide.md` | `docs/manuals/nl/h08-ontwikkelaarsgids.md` |
| 9 (API reference) | `docs/manuals/en/ch09-api-reference.md` | `docs/manuals/nl/h09-api-referentie.md` |
| 10 (Appendix) | `docs/manuals/en/ch10-appendix.md` | `docs/manuals/nl/h10-bijlagen.md` |

**Prompt template:**

> Read the current `[EN file]` and `[NL file]`. Read the git diff for the relevant source files: `git diff [PREV_TAG]..HEAD -- [source files]`. Update both files to reflect the changes accurately. Preserve all existing content that is still correct. The EN file must be in English, the NL file must be in Dutch. Technical terms stay in English in both. Keep the same `## Chapter N:` / `## Hoofdstuk N:` heading format. Write the updated files back. Report which sections you changed and why; flag any change that needed an architectural decision rather than a doc edit.

### 3B: API documentation subagent

Single subagent, only if REST or MQTT changed.

Files in scope: `docs/api/openapi.yaml`, `docs/api/README.md`, `docs/api/MQTT.md`, `docs/api/WEBSOCKET_FLOW.md` (only if WebSocket changed), `docs/api/DALLAS_SENSOR_LABELS_API.md` (only if Dallas API changed).

**Prompt template:**

> Read the current docs in `docs/api/`. Read the git diff: `git diff [PREV_TAG]..HEAD -- restAPI.ino MQTTstuff.ino`. Update the affected API docs to match the current implementation. For openapi.yaml: ensure every endpoint present in `kV2Routes[]` in `restAPI.ino` has a spec entry. Add new endpoints, remove removed ones, update changed response schemas. For MQTT.md: verify topic paths and payload formats match the current `MQTTstuff.ino` publish calls. Report endpoints added, removed, and changed.

### 3C: C4 architecture docs subagent

Single subagent, only if a new `.ino` file was added or three or more `.ino` files changed.

**Prompt template:**

> Read the current `docs/c4/c4-code-*.md` and `docs/c4/c4-component-*.md` files affected by the changes in `git diff [PREV_TAG]..HEAD -- src/OTGW-firmware/*.ino`. Targeted updates only, no rewrites from scratch. Preserve the existing C4 structure (Code level documents per source area, Component level per logical group). Report which sections were touched.

### 3D: Release documents (`--release` mode only)

Five sub-ACs, executed sequentially in order.

**3D-1: Gather changes and contributors.** Single subagent.

```bash
git log $PREV_TAG..HEAD --oneline | grep -v "CI: update version.h"
```

> Categorize each commit into: new feature, bug fix, internal improvement, breaking change. Scan `docs/adr/` for ADRs added or modified since `[PREV_TAG]`. Pull contributors from three sources sequentially: (1) `gh pr list --state merged --search "merged:>[PREV_DATE]" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`. (2) Discord `#beta-testing` (channel `914498730001072149`) since `[PREV_DATE]`. (3) Discord `#devs-esp-firmware` (channel `924989767966425158`). Strip trailing digits from Discord usernames. Exclude bot IDs and maintainer `384411356616720384`. Output a structured commit-classification table and contributor list.

**3D-2: Generate `RELEASE_NOTES_<version>.md`** at repo root. Single subagent.

> Read the template in `docs/process/RELEASE_PROCESS.md`. Write `RELEASE_NOTES_<version>.md` with sections: release summary (2-3 sentences), what's new (features grouped by subsystem), bug fixes, breaking changes (always explicit, either "none" or list), upgrade notes, known issues, contributors. Use the categorized commit list from 3D-1. English. No em dashes. No emojis.

**3D-3: Generate `RELEASE_GITHUB_<version>.md`** at repo root. Single subagent.

> Concise GitHub release body. Sections: short intro (one sentence), highlights (bullet list, max eight items), bug fixes (bullet list), upgrade notes (only if needed), thank you (shoutout to most active contributor, bullet list of others, Discord invite link). English. No em dashes.

**3D-4: Update `docs/BREAKING_CHANGES.md`.** Single subagent.

> Prepend a new version section to `docs/BREAKING_CHANGES.md`. Explicitly state whether there are breaking changes for this version: either "None" or a list. Read the existing file first to match its format.

**3D-5: Update `README.md` What's New section.** Single subagent.

> In `README.md`: demote the current "What's New in v<prev>" section to "What was new in v<prev>". Add a new "What's New in v<version>" section with four to six bullet highlights drawn from `RELEASE_NOTES_<version>.md`.

---

## Phase 4: Docs folder cleanup

Inline shell operations, no subagents. Always runs, regardless of mode. Idempotent.

### 4A: Archive old release notes

If `docs/releases/` has more than ten release-note files, move the oldest into `docs/releases/archive/`, keeping the four newest in place.

```bash
ls docs/releases/RELEASE_NOTES_*.md | sort | head -n -4
ls docs/releases/RELEASE_GITHUB_*.md | sort | head -n -4
```

### 4B: Move misplaced files

Identify and move release documents from the repo root to `docs/releases/`:

```bash
ls RELEASE_NOTES_*.md RELEASE_GITHUB_*.md GITHUB_RELEASE_*.md 2>/dev/null
```

Exception: the CURRENT release's documents stay at root during the release phase, then move after publication.

### 4C: Reviews folder organization

If `docs/reviews/` has more than fifteen subdirectories, group those older than ninety days under `docs/reviews/archive/<YYYY-Q<N>>/`. Preserve the ten most recent as-is.

### 4D: Verify docs/archive

Check `docs/*.md` for clearly outdated files (version-specific or superseded) and move them to `docs/archive/`. `BREAKING_CHANGES.md` and `upgrade-from-*.md` always stay at root.

---

## Phase 5: Verify, finalize, commit

After all ACs are checked and cleanup is done:

1. `git diff --name-only` to see what changed.
2. Append a final summary to the backlog task: `mcp__backlog__task_edit --final-summary "<one paragraph: areas updated, contributors counted, anything notable>"`.
3. Flip the task to `Done`: `mcp__backlog__task_complete` (preferred) or `mcp__backlog__task_edit -s Done`.
4. Stage all docs PLUS the task file (the commit-msg hook requires it):
   ```bash
   git add docs/ README.md CHANGELOG.md backlog/tasks/task-<NNN>-*.md
   ```
5. Commit with the task ID in the message:
   ```
   docs: update documentation for changes since <PREV_TAG> (TASK-<NNN>)
   ```
6. **Standalone mode:** `git push`.
7. **Release mode:** do NOT push. The `/release` skill commits and pushes everything together.

If `git diff --name-only` returns empty after Phase 3, skip the commit and report "no documentation changes detected." Still flip the backlog task to `Done` with the final-summary noting "no changes required."

---

## Integration with /release

The `/release` skill calls this workflow in its Phase 4. When called from `/release`:

- Pass `--release <version>` so Phase 3D runs.
- Phase 5 commit is skipped (release skill commits everything together with the version bump).
- The release skill's CHECKPOINT 1 reviews the generated release documents before commit.
- The sequential model still applies in `--release` mode: Phase 3D is itself five sequential subagents, not a parallel fan-out.

---

## Important rules

- **Sequential and backlog-tracked.** Exactly one subagent at a time. Every doc area is its own AC.
- **One task per run.** Every standalone `/update-docs` invocation creates a NEW backlog task; do not reuse a previous task.
- **Read before writing.** Every subagent must read the current version of a file before updating it.
- **Scope discipline.** Only update what actually changed. Do not rewrite docs for unchanged subsystems.
- **Preserve structure.** Keep existing heading levels, table formats, file organization.
- **Accuracy over completeness.** A correct partial update beats a comprehensive inaccurate one.
- **Dutch chapters in Dutch.** Technical terms stay English in both files.
- **OpenAPI matches implementation.** Always verify endpoints against `kV2Routes[]` in `restAPI.ino`.
- **Commit and push at end of standalone runs.** Never leave doc changes uncommitted.
- **Release-mode commit is owned by /release.** Do not commit from inside `/update-docs` when invoked from the release skill.
