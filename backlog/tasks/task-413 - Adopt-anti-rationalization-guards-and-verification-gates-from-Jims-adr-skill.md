---
id: TASK-413
title: Adopt anti-rationalization guards and verification gates from Jim's adr-skill
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-25 14:27'
updated_date: '2026-04-25 14:28'
labels:
  - skill
  - adr
  - documentation
  - tooling
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Import two patterns from Jim's adr-skill (https://github.com/Jvdbreemen/adr-skill) into the project's .claude/skills/adr/SKILL.md while preserving all OTGW-firmware-specific content.

Patterns to add:

1. **Anti-Rationalization Guards**: a table of excuses agents (or humans) use to skip writing an ADR, paired with counter-arguments. Inspired by Jim's adr-skill which drew it from addyosmani/agent-skills. Plays a pre-flight discipline role: when you hear yourself reaching for one of the excuses, the counter-argument tells you to do the work anyway. Calibrated for this project's tone and constraints.

2. **Named Verification Gates** (Completeness, Evidence, Clarity, Consistency): four gates an ADR must pass before its Status can flip from Proposed to Accepted. Inspired by Jim's adr-skill which drew it from trailofbits/skills. Currently the same content lives scattered across "Critical Analysis Guidelines", "Common ADR Mistakes to Avoid", and "When in Doubt" in the project skill; this restructures it as four enforceable named checkpoints.

Both patterns are additive. They do not remove or change any existing project-specific content (Initial Codebase Analysis workflow, Human Decision Documentation pattern, CI/CD integration with evaluate.py, project-specific category list, ADR examples from the actual codebase).

Insertion points:
- Anti-Rationalization Guards: between the "Do NOT Create ADR For" subsection and the "Initial Codebase Analysis" section. The placement keeps it adjacent to the closely-related "when not to create" content.
- Verification Gates: between "ADR Principles" and "ADR Template". Reviewers can cite a specific gate by name when blocking ADR acceptance, which matches the existing pattern of citing ADR-XXX by number in code review.

Source attribution: a link to Jim's adr-skill at the bottom of each new section, with credit to the upstream inspirations (addyosmani, trailofbits).</description>
<parameter name="acceptanceCriteria">[".claude/skills/adr/SKILL.md contains a new top-level section 'Anti-Rationalization Guards' with at least 7 excuse/counter-argument pairs in a markdown table", "The Anti-Rationalization Guards section is positioned between 'When NOT to Create ADR For' (or its parent) and 'Initial Codebase Analysis'", ".claude/skills/adr/SKILL.md contains a new top-level section 'Verification Gates' with four named gates (Completeness, Evidence, Clarity, Consistency), each with at least 4 checklist items", "The Verification Gates section is positioned between 'ADR Principles' and 'ADR Template'", "Both new sections include a brief credit line linking to Jim's adr-skill (https://github.com/Jvdbreemen/adr-skill) and acknowledging the upstream inspirations (addyosmani/agent-skills, trailofbits/skills)", "All existing project-specific content is preserved verbatim: Initial Codebase Analysis, Human Decision Documentation, ADR examples from the codebase, CI/CD integration examples, project-specific categories (Memory Management, Browser & Client, etc.)", "Skill frontmatter (name, description, license) is unchanged", "No em dashes introduced anywhere in the new content", "Markdown structure (heading levels, table format, list style, code-fence conventions) is consistent with the rest of the file"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 .claude/skills/adr/SKILL.md contains a new top-level section 'Anti-Rationalization Guards' with at least 7 excuse/counter-argument pairs in a markdown table
- [ ] #2 The Anti-Rationalization Guards section is positioned between 'When NOT to Create ADR For' (or its parent) and 'Initial Codebase Analysis'
- [ ] #3 .claude/skills/adr/SKILL.md contains a new top-level section 'Verification Gates' with four named gates (Completeness, Evidence, Clarity, Consistency), each with at least 4 checklist items
- [ ] #4 The Verification Gates section is positioned between 'ADR Principles' and 'ADR Template'
- [ ] #5 Both new sections include a brief credit line linking to Jim's adr-skill (https://github.com/Jvdbreemen/adr-skill) and acknowledging the upstream inspirations (addyosmani/agent-skills, trailofbits/skills)
- [ ] #6 All existing project-specific content is preserved verbatim: Initial Codebase Analysis, Human Decision Documentation, ADR examples from the codebase, CI/CD integration examples, project-specific categories
- [ ] #7 Skill frontmatter (name, description, license) is unchanged
- [ ] #8 No em dashes introduced anywhere in the new content
- [ ] #9 Markdown structure (heading levels, table format, list style, code-fence conventions) is consistent with the rest of the file
<!-- AC:END -->
