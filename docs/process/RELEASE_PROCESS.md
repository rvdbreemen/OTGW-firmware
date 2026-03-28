# Release Process

This document describes the complete end-to-end release process for OTGW-firmware, from documentation preparation through GitHub release publication.

---

## Phase 1: Gather changes

Before writing anything, understand what changed since the last release.

```bash
# List all commits since last release tag (ignore CI auto-commits)
git log v1.3.0..HEAD --oneline | grep -v "CI: update version.h"
```

Categorize each commit as: new feature, bug fix, internal improvement, or breaking change. Check the `docs/adr/` directory for any new or updated ADRs that should be mentioned.

---

## Phase 2: Documentation artifacts

Create or update the following files **before** finalizing version strings.

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

For Discord contributors, check the relevant channels for users who reported bugs, shared logs, or helped test. Ask the maintainer if unsure.

Format:

```markdown
## Thank you

Thanks to everyone who contributed to this release through bug reports, testing, and feedback:
- **@github-user** — reported the CS override issue with detailed logs
- **Discord: username** — tested beta builds and confirmed the fix
- Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
```

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

## Phase 3: Pre-release checklist

Run through every item below before creating the GitHub release.

### Version strings

- [ ] `src/OTGW-firmware/version.h` — `_VERSION_PRERELEASE` is **commented out** (no `-beta`, `-rc`)
- [ ] `_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION` contain no pre-release suffix
- [ ] File header comments show the correct version (CI auto-fixes via `scripts/autoinc-semver.py` — verify after last CI run)

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

### CI / GitHub Actions

- [ ] All CI checks pass on `main` (green build)
- [ ] No hardcoded pre-release version strings in `.github/workflows/`
- [ ] Release workflow triggers on `release: published` — no changes needed

### Git state

- [ ] Working tree is clean (`git status`)
- [ ] Branch is `main`, up to date with remote
- [ ] No `-beta` or `-rc` tag exists for this version

---

## Phase 4: Release execution

Once the checklist is complete:

1. **Push final commit to `main`** — CI runs `autoinc-semver.py`, increments build number, pushes updated `version.h`.

2. **Verify CI push** — Confirm the "CI: update version.h" commit lands on `main` and the build is green.

3. **Create GitHub release:**
   - Tag: `v<version>` (e.g. `v1.3.1`) pointing at the latest `main` commit
   - Title: `v<version>`
   - Body: paste contents of `RELEASE_GITHUB_<version>.md`
   - Mark as **latest release** (not pre-release)

4. **Release workflow fires automatically** — `.github/workflows/release.yml` builds and attaches `.elf`, `.bin`, and `.littlefs.bin` artifacts.

---

## Phase 5: Post-release verification

- [ ] Verify artifacts are attached to the GitHub release
- [ ] Flash a device and verify `fwversion` in `GET /api/v2/device/info` shows correct version (no `-beta`)
- [ ] Announce on Discord

## Phase 6: Sync dev branch with main

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
| **Build** | Auto-incremented by CI on every push; not user-controlled |

Version strings are generated by `scripts/autoinc-semver.py` from `version.h`. Do not edit derived macros (`_SEMVER_FULL`, `_SEMVER_NOBUILD`, `_VERSION`) by hand — they are overwritten by CI. Edit only `_VERSION_MAJOR`, `_VERSION_MINOR`, `_VERSION_PATCH`, and `_VERSION_PRERELEASE`.

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
