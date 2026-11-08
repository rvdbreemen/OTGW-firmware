# Worktrees, parallel sessions and cross-line work

Split out of CLAUDE.md by TASK-1631.

The everyday state is a **single working tree on `dev`**; a second worktree is only needed when actively maintaining the 1.x line. **The paths below assume the Windows root `D:/Users/Robert/Documents/GitHub/RvdB/` (Mac-canonical equivalent: `~/Library/CloudStorage/OneDrive-Belastingdienst/Documenten/GitHub/...`).**

| Worktree dir | Branch | Purpose |
|---|---|---|
| `.../GitHub/RvdB/OTGW-firmware` | `dev` | **DEFAULT working tree.** 2.0.0 ESP32-S3-only async + FreeRTOS line (epic TASK-865, ADR-123/128; ESP8266 dropped). |
| (add on demand) | `otgw-1.x.x` | 1.5.x/1.6.x maintenance/LTS line (the former `dev`). Spin up a worktree only when patching 1.x. |
| (release only) | `main` | ALWAYS the latest public release. Never a working tree; touched only during the release flow. |

**One line, no stray sub-branches (maintainer directive, 2026-06-19, still in force).** Single-lane work commits and pushes DIRECTLY on `dev`, NO sub-branch. Create a sub-branch/worktree ONLY for a genuine parallel lane, and DELETE it immediately after merging back: `git branch -d <sub>`, `git push origin --delete <sub>`, `git worktree remove <dir>`. Codified in `.claude/skills/implement-next-task/SKILL.md`.

**Rule: don't cross lines inside one tree.** Work targeting `otgw-1.x.x` belongs in its own worktree. Never `git checkout otgw-1.x.x` inside the `dev` tree to do 1.x work: it defeats the split and risks losing in-flight changes.

**Windows gotcha:** `git worktree remove` can fail with "Function not implemented" (ENOSYS) when a worktree holds internal symlinks (venv `lib64`, toolchain). Remove junction-safe with `cmd //c rmdir /s /q "<path>"` then `git worktree prune`, never a blind recursive delete that may follow a junction's target.

Verify with `git worktree list`.

**Backlog.md: always use the CLI, never the MCP server.** The `backlog` MCP server (`mcp__backlog__task_*` tools) inherits the launching session's working directory and indexes only that single worktree. Tasks living in a sibling worktree are invisible to `mcp__backlog__task_search`, and `mcp__backlog__task_view` returns cached/stale content for cross-tree tasks (verified 2026-05-05: MCP kept returning the pre-edit "In Progress" snapshot of TASK-514 long after a CLI edit had marked it Done on disk in the 2.0.0 tree). Mixing CLI and MCP on the same task is fragile because MCP caches and does not reflect CLI-side writes without a server restart. **Use `backlog task ...` CLI for every read, edit, create, complete, and archive in this project.**

**CLI cross-tree behaviour.** The backlog now lives in the `dev` tree's `backlog/tasks/` — all 2.0.0 (TASK-865.x and friends) and legacy task files are here. If you spin up a second worktree for `otgw-1.x.x`, remember `backlog task edit` only writes to the worktree where the task file actually lives: if an edit returns `Task not found`, `find` for `task-<id>*` across both worktrees and run the edit from the worktree that holds the file.

### One worktree per concurrent session — never share a working tree between two live sessions

Two Claude (or Codex / Gemini) sessions doing git operations in the **same** worktree share one git index and one set of working-tree files. This is fragile and was observed to fail (2026-05-29):

- **Shared-index corruption.** Concurrent `git add`/commit from two sessions corrupts the index: `error: invalid object <hash> for '.claude-plugin/marketplace.json'` / `.gemini/settings.json`, then `error: Error building trees` — every commit is blocked. Recovery: `git reset` (mixed, no args) rebuilds the index from HEAD without touching commits or the working tree, then re-stage only your files and commit. A targeted `git reset -- <path>` only fixes one entry at a time (whack-a-mole when several are corrupt).
- **`.pio` build collisions.** Two `python build.py` runs in the same tree clobber `.pio/build/`, producing a spurious ESP32 LTO link failure (`undefined reference to setup()/loop()`) **even though `build.py` exits 0** (it does not propagate the per-env PlatformIO failure). Recovery: `rm -rf .pio/build/esp32` and rebuild solo. A background agent that backgrounds its own build and then ends leaves an orphaned build that collides with the next one — have agents NOT build, and let the main thread build once after they finish.
- **EOL churn is normal, not work.** On Windows every git touch flags `LF will be replaced by CRLF` on ~90 files that show as modified with zero content change. `git diff --ignore-cr-at-eol` reveals the real set; after committing real work, `git checkout -- .` restores the EOL-only files to leave a clean tree.

**Prevention.** Give each concurrent session its OWN worktree (`git worktree add ../wt-<topic> <branch-or-HEAD>`), OR run parallel work as **background sub-agents inside ONE session** (they serialise git through the single main thread, and the convention here is that sub-agents implement + report but do NOT commit/bump/build — the main thread does that once after reviewing). Do not run two top-level sessions in the same tree expecting clean commits.

### A new worktree gets its submodules by itself (TASK-1725)

`git worktree add` does not initialise git submodules, so a fresh worktree used to have empty `src/libraries/OpenTherm`, `src/libraries/SimpleTelnet` and `other-projects` directories: the C++ host harnesses then failed on a missing `OpenTherm.h`, and the SAT ground-truth and PIC reference reads found nothing. Since TASK-1725:

- **`.githooks/post-checkout`** runs on a worktree's first checkout (git passes the null ref as the previous HEAD; a normal branch switch is left alone) and runs `git submodule update --init` for every submodule the commit records, at the recorded commit. It borrows the objects from the main worktree's copy when that copy is a full clone (`--reference ... --dissociate`, so `other-projects`' 77 MB come from disk and the new copy does not depend on the main tree afterwards); a shallow copy (the main tree's OpenTherm) cannot serve as a reference, so that one is fetched from its URL. A failure never blocks the checkout: the hook prints the command to run. Measured: a fresh worktree took 89 s with all three submodules populated. Bypass: `OTGW_SUBMODULE_HOOK_DISABLE=1 git worktree add ...`.
- The hook only runs where `core.hooksPath` points at `.githooks` (set once per clone, see `.githooks/README.md`). In this repo `core.hooksPath` is the main tree's absolute `.githooks` path, so every worktree uses the hooks of the main tree's checkout: a hook change takes effect once the main tree is on a commit that has it.
- **`build.py` and `scripts/run_tests.py`** stop before any work when OpenTherm or SimpleTelnet is missing, with one message naming the submodule and the command `git submodule update --init -- <path>` (`scripts/submodule_check.py`). A missing `other-projects` is only a note in `run_tests.py`: the tests that need it skip, and CI has no access to that repository (`.github/workflows/build.yml`).
- An existing worktree made before TASK-1725 is fixed with `git submodule update --init` in that worktree.
- Until the hook is live in the main tree, or with hooks off, run `git submodule update --init` in a new worktree before `python scripts/run_tests.py` or `build.bat` (TASK-1723). A fresh worktree also needs its own `.build-venv` before `build.bat` works (TASK-1723).

### Cross-worktree work — ask first, then plan once, then parallelise

Whenever you take on a bug fix, feature change, or architectural decision, **explicitly ask yourself**: *does this also need to land on the other line?* For `dev` (2.0.0) ↔ `otgw-1.x.x` (1.x) the answer is **yes** when the change touches:

- Any file under `src/OTGW-firmware/` whose name is the same on both lines (most `.ino`, `.h`, `.cpp` and the LittleFS data assets) — even when the line numbers or filenames differ between branches (e.g. 1.x's `mqtt_configuratie.cpp` is `MQTTHaDiscovery.cpp` on 2.0.0), the architectural fix usually applies to both.
- ADRs that codify a cross-cutting decision (MQTT topic shape, heap policy, discovery semantics, settings schema). The `dev` ADR and the `otgw-1.x.x` ADR have separate numbering, but the *decision* must be coherent across both.
- Anything driven by a HA-side, broker-side or PIC-side contract (HA discovery regex, MQTT retained behaviour, OpenTherm message ID semantics) — those are platform-independent and the firmware-side fix must apply on both lines.

The answer is usually **no** when the change is genuinely scoped to a feature that only exists on one line (SAT dashboard, ESP32-S3 board pinning, OTGW32 hardware bring-up, async/FreeRTOS → `dev`/2.0.0 only; 1.x-specific patch backports → `otgw-1.x.x` only). Note: with 1.x now in maintenance/LTS, most active work is `dev`-only and cross-line porting is the exception, not the default.

**If both: one master plan, two tasks, two agents.**

1. **Write ONE master plan first**, before creating any task or spawning any agent. The plan covers *both* worktrees in a single document and must contain, for each change:
   - The desired outcome in plain language (the "what" and "why").
   - The exact file(s) and line number(s) on **both** branches — they often differ (dev refactor split files differently than 2.0.0). Read each branch's source to confirm; do not assume parity.
   - Per-platform considerations explicitly called out for the 2.0.0 side (ESP8266 vs ESP32-S3 deadbands/thresholds, board-specific pin maps, conditional compilation flags `#if defined(ESP32)` etc.).
   - The full AC list each per-worktree task will inherit (build commands per target, evaluator commands, field-validation gates, ADR-acceptance gates).
   - Cross-tree dependencies and ordering — e.g. dev ADR-N must be Accepted before 2.0.0 ADR-M can be drafted; dev impl can be pushed before 2.0.0 impl is reviewed.
   - The expected commit message prefix and the push gate per branch (`origin/dev` auto-push allowed under the dev worktree's policy; 2.0.0 feature branch push needs explicit confirmation).

2. **Share the master plan with the user for approval** before any task creation. The user's "go" on the master plan replaces the per-task plan-review gate that would otherwise apply individually.

3. **Then create two separate backlog tasks**, one per worktree. Use the convention `feat-2.0.0: port TASK-N — <title>` for the 2.0.0 sibling so cross-references stay legible. Each task carries its own AC list derived from the master plan; do not over-share ACs across tasks (each agent must be able to verify its own task in isolation).

4. **Spawn two agents in parallel**, one per worktree. Each agent:
   - Has its own self-contained prompt referencing the master plan (or restating its scope in full).
   - Works exclusively in its own worktree and explicitly does not touch the other tree.
   - Reports back independently with build/evaluator/commit/push receipts.

5. **Verify both reports**, then report a consolidated summary to the user.

Do **not** sequence (dev first, then 2.0.0 only after dev is fully done) unless there's a hard ordering dependency (e.g. 2.0.0 ADR cites a regex finding from the dev ADR — in that case dev ADR must be Accepted before the 2.0.0 ADR can be drafted, but the 2.0.0 *code* impl can still parallelise once both ADRs are Accepted).

The benefit of writing the plan once is symmetry: agents see the same intent, the two ADRs cross-reference cleanly, the two commits land within minutes of each other, and the user reviews one design instead of two slightly-divergent designs.
