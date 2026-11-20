# Branch, tag and worktree audit (read-only), 2026-11-20

Repo: shared `.git` of `RvdB/OTGW-firmware` (25 worktrees). Only `git fetch origin` (no prune) and read commands were run; nothing was deleted, moved, checked out or edited. Dirty counts used `git --no-optional-locks status --short`.

## Method

- **Base**: `origin/dev` for 2.x lanes, `origin/otgw-1.x.x` for 1.x refs (picked automatically when a ref is >400 commits ahead of origin/dev). The analysis ran against origin/dev = `4a9bc3c7b`; origin/dev moved to `55c4146b7` (TASK-2953 docs) later in the audit by another session's fetch. "Behind" counts are vs `4a9bc3c7b`.
- **Landed**: `git cherry <base> <ref>`. For every `+` commit, a dev commit with the identical subject was searched; then both were compared per file after dropping `version.h`, `version.hash` and banner lines (`_VERSION_`, `Version :`, `x.y.z-beta.N`). That per-file check split paths on spaces, so it was blind to `backlog/tasks/*` files; every pair that backs a DELETE row was therefore re-checked with `git range-diff <c>^! <d>^!` (10 pairs). Outcomes:
  - *cherry-equivalent*: no `+` at all.
  - *banner-only*: same subject, range-diff shows only `**  Version  :` banner lines and rebase context (ADR-INDEX/README rows, task-file `updated_date`) → landed.
  - *docs-diff*: same subject, only README/ADR-INDEX/CHANGELOG/spec docs differ (typical conflict resolution) → landed, small uncertainty.
  - *code-diff*: same subject, code differs → superseded snapshot (dev has a later version).
  - *unique*: no same-subject commit on dev. A few were then found on dev by task ID or with the `(2.0.0-beta.N)` suffix stripped; marked "likely landed".
- **In tag**: the same cherry check against `v2.0.0-beta.401` (latest published, GitHub prerelease 2026-11-20 09:56Z); for 1.x against `v1.7.9`. All 35 local v2 tags have a GitHub release (94 releases total, none Draft).
- **Class precedence** (overlapping definitions, stated explicitly): HARD KEEP / HELD > dirty worktree (KEEP, owner-must-decide) > fully landed + in beta.401 → DELETE regardless of commit date (a clean worktree must be retired first) > landed but not yet in a tag → KEEP until the next beta > unlanded work committed on or after 2026-11-17 → KEEP > everything else with unique or uncertain commits → ARCHIVE (`archive/<name>` tag at tip, then delete).
- `backup-5e-*` branches are classed HARD KEEP under the "every 5e lane" rule: **my interpretation**, they are backup snapshots of 5e lanes.

## Local branches (65)

