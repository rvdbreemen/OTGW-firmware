---
name: release
description: Prepare and execute a full OTGW-firmware release following the documented release process
disable-model-invocation: true
---

# /release - OTGW-firmware Release Skill

Prepare and execute a complete release of the OTGW-firmware project.

## Usage

```
/release <version>
```

Example: `/release 1.3.2`

The version argument is the target release version (without `v` prefix). The previous version is auto-detected from the latest git tag.

## Writing style rules

- **Never use em dashes** in any output: not in release notes, GitHub release body, Discord messages, commit messages, README updates, or conversation text. Use colons, periods, commas, or parentheses instead.
- **All release notes and GitHub release messages MUST be in English** (international audience).
- **No emojis** in release notes unless the existing format uses them (README uses them in headings).

## Process

Follow these phases in order. There are only **2 mandatory checkpoints** (marked with CHECKPOINT). All other phases proceed automatically unless something unexpected happens.

### Phase 0: Prepare - clean state & detect previous release

Start every release by ensuring a clean working state and detecting the baseline.

1. **Ensure you are on `dev`**: `git checkout dev`
2. **Commit and push any uncommitted changes**:
   - `git status` - if there are modified or untracked files, stage, commit, and push them
   - `git pull` - incorporate any remote changes
   - `git push origin dev` - ensure local and remote are in sync
   - Verify: `git status` must show `nothing to commit, working tree clean`
3. **Detect the latest GitHub release** (this is the authoritative previous release, not a local git tag):
   ```bash
   gh release view --json tagName,name,publishedAt --jq '{tag: .tagName, title: .name, date: .publishedAt}'
   ```
   Store the tag name (e.g., `v1.3.2`) and published date for use in later phases.
4. **Verify the release tag exists locally**: `git fetch --tags && git log <prev-tag> --oneline -1`
5. **List code changes since that release**: `git log <prev-tag>..HEAD --oneline -- src/ | grep -v "CI: update version.h"`
   - If there are no code changes, warn the user and ask whether to proceed. (Conditional stop.)

### Phase 1: ADR validation

Check whether any architectural changes since the previous release require new or updated ADRs.

1. Review the code commits from Phase 0 step 5
2. For each significant change: does it affect architecture, NFRs, API contracts, new/replaced dependencies, or build/CI tooling?
3. Check `docs/adr/` for existing ADRs that may need their Related section updated
4. If new ADRs are needed, create them now on `dev` before proceeding

**Conditional stop:** Only pause for user input if ADRs are actually needed. If no ADRs are required, report that and proceed automatically.

### Phase 2: Stabilize dev branch

1. Commit all open/uncommitted changes on `dev` and push to remote
2. Run `python build.py` to verify the build works
3. Commit version.h changes from build.py and push to remote
4. If the build fails, fix the issue, commit and push again. Repeat until green.

**No checkpoint.** If the build succeeds, proceed automatically to Phase 3.

### Phase 3: Merge dev to main

1. `git checkout main && git pull origin main`
2. `git merge dev`
3. Verify merge succeeded without conflicts

**Conditional stop:** Only pause if there are merge conflicts. Otherwise proceed.

### Phase 4: Gather changes, contributors & generate documentation

On `main`, gather all information AND generate all documentation in one pass. Present everything together for a single review.

**Changes:**
1. Detect the previous release tag: `git describe --tags --abbrev=0`
2. List all commits since that tag: `git log <prev-tag>..HEAD --oneline` (exclude "CI: update version.h" commits)
3. Categorize each commit as: new feature, bug fix, internal improvement, or breaking change
4. Check `docs/adr/` for new or updated ADRs since the previous release

**Contributors (automated from 3 sources):**

*Source 1 - GitHub Issues & PRs:*
- `gh issue list --state closed --search "closed:>YYYY-MM-DD" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`
- `gh pr list --state merged --search "merged:>YYYY-MM-DD" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`

*Source 2 - Discord #beta-testing channel:*
- Read messages from `#beta-testing` using `mcp__discord-mcp__fetch_channel_history` with `channel_id="914498730001072149"` and `limit=100`
- Filter messages since the previous release date
- Extract unique contributors (exclude bot accounts and the maintainer `number3nl` / user ID `384411356616720384`)
- For each contributor, note what they did: tested builds, reported bugs, shared logs, provided diagnostic insights
- **Username formatting**: Discord usernames often have a 4-digit numeric suffix (e.g., `fuzzyduck3793`, `simontemplar6623`). Strip the trailing digits to get the display name (e.g., `fuzzyduck`, `simontemplar`). Exception: if removing digits makes the name ambiguous or clearly wrong, keep the original.

*Source 3 - Discord #devs-esp-firmware channel:*
- Read messages from `#devs-esp-firmware` (channel ID: `924989767966425158`) with limit 100
- Filter messages since the previous release date
- Issues and bug reports are also reported here; extract them alongside contributors

**Discord server reference:** Guild ID `812969634638725140` (OTGW-firmware community).

**Compile the contributor list:**
- Deduplicate across all sources (same person may appear on GitHub and Discord)
- Identify the **most active contributor** for a special shoutout
- Present as: shoutout paragraph + bullet list of remaining contributors with their contribution

