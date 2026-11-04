---
id: TASK-416
title: >-
  adr-kit v0.5.0: should-have polish (CI, CONTRIBUTING, manifest enrich,
  frontmatter)
status: Done
assignee: []
created_date: '2026-04-25 19:02'
updated_date: '2026-04-25 19:07'
labels:
  - adr-kit
  - external-repo
  - polish
  - should-have
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add quality-signal items: CI validation, contribution guide, richer plugin manifest, and skill frontmatter improvements.

Scope:
1. GitHub Actions workflow at .github/workflows/validate.yml that on push and pull_request validates: plugin.json with `jq empty`, all SKILL.md / agent / instructions markdown with markdownlint (relaxed config), and presence of required files (.claude-plugin/plugin.json, skills/adr/SKILL.md, skills/setup/SKILL.md, agents/adr-generator.md, README.md, LICENSE).
2. CONTRIBUTING.md describing: dev loop (`claude --plugin-dir .` for local test), how to add a skill or agent, how to bump version, how to release, code style (no em dashes, English, kebab-case file names).
3. Manifest enrichment: add `homepage` field if missing, ensure `keywords` covers the topic set, optionally add an empty `dependencies: []` placeholder for forward-compat.
4. `argument-hint` on the main `adr` skill so users see the expected slash-command syntax.
5. `allowed-tools` on the `setup` skill (Read, Write, Edit) so the user does not get a tool-prompt during the one-time setup.
6. Bump to v0.5.0; tag adr-kit--v0.5.0; GitHub Release.
7. Sync mirror in OTGW-firmware/tools/adr-kit/.

Out of scope (handled in next task):
- Issue/PR templates, security policy, sample ADRs in examples/, README FAQ.</description>
<parameter name="acceptanceCriteria">[".github/workflows/validate.yml exists, runs on push and pull_request, validates plugin.json with jq empty, runs markdownlint over skills/, agents/, instructions/, and verifies the required-files set", "CONTRIBUTING.md exists at adr-kit root with dev-loop, add-a-skill, version-bump, release, and code-style sections", ".claude-plugin/plugin.json contains homepage, an enriched keywords list, and an empty dependencies array placeholder", "skills/adr/SKILL.md frontmatter has an argument-hint field", "skills/setup/SKILL.md frontmatter has allowed-tools listing Read, Write, and Edit", "Plugin manifest version is 0.5.0", "Tag adr-kit--v0.5.0 exists, GitHub Release v0.5.0 is published", "OTGW-firmware tools/adr-kit/ mirror is in sync with the canonical repo at v0.5.0", "All edits are em-dash-free, English, no emojis"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 .github/workflows/validate.yml validates plugin.json + markdownlint + required-files
- [x] #2 CONTRIBUTING.md with dev-loop, add-a-skill, version-bump, release, code-style sections
- [x] #3 plugin.json has homepage, enriched keywords, dependencies: []
- [x] #4 skills/adr/SKILL.md frontmatter has argument-hint
- [x] #5 skills/setup/SKILL.md frontmatter has allowed-tools (Read, Write, Edit)
- [x] #6 plugin.json version is 0.5.0
- [x] #7 Tag adr-kit--v0.5.0 + GitHub Release v0.5.0 published
- [x] #8 OTGW-firmware tools/adr-kit/ mirror in sync with v0.5.0
- [x] #9 All edits em-dash-free, English, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v0.5.0 published. .github/workflows/validate.yml (jq + required-files + version-consistency + markdownlint), CONTRIBUTING.md (dev loop, add-a-skill, version-bump, release, code style), enriched plugin.json (homepage, expanded keywords, dependencies: [], version 0.5.0), argument-hint on skills/adr/SKILL.md, allowed-tools on skills/setup/SKILL.md, CHANGELOG [0.5.0] entry. Tagged adr-kit--v0.5.0, GitHub Release v0.5.0 published with --latest. Mirror in OTGW-firmware/tools/adr-kit/ synced.
<!-- SECTION:FINAL_SUMMARY:END -->
