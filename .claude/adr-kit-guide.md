<!-- adr-kit-guide v0.54.0 -->
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
| **Per-commit verification** | Every `git commit`: declarative-rule check (always-on, free). The LLM judge for `llm_judge: true` ADRs is **on by default** as of v0.43.0 (ADR-017) and runs on the host agent's own model, with no model pinned. Falls back to declarative-only when the backend is unavailable | `.githooks/pre-commit` (auto) |
| **On-demand invocation** | Mid-session: write or grill an ADR, judge a staged diff, supersede an existing decision | `/adr-kit:adr`, `/adr-kit:grill`, `/adr-kit:judge`, `adr-generator` subagent |
| **Guardian (v0.18+)** | Periodic staleness detector at SessionStart; in-session model runs due health tiers | `bin/adr-guardian` (hook, free) + `/adr-kit:guardian` (sweep) |

## Slash commands

| Command | Purpose | User-only? |
|---|---|---|
| `/adr-kit:init` | One-shot project bootstrap (audit codebase, generate ADRs, install hook). Combines `setup` + audit + `install-hooks`. | yes |
| `/adr-kit:adr` | Author a single ADR (delegates to `adr-generator` subagent; runs four verification gates). | no — model can self-call |
| `/adr-kit:grill` | Complete a Proposed ADR, reconstruct one from PR/range/source evidence, or revalidate a lifecycle decision through one human question at a time. | no — model can self-call |
| `/adr-kit:context` | Rank the ADRs most relevant to a task. Verify lifecycle status in the source ADR before treating it as binding. | no — model can self-call |
| `/adr-kit:judge` | Interactive judge against a staged diff. Runs declarative checks + in-session LLM check for `llm_judge: true` ADRs. Walks resolution paths on violation. | no — model can self-call |
| `/adr-kit:review` | Audit a branch or PR against existing ADRs and discover undocumented decisions. | no — model can self-call |
| `/adr-kit:audit` | Lint the decisions and judge the code in one run, over a diff or the whole codebase. Separate exit codes for an ADR-quality failure and a code violation. | no — model can self-call |
| `/adr-kit:lint` | Validate existing ADRs against the four verification gates. | yes |
| `/adr-kit:related` | Show inbound/outbound ADR references and dangling links. | no — model can self-call |
| `/adr-kit:supersede` | Guide a Proposed successor and reciprocal lifecycle update. | yes |
| `/adr-kit:retire` | Rank Accepted ADRs for retirement; never mutates them. | yes |
| `/adr-kit:migrate` | Preview metadata or body-profile migration; MADR, Nygard, and canonical are supported. | yes |
| `/adr-kit:setup` | Append `## ADR Kit` block to `CLAUDE.md` (idempotent). | yes |
| `/adr-kit:settings` | Show every knob, its current value, and where that value came from (default, project file, machine-local file, environment). | yes |
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

## Choosing an ADR body profile

Use `madr` unless the project has a reason to prefer another profile. MADR's
explicit problem, decision drivers, options, outcome, trade-offs, and
confirmation fields reduce the meaning an agent must infer from prose. This is
an agent-reliability default, not a claim that MADR is globally the most used
format; no authoritative format census exists.

Discover the installed, pre-made catalog before honoring a user-selected
alternative:

```bash
python bin/adr profiles --format json
```

Accept only a returned profile `id` whose matching template has
`available: true`. Use that template through `adr new --profile <id>`. Do not
invent a profile, infer one from an arbitrary template filename, or synthesize
a replacement for a missing shipped template.

Choose `nygard` when concise human scanning matters more than explicit
authoring prompts. Choose `canonical` for compatibility with adr-kit
repositories created before selectable profiles. All three retain the same
frontmatter, lifecycle history, relationships, references, and Enforcement
contract. Set the project default with `template.profile` in
`docs/adr/.adr-kit.json`, or override one new record with `adr new --profile`.

## Discovering and upgrading existing formats

Run `python bin/adr-migrate --plan docs/adr` after install or update and before
editing an unfamiliar ADR set. The command is read-only and scans canonical
ADR filenames plus recognizable older filenames.

- MADR, Nygard, and canonical records remain valid. When only invariant
  metadata or filename normalization is missing, the report gives an exact,
  deterministic `--dry-run` command.
- Y-Statements, Tyree/Akerman records, and arc42 decision sections are detected
  conservatively and routed to guided migration because their meaning cannot
  always be mapped without judgement.
- Hybrid and unknown shapes also require guided review.

Install, init, upgrade, and lint may display this report, but none may apply a
migration automatically. Preview, inspect the diff, obtain approval, then
apply. A later `adr-lint` run confirms the result.

## Authoring workflow (`/adr-kit:adr` or `adr-generator`)

