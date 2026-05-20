---
id: TASK-636
title: >-
  feat(release): beta-prerelease workflow — skill + GitHub Action + tag +
  Discord
status: Done
assignee:
  - '@claude'
created_date: '2026-05-20 09:43'
updated_date: '2026-05-20 09:52'
labels:
  - release
  - tooling
  - ci
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Develop a complete beta-prerelease workflow that parallels the existing /release skill but for the lightweight beta cycle. Currently bin/bump-prerelease.sh handles the version mechanics and CLAUDE.md documents the bump policy in prose, but there is no consolidated, repeatable flow for shipping a beta build to the field-testers in Discord #beta-testing. This task delivers (1) a .claude/skills/beta-prerelease/SKILL.md slash-command that orchestrates the local cycle end-to-end, (2) a .github/workflows/beta-prerelease.yml GitHub Action that fires on a v*-beta.* tag push, builds firmware + filesystem binaries, and publishes a GitHub prerelease so the existing release-assets.yml workflow chains in to attach SHA256SUMS / flash scripts / bundle, and (3) a documented Discord-announcement step so testers get a one-line ping in #beta-testing with the new version string and a one-paragraph change summary.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Skill file .claude/skills/beta-prerelease/SKILL.md exists with name+description frontmatter and disable-model-invocation:true (matching the /release skill convention)
- [x] #2 Skill documents the full local flow: clean-state check on dev, run bin/bump-prerelease.sh, build.py --firmware green, evaluate.py --quick green, stage firmware change + version.h + data/version.hash, commit with conventional message, push origin/dev, create+push tag vSEMVER_CORE-PRERELEASE (e.g. v1.6.0-beta.4), post to Discord #beta-testing
- [x] #3 Skill writing-style rules are explicit: no em dashes, English-only release notes, no emojis (matching the /release skill style block)
- [x] #4 Action builds firmware (.ino.bin) and filesystem (.littlefs.bin) binaries using the same build.py invocation as a local build
- [x] #5 Action creates a GitHub release marked prerelease:true, with the tag name as title and a body containing the version string and a link to the dev branch diff since the previous prerelease tag
- [x] #6 Action uploads .ino.bin and .littlefs.bin as release assets so the existing release-assets.yml workflow (triggered on release:published) chains in to attach SHA256SUMS, flash_otgw.sh/.bat, and the flash-bundle zip
- [x] #7 Documentation lives in the skill itself (no separate docs/guides/ file) to keep the source of truth singular, matching the /release skill pattern
- [x] #8 Skill explicitly differentiates itself from /release: beta does not merge to main, does not edit README, does not bump _SEMVER_CORE (only _VERSION_PRERELEASE), and is repeatable many times within one minor cycle
- [x] #9 Verification: a dry-run section in the skill explains how to test the workflow without actually publishing (e.g. push a vX.Y.Z-beta.test tag to a throwaway branch and confirm the Action fires)
- [x] #10 python build.py --firmware exits 0 after the changes (no firmware code is modified, but the gate still applies)
- [x] #11 python evaluate.py --quick shows no new failures
- [x] #12 Final-summary section of the task contains a PR-style summary suitable for the GitHub PR body
- [x] #13 GitHub Action .github/workflows/beta-prerelease.yml triggers on push of tags matching v*-*.* (broad prerelease glob, excludes full releases without dash) and supports workflow_dispatch for manual re-run
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan — TASK-636 beta-prerelease workflow

### 1. Artefacten (twee bestanden + één optionele update)

