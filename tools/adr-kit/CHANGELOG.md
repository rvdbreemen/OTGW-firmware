# Changelog

All notable changes to `adr-kit` are documented in this file. The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.7.0...HEAD
[0.7.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.6.0...adr-kit--v0.7.0
[0.6.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.5.0...adr-kit--v0.6.0
[0.5.0]: https://github.com/rvdbreemen/adr-kit/compare/adr-kit--v0.4.0...adr-kit--v0.5.0
[0.4.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.3.0...adr-kit--v0.4.0
[0.3.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/rvdbreemen/adr-kit/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/rvdbreemen/adr-kit/releases/tag/v0.1.0
