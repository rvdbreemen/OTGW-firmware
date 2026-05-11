# Critical Multi-Perspective Review — last 100 commits on `dev`

**Date**: 2026-05-11
**Scope**: HEAD..HEAD~100 on branch `dev` (range `f863aebe..660d4b93`).
**Tone**: deliberately critical. Findings are biased toward what is wrong, weak, or fragile. Where something is good, that is also called out but kept brief.

## TL;DR

The last 100 commits ship a coherent product story — the JIT-MQTT-discovery line of work (ADR-073) is well-scoped and well-evidenced — but the *repo hygiene around that story is poor*. Two commits add a combined ~480k lines of build artefacts. A 10 643-line `logfile.txt` is now part of the firmware tree. Sixty percent of commits are pure backlog admin. Every prerelease bump rewrites a header comment in 25-plus files for zero functional reason. There is at least one latent buffer-bounds bug in `SATcontrol.ino` that is the same shape as the one the SAT-pressure-attr removal just deleted. The HA-friendly-name normalisation has been re-fixed *four times* in this window — TASK-572, TASK-572-port, TASK-573, TASK-574 — and the underlying transform is still split-on-underscore only, which guarantees the next round will look identical.

Refactoring is needed, but only narrowly. Recommendations are at the end.

---

## 1. Inventory

| Category | Count | Notes |
|---|---:|---|
| Total commits in window | 100 | range `f863aebe..660d4b93` |
| `Update task TASK-*` / `Create task TASK-*` | **60** | pure `backlog/tasks/*.md` edits |
| `docs(*)` | 12 | ADR, CHANGELOG, README, daily-issue-report |
| `fix(*)` | 13 | MQTT/SAT/flash/pre-commit |
| `feat(*)` | 2 | JIT MQTT discovery, flash auto-download |
| `chore(*)` | 7 | version bumps, build-artefact refreshes, adr-kit |
| Merge commits | 2 | `c4d1280a`, `ac317dc4` |
| Authored by `noreply@anthropic.com` | 47 (Co-Authored) | Claude touched ~half of substantive commits |

Substantive code commits: 13. The other 87 are housekeeping, docs, version cascades, or auto-generated noise.

---

## 2. Perspective A — Code quality & ESP8266 constraints

### A1. **CRITICAL** — unguarded `pos += snprintf_P(...)` in `SATcontrol.ino:2272-2348`

The SAT climate-attributes block (`fix(sat) ee37052`, beta.2) builds a 512-byte JSON blob with the canonical-but-unsafe pattern:

```cpp
static char climAttrBuf[512];
int pos = 0;
pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR("..."));
// 16 more identical lines, no clamp
```

`snprintf` returns *would-have-written* length, not bytes-actually-written. Once `pos >= sizeof(climAttrBuf)`, `sizeof(climAttrBuf) - pos` is an unsigned underflow → ~4 GB, telling subsequent `snprintf` calls they have unlimited room. Result: writes past the buffer, stack/data corruption, Exception (3)/(28). On the current 16-field payload it stays around 350-400 bytes so the bug is latent, but adding even one new attribute or widening a float format can blow it.

Notably, this is the *exact* same pattern just deleted by `a1a7795 fix(sat): remove orphaned sat/pressure_health_attr` (beta.3, TASK-590). Removing the symptom did not remove the foot-gun in the sibling block ~400 lines up.

`grep -c "pos += snprintf_P" SATcontrol.ino` = **17**, with **0** guards.

**Fix**: introduce a helper or clamp every call site:

```cpp
int wrote = snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR("..."));
if (wrote < 0 || wrote >= (int)(sizeof(climAttrBuf) - pos)) { /* truncate / abort */ }
pos += wrote;
```

Better: route through `MqttJsonWriter` (already used for HA discovery in `mqtt_configuratie.cpp`) so this bypasses static buffers entirely.

### A2. **MEDIUM** — `logfile.txt` (10 643 lines, 1.6 MB) committed to firmware tree (`f863aeb`)