| branch | where | last commit | landed? (evidence) | in beta.401? | class | risk / note |
|---|---|---|---|---|---|---|
| main | local + origin | 2026-11-04 v1.7.9 production build | = origin/main (19f0f5225) = v1.7.9 | v1.7.9 | HARD KEEP | |
| dev | local, main tree `OTGW-firmware` (dirty 7) | 2026-11-16 TASK-1887 ADR-253 Accepted (0cfcf2548) | ancestor of origin/dev | yes | HARD KEEP | local dev is **328 behind** origin/dev; main tree dirty (.wolf x3, task-2005 file, `other-projects` submodule) |
| otgw-1.x.x | local, `wt-otgw-1.x.x` (dirty 4, untracked only) | 2026-11-04 bump to v1.8.0-beta.1 | = origin/otgw-1.x.x | n/a | HARD KEEP | |
| lane-09-bench | `wt-09-bench` (clean) | 2026-11-20 TASK-1780/1949 bench ACs | not analysed | | HARD KEEP | standing tool lane |
| lane-09-cloudrec | `wt-09-cloudrec` (clean) | 2026-11-17 cloudrec quickstart (TASK-1963) | 0 ahead / 666 behind origin/research/cloud-recording | | HARD KEEP | cloud-recording line; fully contained in origin/research/cloud-recording |
| lane-5e-adr | `wt-5e-adr` (clean) | 2026-11-20 TASK-2937 | not analysed | | HARD KEEP | 5e |
| lane-5e-cloudrec | local only (wt-5e-cloudrec is detached at 1a0ea6302) | 2026-11-20 TASK-2937 | not analysed | | HARD KEEP | 5e |
| lane-5e-heaphold | `wt-5e-heaphold` (clean) | 2026-11-19 TASK-2923/2026 | not analysed | | HARD KEEP | 5e |
| backup-5e-cloudrec-badmerge-b55793bca | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup-5e-heaphold-4bb2097ed | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup-5e-heaphold-7917823de | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup-5e-heaphold-a02399d3a | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup-5e-heaphold-b9eef63e4 | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup-5e-heaphold-de4359109 | local | 2026-11-19 | not analysed | | HARD KEEP | 5e backup (interpretation) |
| backup/ba-1965-pre-restack | local | 2026-11-15 TASK-1773 reboot-count regression gate | 4 `+`; range-diff: 3 only banner lines, 1 (86b05fb43) a task-1919 hunk that differs (TASK-1919 is Done on dev with later notes) | yes | DELETE | pre-restack snapshot |
| ci-combo-only | local | 2026-11-14 TASK-1929 Done | 0 ahead | yes | DELETE | |
| fix/ba-2918-iw | local | 2026-11-19 TASK-2918 Done | 0 ahead | yes | DELETE | same tip as lane-2028 (eddfc16ba) |
| lane-1882 | local | 2026-11-16 TASK-1882 no-ignition events | 1 ahead, cherry-equivalent | yes | DELETE | |
| lane-1890-pre-rebase2 | local | 2026-11-18 TASK-1890 Q-329b | 3 `+`; range-diff: banner lines + ADR-INDEX/README rebase context only | yes | DELETE | |
| lane-1890 | `wt-1890` (clean) | 2026-11-18 TASK-1890 regression bench of 377 | 0 ahead | yes | DELETE | retire wt-1890 first (ignored: .build-venv, .build-python, safeboot/.pio, updateServerHtml.h; no build/) |
| lane-1995n | local | 2026-11-18 TASK-1995 field core dump | 0 ahead | yes | DELETE | |
| lane-2019n | local | 2026-11-18 TASK-2019 note | 0 ahead | yes | DELETE | |
| lane-2026-pre-rebase | local | 2026-11-18 TASK-2026 heap-hold hook | 2 `+`; range-diff: banner lines + ADR-INDEX/README rebase context only | yes | DELETE | |
| lane-2026 | local | 2026-11-19 beta.392 release notes | 0 ahead | yes | DELETE | |
| lane-2028 | local | 2026-11-19 TASK-2918 Done | 0 ahead | yes | DELETE | |
| lane-2040 | local | 2026-11-19 TASK-2040 bench of 382 | 0 ahead | yes | DELETE | |
| lane-2920 | local | 2026-11-19 TASK-2026/2920 Done | 0 ahead | yes | DELETE | |
| lane-2921 | local | 2026-11-19 TASK-2921 picked up | 1 ahead, cherry-equivalent | yes | DELETE | |
| lane-2921b | local | 2026-11-19 TASK-2922 AC drop | 0 ahead | yes | DELETE | |
| lane-2922b | local | 2026-11-19 TASK-2031 CHANGELOG | 0 ahead | yes | DELETE | |
| lane-2923 | local | 2026-11-19 TASK-2923 note | 2 ahead, cherry-equivalent | yes | DELETE | |
| lane-2924 | local | 2026-11-19 TASK-2924 heap-hold key in 'h' menu | 0 ahead | yes | DELETE | |
| lane-apstable | local | 2026-11-19 TASK-1851 Done | 0 ahead | yes | DELETE | |
| lane-busy503 | local | 2026-11-19 TASK-2910 busy-503 | 0 ahead | yes | DELETE | |
| lane-docs-1119 | local | 2026-11-19 TASK-2922 capture-host gap | 0 ahead | yes | DELETE | |
| lane-review | `wt-410` (clean) | 2026-11-19 TASK-2929 production dry run | 1 ahead, cherry-equivalent | yes | DELETE | branch name differs from worktree name; retire wt-410 first (no build/) |
| lane-simport | local | 2026-11-19 TASK-2042 Git Bash | 0 ahead | yes | DELETE | |
| lane-wxprefetch-pre | local | 2026-11-19 TASK-2040 filed | 1 `+`; range-diff: banner lines only | yes | DELETE | |
| lane-wxprefetch | local | 2026-11-19 TASK-2039 bench of 380 | 0 ahead | yes | DELETE | |
| lane/ba-1889 | local (wt-ba-1889 is detached elsewhere) | 2026-11-19 TASK-2023 ADR-262 Accepted | 0 ahead | yes | DELETE | |
| push-397 | local | 2026-11-19 TASK-2924 Done | 0 ahead | yes | DELETE | |
| backup/ba-1919-pre-restack | local | 2026-11-15 TASK-1919 rollback to safeboot | 1 `+`, code-diff (OTGW-ModUpdateServer-esp32.h, OTGW-firmware.ino, test_otadata_stale.py) | superseded | ARCHIVE | pre-restack snapshot; dev holds the restacked version (TASK-1919 Done on dev) |
| lane-1689tls | local + backup/lane-1689tls | 2026-11-07 mini CA bundle (14 roots) for HTTPS client | 1 unique, not on dev | no | ARCHIVE | HTTPS PoC (project rule: no HTTPS); a copy also lives on the NAS backup remote |
| lane-1759a-pre-takeover | local | 2026-11-10 TASK-1786/1787 notes | 2 `+`: 1 banner-only, 1 likely landed (TASK-1759 MsgID 28 subject found on dev, content not diffed) | likely | ARCHIVE | low risk; would be DELETE once the one commit is diffed |
| lane-1882-pre | local | 2026-11-15 TASK-1882 events (pre) | 4 `+`: 1 code-diff (OTDirect.ino + 2 tests), 2 banner-only, 1 unique (older form of the TASK-1882 commit) | superseded | ARCHIVE | snapshot of lane-1882, which landed |
| lane-1890-pre-rebase | local | 2026-11-18 TASK-1890 held until release/2.x | 6 `+`: 1 docs-diff, 5 unique (TASK-2029 notes, "TASK-1890 held"; ADR-259 commit found on dev by subject) | partly | ARCHIVE | notes may have been rewritten on dev; uncertain |
| lane-2026-pre-rebase3 | local | 2026-11-19 TASK-2038 filed; counters parked | 4 `+`: 2 banner-only, 1 docs-diff (ADR-INDEX, README), 1 unique 43298e277 (ADR README counts) | partly | ARCHIVE | 43298e277 is also on lane-2026-ctr (same sha) |
| lane-410 | local (wt-410 holds lane-review) | 2026-11-18 TASK-1887 entries 410/417 | 1 `+`, docs-diff in firmware-sat-spec.md and sat-comparison.md | content differs | ARCHIVE | SAT spec text differs from dev; check which wording is current |
| lane-95-docs | local | 2026-11-18 TASK-2030 Done | 4 unique by subject; TASK-2031 7.12a and TASK-2028 `flash_otgw.sh --update` exist on dev under other subjects | likely | ARCHIVE | likely landed in reworded commits; not proven |
| lane-comms-pre-309 | local | 2026-11-15 queue Q-1804a | 4 `+`: 3 banner-only, 1 docs-diff (ADR-INDEX.json, README.md) | yes (by subject) | ARCHIVE | effectively landed; ARCHIVE only for the docs-diff |
| lane-comms-pre-330b | local | 2026-11-15 Q-PWM2/Q-1882a | 1 `+`, docs-diff (CHANGELOG.md) | yes (by subject) | ARCHIVE | effectively landed |
| lane/ba-vcenv | local | 2026-11-15 TASK-1974 shared vcvars cache (WIP) | 1 unique | no | ARCHIVE | WIP; TASK-1974 task file exists on dev |
| parked/ba-ui-1843-1838-1837 | local | 2026-11-12 TASK-1837 appliance label | 6 unique by exact subject; all 6 found on dev with the `(2.0.0-beta.N)` suffix stripped; content: 1837, 1843, 1830 identical, 1823 differs in v2.js, 1828/1838 not finished | likely, 1823 differs | ARCHIVE | "parked" name, but the work is on dev in some form |
| docs-2026 | `wt-95-docs` (clean) | 2026-11-20 TASK-2950 docs up to beta.402 | 0 ahead (was = origin/dev tip) | no, 19 commits after beta.401 | KEEP | landed, not yet in a tag |
| lane-architect | `wt-architect` (clean) | 2026-11-20 TASK-2950 (same tip as docs-2026) | 0 ahead | no | KEEP | this audit's own worktree |
| lane-2019t | `wt-2019t` (clean) | 2026-11-18 TASK-2019 host repro of SAT outbox burst | 1 unique | no | KEEP | recent, unlanded; ARCHIVE candidate later |
| lane-2026-ctr | local | 2026-11-19 TASK-2026 dead-link supervisor counters | 4 `+`: 2 banner-only, 2 unique (counters + 43298e277) | no | KEEP | recent; "counters parked" per lane-2026-pre-rebase3 tip subject |
| park/task-2019-pace | local | 2026-11-20 TASK-2019 SAT burst uses pace window | 1 unique | no | KEEP | today; parked fix |
| lane-comms | `wt-comms` (dirty 1: untracked sat_tune_test.json) | 2026-11-20 TASK-2945 beta.401 Done | 0 ahead | no, 5 after beta.401 | KEEP | owner-must-decide on the untracked file |
| lane-ux | `wt-ux` (dirty 1: untracked task-2952 file) | 2026-11-20 UX heuristic review draft | 2 unique (TASK-2952 review) | no | KEEP | owner-must-decide; the task file is not committed |
| lane/ba-2023 | `wt-ba-2023` (dirty 71, staged task files + i18n) | 2026-11-20 TASK-2019 verdict | 0 ahead | no, 15 after beta.401 | KEEP | owner-must-decide; large staged change set |
| poc-tls-1689 | `wt-tlspoc` (dirty 6: FSexplorer.ino, OTGW-firmware.h, version.h/hash, untracked tlsPocRoots.h) | 2026-11-07 TASK-1685 Done | 0 ahead | yes | KEEP | owner-must-decide; branch itself is DELETE-eligible, the dirty tree is uncommitted TLS PoC work |
| review-sat-tasks | `wt-ci-combo` (dirty 19, .full-review/*) | 2026-11-15 TASK-1967/1968 created from SAT full review | 1 unique; no task-1967/1968 file found on origin/dev under backlog/ | no | KEEP | owner-must-decide; the review output in .full-review/ is uncommitted |

## Remote-tracking refs (11: 6 origin, 4 backup, 1 orphan ref)

`git remote -v` shows two remotes, `origin` (GitHub) and `backup` (NAS). `git ls-remote --heads origin` confirms all 6 origin branches still exist on GitHub. `refs/remotes/pr/699` belongs to no configured remote: an orphan ref, removed with `git update-ref -d`, not a push.

| ref | remote | last commit | landed? | in tag | class | note |
|---|---|---|---|---|---|---|
| origin/dev, origin/main, origin/otgw-1.x.x | origin | | | | HARD KEEP | |
| origin/research/cloud-recording | origin | 2026-11-20 merge into poc.31 (TASK-2901) | | | HARD KEEP | no local branch of this name |
| origin/claude/funny-franklin-ybf9jh | origin (GitHub) | 2026-11-20 merge dev into it | 17 ahead / 22 behind; 21 unique (TASK-1984 agentic-setup docs, image briefs) | no | KEEP | active today; TASK-1984 not on dev |
| origin/task1757-staged-connect | origin (GitHub) | 2026-11-10 TASK-1757 C+B bench notes | 2 unique (staged MQTT connect + notes), 1270 behind | no | ARCHIVE | subject says "kept off dev, Robert 2026-11-10"; deleting is a GitHub-side push |
| backup/dev | backup (NAS `//terradisk-next.local/backup-remote/...`) | 2026-11-08 | 0 ahead (stale, 1431 behind) | yes | KEEP (backup mirror) | stale mirror; refreshing/deleting is a NAS-remote push, out of scope for a branch cleanup |
| backup/lane-1689tls | backup | 2026-11-07 | same tip as local lane-1689tls (1 unique) | no | KEEP (backup mirror) | the existing off-site copy of the CA-bundle commit |
| backup/main | backup | 2026-11-04 | = v1.7.9 | v1.7.9 | KEEP (backup mirror) | |
| backup/otgw-1.x.x | backup | 2026-11-04 | = origin/otgw-1.x.x | | KEEP (backup mirror) | |
| pr/699 | none (orphan ref under refs/remotes/pr/) | 2026-11-07 merge upstream dev into Safari branch | `gh pr view 699`: MERGED 2026-11-07T18:26Z, merge commit 460286f24, which is an ancestor of origin/dev | yes (460286f24 is in v2.0.0-beta.401) | DELETE (orphan ref) | `git update-ref -d refs/remotes/pr/699`; the PR on GitHub is untouched |

## Worktrees without a branch, detached, or outside RvdB

`git worktree list --porcelain | grep -cE 'prunable|locked'` = **0**: no prunable or locked entries, and every listed directory exists.

| worktree | HEAD | landed? | in tag | dirty | ignored dirs of note | class | note |
|---|---|---|---|---|---|---|---|
| `C:/Users/rvdbr/AppData/Local/Temp/claude/.../d09d8160-.../scratchpad/devcheck` (outside RvdB, another session's scratchpad) | 24679e271 detached, 2026-11-19 | 0 ahead of dev | yes | 0 | caches, tests/test_sat_ch_enable.obj | owner-must-decide → likely DELETE | clean and landed, but it belongs to session d09d8160 |
| `C:/Users/rvdbr/Documents/Codex/2026-11-17/.../work/pr701` (outside RvdB, Codex) | 59c2506f3 detached, 2026-11-17 "fix(cloudrec): address PR review findings (TASK-1991)" | 44 unique vs dev, but ancestor of origin/research/cloud-recording (0 `+` vs that line) | n/a | 2 (version.h, version.hash: build-stamp churn) | **build/** (10 entries), .build-venv, managed_components, safeboot/.pio | owner-must-decide | content is safe on research/cloud-recording; build/ must be checked before any removal |
| `wt-1995` | b72c6d6c5 detached, 2026-11-19 | 0 ahead | yes | 0 | .build-venv, safeboot/.pio, updateServerHtml.h; no build/ | DELETE (move to OTGW-worktree-leftovers) | its lane branch lane-1995n is DELETE as well |
| `wt-2026` | 9199f621d detached, 2026-11-20 | 0 ahead | no, 18 after beta.401 | **6**: evaluate.py, platformio.ini, src/libraries/espMqttClient/{Config.h, ClientSync.cpp}, untracked OTGW-PROVENANCE.md | **build/** (122 entries) | owner-must-decide | uncommitted library changes (project rule: no library changes without permission) |
| `wt-ba-1889` | b885f3d44 detached, 2026-11-20 | 1 ahead, cherry-equivalent | no, 7 after beta.401 | 0 | **build/** (131 entries) | KEEP until next beta | then retire; check build/ first |
| `wt-cloudrec` | 1929bd7ce detached (= lane-09-cloudrec) | | | 0 | managed_components, sdkconfig.defaults | HARD KEEP | cloud-recording |
| `wt-5e-cloudrec` | 1a0ea6302 detached, 2026-11-20 | | | 0 | | HARD KEEP | 5e |
| main tree `OTGW-firmware` | dev 0cfcf2548 | 328 behind origin/dev | | 7 | build/ (23 entries) | HARD KEEP | see dev row |

## Tags (164)

- v2: 35 (v2.0.0-beta.1 … beta.401). Every one has a GitHub prerelease; no Draft releases.
- v1: 77. **31 v1.7.x beta tags have no GitHub release** (v1.7.0-beta.1..34, v1.7.1-beta.2/3/6, v1.7.2-beta.2/4/5, v1.7.3 (stable!), v1.7.3-beta.2-4, v1.7.5-beta.2-8, v1.7.8-beta.1-8). `gh release list --limit 300` returns 94 releases in total, so this is not a paging artefact. Not inspected further.
- v0.x: 21 (`v0.5.0` … `v0.10.3-beta`), plus 4 without the `v` prefix: `0.10.1`, `0.10.2`, `0.10.3`, `1.0.0-rc5`.
- alpha: **0**. Alpha builds are `_VERSION_PRERELEASE` bumps, not tags.
- Other / odd (27): `backup/*` 11 (ba-1919-debug, ba-1919-probe, bench-ba-1989-1983, combo-ba-1888-1983, lane-ba-1887/1887n/1887t/1888/1983/1989/1989c), `bak-*` 8 (2037-pre, 2909-pre390, 2914-391, 2914-pre, 2918-393local, 2918-394, 2918-pre393, lane-ba-2023), `safety/pre-merge-dev-20260417-232622`, `webui-broken-baseline`, `auto-update-version`. These are restack/safety snapshots; no deletion proposal (instruction), but they are the existing archive mechanism and could be renamed into `archive/*` for consistency.

## Counts

| class | local (65) | remote refs (11) | worktrees / detached / outside (25 total) |
|---|---|---|---|
| HARD KEEP | 14 | 4 | wt-09-bench, wt-09-cloudrec, wt-5e-adr, wt-5e-heaphold, wt-5e-cloudrec, wt-cloudrec, wt-otgw-1.x.x, main tree |
| DELETE | 27 (of which 2 sit in a clean worktree: lane-1890/wt-1890, lane-review/wt-410) | 1 (pr/699 orphan ref) | wt-1995 (detached, clean); wt-1890 and wt-410 after their sessions end |
| ARCHIVE | 12 | 1 (task1757-staged-connect) | 0 |
| KEEP | 10 (5 of them dirty → owner-must-decide) | 5 (funny-franklin + 4 NAS backup mirror refs) | wt-ba-1889, wt-architect, wt-95-docs, wt-2019t |
| owner-must-decide (dirty or other session) | lane-comms, lane-ux, lane/ba-2023, poc-tls-1689, review-sat-tasks | | wt-comms, wt-ux, wt-ba-2023, wt-tlspoc, wt-ci-combo, wt-2026, pr701 (Codex), devcheck (other session) |

Not verified: the content of the remaining `parked/ba-ui-*` commits (1828, 1838) and of the TASK-1759 commit on lane-1759a-pre-takeover (diff check did not finish); held and HARD KEEP branches were not analysed by design.
