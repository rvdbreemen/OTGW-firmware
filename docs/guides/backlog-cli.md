# Backlog.md CLI Reference Guide

Full CLI reference for task management in this project. Read this when you need command details.

---

## The Golden Rule

**NEVER edit task files directly. ALL task operations use `backlog` CLI commands.**

Tasks live in `backlog/tasks/task-<id> - <title>.md`. The CLI handles Git, metadata, file naming, and relationships.

---

## Task Workflow

```bash
# 1. Find or create
backlog search "<topic>" --plain
backlog task create "Title" -d "Description" --ac "Criterion 1" --ac "Criterion 2"

# 2. Start work (do this FIRST before writing any code)
backlog task edit <id> -s "In Progress" -a @claude

# 3. Write implementation plan, share with user, wait for approval before coding
backlog task edit <id> --plan $'1. Analyze\n2. Implement\n3. Test'

# 4. During implementation: mark ACs done and append notes
backlog task edit <id> --check-ac 1 --check-ac 2
backlog task edit <id> --append-notes $'- Completed X\n- Next: Y'

# 5. Finish
backlog task edit <id> --final-summary "PR description"
backlog task edit <id> -s Done
```

---

## Definition of Done

A task is Done only when ALL of:
- All ACs checked: `backlog task edit <id> --check-ac N`
- All DoD items checked: `backlog task edit <id> --check-dod N`
- Final summary written: `backlog task edit <id> --final-summary "..."`
- Status set to Done: `backlog task edit <id> -s Done`
- Tests pass, no regressions

---

## Test Plan Section (firmware changes)

For the full pre-merge checklist (build, lint, test, hardware/browser smoke tests), see `docs/guides/pr-checklist.md`. The Test Plan below is the task-level counterpart: the per-task steps that feed into the checklist.

Any task that changes firmware behaviour a human could only verify on real
hardware MUST include a `## Test Plan` section in the notes. Without it,
regressions surface weeks later as user reports on Discord or Tweakers rather
than as a failing AC.

Required for: OpenTherm pipeline changes, MQTT topic changes, OTA path
modifications, SAT control loop changes, BLE/SAT pairing, WiFi/Ethernet stack,
anything that flips a feature flag observed by Home Assistant.

Append to any task with:

```bash
backlog task edit <id> --append-notes $'## Test Plan\n- Precondition: ...\n- Step 1: ...\n- Expected MQTT topic/value: ...\n- How to roll back: ...'
```

**Template** (keep it to 3-5 bullets):

```
## Test Plan
- Precondition: device state, settings, connected peripherals
- Steps: numbered actions that exercise the change
- Expected observable: MQTT topic/value, REST response, OLED text, log line
- Negative case: what SHOULDN'T happen (avoid silent regressions)
- Roll-back: how to revert if the change misbehaves in the field
```

Historic tasks (closed without Test Plan) should be retrofitted when reopened.

---

## Task Creation

| Action | Command |
|---|---|
| Basic | `backlog task create "Title"` |
| With description | `backlog task create "Title" -d "Description"` |
| With AC | `backlog task create "Title" --ac "Criterion 1" --ac "Criterion 2"` |
| With references | `backlog task create "Title" --ref src/api.ts --ref https://github.com/...` |
| With documentation | `backlog task create "Title" --doc https://design-docs.example.com` |
| Create draft | `backlog task create "Title" --draft` |
| Create subtask | `backlog task create "Title" -p 42` |
| No DoD defaults | `backlog task create "Title" --no-dod-defaults` |

---

## Task Editing

| Action | Command |
|---|---|
| Edit title | `backlog task edit 42 -t "New Title"` |
| Edit description | `backlog task edit 42 -d "New description"` |
| Change status | `backlog task edit 42 -s "In Progress"` |
| Assign | `backlog task edit 42 -a @claude` |
| Add labels | `backlog task edit 42 -l backend,api` |
| Set priority | `backlog task edit 42 --priority high` |
| Add plan | `backlog task edit 42 --plan $'1. Step one\n2. Step two'` |
| Add notes (replace) | `backlog task edit 42 --notes "What I did"` |
| Append notes | `backlog task edit 42 --append-notes $'- Did X\n- Did Y'` |
| Add final summary | `backlog task edit 42 --final-summary "PR description"` |
| Append final summary | `backlog task edit 42 --append-final-summary "More details"` |
| Add dependencies | `backlog task edit 42 --dep task-1 --dep task-2` |

---

## Acceptance Criteria

| Action | Command |
|---|---|
| Add AC | `backlog task edit 42 --ac "New criterion" --ac "Another"` |
| Check AC #1 | `backlog task edit 42 --check-ac 1` |
| Check multiple | `backlog task edit 42 --check-ac 1 --check-ac 3` |
| Uncheck AC #2 | `backlog task edit 42 --uncheck-ac 2` |
| Remove AC #3 | `backlog task edit 42 --remove-ac 3` |
| Mixed | `backlog task edit 42 --check-ac 1 --uncheck-ac 2 --remove-ac 3 --ac "New"` |