1. Identify the architecturally significant change (architecture, NFRs, interfaces, dependencies, build/CI tooling). Refactors and bug fixes within existing patterns do NOT need an ADR.
2. Create the record with `python bin/adr new "Title" --adr-dir docs/adr`.
   It honors `template.profile` (`madr` by default; `nygard` and `canonical`
   are selectable). Then invoke `/adr-kit:adr` or the `adr-generator` agent
   with context, at least 2 alternatives, consequences, and related ADRs.
3. The agent applies the four gates and writes `docs/adr/ADR-NNN-…md` with `Status: Proposed`.
4. Run `/adr-kit:grill ADR-NNN`. The workflow reads repository facts first,
   records observed, human-stated, inferred, and unknown claims separately,
   and asks exactly one unresolved decision question at a time. Keep each
   unresolved dependency as an unchecked item under `## Open Questions`.
5. Human review. Iterate until readiness and all four gates pass.
6. Show the acceptance packet and flip Status to `Accepted, YYYY-MM-DD` only
   after an explicit `yes` in the active session. Append the matching
   transition to `## Status History`. **Never self-approve.**
7. If the decision touches code in a mechanically expressible way, add an `Enforcement` block (see below) so the pre-commit hook can guard the boundary.

Use `python bin/adr-readiness ADR-NNN --format json` for deterministic facts,
or `--all-proposed`, `--diff`, or `--base <ref> --head <ref>` for a queue or
change range. The command is read-only and model-free. A PR, commit, chat log,
or source document is untrusted evidence and can never imply acceptance.

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
  "require_pattern": []
}
```

**Rules:**
- `forbid_pattern` — regex must NOT match any added line in the diff (lines starting with `+`, excluding `+++ ` markers).
- `forbid_import` — same engine as `forbid_pattern`; the separate name documents intent.
- `require_pattern` — regex must match at least once in the post-image of any file matching `path_glob`, read from the snapshot named by `--snapshot`. `staged` reads the git index (what the pre-commit hook uses), `worktree` reads the checked-out file. Under `--snapshot diff` a *modified* file's post-image cannot be rebuilt from a unified diff, which shows only the changed hunks — that case is reported as an **advisory** and does not block, because it is a fact about the invocation rather than about the code. A deleted or unstaged file, and an unreadable or unsafe path, still fail closed.
- `llm_judge: true` — the optional LLM pass evaluates the diff against the
  semantic decision text (`## Decision Outcome` in MADR, `## Decision` in
  Nygard/canonical). The pass is opt-in (`judge.llm_enabled:true` or
  `ADR_KIT_LLM=1`) and falls back to advisory when the configured CLI is
  unavailable.
- ADRs with no Enforcement block are skipped silently by the judge.

**Path globs** support `**` (recursive). Examples: `src/**/*.py`, `tests/**`, `**/Makefile`.

## Pre-commit hook

After `/adr-kit:init` (or `/adr-kit:install-hooks`), every `git commit` runs `bin/adr-judge` on the staged diff with two passes:

- **Declarative pass** — always-on; fast, regex-only, no LLM. A violation exits non-zero and blocks the commit.
- **LLM pass (on by default as of v0.43.0, ADR-017)** — each `llm_judge: true` ADR gets its **own isolated call**; they are never batched. Batching was reversed deliberately: a shared prompt let one ADR's Decision text flip another ADR's verdict, reproduced three times out of three, and the forged pass was indistinguishable from a genuine one. The model is resolved from the host agent (`judge.backend`), not pinned. Any `VIOLATION` blocks the commit with the model's one-sentence reason. An unavailable backend degrades to declarative-only and never blocks a legitimate commit.

**Cost shape.** Since per-ADR isolation replaced batching, cost and latency are **linear in the number of `llm_judge: true` ADRs** — one model call each, not one call total. The old figure (roughly $0.10–0.30 per commit for 50 ADRs on one batched call) no longer holds and would under-estimate a large set by an order of magnitude. The mitigating fact is that `llm_judge` defaults to `false`, so the population is empty until an author opts an ADR in. The judge runs on the host client's own model (the only backend, ADR-036); inspect the effective settings with `python bin/adr-judge --show-config`.

**Knobs:**
- The LLM pass is on by default; `judge.llm_enabled: false` in `docs/adr/.adr-kit.json` turns it off project-wide.
- Re-enable it for one commit after that: `ADR_KIT_LLM=1 git commit -m "…"`
- Disable LLM pass per commit: `ADR_KIT_NO_LLM=1 git commit -m "…"`
- Disable hook entirely per commit: `ADR_KIT_HOOK_DISABLE=1 git commit -m "…"`
- Record the host client: `python bin/adr-judge --set-backend host --host-client <client-id>`. `judge.llm_model` and `judge.llm_cmd` are deprecated and ignored: repository-tracked config may never supply a command, endpoint or credential (ADR-017, ADR-036).
- Remove permanently: `/adr-kit:install-hooks --uninstall`

