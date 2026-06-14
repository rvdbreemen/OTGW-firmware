<!-- adr-kit-guide v0.30.5 -->
<!-- Canonical project-side ADR guide. Copied from the plugin's templates/adr-kit-guide.md to .claude/adr-kit-guide.md by /adr-kit:init, /adr-kit:upgrade, and /adr-kit:setup. -->
<!-- This file is plain markdown — readable by Claude Code, headless `claude -p`, shell scripts in pre-commit hooks, evaluator scripts, and any agent that doesn't process @-imports. Do not embed Claude-Code-specific syntax inside this file. -->

# ADR Kit Guide

This project uses [adr-kit](https://github.com/rvdbreemen/adr-kit) to manage Architecture Decision Records. The kit ships:

- a project-side guide (this file) referenced from `CLAUDE.md`,
- a library of slash commands and a subagent for ADR authorship,
- a pre-commit hook that catches code changes drifting outside accepted ADRs.

ADR files live at `docs/adr/ADR-NNN-kebab-case-title.md`. They are versioned, immutable once accepted, and the durable record of *why* the codebase looks the way it does.

## Three operating modes

| Mode | When | Entry point |
|---|---|---|
| **Init / bootstrap** | Once per project: scan source + docs, propose a starter ADR set, hook the kit into `CLAUDE.md`, install the pre-commit hook | `/adr-kit:init` |
| **Per-commit verification** | Every `git commit`: declarative-rule check (always-on, free). Claude Sonnet LLM judge for `llm_judge: true` ADRs is opt-in as of v0.17.0 (enable via `judge.llm_enabled:true` or `ADR_KIT_LLM=1`). Falls back to declarative-only when the `claude` CLI is unavailable | `.githooks/pre-commit` (auto) |
| **On-demand invocation** | Mid-session: write a new ADR, judge a staged diff, supersede an existing decision | `/adr-kit:adr`, `/adr-kit:judge`, `adr-generator` subagent |
| **Guardian (v0.18+)** | Periodic staleness detector at SessionStart; in-session model runs due health tiers | `bin/adr-guardian` (hook, free) + `/adr-kit:guardian` (sweep) |

## Slash commands

| Command | Purpose | User-only? |
|---|---|---|
| `/adr-kit:init` | One-shot project bootstrap (audit codebase, generate ADRs, install hook). Combines `setup` + audit + `install-hooks`. | yes |
| `/adr-kit:adr` | Author a single ADR (delegates to `adr-generator` subagent; runs four verification gates). | no — model can self-call |
| `/adr-kit:judge` | Interactive judge against a staged diff. Runs declarative checks + in-session LLM check for `llm_judge: true` ADRs. Walks resolution paths on violation. | no — model can self-call |
| `/adr-kit:lint` | Validate existing ADRs against the four verification gates. | yes |
| `/adr-kit:migrate` | Rewrite legacy ADRs into canonical format. | yes |
| `/adr-kit:setup` | Append `## ADR Kit` block to `CLAUDE.md` (idempotent). | yes |
| `/adr-kit:upgrade` | Migrate v0.11 → v0.12 footprint without re-running the heavy audit. | yes |
| `/adr-kit:install-hooks` | Install or uninstall the pre-commit hook; also manages the guardian's project-scoped SessionStart hook. | yes |
| `/adr-kit:guardian` | Run the ADR-set health sweep for due tier(s). Cheap tier is free; LLM tier confirms cost first. | no — model can self-call |

## The four verification gates

An ADR cannot move from `Proposed` to `Accepted` until all four pass.

1. **Completeness** — every required section is present and non-empty: Status, Context, Decision, Alternatives Considered (≥ 2), Consequences (positive + negative), Related Decisions, References. Plus filename matches `ADR-NNN-kebab-case.md` and the heading number agrees.
2. **Evidence** — Context or References cites at least one concrete external/internal artefact (incident, profiling data, code path, RFC, vendor doc). No hand-waving justifications.
3. **Clarity** — Decision section names a single concrete choice (not a survey), uses imperative voice, no hedging language ("perhaps", "we should consider"). Identifiers (file paths, function names, config keys) are traceable.
4. **Consistency** — filename, heading number, and any cross-references resolve. No duplicate ADR numbers in the directory.

`bin/adr-lint` enforces Completeness and Consistency deterministically. Evidence and Clarity are heuristic; opt in via `--gates evidence,clarity` or run `/adr-kit:lint` to use the LLM-aware skill.

## Authoring workflow (`/adr-kit:adr` or `adr-generator`)

1. Identify the architecturally significant change (architecture, NFRs, interfaces, dependencies, build/CI tooling). Refactors and bug fixes within existing patterns do NOT need an ADR.
2. Invoke `/adr-kit:adr` (or the `adr-generator` subagent). Provide: title, context with concrete forces, ≥ 2 alternatives with rejection reasons, consequences (both directions), related ADRs.
3. The agent applies the four gates and writes `docs/adr/ADR-NNN-…md` with `Status: Proposed`.
4. Human review. Iterate until all gates pass.
5. Flip Status to `Accepted, YYYY-MM-DD` after explicit human approval and append the matching transition to `## Status History`. **Never self-approve.**
6. If the decision touches code in a mechanically expressible way, add an `Enforcement` block (see below) so the pre-commit hook can guard the boundary.

## Enforcement block (v0.12+)

Optional `## Enforcement` section at the end of an ADR. Fenced JSON code block, parsed by `bin/adr-judge`. Schema: plugin's `schemas/adr-enforcement.schema.json`.

```json
{
  "forbid_pattern": [
    { "pattern": "\\bArduinoJson\\b", "path_glob": "src/**/*.{ino,cpp,h}",
      "message": "Use snprintf_P + sendJsonMapEntry; ArduinoJson fragments the heap (ADR-042)." }
  ],
  "forbid_import": [
    { "pattern": "^#include\\s+<ArduinoJson\\.h>", "path_glob": "src/**" }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

**Rules:**
- `forbid_pattern` — regex must NOT match any added line in the diff (lines starting with `+`, excluding `+++ ` markers).
- `forbid_import` — same engine as `forbid_pattern`; the separate name documents intent.
- `require_pattern` — regex must match at least once in the post-diff content of any file matching `path_glob`.
- `llm_judge: true` — Claude Sonnet evaluates the diff against this ADR's `## Decision` text when the LLM pass is active. The pre-commit hook batches all `llm_judge: true` ADRs into one Sonnet call and blocks the commit on `VIOLATION`. The LLM pass is opt-in as of v0.17.0 (enable via `judge.llm_enabled:true` in `.adr-kit.json`, or `ADR_KIT_LLM=1` per-commit). Falls back gracefully (advisory only, exit 0) when the `claude` CLI is missing.
- ADRs with no Enforcement block are skipped silently by the judge.

**Path globs** support `**` (recursive). Examples: `src/**/*.py`, `tests/**`, `**/Makefile`.

## Pre-commit hook

After `/adr-kit:init` (or `/adr-kit:install-hooks`), every `git commit` runs `bin/adr-judge` on the staged diff with two passes:

- **Declarative pass** — always-on; fast, regex-only, no LLM. A violation exits non-zero and blocks the commit.
- **LLM pass (Sonnet, opt-in as of v0.17.0)** — all `llm_judge: true` ADRs are batched into one `claude -p --model claude-sonnet-4-6` call. Sonnet returns a per-ADR JSON verdict; any `VIOLATION` blocks the commit with the model's one-sentence reason. Falls back gracefully when the `claude` CLI is missing or unauthenticated — never blocks a legitimate commit due to tooling drift.

**Cost shape** (typical project, 50 `llm_judge` ADRs, small diff): roughly $0.10–0.30 per commit on Sonnet 4.6 with prompt caching. Latency 5–10s. Configurable via `judge.llm_model` / `judge.llm_timeout_seconds` / `judge.llm_cmd` in `docs/adr/.adr-kit.json`.

**Knobs:**
- Enable LLM pass per commit: `ADR_KIT_LLM=1 git commit -m "…"`
- Enable LLM pass project-wide: set `judge.llm_enabled: true` in `docs/adr/.adr-kit.json`
- Disable LLM pass per commit: `ADR_KIT_NO_LLM=1 git commit -m "…"`
- Disable hook entirely per commit: `ADR_KIT_HOOK_DISABLE=1 git commit -m "…"`
- Switch model: set `judge.llm_model: "claude-haiku-4-5"` in `.adr-kit.json` for higher throughput at lower cost.
- Remove permanently: `/adr-kit:install-hooks --uninstall`

## Supersession (changing a decision)

Accepted ADRs are immutable. To change a decision:

1. Author a new ADR with the next number. Status `Proposed`. The Decision should explain what changes and why now.
2. In its Related Decisions: `Supersedes ADR-OLD`.
3. After the new ADR is `Accepted`: edit the old ADR's Status line to `Superseded by ADR-NEW, YYYY-MM-DD.` and append that transition to its `## Status History`. Leave every other section and all earlier history entries untouched: the old decision's content is the historical record.

Never edit Decision, Context, Consequences, or Alternatives of an Accepted/Deprecated ADR. Only the Status line and an appended Status History transition may change.

## Code review checks

When reviewing a PR, apply these seven checks (Check 7 added in v0.12):

1. **ADR exists** for any architecturally significant change in the PR (new dep, interface change, NFR shift, build tooling change). Missing → request the author to invoke `/adr-kit:adr` or `adr-generator`.
2. **ADR is linked** in the PR description (path or relative URL).
3. **No violation** of Accepted ADRs in the diff. Cross-reference against `docs/adr/README.md` and the Enforcement blocks. The pre-commit hook should have caught this; if it didn't, the ADR is missing rules or wasn't installed.
4. **Supersession chain is correct** — old ADR's Status updated, new ADR cross-references, content immutability preserved.
5. **All four gates pass** on any new/modified ADR. Cite the failing gate when blocking ("fails Evidence gate — no concrete reference in Context").
6. **Legacy non-compliance has a remediation plan** — pre-existing violations that this PR doesn't fix should at least carry a `// TODO(ADR-NNN): align` or a backlog entry, not be silently ignored.
7. **Enforcement block is set appropriately** on any new Accepted ADR with a code surface. Either declarative rules, OR `llm_judge: true`, OR an explicit "manual review only" note in the ADR body explaining why the rule cannot be expressed mechanically. Missing block on a code-touching ADR is a smell.

## Anti-rationalisation guards

When `/adr-kit:adr` is asked to write or accept an ADR, it actively pushes back on these nine common excuses (see plugin's `skills/adr/SKILL.md` for the full text):

- "It's just a small change" — the rule is "architecturally significant", not "large".
- "We can decide later" — later is now; defer = decide.
- "Everyone knows this" — undocumented tacit knowledge is the problem ADRs solve.
- "It's documented in the code" — code shows what, not why.
- "We'll do it the same as last time" — name "last time" with an ADR reference.
- "There's only one option" — there are always alternatives; "do nothing" is one.
- "It's reversible" — most architecture is partially reversible; the ADR captures the *current* commitment.
- "It's a refactor" — pure refactors don't need ADRs; *new patterns* introduced during refactoring do.
- "We don't have time" — opportunity cost of skipping is a future maintainer hunting for the why.

## Guardian (v0.18.0+)

The ADR Guardian is a periodic staleness detector that injects an `[adr-guardian]` nudge block at Claude Code SessionStart when a health tier is due. It never blocks session start, never runs an LLM, and emits nothing when nothing is due.

**Two-tier cadence:**

| Tier | Default cadence | Tools | Cost |
|---|---|---|---|
| **cheap** | Daily (`drift_stale_days: 1`) | `adr-judge` (declarative), `adr-retire`, `adr-lint`/`adr-status` | Free |
| **llm** | Bi-weekly (`llm_stale_days: 14`) | `adr-suggest` (missing-ADR), `adr-judge --llm` (audit) | ~$0.10–0.30 |

**On seeing an `[adr-guardian] ... DUE` block at session start:**
1. Identify which tier(s) are marked `DUE`.
2. For the cheap tier: offer "ADR drift/health check is due — run `/adr-kit:guardian cheap` to sweep (free, ~30s)?"
3. For the LLM tier: offer "ADR semantic check is due (bi-weekly) — run `/adr-kit:guardian llm`? This will confirm cost (~$0.10–0.30) before spending."
4. Confirm cost before the LLM tier. Never run LLM-tier work without explicit user confirmation (unless `guardian.llm_autorun: true` in `.adr-kit.json`).

**Mix-by-finding-type responses:**

| Finding | Response |
|---|---|
| **Drift** — code violates an Accepted ADR | Surface prominently. List violations with file:line + ADR. Offer fix or task. |
| **Missing ADR** — new decision not recorded | Passive. List candidates; offer to author via `adr-generator`. User picks. |
| **Stale ADR** — tech removed/superseded/policy drift | Draft retirement/supersession for review. Never auto-apply. |
| **Health** — gate failures, broken chains | Report PASS/ADVISORY/FAIL. Offer to fix FAILs via `adr-generator`. |

**Config** (`docs/adr/.adr-kit.json`, `guardian` block):

```json
"guardian": {
  "enabled": true,
  "drift_stale_days": 1,
  "llm_stale_days": 14,
  "nudge_cooldown_hours": 24,
  "llm_autorun": false
}
```

**Hook install paths (both are shipped):**
- Plugin-level (default): the adr-kit plugin declares a `SessionStart` hook in `.claude-plugin/plugin.json`. Auto-registers when the plugin is enabled. Self-guards (no-ops in non-ADR projects).
- Project-scoped (explicit): `/adr-kit:install-hooks` adds a `SessionStart` entry to the project's `.claude/settings.json`. Idempotent add and clean remove (JSON-structural, never clobbers other hooks).

State file: `docs/adr/.adr-kit-state.json` (gitignored, per-machine).

## Plugin-side deep dives

This guide is the project's own copy. For agents inside Claude Code, the plugin auto-loads richer instructions:

- Plugin path (locale-dependent): `~/.claude/plugins/cache/rvdbreemen-adr-kit/adr-kit/<version>/`
- `instructions/adr.coding.md` — per-developer rules (when to invoke the agent, supersession workflow, Definition of Done).
- `instructions/adr.review.md` — the seven review checks with citation templates.
- `skills/adr/SKILL.md` — full anti-rationalisation guard list, gate definitions, code examples.
- `agents/adr-generator.md` — the subagent prompt.

If you're working outside Claude Code (in a hook, a CI job, or a different agent), this file (`.claude/adr-kit-guide.md`) is your one-stop reference. Keep it in version control with the rest of the project.
