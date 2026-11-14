---
name: release-2x
description: Publish a stable 2.x (ESP32-S3) release from the release/2.x branch: freeze a tested dev commit into release/2.x, drop the prerelease tag, build esp32-combo, bench check, annotated tag, draft-first GitHub release, NL + EN announcement
disable-model-invocation: true
---

# /release-2x: OTGW-firmware 2.x Stable Release Skill

Publish a stable 2.x release (`v2.0.0`, `v2.0.1`, `v2.1.0`, ...). This is the 2.x
counterpart of `/release` (1.x only) and the stable sibling of `/beta-release-2x`.

## Usage

```
/release-2x 2.0.0
```

The argument is the target version without `v`.

## Branch model (maintainer decision 2026-11-01, TASK-1545)

| Branch | Role |
|---|---|
| `dev` | 2.x development: alphas and betas (`/beta-release-2x`) |
| `release/2.x` | **Stable 2.x line.** Every stable 2.x tag sits on this branch. Created from dev at the first 2.x stable. |
| `otgw-1.x.x` | 1.x maintenance/LTS line |
| `main` | Stays the 1.x stable line until 1.x is end-of-life. Not touched by this flow. |

Why not "merge dev into main" as in `/release`: `main` is not an ancestor of `dev`
(1840 commits apart on 2026-11-01), so that merge is a large conflict merge, and
main still carries 1.x users.

`release/2.x` only ever receives: a fast-forward to a tested dev commit, a
cherry-picked hotfix, the production version commit and release-doc fixes. It
never merges back into dev as a whole: its production commit comments out
`_VERSION_PRERELEASE`, which dev must keep. Doc fixes go to dev by cherry-pick.

## Writing style rules

- **Never use em dashes.** All release text in English (the NL Discord post in idiomatic Dutch). No emojis.
- **Never name the OTGW32 board or the `esp32-otgw32` token publicly.**

## Checkpoints (3)

1. Robert's go, including the "Latest" decision and every waiver (Phase 0).
2. Release content review: notes, CHANGELOG section, README (Phase 2).
3. Discord messages, NL and EN (Phase 6).

Each push to `release/2.x` happens only inside this flow after checkpoint 1.

## Process

### Phase 0: Preconditions and go

1. The candidate is a dev commit that shipped as a beta (or rc) and passed field
   testing. Name it: `CAND=<commit>` (usually the last beta tag: `git rev-parse "v2.0.0-beta.N^{}"`).
   Everything between that beta and `CAND` must be bench-tested.
2. Gates on the candidate: the open release blockers in the tracking task, the 24 h
   soak on a Classic + S3 Mini, capture runs on Windows, Linux and macOS
   (TASK-1544 for the first stable). Report each as met or not met with evidence.
3. **CHECKPOINT 1** (by Discord DM via the coordinator, never as an interactive CLI question; one question per
   decision, with a recommendation. A release is a hard stop: only Robert's explicit yes as a reply to a DM that names
   the version and the action counts, TASK-1829):
   - the go for version `<v>` on `CAND`;
   - each unmet gate: waive or wait;
   - **GitHub "Latest"**: mark this 2.x stable as Latest (recommended once 2.x is
     the line new users should take), or keep the 1.x stable as Latest. Once a 2.x
     stable is Latest, every later 1.x release must use `--latest=false`.
   Record the answers in the tracking task before any push.
4. Freeze pushes: message every live peer session (`ListAgents`) to hold dev pushes and builds.
5. `git fetch origin --tags`. Previous 2.x stable (none before 2.0.0):
   `git tag --list 'v2.*' --sort=-v:refname | grep -v -- '-' | head -1`.

### Phase 1: Move release/2.x to the candidate

1. First 2.x stable (branch does not exist: `git ls-remote --heads origin release/2.x` is empty):
   ```bash
   git push origin "$CAND:refs/heads/release/2.x"
   ```
2. Later stables (feature release from dev): fast-forward only.
   ```bash
   git worktree add ../wt-release-2x origin/release/2.x
   cd ../wt-release-2x && git merge --ff-only "$CAND"
   ```
   A refused fast-forward means release/2.x has commits dev lacks (a hotfix or the
   previous production commit). Stop and ask; do not resolve it silently.