`.gitignore` covers `*.log` but `logfile.txt` slips through. The file is a captured telnet stream and has zero relation to firmware build; it inflates clone size by ~1.6 MB permanently and confuses repo-wide grep/lint. Either:

- delete it and add `logfile*.txt` to `.gitignore`, or
- move it under `docs/diagnostics/` next to the other evidence captures.

The commit message "Create logfile.txt" is also content-free — no `why`.

### A3. **MEDIUM** — version-stamp cascade churn in 25+ files per bump

Every `.ino`, `.h`, `.cpp`, `.css`, `.html`, `.js` carries a `**  Version  : v1.5.x-beta.N` header comment that `scripts/autoinc-semver.py` rewrites on every prerelease bump. Sample evidence:

| Commit | Subject | Files | Substantive change |
|---|---|---:|---|
| `503461c8` | reset prerelease to beta.1 | 26 | none — pure stamp churn |
| `5903b215` | refresh build artefacts post-beta.29 | 2 | only `version.h` + `version.hash` |
| `ee370527` | wire `sat/climate_attributes` | 27 | **1** source-line addition in `mqtt_configuratie.cpp` |
| `a1a7795e` | remove `pressure_health_attr` | 27 | **1** source-block deletion in `SATcontrol.ino` |
| `660d4b93` | flip Class 1/8 echo flags | 27 | **4** rows in `OTmap[]` |

`git blame` for any file gets dominated by version stamps. Reviewers chasing what changed have to mentally filter the `-/+` noise on every commit. There is one source of truth (`version.h`) — propagating it into 25 header comments is a YAGNI of the worst kind.

### A4. **LOW** — friendlyName transform is `_` → space + Title Case, still

`writeFriendlyName` (`mqtt_configuratie.cpp:1876`) splits on underscore. This is exactly why the project has now shipped four fixes in this window:

1. `fc72adfe` — fix(mqtt): HA discovery friendly name uses spaces, not underscores (TASK-572)
2. `9a53a4f1` — fix(mqtt): drop hostname + Title-Case all HA discovery friendly names (TASK-572)
3. `6d162be4` — fix(mqtt): normalise HA discovery friendly-name strings (TASK-573, 254 lines of PROGMEM string churn)
4. `4b14fa71` — same on 2.0.0 (TASK-574)

Each round added underscores to `~125` PROGMEM string literals because the transform cannot split `OEMFaultCode` / `MaxCapacityMinModLevel_hb_u8` / `DayTime_dayofweek` on case boundaries. We are doing the splitter's job by hand in 256-entry PROGMEM tables. The next time a tester says "MyEntityNameRendersWrong" we will be back here for a fifth round.

A 25-line camelCase-aware splitter in `writeFriendlyName` would have made all four fixes a one-line change. Note: the transform runs in flash-streaming context so it has to allocate nothing — but a single-pass state machine over the RAM buffer is allocation-free.

### A5. **LOW** — broker-restart heuristic is not field-tunable

`MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS = 300000UL` (5 min) is `constexpr` (`MQTTstuff.ino:58`). ADR-073's own "Negative/Risks" section warns:

> A broker without persistence that recovers in under 5 minutes would leave stale discovery state — mitigated by the fact that MQTT brokers with persistence (the common case) retain messages across restarts.

`mosquitto`'s default config does not persist retained messages. A user running default `mosquitto` who restarts it during a config tweak gets a silent JIT-discovery hole until the next OT bus event re-publishes each MsgID. There is no setting in `OTGWSettings` for this and no debug command to force it lower for testing. At minimum, expose it as a setting; better, treat any reconnect from a clean state as broker-restart-suspect and republish.

### A6. **LOW** — `iLastConnectedMs` stamp every tick is a minor write-amplification pattern

