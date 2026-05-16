---
id: TASK-605
title: Fix documentation review findings 1-5 on dev
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 07:10'
updated_date: '2026-05-16 07:13'
labels:
  - documentation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Address the 5 findings from the dev documentation review: README dev/stable banner self-contradiction, v1.3.5 archive-policy contradiction, CI link-check scope gap, broken ../ relative links in docs/guides, and cosmetic/translation nits.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README.md banner correctly identifies dev as the development/1.5.x maintenance line (not 'stable/public release line')
- [x] #2 DOCUMENTATION_LINKS_POLICY.md rule 4 matches actual release-notes placement (v1.3.4 and older archived; v1.3.5/v1.4.1/v1.5.x current)
- [x] #3 .github/workflows/evaluate.yml link-check scope includes docs/guides and docs/process
- [x] #4 All ../ links in docs/guides/BUILD.md and docs/guides/browser-debug-console.md resolve to real targets
- [x] #5 Cosmetic nits fixed: README.md:24 hard break, PIC_FIRMWARE_EN.md CH-water wording, FLASH_GUIDE_NL.md volgorde-eis
- [x] #6 Extended internal link checker reports 0 broken links across README + docs/{releases,api,features,guides,process}
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. F1 README.md:3-4 banner -> dev/maintenance wording
2. F2 DOCUMENTATION_LINKS_POLICY.md:18 reword rule 4
3. F4 fix ../ links in BUILD.md + browser-debug-console.md (verified targets)
4. F3 add docs/guides+docs/process to evaluate.yml scope
5. F5 README hard break, PIC_FIRMWARE_EN CH wording, FLASH_GUIDE_NL volgorde-eis
6. Run extended link checker -> 0 broken; commit+push; draft PR
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed all 5 dev documentation-review findings.

Changes:
- F1 README.md banner: replaced the main-oriented "stable/public release line" wording (self-contradictory on dev) with branch-correct text identifying dev as the 1.5.x maintenance line, pointing to main/v1.5.0 for stable.
- F2 DOCUMENTATION_LINKS_POLICY.md rule 4: reworded so it matches actual placement (v1.3.4 and older archived; v1.3.5/v1.4.1/v1.5.x stay current) instead of the contradictory "v1.3.x and older".
- F3 .github/workflows/evaluate.yml: added docs/guides and docs/process to the markdown link-check scope (previously the guardrail did not cover the files the recent doc cluster added).
- F4 fixed broken relative links: docs/guides/BUILD.md (../ -> ../../ for README/Makefile/scripts) and docs/guides/browser-debug-console.md (../ -> ../../ for README anchors; .ino links repointed to real ../../src/OTGW-firmware/ paths).
- F5 README.md:24 hard break (match line-31 trailing-space convention); PIC_FIRMWARE_EN.md "hot-water" -> "central-heating water" setpoint (matches NL aanvoerwatertemperatuur, removes DHW ambiguity); FLASH_GUIDE_NL.md "volgordekeuze" -> "volgorde-eis".

Verification: extended internal link checker (README + docs/{releases,api,features,guides,process}) reports 0 broken links. Docs/CI-tooling only: no version bump, no ADR.
<!-- SECTION:FINAL_SUMMARY:END -->
