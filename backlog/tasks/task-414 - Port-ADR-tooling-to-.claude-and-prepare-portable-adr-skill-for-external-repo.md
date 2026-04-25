---
id: TASK-414
title: Port ADR tooling to .claude and prepare portable adr-skill for external repo
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-25 14:32'
updated_date: '2026-04-25 14:32'
labels:
  - adr
  - skill
  - tooling
  - documentation
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bring the .github/ ADR tooling (agent + path-specific instructions) to parity in .claude/ for Claude Code, then prepare a generic portable version that can be lifted into a standalone public repository as a reusable adr-skill.

Background: the project has a comprehensive .github/skills/adr/ setup for GitHub Copilot, plus .github/agents/adr-generator.agent.md (creator agent) and .github/instructions/adr.{coding-agent,code-review}.instructions.md (path-specific behaviour). On the .claude/ side, only .claude/skills/adr/SKILL.md exists. Claude Code agents and project-specific instructions live elsewhere in this repo (CLAUDE.md root) and there is no .claude/agents/ or .claude/instructions/ directory yet.

Phase 1 deliverables (.claude/ parity):
- .claude/agents/adr-generator.md: Claude Code subagent for ADR creation, ported from .github/agents/adr-generator.agent.md, with ADR numbering convention matched to the project (ADR-XXX uppercase 3-digit, not adr-NNNN lowercase 4-digit) so it agrees with docs/adr/README.md.
- .claude/instructions/adr.coding.md: instructions for Claude Code while implementing changes, ported from adr.coding-agent.instructions.md.
- .claude/instructions/adr.review.md: instructions for Claude Code while reviewing PRs, ported from adr.code-review.instructions.md, including the six named checks and review-comment templates.

Phase 2 deliverables (portable staging in tools/adr-skill-portable/):
- README.md: portable skill overview, install instructions for Claude Code / Cursor / GitHub Copilot.
- LICENSE: MIT.
- SKILL.md: generic version of the project SKILL.md, OTGW-specific examples (ESP8266 PROGMEM, static buffers, etc.) replaced with generic equivalents, project-specific category list replaced with neutral domains.
- agents/adr-generator.md: generic subagent.
- instructions/adr.coding.md and adr.review.md: generic versions.
- examples/ADR-template.md: a clean template file.
- INSTALL.md: per-tool install steps.

Phase 3 deliverable (external repo, requires user confirmation first):
- Create rvdbreemen/adr-skill (or similar) on GitHub.
- Push the portable staging contents.

Out of scope:
- Updating .github/skills/adr/SKILL.md to match the .claude/ version (Anti-Rationalization Guards + Verification Gates added in TASK-413). Can be a follow-up task if the user wants Copilot's view to stay in sync.
- Importing ALWAYS_USE_SKILL.md, IMPLEMENTATION_SUMMARY.md, QUICK_START.md, USAGE_GUIDE.md into .claude/. Those are Copilot-specific operational docs; the Claude Code equivalent is the skill itself plus the new instructions files.</description>
<parameter name="acceptanceCriteria">[".claude/agents/adr-generator.md exists with Claude Code subagent frontmatter (name, description, tools or model) and content ported from .github/agents/adr-generator.agent.md, using project ADR conventions (ADR-XXX uppercase 3-digit) consistent with docs/adr/README.md", ".claude/instructions/adr.coding.md exists, content ported from .github/instructions/adr.coding-agent.instructions.md, with references updated from .github/skills/adr/SKILL.md to .claude/skills/adr/SKILL.md", ".claude/instructions/adr.review.md exists, content ported from .github/instructions/adr.code-review.instructions.md, six named checks and review-comment templates preserved, references updated to .claude/skills/adr/", "tools/adr-skill-portable/ directory exists at repo root level (or .claude/skills-export/ if preferred) with README.md, LICENSE (MIT), SKILL.md, agents/adr-generator.md, instructions/adr.coding.md, instructions/adr.review.md, examples/ADR-template.md, INSTALL.md", "Portable SKILL.md is OTGW-firmware-free: no references to ESP8266, PROGMEM, static buffers, evaluate.py, or specific ADR numbers from this project. Project-specific examples replaced with generic ones (e.g., a sample ADR for choosing PostgreSQL over SQLite).", "Portable INSTALL.md documents installation steps for at least three target tools: Claude Code, Cursor, GitHub Copilot.", "Portable LICENSE is MIT, with a sensible copyright line.", "All new files: no em dashes, English throughout, markdown structure consistent.", "External repo creation is gated behind user confirmation; do not run gh repo create without explicit go-ahead."]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 .claude/agents/adr-generator.md exists with Claude Code subagent frontmatter, ported from .github/agents/adr-generator.agent.md, using project ADR conventions (ADR-XXX uppercase 3-digit)
- [ ] #2 .claude/instructions/adr.coding.md exists, ported from .github/instructions/adr.coding-agent.instructions.md, references updated to .claude/skills/adr/SKILL.md
- [ ] #3 .claude/instructions/adr.review.md exists, ported from .github/instructions/adr.code-review.instructions.md, six named checks and review-comment templates preserved
- [ ] #4 tools/adr-skill-portable/ directory exists with README.md, LICENSE (MIT), SKILL.md, agents/adr-generator.md, instructions/adr.coding.md, instructions/adr.review.md, examples/ADR-template.md, INSTALL.md
- [ ] #5 Portable SKILL.md is OTGW-firmware-free: no ESP8266/PROGMEM/static-buffers/evaluate.py references, generic examples instead
- [ ] #6 Portable INSTALL.md documents Claude Code, Cursor, and GitHub Copilot install paths
- [ ] #7 Portable LICENSE is MIT with sensible copyright line
- [ ] #8 All new files: no em dashes, English throughout, markdown structure consistent
- [ ] #9 External repo creation gated behind explicit user confirmation
<!-- AC:END -->
