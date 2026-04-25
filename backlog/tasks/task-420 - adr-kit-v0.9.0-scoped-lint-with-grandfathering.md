---
id: TASK-420
title: 'adr-kit v0.9.0: scoped lint with grandfathering'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 19:56'
updated_date: '2026-04-25 20:14'
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
## Problem

`/adr-kit:lint` runs the four canonical verification gates against every ADR file it finds. That is the right behaviour for *new* ADRs in a project that has adopted the toolkit from day one. It is the wrong behaviour for an established project where 80+ ADRs already exist in a pre-canonical template. The legacy template is fine but the heading shape diverges from canonical, so dozens of files report Completeness FAIL on a structural rule that nobody is being asked to fix.

A lint tool that flags most of the set as failing on a single template-shape rule is not actionable: it is noise. Users either disable it or learn to ignore it. Either outcome defeats the point.

## Goal

Allow projects to opt newer ADRs into strict gating while leaving legacy ADRs in an advisory tier that does not produce FAIL results. Two opt-in mechanisms shipped together.

### Mechanism 1: project-level config file at docs/adr/.adr-kit.json

Recognised fields: `strict_from`, `ignore`, `severity` per gate, `template.required_sections`.

### Mechanism 2: per-ADR HTML-comment markers

`<!-- adr-kit-lint: skip -->`, `skip <gates>`, or `advisory` for one-off grandfathering without a project-wide config.

### Output format

Three result tiers (PASS, ADVISORY, FAIL). The aggregate's bottom line always points at a FAIL, never an ADVISORY.

## What this is not

- Not a per-invocation flag (declarative config only).
- Not a migration helper (that is the planned v0.10.0 `/adr-kit:migrate` work).
- Not a way to silence Consistency by default (filename / heading mismatch and duplicate numbers are real bugs at any age).

## Future work

- v0.10.0: `/adr-kit:migrate` -- interactive helper that walks legacy ADRs and rewrites them into the canonical template.
- v0.11.0: per-team `severity_profile` presets as named bundles instead of per-gate configuration.

## Real-world smoke test

Critical AC: lint an 87-ADR project (OTGW-firmware) after dropping in a `.adr-kit.json` with `strict_from: "ADR-082"`. Expected FAIL count <= 5 and ADVISORY only >= 70.

## Rationale

The four verification gates are good. They are also too rigid for adoption in projects with existing ADR history. v0.9.0 keeps the gates intact and adds a way to apply them surgically: strict where it matters, advisory where it would be noise. This pattern (`disable-next-line`, `noqa`, `ignore`) is borrowed from established lint tools.
<!-- SECTION:DESCRIPTION:END -->

- [ ] #1 skills/lint/SKILL.md reads docs/adr/.adr-kit.json before linting; supports strict_from, ignore, severity, template.required_sections; reports clearly when no config is found
- [ ] #2 skills/lint/SKILL.md scans each ADR for adr-kit-lint markers (skip / skip <gates> / advisory) and applies them per-file
- [ ] #3 Output format gains a third tier ADVISORY between PASS and FAIL with reason annotation
- [ ] #4 Aggregate summary distinguishes PASS strictly / ADVISORY only / FAIL / SKIPPED counts; final next-steps line refers to FAIL count
- [ ] #5 examples/.adr-kit.sample.json with annotated comments explaining each field
- [ ] #6 examples/ADR-sample-003-grandfathered-legacy.md demonstrating per-ADR marker on legacy-shaped ADR
- [ ] #7 README.md gains Configuration section explaining both mechanisms
- [ ] #8 Real-world smoke test: linting OTGW 87-ADR set with strict_from=ADR-082 produces FAIL <= 5 AND ADVISORY only >= 70
- [ ] #9 plugin.json 0.9.0 + CHANGELOG [0.9.0] + tag adr-kit--v0.9.0 + GitHub Release with --latest
- [ ] #10 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte
- [ ] #11 All edits English, em-dash-free, no emojis
<!-- AC:END -->