**A) `.claude/skills/beta-prerelease/SKILL.md`** — slash-command skill, parallel aan `/release`
- Frontmatter: `name: beta-prerelease`, `disable-model-invocation: true` (gebruiker triggert handmatig, niet model)
- Writing-style block: geen em dashes, English-only release notes, geen emojis
- Phases (analoog aan /release maar lichter):
  - Phase 0 - Prepare: `git checkout dev`, clean state check, `git pull`, detect latest prerelease tag via `git tag --list 'v*-beta.*' --sort=-v:refname | head -1`
  - Phase 1 - ADR validation: skip-by-default met disclaimer; alleen pauzeren als changes ADR-impact hebben
  - Phase 2 - Bump: run `bin/bump-prerelease.sh`, capture old->new (bv. `beta.3 -> beta.4`)
  - Phase 3 - Build verify: `python build.py --firmware` exit 0 (mandatory gate)
  - Phase 4 - Evaluator: `python evaluate.py --quick` (mandatory gate)
  - Phase 5 - Commit + push: stage firmware change + `version.h` + `data/version.hash`, commit met conventional message `chore(release): beta.N`, push `origin/dev`
  - Phase 6 - Tag: construeer `v${SEMVER_CORE}-${PRERELEASE}` (bv. `v1.6.0-beta.4`), `git tag -a` met message, `git push origin <tag>`
  - Phase 7 - GitHub Action wachten: hint dat workflow op tag-push fired; URL naar Actions tab
  - Phase 8 - Discord aankondiging: template-block met versie, een-paragraaf summary, link naar release. Skill biedt de tekst aan; user paste'd of (als discord-mcp tools beschikbaar) Claude post via MCP
- Differentiatie van /release expliciet gedocumenteerd: geen main-merge, geen README-edit, geen `_SEMVER_CORE` bump, herhaalbaar binnen één minor cycle
- Dry-run sectie: hoe te testen zonder publicatie (test tag `v0.0.0-beta.test` pushen, Action runnen, release deleten)

**B) `.github/workflows/beta-prerelease.yml`** — GitHub Action, fires op prerelease tag push
- Trigger: `on.push.tags: ['v*-*.*']` (matcht alle prereleases via dash; full releases `v1.5.0` zonder dash matchen niet) + `workflow_dispatch` voor manuele re-run
- Job `build-and-publish`:
  1. checkout
  2. setup-python@v5 met 3.x
  3. `python build.py` (firmware + filesystem) — build.py installeert arduino-cli auto
  4. Locate binaries in `build/` (pattern `*.ino.bin` en `*.littlefs.bin` per `config.py:BUILD_DIR`)
  5. `gh release create` met `--prerelease`, tag van push event, title = tag, body = link naar dev branch diff sinds previous prerelease tag
  6. `gh release upload` voor beide .bin files
- Chain met bestaande `release-assets.yml`: die fired op `release: published`, dus wanneer onze Action de prerelease publiceert pikt release-assets.yml hem op en attached SHA256SUMS + flash_otgw.sh/.bat + flash-bundle zip (geen wijziging nodig in release-assets.yml; getest mentaal door de trigger spec te lezen)
- Permissions: `contents: write` om release te creeren

**C) Geen wijzigingen aan `release-assets.yml`** — die werkt al voor elke published release (incl. prereleases). Wordt alleen geverifieerd.

### 2. Wat NIET in scope is
- Geen automatische Discord-post via GitHub Action webhook in deze iteratie. Reden: vereist `DISCORD_WEBHOOK_URL` secret-setup buiten code; toegevoegd als follow-up task indien gewenst.
- Geen wijziging aan `bin/bump-prerelease.sh` — die werkt prima.
- Geen wijziging aan CLAUDE.md versioning-policy sectie — die blijft de canonical reference.
- Geen ADR. Dit is build/CI tooling binnen een bestaande pattern (parallel aan `/release`), valt onder "minor features within existing patterns" per CLAUDE.md ADR-richtlijn. Open voor discussie als je vindt dat het wel een ADR verdient.

### 3. Cross-worktree impact (per CLAUDE.md rule)
- 2.0.0 worktree heeft eigen version.h en eigen beta cycle. Dezelfde skill+workflow kan analoog werken op de feature-dev-2.0.0 branch. Maar:
  - Deze remote container heeft maar 1 worktree (geverifieerd: alleen `dev`-tree zichtbaar)
  - Cross-worktree port wordt apart taak (`feat-2.0.0: port TASK-636`) zodra de dev-implementatie merged is
  - Niet blocking voor deze task

### 4. Verification gates (matchen ACs)
1. Skill file bestaat met juiste frontmatter en alle 8 phases
2. GitHub Action YAML is geldig (`yamllint` of dry-parse)
3. `python build.py --firmware` exit 0 (geen firmware-impact verwacht, maar gate geldt altijd)
4. `python evaluate.py --quick` geen nieuwe failures
5. Manuele review: skill style block matcht /release conventie