`MQTTstuff.ino:845` writes `state.mqtt.iLastConnectedMs = millis()` on every `MQTT_STATE_IS_CONNECTED` invocation while connected. State is RAM-only (not persisted) so flash-wear is not the concern. The concern is the comment ("stamp each confirmed-live tick") buries the actual semantics — what we want is "the last time we *saw* the connection healthy", and the value is read only in the `offlineMs` computation on next reconnect. The current implementation works, but the variable name + comment encourage a future maintainer to read it for the wrong reason (e.g., uptime, last-tx).

---

## 3. Perspective B — Architecture & design

### B1. JIT MQTT discovery (ADR-073, `1bb58d8f`) — the headline change

**Positive**: the ADR is one of the better-written ones in the directory. It explicitly cites the field-evidence Discord log (2026-05-08, 256 configs published at boot), enumerates four rejected alternatives with reasons, and the `Enforcement` block has `llm_judge: true` plus targeted guidance ("flag any `markAllMQTTConfigPending()` outside `doAutoConfigure()`"). The trigger table at the end is the kind of document that survives a reviewer cold-reading the file two years later. Supersession of ADR-041 is recorded correctly (immutability respected; only Status updated).

**Negative**:
- **Progressive entity discovery is a real product regression in the corner case where the boiler is in standby.** A heat pump that only chatters when actively heating might leave MsgID-related entities invisible to HA for hours after a boot. ADR-073 lists this under Negatives but does not propose a mitigation. A "kick the bus" force-poll after boot (read MsgID 0 + a curated handful) would close the gap without breaking JIT.
- **`publishNonOTDiscoveryConfigs()` is duplicated logic with `markAllMQTTConfigPending()` (lines 1311–1318 vs 1344–1354).** Both enumerate the same seven pseudo-ID set. A typo in one and not the other will silently desynchronise boot vs force paths. Factor it.
- **`verifyAccessorMarkAllMQTTConfigPending()`** at line 260 is an obvious testing seam, but the project has no test runner. Either delete it or wire it to a Serial debug command and document the contract.

### B2. SAT `pressure_health_attr` removal (`a1a7795e`, TASK-590)

Clean removal — but the commit deletes the publish and the static buffer, not the underlying state (`state.sat.fSmoothedPressure`, `iLastPressureMs`, `iLastSeenPressureMs`). If those are now only used by the deleted attr block, they are dead state taking RAM. A quick check:

```
$ grep -rn "fSmoothedPressure\|iLastPressureMs\|iLastSeenPressureMs" src/OTGW-firmware/
```

would tell us in 30 seconds whether ~20 bytes of `OTGWState` is leakable. The commit does not document this check.

### B3. SAT `climate_attributes` wire-up (`ee37052`)

Correct fix — discovery now declares the json_attributes_topic that was being published into the void. Two observations:

- **`climateIdx == 0` is a magic number.** The streaming function iterates climate entities; only the *thermostat* (index 0) gets the attrs block. If a future `climateIdx == 1` (DHW) ever needs attrs, the conditional becomes an else-if chain. Name the constant (`SAT_CLIMATE_INDEX_THERMOSTAT = 0`) — the rename is a 5-token diff and forestalls the magic-number creep.
- **The attribute set is hardcoded in two unrelated files** — `SATcontrol.ino` builds the JSON; `mqtt_configuratie.cpp` declares the discovery hook. Adding a new attribute means editing both *and* HA's downstream YAML. There is no single contract document. Either add a comment "if you add a field here, also update X" in both spots, or move the field list into a shared header table.

### B4. OT echo audit flips (`660d4b93`, TASK-571)

The reasoning is sound and explicitly grounded in a *captured telnet log with timestamps* — exactly the evidence quality we want. The "Defensive-defaults policy" written into the commit message ("when the spec does not REQUIRE the slave to echo, default to suppress") is the kind of project policy that should live in an ADR, not in commit prose. Right now, three months from now nobody will find this when deciding what default to use for a new MsgID.

Also: MsgID 1 has *direct* field evidence; 7, 8, 71 were flipped by analogy ("same shape, same rationale"). The commit message admits this and offers to flip back individually if a tester complains. That is reasonable risk management but it also means we are shipping behaviour changes without field validation on 3 of the 4 MsgIDs flipped — and the next deployment with a different boiler model becomes the validation step. A "candidate flips needing field confirmation" backlog entry would close the loop.

