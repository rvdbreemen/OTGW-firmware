---
id: TASK-418
title: 'adr-kit v0.7.0: lint skill + ROADMAP + migration guide'
status: Done
assignee: []
created_date: '2026-04-25 19:23'
updated_date: '2026-04-25 19:27'
labels:
  - adr-kit
  - external-repo
  - feature
  - tier-1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tier-1 follow-up after the v0.4.0/v0.5.0/v0.6.0 polish series. Adds three things that lift adr-kit from "documented" to "actively enforced":

1. **`/adr-kit:lint` skill** at `skills/lint/SKILL.md`. Reads existing ADR files (single file or whole `docs/adr/` directory) and runs the four verification gates against each one. Reports pass/fail per gate with file:line citations for failures. Read-only (allowed-tools: Read, Glob, Grep). Argument-hint: file or directory; defaults to `docs/adr/`.
2. **ROADMAP.md** at repo root. Documents v1.0.0 criteria, planned features for future versions (signals not commitments), out-of-scope non-goals (multi-language, visualisation, Anthropic-specific features), and the maintainer's framing of how decisions get made.
3. **MIGRATING-FROM-ADR-SKILL.md** at repo root. Helps users of Jim's `Jvdbreemen/adr-skill` understand the overlap, the differences, and the migration path (or co-installation, since the skill names are namespaced by plugin prefix and do not conflict).

Updated:
- `README.md`: Install section adds the new `/adr-kit:lint` slash command in the four-command flow becoming five. Adds links to ROADMAP and MIGRATING.
- `INSTALL.md`: optional, mention the lint command in the verification step.
- `.claude-plugin/plugin.json`: version bump to 0.7.0.
- `CHANGELOG.md`: `[0.7.0]` section + link references.

Released as `adr-kit--v0.7.0` (new convention only; legacy `vX.Y.Z` tags are not added for this release). GitHub Release with `--latest`.

Mirror in `OTGW-firmware/tools/adr-kit/` synced.</description>
<parameter name="acceptanceCriteria">["skills/lint/SKILL.md exists with frontmatter (name, description, argument-hint, allowed-tools: [Read, Glob, Grep]) and a body that documents the four-gate lint procedure for both single-file and directory-tree inputs", "ROADMAP.md exists at adr-kit root with sections: Status, Toward v1.0.0, Planned features, Out of scope, How decisions get made, Contributing", "MIGRATING-FROM-ADR-SKILL.md exists at adr-kit root with: TL;DR, Overlap, Differences, Migration path, Co-installation guidance", "README.md install section shows five slash commands (marketplace add, install, reload, setup, lint) and links to both ROADMAP.md and MIGRATING-FROM-ADR-SKILL.md", ".claude-plugin/plugin.json version is 0.7.0, CHANGELOG.md has [0.7.0] entry with link references", "Tag adr-kit--v0.7.0 + GitHub Release v0.7.0 published with --latest", "OTGW-firmware tools/adr-kit/ mirror synced", "All edits em-dash-free, English, no emojis"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 skills/lint/SKILL.md with frontmatter (name, description, argument-hint, allowed-tools: [Read, Glob, Grep]) + body documenting four-gate lint for single-file and tree inputs
- [x] #2 ROADMAP.md at root with Status, Toward v1.0.0, Planned features, Out of scope, How decisions get made, Contributing sections
- [x] #3 MIGRATING-FROM-ADR-SKILL.md at root with TL;DR, Overlap, Differences, Migration path, Co-installation guidance
- [x] #4 README.md install section adds /adr-kit:lint and links to ROADMAP and MIGRATING
- [x] #5 plugin.json version is 0.7.0, CHANGELOG has [0.7.0] entry + link references
- [x] #6 Tag adr-kit--v0.7.0 + GitHub Release published with --latest
- [x] #7 OTGW-firmware tools/adr-kit/ mirror synced
- [x] #8 All edits em-dash-free, English, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v0.7.0 published. Three new artefacts: skills/lint/SKILL.md (/adr-kit:lint slash command, read-only, runs the four gates against existing ADRs in docs/adr/), ROADMAP.md (Status, v1.0.0 criteria, planned features, out-of-scope non-goals, dog-food framing), MIGRATING-FROM-ADR-SKILL.md (three honest migration paths from Jim's adr-skill: replace, co-install, stay). README enriched with optional fifth slash command, new Quickstart bullet for ADR audit, new Project resources section. plugin.json bumped to 0.7.0 with expanded description and "lint" keyword. CHANGELOG [0.7.0] entry plus link reference. Tagged adr-kit--v0.7.0, GitHub Release published with --latest. Mirror in OTGW-firmware/tools/adr-kit/ synced. The lint skill is the conceptual upgrade: it shifts adr-kit from documentation to enforcement against existing ADRs, not just newly generated ones.
<!-- SECTION:FINAL_SUMMARY:END -->
