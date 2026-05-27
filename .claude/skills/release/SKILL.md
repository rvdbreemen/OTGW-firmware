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

## Token-efficiency rules (apply throughout all phases)

- **R1 Discord limit**: Use `limit=50` per channel for contributor detection, not 100. Releases are monthly; 50 messages captures all unique contributors.
- **R2 Build output**: Pipe build through `tee .tmp/build_release.log | tail -5`. Only read the full log if exit code != 0. Never load verbose build output into context on success.
- **R3 Sequential docs**: Generate each document, write it to disk immediately, keep only the filename in context. At Checkpoint 1, present a compact summary (key changes + contributor highlights) and reference the filenames — do NOT paste full document contents into context.
- **R4 GitHub --jq**: Always filter `gh issue list` and `gh pr list` with `--jq '.[] | {login: .author.login, title: .title}'`. Only author and title are needed for contributor detection.
- **R5 ADR context**: Use `bin/adr-context` with the commit-message list to find relevant ADRs instead of manually reading ADR files.

## Process

Follow these phases in order. There are only **2 mandatory checkpoints** (marked with CHECKPOINT). All other phases proceed automatically unless something unexpected happens.

### Phase 0: Prepare — clean state & detect previous release

1. **Ensure you are on `dev`**: `git checkout dev`
2. **Commit and push any uncommitted changes**:
   - `git status` — if dirty: stage, commit, push
   - `git pull` — incorporate remote changes
   - `git push origin dev` — sync local and remote
   - Verify: `git status` must show `nothing to commit, working tree clean`
3. **Detect the latest GitHub release**:
   ```bash
   gh release view --json tagName,name,publishedAt \
     --jq '{tag: .tagName, title: .name, date: .publishedAt}'
   ```
   Store tag name (e.g. `v1.3.2`) and published date.
4. **Verify the release tag exists locally**: `git fetch --tags && git log <prev-tag> --oneline -1`
5. **List code changes since that release** (compact, in context):
   ```bash
   git log <prev-tag>..HEAD --oneline -- src/ | grep -v "CI: update version.h"
   ```
   If no code changes: warn the user and ask whether to proceed. (Conditional stop.)

### Phase 1: ADR validation (R5)

Check whether architectural changes since the previous release require new or updated ADRs.

1. Take the commit messages from Phase 0 step 5.
2. Run `bin/adr-context` to identify relevant ADRs (uses commit messages as relevance signal):
   ```bash
   ADR_KIT=$(ls -d ~/.claude/plugins/cache/rvdbreemen-adr-kit/adr-kit/*/ | sort -V | tail -1)
   git log <prev-tag>..HEAD --format="%s" -- src/ | head -20 | \
     "$ADR_KIT/bin/adr-context" --format text --limit 5
   ```
   Read only the top-5 returned ADRs to check whether they cover the changes made.
3. For each significant change not covered by an existing ADR: ask the user if a new ADR is needed.
4. If new ADRs are needed, create them now on `dev` before proceeding.

**Conditional stop:** Only pause if ADRs are actually needed. Otherwise proceed automatically.

### Phase 2: Stabilize dev branch

1. Commit all open/uncommitted changes on `dev` and push to remote.
2. Run the build and filter output (R2):
   ```bash
   mkdir -p .tmp
   python build.py 2>&1 | tee .tmp/build_release.log | tail -10
   echo "Exit: $?"
   ```
   If exit code != 0: read `.tmp/build_release.log` for diagnosis, fix, retry.
   If exit code == 0: proceed — do NOT read the full log.
3. Commit `version.h` changes from build and push.

**No checkpoint.** Proceed automatically to Phase 3 on success.

### Phase 3: Merge dev to main

1. `git checkout main && git pull origin main`
2. `git merge dev`
3. Verify merge succeeded without conflicts.

**Conditional stop:** Only pause on merge conflicts.

### Phase 4: Gather changes, contributors & generate documentation (R1, R3, R4)

Gather information and generate documentation. Write each document to disk as soon as it is generated — do NOT accumulate all content in context simultaneously.

**Step 4a — Changes:**
1. Detect previous release tag: `git describe --tags --abbrev=0`
2. List commits: `git log <prev-tag>..HEAD --oneline` (exclude "CI: update version.h")
3. Categorize each commit as: new feature, bug fix, internal improvement, or breaking change.
4. Note any new/updated ADRs in `docs/adr/` since the previous release.

**Step 4b — Contributors (R1, R4):**

Source 1 — GitHub issues & PRs (compact, R4):
```bash
DATE="<previous-release-date>"
gh issue list --state closed --search "closed:>$DATE" --limit 50 \
  --json author,title --jq '.[] | "\(.author.login): \(.title)"'
gh pr list --state merged --search "merged:>$DATE" --limit 50 \
  --json author,title --jq '.[] | "\(.author.login): \(.title)"'
```

Source 2 — Discord `#beta-testing` (R1 — limit 50):
```
mcp__discord-mcp__fetch_channel_history(channel_id="914498730001072149", limit=50)
```
Filter to messages since the previous release date. Extract unique contributor usernames. Strip trailing 4-digit suffixes (e.g. `fuzzyduck3793` → `fuzzyduck`). Note what each person did.

