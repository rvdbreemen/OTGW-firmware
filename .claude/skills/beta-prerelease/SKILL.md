---
name: beta-prerelease
description: Publish an OTGW-firmware beta prerelease — bump _VERSION_PRERELEASE, push to otgw-1.x.x, tag, and let CI build + publish the GitHub prerelease
disable-model-invocation: true
---

# /beta-prerelease - OTGW-firmware Beta Prerelease Skill

Publish a single beta build to field testers in Discord `#beta-testing`. Lightweight, repeatable many times within one minor cycle. Does NOT merge to main, does NOT touch the stable `_SEMVER_CORE`.

## Usage

```
/beta-prerelease
```

No arguments. Reads the current `_VERSION_PRERELEASE` and either increments it via `bin/bump-prerelease.sh` or adopts it unchanged when a previous run already bumped but never shipped the tag (Phase 2).

## Token-efficiency rules (apply throughout all phases)

- **P1 Build output**: Tee to `logs/build_beta.log`, NOT to anything under `.tmp/`. `build.py` wipes `.tmp/` at the end of a run and emits `WARNING: Could not remove ...\.tmp: [WinError 32]` when it collides with a log file you are holding open. Only read the log if the build did not verify (Phase 4).
- **P2 Phase 3 reads**: Use targeted `grep`/`sed` to extract only the relevant section from each file. Never read README, CHANGELOG, or RELEASE_NOTES in full.
- **P3 Phase 3 writes**: Edit each file and note "Updated." immediately. Do NOT keep all edited content in context — keep only the filename.
- **P4 Known Traps**: Summarized as 8 bullets below. Read `.github/workflows/beta-prerelease.yml` only if a trap is actually hit.
- **P5 Phase 8 CI poll**: Use `gh run watch <run-id>` instead of opening a browser. The run ID is mandatory, see Trap 6.

## When to use

Run when a firmware change under `src/OTGW-firmware/**` or `src/libraries/**` is committed and ready for field testing. Do NOT use for docs-only commits or full releases to `main` (use `/release <version>` for those).

## How this differs from /release

| Aspect | `/release` | `/beta-prerelease` |
|---|---|---|
| Target branch | `dev` then merge to `main` | `otgw-1.x.x` only |
| Bumps `_SEMVER_CORE` | yes | no |
| GitHub release | stable, not prerelease | prerelease: true |
| Discord channels | `#nederlandse-ondersteuning`, `#english-support` | `#beta-testing` |
| Mandatory checkpoints | 2 | 1 (Discord announcement) |

## Writing style rules

- **Never use em dashes** — use colons, periods, commas, or parentheses instead.
- **All release text MUST be in English** (international audience).
- **No emojis** in release notes or Discord posts.

## Process

### Phase 0: Prepare — clean state on otgw-1.x.x

The 1.x maintenance/LTS line lives in its own worktree. Run this skill FROM the
`wt-otgw-1.x.x` worktree. Do NOT `git checkout otgw-1.x.x` inside the dev tree:
the branch is already checked out in the worktree and the checkout will fail.

**Worktree preflight (run this first, every time).** Every relative path in this
skill (`bin/bump-prerelease.sh`, `build.bat`, `src/OTGW-firmware/version.h`,
`.githooks/`) resolves against the *current working directory*, not against the
repo. A tool call that lands in a sibling worktree silently bumps and tags the
wrong branch, so anchor first:

```bash
cd "$(git rev-parse --show-toplevel)"          # worktree root, not the common .git
git rev-parse --abbrev-ref HEAD                # must print otgw-1.x.x
git rev-parse --git-dir --git-common-dir       # .../worktrees/wt-otgw-1.x.x + .../OTGW-firmware/.git
git config core.hooksPath                      # .githooks (relative: resolves per worktree)
```

Facts that follow from the worktree layout and that the phases below depend on:

- `core.hooksPath` is the relative value `.githooks`, so hooks come from THIS
  worktree's copy. A hook fix in the dev tree does not apply here.
