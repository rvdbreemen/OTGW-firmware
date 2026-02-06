# ADR Skill for GitHub Copilot

This directory contains the Architecture Decision Record (ADR) management skill for GitHub Copilot.

## Files

- **SKILL.md** - The main skill file containing comprehensive ADR guidance, templates, and best practices
- **USAGE_GUIDE.md** - Instructions for ensuring Copilot always uses this skill, including CI/CD integration
- **README.md** - This file

## What is the ADR Skill?

The ADR skill enables GitHub Copilot to:
- **Create** well-structured Architecture Decision Records
- **Enforce** architectural compliance during code reviews
- **Validate** changes against existing architectural decisions
- **Document** alternatives considered and rejected
- **Guide** developers in making and recording architectural choices

## Quick Start

### For Developers

**Check if your change needs an ADR:**
```
Ask Copilot: "Does this change require an ADR?"
```

**Create a new ADR:**
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

## Documentation

- **Full skill documentation:** [SKILL.md](SKILL.md)
- **Usage and configuration:** [USAGE_GUIDE.md](USAGE_GUIDE.md)
- **Existing ADRs:** [../../docs/adr/README.md](../../docs/adr/README.md)

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

## Contributing

When updating the ADR skill:
1. Ensure SKILL.md follows the skill frontmatter format
2. Update USAGE_GUIDE.md if adding new features
3. Test skill invocation with Copilot
4. Update this README if adding new files

## Related Documentation

- [Architecture Decision Records Index](../../docs/adr/README.md)
- [Copilot Instructions](../copilot-instructions.md)
- [Refactor Skill](../refactor/SKILL.md)

## License

MIT - Same as the OTGW-firmware project
