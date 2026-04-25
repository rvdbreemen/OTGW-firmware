# Changelog

All notable changes to `adr-kit` are documented in this file. The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.11.0] - 2026-04-25

### Added

- **`/adr-kit:migrate`** (`skills/migrate/SKILL.md`): guided rewrite skill that brings legacy-shaped ADRs into the canonical-seven-section template enforced by `/adr-kit:lint`. User-only invocable (`disable-model-invocation: true`); never silent. Six named transformation patterns:
  - **Pattern A**: inline `**Status:** ...` / `**Date:** ...` / `**Supersedes:** ...` lines folded into a top-level `## Status` heading.
  - **Pattern B**: `### Alternatives considered` nested inside Context promoted to top-level `## Alternatives Considered` between Decision and Consequences.
  - **Pattern C**: `### Alternatives considered and rejected` nested inside Consequences promoted to top-level before Consequences.
  - **Pattern D**: `## Related` renamed to `## Related Decisions`, with external file paths / URLs / PR references split off into a new `## References` section.
  - **Pattern E**: missing `## References` section with no source content gets a `<!-- TODO: populate -->` placeholder. Never fabricates.
  - **Pattern F**: missing `## Alternatives Considered` with no source discussion gets a `<!-- TODO: document at least 2 alternatives -->` placeholder. Never fabricates.
- README "What it does" section gains entries for `/adr-kit:lint`, `bin/adr-lint`, and `/adr-kit:migrate`. The single-paragraph "the pieces work together" closing now mentions all four roles (skill, agent, lint, migrate, instructions).
- `.github/workflows/validate.yml` required-files set extended with `skills/migrate/SKILL.md`.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.11.0.
- `.claude-plugin/marketplace.json` plugin entry version bumped to 0.11.0.

### Notes

- The skill is **read-then-confirm**: it always prints a per-file plan first ("Pattern A on ADR-007: promote inline Status to heading") and asks for explicit user confirmation before writing. No silent edits.
- The skill is **idempotent**: running migrate on an already-canonical ADR is a no-op.
- The skill **respects markers**: files with `<!-- adr-kit-lint: skip -->` are left untouched. Files with `<!-- adr-kit-lint: advisory -->` get a warning before migration (the marker becomes meaningless once the file is canonical-shaped).
- The skill **respects `template.required_sections`** in `.adr-kit.json`. If a project has codified a different template, migrate targets that.
- Out of scope on purpose: filename renaming (Consistency-FAILs), body-prose rewriting, auto-fabricating Alternatives or References content, deterministic Python CLI variant. Migration is judgement-heavy; same reasoning that put Evidence and Clarity gates as opt-in for `bin/adr-lint`.

## [0.10.1] - 2026-04-25

### Fixed

- `skills/lint/SKILL.md`: added `disable-model-invocation: true` to the frontmatter so the lint skill follows the same user-only invocation discipline as `skills/setup/SKILL.md`. Before this fix, `/lint` (without plugin prefix) appeared in Claude Code's autocomplete at the root namespace, and the skill was auto-invocable by Claude. With the fix, only the canonical `/adr-kit:lint` form is registered, matching `/adr-kit:setup` and giving the plugin a uniform invocation pattern. Lint is a deliberate user action (a checking tool, not a background helper), so disabling auto-invocation is the right discipline.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.10.1.
- `.claude-plugin/marketplace.json` plugin entry version bumped to 0.10.1.

### Notes

- No skill body change. The lint logic (severity model, gate evaluation, output format) is identical to v0.10.0.
- No `bin/adr-lint` change. The deterministic CLI is independent of the skill's invocation discipline.

## [0.10.0] - 2026-04-25

### Added