### B5. Branch strategy (worktree layout)

`CLAUDE.md` documents the 1.5.x / 2.0.0 split as parallel worktrees, with "if both: one master plan, two tasks, two agents." The TASK-572/573/574 pattern (dev fix + 2.0.0 port within hours) shows this works. But the cost is visible — `1d6c3c84` / `5876caaa` / `66cc24b7` / `4b14fa71` are all explicit ports tagged "port-of-N". Each port commit is hand-mirrored and exposed to drift. Over four iterations on the same friendly-name issue, the cost compounds.

If a fix touches `mqtt_configuratie.cpp` or `OTGW-Core.h` on both branches and the diff is mechanically identical, a `git cherry-pick` discipline + a CI check that flags un-ported "dev → 2.0.0" commits would be cheaper than the manual port-tasks. Not a refactor, but worth a process tweak.

---

## 4. Perspective C — Git hygiene & commit discipline

### C1. **HIGH** — 480k-line build-artefact commits hidden under fix labels

Two of the last 60 commits add enormous diffs that are not what the subject line claims:

| Commit | Subject | Files | Insertions |
|---|---|---:|---:|
| `67980f1e` | `fix(flash_otgw.bat): use registry for COM port detection` | **941** | **240 940** |
| `5903b215` (older, but resurfaced under `ac317dc4` merge) | `chore(version): refresh build artefacts post-beta.29` | 937 | 240 486 |

`67980f1e` claims to be a flash-script fix but its diff includes the entire `.claude/` skill tree, `mqtt_configuratie.cpp` (2729 lines), `OTGWSerial.cpp` (1046 lines), `restAPI.ino` (1519 lines), `tests/`, and `tools/`. It is functionally a re-add of the whole codebase masquerading as a one-line `.bat` fix. (It is in fact a merge of an old branch state — see `ac317dc4 chore: merge main back to dev (flash tool fixes + ADR-072 schema fix)` — but the merge-merge sequence has made the resulting linear history misleading.)

**Impact**: `git log -p src/OTGW-firmware/MQTTstuff.ino` jumps to a 2729-line "added" event at this commit. `git bisect` becomes useless inside this window. PR-style review is impossible.

**Fix**: in future, if a merge-back drags in artefacts, separate out the artefact-refresh into its own commit with a clear `chore(merge-back):` label, and resolve the merge as a proper merge commit (`git merge --no-ff`) rather than a flattened mega-commit.

### C2. **HIGH** — 60% of commits are `Update task TASK-*` housekeeping

Of the last 100 commits:

- 47 `Update task TASK-*` (1 file changed, 1-8 lines, in `backlog/tasks/`)
- 13 `Create task TASK-*`
- 1 daily-issue-report
- 39 other (substantive + docs + version bumps)

Every CLI-edit to a backlog task file becomes a commit. `git log --oneline` is now dominated by backlog admin. Two systemic options:

1. **Squash backlog updates into the originating code commit.** A task moving through To Do → In Progress → AC-checked → Done → Final-Summary should not produce 5 commits. Either edit the task file in the same commit as the code change (Backlog.md CLI supports this) or batch backlog updates into one daily `chore(backlog):` commit.
2. **Move `backlog/` to a separate branch or repo.** Keeps firmware history clean. Heavier lift; only worth it if option 1 fails.

Without one of these, "review the last 100 commits" effectively means "review the last 39 substantive commits because the other 61 are admin noise."

### C3. **MEDIUM** — `Initial plan (#567)` empty merge commit (`64a86679`)

A Copilot-authored merge with no payload, sitting on `dev`. If the PR was intended to seed something, the seed never landed. Either close + delete the branch, or commit content. Empty commits in mainline are a cognitive cost.

### C4. **LOW** — Two consecutive commits in the window share the same subject

