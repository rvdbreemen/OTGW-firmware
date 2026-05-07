---
name: update-docs
description: Update OTGW-firmware documentation in one sequential, backlog-tracked workflow (dev / 1.5.x line)
disable-model-invocation: true
---

# /update-docs : Documentation Update Workflow (dev branch)

Update project documentation in one well-tracked sequential pass. Can be invoked standalone or as part of a release.

This is the **dev / 1.5.x line** copy of the skill. The 2.0.0 feature line carries a richer variant with manual chapters (EN/NL) and C4 architecture docs that do not exist on dev. Sections that depend on those docs are absent here. If dev later grows `docs/manuals/` or `docs/c4/`, port the corresponding phases back from the 2.0.0 SKILL.md.

## Why sequential and backlog-tracked

This workflow used to spawn one subagent per affected doc area in parallel. Fast in theory, but on releases that touched several areas it tripped Claude API concurrency limits and the run failed mid-flight. The new model has two properties:

- **Exactly one subagent runs at a time.** Slower wall-clock, no rate-limit hits, and a deterministic order of operations.
- **Every doc area is an AC on a single backlog task.** The run is replayable from the task on its own; progress is visible via `backlog task <id> --plain` while the workflow runs in the background; partial failures leave a clear breadcrumb trail.

## Usage

```
/update-docs                       # Standalone: scope from git changes
/update-docs --release 1.5.1       # Release mode: also generate release documents
/update-docs --scope full          # Force-update all docs regardless of git diff
```

## Writing style rules

- **Never use em dashes.** Use colons, periods, commas, or parentheses.
- **All release documents in English** (international audience).
- **No emojis** in technical documentation.
- **Concise and correct over long and impressive.**

## Backlog rule (dev-specific)

Use the `backlog` CLI for every task operation. This project does NOT use the backlog MCP server in mixed-worktree scenarios because it indexes one tree only and serves stale content cross-tree (see `CLAUDE.md` worktree section). Substitute every `mcp__backlog__task_*` reference in the original 2.0.0 skill with `backlog task ...` here.

---

## Phase 1: Scope detection

Determine what changed since the last release and which documentation is affected.

```bash
PREV_TAG=$(gh release view --json tagName --jq '.tagName' 2>/dev/null || git describe --tags --abbrev=0)
git diff --name-only $PREV_TAG..HEAD
```

Categorize using this mapping (dev-line files only):

| Changed files match | Subsystem | Docs affected |
|---|---|---|
| `networkStuff.ino` | Network | docs/guides/WIFI_RECOVERY.md (if WiFi-related), docs/api/README.md |
| `MQTTstuff.ino` | MQTT | docs/api/MQTT.md, docs/api/openapi.yaml, docs/guides/MQTT_LWT.md |
| `restAPI.ino` | REST API | docs/api/openapi.yaml, docs/api/README.md |
| `OTGW-Core.ino` | OpenTherm core | docs/api/MQTT-message-id-echo-audit.md (if message-table change), API docs |
| `SAT*.ino` (`SATcontrol.ino`, `SATcycles.ino`, `SATpid.ino`, `SATpressure.ino`, `SATweather.ino`) | SAT thermostat | API docs, relevant `docs/features/` or `docs/fixes/` notes |
| `sensors_ext.ino` | Sensors | docs/api/DALLAS_SENSOR_LABELS_API.md, docs/api/openapi-dallas-sensors.yaml |
| `settingStuff.ino`, `OTGW-firmware.h` | Settings/State | API docs (settings schema sections), `docs/api/MQTT.md` (if topic shape change) |
| `data/index*.html`, `data/*.js`, `data/*.css` | Web UI | docs/api/WEBSOCKET_FLOW.md (if WebSocket change), `docs/api/WEBSOCKET_QUICK_REFERENCE.md` |
| `webSocketStuff.ino` | WebSocket | docs/api/WEBSOCKET_FLOW.md, docs/api/WEBSOCKET_QUICK_REFERENCE.md |
| `build.py`, `evaluate.py`, top-level `*.sh`/`*.bat` | Build/QA | docs/guides/BUILD.md (if build flow change) |
| New ADR added under `docs/adr/` | Architecture | always: confirm cross-references in `docs/adr/README.md` if it exists |

If `--scope full` or six or more subsystems changed, treat all docs as affected.

**Output of Phase 1:** an ordered list of affected doc areas, the PREV_TAG hash, and a categorized commit summary. Pass this to Phase 2 verbatim. Do NOT wait for user confirmation in standalone mode.

---

## Phase 2: Plan as a backlog task

Before any documentation is written, capture the run as one backlog task.

**Create the task** via `backlog task create`:

```bash
backlog task create "docs: update for changes since <PREV_TAG>" \
  -s "In Progress" \
  -a @claude \
  -l docs,update-docs \
  -d "<paste Phase 1 output verbatim>" \
  --ac "API documentation (openapi.yaml, README.md, MQTT.md, ...) updated" \
  --ac "Cleanup phase complete (archive old releases, move misplaced files)"
```

Add one AC per affected area in execution order:
1. API documentation (only if API changed)
2. Subsystem-specific guides under `docs/guides/` (only if affected)
3. Feature / fix notes under `docs/features/` or `docs/fixes/` (only if a feature shipped or a bug fix landed in this window)
4. Cleanup phase
5. (--release only) Release documents generated (`RELEASE_NOTES_<v>.md`, `RELEASE_GITHUB_<v>.md`, `BREAKING_CHANGES.md` update, `README.md` What's New)

**Record the assigned `TASK-NNN` ID** returned by the CLI. Phase 5's commit message uses it; the commit-msg hook may block the commit if the task file is not staged (the bump-prerelease hook on dev doesn't enforce TASK-NNN today, but stage the task file regardless for traceability).

---

## Phase 3: Sequential execution

**Exactly one subagent runs at a time.** When it finishes, update the backlog task and start the next.

For each AC in order:

1. Spawn ONE subagent in the background (`run_in_background=true`) with the relevant prompt template (3A / 3B / 3C below).
2. Wait for the completion notification. Do not spawn the next subagent before this notification arrives.
3. Read the agent's summary. If it reports errors, `backlog task edit <id> --append-notes "..."` with the failure detail and either retry once with a corrected prompt or escalate to the user.
4. On success: `backlog task edit <id> --check-ac <N>` to tick the AC, and `--append-notes "..."` with a one-line summary of what was changed.
5. Move to the next AC.

### 3A: API documentation subagent

Single subagent, only if REST or MQTT or WebSocket changed.

Files in scope: `docs/api/openapi.yaml`, `docs/api/README.md`, `docs/api/MQTT.md`, `docs/api/WEBSOCKET_FLOW.md` (only if WebSocket changed), `docs/api/WEBSOCKET_QUICK_REFERENCE.md`, `docs/api/DALLAS_SENSOR_LABELS_API.md` (only if Dallas API changed), `docs/api/openapi-dallas-sensors.yaml`, `docs/api/MQTT-message-id-echo-audit.md` (only if message-table change).

**Prompt template:**

> Read the current docs in `docs/api/`. Read the git diff: `git diff [PREV_TAG]..HEAD -- src/OTGW-firmware/restAPI.ino src/OTGW-firmware/MQTTstuff.ino src/OTGW-firmware/webSocketStuff.ino`. Update the affected API docs to match the current implementation. For openapi.yaml: ensure every endpoint present in `kV2Routes[]` in `restAPI.ino` has a spec entry. Add new endpoints, remove removed ones, update changed response schemas. For MQTT.md: verify topic paths and payload formats match the current `MQTTstuff.ino` publish calls. For WebSocket docs: verify event types and payload structure against `webSocketStuff.ino`. Report endpoints / topics / event types added, removed, and changed.

### 3B: Guides / features / fixes subagent

Single subagent. Only if a build-flow / WiFi / MQTT-LWT or similar operational change shipped in this window, OR a feature / fix note belongs in `docs/features/` or `docs/fixes/`.

**Prompt template:**

> Read the relevant files under `docs/guides/`, `docs/features/`, `docs/fixes/` listed in the Phase 1 affected-areas list. Read the git diff: `git diff [PREV_TAG]..HEAD -- <relevant source files>`. Update the docs to reflect what actually changed. Preserve all existing content that is still correct. Targeted updates only, no rewrites from scratch. Report which files and which sections were touched, and flag anything that needed an architectural decision rather than a doc edit.

### 3C: Release documents (`--release` mode only)

Five sub-ACs, executed sequentially in order.

**3C-1: Gather changes and contributors.** Single subagent.

```bash
git log $PREV_TAG..HEAD --oneline | grep -v "CI: update version.h"
```

> Categorize each commit into: new feature, bug fix, internal improvement, breaking change. Scan `docs/adr/` for ADRs added or modified since `[PREV_TAG]`. Pull contributors from three sources sequentially: (1) `gh pr list --state merged --search "merged:>[PREV_DATE]" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`. (2) Discord `#beta-testing` (channel `914498730001072149`) since `[PREV_DATE]`. (3) Discord `#devs-esp-firmware` (channel `924989767966425158`). Strip trailing digits from Discord usernames. Exclude bot IDs and maintainer `384411356616720384`. Output a structured commit-classification table and contributor list.

**3C-2: Generate `RELEASE_NOTES_<version>.md`** at repo root. Single subagent.