- Tags and remotes live in the shared common dir, so `git fetch --tags` in any
  worktree is visible in all of them. Tag collisions are repo-wide.
- `backlog/` on this branch is not the dev tree's backlog. Cross-tree TASK-NNN
  references cannot satisfy the commit-msg hook here (see Phase 6).

1. Confirm the branch and anchor the cwd (preflight above).
2. Verify state. Do NOT require a fully clean `git status`: this worktree
   routinely carries untracked tool droppings (`graphify-out/`,
   `.external-reviews/`, `docs/adr/.adr-kit-state.json{,.lock}`). What must hold:
   no *tracked* modification outside the firmware change you are about to ship,
   plus the version stamp churn from an earlier local build. Untracked artefacts
   stay untracked: never `git add -A`, never commit a deletion you did not
   intend (`git checkout -- <path>` to restore, for example
   `.claude/scheduled_tasks.lock`).
3. Sync, and count what a push would carry:
   ```bash
   git pull --ff-only origin otgw-1.x.x
   git rev-list --left-right --count origin/otgw-1.x.x...HEAD   # behind<TAB>ahead
   ```
   A non-zero ahead count is normal on this branch (backlog commits accumulate
   between releases), but read `git log --oneline origin/otgw-1.x.x..HEAD` before
   Phase 6 so you know what you are publishing. Non-zero *behind* means stop and
   reconcile: never tag a commit that is not a descendant of origin.
4. Detect the latest prerelease tag:
   ```bash
   git fetch --tags
   PREV_TAG=$(git tag --list 'v*-*.*' --sort=-v:refname | head -1)
   ```
5. Detect the latest public stable release (store as `LATEST_PUBLIC`):
   ```bash
   LATEST_PUBLIC=$(gh release view --json tagName --jq '.tagName' 2>/dev/null \
     || git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname | grep -v -- '-' | head -1)
   ```
6. Read the current version, and keep both values (Phase 2, 7 and 9 need them):
   ```bash
   SEMVER_CORE=$(grep '_SEMVER_CORE ' src/OTGW-firmware/version.h | awk -F'"' '{print $2}')
   CUR_PRERELEASE=$(grep -oP '(?<=_VERSION_PRERELEASE ).*' src/OTGW-firmware/version.h | tr -d '\r')
   echo "${SEMVER_CORE} / ${CUR_PRERELEASE}"
   ```
   The `tr -d '\r'` matters: `version.h` carries CRLF, and a trailing CR silently
   corrupts every tag name and grep you build from the value.
7. Enumerate what GitHub already published for this `_SEMVER_CORE`, so Phase 2
   can tell "not yet shipped" from "tag burned" (Trap 2, Trap 7):
   ```bash
   gh api repos/rvdbreemen/OTGW-firmware/releases --paginate --jq '.[].tag_name' \
     | grep "^v${SEMVER_CORE}-" || echo "(no releases for ${SEMVER_CORE})"
   ```
   Neither `git tag --list` nor `gh release list --limit N` is authoritative
   here: a tag can exist with no release, and a release can be burned with no
   tag left behind. Only the paginated API listing answers this.

### Phase 1: ADR validation

Skip by default. Only pause if the staged firmware change introduces a new architectural pattern, dependency, or NFR shift. Most beta cycles do not need this gate.

### Phase 2: Bump prerelease (or adopt an unshipped one)

**Decide before running the helper.** The bump is not always owed. Take the
value already in `version.h` (Phase 0 step 6) and ask two questions:

1. Is `v${SEMVER_CORE}-${CURRENT_PRERELEASE}` absent from the Phase 0 step 7
   release listing (never published, and never burned)?