3. Patch release (2.0.x hotfix): the fix lands on dev first (normal per-task
   cadence: tests, bench, alpha tag), then `git cherry-pick -x <fix>` onto release/2.x.
4. Work in the `wt-release-2x` worktree from here on. Fresh worktree:
   `git submodule update --init --recursive src/libraries/OpenTherm src/libraries/SimpleTelnet`.
   Never check out release/2.x in the main dev tree.

### Phase 2: Release content (CHECKPOINT 2)

1. `docs/releases/RELEASE_NOTES_<v>.md`: start from the last beta's notes. Remove
   the beta disclaimer; keep the SAT status as it really is; BREAKING changes since
   the 1.x line (first stable) or since the previous 2.x stable; Known issues;
   upgrade routes (USB first flash for a new S3 Mini, `/update` with the zip on a
   2.x unit, `--migrate` for an old layout); how to report.
2. CHANGELOG: on release/2.x rename `## [Unreleased]` to `## [<v>] - <date>`, add an
   empty `[Unreleased]` and the link refs. The beta sections stay below it.
3. README: no beta disclaimer, highlights for `<v>`.
4. `/update-docs-2x --release <v>` in `../wt-release-2x` for the manuals, API docs,
   C4 docs, `BREAKING_CHANGES.md` and the notes draft. It does not commit and skips
   cleanup; its files are part of this checkpoint and of the Phase 7 cherry-pick.
5. Every claim checked against code (`file:line`) or an ADR. Checks:
   ```bash
   grep -n -E "TODO|TBD|XXX|—" docs/releases/RELEASE_NOTES_<v>.md
   grep -o -E "\]\([^)]+\)" docs/releases/RELEASE_NOTES_<v>.md | grep -v -E "\(https?://|\(#"
   ```
