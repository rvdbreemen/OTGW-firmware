---
name: adr
description: 'Architecture Decision Record (ADR) management skill. Creates, maintains, and enforces architectural decisions. Ensures code changes align with documented decisions. Documents alternatives considered and rejected. Facilitates architectural planning and human decision documentation.'
license: MIT
---

# ADR-Skill: Architecture Decision Record Management

## Overview

This skill enables systematic creation, maintenance, and enforcement of Architecture Decision Records (ADRs) for the OTGW-firmware project. ADRs document significant architectural choices along with their context, alternatives considered, and consequences. They serve as living documentation to help current and future developers understand why the system is built the way it is.

## When to Use

### Automatic Trigger Scenarios

Use this skill **automatically** when:

- **Code review or PR analysis** - Verify changes align with existing ADRs
- **CI/CD automation review** - Enforce architectural compliance
- **Major code changes** - Check if new ADR is needed
- **Architecture planning** - Document important decisions before implementation
- **Refactoring proposals** - Validate against existing decisions or create new ADR

### Explicit User Requests

Use this skill when user mentions:
- "Create an ADR"
- "Document this decision"
- "Architecture decision"
- "Why did we choose..."
- "Alternatives considered"
- "Document my choice"

### Decision Triggers

Create a new ADR when making a decision that:
- Has **long-term impact** on architecture
- Affects **multiple components** or modules
- Involves **trade-offs** between alternatives
- **Constrains future** development choices
- Addresses a **significant technical challenge**
- **Changes existing** architectural patterns
- Requires **human decision** that should be preserved

### Do NOT Create ADR For

- Bug fixes that don't change architecture
- Code refactoring maintaining same structure
- Configuration changes
- Documentation updates (non-architectural)
- Minor feature additions within existing patterns
- Temporary workarounds or experiments

---

## Initial Codebase Analysis

### First-Time Use: Discovering Undocumented Decisions

**IMPORTANT:** On first use or when introducing this skill to an existing codebase, perform a comprehensive architectural analysis to identify and document existing but undocumented decisions.

#### Analysis Workflow

**Step 1: Identify Architectural Patterns**
```bash
# Areas to analyze:
1. Platform choices (ESP8266, Arduino, frameworks)
2. Memory management patterns (static buffers, PROGMEM)
3. Network architecture (protocols, security models)
4. Integration patterns (MQTT, APIs, WebSocket)
5. Core system design (timers, scheduling, persistence)
6. Hardware interfaces (sensors, watchdog, GPIO)
7. Build and development tools
```

**Step 2: Ask Critical Questions**
```
For each pattern discovered:
- WHY was this approach chosen? (context, constraints)
- WHAT alternatives exist? (at least 2-3 viable options)
- WHY were alternatives rejected? (specific technical reasons)
- WHAT are the consequences? (benefits, costs, risks)
- HOW is this implemented? (code examples, key files)
- WHEN was this decided? (estimate if unknown)
```

**Step 3: Generate ADRs Systematically**
```
For each undocumented architectural decision:
1. Use the explore agent to understand the pattern
2. Review code, comments, git history for context
3. Identify constraints (memory, performance, compatibility)
4. Research alternatives (even if obvious)
5. Document consequences (positive AND negative)
6. Create ADR with Status: Accepted (since implemented)
7. Link to actual implementation (files, commits)
```

**Step 4: Prioritize Documentation**
```
Start with foundational decisions that:
- Affect multiple components
- Constrain future choices
- Are non-obvious or counterintuitive
- Have significant trade-offs
- Are frequently questioned
```

#### Initial Analysis Prompts

**Trigger codebase analysis:**
```
"Analyze this codebase to identify undocumented architectural decisions"
"Generate ADRs for existing architectural patterns in this codebase"
"What architectural decisions should be documented in this project?"
```

**For specific areas:**
```
"Identify and document memory management architectural decisions"
"What network architecture decisions are undocumented?"
"Analyze platform choices and create ADRs"
```

#### Example: Discovering ADR-009 (PROGMEM)

**Pattern discovered:** String literals use F() and PSTR() macros throughout codebase

**Critical questions:**
- WHY? → ESP8266 has only 40KB RAM; string literals waste 5-8KB
- Alternatives? → Keep in RAM, external RAM, compressed strings, string table
- Why rejected? → RAM too limited, hardware changes, complexity, doesn't solve problem
- Consequences? → +5-8KB heap (positive), verbose code (negative), flash slower than RAM (accepted)

