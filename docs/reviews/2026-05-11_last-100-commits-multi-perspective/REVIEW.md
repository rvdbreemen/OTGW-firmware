# Critical Multi-Perspective Review — last 100 commits on `dev`

**Date**: 2026-05-18 (updated; original 2026-05-11)
**Scope**: HEAD..HEAD~100 on branch `dev` (range `8527c56..ebbbb4d`).
**Tone**: deliberately critical. Findings are biased toward what is wrong, weak, or fragile. Where something is good, that is also called out but kept brief.

---

## Changelog since 2026-05-11 review

Robert has been busy. The 2026-05-11 review covered range `f863aebe..660d4b93`. The window has shifted substantially — the new HEAD is `ebbbb4d` (2026-05-18), 26 new commits past the old boundary. Key changes to prior findings:

| Prior finding | Outcome |
|---|---|
| **CRITICAL A1** — `SATcontrol.ino` 17 unguarded `pos += snprintf_P` | **RESOLVED** — SAT subsystem removed from dev (`a25c3b1`) |
| **HIGH C1** — 480k-line build-artefact commits | **REFRAMED** — was a merge-sequence diff artefact, not real |
| **MEDIUM A2** — `logfile.txt` 1.6 MB at repo root | **STILL OPEN** — plus `commits.txt` added |
| **MEDIUM A3** — version-stamp cascade 25+ files | **TRACKED** — TASK-597 created, not fixed |
| **MEDIUM A4/R3** — `writeFriendlyName` underscore-only split | **STILL OPEN** — no task created yet |
| **NEW HIGH** — ADR-074 implementation silently reverted | **NEW** — code still drives avty_t from OT-bus liveness |

---

## TL;DR

Significant product progress since the last review: SAT subsystem cleaned up, JIT discovery bugs patched, HA availability correctly decoupled by ADR-074. But ADR-074's actual code changes were **silently rolled back** by a subsequent squash-merge from an older branch — HA entity availability still flaps with OT-bus liveness in the current `beta.11`. `logfile.txt` is still in the tree. Backlog-admin noise improved from 60% to 44% of commits. The new PIC-control entities (button + selects) are well-implemented.

---

## 1. Inventory

| Category | Count | Notes |
|---|---:|---|
| Total commits in window | 100 | range `8527c56..ebbbb4d` |
| `Update task TASK-*` / `Create task TASK-*` | **44** | improved from 60 in prior window |
| `docs(*)` | 23 | ADR, guides, daily reports, CHANGELOG |
| `fix(*)` | 14 | MQTT/SAT/flash/UI/build |
| `feat(*)` | 3 | JIT discovery, flash scripts, PIC-control entities |
| `chore(*)` | 10 | SAT cleanup, version bumps, adr-kit |
| Other | 6 | merges, empty commits, backlog-only |
| Authored by `noreply@anthropic.com` | ~42 (Co-Authored) | consistent attribution |

Firmware-touching commits (changed `src/**`): ~20. The other 80 are housekeeping, docs, version stamps, or backlog admin.

---

## 2. Perspective A — Code quality & ESP8266 constraints

### A1. ~~CRITICAL~~ → **RESOLVED** — `SATcontrol.ino` buffer overrun

`a25c3b1 chore(sat): remove orphaned SAT subsystem from dev branch` and `4fcd30a chore(sat): remove dead ENABLE_SAT scaffolding` together completely removed the SAT subsystem from `dev`. The 17 unguarded `pos += snprintf_P` calls are gone. `4fcd30a` also cleaned `OTGW-firmware.h` of 350 lines of dead `#ifdef ENABLE_SAT` scaffolding.

**This is the right call.** SAT belongs on the 2.0.0 branch where it is actively maintained. No residual dead state detected.

### A2. **MEDIUM** — `logfile.txt` (10 643 lines) and `commits.txt` still in repo root

`f863aeb Create logfile.txt` is still in the 100-commit window. `logfile.txt` (a captured telnet stream, ~1.6 MB) and a new `commits.txt` (68 lines, a `git log` dump) are both tracked files at the repo root. `.gitignore` covers `*.log` and `*.bin` but not `*.txt`.

**Remediation**: `git rm logfile.txt commits.txt && echo "*.txt" >> .gitignore` at the root, or at minimum `echo "/logfile.txt\n/commits.txt" >> .gitignore` then `git rm --cached logfile.txt commits.txt`. Add a pre-push hook: `find . -maxdepth 1 -name "*.txt" -size +100k` → block.