- [ ] #1 skills/lint/SKILL.md reads docs/adr/.adr-kit.json before linting; supports strict_from, ignore, severity, template.required_sections; reports clearly when no config is found
- [ ] #2 skills/lint/SKILL.md scans each ADR for `<!-- adr-kit-lint: skip -->`, `<!-- adr-kit-lint: skip <gates> -->`, and `<!-- adr-kit-lint: advisory -->` markers and applies them per-file
- [ ] #3 Output format gains a third tier ADVISORY between PASS and FAIL; ADVISORY findings include the reason (e.g. 'ADR predates strict_from=ADR-082' or 'marker: advisory')
- [ ] #4 Aggregate summary distinguishes PASS strictly / ADVISORY only / FAIL / SKIPPED counts; final next-steps line refers to FAIL count, not ADVISORY
- [ ] #5 examples/.adr-kit.sample.json with annotated comments explaining each field
- [ ] #6 examples/ADR-sample-003-grandfathered-legacy.md demonstrating the per-ADR marker on a legacy-shaped ADR
- [ ] #7 README.md gains a Configuration section between ADR conventions and FAQ explaining both mechanisms with examples
- [ ] #8 Real-world smoke test: linting OTGW-firmware's docs/adr/ with strict_from='ADR-082' produces FAIL count <= 5 and 70+ legacy ADRs reported as ADVISORY only
- [ ] #9 plugin.json version 0.9.0, CHANGELOG has [0.9.0] entry + link reference, tag adr-kit--v0.9.0 + GitHub Release with --latest
- [ ] #10 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte with the external repo
- [ ] #11 All edits English, em-dash-free, no emojis
<!-- AC:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 skills/lint/SKILL.md reads docs/adr/.adr-kit.json before linting; supports strict_from, ignore, severity, template.required_sections; reports clearly when no config is found
- [x] #2 skills/lint/SKILL.md scans each ADR for adr-kit-lint markers (skip / skip <gates> / advisory) and applies them per-file
- [x] #3 Output format gains a third tier ADVISORY between PASS and FAIL with reason annotation
- [x] #4 Aggregate summary distinguishes PASS strictly / ADVISORY only / FAIL / SKIPPED counts; final next-steps line refers to FAIL count
- [x] #5 examples/.adr-kit.sample.json with annotated comments explaining each field
- [x] #6 examples/ADR-sample-003-grandfathered-legacy.md demonstrating per-ADR marker on legacy-shaped ADR
- [x] #7 README.md gains Configuration section explaining both mechanisms
- [x] #8 Real-world smoke test: linting OTGW 87-ADR set with strict_from=ADR-082 produces FAIL <= 5 AND ADVISORY only >= 70
- [x] #9 plugin.json 0.9.0 + CHANGELOG [0.9.0] + tag adr-kit--v0.9.0 + GitHub Release with --latest
- [x] #10 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte
- [x] #11 All edits English, em-dash-free, no emojis
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan TASK-420 — adr-kit v0.9.0 scoped lint with grandfathering

### Werkrepo

