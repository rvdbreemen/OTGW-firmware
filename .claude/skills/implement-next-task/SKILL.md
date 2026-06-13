---
name: implement-next-task
description: Drive the autonomous 2.0.0 ESP32-S3-only async + FreeRTOS migration (epic TASK-865). Runs a continuous workflow that audits stuck/finishable backlog tasks, then drains every actionable async-esp32s3 task back-to-back (implement -> build/eval -> adversarial review -> Proposed ADR -> commit/push -> Discord) without waiting between tasks, and can fan out to parallel worktree lanes. Use for one drain run of the migration loop (cron or manual).
---

# implement-next-task

Drives the autonomous 2.0.0 ESP32-S3-only async + FreeRTOS migration (epic
**TASK-865**, ADR-123/128). The deterministic multi-agent engine is the workflow
script; it is **continuous and self-announcing** — one invocation drains as many
tasks as it can.

## Run (single lane, default worktree)

```
Workflow({ scriptPath: "D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-esp32s3-async/.claude/workflows/implement-next-task.js" })
```

The engine, in one run:
1. **Audit** — reads every In Progress / In Review async-esp32s3 task. Resets STUCK
   ones (abandoned partial edits from a transient abort) to To Do; moves FINISHABLE
   ones (all remaining ACs build/evaluator-verifiable, committed + green, no hardware
   AC) to **Done**; leaves correctly-In-Review (field-validation pending) ones alone.
   It decides this itself — no permission prompt.
2. **Drain loop** — repeatedly: **Select** the lowest actionable task (deps Done/In
   Review; single-flight-guarded per worktree) -> **Implement** (coding agent, ACs +
   CLAUDE rules) -> build (`python build.py`) + `evaluate.py --quick` -> **adversarial
   Review** (one fix pass) -> **ADR** (`adr-kit:adr-generator` drafts a *Proposed* ADR
   for any architectural decision; never Accepted) -> **Land** (set status In Review /
   Done, bump prerelease if `src/**` touched, commit incl. the ADR, push) ->
   **Announce** (Discord, below). Then it advances to the **next task immediately** —
   it does NOT wait for the next tick.
3. **Stop** when nothing is actionable, on a transient agent death (rate limit — it
   cleans up the task back to To Do and ends so a later run retries), or when a task
   fails review twice (flags it for attention).

Returns `{ done, completed: [...], count, endReason }`. `endReason` tells you whether
it drained the backlog, hit a transient rate-limit, or needs attention on a task.

## Discord (alpha-channel policy, done INSIDE the loop)

2.0.0+ is the **alpha** line. Test/publish only in **#alpha-testing** and
**#dev-sat-mqtt**; never #beta-testing. Per landed task the Announce phase posts:
- a one-line completion to **#dev-sat-mqtt** (`1105556725714649128`): id, title,
  status, commit, prerelease tag;
- if the task bumped the prerelease (a semver feature step), a short feature summary
  to **#alpha-testing** (`1514720723980259460`) — facts only; if it returns
  "Missing Access" it notes the bot lacks access and continues (no retry).

## Parallel lanes (fan out when genuinely independent)

One worktree can only run one lane (shared git index + `.pio`). To run more tasks at
once, give each its own worktree + sub-branch and pin its task. `Select` reports
`alsoActionable` — other actionable tasks whose files are disjoint from the current
one — as parallel candidates. Dispatch a lane with `args`:

```
Workflow({ scriptPath: "<this script>", args: {
  repo: "D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-esp32s3-async-2",
  branch: "feature-2.0.0-esp32s3-async-b",
  targetTask: "TASK-865.12",   // pin (parallel lanes do not share live status)
  skipBump: true               // defer the bump; integrate + bump + tag serially at merge
}})
```

Parallel-lane setup: `git worktree add <dir> -b <sub-branch> feature-2.0.0-esp32s3-async`,
copy `other-projects/` + `src/libraries/OpenTherm` + `src/libraries/SimpleTelnet`
into it (the `other-projects` submodule remote 404s, so copy from a populated
worktree), then launch with the args above. **Integration is serial**: when a
sub-branch lane finishes, merge it into `feature-2.0.0-esp32s3-async`, run
`bin/bump-prerelease.sh` once for it (the deferred semver tag), resolve any
version-banner conflicts, then announce. Caution: parallel lanes that touch
overlapping files (loop edits in `OTGW-firmware.ino`, `platform_esp32.h`, version
banners) conflict at merge — only fan out file-disjoint tasks.

## Notes
- Field/hardware ACs are never self-certified -> those tasks end **In Review**; the
  maintainer accepts the Proposed ADRs and closes field-validation under the epic.
- All work is on `feature-2.0.0-esp32s3-async` in `OTGW-firmware-esp32s3-async` (or a
  parallel sub-branch/worktree). Never touch `dev` or the other 2.0.0 branch.
- Rate-limit storms: the engine ends a run on a transient agent death after cleaning
  up; just re-run (cron does this every 30 min). The abort-cleanup is itself an
  agent and can die under heavy limiting — the launching session then cleans up
  manually (revert partial edits + reset the task To Do).
