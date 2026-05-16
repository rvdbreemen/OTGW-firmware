---
id: TASK-605
title: Fix documentation review findings 1-5 on dev
status: To Do
assignee: []
created_date: '2026-05-16 07:10'
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
- [ ] #1 README.md banner correctly identifies dev as the development/1.5.x maintenance line (not 'stable/public release line')
- [ ] #2 DOCUMENTATION_LINKS_POLICY.md rule 4 matches actual release-notes placement (v1.3.4 and older archived; v1.3.5/v1.4.1/v1.5.x current)
- [ ] #3 .github/workflows/evaluate.yml link-check scope includes docs/guides and docs/process
- [ ] #4 All ../ links in docs/guides/BUILD.md and docs/guides/browser-debug-console.md resolve to real targets
- [ ] #5 Cosmetic nits fixed: README.md:24 hard break, PIC_FIRMWARE_EN.md CH-water wording, FLASH_GUIDE_NL.md volgorde-eis
- [ ] #6 Extended internal link checker reports 0 broken links across README + docs/{releases,api,features,guides,process}
<!-- AC:END -->
