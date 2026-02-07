# How to Ensure GitHub Copilot Always Uses the ADR Skill

This document provides step-by-step instructions for ensuring that GitHub Copilot consistently uses the ADR skill whenever working with the OTGW-firmware repository.

## Quick Start

**TL;DR:** The ADR skill is automatically available to all Copilot agents. To maximize its effectiveness:

1. ✅ Reference ADRs in `.github/copilot-instructions.md` (already done)
2. ✅ Enable GitHub Actions workflow for PR checks (optional, see below)
3. ✅ Use PR template with ADR checklist (optional, see below)
4. ✅ Explicitly invoke when needed: "Use the ADR skill to..."

---

## What Makes the Skill Available?

### Automatic Discovery

GitHub Copilot automatically discovers skills in the `.github/skills/` directory. The ADR skill is already installed at:

```
.github/skills/adr/SKILL.md
```

**No additional setup required** for basic availability.

### Skill Metadata

The skill is defined with this metadata (in SKILL.md frontmatter):

```yaml
---
name: adr
description: 'Architecture Decision Record (ADR) management skill...'
license: MIT
---
```

This makes it a **project-scoped skill** available to all agents.

---

## Ensuring Automatic Invocation

### 1. Copilot Instructions Integration

**Status:** ✅ Already configured

The `.github/copilot-instructions.md` file already includes ADR references in the "Architecture Decision Records (ADRs)" section. This ensures Copilot is aware of:

- ADR location (`docs/adr/`)
- ADR workflow (review before changes)
- Key architectural decisions
- ADR compliance requirements

**No action needed** - already in place.

### 2. GitHub Actions Workflow (Optional but Recommended)

**Status:** ⚠️ Example provided, not enabled by default

To enable automatic ADR compliance checking on every PR:

```bash
# Copy the example workflow
cp .github/workflows/adr-compliance.yml.example .github/workflows/adr-compliance.yml

# Commit and push
git add .github/workflows/adr-compliance.yml
git commit -m "Enable ADR compliance checking in CI"
git push
```

**What this does:**
- Runs `python evaluate.py` on every PR
- Checks ADR references are valid
- Detects new ADRs
- Identifies architectural changes
- Comments on PRs with compliance status

**Benefits:**
- Automatic enforcement
- Catches violations before merge
- Provides guidance in PR comments
- Ensures ADR documentation is complete

### 3. Pull Request Template (Optional but Recommended)

**Status:** ⚠️ Example provided, not enabled by default

To use the PR template with ADR checklist:

```bash
# Copy the example template
cp .github/PULL_REQUEST_TEMPLATE.md.example .github/PULL_REQUEST_TEMPLATE.md

# Commit and push
git add .github/PULL_REQUEST_TEMPLATE.md
git commit -m "Add PR template with ADR compliance checklist"
git push
```

**What this does:**
- Every new PR starts with ADR compliance checklist
- Reminds contributors to review ADRs
- Provides specific ADRs to check against
- Links to ADR documentation

**Benefits:**
- Human-readable compliance guide
- Self-service for contributors
- Consistent PR format
- Clear expectations

---

## Manual Invocation Methods

Even with automatic triggers, you can explicitly invoke the ADR skill:

### Direct Skill Invocation

```
User: "Use the ADR skill to create an ADR for PostgreSQL integration"
User: "Use the ADR skill to check my changes"
User: "ADR skill: document this decision"
```

### Trigger Phrases

These phrases will invoke the skill:

```
"Create an ADR for..."
"Document this architectural decision"
"What alternatives were considered?"
"Why did we choose X over Y?"
"Does this require an ADR?"
"Check ADR compliance"
```

### During Code Review

```
Copilot: "This change introduces a new pattern. Creating ADR-XXX..."
Copilot: "This violates ADR-004. Use static buffers instead."
Copilot: "Checking against existing ADRs..."
```

---

## Verification Steps

### Verify Skill is Available

1. **Ask Copilot:**
   ```
   "What skills are available in this repository?"
   "Show me the ADR skill"
   "List project skills"
   ```

2. **Expected response:**
   Copilot should mention the `adr` skill with its description.

### Verify Automatic Invocation

1. **Make a test change:**
   ```
   # Modify a .ino file to add String class usage
   String myString = "test";
   ```

2. **Ask Copilot:**
   ```
   "Review this change for ADR compliance"
   "Does this violate any architectural decisions?"
   ```

3. **Expected response:**
   Copilot should reference ADR-004 (Static Buffer Allocation) and flag the violation.

### Verify CI Integration (if enabled)

1. **Create a PR with changes**
2. **Check GitHub Actions tab**
3. **Expected result:**
   - ADR Compliance Check workflow runs
   - Comments appear on PR if issues found
   - Workflow passes if compliant

---

## Best Practices for Consistent Skill Usage

### For Developers

**Before coding:**
```
1. Read docs/adr/README.md
2. Ask Copilot: "What ADRs relate to [feature]?"
3. Review relevant ADRs
4. Ask: "Does my approach align with ADRs?"
```

**During coding:**
```
1. Add comments referencing ADRs:
   // See ADR-009 for PROGMEM usage
2. Ask Copilot to verify:
   "Check this code against ADR-004"
```

