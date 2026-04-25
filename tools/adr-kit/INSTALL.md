# Installing adr-kit

Pick the install path that matches your AI coding tool. The same skill, agent, and instructions work everywhere; only the directory layout differs.

The package is portable: drop the same content into the location your tool expects.

## Claude Code

> **Recommended**: install adr-kit as a Claude Code plugin instead of copying files. From your Claude Code session:
>
> ```
> /plugin marketplace add rvdbreemen/adr-kit
> /plugin install adr-kit@rvdbreemen-adr-kit
> /reload-plugins
> /adr-kit:setup
> ```
>
> The first three commands install the plugin (marketplace, install, reload). The fourth is a one-time per-project setup that appends an "ADR Kit Rules" section to your `CLAUDE.md` so future sessions know the skill is governing ADR work in this project. The setup command is idempotent: re-running it skips the append if the section is already present.
>
> The manual path below is the fallback if you prefer to vendor the files in your repo or work without the plugin marketplace.

Claude Code looks for skills in `.claude/skills/`, agents in `.claude/agents/`, and respects project-specific instructions referenced from `CLAUDE.md`.

```bash
# From your project root, with this repo cloned alongside as adr-kit/:
mkdir -p .claude/skills/adr .claude/agents .claude/instructions

cp adr-kit/skills/adr/SKILL.md                   .claude/skills/adr/SKILL.md
cp adr-kit/agents/adr-generator.md    .claude/agents/adr-generator.md
cp adr-kit/instructions/adr.coding.md .claude/instructions/adr.coding.md
cp adr-kit/instructions/adr.review.md .claude/instructions/adr.review.md
```

Then add a short pointer to your project's `CLAUDE.md` so the instructions are discoverable:

```markdown
## ADR rules

- Skill: `.claude/skills/adr/SKILL.md`
- Generator agent: `.claude/agents/adr-generator.md`
- Coding rules: `.claude/instructions/adr.coding.md`
- Review checks: `.claude/instructions/adr.review.md`

Before architecturally significant code changes, follow `adr.coding.md`.
When reviewing PRs, follow `adr.review.md`.
```

Claude Code subagents are invoked as `Agent({subagent_type: "adr-generator", ...})` once the file is in place.

## Claude Cowork

Claude Cowork (Anthropic's collaborative coding workspace, web-based) does not yet expose a public skills directory the way Claude Code does. Two install paths work today:

**Path 1: paste into the workspace's system prompt or instructions panel.**

1. Open the Cowork workspace settings.
2. Locate the "instructions" or "system prompt" field.
3. Paste the contents of `SKILL.md`, then append the contents of `instructions/adr.coding.md` and `instructions/adr.review.md`. The agent will treat all three as part of the workspace's standing instructions.
4. For ADR creation flows, paste the `agents/adr-generator.md` content into a separate prompt or saved snippet that you reach for when you want to scaffold an ADR.

**Path 2: commit the skill files into the project repo and let Cowork read them.**

```bash
mkdir -p .claude/skills/adr .claude/agents .claude/instructions

cp adr-kit/skills/adr/SKILL.md                   .claude/skills/adr/SKILL.md
cp adr-kit/agents/adr-generator.md    .claude/agents/adr-generator.md
cp adr-kit/instructions/adr.coding.md .claude/instructions/adr.coding.md
cp adr-kit/instructions/adr.review.md .claude/instructions/adr.review.md
```

Cowork shares the `.claude/` convention with Claude Code, so the same install layout works once committed to the repo Cowork is connected to. Add a short pointer in your project's `CLAUDE.md` (see the Claude Code section above).

## Cursor

Cursor reads project rules from `.cursor/rules/` and supports skills in `.cursor/skills/` (varies by version). The simplest portable layout:

```bash
mkdir -p .cursor/skills/adr .cursor/rules

cp adr-kit/skills/adr/SKILL.md                   .cursor/skills/adr/SKILL.md
cp adr-kit/agents/adr-generator.md    .cursor/skills/adr/AGENT.md
cp adr-kit/instructions/adr.coding.md .cursor/rules/adr-coding.mdc
cp adr-kit/instructions/adr.review.md .cursor/rules/adr-review.mdc
```

Then add an `applyTo` frontmatter block at the top of each `.mdc` file so Cursor scopes the rule correctly. Example for `adr-coding.mdc`:

```mdc
---
description: ADR rules during coding work
globs: ["**/*"]
alwaysApply: true
---
```

## GitHub Copilot

Copilot reads from `.github/skills/`, `.github/agents/`, and `.github/instructions/`.

```bash
mkdir -p .github/skills/adr .github/agents .github/instructions

cp adr-kit/skills/adr/SKILL.md                   .github/skills/adr/SKILL.md
cp adr-kit/agents/adr-generator.md    .github/agents/adr-generator.agent.md
cp adr-kit/instructions/adr.coding.md .github/instructions/adr.coding-agent.instructions.md
cp adr-kit/instructions/adr.review.md .github/instructions/adr.code-review.instructions.md
```

Adjust the frontmatter on each `.instructions.md` file with the correct `applyTo:` and `excludeAgent:` keys to match Copilot's path-specific instruction format. Example for `adr.coding-agent.instructions.md`:

```yaml
---
applyTo: "**"
excludeAgent: "code-review"
---
```

## OpenAI Codex CLI

The OpenAI Codex CLI (the modern reincarnation, not the deprecated 2021 model) reads from `~/.codex/` for user-level config and supports project-local `.codex/` if your version recognises it.

Project-local install:

```bash
mkdir -p .codex/skills/adr .codex/agents .codex/instructions

cp adr-kit/skills/adr/SKILL.md                   .codex/skills/adr/SKILL.md
cp adr-kit/agents/adr-generator.md    .codex/agents/adr-generator.md
cp adr-kit/instructions/adr.coding.md .codex/instructions/adr.coding.md
cp adr-kit/instructions/adr.review.md .codex/instructions/adr.review.md
```

If your Codex version does not yet load skills from `.codex/skills/`, fall back to the system-prompt approach: paste the contents of `SKILL.md` into your project's `AGENTS.md` (Codex's standing instructions file at the project root) under an "ADR rules" heading.