**Result:** ADR-009 documents mandatory PROGMEM usage with clear rationale

---

## ADR Principles

### The Golden Rules

1. **One Decision Per ADR** - Each ADR captures a single architectural choice
2. **Immutable History** - Never modify accepted ADRs; supersede with new ones instead
3. **Context is King** - Explain WHY the decision was made, not just WHAT
4. **Alternatives Matter** - Document what was considered but rejected
5. **Human Decisions Marked** - Clearly indicate when decision came from user/stakeholder
6. **Critical Analysis** - Be thorough, question assumptions, document trade-offs honestly
7. **Understandable Language** - Write for developers unfamiliar with the decision; avoid unexplained jargon

### ADR Best Practices

```
✓ Write for future developers who weren't there
✓ Include code examples and diagrams
✓ Reference related ADRs
✓ Use clear, simple language
✓ Document constraints that drove the decision
✓ Explain consequences (positive and negative)
✓ Link to implementation (files, PRs, commits)
✓ Be critical - question the decision, document risks
✓ Provide specific evidence (measurements, benchmarks)
✓ Explain technical terms on first use

✗ Don't use jargon without explanation
✗ Don't assume reader knows the context
✗ Don't skip alternatives (even obvious ones)
✗ Don't make assumptions unstated
✗ Don't forget to update status when superseding
✗ Don't be vague ("it's better", "improves performance")
✗ Don't skip negative consequences
✗ Don't write marketing copy - be honest about trade-offs
```

---

## ADR Template

Use this comprehensive template for all new ADRs:

```markdown
# ADR-XXX: [Concise Decision Title]

**Status:** Proposed | Accepted | Deprecated | Superseded by ADR-XXX  
**Date:** YYYY-MM-DD  
**Decision Maker:** [Copilot Agent | User: Name | Team Discussion]

## Context

### Problem Statement
[What problem are we solving? What is the situation or challenge?]

### Background
[Relevant history, current state, or technical context]

### Constraints
[What constraints apply? Hardware, memory, security, compatibility, budget, timeline?]

### Stakeholders
[Who is affected by this decision? Users, developers, operations, integrations?]

## Decision

[Clear statement of the choice made and rationale]

### Why This Choice
[Explain reasoning behind the decision]

### Implementation Summary
[High-level description of how this will be implemented]

## Alternatives Considered

### Alternative 1: [Name]
**Description:** [What is this alternative?]

**Pros:**
- Benefit 1
- Benefit 2

**Cons:**
- Drawback 1
- Drawback 2

**Why Not Chosen:** [Clear explanation]

### Alternative 2: [Name]
[Repeat structure for each alternative]

[Include at least 2-3 alternatives. If none exist, explain why this is the only viable option.]

## Consequences

### Positive
- **[Benefit Category]:** Specific benefit
- **[Another Category]:** Another benefit

### Negative
- **[Cost/Limitation]:** Specific drawback
- **[Trade-off]:** What we're giving up

### Risks & Mitigation
- **Risk:** [Description]  
  **Mitigation:** [How we address this]

### Impact Areas
- **Performance:** [Impact on system performance]
- **Maintainability:** [Impact on code maintenance]
- **Security:** [Security implications]
- **Scalability:** [Scaling implications]
- **Developer Experience:** [Impact on development]

## Implementation Notes

### Key Files/Modules Affected
- `path/to/file.ext` - [Brief description of changes]
- `another/file.ext` - [Brief description]

### Code Examples

```language
// Example showing how this decision is implemented
function example() {
  // Demonstrate the pattern
}
```

### Migration Required
[If this changes existing code, describe migration steps. Otherwise state "None."]

## Verification

### How to Verify This Decision
[How can a developer verify this decision is being followed?]

### Testing Requirements
[What testing ensures this decision is properly implemented?]

### Monitoring/Metrics
[What metrics indicate this decision is working?]

## Related Decisions

- **Depends on:** ADR-XXX ([Title])
- **Related to:** ADR-XXX ([Title])
- **Supersedes:** ADR-XXX ([Title]) - if applicable
- **Superseded by:** ADR-XXX ([Title]) - if applicable

## References

- [Link to relevant documentation]
- [Link to code examples]
- [Link to related issues/PRs]
- [External resources]
- [Standards or specifications]

## Timeline

- **YYYY-MM-DD:** Initial proposal
- **YYYY-MM-DD:** Discussion/review
- **YYYY-MM-DD:** Accepted
- **YYYY-MM-DD:** Implemented
- **YYYY-MM-DD:** Superseded (if applicable)

---

**Metadata:**
- **ADR Number:** XXX
- **Status:** [Current status]
- **Category:** [Platform/Memory/Network/Integration/etc.]
- **Impact:** [High/Medium/Low]
```

