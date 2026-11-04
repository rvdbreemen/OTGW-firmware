# Handoff: agentic setup improvements (TASK-1612 .. TASK-1623)

| | |
|---|---|
| Written | 2026-11-04, cloud session, for Robert's local Claude Code sessions |
| Parent task | TASK-1611 |
| Background | [`agentic-setup.md`](agentic-setup.md) (Dutch report, findings F1-F8) and [`agentic-setup.html`](agentic-setup.html) (visual) |
| Branch of origin | `claude/amazing-babbage-j1mdcm` (docs and backlog only, no firmware change) |

Read this before you pick up any of the tasks below. It gives the order, the context a fresh
session lacks, the traps, and how to prove each task done.

## 1. Before you start

1. Get these files onto your `dev` tree. All are docs/backlog only, so no bump and no build are
   needed, and a direct push to `dev` is allowed:
   - `docs/process/agentic-setup.md`, `agentic-setup.html`, this handoff
   - `backlog/tasks/task-1611 .. task-1623`
2. **Task-ID collision check.** These IDs were taken from `origin/dev` at bc4119a3 (highest
   TASK-1610). If your local tree created TASK-1611+ in the meantime, renumber on the side that
   is not pushed yet before merging (backlog CLI, never by hand), and update the IDs in
   `agentic-setup.md` section 8/9 and in this file.
3. Make sure the git hooks are active in your tree: `git config core.hooksPath .githooks`.
4. These tasks are **process work, not SAT behaviour and not firmware**. None needs Sergeant D.
   TASK-1617 and TASK-1618 need Robert's approval of a design before implementing (plan
   checkpoint, AskUserQuestion). TASK-1623 is a decision task for Robert.
5. They carry no milestone and the labels `process, agentic` (plus `sat` for 1616, 1617, 1621).
   `/implement-next-task` step 1 only selects milestone 2.0.0. Either run them explicitly
   ("pick up TASK-1612") or give them a milestone; that is Robert's choice.

## 2. Order and dependencies

```
Wave 1 (independent, high):   1612  1613  1614
Wave 2 (independent, medium): 1615  1616  1622
Wave 3 (needs approval):      1617 (after 1616)   1618 (after 1612)   1619
Wave 4 (low):                 1620 -> 1623        1621
```

1612, 1613 and 1614 touch different files (`AGENTS.md`/`.github/copilot-instructions.md`,
`.github/workflows/`, `.claude/settings.json` + `.claude/hooks/`) and can run as parallel lanes.
1618 edits `CLAUDE.md` heavily: do not run it in parallel with anything that edits `CLAUDE.md`
or `AGENTS.md` (1612, 1622).

## 3. Per task: context, approach, proof, traps

### TASK-1612 Agent instructions from one shared core (high)

- **Why:** F1. Three instruction files for three agent families contradict CLAUDE.md on the
  backlog interface, the platform and ArduinoJson. The bridge doc describes a Discord setup that
  no longer exists.
- **Approach (suggested):** mark a shared core in CLAUDE.md with begin/end markers, the same way
  the binding-ADR block is handled today (`scripts/adr_governance.py`,
  `tests/test_adr_governance.py`). Copy the block verbatim into AGENTS.md and
  copilot-instructions.md (or generate it with a script) and test equality. Rewrite the
  copilot file's project overview for the ESP32-S3 2.x line. Make AGENTS.md say "backlog CLI
  only". Either rewrite `.claude/docs/discord-backlog-bridge.md` from
  `.claude/commands/backlog_discord.md` and the MCP section of CLAUDE.md, or delete it and its
  references.
- **Proof:** the new test red on a deliberately drifted copy, green after the fix.
  `python scripts/run_tests.py --no-host` green.
- **Traps:** AGENTS.md is 918 lines, much of it a generic Backlog.md reference. Keep that part,
  replace only the project rules. Do not touch the ADR block's own markers.

### TASK-1613 CI on push to dev (high)

- **Why:** F2. Direct pushes to `dev` are never checked by CI.
- **Approach:** add `push: branches: [dev]` to `evaluate.yml` and `adr-governance.yml`. For
  `build.yml`, measure the run time on a PR first and let Robert choose.
- **Proof:** a link to a green run started by a push.
- **Traps:** check that the workflows do not assume a PR context (`github.event.pull_request.*`,
  base-ref diffs). An ADR-governance diff needs a base: use `github.event.before` on push.

### TASK-1614 Portable hooks, visible failures (high)

- **Why:** F3. The evaluate PostToolUse hook does `cd 'D:/Users/Robert/...'` and ends in
  `|| true`. It never runs in cloud or macOS sessions or other worktrees, and it hides failures
  everywhere. Fresh clones lack `core.hooksPath`.
- **Approach:** reuse the `export CLAUDE_PROJECT_DIR=...` prefix the other hooks use. Print the
  evaluate summary and return a non-zero exit only on FAIL, so the agent sees it (decide whether
  a FAIL should block or just inform, and ask Robert if unsure). Add `git config core.hooksPath
  .githooks` to `.claude/hooks/session-start-build-toolchain.sh` or a new tiny SessionStart hook.
