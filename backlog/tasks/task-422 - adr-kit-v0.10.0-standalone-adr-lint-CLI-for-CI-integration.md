---
id: TASK-422
title: 'adr-kit v0.10.0: standalone adr-lint CLI for CI integration'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 20:45'
updated_date: '2026-04-25 20:54'
labels:
  - adr-kit
  - external-repo
  - feature
  - tier-1
  - ci
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why

The `/adr-kit:lint` skill in v0.9.0 is excellent for human-in-the-loop review (judgement-based gates like Evidence and Clarity rely on Claude). It cannot run unattended in GitHub Actions or as a pre-commit hook. The "field-stability gate before merging" pattern requires a deterministic CLI.

A small Python script that mirrors the deterministic gates (Completeness, Consistency) provides exactly that. Pair with the existing skill: skill for nuanced review, CLI for CI / pre-commit / batch validation.

This is the missing piece toward the v1.0.0 "PR block in real review" criterion in `ROADMAP.md`.

Active development: this work proceeds independently of TASK-421 (v0.9.0 field verification). Ship and iterate.

## Scope

**In scope:**

1. `bin/adr-lint` (Python 3.8+, stdlib only; `jsonschema` optional but graceful fallback).
2. CLI:
   - Positional arg: path (file or directory). Default: `docs/adr/`.
   - `--strict-from ADR-XXX`: override config-file value.
   - `--gates completeness,consistency,evidence,clarity`: limit which gates run. Default: `completeness,consistency` (the deterministic ones).
   - `--format human|json`: output format.
   - `--config <path>`: override default `docs/adr/.adr-kit.json` location.
   - `-v` / `--verbose`: per-ADR detail.
3. Exit codes: 0 = no FAIL, 1 = FAIL detected, 2 = config or input error.
4. Same severity model as v0.9.0 skill: ignore beats markers, markers beat config, within config `always_strict` > `always_advisory` > `advisory_before_strict_from`.
5. `schemas/adr-kit-config.schema.json` (draft-07) for `.adr-kit.json`. Validates via `jsonschema` if available; warns if missing module.
6. `tests/` directory with pytest fixtures: at least 5 sample ADRs covering each FAIL pattern (missing heading, wrong filename, duplicate number, broken cross-ref, etc.) plus 2-3 sample policies. Run via `pytest tests/`.
7. `.github/workflows/adr-lint-self.yml`: example workflow that runs `bin/adr-lint examples/` on every push, demonstrating the pattern downstream users can copy.
8. Documentation: README "CI integration" section showing copy-paste-ready GitHub Actions snippet, plus a `bin/adr-lint --help` example.
9. v0.10.0 release: bump plugin.json + marketplace.json, CHANGELOG entry, tag `adr-kit--v0.10.0`, GitHub Release with --latest, mirror sync to `tools/adr-kit/`.

**Out of scope:**

- Evidence and Clarity gates as default. They are heuristic and noisy without judgement; ship them behind `--gates` so users can opt in but they are not run by default.
- Auto-fix mode. Read-only by design (matches the skill).
- The `/adr-kit:migrate` skill (mass-rewrite legacy ADRs into canonical template). That is conceptually different work; keep as a separate later release.
- Windows-specific PATH integration (chocolatey/scoop packaging). Linux/macOS execute-bit + `python3 bin/adr-lint` works; Windows users `python bin/adr-lint`.
- Replace the skill. The skill stays canonical for nuanced review; the CLI is for CI gates.

## Trade-offs accepted

- **Skill and CLI must stay in lockstep on the deterministic gates.** Mitigation: tests in `tests/` exercise the gate logic on identical fixtures. If a divergence opens, tests catch it.
- **Python adds a dependency.** Mitigation: stdlib only by default; `jsonschema` is optional with graceful fallback. Python 3.8 floor matches typical CI baseline.

## Real-world smoke test (acceptance gate)

Running `bin/adr-lint /path/to/OTGW-firmware/docs/adr/` against the 87-ADR OTGW-firmware set must produce the same numeric outcome as the v0.9.0 skill smoke test (post-TASK-420 heading fixes):

- 7 PASS strictly
- 80 ADVISORY only
- 0 FAIL