### 5. Branch + push strategie
- Werk op huidige branch `claude/beta-prerelease-workflow-CsHd8`
- Commits per artefact: (1) skill, (2) workflow, optioneel (3) docs/cleanup
- Push naar origin met `-u` op die branch
- Draft PR aanmaken na push (per repo-instructie)
- Geen merge naar dev tot je expliciet akkoord geeft

### Vragen voor de gebruiker (graag bevestigen of corrigeren)
- (a) Tag-glob `v*-*.*` OK, of voorkeur voor expliciet `v*-beta.*` + `v*-alpha.*`?
- (b) Discord-post in deze task alleen als template (manueel) of toch ook automatisch via webhook (vereist DISCORD_WEBHOOK_URL secret setup buiten code)?
- (c) Akkoord om ADR over te slaan?
- (d) Akkoord om de 2.0.0-port als aparte follow-up task te plannen?
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Design decisions (locked after user review of plan)
- Tag glob: v*-*.* (broad; future-proof for rc.*, alpha.*, beta.*)
- Discord: template-only in skill, no webhook automation (no secret setup needed)
- ADR: none (tooling parallel to /release, falls under "minor features within existing patterns")
- 2.0.0 port: skipped (out of scope, 2.0.0 manages its own release flow)
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds a complete beta-prerelease workflow parallel to the existing /release skill. Two artefacts, no other firmware code touched.

## What landed

**.claude/skills/beta-prerelease/SKILL.md** (user-only slash command, disable-model-invocation:true). 8 phases:
- 0: clean-state on dev, detect previous prerelease tag
- 1: ADR validation (skip-by-default)
- 2: bin/bump-prerelease.sh
- 3: python build.py --firmware (mandatory gate)
- 4: python evaluate.py --quick (mandatory gate)
- 5: stage firmware + version.h + data/version.hash, commit, push origin/dev
- 6: tag v${SEMVER_CORE}-${PRERELEASE} (e.g. v1.6.0-beta.4), push tag
- 7: wait for the GitHub Action, verify assets
- 8: Discord announcement template in #beta-testing (one mandatory checkpoint)

Style rules match /release: no em dashes, English-only, no emojis. Dry-run section included.

**.github/workflows/beta-prerelease.yml**. Triggers on push of tags matching v*-*.* (broad prerelease glob: beta, alpha, rc all qualify; full releases v1.5.0 without a dash are excluded). Also supports workflow_dispatch for manual re-run. Job:
1. Resolve tag from GITHUB_REF or workflow_dispatch input
2. Checkout at the tag
3. Setup Python 3.x
4. python build.py (build.py auto-installs arduino-cli on ubuntu-latest)
5. Locate build/*.ino.bin and build/*.littlefs.bin
6. Detect previous prerelease tag for diff link
7. Compose release body (tag, diff link, Discord pointer)
8. gh release create --prerelease (or edit if it already exists)
9. gh release upload for both binaries
10. Verify assets attached

The existing .github/workflows/release-assets.yml chains in automatically on release:published, attaching SHA256SUMS, flash_otgw.sh, flash_otgw.bat, and the OTGW-firmware-<version>-flash-bundle.zip. No change needed to release-assets.yml.

## Design decisions locked with maintainer

- Tag glob v*-*.* (broad, future-proof)
- Discord: template-only in skill, no webhook automation (no secret setup)
- No ADR (tooling parallel to existing pattern)
- No 2.0.0 port in this PR (out of scope)
- CI build is a deliberate departure from /release's "no CI workflows for releases" rule. Betas happen frequently; CI saves time; the build script is the same, only the host differs. Documented in the skill.

## Verification

- python build.py --firmware exit 0 (output: OTGW-firmware-1.6.0-beta.3+eacddbc.ino.bin, 0.71 MB)
- python evaluate.py --quick: 34 passed, 0 warnings, 0 failed, 100% health score
- python3 yaml.safe_load on the workflow file: valid

## PR

Draft PR #607: https://github.com/rvdbreemen/OTGW-firmware/pull/607

## Risks and follow-ups

- The Action does not auto-announce on Discord. The skill produces a template the maintainer pastes. Acceptable for now; can be upgraded to a webhook later if cycle frequency justifies it.
- 2.0.0 port deferred to a separate task once dev implementation is reviewed and merged.
- First real-world dry-run is in the test plan of PR #607 (maintainer pushes v0.0.0-beta.dryrun on a throwaway branch).
<!-- SECTION:FINAL_SUMMARY:END -->