- **`bin/adr-lint`**: standalone Python 3.8+ CLI for CI / pre-commit integration. Mirrors the deterministic gates of `/adr-kit:lint` (Completeness, Consistency by default; Evidence and Clarity available behind `--gates`). Reads the same `.adr-kit.json` policy, supports per-ADR markers, and produces both human-readable and JSON output. Exit codes: `0` (no FAIL), `1` (FAIL detected), `2` (config or input error). Stdlib-only; `jsonschema` auto-detected for deeper config validation.
- `schemas/adr-kit-config.schema.json`: JSON Schema (draft-07) for `docs/adr/.adr-kit.json`. Pattern-validates `strict_from` (`^ADR-\d{3}$`), enum-validates `severity` values, validates `template.required_sections` heading shape. Used by `bin/adr-lint` when `jsonschema` is installed; falls back to basic checks otherwise.
- `tests/`: pytest suite with 15 tests covering every FAIL pattern and severity combination. Subprocess-based: each test runs `adr-lint --format json` and asserts on the JSON output, so the public interface is exercised, not internal helpers. Fixtures: `canonical/`, `missing-headings/`, `bad-filename/`, `heading-mismatch/`, `marker-skip/`, `marker-advisory/`, `marker-skip-gate/`, `with-policy/` (strict_from boundary), `bad-config/`.
- `.github/workflows/adr-lint-self.yml`: dual job that runs `pytest` and a smoke test against `examples/`. Runs on push and pull request to `main`.
- `README.md` "CI integration" section between "Configuration" and "FAQ", with a copy-paste-ready GitHub Actions snippet that downstream users can drop into their own workflow to block PRs on FAIL.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.10.0.
- `.claude-plugin/marketplace.json` plugin entry version bumped to 0.10.0.

### Notes

- The CLI does not replace the skill. The skill stays canonical for nuanced review; the CLI is for unattended CI gates. They are designed to agree on Completeness and Consistency. They can legitimately disagree on Evidence and Clarity, where Claude's judgement is structurally better than a regex.
- Smoke-tested locally against a representative 87-ADR real-world project (the same set used in the v0.9.0 smoke test): produces 7 PASS / 80 ADVISORY / 0 FAIL with exit code 0 and parseable JSON. Matches the skill's output exactly.
- Future work: `/adr-kit:migrate` (interactive helper to rewrite legacy ADRs into the canonical template) is still planned for a later release; v0.10.0 is scoped to the CLI alone.

## [0.9.0] - 2026-04-25

### Added

- **Scoped lint with grandfathering.** `/adr-kit:lint` now reads an optional project-level config file at `docs/adr/.adr-kit.json` and per-file HTML-comment markers inside individual ADRs. The two mechanisms together let a project apply the four gates surgically: strict on new ADRs, advisory on legacy ones, ignored on archived ones.
- `skills/lint/SKILL.md`:
  - New "Configuration" section documenting the `.adr-kit.json` schema (`strict_from`, `ignore`, `severity` per-gate overrides, `template.required_sections` override).
  - New "Per-ADR markers" subsection: `<!-- adr-kit-lint: skip -->` / `skip <gates>` / `advisory` tell the linter how to treat a single ADR without a project-wide config.
  - New "Severity decision tree" (Graphviz block) that documents the precedence rules: ignore beats markers, markers beat config, and within config the precedence is `always_strict` > `always_advisory` > `advisory_before_strict_from`.
  - Output format gains an ADVISORY tier between PASS and FAIL. Single-file output reports each finding with the reason it was downgraded (e.g. "ADVISORY: ADR predates strict_from=ADR-042"). Directory-tree output groups files into PASS strictly / ADVISORY only / FAIL / SKIPPED counts.
  - Reporting section: the bottom-line "next step" sentence now always points at a FAIL, never an ADVISORY. ADVISORY is informational; FAIL is what the user is asked to act on.
  - Completeness gate now respects `template.required_sections` when set in the config; otherwise the canonical seven still apply.
- `examples/.adr-kit.sample.json`: fully annotated example config with `_comment` keys explaining each field.
- `examples/ADR-sample-003-grandfathered-legacy.md`: a worked legacy-template ADR using the `<!-- adr-kit-lint: advisory -->` marker, demonstrating how a pre-canonical ADR coexists with strict gating on newer ADRs.
- `README.md` gains a "Configuration" section between "ADR conventions" and "FAQ" covering both mechanisms with copy-paste-ready snippets.
- `.github/workflows/validate.yml` required-files set extended with the two new example files.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.9.0.
- `.claude-plugin/marketplace.json` plugin entry version bumped to 0.9.0.

### Notes

- Backwards compatible: when no `.adr-kit.json` and no per-ADR markers are present, behaviour is identical to v0.8.0 (everything strict, FAIL on any gate failure).
- Defaults are safe: Consistency stays `always_strict` by default even when `strict_from` is set, because filename / heading mismatches and duplicate numbers are real bugs regardless of when the ADR was written.
- Future work: `/adr-kit:migrate` (v0.10.0) will help projects mass-rewrite legacy ADRs into the canonical template; `severity_profile` presets (v0.11.0) will offer named bundles instead of per-gate configuration.