---

## Naming Convention

### ADR File Naming

```
Format: ADR-XXX-short-descriptive-title.md

Where:
- XXX = Zero-padded sequential number (001, 002, ..., 029, 030, etc.)
- short-descriptive-title = Kebab-case description

Examples:
✓ ADR-001-esp8266-platform-selection.md
✓ ADR-009-progmem-string-literals.md
✓ ADR-029-simple-xhr-ota-flash.md

✗ ADR-1-esp8266.md (not zero-padded)
✗ ADR-030-This_Is_Wrong.md (not kebab-case)
✗ adr-030-lowercase-adr.md (ADR prefix must be uppercase)
```

### Number Assignment

- Sequential numbering starting from 001
- Check `docs/adr/` for highest number and increment
- Don't reuse numbers from deprecated/superseded ADRs
- Don't leave gaps in numbering

---

## ADR Categories

Group ADRs by architectural domain for easier navigation:

- **Platform & Build System** - Platform choice, build tools, frameworks
- **Memory Management** - RAM optimization, buffer strategies, PROGMEM
- **Network & Security** - Protocols, encryption, authentication
- **Integration & Communication** - APIs, MQTT, WebSocket
- **System Architecture** - Core patterns, scheduling, persistence
- **Hardware & Reliability** - Hardware interface, watchdog, sensors
- **Development & Build** - Developer tools, CI/CD, testing
- **Core Services** - Time management, queuing, configuration
- **Features & Extensions** - Specific features, sensor integration
- **Browser & Client** - Frontend, browser compatibility, UX
- **OTA & Updates** - Firmware updates, version management

---

## ADR Workflow

### 1. Before Making Architectural Changes

```bash
# Step 1: Review existing ADRs
- Read docs/adr/README.md for navigation
- Search for related decisions
- Check if change conflicts with existing ADRs
- Understand architectural constraints

# Step 2: Determine if new ADR needed
- Will this have long-term impact?
- Does it affect multiple components?
- Are there multiple alternatives?
- Will future developers need context?

# Step 3: If ADR needed, draft it
- Use the template above
- Fill in all sections thoughtfully
- Include code examples
- Document alternatives thoroughly
```

### 2. During Implementation

```bash
# Step 1: Create ADR with Status: Proposed
- Get next ADR number
- Write comprehensive ADR
- Include decision maker (Copilot Agent or User: Name)

# Step 2: Reference ADR in code
- Add comments linking to ADR
- Example: // See ADR-030 for why we use this pattern

# Step 3: Implement according to decision
- Follow patterns established in ADR
- Ensure code aligns with decision
- Implement mitigation for identified risks
```

### 3. After Implementation

```bash
# Step 1: Update ADR status to Accepted
- Change Status: Proposed → Accepted
- Add implementation date to timeline
- Link to PR/commit that implemented it

# Step 2: Update docs/adr/README.md
- Add entry to ADR index
- Categorize appropriately
- Update reference counts if applicable

# Step 3: Store memory (if using Copilot)
- Store key facts for future reference
- Include ADR number in citations
```

### 4. When Superseding an ADR

```bash
# Step 1: Create new ADR
- Write new ADR explaining change
- Reference original ADR
- Explain why change needed

# Step 2: Update old ADR
- Change status to "Superseded by ADR-XXX"
- Do NOT delete or modify decision/context
- Add superseded date to timeline

# Step 3: Update README
- Update both ADRs in index
- Note the supersession relationship
```

---

## Code Review Integration

### During Code Reviews

**Automatic Checks:**
1. Do changes violate any existing ADRs?
2. Do changes require a new ADR?
3. Are ADR references in code accurate?
4. Is ADR status up to date?

