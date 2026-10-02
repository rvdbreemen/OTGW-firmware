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
- [2026-10-02] On a provisioning boot the telnet header's "Boot: Software reset" and "Up 00:00" do not mean a reboot after /wifisave. `state.uptime.iSeconds` counts only from loop() (doTaskEvery1s), so the blocking startConfigPortal() in setup() is not in the uptime, and the reset reason is the previous 240 s portal-timeout restart. The provisioning ran in that same boot (TASK-1130 FIX run).
- [2026-10-02] Identify the firmware on an OTGW32 without network: `esptool read-flash 0x10000 0x80000` and grep `2\.0\.0-alpha\.[0-9]+\+[0-9a-f]{7}` (4 s over USB-JTAG). The app descriptor (esp_app_desc_t) holds the arduino-lib-builder version, not ours.
- [2026-10-02] A Windows TCP probe needs a 3 s or longer budget to tell 'refused' from 'filtered': Windows retries the SYN after an RST, so a closed port on the device reports refused only after about 2.6 s and shows as 'timeout' at 1 s (bug-884).
- [2026-10-01] A 'needs the bench' label in task notes is not the AC itself. Read the AC text: when it does not demand hardware, a harness on the sliced real code (OLD vs FIX plus mutants) can verify it now. That closed TASK-1037 AC#10 (browser plus limiter oracle) and TASK-1124 AC#3 (header staging up to the AsyncWebServer response). Skip it when another AC of the same task needs the same hardware run anyway (TASK-1059 AC#3 rides on AC#11's HA restart).

