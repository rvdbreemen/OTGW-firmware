# adr-kit

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/tag/rvdbreemen/adr-kit?label=release&sort=semver)](https://github.com/rvdbreemen/adr-kit/releases)

A complete Architecture Decision Record (ADR) toolkit for AI coding agents. Drop into any project to give Claude Code, Claude Cowork, Cursor, GitHub Copilot, OpenAI Codex CLI, or any agent that supports the [Agent Skills](https://agentskills.io/) format a shared, enforceable ADR workflow.

> **Pre-1.0**: the toolkit is functional and in use, but the API and conventions may change before v1.0.0. Pin to a specific tag if you need stability across upgrades. Latest release: see the badge above.

## What it does

- **Skill** (`SKILL.md`): the comprehensive ADR guide. When to write one, what to write, how to verify it, how to supersede it. Includes anti-rationalization guards (the excuses agents use to skip documentation) and four named verification gates (Completeness, Evidence, Clarity, Consistency).
- **Agent** (`agents/adr-generator.md`): a focused subagent for *creating* a new ADR. Hand it a decision and it produces a fully populated `docs/adr/ADR-XXX-title.md` file.
- **Lint skill** (`/adr-kit:lint`, since v0.7.0): checks existing ADRs against the four gates with file:line citations. Supports scoped policy via `.adr-kit.json` since v0.9.0.
- **Lint CLI** (`bin/adr-lint`, since v0.10.0): deterministic Python CLI for CI / pre-commit / batch validation. Same gate logic as the skill, exit-code-based.
- **Migrate skill** (`/adr-kit:migrate`, since v0.11.0): guided rewrite of legacy-shaped ADRs into the canonical-seven-section template. Read-then-confirm workflow. No fabrication; uses TODO placeholders for genuine content gaps.
- **Instructions** (`instructions/`): two path-specific instruction files, one for coding work (`adr.coding.md`) and one for code review (`adr.review.md`). They tell the agent which ADR rules apply and when.
- **Template** (`examples/ADR-template.md`): a clean ADR file you can copy.

The pieces work together: the skill is the reference, the agent does the writing, the lint flags deviations, the migrate command brings legacy work into shape, the instructions tell the agent what to do at the right moment.

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

Optional fifth command: `/adr-kit:lint` reads every `ADR-*.md` in your `docs/adr/` directory and reports per-file, per-gate pass/fail with line-level citations for failures. Useful right after install to see how your existing ADRs measure up against the four gates, and useful before merging a PR that touches an ADR. Read-only.

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
│   ├── setup/
│   │   └── SKILL.md                # /adr-kit:setup: appends ADR rules to CLAUDE.md
│   └── lint/
│       └── SKILL.md                # /adr-kit:lint: validates ADRs against the four gates
├── agents/
│   └── adr-generator.md            # subagent: create a new ADR
├── instructions/
│   ├── adr.coding.md               # ADR rules during coding
│   └── adr.review.md               # ADR checks during PR review
└── examples/
    └── ADR-template.md             # clean template to copy
```

## Slash commands reference

After `/plugin install adr-kit@rvdbreemen-adr-kit` + `/reload-plugins`, your Claude Code session has four slash commands. Two of the five names below (`/adr` and `/adr-kit:adr`) invoke the same skill; the other three are independent.

| Command | Type | Auto-invocable | When to use |
|---|---|---|---|
| `/adr [title]` | knowledge / guide | yes | Author or review an ADR. Loads the comprehensive ADR guide (anti-rationalization guards, four verification gates, supersession workflow). Same skill as `/adr-kit:adr`. |
| `/adr-kit:adr [title]` | knowledge / guide | yes | Identical to `/adr`. The prefix form is canonical; the root form is a shortcut Claude Code exposes when a skill allows model-invocation. Use whichever fits your typing. |
| `/adr-kit:setup` | one-time write | no | Run once per project after install. Appends an "ADR Kit Rules" section to your project's `CLAUDE.md` so future sessions know about the skill, the agent, and the path-specific instructions. Idempotent: re-running reports "Already set up" rather than duplicating. |
| `/adr-kit:lint [path]` | deliberate check | no | Validate existing ADRs against the four gates with file:line citations. Reads `docs/adr/.adr-kit.json` if present. Default target is `docs/adr/`; pass a directory or file as argument to scope. Read-only. Three result tiers: PASS, ADVISORY (informational), FAIL (action required). |
| `/adr-kit:migrate [path]` | guided rewrite | no | Bring a legacy-shaped ADR into the canonical-seven-section template. Read-then-confirm: prints a per-file plan first, applies after explicit yes. Six named patterns (Status promotion, Alternatives lift, Related-to-Related-Decisions split, TODO placeholders for genuine content gaps). Default target is `docs/adr/`. |

### Auto-invocable vs user-only

- **Auto-invocable** (`/adr`, `/adr-kit:adr`): Claude can also load this skill in the background when context calls for it (e.g. you ask "should I document this decision?"). The skill body activates without you typing the slash command. Knowledge / reference skills sit here.
- **User-only** (`/adr-kit:setup`, `/adr-kit:lint`, `/adr-kit:migrate`): only fires when you explicitly type the slash command. Set via `disable-model-invocation: true` in the skill frontmatter. Write actions and deliberate checks sit here so Claude does not surprise you by triggering them.

This is a deliberate design pattern. Knowledge skills should be cheap to auto-trigger; write-and-check skills should be costly enough that you have to ask.

### Companion CLI: `bin/adr-lint`

`/adr-kit:lint` runs in your Claude Code session. For unattended use (CI / pre-commit / batch validation) the toolkit ships a deterministic Python CLI at `bin/adr-lint`. Same gate logic, exit-code-based, runs anywhere with Python 3.8+. See the [CI integration section](#ci-integration-binadr-lint-since-v0100) below for a copy-paste GitHub Actions snippet.

The CLI defaults to the deterministic gates (Completeness, Consistency); the heuristic gates (Evidence, Clarity) are opt-in via `--gates` because they need judgement that a regex cannot reliably provide. That judgement is where the slash-command form remains canonical.

## Quickstart

Once installed in your project:

1. **First time setup**: run `/adr-kit:setup` to wire the rules into your project's `CLAUDE.md`.
2. **First-time analysis**: ask your agent to "analyze this codebase for undocumented architectural decisions". Use the workflow in `SKILL.md` (Initial Codebase Analysis section) to retroactively document existing patterns at `Status: Accepted`.
3. **For new work**: when about to make an architecturally significant change, the coding instructions (`adr.coding.md`) point at the agent. The agent (`agents/adr-generator.md`) writes the ADR. The verification gates from the skill validate it. You can also invoke `/adr <short title>` directly.
4. **In code review**: the review instructions (`adr.review.md`) walk through six named checks. The reviewer cites a check by name when blocking a PR.
5. **Audit existing ADRs**: `/adr-kit:lint` runs the four verification gates over every ADR in `docs/adr/` and reports per-file, per-gate pass/fail. Useful right after install and before merging ADR-touching PRs.
6. **Bring legacy ADRs into shape**: `/adr-kit:migrate` rewrites legacy-shaped ADRs into the canonical-seven-section template, read-then-confirm. Pair with `/adr-kit:lint` afterwards to verify the result.

## ADR conventions

The toolkit defaults to:

- **Filename**: `ADR-XXX-kebab-case-title.md` with uppercase prefix and 3-digit zero-padded number, stored in `docs/adr/`.
- **Heading**: `# ADR-XXX Title`.
- **Sections** in order: Status, Context, Decision, Alternatives Considered, Consequences, Related Decisions, References.
- **Status values**: `Proposed`, `Accepted`, `Deprecated`, `Superseded by ADR-YYY`.
- **Date format**: `YYYY-MM-DD`.

You can change the convention if your project already has a different one (some teams use `adr-NNNN-` lowercase 4-digit, or `0001-` with no prefix). Edit the `## Project Conventions` section in the skill and the agent definition; the rest of the toolkit follows from there.

## Configuration (since v0.9.0)

Established projects often have a long history of ADRs that predate the four canonical gates. Linting them under strict rules produces noise rather than actionable feedback. Two opt-in mechanisms let a project apply the gates surgically: strict on new ADRs, advisory on legacy ones.

### Project-level config: `docs/adr/.adr-kit.json`

Drop this file at `docs/adr/.adr-kit.json` to set the project's lint policy. Skipped if absent (defaults: everything strict, exact match to v0.7.x output).

```json
{
  "strict_from": "ADR-042",
  "ignore": ["ADR-001", "ADR-007"],
  "severity": {
    "completeness": "advisory_before_strict_from",
    "evidence": "advisory_before_strict_from",
    "clarity": "always_advisory",
    "consistency": "always_strict"
  },
  "template": {
    "required_sections": ["## Status", "## Context", "## Decision", "## Consequences"]
  }
}
```

- `strict_from` is the first ADR id (inclusive) on which the gates are enforced strictly. ADRs with a lower number are linted in advisory mode.
- `ignore` lists ADR ids (or filenames) to skip entirely.
- `severity` overrides the per-gate behaviour. Legal values: `always_strict`, `always_advisory`, `advisory_before_strict_from`. Consistency stays strict by default because filename / heading mismatches and duplicate numbers are real bugs regardless of when the ADR was written.
- `template.required_sections` overrides the canonical seven sections with your project's actual template.

A fully annotated copy lives at [`examples/.adr-kit.sample.json`](examples/.adr-kit.sample.json).

### Per-ADR markers

For one-off grandfathering without a project-wide config, drop one of these HTML comments anywhere in an ADR file:

```html
<!-- adr-kit-lint: skip -->
<!-- adr-kit-lint: skip completeness, evidence -->
<!-- adr-kit-lint: advisory -->
```

`skip` (no args) skips the file entirely; `skip <gate>[, <gate>...]` skips specific gates; `advisory` runs all gates in advisory mode on this file. A worked example lives at [`examples/ADR-sample-003-grandfathered-legacy.md`](examples/ADR-sample-003-grandfathered-legacy.md).

### Result tiers

`/adr-kit:lint` now reports three tiers: PASS, ADVISORY (a finding that does not block but is reported), and FAIL. The aggregate's "next step" line always points at a FAIL, never an ADVISORY: ADVISORY is informational, FAIL is what you act on.

## CI integration: `bin/adr-lint` (since v0.10.0)

The `/adr-kit:lint` skill is for human-in-the-loop review (judgement-based gates rely on Claude). For CI / pre-commit / batch validation, v0.10.0 ships a deterministic Python CLI at `bin/adr-lint`. It mirrors the deterministic gates of the skill (Completeness and Consistency by default; Evidence and Clarity available behind `--gates`), reads the same `.adr-kit.json` policy, and exits with a status code that makes blocking a PR trivial.

### Quick start

```bash
# Lint your project's ADRs (default: docs/adr/, gates: completeness,consistency)
python bin/adr-lint

# Limit to one gate, JSON output for tooling
python bin/adr-lint --gates completeness --format json

# Override the strict_from boundary on the command line
python bin/adr-lint --strict-from ADR-100

# Lint a different directory or a single file
python bin/adr-lint docs/decisions/
python bin/adr-lint docs/adr/ADR-042-foo.md
```

Exit codes: `0` = no FAIL (PASS / ADVISORY counts may be non-zero), `1` = at least one FAIL, `2` = config or input error.

### Drop-in GitHub Actions snippet

Add this job to your `.github/workflows/<ci>.yml` to block PRs that introduce a FAIL:

```yaml
adr-lint:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-python@v5
      with: { python-version: '3.11' }
    - name: Fetch adr-lint
      run: |
        curl -fsSL -o /tmp/adr-lint \
          https://raw.githubusercontent.com/rvdbreemen/adr-kit/main/bin/adr-lint
        chmod +x /tmp/adr-lint
    - name: Lint ADRs
      run: python /tmp/adr-lint docs/adr/
```

The script is stdlib-only, so no `pip install` is needed in CI. `jsonschema` is auto-detected if installed and used for deeper config validation; absence is non-fatal.

### Help text

```
$ adr-lint --help
usage: adr-lint [-h] [--strict-from ADR-NNN] [--gates GATES]
                [--format {human,json}] [--config PATH] [-v] [--version]
                [path]

Deterministic CLI for the four adr-kit verification gates.

positional arguments:
  path                  File or directory to lint (default: docs/adr/)

options:
  --strict-from ADR-NNN
                        First ADR id (inclusive) on which gates are strict; overrides config.
  --gates GATES         Comma-separated gates to run. Default:
                        completeness,consistency. All:
                        completeness,evidence,clarity,consistency
  --format {human,json}
                        Output format (default: human)
  --config PATH         Override .adr-kit.json location.
  -v, --verbose         Show ADVISORY and SKIPPED details too
```

### When to use which

- `/adr-kit:lint` (skill, in Claude Code): nuanced review, all four gates, judgement on Evidence and Clarity.
- `bin/adr-lint` (CLI, in CI): deterministic gates only by default, exit-code based, runs unattended. Use as a PR merge gate.

The two are designed to agree on Completeness and Consistency. They can disagree on Evidence and Clarity by design: Claude's judgement is structurally better at those.

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

## Project resources

- [ROADMAP.md](ROADMAP.md): direction, v1.0.0 criteria, deliberate non-goals.
- [MIGRATING-FROM-ADR-SKILL.md](MIGRATING-FROM-ADR-SKILL.md): switching from or co-installing alongside Jim van den Breemen's adr-skill.
- [CHANGELOG.md](CHANGELOG.md): release history in Keep a Changelog format.
- [CONTRIBUTING.md](CONTRIBUTING.md): dev loop, add-a-skill, version-bump, release procedure, code style.
- [SECURITY.md](SECURITY.md): security disclosure policy.
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md): community standards (Contributor Covenant 2.1 by reference).

## License

MIT. See [LICENSE](LICENSE).

## Contributing

Issues and PRs welcome. The skill is intentionally domain-agnostic; project-specific examples should stay in your own copy of `SKILL.md`, not upstream.
