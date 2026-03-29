# Release Process

This document describes the complete end-to-end release process for OTGW-firmware, from stabilizing the dev branch through GitHub release publication.

---

## Phase 0: Prepare — clean state & detect previous release

Start every release by ensuring a clean working state and detecting the baseline.

1. **Ensure you are on `dev`**: `git checkout dev`
2. **Commit and push any uncommitted changes**:
   - `git status` — if there are modified or untracked files, stage, commit, and push them.
   - `git pull` — incorporate any remote changes.
   - `git push origin dev` — ensure local and remote are in sync.
   - Verify: `git status` must show `nothing to commit, working tree clean`.
3. **Detect the latest GitHub release** (this is the authoritative previous release, not a local git tag):
   ```bash
   gh release view --json tagName,name,publishedAt --jq '{tag: .tagName, title: .name, date: .publishedAt}'
   ```
   Store the tag name (e.g., `v1.3.2`) and published date for use in later phases.
4. **Verify the release tag exists locally**: `git fetch --tags && git log <prev-tag> --oneline -1`
5. **List code changes since that release**: `git log <prev-tag>..HEAD --oneline -- src/ | grep -v "CI: update version.h"`
   - If there are no code changes, warn the user and ask whether to proceed.

---

## Phase 1: ADR validation

Check whether any architectural changes since the previous release require new or updated ADRs.

1. Review the code commits from Phase 0 step 5.
2. For each significant change — does it affect: architecture, NFRs (security/performance/availability), API contracts, new/replaced dependencies, or build/CI tooling?
3. Check `docs/adr/` for existing ADRs that may need their Related section updated.
4. If new ADRs are needed, create them now on `dev` before proceeding.

See `CLAUDE.md` for ADR creation criteria and format.

---

## Phase 2: Stabilize dev branch

Before starting the release, ensure `dev` is in a releasable state.

1. Commit all open/uncommitted changes on `dev` and push to remote.
2. Run `python build.py` to verify the build succeeds.
3. Commit version.h changes from build.py and push to remote.
4. If the build fails, fix the issue, commit, push, and retry. Repeat until green.

---

## Phase 3: Merge dev to main

1. `git checkout main && git pull origin main`
2. `git merge dev` — resolve conflicts if any (prefer dev for version.h).
3. Commit merge and push to remote.

---

## Phase 4: Gather changes & contributors

On `main`, gather all information for the release notes.

```bash
# List all commits since last release tag (ignore CI auto-commits)
git log $(git describe --tags --abbrev=0)..HEAD --oneline | grep -v "CI: update version.h"
```

Categorize each commit as: new feature, bug fix, internal improvement, or breaking change. Check the `docs/adr/` directory for any new or updated ADRs that should be mentioned.

Gather contributors from GitHub (closed issues, merged PRs) and Discord (`#beta-testing`, `#devs-esp-firmware`). See the `/release` skill for automated contributor gathering instructions.

---

## Phase 5: Documentation artifacts

Create or update the following files on `main`.

### 1. Full release notes — `RELEASE_NOTES_<version>.md`

Create in the repository root. Structure:

```
# OTGW-firmware v<version> Release Notes
**Release date:** YYYY-MM-DD
**Branch:** main (from dev)
**Compare:** [v<prev>...v<version>](https://github.com/rvdbreemen/OTGW-firmware/compare/v<prev>...v<version>)

## Overview (1-2 sentences)
## Bug fixes (bulleted, specific)
## New features (if any)
## Internal improvements (if any)
## Breaking changes (explicit — or "No breaking changes vs v<prev>")
## Upgrade notes
```

Categorize changes into:
- **New Features**: functionality added to firmware, Web UI, MQTT, REST API, or hardware
- **Bug Fixes**: issues resolved, with root cause when non-obvious
- **Internal Improvements**: refactors, memory optimizations, code quality
- **Breaking Changes**: MQTT topic renames, API removals, settings format changes. Always declare explicitly even if there are none.

### 2. GitHub release message — `RELEASE_GITHUB_<version>.md`

Create in the repository root. This is the concise version pasted into the GitHub Release UI.

- One-line summary at the top
- Links to full release notes, README, and API docs
- Bulleted sections: Bug fixes, Improvements, Upgrade notes
- Keep it punchy — avoid deep technical refactors unless they affect stability
- End with a **Thank You** section and Discord link (see below)

#### Community acknowledgments (Thank You section)

Every GitHub release message **must** include a Thank You section that credits community members who contributed to the release. This includes:

- **Issue reporters**: users who filed bug reports or feature requests on GitHub
- **Testers**: users who tested beta builds and provided feedback or logs
- **Discord contributors**: community members who helped diagnose issues, shared logs, or provided insights on Discord
- **Code contributors**: anyone who submitted PRs (auto-credited by GitHub, but mention explicitly too)

To gather contributors:

```bash
# GitHub issue/PR authors since last release
gh issue list --state closed --search "closed:>YYYY-MM-DD" --json author --jq '.[].author.login' | sort -u
gh pr list --state merged --search "merged:>YYYY-MM-DD" --json author --jq '.[].author.login' | sort -u
```

For Discord contributors, automatically read the `#beta-testing` channel (ID: `914498730001072149`) and optionally `#devs-esp-firmware` (ID: `924989767966425158`) on the OTGW-firmware Discord server (guild ID: `812969634638725140`). Filter messages since the previous release date and extract contributors who reported bugs, shared logs, tested builds, or provided diagnostic insights.

**Discord username formatting:** Strip trailing 4-digit numeric suffixes from usernames (e.g., `fuzzyduck3793` → `fuzzyduck`, `simontemplar6623` → `simontemplar`). Keep the original if stripping makes the name ambiguous.

Format:

```markdown
## Thank you

Special shoutout to **@most-active-contributor** for <specific contribution that made the biggest impact this release>!

Thanks to everyone who contributed to this release through bug reports, testing, and feedback:
- **@github-user** — reported the CS override issue with detailed logs
- **username** (Discord) — tested beta builds and confirmed the fix
- **username** (Discord) — provided diagnostic insights that helped identify the root cause

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
```

**Shoutout rule:** Every release highlights the single most impactful community contributor with a special shoutout at the top of the Thank You section. This can be the person who found the most critical bug, did the most testing, or provided the key insight that led to a fix.

**Discord name formatting:** Strip trailing 4-digit suffixes (e.g., `crashevans` not `crashevans9876`, `fuzzyduck` not `fuzzyduck3793`). Use the cleaned name with "(Discord)" suffix to distinguish from GitHub usernames.

When no specific individuals can be identified, still include a general thank you to the community and Discord.

### 3. Breaking changes log — `docs/BREAKING_CHANGES.md`

Prepend a new section at the **top** of the existing log:

```markdown
## <version>

There are **no breaking changes** in `<version>`. <one-line description of release type>.

---
```

If there ARE breaking changes, list them with migration instructions. This file is the chronological record for users who skip multiple versions.

### 4. README.md

Update the top of the file:

1. **Demote** the current "What's New in v<prev>" to "What was new in v<prev>"
2. **Add** a new "What's New in v<version>" section with highlights from the release notes
3. **Link** to the full `RELEASE_NOTES_<version>.md`
4. Verify all version references are correct (no `-beta`, no stale version numbers)

### 5. ADR updates (if applicable)

Check if any changes warrant a new ADR or update to an existing one:
- New architectural patterns → new ADR
- Changes that affect documented decisions → update Related section of existing ADR
- Review `docs/adr/README.md` for the ADR creation criteria

---

## Phase 6: Pre-release checklist

Run through every item below before creating the GitHub release.

### Version strings

- [ ] `src/OTGW-firmware/version.h` — `_VERSION_PRERELEASE` is **commented out** (no `-beta`, `-rc`)
- [ ] `_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION` contain no pre-release suffix
- [ ] File header comments show the correct version (`python build.py` runs `autoinc-semver.py` to propagate)

> **Check:** `grep -r "beta\|rc\|-dev" src/OTGW-firmware/ --include="*.h" --include="*.ino"`

### Documentation completeness

- [ ] `RELEASE_NOTES_<version>.md` exists, is final, contains no "beta" / "rc" / "this branch" language
- [ ] `RELEASE_GITHUB_<version>.md` exists and is ready to paste
- [ ] `docs/BREAKING_CHANGES.md` has a section for this version
- [ ] `README.md` "What's New" heading matches the release version
- [ ] Previous release section in `README.md` is demoted to "What was new"

> **Check:** `grep -n "beta\|Development Release\|new in this branch" README.md RELEASE_NOTES_*.md`

### No debug / placeholder artifacts

- [ ] No `TODO`, `FIXME`, `HACK`, `WIP`, or `remove before release` markers in firmware source
- [ ] No test/debug defaults in `src/OTGW-firmware/data/settings.ini`
- [ ] Default settings are safe for end users (e.g. `MQTTenable: false`, no hardcoded test broker)

> **Check:** `grep -rn "TODO\|FIXME\|HACK\|WIP" src/OTGW-firmware/ --include="*.ino" --include="*.h" --include="*.cpp"`

### Code quality

- [ ] `python evaluate.py` passes without errors
- [ ] `python build.py` builds cleanly (firmware + filesystem)

### Git state

- [ ] Working tree is clean (`git status`)
- [ ] Branch is `main`, up to date with remote
- [ ] No `-beta` or `-rc` tag exists for this version

---

## Phase 7: Release execution

Once the checklist is complete:

1. **Commit all outstanding changes on `main`** and push to remote.

2. **Remove pre-release from `version.h`** — Comment out `_VERSION_PRERELEASE` (or remove the `beta`/`rc` suffix) so the firmware version string is a clean `v<version>` without any pre-release tag. Verify: `grep -n "PRERELEASE" src/OTGW-firmware/version.h`

