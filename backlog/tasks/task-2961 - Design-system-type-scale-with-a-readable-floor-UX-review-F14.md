---
id: TASK-2961
title: Design-system type scale with a readable floor (UX review F14)
status: To Do
assignee: []
created_date: '2026-11-20T14:32:00Z'
labels:
  - ux
  - design-system
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1095000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, finding F14). v2.css has 66 font-size declarations below .75rem (12 px), from .55rem (8.8 px) up; examples v2.css:86, v2.css:282, v2.css:825; the SAT->BOILER tag renders at 9 px. Not a WCAG failure (WCAG sets no minimum size); a readability decision for phones and older users. Design task, not a sprint item: owner otgw-architect with otgw-ux-designer, against the ADR-091 drift gate. A later otgw-webui task brings v2.css onto the scale.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A written type scale in the design system: named steps, at most 4 steps below body size, and a 12 px floor for text a user must read
- [ ] #2 The decision records which elements may stay below the floor (decorative tags) and why
- [ ] #3 A follow-up otgw-webui task exists to bring v2.css onto the scale, with the ADR-091 drift gate as its check
<!-- AC:END -->
