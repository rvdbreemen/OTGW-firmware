# Contributing to adr-kit

Thanks for considering a contribution. `adr-kit` is intentionally small and project-agnostic; the bar for upstream changes is "this is generic and improves the toolkit for any user", not "this is what we needed for project X".

## Dev loop

Local plugin testing without publishing:

```bash
# In any test project, point Claude Code at your local clone:
claude --plugin-dir /path/to/your/adr-kit

# Then try the slash commands:
#   /adr-kit:setup
#   /adr-kit:adr
#   /agents     (should list adr-generator)
```

`--plugin-dir` makes edits in your clone visible immediately on `/reload-plugins`. No publish, no install.

## How to add a skill

1. Create `skills/<name>/SKILL.md` with frontmatter:
   - `name`: lowercase, hyphenated.
   - `description`: 1 to 2 sentences front-loading the trigger phrases.
   - Optional: `disable-model-invocation: true` if the skill mutates user files; the user must explicitly invoke it.
   - Optional: `allowed-tools` (e.g. `[Read, Write, Edit]`) to pre-approve tools so the user is not prompted on every use.
   - Optional: `argument-hint` to show expected slash-command syntax.
2. Body: clear, scannable instructions to the model. Sections, bullets, code examples.
3. Update `README.md` with a one-line mention of the new skill so users discover it.

## How to add an agent

1. Create `agents/<name>.md` with frontmatter:
   - `name`: lowercase, hyphenated.
   - `description`: when Claude should delegate to this agent (used for auto-delegation).
   - `tools`: whitelist. Restrict to the minimum the agent needs.
   - Optional: `model`, `effort` for cost or quality tuning.
2. Body: the agent's system prompt. Clear scope, refusal conditions, output format.
3. Document in `README.md` when to delegate to this agent vs running the task in the main session.

## Version bump and release

`adr-kit` follows [Semantic Versioning 2.0.0](https://semver.org/):

- **MAJOR**: breaking change in skill names, manifest schema, or default conventions.
- **MINOR**: new feature, backwards-compatible.
- **PATCH**: bug fix, doc-only change, no behavioural change.

Release steps:

1. Bump the `version` field in `.claude-plugin/plugin.json`.
2. Move the `## [Unreleased]` section in `CHANGELOG.md` to a new `## [X.Y.Z] - YYYY-MM-DD` section, and add a fresh empty `## [Unreleased]` at the top.
3. Commit the changes (`chore(release): vX.Y.Z (...)`).
4. Tag with the new convention: `git tag -a adr-kit--vX.Y.Z -m "..."`.
5. `git push` and `git push --tags`.
6. Create a GitHub Release on the new tag with notes summarised from the CHANGELOG.

## Code style

- **No em dashes.** Use colons, periods, commas, parentheses, semicolons. Project-wide style.
- **English** for all repo content (skill bodies, README, CHANGELOG, commit messages).
- **Kebab-case** for file names: `adr-generator.md`, not `ADRGenerator.md` or `adr_generator.md`.
- **Concise over comprehensive.** A short, accurate sentence beats a verbose paragraph.
- **Cite when adapting upstream patterns.** If you bring in a pattern from another project, credit them in `SKILL.md` and `README.md`.

## Validation

CI is enforced by `.github/workflows/validate.yml` on every push and pull request:

- `jq empty` on `plugin.json` (syntax check).
- Presence check on the required-files set.
- `plugin.json` version must match the top entry of `CHANGELOG.md`.
- `markdownlint` on skills, agents, instructions, and examples.

PRs that break CI will not be merged.

## Reporting issues

Use the issue templates if they fit. If not, a plain issue is fine. Include:

- Your AI coding tool and version (Claude Code, Cursor, Copilot, Codex CLI).
- The reproduction steps.
- What you expected to happen.
- Any relevant config (sanitised, no secrets).

## License

By contributing, you agree that your contributions are licensed under the MIT license, the same license as the project.
