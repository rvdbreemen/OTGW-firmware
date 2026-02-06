# ADR Skill - Usage and Configuration Guide

## Overview

This guide explains how to ensure GitHub Copilot always uses the ADR skill when working with the OTGW-firmware repository.

## Installation

The ADR skill is installed by default in this repository at:
```
.github/skills/adr/SKILL.md
```

No additional installation steps are required.

---

## Automatic Skill Activation

### How Copilot Detects the Skill

GitHub Copilot automatically discovers skills in the `.github/skills/` directory. The ADR skill will be available to all Copilot agents working in this repository.

### Skill Metadata

```yaml
name: adr
description: Architecture Decision Record management
location: project
```

The skill is **project-scoped**, meaning it's available to all agents in the OTGW-firmware repository.

---

## When the ADR Skill is Invoked

### Automatic Invocation

The ADR skill is automatically invoked when Copilot detects:

1. **Architectural Changes**
   - New design patterns introduced
   - Changes to core system architecture
   - Protocol or communication pattern changes
   - Data structure modifications affecting multiple modules

2. **Code Review & PR Analysis**
   - Pull requests are analyzed against existing ADRs
   - Violations are flagged
   - New ADRs suggested when needed

3. **CI/CD Integration**
   - GitHub Actions workflows trigger ADR compliance checks
   - Automated review comments on PRs
   - Build failures for ADR violations (configurable)

4. **Planning & Design**
   - When discussing architectural alternatives
   - When evaluating technology choices
   - When considering refactoring approaches

### Manual Invocation

Users can explicitly invoke the ADR skill by:

**Direct mention:**
```
User: "Use the ADR skill to document this decision"
User: "Create an ADR for choosing PostgreSQL"
User: "Check ADRs for this change"
```

**Skill-triggering phrases:**
```
User: "Document this architectural decision"
User: "What alternatives did we consider?"
User: "Why did we choose X over Y?"
User: "Create architecture decision record"
```

---

## Ensuring Copilot Always Uses the Skill

### 1. Reference in Copilot Instructions

The `.github/copilot-instructions.md` file already references ADRs. Ensure it includes:

```markdown
## Architecture Decision Records (ADRs)

**IMPORTANT:** This project maintains Architecture Decision Records (ADRs).

### Using the ADR Skill

- **Automatic:** The ADR skill is automatically invoked during code reviews and architectural changes
- **Manual:** Use "adr" skill for creating or managing ADRs
- **Location:** `.github/skills/adr/SKILL.md`

### Before Making Changes

1. Review existing ADRs in `docs/adr/README.md`
2. Check if your change violates any existing decision
3. Create new ADR if introducing architectural change
4. Use the ADR skill for guidance

### ADR Compliance

- Follow patterns established in ADRs
- Never violate architectural decisions without discussion
- Update ADRs when decisions change (supersede, don't modify)
- Reference ADR numbers in code reviews and PRs
```

### 2. GitHub Actions Integration

Create `.github/workflows/adr-compliance.yml`:

```yaml
name: ADR Compliance Check

on:
  pull_request:
    types: [opened, synchronize, reopened]

jobs:
  adr-check:
    runs-on: ubuntu-latest
    name: Check ADR Compliance
    
    steps:
      - name: Checkout code
        uses: actions/checkout@v3
        with:
          fetch-depth: 0
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      
      - name: Run ADR compliance checks
        run: |
          # Check for architectural pattern violations
          python evaluate.py
          
          # Verify ADR references are valid
          echo "Checking ADR references..."
          for file in $(git diff --name-only origin/${{ github.base_ref }}...HEAD); do
            if [ -f "$file" ]; then
              # Extract ADR references
              adr_refs=$(grep -oE "ADR-[0-9]{3}" "$file" || true)
              for adr in $adr_refs; do
                adr_file="docs/adr/$adr-*.md"
                if ! ls $adr_file 1> /dev/null 2>&1; then
                  echo "❌ ERROR: Referenced $adr not found in docs/adr/"
                  exit 1
                fi
              done
            fi
          done
          
          echo "✅ ADR compliance check passed"
      
      - name: Comment on PR
        if: failure()
        uses: actions/github-script@v6
        with:
          script: |
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: '⚠️ **ADR Compliance Check Failed**\n\nThis PR may violate existing Architecture Decision Records or reference non-existent ADRs. Please review the ADR compliance check logs and ensure your changes align with documented architectural decisions.\n\nSee `.github/skills/adr/SKILL.md` for guidance on ADR compliance.'
            })
```

### 3. Pre-commit Hook (Optional)

Create `.githooks/pre-commit`:

```bash
#!/bin/bash
# ADR Compliance Pre-commit Hook

echo "🔍 Checking ADR compliance..."

# Check for common ADR violations
violations=0

# Check for String usage (should use static buffers - ADR-004)
if git diff --cached --name-only | grep -E '\.(ino|cpp|h)$' > /dev/null; then
  if git diff --cached | grep -E '^\+.*String[[:space:]]' | grep -v '//' > /dev/null; then
    echo "⚠️  WARNING: Possible String class usage detected (ADR-004: Static Buffer Allocation)"
    violations=$((violations + 1))
  fi
fi

# Check for missing PROGMEM on string literals (ADR-009)
if git diff --cached | grep -E '^\+.*DebugTln\("' | grep -v 'F(' > /dev/null; then
  echo "⚠️  WARNING: String literal without F() macro detected (ADR-009: PROGMEM)"
  violations=$((violations + 1))
fi

if [ $violations -gt 0 ]; then
  echo ""
  echo "Found $violations potential ADR violation(s)."
  echo "Review your changes against docs/adr/ before committing."
  echo "Use '.github/skills/adr/SKILL.md' for guidance."
  echo ""
  read -p "Continue with commit anyway? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
  fi
fi

echo "✅ ADR compliance check passed"
exit 0
```

Enable the hook:
```bash
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

### 4. Pull Request Template

Create `.github/PULL_REQUEST_TEMPLATE.md`:

```markdown
## Description

[Describe your changes]

## ADR Compliance Checklist

- [ ] I have reviewed existing ADRs in `docs/adr/README.md`
- [ ] My changes do not violate any existing architectural decisions
- [ ] I have created a new ADR if this introduces architectural changes (Status: Proposed)
- [ ] I have updated ADR status if implementing an existing ADR (Proposed → Accepted)
- [ ] I have added code comments referencing relevant ADRs
- [ ] I have updated `docs/adr/README.md` if adding a new ADR

## Related ADRs

[List any ADRs related to this change]

- ADR-XXX: [Title and brief relevance]

## Type of Change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Architectural change (requires new ADR)

## Testing

- [ ] I have tested my changes locally
- [ ] I have run the evaluation framework: `python evaluate.py`
- [ ] I have verified ADR compliance

## Additional Notes

[Any additional information]
```

### 5. Issue Templates

Create `.github/ISSUE_TEMPLATE/architectural-decision.md`:

```markdown
---
name: Architectural Decision
about: Propose a new architectural decision
title: '[ADR] '
labels: ['architecture', 'ADR']
assignees: ''
---

## Proposed Decision

[Brief description of the architectural decision]

## Problem Statement

[What problem does this solve?]

## Proposed Solution

[Your proposed approach]

## Alternatives Considered

1. **Alternative 1:**
   - Pros:
   - Cons:

2. **Alternative 2:**
   - Pros:
   - Cons:

## Constraints

[Any constraints that apply: hardware, memory, security, compatibility]

## Impact

[Which components/modules will be affected?]

## Questions

[Any questions or areas needing discussion]

---

**Note:** Once approved, this will be documented as an ADR in `docs/adr/`. Use the ADR skill for creating the formal ADR document: `.github/skills/adr/SKILL.md`
```

---

## Verifying Skill Activation

### Check if Skill is Available

Ask Copilot:
```
"What skills are available in this repository?"
"Show me the ADR skill capabilities"
"List available project skills"
```

Copilot should respond with information about the ADR skill.

### Test Skill Invocation

Try these commands:
```
"Use the ADR skill to check if my changes violate any decisions"
"Create an ADR for using Redis as a cache"
"What ADRs are related to memory management?"
```

### Verify Automatic Invocation

When making changes:
```
1. Make an architectural change (e.g., add new dependency)
2. Ask Copilot: "Should this change require an ADR?"
3. Copilot should reference the ADR skill and provide guidance
```

---

## Best Practices for Using the ADR Skill

### 1. Early in Development

**Before writing code:**
```
User: "I'm considering using GraphQL instead of REST for the API"
Copilot: [Invokes ADR skill, analyzes existing ADRs, suggests creating ADR]
```

### 2. During Code Review

**Automated PR review:**
```
GitHub Actions → Copilot Review → ADR Skill Invoked → Comments on PR
```

### 3. During Refactoring

**Before major refactor:**
```
User: "I want to refactor the MQTT module to use async/await"
Copilot: [Checks ADR-006 (MQTT Integration Pattern), flags potential violation]
```

### 4. When Answering "Why" Questions

**Understanding decisions:**
```
User: "Why don't we use HTTPS?"
Copilot: [References ADR-003, explains memory constraints and local network model]
```

---

## Troubleshooting

### Skill Not Being Invoked

**Issue:** Copilot doesn't seem to use the ADR skill

**Solutions:**
1. Verify skill file exists at `.github/skills/adr/SKILL.md`
2. Check YAML frontmatter is correctly formatted
3. Explicitly invoke: "Use the ADR skill to..."
4. Check Copilot has access to `.github/skills/` directory

### Skill Invoked but Not Working as Expected

**Issue:** Skill runs but doesn't provide expected guidance

**Solutions:**
1. Review the SKILL.md file for clarity
2. Ensure examples are comprehensive
3. Add more specific trigger phrases
4. Update copilot-instructions.md to better reference skill

### ADR Compliance Check Failing

**Issue:** CI/CD ADR check fails unexpectedly

**Solutions:**
1. Run `python evaluate.py` locally
2. Check for broken ADR references in code
3. Verify ADR files exist for all referenced numbers
4. Review GitHub Actions logs for specific failures

---

## Customization

### Adding Custom ADR Checks

Edit `evaluate.py` to add custom ADR compliance checks:

```python
def check_adr_compliance(file_path, content):
    """Check if code follows ADR patterns"""
    issues = []
    
    # ADR-004: Static Buffer Allocation
    if 'String ' in content and file_path.endswith(('.ino', '.cpp')):
        # Check if in acceptable context (e.g., String methods)
        if not in_string_method_context(content):
            issues.append({
                'file': file_path,
                'adr': 'ADR-004',
                'message': 'String class usage detected. Use static buffers.'
            })
    
    # ADR-009: PROGMEM String Literals
    if 'DebugTln("' in content:
        issues.append({
            'file': file_path,
            'adr': 'ADR-009',
            'message': 'String literal without F() macro. Use F("...")'
        })
    
    # ADR-003: HTTP-Only (no HTTPS)
    if 'https://' in content or 'wss://' in content:
        issues.append({
            'file': file_path,
            'adr': 'ADR-003',
            'message': 'HTTPS/WSS detected. This firmware uses HTTP/WS only.'
        })
    
    return issues