## [0.8.0] - 2026-04-25

### Added

- `schemas/plugin.json.schema.json`: hand-curated JSON Schema (draft-07) for `.claude-plugin/plugin.json`. Validates the documented field types and rejects the historical bug pattern that broke install in v0.7.2 (`repository` as object). The schema's top-level `description` field documents which historical bugs each constraint prevents.
- `schemas/marketplace.json.schema.json`: JSON Schema (draft-07) for `.claude-plugin/marketplace.json`. Required fields: `name`, `description`, `owner` (object with `name`), `plugins` (non-empty array). Each plugin entry requires `name`, `source`, `version`, `description`. The schema's top-level description references the v0.7.1 missing-manifest incident.
- `.github/workflows/validate.yml`: two new CI steps that run `ajv-cli` (draft-07, with `ajv-formats`) against both manifests on every push and pull request. The workflow now fails the build on schema violations, not just JSON syntax errors. `marketplace.json` was added to the required-files set; `schemas/plugin.json.schema.json` and `schemas/marketplace.json.schema.json` are now also required (so a future contributor cannot accidentally remove the schemas without CI noticing). `skills/lint/SKILL.md` was also added to the required-files list (was missing since v0.7.0).
- `CONTRIBUTING.md` "Pre-release smoke test" section: a 5-step manual checklist that release authors run in a fresh Claude Code session before tagging. Steps cover `claude --plugin-dir`, `/plugin`, `/help`, `/adr-kit:setup` (idempotency check), and `/adr-kit:lint`. Schema validation catches manifest field-type bugs; the smoke test catches the install-path bugs schema validation cannot reach.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.8.0.
- `.claude-plugin/marketplace.json` plugin entry version bumped to 0.8.0 (also corrects the v0.7.1/v0.7.2 drift where the marketplace manifest still listed 0.7.1 while the plugin manifest had advanced to 0.7.2).
- `CONTRIBUTING.md` "Validation" section now mentions the new schema validation step alongside the existing `jq empty` and required-files checks.

### Notes

- This release closes the post-mortem from the v0.7.1 and v0.7.2 install-side regressions. The schemas are sized to current manifest fields only; they are not a substitute for the official Claude Code plugin manifest spec, which (when published) will replace this hand-curated pair. The schemas are best-effort regression tests for the bugs we have actually shipped.

## [0.7.2] - 2026-04-25

### Fixed

- **`plugin.json` `repository` field rejected by Claude Code plugin manifest schema.** Versions v0.1.0 through v0.7.1 declared `repository` as an object (`{ "type": "git", "url": "..." }`), borrowing the convention from npm's `package.json`. Claude Code's plugin manifest schema instead expects a plain URL string for `repository`. Result: after fixing the marketplace.json issue in v0.7.1, `/plugin install adr-kit@rvdbreemen-adr-kit` failed with `Validation errors: repository: Invalid input: expected string, received object`.
- Changed `repository` to a plain URL string: `"https://github.com/rvdbreemen/adr-kit.git"`.
- The CI workflow added in v0.5.0 catches JSON syntax errors via `jq empty` but does not validate the manifest schema. A follow-up task (post-v1.0.0) will add schema validation against the official Claude Code plugin manifest spec.

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.7.2.

## [0.7.1] - 2026-04-25

### Fixed

- **Plugin install via marketplace was incomplete in v0.1.0 through v0.7.0.** The repo had `.claude-plugin/plugin.json` (the per-plugin manifest) but lacked `.claude-plugin/marketplace.json` (the marketplace manifest). Without the marketplace manifest, `/plugin marketplace add rvdbreemen/adr-kit` could not register the marketplace under the `rvdbreemen-adr-kit` name, and the subsequent `/plugin install adr-kit@rvdbreemen-adr-kit` failed with "Unknown command" errors when users tried `/adr-kit:setup` afterwards.
- Adds `.claude-plugin/marketplace.json` declaring `name: "rvdbreemen-adr-kit"`, owner metadata, and one plugin entry (`adr-kit`, source `./`, version 0.7.1).
- After upgrading: existing installations should remove and re-add the marketplace, then re-install:
  ```
  /plugin marketplace remove rvdbreemen-adr-kit
  /plugin marketplace add rvdbreemen/adr-kit
  /plugin install adr-kit@rvdbreemen-adr-kit
  /reload-plugins
  /adr-kit:setup
  ```