2. Is everything between the commit that set that value and `HEAD` release noise
   only, i.e. no firmware surface change?
   ```bash
   CUR=$(grep -oP '(?<=_VERSION_PRERELEASE ).*' src/OTGW-firmware/version.h | tr -d '\r')
   BUMP_COMMIT=$(git log -1 --format=%H -G"_VERSION_PRERELEASE ${CUR//./\\.}" \
                   -- src/OTGW-firmware/version.h)
   git log --oneline "${BUMP_COMMIT}..HEAD"
   git diff --name-only "${BUMP_COMMIT}..HEAD" -- src/OTGW-firmware/ src/libraries/
   ```
   Empty `git diff` output means the firmware surface is identical to the bump
   commit, so whatever landed since is backlog or docs noise.

   Use `-G` (diff-content regex), NOT `-S` (pickaxe on occurrence *count*). A
   bump rewrites the value on an existing line, so the count of
   `_VERSION_PRERELEASE` never changes and `-S` walks right past it: on this repo
   it returns a 2632-era build commit instead. Escaping the dot in the value
   keeps `beta.5` from also matching `betaX5`.

**Both yes: adopt it, do NOT bump.** `NEW_PRERELEASE` = the current value, skip
straight to Phase 3. A previous run that committed the bump but never pushed the
tag leaves exactly this state. Bumping over it means that beta number never
exists for testers and you rewrite `version.h` for nothing.

**Otherwise: bump.**

```bash
bin/bump-prerelease.sh
```

Prints the transition (e.g. `beta.3 -> beta.4`). Store the new value as `NEW_PRERELEASE`. The helper does NOT git-add — you stage in Phase 6.

If Phase 0 step 7 shows the next number is burned (Trap 2), run the helper again
to skip past it and record the skipped tag in the Phase 6 commit message.

Assemble the tag: `TAG="v${SEMVER_CORE}-${NEW_PRERELEASE}"`

### Phase 3: Refresh README + CHANGELOG + RELEASE_NOTES (mandatory, P2, P3)

The GitHub Action reads these files at the tagged commit. Stale narrative at the tag = stale release page (Trap 1). Refresh immediately after the bump so `NEW_PRERELEASE` is known.

**Staleness check (P2 — targeted extractions, not full reads):**

```bash
# 1. Commits since last public release (the change set to account for)
git log --pretty=format:'%h %s' "${LATEST_PUBLIC}..HEAD" -- src/OTGW-firmware/ src/libraries/ docs/

# 2. Existing narrative — CHANGELOG [Unreleased] is the rolling beta log on the
#    1.x line. (The README "What's New in v<stable>" sections are refreshed at
#    STABLE release, not per beta, and there is no RELEASE_NOTES_*-beta file.)
grep -A 35 "## \[Unreleased\]" CHANGELOG.md | head -40
```

**Decision:**
- Every commit subject since `LATEST_PUBLIC` appears under CHANGELOG `## [Unreleased]` → pass silently, continue to Phase 4.
- Any commit missing → refresh the CHANGELOG now. Stop and ask only if the gap is ambiguous; otherwise edit in-session.

**Authoring rules (P3 — write immediately, keep only filename in context):**

1. `CHANGELOG.md` — append under `## [Unreleased]` using Keep-a-Changelog headings (`### Added/Changed/Fixed/Removed/Documentation`). One bullet per change, with ADR/TASK/PR/GH-issue reference. → note "Updated." This is the only mandatory narrative for a 1.x beta.
2. `README.md` — leave untouched. The 1.x README's "What's New in v<stable>" sections are refreshed at STABLE release (via `/release`), not per beta.
3. `RELEASE_NOTES_<next-stable>.md` (e.g. `RELEASE_NOTES_1.7.1.md`) — optional during a beta cycle (authored in full at stable release). Update it now only if you keep a running draft. → note "Updated."

Skip the CHANGELOG edit only when this is a re-cut at the same change surface (previous tag hit Trap 2). Note the reason in the commit message.

### Phase 4: Build verification (P1)

Build through the wrapper, in PowerShell, with no `--firmware` flag. The wrapper
bootstraps the Python 3.12 venv and the toolchain; `python build.py` invoked
directly can pick up the wrong interpreter. CI builds firmware *and* filesystem
for the release, so build both here or the gate does not cover what ships.

Record the pre-build artefact mtimes first, so "fresh" is a comparison and not a
guess:

