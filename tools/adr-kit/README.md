# adr-kit

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/tag/rvdbreemen/adr-kit?label=release&sort=semver)](https://github.com/rvdbreemen/adr-kit/releases)

A complete Architecture Decision Record (ADR) toolkit for AI coding agents. Drop into any project to give Claude Code, Claude Cowork, Cursor, GitHub Copilot, OpenAI Codex CLI, or any agent that supports the [Agent Skills](https://agentskills.io/) format a shared, enforceable ADR workflow.

> **Pre-1.0**: the toolkit is functional and in use, but the API and conventions may change before v1.0.0. Pin to a specific tag if you need stability across upgrades. Latest release: see the badge above.

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

### Claude Code (recommended): four slash commands

```
/plugin marketplace add rvdbreemen/adr-kit
/plugin install adr-kit@rvdbreemen-adr-kit
/reload-plugins
/adr-kit:setup
```

The first three install the plugin: marketplace registration, plugin install, plugin reload. The fourth is a one-time per-project setup: it appends a short "ADR Kit Rules" section to your project's `CLAUDE.md` (creating the file if needed) so future sessions know about the skill, the `adr-generator` subagent, and the path-specific instructions. The setup command is idempotent: re-running it skips the append if the section is already there.

Claude Cowork shares the `.claude/` convention; the same plugin commands work once your workspace is connected to a repo.

### Other AI coding tools: copy the files

For Cursor, GitHub Copilot, OpenAI Codex CLI, and any other agent that reads skills from its own directory layout, see [INSTALL.md](INSTALL.md). It documents the per-tool target paths and includes a one-shot install script that lays everything down in one command.

## File map

```
adr-kit/
├── README.md                       # this file
├── LICENSE                         # MIT
├── INSTALL.md                      # per-tool install (manual route)
├── .claude-plugin/
│   └── plugin.json                 # Claude Code plugin manifest
├── skills/
│   ├── adr/
│   │   └── SKILL.md                # the comprehensive ADR guide
│   └── setup/
│       └── SKILL.md                # /adr-kit:setup: appends ADR rules to CLAUDE.md
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

## FAQ

**Where are ADRs stored?**

Under `docs/adr/` in your project, one file per decision, named `ADR-XXX-kebab-case-title.md`. The numbering is sequential and zero-padded to three digits. The skill assumes this layout but documents how to override it.

**How do I customize the conventions?**

Open `skills/adr/SKILL.md` (or the installed copy in your tool's skills directory) and edit the `## Project Conventions` section. The agent and the instructions read from that section, so a single edit propagates. Some teams prefer `adr-NNNN-` lowercase 4-digit; some prefer `0001-...` without a prefix; both work as long as you commit to one.

**What if my project already has ADRs in a different format?**

Two options. (1) Migrate the existing files to the adr-kit conventions in one pass; the most common change is the filename casing. (2) Override the conventions in `SKILL.md` to match what you have, so future ADRs use the same format. Migration is cleaner long-term; override is faster short-term.

**Does the skill auto-create ADRs without me asking?**

No. The main skill activates when you author or review ADRs, but it does not write files on its own. The `adr-generator` subagent writes a file only when you ask it to. The `setup` skill writes once to `CLAUDE.md` and only when you run `/adr-kit:setup`. All file mutations are user-triggered.

**Is this an Anthropic product?**

No. `adr-kit` is an independent open-source toolkit. It happens to install cleanest in Claude Code because Claude Code's plugin system is the most mature option for this kind of multi-file bundle, but the same files run in Cursor, Copilot, and Codex.

## Comparison

A plain ADR template gives you a markdown file with sections to fill in. `adr-kit` adds three things on top:

| Concern | Plain ADR template | adr-kit |
|---|---|---|
| Format | yes (one file) | yes (one file plus a generator agent) |
| Pre-flight discipline | absent | **anti-rationalization guards**: a 9-row excuse / counter-argument table that fires before you reach for "this is too obvious to document" |
| Acceptance bar | "fill it in" | **four named verification gates** (Completeness, Evidence, Clarity, Consistency) that must pass before Status flips from Proposed to Accepted |
| Code-review integration | absent | six named checks plus review-comment templates, ready to paste into a PR review |
| Tool integration | none | drop-in skill, agent, and instructions for Claude Code, Claude Cowork, Cursor, GitHub Copilot, OpenAI Codex CLI |
| Onboarding | "read this template" | one-time setup command that wires the rules into your project's `CLAUDE.md` |

The patterns themselves (anti-rationalization, verification gates) are not novel; both predate this toolkit. What `adr-kit` contributes is the **packaging**: a plug-and-play installation in any major AI coding tool, with the patterns wired into the place they need to be (the agent's pre-flight, the reviewer's checklist, the project's standing instructions).

If your team is happy with a plain template and the discipline lives in your culture, you do not need this toolkit. If you want the discipline to be enforceable by an AI agent reviewing a PR, this is what `adr-kit` does.

## Credits

Based on [Michael Nygard's ADR format](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions).

The two distinguishing patterns of this toolkit, **anti-rationalization guards** and **verification gates**, were first combined into a single ADR skill by [Jim van den Breemen's adr-skill](https://github.com/Jvdbreemen/adr-skill). That combination is what turns a plain ADR template into a discipline tool: the guards catch the excuses *before* a decision goes undocumented, and the gates catch the gaps *before* a `Proposed` ADR flips to `Accepted`. Without that pairing, this toolkit would be a thicker template, not a workflow.

The original sources of the two patterns:

- Anti-rationalization guards: [addyosmani/agent-skills](https://github.com/addyosmani/agent-skills).
- Verification gates: [trailofbits/skills](https://github.com/trailofbits/skills).

`adr-kit` builds on that combination by adding a focused creator agent (`adr-generator`), path-specific instructions for coding and review work, and per-tool install paths for Claude Code, Claude Cowork, Cursor, GitHub Copilot, and OpenAI Codex CLI.

## License

MIT. See [LICENSE](LICENSE).

## Contributing

Issues and PRs welcome. The skill is intentionally domain-agnostic; project-specific examples should stay in your own copy of `SKILL.md`, not upstream.
