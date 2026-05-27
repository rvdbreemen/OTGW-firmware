---
name: beta-prerelease
description: Publish an OTGW-firmware beta prerelease — bump _VERSION_PRERELEASE, push to dev, tag, and let CI build + publish the GitHub prerelease
disable-model-invocation: true
---

# /beta-prerelease - OTGW-firmware Beta Prerelease Skill

Publish a single beta build to field testers in Discord `#beta-testing`. Lightweight, repeatable many times within one minor cycle. Does NOT merge to main, does NOT touch the stable `_SEMVER_CORE`.

## Usage

```
/beta-prerelease
```

No arguments. Auto-detects the current `_VERSION_PRERELEASE` and increments it via `bin/bump-prerelease.sh`.

## Token-efficiency rules (apply throughout all phases)

- **P1 Build output**: Pipe through `tee .tmp/build_beta.log | tail -5`. Only read `.tmp/build_beta.log` if exit code != 0.
- **P2 Phase 3 reads**: Use targeted `grep`/`sed` to extract only the relevant section from each file. Never read README, CHANGELOG, or RELEASE_NOTES in full.
- **P3 Phase 3 writes**: Edit each file and note "Updated." immediately. Do NOT keep all edited content in context — keep only the filename.
- **P4 Known Traps**: Summarized as 5 bullets below. Read `.github/workflows/beta-prerelease.yml` only if a trap is actually hit.
- **P5 Phase 8 CI poll**: Use `gh run watch` instead of opening a browser.

## When to use

Run when a firmware change under `src/OTGW-firmware/**` or `src/libraries/**` is committed and ready for field testing. Do NOT use for docs-only commits or full releases to `main` (use `/release <version>` for those).

## How this differs from /release

| Aspect | `/release` | `/beta-prerelease` |
|---|---|---|
| Target branch | `dev` then merge to `main` | `dev` only |
| Bumps `_SEMVER_CORE` | yes | no |
| GitHub release | stable, not prerelease | prerelease: true |
| Discord channels | `#nederlandse-ondersteuning`, `#english-support` | `#beta-testing` |
| Mandatory checkpoints | 2 | 1 (Discord announcement) |

## Writing style rules

- **Never use em dashes** — use colons, periods, commas, or parentheses instead.
- **All release text MUST be in English** (international audience).
- **No emojis** in release notes or Discord posts.

## Process

### Phase 0: Prepare — clean state on dev

1. Ensure you are on `dev`: `git checkout dev`
2. Verify clean state: `git status` must be clean (or only the firmware change about to be bumped). Then `git pull origin dev`.
3. Detect the latest prerelease tag:
   ```bash
   git fetch --tags
   PREV_TAG=$(git tag --list 'v*-*.*' --sort=-v:refname | head -1)
   ```
4. Detect the latest public stable release (store as `LATEST_PUBLIC`):
   ```bash
   LATEST_PUBLIC=$(gh release view --json tagName --jq '.tagName' 2>/dev/null \
     || git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname | grep -v -- '-' | head -1)
   ```
5. Read current prerelease: `grep _VERSION_PRERELEASE src/OTGW-firmware/version.h`

### Phase 1: ADR validation

Skip by default. Only pause if the staged firmware change introduces a new architectural pattern, dependency, or NFR shift. Most beta cycles do not need this gate.

### Phase 2: Bump prerelease

```bash
bin/bump-prerelease.sh
```

Prints the transition (e.g. `beta.3 -> beta.4`). Store the new value as `NEW_PRERELEASE`. The helper does NOT git-add — you stage in Phase 6.

Assemble the tag: `TAG="v${SEMVER_CORE}-${NEW_PRERELEASE}"`

### Phase 3: Refresh README + CHANGELOG + RELEASE_NOTES (mandatory, P2, P3)

The GitHub Action reads these files at the tagged commit. Stale narrative at the tag = stale release page (Trap 1). Refresh immediately after the bump so `NEW_PRERELEASE` is known.

**Staleness check (P2 — targeted extractions, not full reads):**

```bash
# 1. Commits since last public release (the change set to account for)
git log --pretty=format:'%h %s' "${LATEST_PUBLIC}..HEAD" -- src/OTGW-firmware/ src/libraries/ docs/

# 2. Existing narrative — extract only the relevant sections
grep -A 50 "What's new on dev" README.md | head -55
grep -A 35 "## \[Unreleased\]" CHANGELOG.md | head -40
sed -n '1,/<!-- digest:end -->/p' RELEASE_NOTES_*-beta.md 2>/dev/null
```

**Decision:**
- All commit subjects appear in at least one narrative AND dev-banner version label matches `NEW_PRERELEASE` → pass silently, continue to Phase 4.
- Any commit missing from the narrative OR dev-banner label stale → refresh now. Stop and ask only if the gap is ambiguous; otherwise edit in-session.

**Authoring rules (P3 — write immediately, keep only filename in context):**