```powershell
New-Item -ItemType Directory -Force logs | Out-Null
Get-ChildItem build/*.bin | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
.\build.bat 2>&1 | Tee-Object -FilePath logs\build_beta.log | Select-Object -Last 6
```

**Exit code 0 does NOT mean the firmware compiled.** `build.py` can swallow a
per-target failure and still exit 0. Verify all three of these:

```powershell
Get-ChildItem build/*.bin | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Select-String -Path logs\build_beta.log -Pattern "Build completed successfully" | Select-Object -Last 1
```

1. `build/*.ino.bin` and `build/*.littlefs.bin` both carry a mtime from this run.
2. Their filenames embed the expected `${SEMVER_CORE}-${NEW_PRERELEASE}` (the
   `+<githash>` suffix will track whatever commit you are on, which is fine).
3. The log contains the literal `Build completed successfully!` line.

Any of the three missing: read `logs\build_beta.log` for diagnosis, fix,
retry. Do NOT push a tag on an unverified build.

Note: `build.bat` re-stamps `version.h` and `data/version.hash` with a fresh
build number, timestamp and githash on every run. That churn is expected and is
committed in Phase 6. It does not touch `_VERSION_PRERELEASE`.

Run this in the background if it is slow, then verify on the notification. Do
NOT chain `sleep` calls to poll it.

### Phase 5: Evaluator

```bash
python evaluate.py --quick
```

Must show no new failures. Pre-existing baseline failures unrelated to this change: document in the commit message.

### Phase 6: Commit and push to otgw-1.x.x

**Split unrelated work out first.** If Phase 0 step 2 found tracked changes that
are not part of this beta (a `CLAUDE.md` note, an ADR provenance line, a script
tweak), commit those separately BEFORE the release commit. Two reasons: the
release commit should be reviewable as "what shipped in this beta", and a hook
that objects to those paths then blocks a docs commit instead of your release.

```bash
# Review what is actually dirty, then stage by explicit path.
git status --short
git diff --ignore-cr-at-eol --name-only    # --ignore-cr-at-eol hides EOL-only churn on Windows
```

**Stage by explicit path. Never `git add $(git diff --name-only)`** and never
`git add -A`: the sweep breaks on paths containing spaces and happily stages any
unrelated tracked file that another tool touched in this frequently-dirty
worktree.

```bash
# The 1.x bin/bump-prerelease.sh updates version banners across ~24 files and
# does NOT auto-stage. For a bump run, list the banner files it rewrote plus:
git add CHANGELOG.md src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
# ...and your firmware change paths, spelled out.

git commit -F - <<'EOF'
chore(release): ${NEW_PRERELEASE}

<what is in this beta, and why the version stamp moved>

Gates: build.bat verified (fresh build/*.ino.bin + *.littlefs.bin, literal
"Build completed successfully"). evaluate.py --quick <N>/<M> pass; any
pre-existing baseline failure named here with the evidence that it pre-exists.
EOF

git push origin otgw-1.x.x
```

Push the branch BEFORE the tag (Phase 7). A tag pushed to a commit the remote
does not have yet makes CI check out a ref that resolves to nothing.

If the pre-commit hook blocks: re-stage `version.h` + `data/version.hash` and retry. Do NOT bypass with `OTGW_BUMP_HOOK_DISABLE=1`.

**Commit-msg task hook.** `.githooks/commit-msg` demands a TASK-NNN whose
`backlog/tasks/task-NNN-*.md` is tracked in THIS worktree, and it triggers on
`docs/**` as well as firmware paths. The backlog lives in the dev tree, so a 1.x
commit usually cannot satisfy it. Exemptions, cheapest first:

- Release commit: `chore(release): ...` subject prefix (exempt).
- Docs or provenance commit: `chore(housekeeping): ...` prefix, or `[no-task]`
  anywhere in the body.
- Genuinely referencing a cross-tree TASK-NNN: `OTGW_TASK_HOOK_DISABLE=1`.