**Documentation (generate all files):**

1. **`RELEASE_NOTES_<version>.md`** (repository root): Full technical release notes following the template in `docs/process/RELEASE_PROCESS.md`
2. **`RELEASE_GITHUB_<version>.md`** (repository root): Concise GitHub release body with bug fixes, improvements, upgrade notes, and Thank You section (shoutout + contributor list + Discord invite link)
3. **`docs/BREAKING_CHANGES.md`**: Prepend a new version section. Always declare explicitly whether there are breaking changes or not.
4. **`README.md`**: Demote current "What's New" to "What was new", add new "What's New in v<version>" section with highlights

**CHECKPOINT 1: Present the categorized changes, contributor list, AND all generated documentation content to the user for review. Wait for approval before proceeding.**

### Phase 5: Release execution

Proceed directly after Phase 4 approval. No additional confirmation needed.

1. **Commit all outstanding changes on `main`** and push to remote
2. **Remove pre-release from `version.h`**: Comment out `_VERSION_PRERELEASE` so the build produces a clean `v<version>` without `-beta`. Verify: `grep -n "PRERELEASE" src/OTGW-firmware/version.h`
3. **Run `python build.py`** to produce the release build. Fix any issues.
4. **Commit the release build** and push `main` to remote
5. **Create draft GitHub release with tag**: Derive a short title (3-6 words) summarizing the release theme. Use format `v<version> - <Short Title>`. Examples: `v1.3.2 - File Explorer Reliability Fix`, `v1.4.0 - REST API v3 & Prometheus`. Command: `gh release create v<version> --target main --title "v<version> - <Short Title>" --notes-file RELEASE_GITHUB_<version>.md --draft`
6. **Upload build artifacts to the draft release**: `gh release upload v<version> build/*.ino.bin build/*.littlefs.bin flash_otgw.sh flash_otgw.bat --clobber`
7. **Verify artifacts are attached**: `gh release view v<version> --json assets --jq '.assets[].name'`
8. **Publish the release**: `gh release edit v<version> --draft=false --latest`

### Phase 6: Post-release, Discord announcement & sync dev

1. Verify release artifacts are attached to the GitHub release
2. Remind user to flash a device and check `GET /api/v2/device/info`
3. **Prepare Discord announcements** for both community channels:
   - Dutch in `#nederlandse-ondersteuning` (channel ID: `815561033036333076`)
   - English in `#english-support` (channel ID: `931267109726593116`)
   - Both messages include: version, summary, contributor shoutout, download link

**CHECKPOINT 2: Show both Discord messages to the user before sending.**

4. **Send Discord messages** after approval
5. **Sync dev branch:**
   - `git checkout dev && git merge main`
   - Bump `version.h`: increment patch version, uncomment `_VERSION_PRERELEASE` and set to `beta`
   - Run `autoinc-semver.py` to update derived strings
   - Commit: `feat: Bump version to v<next>-beta for development`
   - Push `dev`

## Phase 7: Post-publication corrections (when release notes need updating after publish)

Sometimes a user asks to correct text in an already-published release. Editing `RELEASE_GITHUB_<version>.md` in the repo does **not** update the GitHub release page automatically: the release body is a copy taken at `gh release create` time and lives on GitHub, not in the repo.

Whenever you update release notes, README, or the GitHub release body after publication, do all three in the same round:

1. **Edit the files in the repo** (`RELEASE_NOTES_<version>.md`, `README.md`, `RELEASE_GITHUB_<version>.md`) on `main`.
2. **Commit and push** to `main`.
3. **Update the live GitHub release body** with: `gh release edit v<version> --notes-file RELEASE_GITHUB_<version>.md`
4. **Merge `main` back into `dev`** so both branches reflect the correction: `git checkout dev && git merge main && git push origin dev`

Skipping step 3 leaves the repo and the GitHub release page out of sync. Skipping step 4 means the next beta cycle starts from stale release docs.

## Important rules

- **Never use em dashes** in any generated text (release notes, Discord messages, commit messages, README, conversation). Use colons, periods, commas, or parentheses.
- **Always push to remote after every commit**: keep local and remote in sync throughout the release
- **Always create releases as draft first**: upload artifacts, verify, then publish. Once published, releases are immutable.
- **No CI workflows for releases**: builds are done locally via `python build.py`, artifacts uploaded via `gh release upload`
- **Only 2 mandatory checkpoints**: Phase 4 (content review) and Phase 6 (Discord messages). Do not add unnecessary confirmation prompts.
- **Conditional stops**: only pause for merge conflicts, missing ADRs, build failures, or zero code changes.
- **Never force-push**: all pushes are normal pushes
- **Read `docs/process/RELEASE_PROCESS.md`** at the start of every release for the latest process updates
- **All release notes and GitHub release messages MUST be in English**: international audience
- **No emojis** in release notes unless the existing format uses them (README uses them in headings)
- **GitHub release body is decoupled from the repo file**: editing `RELEASE_GITHUB_<version>.md` does NOT update the published release page. After any edit, run `gh release edit v<version> --notes-file RELEASE_GITHUB_<version>.md` to push the change to GitHub, then merge main back into dev so both branches carry the correction.