- External: `D:\Users\Robert\Documents\GitHub\RvdB\adr-kit\` (canonical)
- Mirror: `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\tools\adr-kit\`

### Stappen (volgorde)

1. **Lees huidige `skills/lint/SKILL.md`** integraal om de gate-logica niet te breken.
2. **Update `skills/lint/SKILL.md`** met:
   - Nieuwe sectie "Configuration" boven "Inputs": documenteer `docs/adr/.adr-kit.json` (schema, defaults, error handling)
   - Nieuwe sectie "Per-ADR markers": documenteer drie markervormen (`skip`, `skip <gates>`, `advisory`)
   - "What you check, gate by gate" sectie krijgt severity-laag bovenop bestaande gate-logica
   - "Output format" sectie krijgt ADVISORY-tier toegevoegd in single-file en directory-tree output
   - "Reporting" eindigt met FAIL-count, niet ADVISORY-count
3. **Maak `examples/.adr-kit.sample.json`** met geannoteerde voorbeeld-config (alle velden, comments via `_comment` keys of door inline JSON Schema-stijl `$comment`)
4. **Maak `examples/ADR-sample-003-grandfathered-legacy.md`** als legacy-shaped ADR met `<!-- adr-kit-lint: advisory -->` marker; demonstreert hoe het marker werkt op een ADR die anders zou FAILen op Completeness
5. **Update `README.md`**: voeg "Configuration" sectie toe tussen "ADR conventions" en "FAQ"
6. **Bump versies**:
   - `.claude-plugin/plugin.json` 0.8.0 → 0.9.0
   - `.claude-plugin/marketplace.json` plugin entry → 0.9.0
7. **CHANGELOG entry [0.9.0]**: Added (config-reading, marker-reading, ADVISORY tier, sample files, README section), met link reference
8. **Smoke test (AC #8)**: maak een `docs/adr/.adr-kit.json` met `strict_from: "ADR-082"` in OTGW-firmware en draai de bijgewerkte lint-skill. Verwacht: 70+ ADVISORY (legacy), <=5 FAIL.
9. **Mirror sync** naar `tools/adr-kit/` (alle gewijzigde bestanden + nieuwe samples)
10. **Commit + tag `adr-kit--v0.9.0` + GitHub Release** (--latest)

### Critical design choices

- **JSON met inline comments?** JSON staat geen comments toe. Twee opties:
  - (a) Sample file gebruikt `"_comment"` keys (string values worden door schema genegeerd, blijft valide JSON).
  - (b) Sample file is gewoon `.adr-kit.json` zonder comments en de uitleg staat in een `.adr-kit.sample.md` ernaast.
  
  Kies (a): één file, semi-zelfdocumenterend, valide JSON. Schema mag `additionalProperties: true` zijn op top-level zodat `_comment` keys niet failen.

- **Marker formaat**: `<!-- adr-kit-lint: <directive> -->`. Case-insensitive op directive (skip/SKIP). Whitespace-tolerant. Eerste matching marker wint per gate (om ambiguïteit te voorkomen).

- **Severity logic per gate**:
  - Default per gate (zonder config): `always_strict` (huidige gedrag)
  - `strict_from` actief + ADR predates → `advisory_before_strict_from` reduceert FAIL → ADVISORY
  - Per-ADR marker `advisory` of `skip <gate>` overrules config voor die ADR
  - `ignore` overrules alles → SKIPPED (geen output regels behalve in aggregate)

- **Defaults** (als `.adr-kit.json` ontbreekt): bestaand gedrag onveranderd. Backwards-compatible.

### Risico's

- Skill body wordt langer → overweeg samenvattende decision tree bovenaan om scanability te behouden.
- Gates blijven inhoudelijk hetzelfde — alleen severity verandert. Niet de gate-checks zelf herschrijven; daar zit niets mis mee.
- Smoke test op OTGW-firmware's 87 ADRs is een echte real-world test; als die >5 FAIL geeft, dan zat de gate-logica zelf al strikt op iets anders dan template-shape.

### Out of scope (vooruit gepind)

- `/adr-kit:migrate` (v0.10.0)
- Per-team severity profiles (v0.11.0)
- Automatische CI-integratie van user-project ADRs (de v0.5.0 CI lint blijft alleen `examples/`)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-420 -- adr-kit v0.9.0 published

Adds scoped lint with grandfathering. Two opt-in mechanisms let projects apply the four verification gates surgically.

### Mechanism 1: project-level config

`docs/adr/.adr-kit.json` with fields:

- `strict_from`: first ADR id (inclusive) on which gates are enforced strictly
- `ignore`: ADR ids or filenames to skip entirely
- `severity` per gate: `always_strict` / `always_advisory` / `advisory_before_strict_from` (consistency stays always_strict by default)
- `template.required_sections`: override canonical seven with project's actual template

### Mechanism 2: per-ADR HTML-comment markers

`<!-- adr-kit-lint: skip -->` / `skip <gates>` / `advisory` for one-off grandfathering without a project-wide config.

### Output and reporting

Three result tiers (PASS, ADVISORY, FAIL). Aggregate groups by `PASS strictly / ADVISORY only / FAIL / SKIPPED` with a config banner. The "next step" line always points at a FAIL, never an ADVISORY. Severity decision tree (Graphviz block in SKILL.md) documents precedence: ignore beats markers, markers beat config, within config `always_strict` > `always_advisory` > `advisory_before_strict_from`.

### Real-world smoke test (AC #8 evidence)

Validated against the 87-ADR OTGW-firmware set:

| Config | PASS strictly | ADVISORY only | FAIL |
|---|---|---|---|
| Default (canonical seven, no config) | 0 | 0 | 87 |
| `strict_from=ADR-082`, default severity | 2 | 80 | 5 |

AC #8 expectation `FAIL <= 5 AND ADVISORY only >= 70` is met (5 and 80 respectively). The 5 actionable FAILs are recent ADRs (082, 083, 085, 086, 087) that lack canonical headings: either fix them or add `template.required_sections` to codify a project-specific template.

### Generic vs project-specific separation

User flagged mid-task that the generic adr-kit examples should not contain OTGW-specific data. Verified: generic samples use ADR-042 (HHGTTG-friendly placeholder); ADR-sample-003 is about a fictional ingest service / dedup hash, not OTGW. OTGW-specific config lives in `OTGW-firmware/docs/adr/.adr-kit.json` only.

### Release outputs

- Commit: `b5ff096 chore(release): v0.9.0 (scoped lint with grandfathering)` (10 files; +529 insertions)
- Tag: `adr-kit--v0.9.0` pushed to origin
- GitHub Release: <https://github.com/rvdbreemen/adr-kit/releases/tag/adr-kit--v0.9.0> (marked --latest)
- Mirror sync: `tools/adr-kit/` byte-equivalent (committed in OTGW-firmware on this branch)
- OTGW-firmware: `docs/adr/.adr-kit.json` added with `strict_from: "ADR-082"` policy

### Backwards compatibility

When no `.adr-kit.json` and no per-ADR markers are present, behaviour is identical to v0.8.0 (everything strict, FAIL on any gate failure). Existing users see no change unless they opt in.

### Future work (out of scope, deferred)

- v0.10.0: `/adr-kit:migrate` -- interactive helper to mass-rewrite legacy ADRs into the canonical template.
- v0.11.0: per-team `severity_profile` presets (e.g. `"open-source"` / `"internal"`) as named bundles.
<!-- SECTION:FINAL_SUMMARY:END -->
