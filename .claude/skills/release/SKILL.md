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

Follow the release process documented in `docs/process/RELEASE_PROCESS.md` exactly. The process has 6 phases — execute them in order, pausing for user approval at the marked checkpoints.

### Phase 1: Gather changes

1. Detect the previous release tag: `git describe --tags --abbrev=0`
2. List all commits since that tag: `git log <prev-tag>..HEAD --oneline` (exclude "CI: update version.h" commits)
3. Categorize each commit as: new feature, bug fix, internal improvement, or breaking change
4. Check `docs/adr/` for new or updated ADRs since the previous release
5. Present the categorized change summary to the user

**CHECKPOINT: Ask the user to confirm the change categorization before proceeding.**

### Phase 2: Documentation artifacts

Generate all documentation files. For each file, show the content to the user before writing.

1. **`RELEASE_NOTES_<version>.md`** (repository root) — Full technical release notes following the template in `docs/process/RELEASE_PROCESS.md`
2. **`RELEASE_GITHUB_<version>.md`** (repository root) — Concise GitHub release body with links, bulleted sections, Discord link
3. **`docs/BREAKING_CHANGES.md`** — Prepend a new version section at the top. Always declare explicitly whether there are breaking changes or not.
4. **`README.md`** — Demote current "What's New" to "What was new", add new "What's New in v<version>" section with highlights
5. **ADRs** — If any changes warrant a new ADR, propose it. If existing ADRs need their Related section updated, do so.

**CHECKPOINT: Show all generated documentation to the user for review before proceeding.**

### Phase 3: Pre-release checklist

Run through the checklist from `docs/process/RELEASE_PROCESS.md`:

1. **Version strings**: Check `version.h` — `_VERSION_PRERELEASE` must be commented out, no `-beta` in derived macros
2. **Documentation completeness**: Verify all release files exist and contain no beta/rc language
3. **Debug artifacts**: Scan for TODO/FIXME/HACK/WIP in firmware source
4. **Code quality**: Run `python evaluate.py` if available
5. **Git state**: Verify clean working tree, correct branch

Report the checklist results. Flag any failures.

**CHECKPOINT: Ask the user to confirm the checklist passes before proceeding.**

### Phase 4: Release execution

**CHECKPOINT: Confirm with user before each of these steps — they are not reversible.**

1. Commit all documentation changes to `main`
2. Push to `main` — wait for CI to run `autoinc-semver.py`
3. Verify CI commit lands and build is green
4. Create GitHub release using `gh release create v<version> --title "v<version>" --notes-file RELEASE_GITHUB_<version>.md --latest`

### Phase 5: Post-release verification

1. Verify release artifacts are attached (`.elf`, `.bin`, `.littlefs.bin`)
2. Remind user to flash a device and check `GET /api/v2/device/info`
3. Remind user to announce on Discord

### Phase 6: Sync dev branch with main

1. `git checkout dev`
2. `git merge main`
3. Bump `version.h`: increment patch or minor as appropriate, uncomment `_VERSION_PRERELEASE` and set to `beta`
4. Commit: `feat: Bump version to v<next>-beta for development`
5. Push `dev`

**CHECKPOINT: Confirm the next development version number with the user before committing.**

## Important rules

- **Never skip a checkpoint** — always wait for user approval
- **Never force-push** — all pushes are normal pushes
- **Read `docs/process/RELEASE_PROCESS.md`** at the start of every release for the latest process updates
- **All release notes in Nederlands** if the user communicates in Dutch, otherwise English
- **No emojis** in release notes unless the existing format uses them (README uses them in headings)
