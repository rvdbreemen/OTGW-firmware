---
name: implement-next-task
description: Drive the autonomous 2.0.0 ESP32-S3-only async + FreeRTOS migration (epic TASK-865). Runs a continuous workflow that audits stuck/finishable backlog tasks, then drains every actionable async-esp32s3 task back-to-back (implement -> build/eval -> adversarial review -> commit/push -> Discord) without waiting between tasks, then runs ONE end-of-loop ADR-evaluation pass that drafts Proposed ADRs for the run's architectural decisions, and can fan out to parallel worktree lanes. Use for one drain run of the migration loop (cron or manual).
---

# implement-next-task

Drives the autonomous 2.0.0 ESP32-S3-only async + FreeRTOS migration (epic
**TASK-865**, ADR-123/128). The deterministic multi-agent engine is the workflow
script; it is **continuous and self-announcing** — one invocation drains as many
tasks as it can.

## Run (single lane, default worktree)

> Branch model changed 2026-06-20: the 2.0.0 async line was promoted to **`dev`**
> (the former `feature-2.0.0-esp32s3-async`). `dev` is the default working line and
> the default worktree (`D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware`); the
> old async worktree is gone. `otgw-1.x.x` is the 1.x maintenance line; `main` is
> always the latest public release.

```
Workflow({ scriptPath: "D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware/.claude/workflows/implement-next-task.js" })
```

The engine, in one run:
1. **Audit** — captures the run's start commit (`git rev-parse HEAD`, the stable
   `startHead..HEAD` range the ADR-Evaluation pass diffs later), then reads every In
   Progress / In Review async-esp32s3 task. Resets STUCK ones (abandoned partial edits
   from a transient abort) to To Do; moves FINISHABLE ones (all remaining ACs
   build/evaluator-verifiable, committed + green, no hardware AC) to **Done**; leaves
   correctly-In-Review (field-validation pending) ones alone. It decides this itself —
   no permission prompt.
2. **Drain loop** — repeatedly: **Select** the lowest actionable task (deps Done/In
   Review; single-flight-guarded per worktree) -> **Implement** (coding agent, ACs +
   CLAUDE rules) -> build (`python build.py`) + `evaluate.py --quick` -> **adversarial
   Review** (one fix pass) -> **Land** (set status In Review / Done, bump prerelease if
   `src/**` touched, commit, push — **no ADR in the per-task commit**) -> **Announce**
   (Discord, below). Then it advances to the **next task immediately** — it does NOT
   wait for the next tick.
3. **ADR Evaluation (end of run)** — after the drain loop exits (whether it drained
   the backlog or `break`s on a transient failure), if any task landed this run a
   single pass reviews the **whole-run diff** (`startHead..HEAD`) plus the landed task
   bodies, dedups the **architectural decisions across all tasks** into one entry each,
   has the JS assign the next free ADR numbers (no LLM number-picking → no collision),
   drafts one **Proposed** ADR per decision (`adr-kit:adr-generator`, never Accepted),
   runs the self-accept governance guard over them, commits them in **one
   `docs(adr): ... (TASK-865)`** commit (docs-only, no bump), pushes, and posts a
   maintainer-facing "N Proposed ADRs drafted for review" note to #dev-sat-mqtt. Each
   ADR's References section cites the contributing task ids + their commit hashes (the
   traceability that replaces riding the ADR inside the code commit). **This is the
   only place the loop authors ADRs** — moved out of the per-task path 2026-06-24
   (TASK-928) so one evaluation sees cross-task decisions and the maintainer reviews a
   coherent batch instead of a per-task drip.
   - **Process-death recovery.** Every **Land** stamps the task note `ADR-PENDING`;
     the ADR-Evaluation pass stamps `ADR-EVALUATED: <ADR-ids|none>` ONLY on confirmed
     completion (ADRs drafted + committed, or nothing architectural to draft). If the
     pass dies (rate-limit) before stamping, the `ADR-PENDING` marker survives in git,
     so the NEXT run's Audit returns the task in `adrOrphans` and this pass sweeps it
     up alongside its own tasks. Enumerate greps `docs/adr/` for the task ids first and
     skips any decision already documented, so a retry after a partial death never
     double-drafts. (The same marker drives the parallel-lane handoff: a `skipAdrEval`
     lane leaves its tasks `ADR-PENDING` for the integrating run to draw in.)
4. **Stop** when nothing is actionable, on a transient agent death (rate limit — it
   cleans up the task back to To Do and ends so a later run retries), or when a task
   fails review twice (flags it for attention). The ADR-Evaluation pass still runs for
   whatever did land before the stop, plus any `ADR-PENDING` orphans from a prior dead
   run.

Returns `{ done, completed: [...], count, endReason, adrsDrafted: [...] }`. `endReason`
tells you whether it drained the backlog, hit a transient rate-limit, or needs
attention on a task; `adrsDrafted` lists the Proposed ADRs the end-of-run pass drafted
(awaiting maintainer acceptance).

Parallel lanes (`args.skipAdrEval`, set automatically alongside `skipBump`) defer the
ADR-Evaluation pass to serial integration so two lanes never race on ADR numbers; the
integrating run drafts the ADRs once after the merge.

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

Parallel-lane setup: `git worktree add <dir> -b <sub-branch> dev`,
copy `other-projects/` + `src/libraries/OpenTherm` + `src/libraries/SimpleTelnet`
into it (the `other-projects` submodule remote 404s, so copy from a populated
worktree), then launch with the args above. **Integration is serial**: when a
sub-branch lane finishes, merge it into `dev`, run
`bin/bump-prerelease.sh` once for it (the deferred semver tag), resolve any
version-banner conflicts, then announce. **Then DELETE the merged sub-branch
immediately**: `git branch -d <sub-branch>`, `git push origin --delete <sub-branch>`,
and `git worktree remove <dir>`. Never leave a merged sub-branch (or its worktree)
lying around — that is the `-jsonemit` / `-chunked` proliferation cleaned up 2026-06-19
(maintainer directive: one async branch). Caution: parallel lanes that touch
overlapping files (loop edits in `OTGW-firmware.ino`, `platform_esp32.h`, version
banners) conflict at merge — only fan out file-disjoint tasks.

## Notes
- Field/hardware ACs are never self-certified -> those tasks end **In Review**; the
  maintainer accepts the Proposed ADRs and closes field-validation under the epic.
- **Single lane (the default) commits and pushes DIRECTLY on `dev`
  in `OTGW-firmware` — NO sub-branch.** `dev` IS the 2.0.0 async line now (one-branch
  maintainer directive 2026-06-19, branch model change 2026-06-20). Sub-branches/worktrees
  exist ONLY for genuine parallel lanes and are deleted right after merge (see Parallel
  lanes above). Never touch `otgw-1.x.x` (1.x maintenance) or `main` (public release).
- Rate-limit storms: the engine ends a run on a transient agent death after cleaning
  up; just re-run (cron does this every 30 min). The abort-cleanup is itself an
  agent and can die under heavy limiting — the launching session then cleans up
  manually (revert partial edits + reset the task To Do).