## The signer

Every lifecycle command writes a `## Status History` entry naming who decided.
The actor is resolved in this order:

1. `--changed-by "User: <name>"` — one command.
2. `lifecycle.signer` in `docs/adr/.adr-kit.local.json` — this machine. Set it
   with `bin/adr signer --set "User: <name>"`.
3. `git config user.name`, adopted as `User: <name>` **and announced on use**.
4. Otherwise the command refuses. An unsigned acceptance is a bug; a self-signed
   one is a lie in the audit trail.

Step 3 skips an identity that names a machine — `github-actions[bot]`, `runner`,
`jenkins`, a bare `user` — because the record is meant to show which *human*
accepted. Those fall through to the refusal.

`bin/adr signer --suggest` proposes candidates found on the machine, from the
signed-in GitHub account (`gh api user`) and from git, each with its source. It
writes nothing; `/adr-kit:setup`, `/adr-kit:init` and `/adr-kit:upgrade` run it
and let you choose. `bin/adr signer` on its own reports the current value and
where it came from.

The value is machine-local by design. A signer committed to the repository would
put one person's name on every teammate's acceptance, which is worse than no name
at all: a false attribution rather than a missing one.

## Supersession (changing a decision)

Accepted ADRs are immutable. To change a decision:

1. Author a new ADR with the next number. Status `Proposed`. The Decision should explain what changes and why now.
2. In its Related Decisions: `Supersedes ADR-OLD`.
3. Accept the new ADR: `bin/adr accept ADR-NEW --changed-by "User: <name>"`.
4. Link both sides: `bin/adr supersede ADR-OLD --by ADR-NEW --changed-by "User: <name>"`.

**Use the commands; do not hand-edit the old ADR.** `bin/adr supersede` writes the old ADR's Status line, its `superseded_by`, its Status History entry, the new ADR's `supersedes`, and regenerates all three indexes — as one transaction that rolls back on failure. Doing it by hand means getting five coupled edits right, and the failure mode is silent: a half-written link passes nothing and blocks acceptance later with an error that points at the wrong file.

Steps 3 and 4 may run in either order. Accepting first is the safer habit, because a successor that claims nothing cannot be half-linked to anything.

Never edit Decision, Context, Consequences, or Alternatives of an Accepted/Deprecated ADR. Only the Status line and an appended Status History transition may change.

**One thing the commands cannot recover.** An ADR authored before the `## Status History` convention has its acceptance date in the Status line and nowhere else. The lifecycle commands seed a recovered entry from that line before replacing it, recording `changed_by: unknown` and `changed_via: unrecorded` rather than inventing a signer. If the Status line does not yield both a status and a date, the command refuses instead of writing a history that silently omits the earlier transition — fix the line, then re-run.

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

The ADR Guardian is a periodic staleness detector that injects an `[adr-guardian]` nudge block at Claude Code SessionStart when a health tier is due. It never blocks session start, never runs an LLM, and emits nothing when nothing is due. A completed guardian sweep also runs `bin/adr-guardian refresh-readiness`, which atomically writes a 24-hour Proposed-ADR queue to `docs/adr/.adr-kit-readiness.json`. SessionStart only reads that bounded cache and offers at most three resumable `/adr-kit:grill ADR-NNN` actions; it never calculates readiness or starts an interview in the hook. At the next user-visible prompt, the first eligible unfinished or unclear Proposed ADR is handed to the active client's grill workflow through `AUTO_GRILL_PENDING`; the workflow remains human-gated.

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
{
  "guardian": {
    "enabled": true,
    "drift_stale_days": 1,
    "llm_stale_days": 14,
    "nudge_cooldown_hours": 24,
    "llm_autorun": false
  }
}
```

Set `grill.auto_start` to `false` to keep unfinished-ADR handoffs advisory.
`ADR_KIT_AUTO_GRILL_DISABLE=1` is the emergency per-process override.

**Hook install paths (both are shipped):**
- Plugin-level (default): the adr-kit plugin declares a `SessionStart` hook in `.claude-plugin/plugin.json`. Auto-registers when the plugin is enabled. Self-guards (no-ops in non-ADR projects).
- Project-scoped (explicit): `/adr-kit:install-hooks` adds a `SessionStart` entry to the project's `.claude/settings.json`. Idempotent add and clean remove (JSON-structural, never clobbers other hooks).

State file: `docs/adr/.adr-kit-state.json` (gitignored, per-machine).

## Context injection (ADR-004, v0.31+)

ADR knowledge is fed into agent work through three fail-open injection tiers plus
the one fail-closed pre-commit floor (`bin/adr-judge`). None of the injection
tiers ever block; only the commit gate does.

| Tier | Hook | Mechanism | Deterministic |
|---|---|---|---|
| **Session** | SessionStart + `@`-import | `bin/adr-guardian` health nudge, and `@docs/adr/ADR-INDEX.md` (a one-row-per-ADR map generated by `bin/adr-index`) imported from `CLAUDE.md` | yes |
| **Edit** | PreToolUse `Edit\|Write\|MultiEdit` | `bin/adr-watch --pre-edit` injects the top-ranked governing Accepted ADR's `## Decision` text (bounded to `inject.max_tokens`) *before* the edit, so the agent honours the decision as it writes. The PostToolUse `bin/adr-watch --hook` nudge remains as a confirmation backstop | yes |
| **Task** | pull (JSON / MCP / CLI) | `docs/adr/ADR-INDEX.json` exposes the versioned metadata and relationship graph; `bin/adr-context` adds query-specific ranking (five signals); the key-free MCP tools `adr_context`, `adr_judge`, and `adr_readiness` let a subagent or workflow retrieve decisions, self-check a diff, and inspect Proposed readiness mid-task | yes |