### Changed

- `.claude-plugin/plugin.json` version bumped to 0.7.1.

## [0.7.0] - 2026-04-25

### Added

- `skills/lint/SKILL.md`: new `/adr-kit:lint` slash command. Reads every `ADR-*.md` in `docs/adr/` (or a single file or directory passed as argument) and reports per-file, per-gate pass/fail against the four verification gates with file:line citations for failures. Read-only (`allowed-tools: [Read, Glob, Grep]`). Lifts the gates from "documented" to "programmatically enforced" against existing ADRs.
- `ROADMAP.md`: documents Status, criteria for v1.0.0 (90 days field time, 5 unrelated installs, gate-based PR block in real review, migration guide), Planned features (signals not commitments), Out-of-scope non-goals (multi-language, visualisation, bundling, Anthropic-specific features, framework wrapping), and how decisions get made (the maintainer eats the dog food).
- `MIGRATING-FROM-ADR-SKILL.md`: guide for users of Jim van den Breemen's adr-skill explaining the overlap (same gates, same template, same patterns), the differences (skill-only vs full toolkit), and three migration paths (replace, co-install, stay). Slash commands are namespaced by plugin prefix so co-installation does not cause conflicts.
- README: optional fifth slash command (`/adr-kit:lint`) added to the Install section. New Quickstart bullet for "Audit existing ADRs". New "Project resources" section linking to ROADMAP, MIGRATING, CHANGELOG, CONTRIBUTING, SECURITY, CODE_OF_CONDUCT.
- `plugin.json` keywords add `lint`. Description expanded to mention the lint and setup commands.

## [0.6.0] - 2026-04-25

### Added

- `.github/ISSUE_TEMPLATE/bug.yml`: structured bug-report form with fields for tool/version, adr-kit version, reproduction steps, expected vs actual behaviour.
- `.github/ISSUE_TEMPLATE/feature_request.yml`: structured feature-request form that requires alternatives-considered (mirroring the same discipline the skill asks of an ADR).
- `.github/pull_request_template.md`: PR checklist that explicitly references the four verification gates (Completeness, Evidence, Clarity, Consistency) and the CHANGELOG-update requirement.
- `SECURITY.md`: minimal security-disclosure policy (no secrets handled, file-write scope is constrained, reports go to maintainer email).
- `CODE_OF_CONDUCT.md`: short adoption-by-reference of Contributor Covenant 2.1, with reporting email and scope; full canonical text lives at the upstream URL.
- `examples/ADR-sample-001-postgresql-for-event-store.md`: worked-example ADR that passes all four verification gates. Fictional but realistic decision (PostgreSQL vs Kafka vs EventStoreDB for an event store) with concrete measurements, alternatives, and risks-with-mitigations.
- `examples/ADR-sample-002-evidence-gate-before-after.md`: same decision (sync vs async webhook delivery) written twice, once failing the Evidence gate and once passing. Illustrates what "replace bare adjectives with measurements" looks like in practice.
- `README.md` FAQ section: where ADRs are stored, how to customize the conventions, what to do if the project already has ADRs in another format, whether the skill auto-creates ADRs, whether this is an Anthropic product.
- `README.md` Comparison section: short table contrasting `adr-kit` with a plain ADR template along format, pre-flight discipline, acceptance bar, code-review integration, tool integration, and onboarding axes.

## [0.5.0] - 2026-04-25

### Added

- `.github/workflows/validate.yml`: GitHub Actions CI that validates `plugin.json` (`jq empty`), enforces the required-files set, asserts that `plugin.json` version matches the top entry of `CHANGELOG.md`, and runs `markdownlint` over skills, agents, instructions, and examples.
- `CONTRIBUTING.md`: dev loop (`claude --plugin-dir .`), how to add a skill or agent, version-bump and release procedure, code style (no em dashes, English, kebab-case file names), validation, and issue-reporting guidelines.
- `argument-hint: "[short title of the decision]"` on `skills/adr/SKILL.md` so users see the expected slash-command syntax in the picker.
- `allowed-tools: [Read, Write, Edit]` on `skills/setup/SKILL.md` so the one-time `/adr-kit:setup` does not prompt for tool permission.
- `homepage` (already present), enriched `keywords` list (covers Claude Cowork, Cursor, Copilot, Codex, agent-skills, AI coding assistant, decision-records, verification-gates, anti-rationalization), and an empty `dependencies: []` placeholder in `.claude-plugin/plugin.json`.

