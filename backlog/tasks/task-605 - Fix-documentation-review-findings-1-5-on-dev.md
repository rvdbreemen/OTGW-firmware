---
id: TASK-605
title: Fix documentation review findings 1-5 on dev
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 07:10'
updated_date: '2026-05-16 07:20'
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
CI note: PR #581 'Run evaluate.py --quick' is RED but PRE-EXISTING on dev base 97fd601 (verified: identical "4 unresolved ADR reference(s) out of 1224", Health 91.7%, before and after this commit; this commit changes zero ADR references). The markdown link-check step this PR governs passes (0 broken). Not a regression — out of scope for a docs review.
<!-- SECTION:FINAL_SUMMARY:END -->