The commit message "Create logfile.txt" remains content-free. No `why`.

### A3. **MEDIUM** — version-stamp cascade (tracked, not fixed)

Every `.ino`/`.h`/`.css`/`.html`/`.js` carries a `**  Version  : v1.5.1-beta.N` header rewritten by `scripts/autoinc-semver.py`. TASK-597 was created to fix this. The fix has not landed — `ebbbb4d` (beta.11) rewrites 14 files for a 1-CSS-selector UI change. Every single-line fix still ships as a 14–27 file diff. TASK-597 priority should be elevated; this is the single biggest reviewability tax in the repo.

### A4. **LOW** — `writeFriendlyName` still underscore-only split

`6d162be4` patched ~125 PROGMEM strings by hand to add underscores. `writeFriendlyName` (called from `mqtt_configuratie.cpp`) still only splits on underscore — camelCase boundaries (`OEMFaultCode`, `MaxCapacityMinModLevel`) still cannot be split automatically. Next time a tester adds a new OT entity with a camelCase name, the fix cycle repeats. A 20-line single-pass state-machine would close this permanently. No task exists for this — R3 from the prior review is still outstanding.

---

## 3. Perspective B — Architecture & design

### B1. **NEW HIGH** — ADR-074 implementation silently reverted by squash-merge

ADR-074 ("HA entity availability reflects MQTT link, not OT-bus liveness", Accepted 2026-05-16) was correctly implemented in `7b0d167`. Within the same day, a squash-merge of PR #572 (`a82de1e fix(mqtt): JIT discovery hasConfig gate`) landed on a branch base that predated `7b0d167`, silently restoring both removed callsites:

| File | Callsite | State after `7b0d167` | State in current `beta.11` |
|---|---|---|---|
| `OTGW-Core.ino:4042` | `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline))` | **removed** | **back** |
| `MQTTstuff.ino:1148` | same, in `sendMQTTstateinformation()` | **removed** | **back** |

`git diff 7b0d167 origin/dev -- src/OTGW-firmware/OTGW-Core.ino` confirms the reintroduction. The ADR-074 comment added by `7b0d167` in `MQTTstuff.ino` is also gone.

**Effect**: HA entity availability on `<toptopic>/value/<nodeid>` still flaps with OT-bus liveness. Every entity (climate, sensor, binary_sensor, number) becomes `unavailable` when the OT bus goes quiet for 30 seconds. The two field testers who reported DHW Control flapping in beta on 2026-05-16 still experience this bug in beta.11.

**Fix**: Re-apply the two-line removal from `7b0d167`. In `processOT()` (OTGW-Core.ino), remove the `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline))` call inside the `isPICEnabled()` block. In `sendMQTTstateinformation()` (MQTTstuff.ino:1148), do the same. OT-bus liveness should drive only `otgw-pic/otgw_connected` (which `sendMQTTDataPic` already does correctly on the line above each removed call).

**Process note**: this is the exact failure mode the "one master plan, two tasks, two agents" cross-worktree rule is designed to prevent. PR #572 was developed against an older dev and merged without rebasing onto `7b0d167`. Pre-merge rebase or a CI check on the `avty_t` callsites would have caught this.

### B2. **POSITIVE** — JIT discovery hasConfig gate (`a82de1e`)

The JIT implementation (`1bb58d8f`, ADR-073) had a subtle re-entrancy bug: OT IDs that had their discovery config bit set (e.g. via a legacy topic, or a retained MQTT message from a previous firmware) but that had never been received on the OT bus could stall the drip publisher. `a82de1e` adds a `hasConfig` gate that checks the OTmap entry exists before setting pending — phantom IDs now never enter the drip queue. Correct fix, well-commented.

### B3. **POSITIVE** — PIC-control entities (pseudo-ID 251, `908e1e1`)

New feature: 9 HA entities for gateway PIC control — one `button` (resetgateway) and eight `select` entities (gpioa/b, leda-ledf). Implementation is clean:

- PROGMEM strings for command names and PIC command codes allocated correctly.
- All-or-nothing semantics: the pending bit is cleared only when all 9 discovery configs are published successfully in a single drip tick. Good atomic-ness for a constrained heap.
- Correctly added to BOTH `publishNonOTDiscoveryConfigs()` AND `markAllMQTTConfigPending()` — the R4 duplication risk (from prior review) was managed with discipline.
- Unconditional discovery (no `isPICEnabled()` gate) is explicitly documented with the rationale (`isPICEnabled()` false causes `result=false` and infinite drip retry — PR #576 review finding).

The comment block explaining the `isPICEnabled()` gate decision is exactly the kind of non-obvious constraint documentation the project principles call for.

### B4. **MEDIUM** — non-OT pseudo-ID list still duplicated (R4 outstanding)

`publishNonOTDiscoveryConfigs()` and `markAllMQTTConfigPending()` both explicitly enumerate the same 8 non-OT pseudo-IDs. The `908e1e1` commit added `OTGWpiccontrolsid` to both in sync — but the structural risk remains. A future contributor adding a new pseudo-ID entity will need to update two places and may miss one. Extracting to `static const uint8_t kNonOTPseudoIds[] PROGMEM = {...}` + a loop is a 10-line change.

---

## 4. Perspective C — Git hygiene & commit discipline

### C1. **REFRAMED** — "480k-line build-artefact commits" (prior HIGH)

Re-examined: `67980f1 fix(flash_otgw.bat)` is a real 1-file, 8-line change. `5903b21 chore(version): refresh build artefacts post-beta.29` is a real 2-file change. The prior review's "480k lines" figure was an artefact of how `git diff` calculated the delta when the merge commit `ac317dc4` (merge main → dev) was at the boundary of the shallow clone. Not a real ongoing issue.

The underlying concern — large merge commits with misleading subjects — is still real but does not recur in the new window.

### C2. **MEDIUM** — 44% backlog-admin commits (improved from 60%)

| Window | Admin commits | % of 100 |
|---|---|---|
| Prior (to 2026-05-07) | 60 | 60% |
| Current (to 2026-05-18) | 44 | 44% |

The CLAUDE.md commit-batching policy (added in `1cec3e3`) and TASK-598 exist to address this. Improvement is visible — the new window has more substantive commits. However, 44% is still the plurality. Until TASK-598 lands, "review the last 100 commits" effectively means reviewing ~56 substantive ones while filtering 44 admin ones.

### C3. **MEDIUM** — squash-merge without rebase erased ADR-074 fix (see B1)

Beyond the commit count, the deeper hygiene issue is that a squash-merge (`a82de1e`, PR #572) landed without rebasing on `7b0d167` (PR #583, same day). In a squash-merge workflow, the entire PR history becomes one commit whose parent is the HEAD at merge time, but the CONTENT can predate concurrent changes. Result: a silent regression.

**Recommendation**: before merging any PR on an active day, `git fetch && git rebase origin/dev` in the PR branch, then re-run evaluate.py. Or add a CI step: `grep -n "sendMQTT(MQTTPubNamespace" src/OTGW-firmware/OTGW-Core.ino src/OTGW-firmware/MQTTstuff.ino` — any hit is an ADR-074 violation.

### C4. **LOW** — `Initial plan (#567)` empty merge commit still present

`64a86679` carries no payload — it's a Copilot-authored merge of a PR branch with no content. Still sitting in mainline `dev`. Either close + delete, or squash into the first real commit that follows it.

### C5. **POSITIVE** — Co-Authored-By discipline consistent

Attribution is clean throughout the new window. Claude co-authorships are accurate (co-authored where AI contributed, author-only where the maintainer worked alone).

---

## 5. Perspective D — Documentation & ADRs

### D1. ADR-074 — well-written, implementation broken (see B1)

ADR-074 is solid: field evidence from two users with timings ("~20 s unavailable, ~2 s available"), root-cause code path traced to three specific lines, correct diagnosis of the TASK-538 regression. The Decision is clear imperative. Consequences list the one edge case (PIC-disabled devices publishing to the OT subtree is already guarded). Status is correctly Accepted.

The problem is not the ADR — it's the merge that rolled back the code. The ADR is now in an inconsistent state: Accepted but not implemented.

### D2. ADR cadence — stable

No new ADRs authored since ADR-074. Five ADRs in the prior window had already pushed the cadence higher than ideal. The pause is healthy. ADR-073 and ADR-074 are both evidence-grounded and self-contained — no in-cycle supersession.

### D3. **LOW** — no-news daily-report commits continue

`bf0acf6 docs: daily issue report 2026-05-18 — no new issues` adds a commit that only says three sources found nothing. Still inflates `git log`. Either skip commits when no actionable findings exist, or aggregate weekly.

### D4. **POSITIVE** — Documentation depth improved

`15cf48a docs: add openHAB and Domoticz integration guides` and `4390778 docs: fix dev documentation-review findings 1-5` show the documentation backlog is being actively worked. The integration guides are new content (not re-wraps of existing text).

---

## 6. Perspective E — Security & robustness

### E1. `logfile.txt` in public tree (see A2)

Still present. Skimmed on 2026-05-11: no obvious credentials. The file contains live telnet output (OT message captures, HTTP access log). Maintaining it in a public GitHub repo is unnecessary exposure. Remove before next release.

### E2. `a82de1e` MQTT rate-gate change (TASK-612)

`a82de1e` also refines the MQTT publish rate gate: `first-seen` and `forced` publishes now bypass the 250ms spacing gate, which previously starved entities like ASF/RBP/VH whose parent OT messages arrive rarely. Correct — a one-shot first-seen event should not be rate-limited by a gate designed for recurring 60s heartbeats. The comment explains the invariant clearly.

### E3. No new attack surface

The PIC-control entities (`908e1e1`) extend MQTT command dispatch — the `resetgateway` button triggers `resetOTGW()` which resets the PIC. This is command-injection-safe: the payload is explicitly ignored (`payload is ignored` comment), and the function name lookup goes through a PROGMEM table with `strcmp_P` comparisons. No shell exec, no format string exposure. REST API surface unchanged.

---

## 7. Perspective F — Build, release, and tooling

### F1. **POSITIVE** — `evaluate.py` false-positive fixes (`946708b`)

`946708b Fix false-positive and stale checks in evaluate.py` tightens the evaluator:
- Removes stale pattern checks that fired on correct code
- Adds context-aware matching to reduce false positives on PROGMEM-safe patterns

This is important: a false-positive-heavy evaluator trains maintainers to ignore its output. The fix restores trust in the CI baseline.

### F2. **POSITIVE** — git submodule auto-init in build.py (`4ae9627`)

`4ae9627 fix(build): auto-init missing git submodules in build.py` adds a check-and-init step so a fresh clone that forgets `--recursive` doesn't silently produce a broken build. Simple, high-value reliability fix.

### F3. **POSITIVE** — flash script hardening (`c2f46e1`)

`c2f46e1 fix(flash): harden flash_otgw.sh/.bat — spec parity, SHA256 integrity, version-aware selection` adds:
- SHA256 integrity verification of downloaded binaries
- Version-aware esptool/esptool.py selection
- Spec parity between the `.sh` and `.bat` scripts

After five iterative flash-script commits in the prior window, this commit looks like the stabilising one. SHA256 verification is a meaningful security improvement for the download path.

### F4. **LOW** — Release cycle: beta.11 for a 1-selector CSS fix

`ebbbb4d` (FSexplorer Update Firmware button fix) bumps to beta.11 for adding two CSS rules (`@media (hover: hover)` and cursor override). The fix is correct and important (GitHub #575). But it required rewriting 14 files for 2 CSS lines — exactly the cost the TASK-597 version-stamp fix would eliminate. Until TASK-597 lands, every minor fix will keep burning bump slots.

---

## 8. Updated refactoring recommendations

### R1. ~~SAT JSON builders (`MqttJsonWriter` migration)~~ → RESOLVED

SAT subsystem removed from dev. R1 is closed.

### R2. Remove per-file version-stamp comments (TASK-597) — priority medium

Status: TRACKED. TASK-597 exists; no implementation yet. The pain is real (every 1-line fix ships as 14-27 file diff). Priority should be elevated — this is the single highest-ROI cleanup in the repo. One-time mechanical edit + tooling change.

### R3. Replace `writeFriendlyName` underscore-only split — priority medium

Status: NOT STARTED, no task. The `6d162be4` hand-patch treated the symptom, not the cause. Next new OT entity with a camelCase identifier name restarts the cycle. A 20-line allocation-free state machine (uppercase-boundary detection + underscore split) closes this permanently.

### R4. De-duplicate non-OT pseudo-ID list — priority low

Status: NOT STARTED. `publishNonOTDiscoveryConfigs()` and `markAllMQTTConfigPending()` still enumerate the same pseudo-IDs independently. `908e1e1` correctly maintained both in sync for pseudo-ID 251, but the structural divergence risk remains. One `static const uint8_t kNonOTPseudoIds[] PROGMEM = {...}` and a loop in each function.

### R5. **NEW — Re-apply ADR-074 code changes — priority HIGH**

Status: REGRESSION — was fixed in `7b0d167`, silently reverted. Two targeted one-line removals:

1. `OTGW-Core.ino` — inside `if ((state.otgw.bOnline != bOTGWpreviousstate) || ...)` → `if (isPICEnabled())` block: remove `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline));`
2. `MQTTstuff.ino` — in `sendMQTTstateinformation()`: remove the same call at line 1148.

OT-bus liveness should drive only `otgw-pic/otgw_connected` (already done via `sendMQTTDataPic(F("otgw_connected"), ...)`). The LWT/birth mechanism owns the namespace topic. This is the fix HA field testers need to stop seeing entity availability flap.

---

## 9. Process recommendations (no code change)

1. **Rebase PRs before merge on active dev days.** PR #572 + PR #583 merged same-day without rebase; the ADR-074 regression resulted. Pre-merge rebase is a 30-second step.
2. **Add a CI lint for the two ADR-074 callsites.** `grep -n "sendMQTT(MQTTPubNamespace, CONLINEOFFLINE" src/OTGW-firmware/*.ino` — any hit blocks merge. Cheap, specific, permanent enforcement.
3. **Elevate TASK-597 (version-stamp cascade).** The 14-27 file diffs are the biggest ongoing tax on reviewability and bisectability. Until it lands, every bump review is noisy.
4. **Create a task for R3 (writeFriendlyName).** The fix is ~20 lines. A task makes it schedulable and prevents the next round of PROGMEM string hand-patches.
5. **Resolve or remove `logfile.txt`/`commits.txt` before v1.5.1 stable.** They have no place in the firmware tree.

---

## 10. What did the new commits do well?

- **SAT cleanup was thorough.** `a25c3b1` removed the whole subsystem, `4fcd30a` cleaned up the scaffolding. No dead state, no TODO trail. This is how platform-specific code should be managed across a worktree split.
- **JIT discovery bug catch was fast.** ADR-073 was accepted in late April; the hasConfig gate regression (`a82de1e`) was caught and fixed within the same release cycle. That's the right cadence for an actively field-tested firmware.
- **PIC-control entities are well-scoped.** `908e1e1` adds a meaningful feature (HA-native PIC reset and GPIO control) without introducing complexity. The all-or-nothing publish semantics and the unconditional-discovery rationale comment are exemplary.
- **evaluate.py trust restored.** `946708b` is a quiet but important commit. A tool people trust gets used; a tool with false positives gets ignored.
- **Backlog noise is trending down.** 44% admin commits vs 60% is measurable progress. CLAUDE.md commit-batching policy is the right lever.

---

## Appendix — substantive commits in new window (since prior review boundary `660d4b93`)

```
c2f46e1  fix(flash): harden flash_otgw.sh/.bat (SHA256, version-aware)    [F3]
f863aeb  Create logfile.txt                                                [A2, E1]
64a8667  Initial plan (#567) — empty merge commit                          [C4]
a1a7795  fix(sat): remove orphaned pressure_health_attr (beta.3)
ee37052  fix(sat): wire sat/climate_attributes (beta.2)
1bb58d8  feat(mqtt): pure JIT MQTT discovery (ADR-073)                     [B2]
7b0d167  fix(mqtt): HA availability reflects MQTT link (ADR-074)           [B1★]
e6f9983  chore(backlog): track CI baseline follow-ups
8dcca6e  feat(tooling): standalone HA discovery topic wiper
a25c3b1  chore(sat): remove orphaned SAT subsystem from dev                [A1]
a82de1e  fix(mqtt): JIT hasConfig gate (PR #572, squash-merged)            [B1★, B2, E2]
4fcd30a  chore(sat): remove dead ENABLE_SAT scaffolding                    [A1]
7b0d167→a82de1e: ADR-074 regression introduced here ★
946708b  Fix false-positive stale checks in evaluate.py                    [F1]
4ae9627  fix(build): auto-init missing git submodules                      [F2]
15cf48a  docs: add openHAB and Domoticz integration guides                 [D4]
4390778  docs: fix dev documentation-review findings 1-5                   [D4]
908e1e1  feat(discovery): HA button+select PIC-control entities (ID 251)   [B3, B4, E3]
ebbbb4d  fix(ui): FSexplorer Update Firmware visible on touch desktops     [F4]
```

★ = ADR-074 regression introduced between `7b0d167` and `a82de1e` via squash-merge without rebase.

Tags `[X.N]` cross-reference to section numbers above.