6. **CHECKPOINT 2**: show the categorized changes, the contributor list (GitHub PRs,
   #alpha-testing, #beta-testing; maintainer excluded) and the generated texts. Wait
   for approval. Commit the docs on release/2.x.

### Phase 3: Production version commit

1. Comment out the prerelease, so the build is a clean `v<v>`:
   ```bash
   sed -i 's|^#define _VERSION_PRERELEASE |//#define _VERSION_PRERELEASE |' src/OTGW-firmware/version.h
   python scripts/autoinc-semver.py src/OTGW-firmware --update-all --print-updated
   grep -n -E "PRERELEASE|SEMVER_FULL" src/OTGW-firmware/version.h   # //#define ..., "<v>+<hash>"
   ```
   Do NOT use `--prerelease none`: the define then still exists, and
   `#if defined(_VERSION_PRERELEASE)` keeps the debug-only code in
   `networkStuff.ino` (lines ~205-414) and the prerelease telnet debug in
   `OTGW-firmware.ino` switched on.
   For a new core version (2.0.0 -> 2.0.1) set `_VERSION_MAJOR/MINOR/PATCH` by hand first.
2. `python evaluate.py --quick`: 0 failed.
3. Commit. The pre-commit hook only accepts a changed `#define _VERSION_PRERELEASE`
   line, so this single commit uses the documented bypass, with the reason:
   ```bash
   OTGW_BUMP_HOOK_DISABLE=1 git commit -m "chore(release): v<v> production build (prerelease tag commented out)

   Hook bypass: the bump hook cannot see a commented-out prerelease as a bump."
   ```

### Phase 4: Build and bench check

1. Announce the build; one build at a time; esp32-combo only:
   ```bash
   OTGW_BUILD_JOBS=4 ./.build-python/python.exe build.py > "$TMP/build-<v>.log" 2>&1
   grep -c "Successfully created ESP32S3 image" "$TMP/build-<v>.log"   # 2 on a cold build (bootloader + firmware), 1 on a warm one (TASK-1921: the bootloader is cached)
   grep "Build completed successfully" "$TMP/build-<v>.log"
   ls -la build | grep "<v>+"                                            # no -beta/-alpha in the name
   ```
   The worktree needs its own `.build-python` (run `build.bat` once in it). Do not
   run the main tree's `.build-python/python.exe build.py` from a worktree: that
   embedded Python (`python312._pth`) imports the main tree's `config.py`, so
   `BUILD_DIR` is the main tree's `build/`, which it cleans (TASK-1553). If you
   must, put the worktree first on `sys.path`:
   `python.exe -c "import sys,runpy; sys.path.insert(0, r'<wt>'); sys.argv=['build.py']; runpy.run_path(r'<wt>\build.py', run_name='__main__')"`.
   A fresh worktree also needs `mkdir build` first.
2. Bench, by the bench-tester agent (`otgw-bench-tester`, maintainer directive 2026-11-11; request it
   through the coordinator), at least the Classic + S3 Mini, preferably two units:
   - save settings, OTA with `scripts/ota_flash.py` (exit 0, `changed: none`);
   - the user route: `/update` with the release zip;
   - `device/info` shows `<v>+<hash>` with no prerelease part, PIC detected;
   - telnet: the prerelease debug output is off;
   - after 30 minutes or more: bootcount unchanged, no crashlog.
3. A failed bench check: fix on dev first, cherry-pick, new production commit, rebuild.

### Phase 5: Push, tag, draft, publish

1. `git push origin HEAD:release/2.x` (after checkpoint 1; never force).
2. Tag and verify:
   ```bash
   git tag -a v<v> -m "v<v>: <short title>"
   git push origin v<v>
   git rev-parse --short "v<v>^{}"
   ```
3. SHA256SUMS (scratchpad) over the flash zip, `.ino.bin`, `.littlefs.bin`,
   `-merged-full.bin`, `.elf`, `OTGW-safeboot-<v>.bin`.
4. Draft:
   ```bash
   gh release create v<v> --draft --verify-tag --title "v<v> - <Short Title>" \
     --notes-file docs/releases/RELEASE_NOTES_<v>.md <assets> "$SCRATCH/SHA256SUMS"
   gh release view v<v> --json isDraft,isPrerelease,assets --jq .
   ```
   Not a prerelease. Sizes equal `build/`; the zip holds the capture tools and the
   flash scripts. Anything off: stop and ask.
5. Publish (draft URL is `untagged-<id>`):
   ```bash
   gh release edit untagged-<id> --draft=false --tag v<v> --latest=<true|false per checkpoint 1>
   gh api repos/rvdbreemen/OTGW-firmware/releases/latest --jq .tag_name
   ```
   Download the zip from the release and compare its SHA-256.
6. Release the peers: "v<v> tag out".

### Phase 6: Announce (CHECKPOINT 3)

1. Draft both posts: Dutch in `#nederlandse-ondersteuning` (`815561033036333076`),
   English in `#english-support` (`931267109726593116`). A short note in
   `#beta-testing` (`914498730001072149`) for the testers, with thanks. Each under
   2000 characters: version, what it is, who should update and how, the release
   link, contributor credits.
2. **CHECKPOINT 3**: show the posts; send after approval with
   `mcp__discord-mcp__send_message`; read each channel back.

### Phase 7: Back to dev

1. Cherry-pick the release docs (notes, CHANGELOG section, README) onto dev with
   `git cherry-pick -x`. Not the production version commit.
2. On dev, start the next cycle: set the next version (e.g. 2.1.0) and
   `_VERSION_PRERELEASE alpha.1` with `autoinc-semver.py --prerelease alpha.1 --update-all`;
   on dev the CHANGELOG keeps an empty `[Unreleased]` above the `[<v>]` section.
   Ask Robert for the next version number when it is not obvious.
3. Close the tracking task with evidence per AC (build lines, bench numbers, release
   URL, SHA match, post links). Remove `../wt-release-2x`.

## Post-publication corrections

Assets are immutable; the body is not. Fix the notes on release/2.x, commit, push,
`gh release edit v<v> --notes-file docs/releases/RELEASE_NOTES_<v>.md`, and
cherry-pick the fix to dev. A broken asset means a new patch release.

## Known traps

1. Immutable releases: no asset change after publish. Draft first.
2. A deleted release burns the tag name forever.
3. `--prerelease none` keeps the prerelease debug code on: comment the define out.
4. The bump hook rejects the production commit: bypass that one commit, say why.
5. Merging release/2.x into dev would comment out dev's prerelease: cherry-pick docs only.
6. The "Latest" flag follows the last publish, not the highest version: set it explicitly on every release, on both lines.

## Important rules

- Never touch `main` in this flow. Never force-push.
- Build, evaluator and bench gates are mandatory.
- Do not flash the PIC. Clear any test HTTP password before ending. Never print the webhook URL.
