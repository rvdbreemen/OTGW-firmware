---
name: setup
description: One-time project setup for adr-kit. Appends an "ADR Kit Rules" section to the project's CLAUDE.md (creating the file if it does not exist) so future sessions know about the ADR skill, agent, and instructions. Idempotent: skips the append if the section is already present.
disable-model-invocation: true
---

# adr-kit setup

You are running the one-time project setup for the adr-kit plugin. Your job is to append a small "ADR Kit Rules" section to the project's `CLAUDE.md` so future Claude Code sessions in this project know about the ADR skill, the `adr-generator` subagent, and the path-specific coding and review instructions.

## Steps

1. **Locate the project's CLAUDE.md.** It lives at the repository root. The user is expected to run `/adr-kit:setup` from the project root. Check whether `CLAUDE.md` exists in the current working directory.

2. **Idempotency check.** If CLAUDE.md exists, read it. If it already contains a heading exactly named `## ADR Kit Rules` (case-sensitive, exact heading text), do nothing. Tell the user the setup is already done and exit. Do not duplicate the section.

3. **Append or create.** If CLAUDE.md exists but lacks the section, append the snippet below to the end of the file, with one blank line separating it from whatever was the last content. If CLAUDE.md does not exist, create it containing only the snippet (no other scaffolding).

4. **Confirm.** Reply with one short line that names the action taken (`created`, `appended`, or `skipped`), the path to the file, and the line number of the new section's heading.

## Snippet to append

```markdown
## ADR Kit Rules

This project uses [adr-kit](https://github.com/rvdbreemen/adr-kit) for Architecture Decision Records. The skill, the `adr-generator` subagent, and the path-specific instructions are loaded via the plugin.

- Use `/adr-kit:adr` (or invoke the `adr-generator` subagent) when authoring a new ADR.
- During coding work, follow the rules in the plugin's `instructions/adr.coding.md`.
- During code review, apply the six named ADR checks in the plugin's `instructions/adr.review.md`.
- An ADR cannot flip from `Proposed` to `Accepted` until it passes all four verification gates: Completeness, Evidence, Clarity, Consistency.

ADR files live at `docs/adr/ADR-XXX-title.md`. Status flow: `Proposed` to `Accepted`, then immutable. To reverse a decision, write a superseding ADR rather than editing the original.
```

## Constraints

- **Idempotent.** The only state mutation is the append. Re-running `/adr-kit:setup` must not duplicate the section. Always do the heading-presence check first.
- **Read before write.** Always read CLAUDE.md (when it exists) before deciding whether to use Edit or Write. Never overwrite existing content.
- **Preserve existing content.** Do not modify or remove anything else in CLAUDE.md. Append only, or create from scratch.
- **No em dashes.** Per adr-kit's own style. The snippet uses colons, periods, parentheses, and commas only.
- **English.** The snippet is in English regardless of the rest of the user's CLAUDE.md.

## Reporting format

Reply with exactly one of these forms:

- `Setup complete. Appended "ADR Kit Rules" section to CLAUDE.md at line <N>.`
- `Setup complete. Created CLAUDE.md with the "ADR Kit Rules" section.`
- `Already set up. CLAUDE.md already contains an "ADR Kit Rules" heading at line <N>. No changes made.`

## When the user is in the wrong directory

If you cannot find a sensible project root (no `CLAUDE.md`, no `.git/`, no `package.json` or other manifest), stop and ask: "I do not see a project root here (no CLAUDE.md, no .git, no package manifest). Are you in your project directory? If yes, I can create CLAUDE.md from scratch. Confirm to proceed."

Do not silently create files in unexpected locations.
