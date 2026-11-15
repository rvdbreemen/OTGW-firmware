---
id: TASK-1984
title: >-
  Docs: architecture description of the agentic setup, BlueFlame Bot deep dive
  and an image-generator brief
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T20:26:00Z'
updated_date: '2026-11-15T20:50:00Z'
labels:
  - docs
  - process
  - agentic
dependencies: []
priority: medium
ordinal: 1014000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert asked (cloud session 2026-11-15) for a documentation project: inventory the current agentic setup, how every part communicates including the external sources it consults, and write an architecture description of the whole: people and roles, channels, the agents and models, the software pipeline, how rules are applied and which documents are used, the knowledge base and the ADR kit. Special attention to BlueFlame Bot: how it watches Discord, GitHub and Tweakers and how it answers on each channel. Strongly visual, described at length, so that an image generator can make an overview plate from it. The 2026-11-04 snapshot docs/process/agentic-setup.md (TASK-1624) is the starting point; it predates the bench-tester role, the DM route to Robert and the standing beta permission.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/process/agentic-architecture.md describes people and roles, channels, the agent layer with models, the software pipeline, rules and how each is enforced, the knowledge base and the ADR kit, with file:line citations opened in this session
- [ ] #2 A BlueFlame Bot chapter describes how it watches Discord, GitHub and Tweakers (trigger, cadence, watermarks, filters) and how it answers per channel, with a flow or sequence diagram per path
- [ ] #3 Every Mermaid diagram in the new document passes a render check in this session
- [ ] #4 docs/process/agentic-architecture-image-brief.md gives an image generator a composition (zones, nodes, connectors, palette, labels) and ready-to-use prompts for an overview plate and a BlueFlame Bot plate
- [ ] #5 An adversarial verification pass re-opened the cited sources and its corrections are applied; the result is logged in the task notes
- [ ] #6 docs/process/agentic-setup.md points to the new document as its current successor
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 tussenstand: inventarisatie loopt als vier parallelle workflows (10 deelgebieden, lezer plus verificateur die elk citaat heropent; actualiteitscheck voor bf-discord, bf-monitor, sat-chain). Beeldbrief docs/process/agentic-architecture-image-brief.md in concept. Correctie uit eigen lezing: #beta-testing, #english-support en #nederlandse-ondersteuning krijgen release-aankondigingen (.claude/skills/beta-release-2x/SKILL.md:278-283, .claude/skills/release-2x/SKILL.md:190-202); ze zijn dus niet puur alleen-lezen. Mermaid-batchcheck opgezet (mermaid 11.4.1 in headless Chromium); conceptdiagrammen 10/10 OK.
<!-- SECTION:NOTES:END -->
