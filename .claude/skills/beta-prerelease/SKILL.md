---
name: beta-prerelease
description: Publish an OTGW-firmware beta prerelease — bump _VERSION_PRERELEASE, push to dev, tag, and let CI build + publish the GitHub prerelease
disable-model-invocation: true
---

# /beta-prerelease - OTGW-firmware Beta Prerelease Skill

Publish a single beta build to the field testers in Discord `#beta-testing`. Lightweight, repeatable many times within one minor cycle. Parallels the `/release` skill but does NOT merge to main, does NOT touch README, and does NOT bump `_SEMVER_CORE`.

## Usage

```
/beta-prerelease
```

No arguments. The skill auto-detects the current `_VERSION_PRERELEASE` in `src/OTGW-firmware/version.h` and increments it via `bin/bump-prerelease.sh`.

## When to use this skill

Run `/beta-prerelease` when:

- A firmware change under `src/OTGW-firmware/**` or `src/libraries/**` is committed and ready for field testing.
- The pre-commit hook (`.githooks/pre-commit`) has already enforced the in-commit bump, OR you are about to commit and want a clean orchestrated cycle.
- The change is not yet ready for a full release to `main`. For a full release use `/release <version>` instead.

Do NOT use this skill for:

- Docs-only commits (`*.md`, `docs/**`, `backlog/**`, `.claude/**`, `scripts/**`). Those are exempt from the bump and do not need a prerelease tag.
- Full releases to `main`. Use `/release <version>`.

## How this differs from /release

