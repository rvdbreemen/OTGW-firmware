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

- `jq empty` on `plugin.json` and `marketplace.json` (syntax check).
- Schema validation (ajv-cli, draft-07): both manifests are validated against `schemas/plugin.json.schema.json` and `schemas/marketplace.json.schema.json` respectively. The schemas reject the field-type bugs that surfaced post-install in v0.7.1 and v0.7.2 (missing marketplace manifest; `repository` declared as object instead of string).
- Presence check on the required-files set.
- `plugin.json` version must match the top entry of `CHANGELOG.md`.
- `markdownlint` on skills, agents, instructions, and examples.

PRs that break CI will not be merged.

## Pre-release smoke test

Schema validation catches manifest field-type bugs but does not exercise the actual install path. Before tagging a new release, run this manual checklist in a fresh Claude Code session against your local clone. The whole sequence takes about 3 minutes.

1. **Start Claude Code with the local clone as plugin source:**

   ```bash
   claude --plugin-dir /path/to/your/adr-kit
   ```

2. **List installed plugins.** Type `/plugin` in the Claude Code prompt. Confirm `adr-kit` appears in the Installed tab. Confirm there are no red error messages or `Validation errors:` lines next to the plugin entry. (This catches manifest schema mismatches that ajv missed and that would otherwise only surface for end users.)

3. **List slash commands.** Type `/help`. Confirm the following commands are registered:
   - `/adr-kit:adr`
   - `/adr-kit:setup`
   - `/adr-kit:lint`

   Missing commands typically mean a skill file is in the wrong directory or has malformed frontmatter.

4. **Run setup against a scratch project.** In a temporary directory (or any project that does not already have an `ADR Kit Rules` section in its `CLAUDE.md`), run:

   ```
   /adr-kit:setup
   ```

   Confirm the response is one of: `Setup complete. Appended ...`, `Setup complete. Created CLAUDE.md ...`, or `Already set up. ... at line N. No changes made.` Inspect `CLAUDE.md` to verify the section was appended literally and not duplicated. Re-run `/adr-kit:setup` and confirm it reports `Already set up.` (idempotency check).

5. **Run lint against a sample ADR.** Create a directory with at least one `ADR-001-foo.md` file, then run:

   ```
   /adr-kit:lint
   ```

   Confirm the output reports per-gate PASS / FAIL for each ADR found. The exact result is not what matters; what matters is that the skill executed and produced gate-by-gate output.

6. **If any of steps 2-5 fails, do not tag the release.** Investigate the failure first. Schema-only fixes (CI green, install red) are exactly the regressions this checklist exists to catch.

When the checklist passes, proceed with the release steps in the next section.

## Reporting issues

Use the issue templates if they fit. If not, a plain issue is fine. Include:

- Your AI coding tool and version (Claude Code, Cursor, Copilot, Codex CLI).
- The reproduction steps.
- What you expected to happen.
- Any relevant config (sanitised, no secrets).

## License

By contributing, you agree that your contributions are licensed under the MIT license, the same license as the project.
