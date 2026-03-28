---
name: release
description: Prepare and execute a full OTGW-firmware release following the documented release process
disable-model-invocation: true
---

# /release — OTGW-firmware Release Skill

Prepare and execute a complete release of the OTGW-firmware project.

## Usage

```
/release <version>
```

Example: `/release 1.3.2`

The version argument is the target release version (without `v` prefix). The previous version is auto-detected from the latest git tag.

## Process

Follow these phases in order, pausing for user approval at the marked checkpoints.

### Phase 0: ADR validation

Before starting the release, check whether any architectural changes since the previous release require new or updated ADRs.

1. Detect the previous release tag: `git describe --tags --abbrev=0`
2. List commits that touch code (not just docs/version bumps): `git log <prev-tag>..HEAD --oneline -- src/`
3. Review each significant change — does it affect: architecture, NFRs (security/performance/availability), API contracts, new/replaced dependencies, or build/CI tooling?
4. Check `docs/adr/` for existing ADRs that may need their Related section updated.
5. If new ADRs are needed, create them now on `dev` before proceeding.

**CHECKPOINT: Present any missing or needed ADRs to the user. Do not proceed until all architectural decisions are documented.**

### Phase 1: Stabilize dev branch

1. Commit all open/uncommitted changes on `dev`
2. Run `python build.py` to verify the build works
3. If the build fails, fix the issue and commit again
4. Repeat until the build succeeds on `dev`

**CHECKPOINT: Confirm with user that dev is stable and ready to merge.**

### Phase 2: Merge dev to main

1. `git checkout main && git merge dev`
2. Verify merge succeeded without conflicts

### Phase 3: Gather changes & contributors

On `main`, gather all information for the release notes.

**Changes:**
1. Detect the previous release tag: `git describe --tags --abbrev=0`
2. List all commits since that tag: `git log <prev-tag>..HEAD --oneline` (exclude "CI: update version.h" commits)
3. Categorize each commit as: new feature, bug fix, internal improvement, or breaking change
4. Check `docs/adr/` for new or updated ADRs since the previous release

**Contributors (automated from 3 sources):**

*Source 1 — GitHub Issues & PRs:*
- `gh issue list --state closed --search "closed:>YYYY-MM-DD" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`
- `gh pr list --state merged --search "merged:>YYYY-MM-DD" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`

*Source 2 — Discord #beta-testing channel:*
- Log in to Discord: `mcp__discord__discord_login` (bot: OTGW bot)
- Read messages from `#beta-testing` (channel ID: `914498730001072149`) with limit 100
- Filter messages since the previous release date
- Extract unique contributors (exclude bot accounts and the maintainer `number3nl` / user ID `384411356616720384`)
- For each contributor, note what they did: tested builds, reported bugs, shared logs, provided diagnostic insights
- **Username formatting**: Discord usernames often have a 4-digit numeric suffix (e.g., `fuzzyduck3793`, `simontemplar6623`). Strip the trailing digits to get the display name (e.g., `fuzzyduck`, `simontemplar`). Exception: if removing digits makes the name ambiguous or clearly wrong, keep the original.

*Source 3 — Discord #devs-esp-firmware channel:*
- Read messages from `#devs-esp-firmware` (channel ID: `924989767966425158`) with limit 100
- Filter messages since the previous release date
- Issues and bug reports are also reported here — extract them alongside contributors

**Discord server reference:** Guild ID `812969634638725140` (OTGW-firmware community).

**Compile the contributor list:**
- Deduplicate across all sources (same person may appear on GitHub and Discord)
- Identify the **most active contributor** — they get a special shoutout
- Present as: shoutout paragraph + bullet list of remaining contributors with their contribution

**CHECKPOINT: Present the categorized changes AND contributor list. Ask user to confirm before proceeding.**

### Phase 4: Documentation

Generate all documentation files on `main`. Show content to the user before writing.

1. **`RELEASE_NOTES_<version>.md`** (repository root) — Full technical release notes following the template in `docs/process/RELEASE_PROCESS.md`
2. **`RELEASE_GITHUB_<version>.md`** (repository root) — Concise GitHub release body with:
   - Bug fixes, improvements, upgrade notes
   - **Thank You section**: special shoutout to most active contributor, then bullet list of other contributors with their contribution. Ends with Discord invite link.
3. **`docs/BREAKING_CHANGES.md`** — Prepend a new version section. Always declare explicitly whether there are breaking changes or not.
4. **`README.md`** — Demote current "What's New" to "What was new", add new "What's New in v<version>" section with highlights
5. **ADRs** — If any changes warrant a new ADR, propose it.

**CHECKPOINT: Show all generated documentation to the user for review.**

### Phase 5: Release execution

**CHECKPOINT: Confirm with user before starting — these steps are not reversible.**

1. **Commit all outstanding changes on `main`** (documentation, version updates, etc.)
2. **Remove pre-release from `version.h`**: Comment out `_VERSION_PRERELEASE` so the build produces a clean `v<version>` without `-beta`. Verify: `grep -n "PRERELEASE" src/OTGW-firmware/version.h`
3. **Run `python build.py`** to produce the release build. Fix any issues.
4. **Commit the release build** and push `main`
5. **Create GitHub release with tag**: `gh release create v<version> --target main --title "v<version>" --notes-file RELEASE_GITHUB_<version>.md --latest`
6. **Upload build artifacts to the release**: `gh release upload v<version> build/*.elf build/*.ino.bin build/*.littlefs.bin --clobber`

### Phase 6: Post-release verification

1. Verify release artifacts are attached to the GitHub release
2. Remind user to flash a device and check `GET /api/v2/device/info`
3. Remind user to announce on Discord

### Phase 7: Sync dev branch

1. `git checkout dev`
2. `git merge main`
3. Bump `version.h`: increment minor version, uncomment `_VERSION_PRERELEASE` and set to `beta`
4. Commit: `feat: Bump version to v<next>-beta for development`
5. Push `dev`

**CHECKPOINT: Confirm the next development version number with the user before committing.**

## Important rules

- **Never skip a checkpoint** — always wait for user approval
- **Never force-push** — all pushes are normal pushes
- **Read `docs/process/RELEASE_PROCESS.md`** at the start of every release for the latest process updates
- **All release notes and GitHub release messages MUST be in English** — the project targets an international audience
- **No emojis** in release notes unless the existing format uses them (README uses them in headings)
