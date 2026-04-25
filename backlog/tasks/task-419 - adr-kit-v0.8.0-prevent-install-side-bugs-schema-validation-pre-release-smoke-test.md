---
id: TASK-419
title: >-
  adr-kit v0.8.0: prevent install-side bugs (schema validation + pre-release
  smoke test)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 19:43'
updated_date: '2026-04-25 20:02'
labels:
  - adr-kit
  - external-repo
  - ci
  - tier-2
  - post-mortem
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two install-side bugs surfaced in v0.7.1 and v0.7.2 because the v0.5.0 CI workflow validates JSON syntax (`jq empty`) and required-files presence, but not the manifest field types or the actual plugin install. Both bugs would have been caught earlier with proper schema validation and a smoke test.

Scope:

1. **JSON schema for `.claude-plugin/plugin.json`** at `schemas/plugin.json.schema.json`. Hand-curated minimum schema based on observed validation errors and the official documented fields:
   - Required: `name` (string, kebab-case), `description` (string), `version` (string, semver pattern).
   - Recommended: `author` (object with `name` and optional `email` / `url`), `homepage` (URL string), `repository` (URL string, NOT object), `license` (string), `keywords` (array of strings), `dependencies` (array).
   - Forbidden patterns we have hit: `repository` as object, `author` as plain string when object expected.

2. **JSON schema for `.claude-plugin/marketplace.json`** at `schemas/marketplace.json.schema.json`. Required: `name`, `description`, `owner`, `plugins` (array). Each plugin entry: `name`, `source`, `version`, `description`.

3. **CI integration**: add a step to `.github/workflows/validate.yml` that runs `ajv` (or `jsonschema` Python lib) to validate both manifests against their schemas. Fails the workflow on schema violations.

4. **CONTRIBUTING.md "Pre-release smoke test" section**: a manual checklist that release authors run in a fresh Claude Code session before pushing a tag. Steps:
   - `claude --plugin-dir /path/to/local/clone`
   - `/plugin` and confirm adr-kit appears in Installed tab without errors.
   - `/help` and confirm `/adr-kit:adr`, `/adr-kit:setup`, `/adr-kit:lint` are listed.
   - Run `/adr-kit:setup` in a scratch project; confirm CLAUDE.md gets the expected section appended (or already-set-up reported).
   - Run `/adr-kit:lint` on a directory with a sample ADR; confirm the four gates report.

5. **v0.8.0 release**: bump plugin.json, CHANGELOG entry, tag adr-kit--v0.8.0, GitHub Release.

6. **Mirror sync**: OTGW-firmware/tools/adr-kit/.

Out of scope:

- Automated install-smoke-test in CI (would require a Claude Code instance in CI, which is not currently feasible).
- Schema for hooks, skills, agents (each has its own frontmatter, but those are markdown not JSON; markdownlint already covers basic structure).
- Tier-2 chore items previously listed: branch protection, dependabot, release-drafter, codespell. Those can be a separate v0.8.x release.

Background context (for whoever picks this up later):

- v0.7.1 fix: missing `.claude-plugin/marketplace.json`. User reported `Unknown command: /adr-kit:setup`.
- v0.7.2 fix: `repository` was object, schema requires string. User reported `Validation errors: repository: Invalid input: expected string, received object`.