`5903b215 chore(version): refresh build artefacts post-beta.29` appears twice in the window (different SHAs but identical subject line, ~3 days apart). Distinguishing them requires reading the bodies. Date-stamp the subject (`post-beta.29 (2026-05-05)`) or vary the wording.

### C5. **POSITIVE** — Co-Authored-By discipline is consistent

47 of the substantive commits explicitly co-author Claude. This is what good provenance looks like and worth preserving.

---

## 5. Perspective D — Documentation & ADRs

### D1. ADR cadence is too high relative to feature velocity

The window contains ADR-069, 070, 071, 072, 073 plus the supersession of 041. Five new ADRs and one supersede in ~3 months. Looking at the supersession chain:

- ADR-069: worldview semantics for `/thermostat`-`/boiler` subtopics (TASK-549)
- ADR-070: sibling-suffix source topic shape (TASK-552)
- ADR-071: flip discovery topic shape to sibling-suffix (supersedes ADR-070!, TASK-556)
- ADR-072: HA discovery friendly-name format
- ADR-073: JIT discovery + smart reconnect (supersedes ADR-041)

ADR-070 was superseded by ADR-071 *in the same release cycle*. That is not "stable architecture decisions" — it is iterative design captured as ADR. The kit's pre-commit judge is now arbitrating between rapidly-shifting positions. Either:

