# adr-kit

A complete Architecture Decision Record (ADR) toolkit for AI coding agents. Drop into any project to give Claude Code, Cursor, GitHub Copilot, or any agent that supports the [Agent Skills](https://agentskills.io/) format a shared, enforceable ADR workflow.

## What it does

- **Skill** (`SKILL.md`): the comprehensive ADR guide. When to write one, what to write, how to verify it, how to supersede it. Includes anti-rationalization guards (the excuses agents use to skip documentation) and four named verification gates (Completeness, Evidence, Clarity, Consistency).
- **Agent** (`agents/adr-generator.md`): a focused subagent for *creating* a new ADR. Hand it a decision and it produces a fully populated `docs/adr/ADR-XXX-title.md` file.
- **Instructions** (`instructions/`): two path-specific instruction files, one for coding work (`adr.coding.md`) and one for code review (`adr.review.md`). They tell the agent which ADR rules apply and when.
- **Template** (`examples/ADR-template.md`): a clean ADR file you can copy.

The three files work together: the skill is the reference, the agent does the writing, the instructions tell the agent what to do at the right moment.

## Why ADRs

Architecture Decision Records are short markdown files that capture *why* a system is built the way it is: the problem, the chosen solution, the alternatives that were rejected, the consequences accepted. They live in the repo (`docs/adr/`) alongside the code they describe.

ADRs are the antidote to "why does this exist?" archaeology three years after the fact. They are also the antidote to silent architectural drift: when the next change conflicts with a documented ADR, the conflict surfaces in code review instead of in a postmortem.

This toolkit adds two patterns to the basic ADR tradition:

- **Anti-rationalization guards**: a table of excuses agents (and humans) use to skip writing an ADR ("it's obvious", "I'll do it later", "the code speaks for itself"), with counter-arguments. Pre-flight discipline.
- **Verification gates**: four named gates an ADR must pass before its Status can flip from `Proposed` to `Accepted`. Reviewer can block on a single named gate ("this fails the Evidence gate, please add measurements").

## Install

See [INSTALL.md](INSTALL.md) for per-tool installation steps:

- **Claude Code**: copy into `.claude/`
- **Cursor**: copy into `.cursor/`
- **GitHub Copilot**: copy into `.github/`

The skill is portable: same content, three target locations. Pick what your team uses.

## File map

```
adr-kit/
├── README.md                       # this file
├── LICENSE                         # MIT
├── INSTALL.md                      # per-tool install
├── SKILL.md                        # the comprehensive ADR guide
├── agents/
│   └── adr-generator.md            # subagent: create a new ADR
├── instructions/
│   ├── adr.coding.md               # ADR rules during coding
│   └── adr.review.md               # ADR checks during PR review
└── examples/
    └── ADR-template.md             # clean template to copy
```

## Quickstart

Once installed in your project:

1. **First time**: ask your agent to "analyze this codebase for undocumented architectural decisions". Use the workflow in `SKILL.md` (Initial Codebase Analysis section) to retroactively document the existing patterns at `Status: Accepted`.
2. **For new work**: when about to make an architecturally significant change, the coding instructions (`adr.coding.md`) point at the agent. The agent (`agents/adr-generator.md`) writes the ADR. The verification gates from the skill validate it.
3. **In code review**: the review instructions (`adr.review.md`) walk through six named checks. The reviewer cites a check by name when blocking a PR.

## ADR conventions

The toolkit defaults to:

- **Filename**: `ADR-XXX-kebab-case-title.md` with uppercase prefix and 3-digit zero-padded number, stored in `docs/adr/`.
- **Heading**: `# ADR-XXX Title`.
- **Sections** in order: Status, Context, Decision, Alternatives Considered, Consequences, Related Decisions, References.
- **Status values**: `Proposed`, `Accepted`, `Deprecated`, `Superseded by ADR-YYY`.
- **Date format**: `YYYY-MM-DD`.

You can change the convention if your project already has a different one (some teams use `adr-NNNN-` lowercase 4-digit, or `0001-` with no prefix). Edit the `## Project Conventions` section in the skill and the agent definition; the rest of the toolkit follows from there.

## Credits

Based on [Michael Nygard's ADR format](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions). The anti-rationalization pattern is inspired by [addyosmani/agent-skills](https://github.com/addyosmani/agent-skills). The verification-gates pattern is inspired by [trailofbits/skills](https://github.com/trailofbits/skills).

## License

MIT. See [LICENSE](LICENSE).

## Contributing

Issues and PRs welcome. The skill is intentionally domain-agnostic; project-specific examples should stay in your own copy of `SKILL.md`, not upstream.
