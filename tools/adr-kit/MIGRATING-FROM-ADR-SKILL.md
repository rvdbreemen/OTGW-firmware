# Migrating from `Jvdbreemen/adr-skill` to `rvdbreemen/adr-kit`

[`Jvdbreemen/adr-skill`](https://github.com/Jvdbreemen/adr-skill) by Jim van den Breemen was the direct inspiration for `adr-kit`. The two patterns that distinguish both toolkits, anti-rationalization guards and verification gates, were first combined in a single ADR skill by Jim's project. `adr-kit` builds on that combination by adding a focused creator agent, path-specific instructions, a setup command that wires `CLAUDE.md`, a lint skill that enforces the four gates against existing ADRs, and per-tool install paths for Claude Code, Claude Cowork, Cursor, GitHub Copilot, and OpenAI Codex CLI.

If you already use `adr-skill`, this guide explains your options. There is no urgency to switch.

## TL;DR

- **Keep using adr-skill** if you only need the skill (anti-rationalization + gates) and you do not want a creator agent, path-specific instructions, or a lint command. `adr-skill` is smaller, simpler, and the patterns it teaches are the same.
- **Switch to adr-kit** if you want one-command Claude Code install, a focused subagent that writes ADR files for you, lint of existing ADRs against the gates, and contributor-facing templates (issue forms, PR template, CONTRIBUTING).
- **Run both side by side** if you are evaluating: the slash commands are namespaced by plugin (`/adr-skill:...` vs `/adr-kit:...`), so they do not conflict.

## What is the same

Both toolkits share:

- The four named verification gates: Completeness, Evidence, Clarity, Consistency.
- The anti-rationalization pattern: a table of excuses for skipping ADR documentation, with counter-arguments.
- The Michael-Nygard-derived ADR template (Context, Decision, Alternatives Considered, Consequences, Related Decisions).
- The MIT license and the public-domain attribution chain (addyosmani/agent-skills, trailofbits/skills).

If you have written ADRs against `adr-skill` and they pass its review, they pass `adr-kit`'s gates without modification. The format is compatible.

## What is different

| Aspect | `Jvdbreemen/adr-skill` | `rvdbreemen/adr-kit` |
|---|---|---|
| Surface | One `SKILL.md` | Skill, agent, instructions, setup command, lint command, examples, CHANGELOG, CONTRIBUTING, SECURITY, COC |
| Install (Claude Code) | Manual file copy or `npx skills add` | Native plugin: `/plugin marketplace add` + `/plugin install` + `/reload-plugins` + `/adr-kit:setup` |
| Project setup | Document the rules in your own `CLAUDE.md` | `/adr-kit:setup` appends the rules block, idempotent |
| Creator agent | Not bundled | `adr-generator` subagent that authors a complete ADR file |
| Lint of existing ADRs | Not bundled | `/adr-kit:lint` runs the four gates against your existing `docs/adr/` tree |
| Code-review integration | Suggested in the skill body | Six named checks plus review-comment templates in `instructions/adr.review.md` |
| Tool reach | Claude Code primarily | Claude Code, Claude Cowork, Cursor, GitHub Copilot, OpenAI Codex CLI, with per-tool install paths in INSTALL.md |
| CI validation | None bundled | GitHub Actions workflow validates plugin.json + required-files + version-consistency + markdownlint |
| Contributor-facing | README only | README, INSTALL, CHANGELOG, CONTRIBUTING, SECURITY, CODE_OF_CONDUCT, ROADMAP, this migration guide, two issue templates, a PR template |

The trade is roughly: `adr-skill` is leaner and easier to keep in your head; `adr-kit` is bigger but does more out of the box.

## Migration path

### Path A: replace `adr-skill` with `adr-kit`

```bash
# 1. Remove adr-skill from your install location, e.g. for Claude Code:
rm -rf .claude/skills/adr-skill

# 2. Install adr-kit:
#    /plugin marketplace add rvdbreemen/adr-kit
#    /plugin install adr-kit@rvdbreemen-adr-kit
#    /reload-plugins
#    /adr-kit:setup

# 3. Validate your existing ADR set:
#    /adr-kit:lint
```

`/adr-kit:lint` reads every `ADR-*.md` in `docs/adr/` and reports per-gate pass/fail. If your existing ADRs were written under `adr-skill`'s discipline, the lint will pass them; if some were written before that discipline was in place, you will see what to fix.

The ADR files themselves do not need to be modified. The format is the same.

### Path B: keep both installed

Slash commands are namespaced by plugin prefix:

- `adr-skill`: `/adr-skill:adr` (or whatever its skill names are)
- `adr-kit`: `/adr-kit:adr`, `/adr-kit:setup`, `/adr-kit:lint`

They do not conflict. You can compare side by side and decide later. Be aware that two skills triggering on similar phrases ("create an ADR") may both auto-invoke; if that becomes annoying, set `disable-model-invocation: true` on whichever skill you do not want to auto-trigger and invoke it explicitly.

### Path C: stay on `adr-skill`

A perfectly valid choice. `adr-kit` is bigger; bigger is not always better. If your team's discipline is already strong, the lint command and the creator agent do not add much. The patterns themselves are what matters, and `adr-skill` ships them in the simplest possible form.

If you stay on `adr-skill` but want one specific piece of `adr-kit`, you can copy individual files (`agents/adr-generator.md`, `skills/lint/SKILL.md`) into your own setup. The MIT license permits this, and credit is appreciated but not required.

## Frequently asked questions

**Will the two toolkits diverge over time?**

Likely yes. Each is maintained independently. Patterns that prove valuable in one may or may not be ported to the other. The verification gates and the anti-rationalization table are the stable substrate; everything else is implementation.

**Whose attribution applies?**

Both. `adr-kit` credits Jim's `adr-skill` (in README and SKILL.md) as the source where the two patterns were first combined. `adr-skill` credits its own upstream sources (`addyosmani/agent-skills`, `trailofbits/skills`). When you write a derivative work, follow the same chain.

**Can I contribute the same idea to both?**

Yes. Open an issue or PR in both repos with the same proposal. The maintainers will likely respond independently.

## Maintainer notes

Robert (maintainer of `adr-kit`) and Jim (maintainer of `adr-skill`) are family. The two projects emerged from the same conversations and share the same ethos. If you have feedback that applies to both, mentioning that in your issue is helpful: it lets the maintainers coordinate informally.