**Note:** `--check-ac` accepts multiple flags: `--check-ac 1 --check-ac 2`. No comma-separated or range syntax.

### AC Deviation Protocol

Not every AC survives contact with reality. When it becomes clear mid-task that an AC cannot be met as written, do NOT silently ship a partial Done. Instead:

1. **Record the deviation** in task notes before closing:
   ```bash
   backlog task edit 42 --append-notes $'AC<N> deviation: <why it could not be met as written>, <what was done instead>, <follow-up task id if any>.'
   ```
2. **Decide the label of the AC**:
   - If the AC was aspirational (e.g. "gate fails CI") and reality forced a softer version (WARN not FAIL), either leave it unchecked with a note, or mark checked and cite the softer form in the note. Be explicit about which.
   - If the AC can't be met because the work legitimately split into a follow-up task, create that task with `backlog task create` and cite its ID in the note.
3. **Request user sign-off for binding-rule deviations**: deviations on ACs tied to ADR-080 pattern-level rules need user approval before marking the task Done. Structural or scope-level deviations can usually be closed autonomously with a good note.
4. **Final summary must name the deviation** so it surfaces in the PR description, not buried in notes. A reviewer reading the commit should understand immediately which ACs were met literally and which were met by a written deviation.

**Prior art** (good examples of deviations documented correctly):

- TASK-296 (evaluate.py String class) — widened regex and split reporting per spec, but the AC-2 promotion to FAIL was deferred because 19 existing hot-path violations would block every CI run. Note explained the deferral + follow-up for burning down the debt.
- TASK-294 (CSRF collectHeaders) — code fix landed, but the AC-2 browser cross-origin proof was deferred to the pre-release smoke test (hardware required). Flagged in the final summary, not silently skipped.
- TASK-308 (build.py smoke tests) — --help smoke shipped; AC-2 fixture-based `_parse_image_header` parser was deferred with a follow-up proposal, because extracting a pure parser was out of scope for the smoke test.

The rule of thumb: if a reviewer looking at your commit a year later can't tell which ACs were literal vs which were negotiated, your deviation isn't documented well enough.

---

## Definition of Done Items

| Action | Command |
|---|---|
| Add DoD | `backlog task edit 42 --dod "Run tests" --dod "Update docs"` |
| Check DoD #1 | `backlog task edit 42 --check-dod 1` |
| Uncheck DoD #1 | `backlog task edit 42 --uncheck-dod 1` |
| Remove DoD #2 | `backlog task edit 42 --remove-dod 2` |

---

## Viewing and Searching

| Action | Command |
|---|---|
| View task | `backlog task 42 --plain` |
| List tasks | `backlog task list --plain` |
| Filter by status | `backlog task list -s "In Progress" --plain` |
| Filter by assignee | `backlog task list -a @claude --plain` |
| Search | `backlog search "auth" --plain` |
| Search in tasks only | `backlog search "login" --type task --plain` |
| Search with status | `backlog search "api" --status "To Do" --plain` |
| Archive | `backlog task archive 42` |

**Known bug:** `backlog task list` sometimes returns empty. Use `backlog task <id> --plain` or read `backlog/tasks/` directly as a fallback.

---

## Multi-line Input

Shells do not convert `\n` in normal quotes. Use ANSI-C quoting:

```bash
# Bash/Zsh
backlog task edit 42 --plan $'1. Step one\n2. Step two\n3. Test'
backlog task edit 42 --append-notes $'- Completed login flow\n- Added tests\n- TODO: edge cases'
backlog task edit 42 --final-summary $'Implemented X.\n\nChanges:\n- Added Y\n- Fixed Z'

# POSIX portable
backlog task edit 42 --notes "$(printf 'Line1\nLine2')"

# PowerShell
backlog task edit 42 --notes "Line1`nLine2"
```

---

## Final Summary Format

Write like a PR description. Include: what changed, why, user impact, tests run, risks/follow-ups.

```
Implemented pattern X because reason Y.

Changes:
- Added function foo() in bar.ino (handles edge case Z)
- Updated settings struct to include new field

Tests:
- Verified on ESP8266 NodeMCU with live OTGW hardware
- Checked MQTT discovery payloads in HA

No breaking changes to MQTT topics or REST API.
```

---

## Common Issues

| Problem | Solution |
|---|---|
| Task not found | `backlog task list --plain` or check `backlog/tasks/` directory |
| AC won't check | `backlog task 42 --plain` to see correct AC index numbers |
| Changes not saving | Ensure using CLI, not editing files directly |
| Metadata out of sync | Re-edit via CLI: `backlog task edit 42 -s <current-status>` |
