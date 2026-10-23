---
name: implement-next-task
description: Use for "pak de volgende taak", "implement next task" or working the 2.0.0 backlog autonomously. Continuous cycle: pick the next actionable task, announce it in #alpha-testing, design, self-review with the advisor, implement with old-vs-fix evidence, reviewer team, Done only with evidence, report the build, post the combo zip to #alpha-testing, repeat.
---

# implement-next-task: the continuous backlog cycle

One invocation works through the backlog task by task until nothing is actionable.
The main session drives every step itself; subagents do the coding, running and
reviewing. Maintainer decisions behind this skill (2026-10-20, TASK-1275):

- After the self-review you implement directly. The announcements in #alpha-testing
  are notices, not approval requests.
- The cycle continues: after a task is finished, pick up the next one without asking.
- This replaces the old TASK-865 drain engine, archived at
  `.claude/workflows/archive/implement-next-task-865-drain.js` (do not run it; it only
  knows `task-865.*`).

CLAUDE.md stays authoritative. This skill only orders its rules into a cycle; where
they disagree, CLAUDE.md wins, with one deliberate exception: inside this cycle the
CLAUDE.md "share the plan and WAIT for approval" step (Task Management step 4, and the
plan-approval exception under Auto-advance) is replaced by the self-review of step 3
plus the #alpha-testing notice of step 4. Do not stop to wait for approval of the plan.
Stopping is still required for the actions that always need explicit permission (PIC
flash, release, push to `main`, force-push, merging another person's work).

## Models

The main session (the model the maintainer chose with `/model`) owns design, the
advisor calls, the final judgement on evidence, and every Discord post. Delegate the
rest with the `model` parameter of the Agent tool:

| Work | Model | Why |
|---|---|---|
| Implementation (code, tests, docs edits) | `sonnet` | Strong coder, faster and cheaper. |
| Running build.bat, evaluate.py, tests, bench scripts and reporting raw output | `haiku` | Mechanical; must quote output verbatim. |
| Reviewer team | `sonnet` per lens, plus one `opus` reviewer on correctness | Diverse lenses, one strong critic. |

A weaker model claims PASS more easily. Never accept a subagent's verdict on its
own: read the raw evidence yourself (exit code, the SUCCESS line, artifact
timestamps, test counts, the before/after values) before you tick an AC.
Subagents implement and report; they do not bump, commit or push. The main session
does those, one at a time (shared `.pio/` and git index).

## Step 0: tester questions first

Before each new task, run the `/backlog_discord` check (it honours its own 5-minute
early exit). A direct question or bug report from a tester in #alpha-testing is
answered before new work starts. A new bug report becomes a task and goes through
the normal selection below.

## Step 1: select and announce

1. First look at tasks already `In Progress` with assignee `@claude` (any milestone).
   Resume one only when its remaining open ACs can be evidenced **now** (the hardware
   it needs is connected, the decision it waits for is recorded). Those are nearly
   done and cheap to finish, so they go before new work. Among several, take the
   highest priority, then the lowest number. A blocked one stays as it is; do not
   re-announce it.
2. Otherwise pick the next new task with this rule:
   - milestone 2.0.0, status `To Do`;
   - highest priority first (high, medium, low), then the lowest task number;
   - **skip** a task whose ACs need hardware that is not available now, a design
     decision that is not recorded in the task notes, input from a tester, or an
     action that needs explicit maintainer permission (PIC flash, release, push to
     `main`, force-push). Record in the task notes why it was skipped.
   - A task waiting for Sergeant D's OK (see step 4) is resumed first once his answer
     is recorded; until then it is skipped.
   - Check memory and the task notes for decisions already taken.
   - `backlog task list` can return empty (known CLI bug): read the frontmatter of
     `backlog/tasks/task-*.md` (status, priority, milestone, assignee) instead.
   - Check which boards are connected before deciding a hardware AC is blocked
     (USB: `Get-PnpDevice -PresentOnly`; OTGW32 VID/PID 303A:1001 on COM4; network:
     router ARP for its MAC), see the bench memories.
3. First action on the chosen task, before reading code:
   `backlog task edit <n> -s "In Progress" -a @claude`.
4. Post to #alpha-testing (`1514720723980259460`): which task you pick up and, in one
   or two sentences, what it is about.
5. If no task is actionable: post nothing, report the blocked tasks and why to the
   maintainer in the session, and stop the cycle.

## Step 2: design

1. Read the task, its notes and the code it touches (follow the CLAUDE.md "what to
   read" table and the C4 docs for the owning component).
2. Run `/adr-kit:context <topic>` and honour the Accepted ADRs it returns.
3. Non-trivial new functionality: look for an open-source solution first and record
   what was considered (project rule).
4. For a bug: decide how you will **reproduce** it (host test, frame replay,
   simulator, bench) before you design the fix.
5. Write the plan into the task: `backlog task edit <n> --plan "..."`. Include the
   files, the approach, the verification per AC and the cross-line question (does it
   also belong on `otgw-1.x.x`? usually not).

## Step 3: critical self-review

Review the design as if someone else wrote it: missed callers and readers, other
platforms and boards (`HAS_*`), old-firmware/new-UI mixes, settings migration, the
2-concurrent-API rule, PROGMEM/no-String/no-ArduinoJson, MQTT/REST/HA contracts. Then
call the **advisor** with the plan (no advisor tool in this session: run one `opus`
review agent on the plan instead). Adapt the plan to findings that hold; if you
reject one, write down why. An architectural change, a new dependency or an API/MQTT
contract change also gets a Proposed ADR via adr-kit (never self-accepted).

## Step 4: announce the plan

Post to #alpha-testing: task id, what you will change and how you will prove it. One
short message. It is a notice; do not wait for a reply.

**Exception: SAT control behaviour (maintainer decision 2026-10-20).** When the task
changes how SAT controls the boiler (setpoint, PWM, PID, cutoffs, modulation, defaults
of SAT control settings), post the plan to Sergeant D (`<@821036882946424852>`) asking
for his advice and his OK, name the open points, and do **not** implement until he
agrees. The plan quotes the thermo-nova code for that area with `file:line` citations at the
pinned commit, as the "SAT ground truth" block in CLAUDE.md requires; a plan without them is
not ready to post. Record the wait in the task notes, leave the task `In Progress`, and continue
the cycle with the next task that does not change SAT behaviour. When his answer
arrives (via `/backlog_discord`), record it and resume the task. SAT changes without a
behaviour choice (logging, UI, graphs, docs) do not wait.

## Step 5: implement, verify, collect evidence

1. Bug: reproduce it first and keep the red result (old code). Feature: write the
   check first where you can.
2. Implementation agent (`sonnet`): give it the plan, the files and the exact rules
   that apply; it edits, adds tests, updates docs, and does not build, bump or commit.
3. Run the gates (main session or a `haiku` runner, one build at a time):
   - `build.bat` by its full path (firmware + filesystem, esp32-combo); add
     `--target all` when `HAS_*`, `boards.h`, platform headers or `platformio.ini`
     changed. Check the SUCCESS line and fresh artifact timestamps; exit 0 alone is
     not proof.
   - `python evaluate.py --quick` (no new FAIL) and the relevant tests in `tests/`.
4. Prove the behaviour: the same reproduction red on the old code and green on the
   fix. Use the boiler emulator, frame replay or host tests as simulation; use the
   bench when it is available (OTGW32 test unit, WiFi OTA through safeboot; save
   `/api/v2/settings` first; never flash or reset a PIC without an explicit order).
   For UI changes take screenshots before and after.
5. Append evidence to the task notes as you go and tick each AC only with evidence
   from this session (`--check-ac <i>`).

## Step 6: reviewer team

When the implementation is complete, run reviewers in parallel on the uncommitted
diff, each with its own lens and told to refute, not to approve:
- correctness end to end (`opus`);
- project rules and regressions: CLAUDE.md rules, binding ADRs, other callers, docs
  that must change (`sonnet`);
- tests and evidence: does the test call the real code, is red-then-green genuine
  (`sonnet`);
- add a lens when the change needs it (security for auth/OTA, UI for web changes).

Verify every finding yourself against the code. Fix the ones that hold (implementation
agent), re-run the affected gates and tests, and note why you rejected the others. A
finding you cannot fix within the task becomes its own task.

## Step 7: Done only with evidence

Set the task `Done` only when every AC and DoD item has evidence from this session,
the gates are green and the review findings are resolved:
- write `--final-summary` naming the evidence per AC, and what was not verified;
- bump with `bin/bump-prerelease.sh` when a firmware path changed
  (`src/OTGW-firmware/**`, `src/libraries/**`); when the bump would also touch
  unrelated uncommitted source files, `git stash` those first;
- one commit per task with a descriptive title, explicit `git add <paths>` (never
  `git add -A`), never `--no-verify`; push to `origin/dev` once build and evaluator
  are green (docs-only commits may skip both gates);
- commit and push directly on `dev`, no sub-branch (maintainer directive 2026-06-19).
  A worktree lane only for genuine parallel work, deleted right after the merge
  (`git branch -d`, `git push origin --delete`, `git worktree remove`);
- every `TASK-NNN` in a commit message needs its task file staged (commit-msg hook);
- run `graphify update src` after source changes; log fixed bugs in
  `.wolf/buglog.json` and new learnings in `.wolf/cerebrum.md`.

If an AC cannot be evidenced (tester, hardware, maintainer sign-off): leave the task
`In Progress`, name the blocking AC in the final summary and the notes, and move on.

## Step 8: report in #alpha-testing

Post what was done: task id, the change in plain words, the evidence (old vs new) and
the build tag (`2.0.0-alpha.N+hash`). Rules for that channel: English, answer first,
never name the OTGW32 board, do not tag Sergeant D on build announcements, and say
plainly what is not verified yet.

## Step 9: leave the build ready

After the push, build once more from the pushed HEAD with `build.bat` so the githash
in the file names matches the code. Leave
`build/OTGW-firmware-esp32-combo-<version>+<hash>-flash.zip` (the complete package:
`.ino.bin`, `.littlefs.bin`, USB images, flash tools, capture scripts, READMEs) and
give its full path to the maintainer in the session. Then post it to #alpha-testing
(maintainer decision 2026-10-23, TASK-1313):

    bash scripts/discord-post-build.sh -m "TASK-NNN: <one-line change>"

Run it through the Bash tool (Git Bash); a bare `bash` from PowerShell can resolve to
WSL, where `LOCALAPPDATA` is unset and the script refuses. It posts through the webhook in
`%LOCALAPPDATA%\OTGW-capture\discord-alpha-webhook.url` (never print or commit that
URL) and refuses (exit 2) when the zip githash is not HEAD, when `src/` has
uncommitted changes, or when the zip is above 10 MB. A refusal or a failed post is
reported in the session; never work around it (no hand-renamed zip, no MCP upload).
Restore `version.h` / `data/version.hash` build-stamp churn afterwards
(`git checkout --`), it is not a bump.

## Then: the next task

Go back to step 0. Stop when:
- step 1 finds nothing actionable;
- the maintainer says stop;
- a failure repeats on the same task (two failed review rounds, a gate that keeps
  failing): leave the task `In Progress` with notes and report it;
- a rate limit or tool failure makes progress impossible.

When the cycle stops, give the maintainer a short summary in the session: tasks done
(with commits and tags), tasks left In Progress and why, the ready zip path, and what
needs their decision.