## Aider, Continue, and other AI coding tools

Most AI coding tools fall into one of two categories for skill loading:

1. **File-based**: they read instructions from a known directory (e.g. `.aider.conf`, `.continue/`, `.cline/`). Drop the four files (`SKILL.md`, `agents/adr-generator.md`, `instructions/adr.coding.md`, `instructions/adr.review.md`) into that directory, preserving the subfolder structure where the tool supports it.
2. **Prompt-based**: they accept a system prompt or "context file" you can edit. Paste the SKILL.md content into that prompt, with the two instructions files appended.

If neither applies, the fallback that always works is to commit `SKILL.md` to the repo root (or `docs/`) and tell the agent at the start of each session: "Read SKILL.md at the project root and follow it for any architectural decision."

## All tools at once

For projects that use multiple AI tools, drop the same content into all three locations. The skill content is identical; only the path is per-tool.

A small helper script (place at `scripts/install-adr-kit.sh` in your project):

```bash
#!/usr/bin/env bash
set -euo pipefail
SRC="${1:-./adr-kit}"

# Claude Code (and Claude Cowork, which shares the .claude/ convention)
mkdir -p .claude/skills/adr .claude/agents .claude/instructions
cp "$SRC/skills/adr/SKILL.md"                   .claude/skills/adr/SKILL.md
cp "$SRC/agents/adr-generator.md"    .claude/agents/adr-generator.md
cp "$SRC/instructions/adr.coding.md" .claude/instructions/adr.coding.md
cp "$SRC/instructions/adr.review.md" .claude/instructions/adr.review.md

# GitHub Copilot
mkdir -p .github/skills/adr .github/agents .github/instructions
cp "$SRC/skills/adr/SKILL.md"                   .github/skills/adr/SKILL.md
cp "$SRC/agents/adr-generator.md"    .github/agents/adr-generator.agent.md
cp "$SRC/instructions/adr.coding.md" .github/instructions/adr.coding-agent.instructions.md
cp "$SRC/instructions/adr.review.md" .github/instructions/adr.code-review.instructions.md

# Cursor
mkdir -p .cursor/skills/adr .cursor/rules
cp "$SRC/skills/adr/SKILL.md"                   .cursor/skills/adr/SKILL.md
cp "$SRC/agents/adr-generator.md"    .cursor/skills/adr/AGENT.md
cp "$SRC/instructions/adr.coding.md" .cursor/rules/adr-coding.mdc
cp "$SRC/instructions/adr.review.md" .cursor/rules/adr-review.mdc

# OpenAI Codex CLI (project-local)
mkdir -p .codex/skills/adr .codex/agents .codex/instructions
cp "$SRC/skills/adr/SKILL.md"                   .codex/skills/adr/SKILL.md
cp "$SRC/agents/adr-generator.md"    .codex/agents/adr-generator.md
cp "$SRC/instructions/adr.coding.md" .codex/instructions/adr.coding.md
cp "$SRC/instructions/adr.review.md" .codex/instructions/adr.review.md

echo "adr-kit installed for Claude Code/Cowork, GitHub Copilot, Cursor, and Codex."
```

## Verifying the install

After installing, ask your agent:

> Do you have access to the ADR skill? List the verification gates it defines.

A correct response names the four gates: **Completeness, Evidence, Clarity, Consistency**.

Then try a small smoke test:

> Use the ADR generator to scaffold an ADR titled "test ADR for install verification". Do not commit. Show me the file path and the first 20 lines.

The agent should write a file at `docs/adr/ADR-001-test-adr-for-install-verification.md` (or the next free number) with the standard sections populated.

## Updating

Pull updates from the upstream repo, then re-run the install script. The skill files are idempotent: re-installing overwrites the in-tree copies but does not affect your `docs/adr/` content.

## Customization

Fork the skill if your project has a different ADR convention (e.g. `adr-NNNN-` lowercase 4-digit). Edit the `## Project Conventions` section in `SKILL.md` and the equivalent block in `agents/adr-generator.md`. The rest of the toolkit references those conventions consistently.

## Uninstall

```bash
rm -rf .claude/skills/adr .claude/agents/adr-generator.md .claude/instructions/adr.*
rm -rf .github/skills/adr .github/agents/adr-generator.agent.md .github/instructions/adr.*
rm -rf .cursor/skills/adr .cursor/rules/adr-*.mdc
rm -rf .codex/skills/adr .codex/agents/adr-generator.md .codex/instructions/adr.*
```

Your existing `docs/adr/` content is preserved; uninstall only removes the toolkit, not your decision records.