- raise the bar for ADR-Accept (the kit's gates already require Evidence + Alternatives + Consequences; the Evidence gate should require *field* evidence, not "we discussed this on Discord"), or
- introduce a `Status: Provisional` between Proposed and Accepted for in-cycle decisions that may move within weeks.

If ADRs move every two weeks they are commits with ceremony. The point of an ADR is that the cost of writing it forces deliberation; that lever weakens when supersession is routine.

### D2. **POSITIVE** — ADR-073 is exemplary

Field-evidence telnet log, four rejected alternatives, explicit trigger-table, `llm_judge` block. Use this as the template for future ADRs.

### D3. Daily-issue-report bot writes commits even when nothing happened (`2c86589e`)

> docs(issues): daily issue report 2026-05-09 — no new issues
> GitHub: no open issues updated in last 24h.
> Tweakers: feed inaccessible (network blocked).
> Discord: MCP not configured in this session.

This is a commit that *only* says "I ran and found nothing actionable, and two of three sources were not even checked." It still inflates `git log` and `docs/daily-issue-report.md` churns. Either skip the commit on no-news days, or aggregate weekly.

---

## 6. Perspective E — Security & robustness

### E1. The `logfile.txt` commit contains *operational telnet output*

Skimmed, no obvious secrets (no MQTT credentials, no JWT, no SSID/PSK). But: it contains live `mqttlastsent` timestamps, OT message captures, and an `httpServer` access log of the maintainer's home network. This is the kind of file you ship to a single tester for diagnosis, not to a public GitHub repo. Audit the file before deciding to delete vs move — if there are any credential leaks the answer is `git filter-repo`, not `git rm`.

### E2. No new attack-surface additions

The window does not add HTTPS, WSS, auth, or new external endpoints. JIT discovery only *reduces* the number of outbound MQTT publishes. REST API surface unchanged. Telnet remains the debug interface (per project policy).

### E3. `writeFriendlyName` correctly handles RAM-only input

`mqtt_configuratie.cpp:1985,2090` use `strlcpy_P` to copy the PROGMEM friendly-name into a `char friendlyName[80]` RAM buffer before passing to `writeFriendlyName`. No flash-alignment crash class (ADR-related, ESP8266 Core 3.1.2+). No truncation risk for the current longest string. Good.

---

## 7. Perspective F — Build, release, and tooling

### F1. Pre-commit hook fix-fixes (`34fbe28`, `d1b8140`, `0acc3d5`)

Three sequential commits to land a single feature (suppress adr-judge advisory spam):

1. `34fbe28` — added the redirect
2. `d1b8140` — *also* capture stderr (oversight from step 1)
3. `0acc3d5` — use `grep -a` because emoji in adr-judge output broke step 1's grep

A 5-minute test run before the first commit would have caught (2) and (3). The hook is committed but apparently not exercised before pushing. Worth adding a `bin/test-pre-commit.sh` that runs the hook over a fixture diff.

### F2. `flash_otgw.bat` fixes (5 commits in window)

| SHA | What |
|---|---|
| `1d3db7da` | auto-download binaries when missing |
| `53128e25` | only search script dir, not `build/` |
| `7c9a66ce` | fix PS1 generation via individual echo lines |
| `67980f1e` | use registry for COM port detection (and accidentally re-add the world) |
| `d541e7dd` | fix PS1 generation + COM port detection + ADR-072 schema |

Five iterations on one `.bat` script suggests the script is fragile or untested. The project has `test_flash_automation.py` (`tests/`) but it does not seem to gate these commits. Either turn it into a CI step or accept that the flash script lives outside the test envelope and budget for these iterations.

### F3. **POSITIVE** — Release process is documented

`0719e086 docs(release): prepare v1.5.0 stable release notes` and `79aeb5dd chore(release): v1.5.0 release build` look right. CHANGELOG cascaded. Beta notes archived to `docs/releases/`. The 1.5.0 release process worked.

---

## 8. Refactoring recommendations (answering "Do we need any refactoring?")

**Short answer**: yes, but only the four below. Resist any larger refactor — KISS/YAGNI as per `CLAUDE.md`. Everything else in this review is an *incident* (a single bug, a single bad commit) and is one-PR-each.

### R1. **Centralise PROGMEM-to-RAM JSON building.** Priority: high.

Same code shape exists in:
- `SATcontrol.ino:2270-2348` (climate_attributes, 17 unguarded `pos += snprintf_P`)
- `SATcontrol.ino:1731+` (PID attributes, Task #55)
- `SATcontrol.ino:1885+` (deleted pressure_health_attr — was identical pattern)

`mqtt_configuratie.cpp` has the `MqttJsonWriter` abstraction that handles bounds correctly. Migrate the SAT JSON builders to it. Eliminates the latent buffer-overflow class and removes ~150 lines of repetition. Self-contained PR, no architectural change.

### R2. **Remove per-file version stamp comments.** Priority: medium.

Delete the `**  Version  : v1.5.x-beta.N` line from every `.ino`/`.h`/`.cpp`/`.css`/`.html`/`.js` header block. Keep `version.h` as the sole source. Update `scripts/autoinc-semver.py` to only touch `version.h` and `data/version.hash`. Every future prerelease bump becomes a 2-file diff instead of 27-file. Massive reviewability win, zero functional change. One-time mechanical edit + tooling tweak.

### R3. **Replace `writeFriendlyName` underscore-split with case+underscore split.** Priority: medium.

Single function, ~15 lines added, handles `OEMFaultCode → OEM Fault Code`, `MaxCapacityMinModLevel → Max Capacity Min Mod Level`, `dayofweek → Day Of Week`. Removes the need for the ~125 PROGMEM string corrections in `6d162be4` and prevents the same fix-cycle from coming back a fifth time. Token-by-token logic, allocation-free, fits in streaming writer.

### R4. **De-duplicate non-OT pseudo-ID list.** Priority: low.

`publishNonOTDiscoveryConfigs()` (`MQTTstuff.ino:1308`) and `markAllMQTTConfigPending()` (`MQTTstuff.ino:1330`) both list:
```cpp
setMQTTConfigPending(0);   // climate
setMQTTConfigPending(27);  // outside temp
setMQTTConfigPending(OTGWdallasdataid);
setMQTTConfigPending(OTGWheapstatsid);
setMQTTConfigPending(OTGWfwinfoid);
setMQTTConfigPending(OTGWpicinfoid);
setMQTTConfigPending(OTGWpicsettingsid);
```

Extract to `static const uint8_t kNonOTPseudoIds[] PROGMEM = {...}` and a single loop. Trivial PR. Prevents the "add a new non-OT entity, forget to update both lists" bug.

### Things that look like refactor candidates but are not (yet)

- **JIT discovery model**: do not refactor — ADR-073 is fresh, evidence-grounded, and matches `CLAUDE.md` design principles. Watch for the "broker without persistence under 5 min" failure mode in field reports first.
- **OTmap[] echo flags**: do not refactor into a runtime-configurable table — the static table is correct per the project's "typed internal control flow" rule. Just keep updating individual rows as field evidence arrives.
- **SAT subsystem**: do not refactor — it works, it has tests of a sort (hardware-in-the-loop on Andre's installation), and the recent fixes are local.
- **Sketch concat / single-translation-unit**: do not touch — this is Arduino-IDE-compatible by design and changing it breaks the contributor on-ramp.

---

## 9. Process recommendations (no code change)

1. **Squash backlog admin commits into the originating code commit.** Or at minimum, batch end-of-day. The signal-to-noise of `git log` would improve ~3x.
2. **Add a `git pre-push` hook** that rejects commits adding files matching `*.txt` > 100 KB at the repo root. Would have caught `logfile.txt`.
3. **Audit ADR cadence.** Five ADRs in three months with one in-cycle supersede suggests the gates are too easy or the cycle is too fast. Either raise Evidence-gate strictness or introduce `Status: Provisional`.
4. **For cross-worktree fixes that are mechanically identical**, prefer `git cherry-pick` over hand-mirrored "port-of" commits. Add a CI check that scans `dev` HEAD~30 for fixes to dual-tracked files and flags missing 2.0.0 mirrors.

---

## 10. What did the last 100 commits do *well*?

To balance the criticism:

- **JIT discovery is a real product improvement** — the bulk-publish-at-boot regression was real, the evidence was captured, the fix is minimal, the ADR is excellent.
- **Field-evidence discipline on OT echo flags** — the captured telnet log in `660d4b93`'s commit message is a model for how to ship boiler-protocol changes.
- **Versioning policy is enforced** — `bin/bump-prerelease.sh` + pre-commit gate seems to have prevented the "two fixes share a beta tag" problem the policy was designed to solve.
- **ADR immutability is respected** — ADR-041's content was not edited when ADR-073 superseded it; only the Status line moved. This is harder than it looks and worth keeping.
- **Co-authorship attribution is consistent** — Claude is credited where it contributed, maintainer-only commits are clean.

---

## Appendix — substantive commits in window

```
f863aebe  Create logfile.txt                                             [E1, A2]
64a86679  Initial plan (#567)                                            [C3]
51356205  docs: update documentation for changes since v1.5.0-fix
ad963c7c  chore(discord-mcp): update skill files for ExilProductions
a1a7795e  fix(sat): remove orphaned sat/pressure_health_attr (beta.3)    [A1, B2]
ee370527  fix(sat): wire sat/climate_attributes (beta.2)                 [A1, B3]
503461c8  chore(version): reset prerelease to beta.1                     [A3]
1bb58d8f  feat(mqtt): pure JIT MQTT discovery                            [B1, R1]
d541e7dd  fix(flash_otgw.bat): PS1 generation + COM port + schema        [F2]
67980f1e  fix(flash_otgw.bat): registry for COM port detection           [C1]
5903b215  chore(version): refresh build artefacts post-beta.29           [A3, C1]
0719e086  docs(release): v1.5.0 stable release notes                     [F3]
79aeb5dd  chore(release): v1.5.0 release build
660d4b93  fix(mqtt): flip Class 1/8 control writes (TASK-571)            [B4]
6d162be4  fix(mqtt): normalise HA discovery friendly-name strings        [A4, R3]
9a53a4f1  fix(mqtt): drop hostname + Title-Case friendly names           [A4]
fc72adfe  fix(mqtt): friendly name uses spaces, not underscores          [A4]
e38cf970  docs(adr): ADR-072 — HA discovery friendly-name format         [D1]
```

Tags `[X.N]` cross-reference to section numbers above.