Both errors had clear messages but were only surfaced at install time. A schema-validation step in CI would have caught both before release.</description>
<parameter name="acceptanceCriteria">["schemas/plugin.json.schema.json exists, validates the documented field types (repository as string, author as object, etc.), and rejects the v0.7.0/v0.7.1 manifest formats that broke install", "schemas/marketplace.json.schema.json exists with required fields name, description, owner, plugins[]", ".github/workflows/validate.yml runs schema validation on both manifests using ajv or jsonschema and fails the build on violations", "CONTRIBUTING.md has a 'Pre-release smoke test' section with the manual checklist (claude --plugin-dir, /plugin, /help, /adr-kit:setup, /adr-kit:lint)", "plugin.json version is 0.8.0, CHANGELOG has [0.8.0] entry + link reference", "Tag adr-kit--v0.8.0 + GitHub Release published with --latest", "OTGW-firmware tools/adr-kit/ mirror synced", "All edits em-dash-free, English, no emojis", "Schema files include comments or top-level descriptions explaining the bugs they prevent (v0.7.1 missing marketplace.json, v0.7.2 repository-as-object)"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 schemas/plugin.json.schema.json validates documented field types (repository as string, author as object) and rejects the v0.7.0/v0.7.1 formats that broke install
- [x] #2 schemas/marketplace.json.schema.json with required fields name, description, owner, plugins[]
- [x] #3 .github/workflows/validate.yml runs schema validation (ajv or jsonschema) on both manifests, fails on violations
- [x] #4 CONTRIBUTING.md has 'Pre-release smoke test' section with the manual checklist
- [x] #5 plugin.json version 0.8.0, CHANGELOG has [0.8.0] entry + link reference
- [x] #6 Tag adr-kit--v0.8.0 + GitHub Release published with --latest
- [x] #7 OTGW-firmware tools/adr-kit/ mirror synced
- [x] #8 All edits em-dash-free, English, no emojis
- [x] #9 Schema files include description fields explaining the historical bugs they prevent
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan TASK-419 — adr-kit v0.8.0 schema validation + smoke test

### Werkrepo

- External: `D:\Users\Robert\Documents\GitHub\RvdB\adr-kit\` (canonical, het is het publieke repo)
- Mirror: `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\tools\adr-kit\` (byte-equivalent kopie)

Werk eerst in external, sync daarna naar mirror.

### Stappen

1. **schemas/plugin.json.schema.json**: JSON Schema (draft-07) dat de v0.7.2 manifest accepteert maar de twee gefixte foutpatronen verwerpt:
   - `repository` als object → reject (regression test voor v0.7.2 bug)
   - `repository` als URL string → accept
   - `name` kebab-case (`^[a-z][a-z0-9-]*[a-z0-9]$`)
   - `version` semver (`^\\d+\\.\\d+\\.\\d+(-[a-z0-9.-]+)?$`)
   - `author` als object met required `name`, optional `url`/`email` → accept
   - `author` als plain string → reject
   - `keywords` array van strings, `dependencies` array, `homepage` URL, `license` SPDX-ish string
   - Top-level `description` field documenteert welke historische bugs het schema voorkomt

2. **schemas/marketplace.json.schema.json**: required `name`, `description`, `owner` (object met `name`), `plugins` (array). Elk plugin entry: `name`, `source`, `version`, `description`. Top-level description noemt v0.7.1 bug (missende marketplace.json).

3. **CI integratie** in `.github/workflows/validate.yml`:
   - Installeer `ajv-cli` via npm (`npm install -g ajv-cli ajv-formats`)
   - Stap "Validate plugin.json against schema" → `ajv validate -s schemas/plugin.json.schema.json -d .claude-plugin/plugin.json --spec=draft7 -c ajv-formats`
   - Stap "Validate marketplace.json against schema" → idem voor marketplace.json
   - Faal de workflow op schema-violations

4. **CONTRIBUTING.md "Pre-release smoke test"** sectie met de 5-stap checklist uit de taak-omschrijving (claude --plugin-dir, /plugin, /help, /adr-kit:setup, /adr-kit:lint).

5. **Bump + release**:
   - `.claude-plugin/plugin.json` version 0.7.2 → 0.8.0
   - `.claude-plugin/marketplace.json` plugin entry version → 0.8.0
   - `CHANGELOG.md`: nieuwe `[0.8.0]` sectie met **Added** (schemas, CI step, smoke-test doc) en link reference toevoegen
   - Mirror sync naar `tools/adr-kit/`
   - Git commit + tag `adr-kit--v0.8.0` + push
   - `gh release create adr-kit--v0.8.0 --latest --notes-file ...` met release notes

6. **Verifieer geen em-dashes** in alle nieuwe content (gebruik colons/periods/parentheses zoals user feedback).

### Risico's

- ajv-cli vereist Node.js in de CI runner; ubuntu-latest heeft dat al, geen actie nodig.
- Schema te strikt → existing manifest faalt eigen schema. Mitigatie: valideer lokaal **vóór** push, zowel het v0.7.2 manifest als bekende foute manifests (object-repository) handmatig.
- Em-dashes: ik gebruik dubbel hyphen `--` of expliciete colons/parens (per memory `feedback_no_em_dashes`).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-419 — adr-kit v0.8.0 published

Closes the v0.7.1 / v0.7.2 install-side post-mortem with two complementary regression layers.

### Layer 1: JSON Schema validation (CI-enforced)

- `schemas/plugin.json.schema.json` (draft-07): rejects repository-as-object (v0.7.2 regression), author-as-string (defensive), enforces semver/kebab-case/URI formats. Top-level `description` documents which historical bug each constraint guards against.
- `schemas/marketplace.json.schema.json` (draft-07): requires `name`, `description`, `owner.name`, non-empty `plugins` array; per-entry requires `name`/`source`/`version`/`description`.
- `.github/workflows/validate.yml`: two new `ajv-cli` (draft-07 + ajv-formats) steps run on every push and PR. Required-files set extended with `marketplace.json`, both schemas, and `skills/lint/SKILL.md` (the latter was missing since v0.7.0).

### Layer 2: pre-release manual smoke test

`CONTRIBUTING.md` gains a 5-step checklist: `claude --plugin-dir`, `/plugin`, `/help`, `/adr-kit:setup` idempotency check, `/adr-kit:lint` sample run. Schema validation cannot exercise the install path; this checklist does. Both v0.7.1 and v0.7.2 would have been caught by step 2.

### Local sanity check before push

Python `jsonschema` validated current manifests (pass) and 6 known-bad shapes (all rejected): repository-object, author-string, missing version, non-semver version, missing plugins array, empty plugins array.

### Release outputs

- Commit: `13add12 chore(release): v0.8.0 (schema validation + pre-release smoke test)`
- Tag: `adr-kit--v0.8.0` pushed to origin
- GitHub Release: <https://github.com/rvdbreemen/adr-kit/releases/tag/adr-kit--v0.8.0> (marked --latest)
- Mirror sync: `tools/adr-kit/` byte-equivalent (commit pending in OTGW-firmware)

### Notes

- Marketplace `plugin entry version` corrected from 0.7.1 (drift) to 0.8.0.
- Out of scope (deferred): automated install-smoke-test in CI (needs Claude Code instance in runner; not feasible today), schema for skills/agents frontmatter (markdownlint covers basics), official Claude Code plugin manifest spec (TBD by Anthropic).
- Pairs naturally with TASK-420 (v0.9.0 scoped lint with grandfathering); no shared files, no merge concerns.
<!-- SECTION:FINAL_SUMMARY:END -->
