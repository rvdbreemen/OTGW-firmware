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
| Edits `README.md` | yes (What's New section) | no |
| Generates `RELEASE_NOTES_*.md` | yes | no |
| GitHub release marked prerelease | no | yes |
| Discord channels | `#nederlandse-ondersteuning`, `#english-support` | `#beta-testing` |
| Frequency | once per minor cycle | many times per minor cycle |
| Mandatory checkpoints | 2 | 1 (Discord announcement) |

## Writing style rules

- **Never use em dashes** in any output: not in release notes, GitHub release body, Discord messages, commit messages, or conversation text. Use colons, periods, commas, or parentheses instead.
- **All release notes and GitHub release messages MUST be in English** (international audience).
- **No emojis** in release notes or Discord posts unless the existing format uses them.

## Process

Nine phases. Only **one mandatory checkpoint** (the Discord announcement at the end). All other phases proceed automatically unless something unexpected happens.

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
   Store this as `PREV_TAG` for the release body diff link in Phase 6.
4. **Read current version**: `grep _VERSION_PRERELEASE src/OTGW-firmware/version.h` and store the current value (e.g. `beta.3`).

### Phase 1: ADR validation

Skip by default. Only stop and create an ADR if the staged firmware change introduces a new architectural pattern, a new dependency, or shifts an NFR (heap budget, MQTT topic shape, settings schema). Most beta cycles do not need this gate.

**Conditional stop:** Only pause for user input if an ADR is actually needed.

### Phase 2: Bump prerelease

Run the bump helper from the project root:

```bash
bin/bump-prerelease.sh
```

The helper rewrites `src/OTGW-firmware/version.h` (`_VERSION_PRERELEASE`, `_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION`) and `src/OTGW-firmware/data/version.hash`. It prints the transition (e.g. `beta.3 -> beta.4`). Store the new value as `NEW_PRERELEASE`.

The helper does NOT git-add. You stage the files yourself in Phase 5.

### Phase 2.5: Update README and CHANGELOG (mandatory)

The GitHub release body is mostly a thin shell that links *back* into the repo. Keep those targets in shape on every beta cut so testers reading the release page on mobile can find the narrative.

1. **`CHANGELOG.md`** — append new entries under `## [Unreleased]` (or open a new `## [<base_version>-beta] - YYYY-MM-DD` section if you prefer to scope per beta line). Keep the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) headings (`### Added`, `### Changed`, `### Fixed`, `### Removed`, `### Documentation`). One short bullet per change, with the ADR/TASK reference in parentheses.
2. **`README.md`** — refresh the dev-branch banner and the "What's new on dev" / "Latest beta line" paragraph so the first screen of the README reflects what this beta carries. Do not touch the "What's New in v<stable>" section — that belongs to the last shipped stable and stays as the historical anchor.
3. **`RELEASE_NOTES_<base_version>-beta.md`** (optional but recommended) — if the line carries a narrative bigger than the CHANGELOG bullets warrant, keep a per-line release-notes file at the repo root. The CI workflow auto-detects and links it in the release body.

These three files are what the workflow links to in the GitHub release body. If any of them is stale, the release page tells testers the wrong story.

**Skip allowed only when**: the bump is a re-cut at the same change surface (e.g., beta.4 hit the immutable-release trap and we are reissuing as beta.5). The README/CHANGELOG narrative did not change, only the tag did. Note the reason in the commit message.

### Phase 3: Build verification (mandatory gate)

```bash
python build.py --firmware
```

Must exit 0. If the build fails, fix the issue, re-run the bump if needed, and retry. Do NOT proceed to commit or push on a broken build.

### Phase 4: Evaluator (mandatory gate)

```bash
python evaluate.py --quick
```

Must show no new failures. If new failures appear, fix them (or, if pre-existing baseline failures unrelated to this change, document that in the commit message). Do NOT proceed on a regressed evaluator.

### Phase 5: Commit and push to dev

1. **Stage** the firmware change, the version bump, AND the documentation refresh from Phase 2.5:
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

The README + CHANGELOG must land in the SAME commit (or a commit ordered before the tag) because the GitHub Action checks out the code at the tag and reads these files when composing the release body. Stale documentation at the tagged commit = stale release page.

### Phase 6: Create and push the prerelease tag

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

### Phase 7: Wait for the GitHub Action

1. Open the Actions tab: `https://github.com/rvdbreemen/OTGW-firmware/actions`
2. Watch the `Beta prerelease publish` workflow run on the new tag.
3. When it succeeds, verify the release exists:
   ```bash
   gh release view "${TAG}" --json tagName,isPrerelease,assets --jq '{tag: .tagName, prerelease: .isPrerelease, assets: [.assets[].name]}'
   ```
   Expected assets: `*.ino.bin`, `*.littlefs.bin`, `SHA256SUMS`, `flash_otgw.sh`, `flash_otgw.bat`, `OTGW-firmware-*-flash-bundle.zip`.

If the Action fails, inspect the logs, fix the issue (usually a build regression or a missing secret), and either re-run via `workflow_dispatch` or push a new tag.

### Phase 8: Discord announcement (CHECKPOINT)

Prepare a one-paragraph announcement for `#beta-testing` (channel ID `914498730001072149`). The Diff link points at the **latest public stable release** (`LATEST_PUBLIC`), not the previous beta — testers usually want to know what changed since the last shipping version. Get the value with:

```bash
LATEST_PUBLIC=$(gh release view --json tagName --jq '.tagName')
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
- **One checkpoint**: the Discord announcement in Phase 8. All other phases proceed automatically.
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

The GitHub release body composed by CI is short by design (auto-generated). Testers reading it on mobile need a one-tap path to the README, CHANGELOG, and per-line release notes.

**Workaround in workflow:** the release body links `README.md`, `CHANGELOG.md`, and `RELEASE_NOTES_<base>-beta.md` (auto-detected at the tagged commit). These must be up-to-date at the tagged commit — see Phase 2.5.