**Example Review Comments:**
```
✗ "This change violates ADR-004 (Static Buffer Allocation). 
   The String class should not be used here."

✓ "This change aligns with ADR-007 (Timer-Based Scheduling).
   Good use of DECLARE_TIMER_SEC macro."

? "This introduces a new architectural pattern. 
   Please create an ADR documenting the decision."

! "ADR-029 is referenced but hasn't been updated to Accepted status.
   Please update the status since this is now implemented."
```

### PR Checklist with ADRs

```markdown
## ADR Compliance Checklist

- [ ] Changes reviewed against existing ADRs
- [ ] No violations of architectural decisions
- [ ] New ADR created if needed (Status: Proposed)
- [ ] ADR status updated if implementing existing ADR
- [ ] Code comments reference relevant ADRs
- [ ] docs/adr/README.md updated if new ADR added
```

---

## CI/CD Integration

### Pre-Commit Checks

```bash
# Check ADR compliance
python evaluate.py  # Includes ADR pattern enforcement

# Verify ADR references are valid
grep -r "ADR-[0-9]" src/ | while read line; do
  # Extract ADR number and verify file exists
  # Report broken references
done

# Check for architectural pattern violations
# (e.g., String usage, missing PROGMEM, etc.)
```

### PR Automation

```yaml
# .github/workflows/adr-check.yml
name: ADR Compliance Check

on: [pull_request]

jobs:
  adr-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Check ADR compliance
        run: |
          # Run automated checks
          # Verify no ADR violations
          # Check if new ADRs needed
          
      - name: Comment on PR
        if: violations found
        # Post comment suggesting ADR review
```

---

## Human Decision Documentation

### When User Makes Architectural Choice

If a user explicitly makes an architectural decision, document it clearly:

```markdown
# ADR-XXX: [Title]

**Status:** Accepted  
**Date:** YYYY-MM-DD  
**Decision Maker:** User: [Name/Role]  ← IMPORTANT: Mark as human decision

## Context
[User's problem or request]

## Decision
**User Decision:** [What the user chose]

The user explicitly chose [X] over [Y] because [reason given].

## Alternatives Considered
[What was presented to the user]

### Alternative 1: [Option presented]
**User Feedback:** [User's reasoning for/against]

### Alternative 2: [Option presented]
**User Feedback:** [User's reasoning for/against]

## Rationale
**User's Stated Reasons:**
- Reason 1
- Reason 2

**Technical Context:**
[Agent's analysis of the decision's technical implications]
```

### Example Human Decision ADR

```markdown
# ADR-030: Use PostgreSQL for Sensor Data Storage

**Status:** Accepted  
**Date:** 2026-02-06  
**Decision Maker:** User: Rob van den Breemen

## Context
User requested evaluation of database options for storing sensor historical data.

## Decision
**User Decision:** Use PostgreSQL instead of SQLite

The user explicitly chose PostgreSQL after being presented with both options,
citing need for multi-client access and superior analytics capabilities.

## Alternatives Considered

### Alternative 1: SQLite
**Pros:** Lightweight, embedded, no server needed  
**Cons:** Single writer limitation  
**User Feedback:** "Need multiple Home Assistant instances to query simultaneously"

### Alternative 2: PostgreSQL
**Pros:** Multi-client, powerful queries, excellent HA integration  
**Cons:** Requires separate server, more complex setup  
**User Feedback:** "Worth the complexity for analytics and flexibility"

## Rationale
**User's Stated Reasons:**
- Need concurrent access from multiple HA instances
- Plan to use TimescaleDB extension for time-series data
- Want to run complex queries for energy analytics
- Already running PostgreSQL for other home automation

**Technical Context:**
This decision means we'll implement a RESTful API for sensor data
instead of direct database access from the firmware.
```

---

## ADR Index Management

### Maintaining docs/adr/README.md

The README is the **navigation hub** for all ADRs. Keep it up to date:

**Required Sections:**
1. **What are ADRs?** - Explanation for new readers
2. **Quick Navigation** - By topic with counts
3. **ADR Index** - Full categorized list
4. **ADR Template** - Link or embed template
5. **Key Architectural Themes** - Cross-cutting concerns
6. **Architectural Dependencies** - Which ADRs depend on which
7. **When to Create an ADR** - Guidance
8. **Superseding ADRs** - How to handle changes
9. **Resources** - Links to best practices