- **Proof:** a deliberate PROGMEM violation shows a visible FAIL after an Edit, then revert it.
  Show it on Windows and on Linux.
- **Traps:** the hook runs `python` on Windows and `python3` elsewhere. `evaluate.py --quick`
  must stay under the 30 s hook timeout.

### TASK-1615 Build-stamp churn from hook builds (medium)

- **Why:** in the 2026-11-04 cloud session the SessionStart toolchain build rewrote
  `_VERSION_BUILD`/`_VERSION_GITHASH` twice. The Stop hook asked to commit it, and the agent had
  to revert it each time.
- **Approach (pick one with evidence):** have the session-start hook restore `version.h` and
  `data/version.hash` after its build when only stamp lines changed, or have the stop hook ignore
  a stamp-only diff. Never ignore a `_VERSION_PRERELEASE` change.
- **Proof:** before and after in a cloud session (`git status` after session start).

### TASK-1616 Open-questions status column (medium)

- **Why:** F5. Rows show `asked`/`queued` while the Answered column names an entry.
- **Approach:** fix the rows, move the "Update" paragraphs into a History section, and add a
  test that parses the table.
- **Traps:** do **not** edit `sergeant-d-corrections.md`; its entries are immutable. Some rows
  are deliberately partial ("? 236", interpretation pending): keep those as asked and make the
  test accept an explicit marker for them.

### TASK-1617 Machine-readable register index (medium, approval needed)

- **Why:** F5. Provenance (Sergeant D or maintainer, confirmed or interpretation) is buried in
  6,013 lines.
- **Approach:** sidecar index file next to the register. Propose the schema to Robert first. Fill
  it by script where possible (id, date, heading tag "maintainer decision"), then by hand for kind
  and confirmation. Add the "add an index row" rule to the CLAUDE.md SAT block and to
  `/backlog_discord`.
- **Traps:** this changes a SAT documentation rule in CLAUDE.md. The SAT ground-truth order
  itself must not change. Keep the index factual: when unsure about `kind`, write
  `interpretation`, never guess `confirmed`.

### TASK-1618 Slim CLAUDE.md (medium, approval needed)

- **Why:** F4. 673 lines load every session. Superseded directives stay in the text.
- **Approach:** draft the core (≤ ~200 lines) and a move list. Ask Robert (AskUserQuestion, one
  question per decision). Move domain sections to `.claude/rules/*.md` or skills. Add a dated
  directive changelog.
- **Traps:** run after 1612, so the shared core exists. Re-run the 1612 drift test and
  `tests/test_adr_governance.py` (the binding-ADR block must stay in CLAUDE.md and AGENTS.md).
  Check that every skill referencing a CLAUDE.md section ("see CLAUDE.md, Per-task cadence")
  still resolves.

### TASK-1619 Reconstructable local state (medium)

- **Why:** F3, bus factor. Ledger, people file, webhook and the Discord MCP exist only on one
  workstation.
- **Approach:** a ledger generator from `git log -G"_VERSION_PRERELEASE" origin/dev --
  src/OTGW-firmware/version.h` plus the post log or Discord reads. Add an anonymised people-file
  template and a runbook in `docs/guides/`.
- **Traps:** never commit names, user IDs, per-person rules, the webhook URL or tokens.

### TASK-1620 Throughput metrics (low)

- **Approach:** a script over the backlog frontmatter (`created_date`, status history in
  `updated_date` and notes), `open-questions.md` (or the 1617 index) and git log. Store one 30-day
  run in `docs/process/`.
- **Trap:** a shallow clone gives wrong numbers. Run it on the full local clone.

### TASK-1621 Move SAT research scripts (low)

- **Trap:** dozens of `docs/sat/` files reference these scripts by path. Update all references
  in one commit and check with grep. Keep the operational SAT tools in `scripts/`.

### TASK-1622 `backlog task list` workaround (low)

- **Context:** on Linux with v1.46.1 `backlog task list --plain` listed all 115 tasks and the
  status filter worked. Reproduce on Windows first, then remove the workaround text from
  CLAUDE.md, AGENTS.md and the implement-next-task skill, or record the root cause.

### TASK-1623 Scaling decision (low, Robert decides)

- Needs the 1620 figures. Present the options (second bench, self-hosted build runner,
  always-on Discord host) with costs, and file tasks for what Robert picks.

## 4. Definition of done for every task in this set

- ACs ticked only with evidence from the session that did the work, and the evidence named in
  the final summary.
- `python evaluate.py --quick` shows no new FAIL. `python scripts/run_tests.py` is green (with
  `--no-host` off Windows).
- One commit per task, `TASK-NNN` in the message, the task file staged (commit-msg hook).
- `docs/process/agentic-setup.md` section 8: mark the finding as resolved ("opgelost in TASK-NNN,
  datum") instead of deleting the row.
- Close TASK-1611 when this report and the tasks are on `dev`. It does not wait for the
  improvement tasks.