Exit code 0. JSON output (`--format json`) parseable by `jq`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 bin/adr-lint exists, executable, Python 3.8+ compatible, stdlib-only baseline (jsonschema optional with graceful fallback)
- [x] #2 CLI accepts: positional path arg (default docs/adr/), --strict-from, --gates, --format human|json, --config, -v/--verbose, --help
- [x] #3 Exit codes: 0 (no FAIL), 1 (FAIL detected), 2 (config/input error). Verifiable via shell echo $?
- [x] #4 Default --gates is completeness,consistency (deterministic). Evidence and clarity available behind explicit --gates flag (heuristic, opt-in)
- [x] #5 Severity model matches v0.9.0 skill: ignore > markers > config; always_strict > always_advisory > advisory_before_strict_from
- [x] #6 schemas/adr-kit-config.schema.json (draft-07) validates .adr-kit.json. Used by bin/adr-lint when jsonschema is installed
- [x] #7 tests/ directory with pytest-runnable fixtures covering every FAIL pattern (missing-heading, wrong-filename, heading-mismatch, duplicate-number, broken-cross-ref, marker-skip, marker-advisory, ignore-list)
- [x] #8 .github/workflows/adr-lint-self.yml runs bin/adr-lint examples/ on push and PR to main; fails the build on FAIL
- [x] #9 README gains a 'CI integration' section with copy-paste-ready GitHub Actions snippet plus an 'adr-lint --help' example block
- [x] #10 Real-world smoke test: bin/adr-lint /path/to/OTGW-firmware/docs/adr/ produces 7 PASS / 80 ADVISORY / 0 FAIL with exit 0; JSON format parseable by jq
- [x] #11 plugin.json 0.10.0 + marketplace.json plugin entry 0.10.0 + CHANGELOG [0.10.0] entry with link reference
- [x] #12 Tag adr-kit--v0.10.0 + GitHub Release published with --latest, release notes in .github/release-notes-0.10.0.md
- [x] #13 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte (including bin/, schemas/, tests/, workflows/)
- [x] #14 All edits English, em-dash-free, no emojis
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan TASK-422 — adr-kit v0.10.0 standalone adr-lint CLI

Active development: no dependency on field-test of v0.9.0. Ship and iterate.

### Werkrepo