**Update Process:**
```bash
# When adding new ADR:
1. Add entry under appropriate category
2. Update category count
3. Update "Foundational ADRs" if highly referenced
4. Update "Decision Timeline" if appropriate
5. Add to "Related Decisions" in other ADRs
```

---

## Code Examples in ADRs

### Good ADR Code Examples

**Principle:** Show, don't just tell

```markdown
## Implementation Notes

### WRONG: No example
Use PROGMEM for string literals.

### RIGHT: Clear example
```cpp
// WRONG - String literal wastes RAM
DebugTln("Starting WiFi");

// CORRECT - F() macro stores in flash
DebugTln(F("Starting WiFi"));

// WRONG - strcmp on PROGMEM
strcmp(value, "ON");

// CORRECT - strcmp_P for PROGMEM
strcmp_P(value, PSTR("ON"));
```
```

### Include Diagrams When Helpful

```markdown
## Implementation Notes

### Architecture Diagram

```
[WebSocket Client] ─── ws:// ───> [ESP8266:81]
                                       │
                                       ├─> [Buffer] (1KB)
                                       │      │
                                       │      ▼
[HTTP Client] ──── http:// ─> [ESP8266:80]  [Queue]
                                       │      │
                                       └──────┴─> [PIC Serial]
```

### Data Flow

```
User Request → REST API → Command Queue → Serial → PIC → OpenTherm Boiler
     ↓             ↓           ↓             ↓
   Validate    JSON Parse  Dedup Check   Protocol
```
```

---

## ADR Metrics & Maintenance

### Health Indicators

**Healthy ADR Repository:**
- ✓ All ADRs have clear status
- ✓ Superseded ADRs reference replacement
- ✓ Code references match existing ADRs
- ✓ README index is up to date
- ✓ Recent ADRs include implementation dates
- ✓ Each ADR has at least 2 alternatives documented

**Needs Attention:**
- ✗ Multiple ADRs with "Proposed" status for >30 days
- ✗ ADR numbers with gaps
- ✗ Code comments reference non-existent ADRs
- ✗ ADRs without alternatives section
- ✗ README categories don't match actual ADRs

### Periodic Review

**Quarterly ADR Review:**
```bash
# Review checklist
1. Are any "Proposed" ADRs abandoned? (Mark deprecated)
2. Are any "Accepted" ADRs being violated? (Update enforcement)
3. Do new patterns need ADRs? (Create them)
4. Are superseded ADRs properly linked? (Verify)
5. Is README accurately reflecting current state? (Update)
```

---

## Integration with Copilot Instructions

### How This Skill Connects to .github/copilot-instructions.md

The copilot instructions file **references** ADRs but doesn't duplicate them:

**In copilot-instructions.md:**
```markdown
## Architecture Decision Records (ADRs)

**IMPORTANT:** This project maintains ADRs documenting key architectural choices.

- **ADR Index:** `docs/adr/README.md`
- **Before making changes:** Review relevant ADRs
- **ADR Compliance:** Follow patterns in ADRs
- **Creating ADRs:** Use the ADR-skill for guidance

**Key Decisions:**
- ADR-003: HTTP-Only (no HTTPS/WSS) ← Link, don't duplicate
- ADR-004: Static Buffer Allocation ← Link, don't duplicate
- ADR-009: PROGMEM String Literals ← Link, don't duplicate
```

**In this skill:**
- Full ADR creation process
- Templates and examples
- Decision guidance
- Workflow details

**Division of Responsibility:**
- **Copilot Instructions:** Quick reference, links to ADRs, enforcement rules
- **ADR-Skill:** Comprehensive ADR creation and management process
- **Individual ADRs:** Detailed context, decisions, and consequences

---

## Examples from OTGW-Firmware

### Example 1: Memory Management ADR (ADR-004)

**What makes it good:**
- Clear problem statement (heap fragmentation)
- Specific measurements (3,130-3,730 bytes saved)
- Multiple alternatives with detailed pros/cons
- Implementation patterns with code examples
- Risk mitigation strategies
- References to evaluation framework

**Key Sections:**
```markdown
## Consequences

### Positive
- **Stability:** Eliminates most out-of-memory crashes
- **Scalability:** Can add features without exhausting RAM
- **Measurable:** RAM usage is stable and predictable

### Negative
- **Code verbosity:** Requires buffer size management
  - Accepted: Necessary trade-off for stability
```