The hook prints the full exemption list when it blocks. Read it rather than
reaching for `--no-verify`.

### Phase 7: Create and push the prerelease tag

```bash
SEMVER_CORE=$(grep '_SEMVER_CORE ' src/OTGW-firmware/version.h | awk -F'"' '{print $2}')
TAG="v${SEMVER_CORE}-${NEW_PRERELEASE}"

git tag -a "${TAG}" -m "Beta prerelease ${NEW_PRERELEASE}"
git push origin "${TAG}"
```

The push fires `.github/workflows/beta-prerelease.yml`. CI builds firmware + filesystem, creates a GitHub prerelease, uploads `.ino.bin`, `.littlefs.bin`, `SHA256SUMS`, flash scripts, and the flash-bundle zip.

### Phase 8: Wait for the GitHub Action (P5)

**Pass the run ID. `gh run watch` without one exits 0 having watched nothing**
(Trap 6). Resolve the run that the tag push actually fired, then watch that:

```bash
RUN_ID=$(gh run list --workflow=beta-prerelease.yml --limit 1 \
           --json databaseId --jq '.[0].databaseId')
echo "watching ${RUN_ID}"
gh run watch "${RUN_ID}" --exit-status --compact
```

`gh run watch` exiting 0 is necessary but not sufficient. Confirm independently,
against the API, before believing the release shipped:

```bash
gh run view "${RUN_ID}" --json status,conclusion --jq '"\(.status) \(.conclusion)"'
# want: completed success

gh release view "${TAG}" --json tagName,isPrerelease,isDraft,assets \
  --jq '{tag: .tagName, prerelease: .isPrerelease, draft: .isDraft, assets: [.assets[].name]}'
```

Required: `prerelease: true`, `draft: false`, and all six assets: `*.ino.bin`,
`*.littlefs.bin`, `SHA256SUMS`, `flash_otgw.sh`, `flash_otgw.bat`,
`OTGW-firmware-*-flash-bundle.zip`. `draft: true` means CI stopped between
attaching assets and flipping the draft flag (Trap 1).

`release not found` while the run still reads `in_progress` is not a failure, it
just means you asked early. `release not found` after `completed success` is.

On failure: inspect logs with `gh run view "${RUN_ID}" --log-failed`, fix the
issue, and re-run via `workflow_dispatch` or push a new tag.

CI writes the release body itself. To replace it with your own narrative:
`gh release edit "${TAG}" --notes-file <file>`.

### Phase 9: Discord announcement (CHECKPOINT)

Prepare announcement for `#beta-testing` (channel ID `914498730001072149`). Diff link points at `LATEST_PUBLIC` (testers want to see what changed since the last stable, not since a previous beta).

```
Beta ${NEW_PRERELEASE} is up.

Version: ${SEMVER_CORE}-${NEW_PRERELEASE}
What is new: <one or two sentences>
Download: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/${TAG}
Diff vs ${LATEST_PUBLIC}: https://github.com/rvdbreemen/OTGW-firmware/compare/${LATEST_PUBLIC}...${TAG}
Changelog: https://github.com/rvdbreemen/OTGW-firmware/blob/${TAG}/CHANGELOG.md

Please flash and report findings here (good and bad).
```

**CHECKPOINT: Show the announcement to the user before sending.** Wait for a
real reply. A background-task notification (build finished, `gh run watch`
returned) is not approval, and neither is your own earlier message saying you
would post it. Nothing goes to Discord without a human "yes" in this turn or a
later one.

## Dry-run (testing without publishing)

Do the dry-run in a throwaway worktree, not by branching inside the release
worktree: a stray `test/...` branch checked out where `otgw-1.x.x` belongs is how
a later phase tags the wrong ref.

