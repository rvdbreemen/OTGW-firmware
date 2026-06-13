---
name: implement-next-task
description: Implement the next actionable task of the 2.0.0 ESP32-S3-only async + FreeRTOS migration (epic TASK-865). Selects the lowest-seq async-esp32s3 backlog task whose dependencies are met, implements it, builds + evaluates, adversarially reviews, drafts a Proposed ADR for any architectural decision, bumps the prerelease, commits + pushes on feature-2.0.0-esp32s3-async, sets the task In Review (field-validation) or Done, and announces per the alpha-channel Discord policy. Use for one iteration of the migration loop (cron 7,37 * * * * or manual).
---

# implement-next-task

One iteration of the autonomous 2.0.0 ESP32-S3-only async + FreeRTOS migration loop
(epic **TASK-865**, ADR-123/128). The deterministic multi-agent engine is the
workflow script; this skill is the stable entry point and handles the Discord
announcement.

## Run

Invoke the engine (background, single-flight-guarded — it no-ops if a prior iteration
is still In Progress, so overlapping cron ticks are safe):

```
Workflow({ scriptPath: "D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-esp32s3-async/.claude/workflows/implement-next-task.js" })
```

The engine phases: **Select** (lowest-seq `async-esp32s3` task that is To Do with all
deps Done/In Review; skip if any is In Progress) → **Implement** (coding agent,
honours the task ACs + CLAUDE.md rules) → **Review** (adversarial vs ACs + binding
ADRs, one fix pass) → **ADR** (`adr-kit:adr-generator` drafts a **Proposed** ADR for
any architectural decision — never Accepted, that stays the maintainer's checkpoint;
several tasks require it: seq7 supersede ADR-108, seq12 new ADR for ADR-011, seq13
supersede ADR-047/048/058) → **Land** (bump prerelease if `src/**` touched, commit
incl. the ADR with `(TASK-865.N)`, push, set status).

It returns: `{ done, task, status, commit, srcTouched, featureSummary, adr, ... }`.
`done:true` means every task is Done or blocked on field-validation / unmet deps —
the migration loop is complete (remaining field-validation gates await hardware soak
under epic TASK-865).

## After the engine returns — Discord (alpha-channel policy)

2.0.0+ is the **alpha** line. Test/publish only in **#alpha-testing** and
**#dev-sat-mqtt**. Never #beta-testing.

- **Always**, post a one-line task completion to **#dev-sat-mqtt**
  (`1105556725714649128`): task id, title, new status (In Review / Done), commit.
- **If `srcTouched` is true** (a prerelease bump happened = a semver feature step),
  also post a short feature summary (`featureSummary`) to **#alpha-testing**
  (`1514720723980259460`): the new prerelease tag + 1-2 lines on the new
  feature/improvement. Facts only (changelog + what changed); no "non-production"
  prescriptions.
- If `done:true`: post a one-line "migration loop complete" note to #dev-sat-mqtt.

Use the `mcp__discord-mcp__*` tools. Do NOT announce builds/zips to Sergeant D
directly (the maintainer handles builds with him).

## Notes
- Status semantics: hardware/user-sign-off ACs cannot be self-certified → the task
  ends **In Review**, not Done. Only docs/build-config tasks with no field-validation
  AC (seq 1, 3) may flip to Done. The maintainer accepts the Proposed ADRs and
  closes field-validation under the epic.
- Worktree: all work happens in `OTGW-firmware-esp32s3-async` on branch
  `feature-2.0.0-esp32s3-async`. Never touch `dev` or the other 2.0.0 branch.