```

### Adding Custom Skill Triggers

Update `.github/copilot-instructions.md`:

```markdown
## Custom ADR Triggers

The ADR skill should be invoked when:
- User mentions "architecture"
- User asks "why we chose..."
- User proposes "alternative to..."
- Code changes affect core patterns
- [Add your custom triggers here]
```

---

## Monitoring and Metrics

### Track ADR Usage

Monitor ADR skill effectiveness:

```bash
# Count ADR references in code
grep -r "ADR-[0-9]" src/ | wc -l

# List most referenced ADRs
grep -rh "ADR-[0-9]{3}" src/ | sort | uniq -c | sort -rn

# Find unreferenced ADRs
for adr in docs/adr/ADR-*.md; do
  num=$(basename "$adr" | grep -oE '[0-9]{3}')
  refs=$(grep -r "ADR-$num" src/ | wc -l)
  if [ $refs -eq 0 ]; then
    echo "ADR-$num: No code references"
  fi
done
```

### ADR Health Dashboard

Create a simple script `scripts/adr-health.sh`:

```bash
#!/bin/bash

echo "📊 ADR Health Dashboard"
echo "======================="

total_adrs=$(ls docs/adr/ADR-*.md 2>/dev/null | wc -l)
proposed=$(grep -l "Status.*Proposed" docs/adr/ADR-*.md 2>/dev/null | wc -l)
accepted=$(grep -l "Status.*Accepted" docs/adr/ADR-*.md 2>/dev/null | wc -l)
superseded=$(grep -l "Status.*Superseded" docs/adr/ADR-*.md 2>/dev/null | wc -l)

echo "Total ADRs: $total_adrs"
echo "Proposed: $proposed"
echo "Accepted: $accepted"
echo "Superseded: $superseded"
echo ""

echo "Recent ADRs (last 5):"
ls -t docs/adr/ADR-*.md | head -5 | while read adr; do
  title=$(grep "^# ADR-" "$adr" | head -1)
  echo "  $title"
done
```

---

## Quick Reference

### For Developers

```bash
# Before making changes
1. Read docs/adr/README.md
2. Check relevant ADRs
3. Ask Copilot: "Does this violate any ADRs?"

# Creating new ADR
1. Get next number: ls docs/adr/ADR-*.md | tail -1
2. Ask Copilot: "Use ADR skill to create ADR-XXX for [decision]"
3. Update docs/adr/README.md

# During PR
1. Reference ADRs in description
2. Complete ADR compliance checklist
3. Respond to automated ADR comments
```

### For Copilot

```markdown
When analyzing code:
1. Check against existing ADRs
2. Flag violations
3. Suggest new ADRs when needed
4. Reference ADR-skill for templates

When creating ADRs:
1. Use template from ADR-skill
2. Include alternatives
3. Add code examples
4. Update README.md
```

---

## Additional Resources

- **ADR Skill Documentation:** `.github/skills/adr/SKILL.md`
- **ADR Index:** `docs/adr/README.md`
- **Copilot Instructions:** `.github/copilot-instructions.md`
- **Evaluation Framework:** `evaluate.py`
- **ADR Best Practices:** https://adr.github.io/

---

## Support

If you have questions about the ADR skill:
1. Review the SKILL.md file
2. Check existing ADRs for examples
3. Ask Copilot: "Help me with ADR skill"
4. Open an issue with label `ADR`

---

**Remember:** The ADR skill is a tool to help document and enforce architectural decisions. Use it proactively to maintain a clear record of why the system is built the way it is.