Source 3 — Discord `#devs-esp-firmware` (R1 — limit 50):
```
mcp__discord-mcp__fetch_channel_history(channel_id="924989767966425158", limit=50)
```
Same filtering. Exclude maintainer (`number3nl`, user ID `384411356616720384`) and bots.

Deduplicate across all three sources. Identify the most active contributor for a shoutout.

**Step 4c — Generate documents sequentially (R3):**

Generate each document and write it to disk immediately after creation. Keep only the filename in context after writing.

1. Generate `RELEASE_NOTES_<version>.md` → write to repo root → note: "Written."
2. Generate `RELEASE_GITHUB_<version>.md` → write to repo root → note: "Written."
3. Update `docs/BREAKING_CHANGES.md` (prepend new section) → note: "Updated."
4. Update `README.md` (demote old "What's New", add new section) → note: "Updated."

**CHECKPOINT 1: Present a compact summary to the user (R3):**
```
Changes: <N commits — key highlights in 3-5 bullets>
Contributors: <shoutout name> + <N others>
Documents written:
  - RELEASE_NOTES_<version>.md
  - RELEASE_GITHUB_<version>.md
  - docs/BREAKING_CHANGES.md (updated)
  - README.md (updated)
Ask: "Review the files above, then approve to proceed — or request changes."
```
Do NOT paste full document contents into context. The user reads the files directly.

### Phase 5: Release execution

Proceed directly after Phase 4 approval.

1. **Commit all outstanding changes on `main`** and push.
2. **Remove pre-release from `version.h`**: comment out `_VERSION_PRERELEASE` so the build produces a clean `v<version>`. Verify: `grep -n "PRERELEASE" src/OTGW-firmware/version.h`
3. **Run the release build (R2)**:
   ```bash
   python build.py 2>&1 | tee .tmp/build_release_final.log | tail -10
   echo "Exit: $?"
   ```
   Fix any issues. Read `.tmp/build_release_final.log` only on failure.
4. **Commit the release build** and push `main`.
5. **Create draft GitHub release**:
   ```bash
   gh release create v<version> --target main \
     --title "v<version> - <Short Title>" \
     --notes-file RELEASE_GITHUB_<version>.md --draft
   ```
   Derive the short title (3-6 words) from the release theme. Examples: `v1.3.2 - File Explorer Reliability Fix`.
6. **Upload artifacts**:
   ```bash
   gh release upload v<version> build/*.ino.bin build/*.littlefs.bin \
     flash_otgw.sh flash_otgw.bat --clobber
   ```
7. **Verify artifacts**: `gh release view v<version> --json assets --jq '.assets[].name'`
8. **Publish**: `gh release edit v<version> --draft=false --latest`

### Phase 6: Post-release, Discord announcement & sync dev

1. Verify release artifacts are attached to the GitHub release.
2. Remind user to flash a device and check `GET /api/v2/device/info`.
3. **Prepare Discord announcements**:
   - Dutch in `#nederlandse-ondersteuning` (channel ID: `815561033036333076`)
   - English in `#english-support` (channel ID: `931267109726593116`)
   - Both messages include: version, summary, contributor shoutout, download link

**CHECKPOINT 2: Show both Discord messages to the user before sending.**

4. **Send Discord messages** after approval.
5. **Sync dev branch**:
   - `git checkout dev && git merge main`
   - Bump `version.h`: increment patch, uncomment `_VERSION_PRERELEASE`, set to `beta`
   - Run `autoinc-semver.py` to update derived strings
   - Commit: `feat: Bump version to v<next>-beta for development`
   - Push `dev`

## Phase 7: Post-publication corrections

When release notes need updating after publish, do all three in the same round:

1. Edit files in the repo (`RELEASE_NOTES_<version>.md`, `README.md`, `RELEASE_GITHUB_<version>.md`) on `main`.
2. Commit and push to `main`.
3. Update the live GitHub release body: `gh release edit v<version> --notes-file RELEASE_GITHUB_<version>.md`
4. Merge `main` back into `dev`: `git checkout dev && git merge main && git push origin dev`

Skipping step 3 leaves the repo and GitHub release page out of sync. Skipping step 4 means the next beta cycle starts from stale release docs.

## Important rules

- **Never use em dashes** in any generated text
- **Always push to remote after every commit**
- **Always create releases as draft first**: upload artifacts, verify, then publish
- **No CI workflows for releases**: builds done locally via `python build.py`
- **Only 2 mandatory checkpoints**: Phase 4 (content review) and Phase 6 (Discord messages)
- **Conditional stops only**: merge conflicts, missing ADRs, build failures, zero code changes
- **Never force-push**
- **Read `docs/process/RELEASE_PROCESS.md`** at the start of every release
- **All release notes and GitHub release messages MUST be in English**
- **No emojis** in release notes unless the existing format uses them
- **GitHub release body is decoupled from the repo file**: after any edit to `RELEASE_GITHUB_<version>.md`, run `gh release edit v<version> --notes-file RELEASE_GITHUB_<version>.md` and merge `main` back into `dev`
