# ADR Skill for GitHub Copilot

This directory contains the Architecture Decision Record (ADR) management skill for GitHub Copilot.

## Files

- **SKILL.md** - The main skill file containing comprehensive ADR guidance, templates, and best practices
- **USAGE_GUIDE.md** - Instructions for ensuring Copilot always uses this skill, including CI/CD integration
- **ALWAYS_USE_SKILL.md** - Step-by-step guide to ensure Copilot consistently invokes the ADR skill
- **README.md** - This file

## What is the ADR Skill?

The ADR skill enables GitHub Copilot to:
- **Analyze** existing codebases to discover undocumented architectural decisions (first-time use)
- **Create** well-structured Architecture Decision Records with critical analysis
- **Enforce** architectural compliance during code reviews
- **Validate** changes against existing architectural decisions
- **Document** alternatives considered and rejected with honest assessment
- **Guide** developers in making and recording architectural choices
- **Write** in clear, understandable language avoiding unexplained jargon

## Quick Start

### For First-Time Use

**Analyze existing codebase to generate ADRs:**
```
Ask Copilot: "Analyze this codebase to identify undocumented architectural decisions"
Ask Copilot: "Generate ADRs for existing architectural patterns"
```

### For Developers

**Check if your change needs an ADR:**
```
Ask Copilot: "Does this change require an ADR?"
```

**Create a new ADR (with critical analysis):**
```
Ask Copilot: "Use the ADR skill to create an ADR for [your decision]"
```

**Review existing ADRs:**
```
Ask Copilot: "What ADRs are related to [topic]?"
```

### For Copilot

The skill is automatically available to all Copilot agents in this repository. It will be invoked:
- During code reviews and PR analysis
- When architectural changes are detected
- When users mention "architecture decision" or "ADR"
- During planning and design discussions

## Ensuring Consistent Use

**Want to make sure Copilot always uses this skill?**

Read **[ALWAYS_USE_SKILL.md](ALWAYS_USE_SKILL.md)** for:
- Automatic invocation configuration
- CI/CD integration examples
- Verification steps
- Troubleshooting guide
- Best practices

## Documentation

- **Full skill documentation:** [SKILL.md](SKILL.md)
- **Usage and configuration:** [USAGE_GUIDE.md](USAGE_GUIDE.md)
- **How to ensure consistent use:** [ALWAYS_USE_SKILL.md](ALWAYS_USE_SKILL.md) ⭐
- **Existing ADRs:** [../../../docs/adr/README.md](../../../docs/adr/README.md)

## Key Features

### Comprehensive Templates
- Detailed ADR template with all required sections
- Human decision documentation patterns
- Code examples and diagram guidance

### Workflow Integration
- Before/during/after implementation workflows
- ADR supersession process
- README.md maintenance guidelines

### Quality Assurance
- ADR creation checklist
- Common mistakes to avoid
- Health indicators and metrics

### CI/CD Integration
- GitHub Actions workflow examples
- Pre-commit hook templates
- PR template with ADR checklist

## Examples

The skill includes detailed examples from this repository:
- ADR-003: HTTP-Only (no HTTPS)
- ADR-004: Static Buffer Allocation
- ADR-009: PROGMEM String Literals
- ADR-029: Simple XHR-Based OTA Flash

## Optional Enhancements

### Enable CI/CD Checks
```bash
# From repo root, copy example workflow
cp .github/workflows/adr-compliance.yml.example .github/workflows/adr-compliance.yml
```

### Enable PR Template
```bash
# From repo root, copy example PR template
cp .github/PULL_REQUEST_TEMPLATE.md.example .github/PULL_REQUEST_TEMPLATE.md
```

See [ALWAYS_USE_SKILL.md](ALWAYS_USE_SKILL.md) for detailed setup instructions.

## Contributing

When updating the ADR skill:
1. Ensure SKILL.md follows the skill frontmatter format
2. Update USAGE_GUIDE.md if adding new features
3. Test skill invocation with Copilot
4. Update this README if adding new files

## Related Documentation

- [Architecture Decision Records Index](../../docs/adr/README.md)
- [Copilot Instructions](../../copilot-instructions.md)
- [Refactor Skill](../refactor/SKILL.md)

## License

MIT - Same as the OTGW-firmware project