## [0.4.0] - 2026-04-25

### Added

- `CHANGELOG.md` in Keep a Changelog format. Retroactively documents v0.1.0, v0.2.0, and v0.3.0.
- `.gitignore` with sensible defaults for Claude Code plugin development (OS files, editor metadata, common cache directories).

### Changed

- Adopted the `adr-kit--vX.Y.Z` git tag convention that `claude plugin tag` expects. Existing legacy tags (`v0.1.0`, `v0.2.0`, `v0.3.0`) remain in place so pinned installs do not break; they are also mirrored to the new naming on the same commits, and the new convention applies from v0.4.0 forward.

## [0.3.0] - 2026-04-25

### Added

- `/adr-kit:setup` slash command via `skills/setup/SKILL.md`. One-time per project, idempotent: appends an "ADR Kit Rules" section to the project's `CLAUDE.md`, creates `CLAUDE.md` if it does not exist, skips if the section is already present.
- The install flow in `README.md` and `INSTALL.md` now lists four slash commands (`marketplace add`, `install`, `reload-plugins`, `setup`).

## [0.2.0] - 2026-04-25

### Added

- Native Claude Code plugin support via `.claude-plugin/plugin.json` manifest.
- Plugin install path: `/plugin marketplace add rvdbreemen/adr-kit` + `/plugin install adr-kit@rvdbreemen-adr-kit` + `/reload-plugins`.

### Changed

- **Breaking for non-Claude-Code tools**: `SKILL.md` source path moved from `adr-kit/SKILL.md` to `adr-kit/skills/adr/SKILL.md` to match the Claude Code plugin layout. Destination paths in `.claude/`, `.cursor/`, `.github/`, `.codex/` are unchanged. `INSTALL.md` and the bundled install script updated accordingly.

## [0.2.0-attribution] - 2026-04-25

### Changed

- `README.md` Credits section and `SKILL.md` credit paragraphs now name [Jim van den Breemen's adr-skill](https://github.com/Jvdbreemen/adr-skill) explicitly as the source where the anti-rationalization guards and verification gates patterns were first combined into a single ADR skill. The original pattern sources (`addyosmani/agent-skills`, `trailofbits/skills`) remain credited as upstream.

## [0.1.0] - 2026-04-25

### Added

- Initial public release of `adr-kit`.
- `SKILL.md`: comprehensive ADR guide with anti-rationalization guards (a 9-row excuse / counter-argument table) and four named verification gates (Completeness, Evidence, Clarity, Consistency).
- `agents/adr-generator.md`: focused subagent for authoring a complete ADR file given a decision and context.
- `instructions/adr.coding.md`: ADR rules during coding work, including implementation checklist and supersession workflow.
- `instructions/adr.review.md`: six named ADR checks for code review with concrete review-comment templates.
- `examples/ADR-template.md`: clean template to copy into new ADRs.
- `INSTALL.md`: per-tool install paths for Claude Code, Claude Cowork, Cursor, GitHub Copilot, and OpenAI Codex CLI, plus a one-shot helper script and a generic fallback.
- `README.md`, `LICENSE` (MIT).

### Credits

The anti-rationalization guards pattern is adapted from [addyosmani/agent-skills](https://github.com/addyosmani/agent-skills). The verification gates pattern is adapted from [trailofbits/skills](https://github.com/trailofbits/skills). Both patterns were first combined into a single ADR skill by [Jim van den Breemen's adr-skill](https://github.com/Jvdbreemen/adr-skill); `adr-kit` builds on that combination.

[Unreleased]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.11.0...HEAD
[0.11.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.10.1...adr-kit--v0.11.0
[0.10.1]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.10.0...adr-kit--v0.10.1
[0.10.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.9.0...adr-kit--v0.10.0
[0.9.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.8.0...adr-kit--v0.9.0
[0.8.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.7.2...adr-kit--v0.8.0
[0.7.2]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.7.1...adr-kit--v0.7.2
[0.7.1]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.7.0...adr-kit--v0.7.1
[0.7.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.6.0...adr-kit--v0.7.0
[0.6.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.5.0...adr-kit--v0.6.0
[0.5.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.4.0...adr-kit--v0.5.0
[0.4.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.3.0...adr-kit--v0.4.0
[0.3.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/rvdbreemen/adr-kit/releases/tag/v0.1.0