### Example 2: Protocol Decision ADR (ADR-003)

**What makes it good:**
- Explains why not HTTPS (memory constraints)
- Documents security model (local network only)
- Lists 4 rejected alternatives with clear reasoning
- Includes documentation requirements
- References related ADRs (dependencies)

**Key Sections:**
```markdown
## Alternatives Considered

### Alternative 1: HTTPS with Self-Signed Certificates
**Cons:**
- Requires 20-30KB RAM for TLS handshake (50-75% of available heap)
- OTA updates may fail due to insufficient memory

**Why not chosen:** Memory constraints prohibitive
```

### Example 3: Feature ADR (ADR-029)

**What makes it good:**
- Quantifies improvement (68.5% code reduction)
- Shows before/after code comparison
- Explains Safari-specific bug being fixed
- Links to the problem it solves (ADR-025)
- Includes migration path

---

## Quick Reference

### ADR Creation Checklist

```markdown
Creating a new ADR? Check these:

- [ ] Next sequential number assigned (check docs/adr/)
- [ ] Filename follows ADR-XXX-kebab-case-title.md format
- [ ] Status field present (Proposed/Accepted/etc.)
- [ ] Date field present (YYYY-MM-DD)
- [ ] Decision Maker identified (Copilot Agent or User: Name)
- [ ] Context section explains problem clearly
- [ ] At least 2-3 alternatives documented
- [ ] Pros and cons listed for each alternative
- [ ] "Why not chosen" for each rejected alternative
- [ ] Consequences section complete (positive, negative, risks)
- [ ] Code examples included (if applicable)
- [ ] Related ADRs referenced
- [ ] Implementation notes with affected files
- [ ] Added to docs/adr/README.md index
- [ ] Category assigned
- [ ] References/links included
```

### Common ADR Mistakes to Avoid

```markdown
❌ Writing "We should use X" without explaining why
❌ Skipping alternatives ("This is the only way")
❌ No code examples for technical decisions
❌ Forgetting to update status after implementation
❌ Not referencing related ADRs
❌ Vague consequences ("It will be better")
❌ No decision maker attribution
❌ Missing constraints that drove the decision
❌ Modifying accepted ADRs instead of superseding
❌ Not updating README.md index
❌ Using jargon without defining it (e.g., "TLS handshake" without explaining)
❌ Being superficial - not digging into the "why" behind constraints
❌ Hiding negative consequences or pretending there are none
❌ Writing marketing copy instead of honest technical analysis
❌ Skipping measurements ("faster" vs "68.5% code reduction")
❌ Not explaining technical trade-offs in understandable terms
```

### Critical Analysis Guidelines

**Be thorough and questioning:**
```
✓ Challenge assumptions - "Why is this constraint real?"
✓ Quantify impacts - "Saves 5-8KB RAM" not "saves memory"
✓ Be honest about negatives - "Code is more verbose (accepted trade-off)"
✓ Question the decision - "Is this still the right choice?"
✓ Document risks explicitly - "Risk: X. Mitigation: Y."
✓ Use real measurements - "Requires 20-30KB RAM (50-75% of heap)"
✓ Explain technical concepts - "TLS/SSL = encrypted network protocol"
```

**Write in understandable language:**
```
✓ Define acronyms on first use: "PROGMEM (Program Memory)"
✓ Explain technical terms: "heap fragmentation = memory becoming unusable"
✓ Use analogies for complex concepts: "like trying to park a bus in scattered car spaces"
✓ Provide context: "ESP8266 has 40KB RAM total, after libraries ~20-25KB available"
✓ Show concrete examples: Include code snippets showing the pattern
✓ Break down complex ideas: Use bullet points and clear structure
✓ Avoid assumed knowledge: Don't assume reader knows ESP8266 specifics
```

