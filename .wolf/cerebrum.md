# Cerebrum

> OpenWolf's learning memory. Updated automatically as the AI learns from interactions.
> Do not edit manually unless correcting an error.
> Last updated: 2026-08-23

## User Preferences
- Build firmware FOREGROUND, serially; verify `firmware.bin` mtime+size. Never trust `build.py` exit 0.
- Version bump = one changeset via `bin/bump-prerelease.sh` (stages version.h + version.hash + ~42 banners).
- OTGWSerial is the ONLY editable vendored lib; all other `src/libraries/**` read-only.
- ADRs: never edit an Accepted ADR; write a new Proposed "…(Amends ADR-XXX)".

## Key Learnings

### Build / tooling
- **`firmware.bin` mtime+size is the only trustworthy build signal.** `build.py` exits 0 on per-env compile failure; require literal `Successfully created ESP32S3 image` / `SUCCESS`. Concurrent runs share `.pio/build` → 0xC0000142 or `OTGW-firmware.ino.cpp: No such file or directory`. Recovery: `rm -rf .pio/build/<env>`, rebuild solo. Only `buildfs` parallelizes. LTO link needs ~2GB RAM. esptool v5 cp1252 crash → prefix `PYTHONUTF8=1`. Never pipe build output through `Select-Object -First N`. `build.sh` self-bootstraps Python/pip.
- App-only flash preserving WiFi+settings: `esptool write-flash 0x0 bootloader 0x8000 partitions 0xe000 boot_app0 0x10000 firmware` — NOT merged-full @0x0 (wipes NVS).
- On dependency build failure, grep the FIRST `error:` line; a conflict marker in `version.h` produced ~40 bogus AceTime errors. (bug-150)
- Firmware epoch is AceTime `time(nullptr)`, never TimeLib `now()`.
- Combo flash budget (ADR-127): esp32-combo at 98.4% of the 1.875MB app slot; `-flto` already on.
- Dict-spread in `TARGETS` silently inherits later keys (`slug`) → artifact name collisions. Set explicit values. (bug-120)
- Vendored libs compile WITHOUT `boards.h` — `#if defined(PIN_XXX)` always takes fallback. Pass pins via per-env `-D` build_flags; verify in `.pio/build/<env>/…/<lib>.cpp.d`. (bug-119)
- Git Bash `ps` cannot see native Windows processes; use `tasklist //FI "PID eq <pid>"`.

### Git / backlog / ADR
- **Fetch before any cross-branch gap analysis.** `git fetch` + `git rev-list --left-right --count HEAD...origin/<branch>` FIRST; a 6-commit-stale tree produced 3 dead tasks.
- `backlog task edit` AUTO-COMMITS in the dev worktree (wipes your index); not in the 2.0.0 tree. Never leave staged work during CLI calls, and don't re-commit ADR/task files afterwards — check `git log` first. Dev commit-msg hook demands a task file for EVERY `TASK-NNN` token — don't cite 2.0.0 ids in dev commits. Exemptions: `chore(meta|release|housekeeping|daily-report):`; version-banner housekeeping needs `OTGW_BUMP_HOOK_DISABLE=1`.
- `other-projects/` is a submodule of private `rvdbreemen/OTGW-other-projects`. Commit inside it, then the gitlink.
- **ADR status has no canonical line.** Read in priority order: frontmatter `status:` → `## Status` prose → inline `**Status:**`. NEVER grep bare "Proposed" (a naive grep claimed 31, real was 5). Flip only the anchored line, `git diff` before commit. (bug-036)
- An Accepted ADR can contradict itself on surfaces it never enumerated (ADR-167: delete counters vs preserve observability, while counters fed MQTT/HA/REST/UI). Grep each named symbol for consumers before executing a removal list.
- Check frontmatter milestone + tail of Implementation Notes before picking up an old task (TASK-687 parked, milestone 3.0.0).
- End-of-loop ADR evaluation (TASK-928): one pass reviews `startHead..HEAD`, dedups decisions, assigns numbers from glob-max, drafts Proposed ADRs, commits docs-only. `ADR-PENDING` / `ADR-EVALUATED:` markers give process-death recovery. Acceptance stays manual.