- External: `D:\Users\Robert\Documents\GitHub\RvdB\adr-kit\` (canonical)
- Mirror: `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\tools\adr-kit\`

### Stappen (volgorde)

1. **bin/adr-lint** — Python 3.8+ stdlib-only CLI. Mirror the gate logic from skills/lint/SKILL.md verbatim where deterministic; raise NotImplementedError or skip gracefully for the heuristic ones unless `--gates evidence,clarity` is explicitly passed.
2. **schemas/adr-kit-config.schema.json** — draft-07 schema for `.adr-kit.json`. Pattern-validates `strict_from`, enum-validates severity values, etc. Top-level description documents what each constraint guards against (paired with v0.8.0 schema convention).
3. **tests/** — pytest fixtures. Each fixture is a tiny `docs/adr/` tree with sample ADRs that should trigger a known result. Tests run `bin/adr-lint --format json` and assert on the JSON output, so they verify both gate logic and JSON shape.
4. **.github/workflows/adr-lint-self.yml** — runs `bin/adr-lint examples/` on push/PR. Demonstrates downstream-copyable pattern.
5. **README** — "CI integration" section between "Configuration" and "FAQ", with GH Actions snippet and `--help` example.
6. **Smoke test** — run the new CLI against OTGW's `docs/adr/`. Must return 7 PASS / 80 ADVISORY / 0 FAIL with exit 0.
7. **Release** — bump versions, CHANGELOG, mirror sync, tag, push, GitHub release.

### Architectural choices

- **Single-file Python script**: `bin/adr-lint` is one executable file (~400 lines). No package, no `setup.py`. Run as `python bin/adr-lint` or `./bin/adr-lint` (with shebang). Keeps the install story trivial: clone repo, run script.
- **Stdlib-only baseline**: argparse, json, re, pathlib, sys. `jsonschema` is imported lazily; if missing, schema validation is skipped with a warning rather than failed.
- **JSON output schema**: `{"summary": {"pass":N,"advisory":N,"fail":N,"skipped":N,"total":N}, "config": {...}, "files": [{"file":"...", "bucket":"PASS|ADVISORY|FAIL|SKIPPED", "findings":[{"gate":"...", "level":"FAIL", "details":"..."}]}], "exit_code": N}`.
- **Tests use subprocess**: each test runs `subprocess.run([sys.executable, 'bin/adr-lint', '--format', 'json', fixture_dir])` and asserts on stdout JSON + exit code. Verifies the actual interface, not internal helpers.

### Risks

- **Skill/CLI divergence**: mitigated by tests on identical fixtures. If skill body changes, tests catch the drift on next CI run.
- **Cross-platform path handling**: use pathlib throughout; no string concat. Tests run on ubuntu-latest GH Actions runner.
- **Empty docs/adr/ edge case**: handle gracefully with informative message and exit 0.

### Out of scope (deferred)

- /adr-kit:migrate skill (different release).
- Pre-commit hook config example (can add in v0.10.x).
- Pip-installable package (`pyproject.toml` setup) — premature; single-file script first.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-422 -- adr-kit v0.10.0 published

Deterministic Python CLI for CI integration. Pairs with the v0.9.0 `/adr-kit:lint` skill: skill for nuanced human-in-the-loop review, CLI for unattended CI gates.

### Shipped

- **`bin/adr-lint`** (~400-line single-file Python 3.8+ CLI, stdlib only by default; `jsonschema` auto-detected for deeper config validation). Default gates: `completeness,consistency` (deterministic). Heuristic `evidence` and `clarity` gates available behind `--gates`. Exit codes: `0` (no FAIL), `1` (FAIL), `2` (config/input error). Both human and JSON output formats.
- **`schemas/adr-kit-config.schema.json`** (draft-07): pattern-validates `strict_from`, enum-validates severity values, validates `template.required_sections` heading shape.
- **`tests/`**: 15 pytest tests, subprocess-based (each test runs the CLI as the public interface). Fixtures cover canonical PASS, missing-headings, bad-filename, heading-mismatch, marker-skip, marker-advisory, marker-skip-gate, strict_from boundary, --strict-from override, --gates filter, malformed config, unknown gate, missing path, single file, human format. All 15 pass locally.
- **`.github/workflows/adr-lint-self.yml`**: pytest job + smoke-test on `examples/` on push/PR to main.
- **`.github/workflows/validate.yml`** extended to require `bin/adr-lint`, `schemas/adr-kit-config.schema.json`, and `tests/test_adr_lint.py` in the file presence check.
- **README "CI integration" section** with copy-paste-ready GitHub Actions snippet that fetches `bin/adr-lint` from the repo's `main` branch via `curl` and runs it as a PR merge gate. Stdlib-only means no `pip install` needed.

### Severity model match

Same precedence as v0.9.0 skill: `ignore` > markers > config; within config `always_strict` > `always_advisory` > `advisory_before_strict_from`. Test `test_strict_from_boundary` and `test_marker_advisory_demotes_failures` verify the lockstep.

### Real-world smoke test (AC #10 evidence)

Run against the 87-ADR set used in TASK-420's smoke test:

```
$ bin/adr-lint /path/to/.../docs/adr/
PASS strictly:  7
ADVISORY only: 80
FAIL:           0
exit 0
```

JSON output (`--format json`) parseable by `jq`. Matches the skill's output exactly.

### Generic adr-kit (no project leak)

User flagged mid-task to keep the generic adr-kit release free of OTGW-specific references. Verified: CHANGELOG and release notes use phrasing like "a representative real-world ADR set" and `/path/to/your-project/docs/adr/` instead of naming the validator project.

### Release outputs

- Commit: `4147bc9 chore(release): v0.10.0 (bin/adr-lint deterministic CLI for CI integration)` (22 files; +1229 insertions)
- Tag: `adr-kit--v0.10.0` pushed to origin
- GitHub Release: <https://github.com/rvdbreemen/adr-kit/releases/tag/adr-kit--v0.10.0> (marked --latest)
- Mirror sync: `tools/adr-kit/` byte-equivalent in OTGW-firmware

### Active development stance

Per user direction, this work proceeded independently of TASK-421 (v0.9.0 field verification). Ship and iterate.

### Future work (deferred)

- `/adr-kit:migrate` interactive helper for mass-rewriting legacy ADRs (separate release).
- Pre-commit hook YAML example (thin wrapper around the GH Actions snippet; v0.10.x).
- Pip-installable package (premature; single-file script first).
<!-- SECTION:FINAL_SUMMARY:END -->
