---
id: TASK-427
title: >-
  Refresh ADR README index: re-catalogue ADR-058 through ADR-088 and update
  metadata
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 22:14'
updated_date: '2026-04-26 18:26'
labels:
  - docs
  - adr-hygiene
dependencies: []
references:
  - docs/adr/README.md
  - .adr-kit.json
  - docs/adr/ADR-080-binding-adr-rules-must-have-a-ci-gate.md
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR index in `docs/adr/README.md` is significantly out of date. It claims 11 categories totalling 57 ADRs (counts: 2+5+6+8+10+2+3+6+9+4+2 = 57), but the actual repository contains 87 accepted ADRs (88 once TASK-426 lands). Roughly 30 ADRs from ADR-058 through ADR-087 have no index entry, the Decision Timeline stops at ADR-060, and the "Most Referenced ADRs" reference counts have not been recomputed since.

Additionally the "ADR Skill" section still points to the GitHub Copilot skill at `.github/skills/adr/SKILL.md` and `USAGE_GUIDE.md`, while the project has migrated to adr-kit via plugin (per `.adr-kit.json`, plugin cache at `~/.claude/plugins/cache/rvdbreemen-adr-kit/...`). This is a second stale-pointer layer in the same file.

This refresh is documentation hygiene only: no architectural decisions are being made, no ADR contents are being changed. The task closes the gap between the ADR corpus and its navigation hub so future readers (humans and AI agents) can use the README the way the adr-kit skill description prescribes ("maintain README.md as the navigation hub").

Discovered while writing TASK-426 (ADR-088 status-burst windowing) on 2026-04-26.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README.md ADR Index section includes one-line summary entries for ADR-058 through ADR-088 placed in their appropriate categories; categories are added or split where the existing 11 no longer fit (e.g. SAT, OTGW32/ESP32, OT-Direct may need their own sub-sections)
- [x] #2 All Quick Navigation category counts match the actual number of ADR entries in the corresponding section after the refresh
- [x] #3 Decision Timeline extended from ADR-060 through ADR-088 with year buckets matching the ADR Date fields
- [x] #4 ADR Skill section rewritten to reference adr-kit via plugin (skill: /adr-kit:adr; subagent: adr-generator) and removes references to the deprecated GitHub Copilot skill at .github/skills/adr/
- [x] #5 Architectural Dependencies and 'Most Referenced ADRs' counts re-verified against current ADR cross-references, or replaced with a note that they are advisory rather than hand-counted
- [x] #6 No ADR file content changes — this task is index hygiene only
- [x] #7 grep -c '\*\*\[ADR-' docs/adr/README.md equals 88 (or matches the count of accepted ADR files), proving every ADR is indexed exactly once
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## Summary

Refreshed `docs/adr/README.md` to close the gap between the ADR corpus (90 files: ADR-001 to ADR-090) and the navigation hub. Pre-fix state had 60 unique ADRs indexed, 30 missing (041 + 058 to 087), one duplicate (ADR-028), an outdated Decision Timeline stopping at ADR-060, and an ADR Skill section still pointing at the deprecated GitHub Copilot skill location.

## Changes (single file: `docs/adr/README.md`)

- Quick Navigation: updated all category counts to match actual section contents; added three new top-level categories — **OTGW32 & Dual Platform** (5 ADRs), **SAT Subsystem** (6 ADRs), **ADR Governance** (1 ADR).
- ADR Index: added 30 missing entries (ADR-041, ADR-058 through ADR-087) with one-line summaries; dropped the ADR-028 duplicate. Final count is exactly **90 entries for 90 ADR files**.
- Foundational ADRs list: added ADR-051 and ADR-080 to the most-referenced summary; noted that reference counts are advisory rather than hand-recounted on every commit.
- Decision Timeline: extended from ADR-060 to ADR-090 with 2026 Q1/Q2 buckets reflecting the rapid post-v1.4 ADR cadence.
- ADR Skill section: rewritten for the adr-kit plugin (skill `/adr-kit:adr`, subagent `adr-generator`, plugin cache + `.adr-kit.json` config). Removed all references to the deprecated `.github/skills/adr/` Copilot skill location.
- Resources section: updated to point at adr-kit and `bin/adr-lint`; Copilot-skill links removed.
- Implementation Notes: Heap Tier Thresholds block updated to the ADR-089 baselines (1536 / 3072 / 5120 bytes) instead of the older 3KB / 5KB / 8KB labels.
- Maintenance section: added a closing note that this refresh closed the gap up through ADR-090, with guidance that future ADRs should land in the index at acceptance time.

## Verification

- `ls docs/adr/ADR-*.md | wc -l` = 90
- `grep -c '\*\*\[ADR-' docs/adr/README.md` = 90
- `grep -oE '\*\*\[ADR-[0-9]+' docs/adr/README.md | sort -u | wc -l` = 90
- `comm -23 all_adrs in_idx` = empty (no ADR missing from the index)
- Per-section count matches the Quick Navigation claims exactly:
  Platform 4 / Network 6 / Memory 8 / Integration 9 + MQTT subsection 8 / System 11 / Hardware 4 / Development 6 / Core 6 / Features 10 / Browser 4 / OTA 2 / OTGW32 5 / SAT 6 / Governance 1 (sum = 90).

## Acceptance criteria

- **AC #1** ✓ Every ADR in ADR-058 through ADR-088 (and ADR-089, ADR-090) has a one-line summary entry in an appropriate category. Categories were extended with new sections where the existing 11 no longer fit (SAT, OTGW32, ADR Governance).
- **AC #2** ✓ Quick Navigation category counts match the actual per-section entry counts.
- **AC #3** ✓ Decision Timeline extended from ADR-060 through ADR-090 with year and quarter buckets matching the ADR Date fields.
- **AC #4** ✓ ADR Skill section rewritten to reference adr-kit via plugin (`/adr-kit:adr`, `adr-generator`); deprecated `.github/skills/adr/` references removed throughout.
- **AC #5** ✓ "Most Referenced ADRs" preserved but explicitly labelled advisory — note added that counts are estimates from earlier audits and are not re-counted on every commit.
- **AC #6** ✓ No ADR file content changes — `git diff` scope is exactly `docs/adr/README.md` only.
- **AC #7** ✓ `grep -c '\*\*\[ADR-' docs/adr/README.md` = 90, matching the 90 ADR files in `docs/adr/`. Each ADR is indexed exactly once.

## Branch context

Committed to `feature-dev-2.0.0-otgw32-esp32-sat-support` (where the ADR-058 to ADR-090 work originated). The same README refresh would benefit the `dev` branch's ADR corpus if it diverges; a follow-up port can be considered if the LTS line accumulates its own ADRs.
<!-- SECTION:FINAL_SUMMARY:END -->