### Firmware / code
- `hd_drip_cooldown_skip` is post-status-burst pacing; the heap-pressure counter is `hd_drip_slowmode`.
- `platformMaxFreeBlock()` on ESP32 = `ESP.getMaxAllocHeap()`.
- **Removing a published REST/MQTT field is coupled to its frontend consumer.** Backend removal + JS rewrite must ship in ONE `build.py` (firmware + LittleFS). Dropping `picavailable` made `#tabPICflash` unreachable.
- `MQTT_HA_SENSOR_COUNT` must EXACTLY equal the row count of `mqttHaSensors[]`; a short count silently darkens the trailing OT id. Update `mqttHaSensorIndex[]` offsets together. (bug-088)
- `satSendStatusJSON()` is a CROSS-FILE open stream: `satBLESendStatusJSON()` (SATble.ino) appends into the same open map. Migrate SATcontrol.ino + SATble.ino together.
- Settings page: `.settings-group-body` is a 2-col grid; full-width sub-panels MUST use `grid-column: 1 / -1`; `normalizeSettingsLabelWidth()` measures only real row labels. (bug-076)
- SAT REST writes: `/sat/enable`, `/sat/target`, `/sat/preset`, `/sat/mode`, `/sat/settings/dhw_setpoint|dhw_enable`, `/sat/settings/simulation`. Bare text/plain bodies. Classic `sat.js` is ground truth.

### Dead-code / refactor discipline
- Scope reference searches wider than `src/`: `evaluate.py`, `tests/`, `docs/` give different answers.
- Count symbol FAMILIES (all `print_*`), not individuals — that's how `print_flag8flag8` surfaced.
- The `#else` branch of a `HAS_*` flag is NOT dead code; only remove when the flag is an invariant platform discriminator.
- 100% survival in an adversarial verify = rubber-stamping. Spot-check before acting (43/43 survived; 2 false positives).
- Brace-match when scripting deletion; never cut on a bare `}`. Run `node --check` after.

### Misc
- `DesignSync` (claude_design MCP) is NOT inherited by sub-agents — do design reads in the main thread. Check whether a design already shipped before rebuilding.
- LOLIN S3 Mini D1-mini footprint (outer row): RST=EN, A0=2, D0=4, D1=36(SCL), D2=35(SDA), D3=18, D4=16, D5=12, D6=13, D7=11, D8=10, TX=43, RX=44.
- Concurrent edits to `v2.html`: `git diff -U1` to split coalesced hunks, filter foreign ones, `git apply --cached --unidiff-zero`.

## Do-Not-Repeat
- **2026-08-01**: Never trust a zero-refutation verify pass; never delete a `HAS_*` `#else`.
- **2026-07-31**: Never analyse branch gaps against an unfetched tree.
- **2026-06-24**: Never flip ADR status by replacing the first bare "Proposed" (corrupted 21 files).
- **2026-06-14**: Never amend an Accepted ADR in place; write a new Proposed one. StructuredOutput: `adrAction="created"`, stage the README index edit too.
- **2026-06-13**: When reviving a removal-commit, walk ALL hunks of the removal diff as a checklist. (bug-121)
- **2026-05-30**: Never pull OTGWSerial into the platform abstraction.
- **2026-05-29 / 2026-08-01**: Never run two builds concurrently in one worktree.
- **2026-05-26**: Don't re-commit ADR/task files after `backlog task edit`.

## Decision Log
- 2026-05-05: MQTT discovery drip is platform-aware on 2.0.0 — ESP8266 keeps `HEAP_LOW`/2000ms; ESP32 uses a shorter burst cooldown and enters slow-mode only when free heap AND largest block are both low.
- 2026-06-12: Maintainer grants OTGWSerial as the one editable vendored lib (TASK-862).
- 2026-06-24: The implement-next-task loop drafts ADRs once at end-of-run, not per task.