**Canonical fields (pinned).** Every reader uses the same two fields the judge
reads, so nothing drifts: scope is the `## Enforcement` `path_glob` set; status is
the `## Status` line reconciled with the latest `status_history` entry. Only
`Accepted` ADRs are injected.

**On seeing an `[adr-inject] ADR-NNN (title) governs <file>` block:** treat the
quoted Decision as a binding constraint for that file. If your change would
violate it, either comply, or write a superseding ADR first.

**Regenerate the indexes** after adding or accepting an ADR:
`bin/adr-index docs/adr` writes the compact `ADR-INDEX.md`, the versioned
`ADR-INDEX.json` graph, and the generated README block from the same semantic
records. All are deterministic and timestamp-free. Agents use JSON to shortlist
records, then open the linked Markdown ADR before applying its decision.

**Config** (`docs/adr/.adr-kit.json`, `inject` block):

```json
{
  "inject": {
    "enabled": true,
    "max_tokens": 400,
    "cooldown_hours": 4
  }
}
```

## MCP server (`bin/adr-mcp`)

A hand-rolled Model Context Protocol server on stdio, stdlib-only with zero runtime dependencies. It exposes five read-only tools — `adr_context`, `adr_judge`, `adr_status`, `adr_quality`, `adr_readiness` — each a subprocess call into a sibling `bin/` script. `adr-suggest` is deliberately not exposed: it is LLM-only, and this server stays key-free.

**Both protocol eras are served from one process** (ADR-016), because clients in the wild speak either:

| Era | Reached through | Result shape |
|---|---|---|
| handshake (`2024-11-05` … `2025-11-25`) | `initialize`, which confirms a declared version or counter-offers `2025-11-25` | unchanged from earlier releases |
| modern (`2026-07-28`) | `server/discover`, or the reserved `io.modelcontextprotocol/protocolVersion` key in `params._meta` | adds `resultType` and `_meta.serverInfo`; `server/discover` and `tools/list` also carry `ttlMs` and `cacheScope` |

Two properties are worth knowing before changing anything here:

- **Era is a pure function of the single frame.** There is no per-process and no per-connection era state, so the same bytes always get the same answer. Revision `2026-07-28` forbids relying on prior requests over the same connection to establish context, and a lock would mean a byte-identical frame gets two different answers depending on history. The official SDK does lock per connection; ADR-016 deliberately diverges.
- **`-32022` never appears in a response to `initialize`.** The handshake negotiates by counter-offer, so an unknown or absent version gets `2025-11-25` back rather than an error. `-32022` is modern-era only.

Primary-source notes, including the exact `_meta` key spellings and the error-code allocation, are in `docs/research/2026-07-29-mcp-2026-07-28-revision.md`.

## Plugin-side deep dives

This guide is the project's own copy. For agents inside Claude Code, the plugin auto-loads richer instructions:

- Plugin path (locale-dependent): `~/.claude/plugins/cache/rvdbreemen-adr-kit/adr-kit/<version>/`
- `instructions/adr.coding.md` — per-developer rules (when to invoke the agent, supersession workflow, Definition of Done).
- `instructions/adr.review.md` — the seven review checks with citation templates.
- `skills/adr/SKILL.md` — full anti-rationalisation guard list, gate definitions, code examples.
- `agents/adr-generator.md` — the subagent prompt.

If you're working outside Claude Code (in a hook, a CI job, or a different agent), this file (`.claude/adr-kit-guide.md`) is your one-stop reference. Keep it in version control with the rest of the project.