**Before PR:**
```
1. Run: python evaluate.py
2. Ask: "Do my changes require a new ADR?"
3. Complete ADR checklist in PR template
```

### For Reviewers

**During review:**
```
1. Check "Files changed" for architectural impact
2. Ask Copilot: "Analyze this PR for ADR compliance"
3. Verify ADR checklist is completed
4. Comment if violations found
```

**When approving:**
```
1. Verify no ADR violations
2. Check new ADRs are properly formatted
3. Ensure ADR index is updated
```

---

## Troubleshooting

### Problem: Copilot doesn't mention ADR skill

**Solution:**
1. Verify file exists: `.github/skills/adr/SKILL.md`
2. Check YAML frontmatter is valid
3. Try explicit invocation: "Use the ADR skill..."
4. Check Copilot has access to `.github/skills/`

### Problem: Skill doesn't provide expected guidance

**Solution:**
1. Review SKILL.md for clarity
2. Update examples if needed
3. Check copilot-instructions.md references ADRs
4. Provide more context in your question

### Problem: CI workflow not running

**Solution:**
1. Verify file at: `.github/workflows/adr-compliance.yml`
2. Check GitHub Actions is enabled for repo
3. Review workflow logs for errors
4. Ensure Python and evaluate.py work locally

### Problem: Too many false positives

**Solution:**
1. Tune evaluate.py checks
2. Update ADR documentation for clarity
3. Add exceptions for valid use cases
4. Document patterns in ADRs

---

## Configuration Options

### Customize Workflow Triggers

Edit `.github/workflows/adr-compliance.yml`:

```yaml
on:
  pull_request:
    types: [opened, synchronize]  # Add/remove PR events
    branches:
      - main
      - dev
      - 'release/**'  # Customize branch patterns
```

### Customize Evaluation Checks

Edit `evaluate.py` to add/modify ADR compliance checks:

```python
# Add custom check
def check_custom_pattern(content):
    if pattern_violated:
        return "Violates ADR-XXX: reason"
    return None
```

### Customize PR Template

Edit `.github/PULL_REQUEST_TEMPLATE.md`:

```markdown
## ADR Compliance Checklist

<!-- Add/remove items based on your needs -->
- [ ] Custom check for your project
```

---

## Monitoring and Metrics

### Track ADR Skill Usage

```bash
# Count ADR references in code
grep -r "ADR-[0-9]" src/ | wc -l

# Most referenced ADRs
grep -roh "ADR-[0-9]{3}" src/ | sort | uniq -c | sort -rn

# ADR health check
bash scripts/adr-health.sh  # If you create this script
```

### GitHub Actions Insights

1. Go to repository → Actions tab
2. View "ADR Compliance Check" workflow
3. Check success rate
4. Review comment patterns

---

## Advanced: Custom Skill Triggers

### Add Custom Trigger Phrases

Edit `.github/copilot-instructions.md`:

```markdown
## ADR Skill Triggers

The ADR skill should be invoked when:
- User asks about architecture
- Code changes affect core patterns
- [Your custom trigger here]
```

### Add Domain-Specific ADR Patterns

Edit `.github/skills/adr/SKILL.md`:

```markdown
## Custom Patterns for OTGW-Firmware

### Memory Management Pattern
[Your project-specific guidance]

### Protocol Pattern
[Your project-specific guidance]
```

---

## Summary Checklist

Use this checklist to ensure optimal ADR skill configuration:

- [x] ADR skill installed at `.github/skills/adr/SKILL.md`
- [x] Copilot instructions reference ADRs
- [x] ADR index exists at `docs/adr/README.md`
- [ ] GitHub Actions workflow enabled (optional)
- [ ] PR template with ADR checklist enabled (optional)
- [x] USAGE_GUIDE.md available for reference
- [ ] Team trained on ADR workflow
- [ ] Evaluation framework includes ADR checks

---

## Quick Reference Commands

```bash
# Ask Copilot to use skill
"Use the ADR skill to..."

# Check compliance locally
python evaluate.py

# Create new ADR
"Use ADR skill to create ADR-030 for [decision]"

# Verify ADR references
grep -r "ADR-[0-9]" src/

# Run ADR health check
ls docs/adr/ADR-*.md | wc -l

# Enable CI workflow
cp .github/workflows/adr-compliance.yml.example .github/workflows/adr-compliance.yml

# Enable PR template  
cp .github/PULL_REQUEST_TEMPLATE.md.example .github/PULL_REQUEST_TEMPLATE.md
```

---

## Additional Resources

- **ADR Skill Documentation:** [SKILL.md](SKILL.md)
- **Usage Guide:** [USAGE_GUIDE.md](USAGE_GUIDE.md)
- **ADR Index:** [docs/adr/README.md](../../../docs/adr/README.md)
- **Copilot Instructions:** [.github/copilot-instructions.md](../../copilot-instructions.md)
- **ADR Best Practices:** https://adr.github.io/

---

**Remember:** The ADR skill works best when:
1. ADRs are comprehensive and up-to-date
2. Code includes ADR references
3. Team consistently uses the skill
4. CI/CD enforces compliance
5. Documentation is clear and accessible

Follow these guidelines to ensure GitHub Copilot consistently helps maintain architectural integrity through the ADR skill.