**Example of critical, understandable writing:**
```markdown
## Consequences

### Positive
- **Stability:** Eliminates most out-of-memory crashes
  - Measured: Heap available increased from ~15KB to ~20KB
  - Evidence: No OOM crashes in 30-day stress test after implementation

### Negative
- **Code verbosity:** Every string needs F() macro wrapper
  - Impact: ~10-15% more characters per debug statement
  - Accepted because: Stability more important than typing convenience
  - Example: `DebugTln(F("Message"))` vs `DebugTln("Message")`

### Risks & Mitigation
- **Risk:** Developers forget to use F() macro
  - **Impact:** RAM gradually consumed, eventual crashes
  - **Mitigation 1:** Evaluation framework catches violations (evaluate.py)
  - **Mitigation 2:** Code review checklist includes PROGMEM check
  - **Mitigation 3:** Copilot instructions enforce pattern
```

### When in Doubt

**Ask these questions:**
1. **Will a developer in 6 months understand WHY we did this?**
2. **Have I explained what we REJECTED, not just what we chose?**
3. **Could this ADR help someone avoid making a wrong decision?**
4. **Have I included enough code examples?**
5. **Is the decision maker clearly identified?**
6. **Can someone unfamiliar with this technology understand the core trade-offs?**
7. **Have I been honest about negative consequences?**
8. **Have I quantified impacts with actual measurements?**

If you answered "No" to any of these, improve the ADR.

---

## Skill Invocation

### How Copilot Uses This Skill

**Automatic triggers:**
- When analyzing code for architectural changes
- When creating/reviewing pull requests
- When user asks architectural questions
- When refactoring requires decision documentation

**Manual invocation:**
- User: "Create an ADR for this"
- User: "Document this architectural decision"
- User: "Why did we choose X?"

**Workflow:**
```
1. Copilot detects architectural change
2. Checks existing ADRs for conflicts
3. If new pattern: Suggests creating ADR
4. Uses this skill to generate comprehensive ADR
5. Updates README and references
6. Stores facts for future sessions
```

### Integration with Copilot Instructions

This skill is integrated with GitHub Copilot through multiple instruction layers:

**Repository-wide instructions** (`.github/copilot-instructions.md`):
- Defines ADR workflow for all Copilot interactions
- Establishes ADR lifecycle (Proposed → Accepted → Superseded)
- Specifies when ADRs are required
- Enforces immutability of accepted ADRs

**Path-specific instructions** (`.github/instructions/`):
- `adr.coding-agent.instructions.md` - Specific guidance for coding agent
  - Before/during implementation ADR requirements
  - Creating new ADRs checklist
  - Supersession workflow
- `adr.code-review.instructions.md` - Specific guidance for code review
  - ADR compliance checks
  - Review checklist for architectural changes
  - Review comment examples

**How it works:**
1. Copilot reads repository-wide instructions for all operations
2. Path-specific instructions apply based on context (coding vs review)
3. This skill provides the comprehensive ADR template and best practices
4. Together, they ensure consistent ADR governance across all Copilot interactions

**Verification:**
You can verify custom instructions are being used by checking the "References" section in Copilot Chat responses, where the instruction files will appear as references.

---

## Resources

### Official ADR Resources
- **ADR Best Practices:** https://adr.github.io/
- **Michael Nygard's Original Post:** https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions
- **MADR Template:** https://github.com/adr/madr
- **Joel Parker Henderson Collection:** https://github.com/joelparkerhenderson/architecture-decision-record
- **Microsoft Azure ADR Guide:** https://learn.microsoft.com/en-us/azure/well-architected/architect-role/architecture-decision-record
- **ThoughtWorks Technology Radar:** ADR mentioned as "Adopt"

### ADR Tooling Ecosystem
- **adr-tools** (npryce) - CLI for creating and managing ADRs: https://github.com/npryce/adr-tools
- **Log4brains** - ADR management with static site generation: https://github.com/thomvaill/log4brains
- **ADR Tools Catalog** - Comprehensive tooling list: https://adr.github.io/#tooling

### Project-Specific Resources
- **OTGW-Firmware ADR Index:** `/docs/adr/README.md`
- **Copilot Instructions:** `/.github/copilot-instructions.md`
- **Coding Agent Instructions:** `/.github/instructions/adr.coding-agent.instructions.md`
- **Code Review Instructions:** `/.github/instructions/adr.code-review.instructions.md`
- **Evaluation Framework:** `/evaluate.py` (enforces ADR decisions)

---

**Remember:** ADRs are **living documentation** stored as docs-as-code in the same repository as the implementation. They should be consulted during development, referenced in code reviews, and evolved through supersession (not modification). Good ADRs make architectural decisions visible, understandable, and enforceable.