3. **Run `python build.py`** — This runs `autoinc-semver.py` internally (increments build number, updates version strings across all files) and builds firmware + filesystem. Verify the build succeeds.

4. **Commit the release build** on `main` and push to remote.

5. **Create draft GitHub release (creates the tag):**

   Derive a short title (3-6 words) that summarizes the release theme. Format: `v<version> — <Short Title>`.

   Examples: `v1.3.2 — File Explorer Reliability Fix`, `v1.4.0 — REST API v3 & Prometheus`, `v1.3.1 — Command Queue & CS Override Fix`.

   ```bash
   gh release create v<version> --target main --title "v<version> — <Short Title>" --notes-file RELEASE_GITHUB_<version>.md --draft
   ```

   This creates the `v<version>` tag on the latest `main` commit and a draft release. The release is not yet visible to the public.

6. **Upload build artifacts to the draft release:**

   ```bash
   gh release upload v<version> build/*.ino.bin build/*.littlefs.bin --clobber
   ```

7. **Verify artifacts are attached:**

   ```bash
   gh release view v<version> --json assets --jq '.assets[].name'
   ```

   Confirm that `.ino.bin` and `.littlefs.bin` are listed.

8. **Publish the release (only after artifacts are confirmed):**

   ```bash
   gh release edit v<version> --draft=false --latest
   ```

   This makes the release public and marks it as the latest release. Once published, the release is immutable — assets can no longer be changed.

---

## Phase 8: Post-release verification & Discord announcement

- [ ] Verify artifacts are attached to the GitHub release
- [ ] Flash a device and verify `fwversion` in `GET /api/v2/device/info` shows correct version (no `-beta`)
- [ ] Announce on Discord (automated via OTGW bot, see below)

### Discord release announcements

Post release announcements in both community channels via the OTGW bot (`mcp__discord__discord_send`).

**Dutch — `#nederlandse-ondersteuning`** (channel ID: `815561033036333076`):

```text
**OTGW-firmware v<version> is beschikbaar!**

<korte samenvatting in het Nederlands>

Special shoutout naar **<contributor>** voor <bijdrage>!

Download: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v<tag>
```

**English — `#english-support`** (channel ID: `931267109726593116`):

```text
**OTGW-firmware v<version> is now available!**

<short summary in English>

Special shoutout to **<contributor>** for <contribution>!

Download: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v<tag>
```

Both messages must include the contributor shoutout from the Thank You section and a direct link to the GitHub release.

**CHECKPOINT: Show both Discord messages to the user for approval before sending.**

## Phase 9: Sync dev branch with main

After every release, `dev` must be updated so it descends from the release commit on `main`. This ensures future development builds on the released code.

```bash
# 1. Switch to dev
git checkout dev

# 2. Merge main into dev (brings in the release commit, CI version bump, and tag)
git merge main

# 3. Bump version.h to next development version
#    - Increment patch (e.g., 1.3.1 → 1.3.2) or minor (e.g., 1.3.1 → 1.4.0) as appropriate
#    - Uncomment _VERSION_PRERELEASE and set to "beta"
#    Edit src/OTGW-firmware/version.h:
#      _VERSION_PATCH  → next number
#      _VERSION_PRERELEASE beta  → uncommented

# 4. Commit and push
git add src/OTGW-firmware/version.h
git commit -m "feat: Bump version to v<next>-beta for development"
git push origin dev
```

After this, `dev` is ahead of `main` by exactly one commit (the version bump). All new feature branches should be created from `dev`.

---

## Version numbering

This project follows [Semantic Versioning](https://semver.org/):

| Component | Meaning |
|-----------|---------|
| **Major** | Incompatible API or protocol changes |
| **Minor** | New backward-compatible features |
| **Patch** | Backward-compatible bug fixes |
| **Pre-release** | `beta` during development; commented out for stable releases |
| **Build** | Auto-incremented by `build.py` on every build; not user-controlled |

Version strings are generated by `scripts/autoinc-semver.py` (called by `build.py`) from `version.h`. Do not edit derived macros (`_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION`) by hand — they are overwritten by `build.py`. Edit only `_VERSION_MAJOR`, `_VERSION_MINOR`, `_VERSION_PATCH`, and `_VERSION_PRERELEASE`.

---

## File locations

| File | Purpose |
|------|---------|
| `src/OTGW-firmware/version.h` | Authoritative version — edit manually for major/minor/patch and pre-release |
| `scripts/autoinc-semver.py` | Auto-increments build, updates timestamps, propagates to file headers |
| `RELEASE_NOTES_<version>.md` | Full detailed release notes (root, linked from README) |
| `RELEASE_GITHUB_<version>.md` | Concise release body for GitHub release UI (root) |
| `docs/BREAKING_CHANGES.md` | Cumulative breaking changes log across all versions |
| `README.md` | Project readme — "What's New" section, links to release notes |
| `docs/adr/` | Architecture Decision Records — check for new/updated ADRs per release |