| Aspect | `/release` | `/beta-prerelease` |
|---|---|---|
| Target branch | `dev` then merge to `main` | `dev` only |
| Bumps `_SEMVER_CORE` | yes (1.5.0 -> 1.6.0) | no |
| Bumps `_VERSION_PRERELEASE` | sets to `beta.0` after release | increments by 1 |
| Edits `README.md` | yes (What's New section) | yes (Phase 1 staleness gate refreshes the "What's new on dev" section when needed) |
| Maintains `RELEASE_NOTES_*.md` | yes (per stable: `RELEASE_NOTES_<version>.md`) | yes (per beta line: `RELEASE_NOTES_<base>-beta.md`); CI inlines the digest above the `<!-- digest:end -->` sentinel into the release body |
| GitHub release marked prerelease | no | yes |
| Discord channels | `#nederlandse-ondersteuning`, `#english-support` | `#beta-testing` |
| Frequency | once per minor cycle | many times per minor cycle |
| Mandatory checkpoints | 2 | 1 (Discord announcement) |

## Writing style rules

- **Never use em dashes** in any output: not in release notes, GitHub release body, Discord messages, commit messages, or conversation text. Use colons, periods, commas, or parentheses instead.
- **All release notes and GitHub release messages MUST be in English** (international audience).
- **No emojis** in release notes or Discord posts unless the existing format uses them.

## Process

Ten phases. The narrative-refresh gate (Phase 1) runs **before** the version bump so a stale README or CHANGELOG cannot ride out under a fresh tag. Only **one mandatory checkpoint** (the Discord announcement at the end). All other phases proceed automatically unless the staleness check fires or something unexpected happens.

### Phase 0: Prepare - clean state on dev

1. **Ensure you are on `dev`**: `git checkout dev`
2. **Verify clean state**:
   - `git status` must show `nothing to commit, working tree clean` (or only the firmware change about to be bumped+committed)
   - `git pull origin dev` to incorporate any remote changes
3. **Detect the latest prerelease tag**:
   ```bash
   git fetch --tags
   git tag --list 'v*-*.*' --sort=-v:refname | head -1
   ```
   Store this as `PREV_TAG` for the release body diff link in Phase 7.
4. **Detect the latest PUBLIC stable release**:
   ```bash
   LATEST_PUBLIC=$(gh release view --json tagName --jq '.tagName' 2>/dev/null \
                   || git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname \
                        | grep -v -- '-' | head -1)
   ```
   Store this as `LATEST_PUBLIC`. Used by Phase 1 for the staleness diff and by the Phase 9 Discord announcement.
5. **Read current version**: `grep _VERSION_PRERELEASE src/OTGW-firmware/version.h` and store the current value (e.g. `beta.3`).

### Phase 1: README + CHANGELOG staleness gate (mandatory, runs first)

**Why this is now Phase 1, not Phase 2.5.** The GitHub Action checks out the repo at the tag commit and reads `README.md`, `CHANGELOG.md`, and `RELEASE_NOTES_<base>-beta.md` to compose the release body. If those files are stale, the tag locks the stale narrative onto the release page (immutable releases policy — see Trap 1). The check therefore gates the bump: a stale narrative blocks Phase 2 and downstream phases until the user (or this skill) refreshes the docs.

**The staleness check (run automatically):**

1. List commits between `LATEST_PUBLIC` and `HEAD` on `dev`, subject only:
   ```bash
   git log --pretty=format:'%h %s' "${LATEST_PUBLIC}..HEAD" -- src/OTGW-firmware/ src/libraries/ docs/ data/
   ```
   This is the set of changes the next release page should account for. Filtering paths trims trivial chore commits.
2. Read the existing narrative bullets:
   - `README.md` "What's new on dev (since <LATEST_PUBLIC>)" section (between the dev banner and the "What's New in v<stable>" anchor).
   - `CHANGELOG.md` `## [Unreleased]` section.
   - `RELEASE_NOTES_<base>-beta.md` digest region (above the `<!-- digest:end -->` sentinel), if the file exists.
3. **Heuristic match (not regex parsing):** for each commit subject in step 1, look for a topical match in any of the three narratives. A match can be by ADR number, TASK ID, PR number, or a substring of the subject. Missing items are listed as candidates for the user.
4. **Decision:**
   - **All commits accounted for** ⇒ pass silently, continue to Phase 2.
   - **One or more commits missing from the narrative** ⇒ STOP. Print the missing-commit list, the file(s) that should receive entries (README and/or CHANGELOG and/or RELEASE_NOTES), and ask the user whether to (a) refresh the docs in this session before bumping, (b) skip the missing items deliberately (justify in the commit message), or (c) abort the beta cut.
5. **Authoring rules** when the gate fires and you refresh in-session:
   - `CHANGELOG.md` — append under `## [Unreleased]` using [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) headings (`### Added`, `### Changed`, `### Fixed`, `### Removed`, `### Documentation`). One short bullet per change, with the ADR/TASK/PR reference in parentheses.
   - `README.md` — refresh the dev-banner version label and the "What's new on dev (since <LATEST_PUBLIC>)" section. Do NOT touch the "What's New in v<stable>" section; that belongs to the last shipped stable and stays as the historical anchor.
   - `RELEASE_NOTES_<base>-beta.md` — if the line carries a narrative bigger than the CHANGELOG bullets warrant, maintain a per-line notes file at the repo root. The CI workflow inlines the content above the `<!-- digest:end -->` sentinel into the release body under a "What's new since the last public release" heading; the link below the inlined teaser points to the full file for the long form.

**Skip allowed only when**: the bump is a re-cut at the same change surface (for example, the previous tag hit the immutable-release trap and we are reissuing under a fresh number). The narrative did not change, only the tag did. Note the reason in the commit message and proceed straight to Phase 2.

### Phase 2: ADR validation

Skip by default. Only stop and create an ADR if the staged firmware change introduces a new architectural pattern, a new dependency, or shifts an NFR (heap budget, MQTT topic shape, settings schema). Most beta cycles do not need this gate.

**Conditional stop:** Only pause for user input if an ADR is actually needed.

### Phase 3: Bump prerelease

Run the bump helper from the project root:

```bash
bin/bump-prerelease.sh
```

The helper rewrites `src/OTGW-firmware/version.h` (`_VERSION_PRERELEASE`, `_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION`) and `src/OTGW-firmware/data/version.hash`. It prints the transition (e.g. `beta.3 -> beta.4`). Store the new value as `NEW_PRERELEASE`.

The helper does NOT git-add. You stage the files yourself in Phase 6.

### Phase 4: Build verification (mandatory gate)

```bash
python build.py --firmware
```

Must exit 0. If the build fails, fix the issue, re-run the bump if needed, and retry. Do NOT proceed to commit or push on a broken build.

### Phase 5: Evaluator (mandatory gate)

```bash
python evaluate.py --quick
```

Must show no new failures. If new failures appear, fix them (or, if pre-existing baseline failures unrelated to this change, document that in the commit message). Do NOT proceed on a regressed evaluator.

### Phase 6: Commit and push to dev

1. **Stage** the firmware change, the version bump, AND the documentation refresh from Phase 1 (if anything was refreshed in this cycle):
   ```bash
   git add src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash \
           <firmware-files> \
           CHANGELOG.md README.md \
           RELEASE_NOTES_*-beta.md   # if you maintain a per-line notes file
   ```
2. **Commit** with a conventional message that includes the new prerelease tag:
   ```bash
   git commit -m "chore(release): ${NEW_PRERELEASE}

   <one-line summary of what is in this beta>"
   ```
3. **Push** to dev:
   ```bash
   git push origin dev
   ```

If the pre-commit hook blocks (bump-check fails because version.h was not staged), re-stage and retry. Do NOT use `OTGW_BUMP_HOOK_DISABLE=1` here: the whole point of this skill is to bump+commit together.

The README + CHANGELOG + RELEASE_NOTES must land in the SAME commit (or a commit ordered before the tag) because the GitHub Action checks out the code at the tag and reads these files when composing the release body. Stale documentation at the tagged commit = stale release page. Phase 1's staleness gate is what keeps this contract honest; do not skip it.

### Phase 7: Create and push the prerelease tag

Construct the tag from `_SEMVER_CORE` and the new prerelease label:

```bash
SEMVER_CORE=$(grep '_SEMVER_CORE ' src/OTGW-firmware/version.h | awk -F'"' '{print $2}')
TAG="v${SEMVER_CORE}-${NEW_PRERELEASE}"
# example: v1.6.0-beta.4
```

Create an annotated tag and push it:

```bash
git tag -a "${TAG}" -m "Beta prerelease ${NEW_PRERELEASE}"
git push origin "${TAG}"
```

The push fires `.github/workflows/beta-prerelease.yml`. The Action builds firmware + filesystem binaries, creates a GitHub release with `prerelease: true`, and uploads the `.ino.bin` and `.littlefs.bin` as assets. The existing `release-assets.yml` workflow then chains in on `release: published` to attach SHA256SUMS, `flash_otgw.sh`, `flash_otgw.bat`, and the flash-bundle zip.

### Phase 8: Wait for the GitHub Action

1. Open the Actions tab: `https://github.com/rvdbreemen/OTGW-firmware/actions`
2. Watch the `Beta prerelease publish` workflow run on the new tag.
3. When it succeeds, verify the release exists:
   ```bash
   gh release view "${TAG}" --json tagName,isPrerelease,assets --jq '{tag: .tagName, prerelease: .isPrerelease, assets: [.assets[].name]}'
   ```
   Expected assets: `*.ino.bin`, `*.littlefs.bin`, `SHA256SUMS`, `flash_otgw.sh`, `flash_otgw.bat`, `OTGW-firmware-*-flash-bundle.zip`.

If the Action fails, inspect the logs, fix the issue (usually a build regression or a missing secret), and either re-run via `workflow_dispatch` or push a new tag.

### Phase 9: Discord announcement (CHECKPOINT)

Prepare a one-paragraph announcement for `#beta-testing` (channel ID `914498730001072149`). The Diff link points at the **latest public stable release** (`LATEST_PUBLIC`), not the previous beta — testers usually want to know what changed since the last shipping version. Reuse the `LATEST_PUBLIC` value already resolved in Phase 0:

```bash
echo "${LATEST_PUBLIC}"   # captured at the start of the flow
```

Template:

```
Beta ${NEW_PRERELEASE} is up.

Version: ${SEMVER_CORE}-${NEW_PRERELEASE}
What is new: <one or two sentences describing the changes in this beta>
Download: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/${TAG}
Diff vs ${LATEST_PUBLIC} (current Latest stable): https://github.com/rvdbreemen/OTGW-firmware/compare/${LATEST_PUBLIC}...${TAG}
Changelog: https://github.com/rvdbreemen/OTGW-firmware/blob/${TAG}/CHANGELOG.md

Please flash and report findings here (good and bad).
```

**CHECKPOINT: Show the announcement to the user before sending.** After approval, the user copies the text into `#beta-testing` (Discord MCP integration is optional and depends on session setup; default flow is copy-paste).

## Dry-run (testing the workflow without publishing)

To verify `beta-prerelease.yml` fires correctly without affecting field testers:

1. Branch off `dev`: `git checkout -b test/beta-prerelease-dryrun`
2. Push a throwaway tag: `git tag -a v0.0.0-beta.dryrun -m "dryrun" && git push origin v0.0.0-beta.dryrun`
3. Watch the Action run in the Actions tab.
4. After it publishes, delete the release and the tag:
   ```bash
   gh release delete v0.0.0-beta.dryrun --yes
   git push --delete origin v0.0.0-beta.dryrun
   git tag -d v0.0.0-beta.dryrun
   ```
5. Delete the throwaway branch.

The Action publishes the release on the SHA the tag points to, so an arbitrary commit on a throwaway branch is fine for the dry-run.

## Important rules

- **Never use em dashes** in any generated text. Use colons, periods, commas, or parentheses.
- **Always push to remote after every commit**: keep local and remote in sync.
- **Never force-push to dev**: standing permission only covers normal pushes.
- **Build and evaluator gates are mandatory**: do not push a tag if either gate is red.
- **One checkpoint**: the Discord announcement in Phase 9. All other phases proceed automatically.
- **Bump-check hook is your friend**: if the pre-commit hook blocks the firmware commit, you forgot to stage `version.h` or `data/version.hash`. Fix the staging, do NOT bypass with `OTGW_BUMP_HOOK_DISABLE=1`.
- **CI build is a deliberate departure from /release**: full releases build locally (so the maintainer toolchain is canonical), betas build in CI (so the maintainer does not pay 5 minutes per cycle). The build script (`build.py`) is the same; only the host differs.

## Known traps (lessons from past beta runs)

Captured here so the same trap is never re-stepped. The `.github/workflows/beta-prerelease.yml` workflow already encodes the workarounds; this section documents *why* the workflow looks the way it does.

### Trap 1: Immutable-releases policy locks the release at publish

The repo has GitHub's "Enforce immutable releases" enabled. The naive flow (`gh release create $TAG --prerelease ...` then `gh release upload $TAG ...`) fails because the release is immutable from the moment of publish; the upload step returns `HTTP 422 Cannot upload assets to an immutable release`.

**Workaround in workflow:** draft-first publication. `gh release create --draft --prerelease ... <all asset files>` attaches every asset in one atomic call while the release is still a draft (drafts are mutable). Only after that, `gh release edit --draft=false` flips it to published — immutability kicks in at that step, with all assets already attached.

Do NOT try to recover by uploading later; it will not work.

### Trap 2: A tag used by a deleted immutable release is reserved forever

If a published immutable release is deleted, GitHub still permanently reserves the tag name. A new release attempt at the same tag fails with `tag_name was used by an immutable release` plus `Cannot create ref due to creations being restricted`.

**Workaround:** bump the prerelease and retry under a new tag. There is no way to reclaim a tag name once an immutable release has held it, even after deletion. Document the skipped tag in the next commit message so testers can decode the gap.

### Trap 3: GITHUB_TOKEN-created release events do not chain

`release-assets.yml` listens on `release: published`. Events created by `secrets.GITHUB_TOKEN` (which is what this workflow uses) do NOT trigger downstream workflows. The old `beta-prerelease.yml` claimed `release-assets.yml` would chain in; it does not.

**Workaround:** `beta-prerelease.yml` is now self-contained. It generates `SHA256SUMS`, builds the flash-bundle zip, and uploads `flash_otgw.sh` / `flash_otgw.bat` itself, instead of relying on `release-assets.yml`. The stable `/release` flow still uses `release-assets.yml` because the maintainer creates the stable release locally with a PAT, which does chain.

### Trap 4: Diff link to the previous beta hides what testers care about

Testers usually want to know what changed since the last *shipping* version, not since some intermediate beta most of them never installed.

**Workaround in workflow:** the diff link in the release body points at the **latest PUBLIC stable release** (resolved via `gh release view --json tagName` — GitHub's "Latest" flag is by construction a non-prerelease). Fallback to the most recent dashless `v[0-9]*.[0-9]*.[0-9]*` tag if no release is flagged as Latest.

### Trap 5: Release body that does not link the in-repo narrative

The GitHub release body composed by CI is short by design (auto-generated). Testers reading it on mobile need a one-tap path to the README, CHANGELOG, and per-line release notes, plus an inline preview of what changed.

**Workaround in workflow:** the release body links `README.md`, `CHANGELOG.md`, and `RELEASE_NOTES_<base>-beta.md` (auto-detected at the tagged commit), and inlines the content of `RELEASE_NOTES_<base>-beta.md` above the `<!-- digest:end -->` sentinel as a "What's new since the last public release" teaser. These must be up-to-date at the tagged commit. Phase 1's staleness gate is the contractual mechanism that keeps them in sync.