### OT frame origin (TASK-1086, TASK-1185)
- **SAT simulation has two off-switches**: the edge hook `satNotifyBoilerFrameSeen()` and the `satControlLoop()` backstop that calls `satOnBoilerDetected()` whenever `satBoilerHardwarePresent()` is true. A frame that must not switch simulation off has to stay out of both; on the PIC path the gate reads `otRealBoilerSeenRecently()`, never `bBoilerState` (which counts loopback and replayed B frames on purpose).
- **Frame origin is carried per frame**: `OTdata.bLocalAnswer` (OT-Direct A and loopback B) and `OTdata.bReplayed` (the `/otgw_simulation.log` replay, queue source `OTFRAME_SRC_REPLAY`). `processOT()` folds both into `boilerEvidence` for the boiler bitmaps. `dispatchOTGWInputLine()` is the replay's entry point, so host harnesses feed live PIC lines via `enqueueOTFrame(..., OTFRAME_SRC_PIC)`, as the PIC task does.
- **`/ot-boiler.json` is format 2** since TASK-1185; a format-1 file is ignored once and rewritten. `/ot-thermo.json` stays format 1.

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
- The OpenTherm library's isValidResponse() accepts READ_ACK, WRITE_ACK and UNKNOWN_DATA_ID (OpenTherm.cpp:518-526), so status SUCCESS does not mean 'the reply carries a value'. Check the message type before treating the data bytes as a boiler value (TASK-1184).
- index.html loads ds-tokens.css -> components.css -> the scripts one at a time after DOMContentLoaded (TASK-960 loader). A browser test must waitForFunction on the state it measures; waitUntil 'domcontentloaded' races it (TASK-1182).
- test/host/build_and_run_override_reply.ps1 is the OT-Direct harness: -Suite 1178|1177|1184 with -OldVsFix -OldRev <commit before the fix>; older suites run as byte-identical regressions. A suite's verdict holds for the commit that introduced it.
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
- [2026-10-01] scripts/tests/test_load.py, test_mqtt_reliability.py, test_telnet.py, test_webserver.py and test_ws_liveload.py are live-device probes (normally run by otgw-test.py against a gateway), not host tests: a sweep of every test_*.py fails them with 'device not reachable' or a missing module whenever no gateway is online. Leave them out of a host-suite run, or report them separately as needing the bench.
- [2026-10-01] Keep an ADR's Status line alone on its line ("Proposed, YYYY-MM-DD.") with any label in a separate paragraph: adr-kit 0.57.0 `adr accept` replaces the whole first line of the Status paragraph, which cut "Guideline-level per ADR-080: ... reports each" out of ADR-180. Also keep the literal "supersedes ADR-NNN" out of an ADR's body unless that ADR is the successor: adr-lint's consistency gate counts it as the ADR's own supersession claim once the ADR is Accepted.
- [2026-10-01] Work on the otgw-1.x.x line follows the 1.x tree's own CLAUDE.md, not dev's: on 1.x a prerelease bump is release prep only (TASK-669, no per-commit bump; the open beta tag collects the work), its backlog has `auto_commit: true` (every `backlog task create/edit` makes a commit, of the whole index), and `core.hooksPath` is shared worktree config pointing at dev's hooks, so commit there with `git -c core.hooksPath=.githooks commit`. I bumped 1.x to beta.3 by the dev rule and had to revert it mid-build.
- [2026-10-01] Before calling a consequence "occasional" in a maintainer question, simulate it against the real code: a Telegraf scrape beside one dashboard under the burst-1 limiter was refused 99.4% of the time (it settles in the refused phase), not occasionally, and the maintainer reversed a decision taken on my wording.
- [2026-09-30] Before presenting a consequence to the maintainer, verify its mechanism: I nearly claimed stale unsupported verdicts mark HA entities unavailable, but ADR-142 is Rejected; they only feed the retained otgw-firmware/boiler/unsupported_msgids CSV and /api/v2/otgw/ot-support.
- [2026-09-30] A CPU-load generator built on multiprocessing must stop its workers when the parent dies: on Windows, killing or terminate()-ing the parent leaves the spawned workers running until their own deadline (20 orphans burned for 15 minutes and contaminated a "quiet host" test run). Give each worker the read end of a pipe and exit on EOF, or kill the tree with `taskkill /T /F /PID`; count `*multiprocessing-fork*` processes before trusting a quiet-host measurement.
- [2026-09-30] From Git Bash run the build as `(cd <tree> && ./build.bat --target all)`, never `cmd //c build.bat ...`: this environment sets NoDefaultCurrentDirectoryInExePath=1, so cmd answers "'build.bat' is not recognized" and a trailing `echo` makes the background task report exit 0. Check the log's first lines before trusting any build.
- [2026-09-30] Count line endings with Python bytes (`b.count(b"\r\n")`), never with Git Bash `grep -c $'\r...'`: grep reported doubled CRs on every line of 25 files that held plain CRLF, and the "fix" would have damaged a correct tree.
- [2026-09-30] "The change does not touch scripts/" does not make a scripts/ test independent of it: the fake device in scripts/tests/test_heap_soak_driver.py reads restAPI.ino, helperStuff.ino, MQTTstuff.ino and handleDebug.ino. Before calling a failure pre-existing, run it several times against a HEAD copy of every file it reads (TASK-1186: 7/8 with the change, 6/8 on HEAD).
- [2026-09-30] Never put a Python regex or escape sequence in a Bash heredoc: `[/\\]` reached Python as an unterminated set (twice this session). Write the script with the Write tool, then run the file.
- [2026-09-30] One OLD-vs-FIX sample of a browser test is not evidence. sat-settings-layout passed once on origin/dev and failed once on HEAD; I reported a regression, but 4 runs per side showed 1/4 on both (a CSS-load race). Repeat a UI test at least 3x per side before calling it a regression.
- [2026-09-30] Derive the 'changed on purpose' list of a regression suite from a byte-identical OLD-vs-FIX run, not from notes: the fixup note named C2/C7b/C7e for the UNKNOWN-caching change, and the run showed C8b too.
- [2026-09-30] Before removing workflow worktrees (`.claude/worktrees/wf_*`), prove each one's diff is in dev: `git -C <wt> diff --binary HEAD > p; git apply --check -R --ignore-whitespace p` on dev. Files that were 3-way merged or edited after integration fail that check; for those, confirm every added line exists in the dev file and trace any missing line to the commit that replaced it (`git log -S`). Only then `git worktree remove --force` + `git branch -d`.
- [2026-09-30] From Python on Windows, call the backlog CLI as `[node, C:/nvm4w/nodejs/node_modules/backlog.md/cli.js, ...]`, never `backlog`/`backlog.CMD`: subprocess cannot find the .CMD shim, and going through cmd.exe would mangle `|`, `&`, `%` and newlines in multi-line -d/--ac/--notes text.
- [2026-09-30] A commit message may not contain a TASK-NNN token for a 1.x-only task (e.g. TASK-1126): the commit-msg hook requires a staged dev task file for every token. Write "the 1.x line's banner callback" instead.
- [2026-09-30] Host harness pattern that works: slice the REAL function by anchor from `git show <rev>:<path>` (OLD) and the working tree (FIX), compile with MSVC via vswhere/vcvars (test/host/test_ot_reserved_range.py exports reusable helpers), and run OLD red / FIX green. Add /utf-8 when a sliced string holds non-ASCII (em dashes in debug text).
- [2026-09-30] Before committing `.wolf/anatomy.md`, drop every section whose header is outside the repo (`## ../`, `## C:/...`) or in a transient worktree (`## .claude/worktrees/...`): OpenWolf records every file read, including the private KennisBank vault, the memory dir and `%LOCALAPPDATA%/OTGW-capture` (secrets file names). This repo is public.
- [2026-10-02] Run the WiFiManager provisioning step (`bin/provision-wifi-ap.py`, which reads the stored WiFi password and POSTs /wifisave) only when the maintainer explicitly asks for it; never on own initiative and never hidden inside a repro script. The password stays out of the context: go through the script, which masks it, and never echo it. This replaces the 2026-09-30 absolute ban, retired by the maintainer.
- [2026-09-30] Never probe telnet :23 while a telnet reader is attached: AsyncSimpleTelnet is `<MAX_CLIENTS = 1>`, a second connect evicts the first. Use the reader's liveness as the port-23 signal.
- [2026-09-30] Before an old-vs-fix bench comparison, copy BOTH images out of `build/` (build.py deletes older artifacts), and confirm which image a device runs by the `build:` number in the telnet `D` dump; both images of one tag share the same fwversion string.
- [2026-09-23] Never `sed -i` a `.bat`/CRLF file from Git Bash: it stripped every CR from flash_otgw.bat and cmd failed with `'M' is not recognized`. Use the Edit tool, then check endings.
- [2026-09-23] When a bench symptom looks nondeterministic, list what changed OUTSIDE the device between runs (build mode, contents of build/, tool args) before instrumenting firmware. TASK-1160 was flash_otgw.bat auto-adding a littlefs image, present only after full builds.
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