> Write `RELEASE_NOTES_<version>.md` with sections: release summary (2-3 sentences), what's new (features grouped by subsystem), bug fixes, breaking changes (always explicit, either "none" or list), upgrade notes, known issues, contributors. Use the categorized commit list from 3C-1. English. No em dashes. No emojis. If a `docs/process/RELEASE_PROCESS.md` template exists, follow its structure; otherwise mirror the most recent prior `docs/releases/RELEASE_NOTES_*.md` for shape.

**3C-3: Generate `RELEASE_GITHUB_<version>.md`** at repo root. Single subagent.

> Concise GitHub release body. Sections: short intro (one sentence), highlights (bullet list, max eight items), bug fixes (bullet list), upgrade notes (only if needed), thank you (shoutout to most active contributor, bullet list of others, Discord invite link). English. No em dashes.

**3C-4: Update `docs/BREAKING_CHANGES.md`.** Single subagent.

> Prepend a new version section to `docs/BREAKING_CHANGES.md`. Explicitly state whether there are breaking changes for this version: either "None" or a list. Read the existing file first to match its format.

**3C-5: Update `README.md` What's New section.** Single subagent.

> In `README.md`: demote the current "What's New in v<prev>" section to "What was new in v<prev>". Add a new "What's New in v<version>" section with four to six bullet highlights drawn from `RELEASE_NOTES_<version>.md`.

---

## Phase 4: Docs folder cleanup

Inline shell operations, no subagents. Always runs, regardless of mode. Idempotent.

### 4A: Archive old release notes

If a `docs/releases/` directory exists and has more than ten release-note files, move the oldest into `docs/releases/archive/`, keeping the four newest in place.

```bash
ls docs/releases/RELEASE_NOTES_*.md 2>/dev/null | sort | head -n -4
ls docs/releases/RELEASE_GITHUB_*.md 2>/dev/null | sort | head -n -4
```

### 4B: Move misplaced files

Identify and move release documents from the repo root to `docs/releases/`:

```bash
ls RELEASE_NOTES_*.md RELEASE_GITHUB_*.md GITHUB_RELEASE_*.md 2>/dev/null
```

Exception: the CURRENT release's documents stay at root during the release phase, then move after publication.

### 4C: Verify docs/archive

Check `docs/*.md` for clearly outdated files (version-specific or superseded) and move them to `docs/archive/`. `BREAKING_CHANGES.md` always stays at root.

---

## Phase 5: Verify, finalize, commit

After all ACs are checked and cleanup is done:

1. `git diff --name-only` to see what changed.
2. Append a final summary to the backlog task: `backlog task edit <id> --final-summary "<one paragraph: areas updated, contributors counted, anything notable>"`.
3. Flip the task to `Done`: `backlog task edit <id> -s Done`.
4. Stage all docs PLUS the task file (the commit-msg hook on dev does not currently enforce TASK-NNN, but stage the task file regardless for traceability):
   ```bash
   git add docs/ README.md CHANGELOG.md backlog/tasks/task-<NNN>-*.md
   ```
5. Commit with the task ID in the message:
   ```
   docs: update documentation for changes since <PREV_TAG> (TASK-<NNN>)
   ```
6. **Standalone mode:** `git push origin dev` (allowed under the standing push permission documented in `CLAUDE.md`; docs-only commits skip the firmware build / evaluator gates per the same policy).
7. **Release mode:** do NOT push. The `/release` skill commits and pushes everything together.

If `git diff --name-only` returns empty after Phase 3, skip the commit and report "no documentation changes detected." Still flip the backlog task to `Done` with the final-summary noting "no changes required."

---

## Integration with /release

The `/release` skill (when present on dev) calls this workflow in its docs phase. When called from `/release`:

- Pass `--release <version>` so Phase 3C runs.
- Phase 5 commit is skipped (release skill commits everything together with the version bump).
- The release skill's review checkpoint reviews the generated release documents before commit.
- The sequential model still applies in `--release` mode: Phase 3C is itself five sequential subagents, not a parallel fan-out.

---

## Important rules

- **Sequential and backlog-tracked.** Exactly one subagent at a time. Every doc area is its own AC.
- **One task per run.** Every standalone `/update-docs` invocation creates a NEW backlog task; do not reuse a previous task.
- **Read before writing.** Every subagent must read the current version of a file before updating it.
- **Scope discipline.** Only update what actually changed. Do not rewrite docs for unchanged subsystems.
- **Preserve structure.** Keep existing heading levels, table formats, file organization.
- **Accuracy over completeness.** A correct partial update beats a comprehensive inaccurate one.
- **OpenAPI matches implementation.** Always verify endpoints against `kV2Routes[]` in `restAPI.ino`.
- **Backlog CLI only on dev.** Never use `mcp__backlog__task_*`; use `backlog task ...` per the CLAUDE.md project policy.
- **Commit and push at end of standalone runs.** Never leave doc changes uncommitted.
- **Release-mode commit is owned by /release.** Do not commit from inside `/update-docs` when invoked from the release skill.