1. `CHANGELOG.md` — append under `## [Unreleased]` using Keep-a-Changelog headings (`### Added/Changed/Fixed/Removed/Documentation`). One bullet per change, with ADR/TASK/PR reference. → note "Updated."
2. `README.md` — update dev-banner version label to `NEW_PRERELEASE` and "What's new on dev" section. Do NOT touch the "What's New in v<stable>" section. → note "Updated."
3. `RELEASE_NOTES_<base>-beta.md` — update the digest region above `<!-- digest:end -->`. → note "Updated."

Skip all three only when this is a re-cut at the same change surface (previous tag hit Trap 2). Note the reason in the commit message.

### Phase 4: Build verification (P1)

```bash
mkdir -p .tmp
python build.py --firmware 2>&1 | tee .tmp/build_beta.log | tail -5
echo "Exit: $?"
```

Must exit 0. On failure: read `.tmp/build_beta.log` for diagnosis, fix, retry. Do NOT push a tag on a broken build.

### Phase 5: Evaluator

```bash
python evaluate.py --quick
```

Must show no new failures. Pre-existing baseline failures unrelated to this change: document in the commit message.

### Phase 6: Commit and push to dev

```bash
git add src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash \
        <firmware-files> \
        CHANGELOG.md README.md \
        RELEASE_NOTES_*-beta.md

git commit -m "chore(release): ${NEW_PRERELEASE}

<one-line summary of what is in this beta>"

git push origin dev
```

If the pre-commit hook blocks: re-stage `version.h` + `data/version.hash` and retry. Do NOT bypass with `OTGW_BUMP_HOOK_DISABLE=1`.

### Phase 7: Create and push the prerelease tag

```bash
SEMVER_CORE=$(grep '_SEMVER_CORE ' src/OTGW-firmware/version.h | awk -F'"' '{print $2}')
TAG="v${SEMVER_CORE}-${NEW_PRERELEASE}"

git tag -a "${TAG}" -m "Beta prerelease ${NEW_PRERELEASE}"
git push origin "${TAG}"
```

The push fires `.github/workflows/beta-prerelease.yml`. CI builds firmware + filesystem, creates a GitHub prerelease, uploads `.ino.bin`, `.littlefs.bin`, `SHA256SUMS`, flash scripts, and the flash-bundle zip.

### Phase 8: Wait for the GitHub Action (P5)

```bash
gh run watch --exit-status
```

When it exits 0, verify the release:
```bash
gh release view "${TAG}" --json tagName,isPrerelease,assets \
  --jq '{tag: .tagName, prerelease: .isPrerelease, assets: [.assets[].name]}'
```

Expected assets: `*.ino.bin`, `*.littlefs.bin`, `SHA256SUMS`, `flash_otgw.sh`, `flash_otgw.bat`, `OTGW-firmware-*-flash-bundle.zip`.

On failure: inspect logs with `gh run view --log-failed`, fix the issue, and re-run via `workflow_dispatch` or push a new tag.

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

**CHECKPOINT: Show the announcement to the user before sending.**

## Dry-run (testing without publishing)

```bash
git checkout -b test/beta-prerelease-dryrun
git tag -a v0.0.0-beta.dryrun -m "dryrun" && git push origin v0.0.0-beta.dryrun
# watch the Action, then clean up:
gh release delete v0.0.0-beta.dryrun --yes
git push --delete origin v0.0.0-beta.dryrun && git tag -d v0.0.0-beta.dryrun
git checkout dev && git branch -d test/beta-prerelease-dryrun
```

## Known traps (P4 — full detail in `beta-prerelease.yml`)

1. **Trap 1: Immutable-releases locks publish** — upload after `gh release create` returns HTTP 422. Workaround already in CI: draft-first, attach all assets, then flip `--draft=false`. If you hit this manually, re-tag under the next beta number.
2. **Trap 2: Deleted immutable release reserves the tag forever** — even after deletion, the tag cannot be reused. Bump the prerelease number and note the skipped tag in the commit message.
3. **Trap 3: `GITHUB_TOKEN` events do not chain** — `release-assets.yml` (triggered by `release: published`) does not fire when the release is created by `GITHUB_TOKEN`. `beta-prerelease.yml` is therefore self-contained (generates SHA256SUMS + zip + flash scripts itself).
4. **Trap 4: Diff link to previous beta hides what testers care about** — always link vs `LATEST_PUBLIC` (the non-prerelease "Latest" release), not the previous beta tag.
5. **Trap 5: Stale narrative at tagged commit is permanent** — Phase 3 refresh runs before the tag is pushed for this reason. Skipping Phase 3 = stale release page that cannot be edited after publish.

## Important rules

- **Never use em dashes** in any generated text.
- **Always push to remote after every commit**.
- **Never force-push to dev**.
- **Build and evaluator gates are mandatory** — do not push a tag if either is red.
- **One checkpoint**: the Discord announcement in Phase 9.
- **Do NOT bypass the bump-check hook** with `OTGW_BUMP_HOOK_DISABLE=1` — if the hook blocks, you forgot to stage `version.h` / `data/version.hash`.