```bash
git worktree add ../wt-beta-dryrun -b test/beta-prerelease-dryrun otgw-1.x.x
cd ../wt-beta-dryrun
git tag -a v0.0.0-beta.dryrun -m "dryrun" && git push origin v0.0.0-beta.dryrun
# watch the Action (Phase 8 form, with the run ID), then clean up:
gh release delete v0.0.0-beta.dryrun --yes
git push --delete origin v0.0.0-beta.dryrun && git tag -d v0.0.0-beta.dryrun
cd ../wt-otgw-1.x.x
git worktree remove ../wt-beta-dryrun && git branch -D test/beta-prerelease-dryrun
```

Remember Trap 2: `v0.0.0-beta.dryrun` is burned once published and deleted. Use a
fresh suffix per dry-run.

## Known traps (P4 — full detail in `beta-prerelease.yml`)

1. **Trap 1: Immutable-releases locks publish** — upload after `gh release create` returns HTTP 422. Workaround already in CI: draft-first, attach all assets, then flip `--draft=false`. If you hit this manually, re-tag under the next beta number.
2. **Trap 2: Deleted immutable release reserves the tag forever** — even after deletion, the tag cannot be reused. Bump the prerelease number and note the skipped tag in the commit message. Detect it with the Phase 0 step 7 API listing: `git tag --list` cannot see a burned-and-deleted tag, and `gh release list --limit N` can hide prereleases behind the stable ones it prints first.
3. **Trap 3: `GITHUB_TOKEN` events do not chain** — `release-assets.yml` (triggered by `release: published`) does not fire when the release is created by `GITHUB_TOKEN`. `beta-prerelease.yml` is therefore self-contained (generates SHA256SUMS + zip + flash scripts itself).
4. **Trap 4: Diff link to previous beta hides what testers care about** — always link vs `LATEST_PUBLIC` (the non-prerelease "Latest" release), not the previous beta tag.
5. **Trap 5: Stale narrative at tagged commit is permanent** — Phase 3 refresh runs before the tag is pushed for this reason. Skipping Phase 3 = stale release page that cannot be edited after publish.
6. **Trap 6: `gh run watch` with no run ID fakes a green CI wait** — the ID-less form needs a TTY for its run picker. Without one it prints the flag usage block and **exits 0 immediately**, so a still-`in_progress` run reads as finished and you announce a release that does not exist yet. Always resolve the ID first (Phase 8) and cross-check with `gh run view`. Same class as `build.py` exiting 0 on a failed compile: an exit code that does not mean what it looks like.
7. **Trap 7: the bump may already have happened** — a previous run can leave `_VERSION_PRERELEASE` bumped and committed but the tag never pushed, which `git tag --list` shows as "this beta does not exist". Bumping again buries a perfectly good, never-shipped beta number and rewrites `version.h` for nothing. Phase 2 decides between adopt and bump; the discriminator is the Phase 0 step 7 release listing plus an empty firmware diff since the bump commit.
8. **Trap 8: relative paths follow the cwd, not the repo** — `bin/bump-prerelease.sh`, `build.bat` and `.githooks/` all resolve against the working directory. A tool call that starts in a sibling worktree (dev, or one of the `wt-*` experiment trees) bumps and tags the wrong branch without complaining. Anchor with `cd "$(git rev-parse --show-toplevel)"` and assert the branch, every time (Phase 0 preflight).

## Important rules

- **Never use em dashes** in any generated text.
- **Anchor the worktree before anything else**: `cd "$(git rev-parse --show-toplevel)"` plus a branch assertion (Trap 8).
- **Always push to remote after every commit**, and push the branch before the tag.
- **Never force-push to otgw-1.x.x**.
- **Build and evaluator gates are mandatory** — do not push a tag if either is red.
- **Never trust an exit code as a gate.** `build.py` exits 0 on a failed target and `gh run watch` exits 0 without watching. Verify on artefacts, log lines and API state instead (Phase 4, Phase 8).
- **Stage by explicit path.** No `git add -A`, no `git add $(git diff --name-only)`.
- **One checkpoint**: the Discord announcement in Phase 9. A background-task notification is not approval.
- **Do NOT bypass the bump-check hook** with `OTGW_BUMP_HOOK_DISABLE=1` — if the hook blocks, you forgot to stage `version.h` / `data/version.hash`.
