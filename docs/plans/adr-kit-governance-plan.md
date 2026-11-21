# Plan: adr-kit governance improvements

**Origin:** the 2026-07-04 ADR audit of the OTGW-firmware `dev` (2.0.0) line found that
every place ADR data had drifted was a place a human hand-edits: status lines, supersession
links, and the README index. The machine-derivable frontmatter was mostly fine. Concrete
damage found in one audit: **19 ADRs missing from the index, 12 stale status labels, a
duplicated entry, 5 shipped-and-verified ADRs stranded in Proposed, 2 Accepted ADRs whose
Decision contradicted shipped code, and 35 of 43 window ADRs with no machine-readable
frontmatter at all.**

**Guiding principle:** *derive the drift-prone surfaces; never hand-maintain them.* The single
source of truth is each ADR file's frontmatter plus a small amount of structured body. The
index, reciprocal links, status consistency, and gate compliance are generated or linted,
never typed by hand.

**Scope note:** the tooling lives in the separate `adr-kit` repo; the CI wiring, the ADR
files, the `evaluate.py` gates, and the git hooks live in `OTGW-firmware`. Tasks below are
tagged by which repo owns them. This document is the shared plan.

---

## Target architecture

Five capabilities, each replacing a hand-edited surface:

| Capability | Replaces the manual thing that drifted | Repo |
|---|---|---|
| **Canonical schema + `adr migrate`** | prose-only ADRs with no machine-readable state | adr-kit |
| **`adr lint`** | human review of status/link/gate consistency | adr-kit |
| **`adr index`** | the hand-maintained README index | adr-kit |
| **Lifecycle commands** (`propose`/`accept`/`supersede`/`reject`) | hand-typed status lines + reciprocal links + "immutability exception" prose | adr-kit |
| **CI + hooks + skill auto-trigger** | "someone remembers to update the index/run the audit" | OTGW-firmware + adr-kit |

Design invariants:

1. **Status is a computed, validated field, not prose.** `superseded_by` non-empty implies
   `status: Superseded`. Body stays immutable; status/links live in frontmatter, so the
   "sanctioned immutability exception" becomes a property of the schema instead of a sentence
   you hope someone wrote.
2. **Every mutating command ends by regenerating the index** (transactional). The index is
   never stale after a command; `adr index --check` is the CI/pre-commit assertion.
3. **Reciprocity is auto-stamped.** `adr supersede 160 --by 164` writes both files' links.
4. **Auto-accept is allowed only for ADRs that document already-shipped code, and only when
   the evidence pointer resolves.** Forward-looking ADRs keep the human checkpoint.

---

## Workstreams (the tasks)

Ordered by dependency. Each is a backlog task; the epic ties them together.

### WS0 (epic) — governance umbrella
Make the drift-prone ADR surfaces generated + linted + CI-gated. Tracks WS1..WS7.

### WS1 — Canonical frontmatter schema + `adr migrate` (adr-kit)
**Why:** 35/43 ADRs are prose-only, so nothing about them is checkable. This is the root enabler.
**Deliverables:**
- A documented frontmatter schema: `id, title, status, date, tags, supersedes[], superseded_by[], related[], deciders[]`, plus new optional `binding: bool`, `gate: str|null`, `documents_shipped: bool`, `verified_in: [file:symbol|commit]`.
- `adr migrate` that parses the prose `## Status` / `## Status History` of legacy files and back-fills frontmatter, idempotently, leaving the immutable body untouched.
- Schema validation as a library function reused by lint.
**ACs:** every ADR in `docs/adr/` has valid frontmatter after `adr migrate`; migration is a no-op on already-migrated files; body bytes unchanged (diff shows only frontmatter added).

### WS2 — `adr lint` validation engine (adr-kit)
**Why:** replace human review of consistency; make ADR-080 machine-checked.
**Rules:**
- Frontmatter present and schema-valid (WS1).
- `superseded_by` non-empty <=> `status: Superseded`; `status: Rejected` requires a rationale; reciprocity: every `supersedes[x]` has `x.superseded_by` containing self, and vice versa.
- **ADR-080 gate rule:** if `binding: true` and `status: Accepted`, then either `gate` names a function that EXISTS in `evaluate.py`/`tests/`, or the ADR is labeled guideline-level. Lint resolves the named gate against the consuming repo.
- Four quality-gate scoring (Completeness/Evidence/Clarity/Consistency) surfaced as a score; below threshold blocks acceptance, not authoring.
- Optional `verified_in` pointers resolve (file/symbol/commit exists).
**ACs:** `adr lint` exits non-zero on any of: missing frontmatter, status/link contradiction, broken reciprocity, an accepted binding ADR whose gate is absent; `--strict` for CI, plain for local; machine-readable JSON output mode.

### WS3 — `adr index` generator (adr-kit)
**Why:** the README index rotted (19 missing, 12 stale labels, a dup, wrong counts). Generate it.
**Deliverables:**
- `adr index` renders the README from the ADR files: entries grouped by topic (topic taken from a frontmatter `topic`/`tags` mapping), status markers, supersession notes, per-topic counts, all computed.
- `adr index --check` exits non-zero if the generated output differs from the committed file (the CI/pre-commit assertion).
- Preserves the human-authored prose sections (intro, "what are ADRs") via sentinel markers; only the generated tables are owned by the tool.
**ACs:** running `adr index` reproduces a correct README with zero missing ADRs and no dup; `--check` is green immediately after; re-running is idempotent.

### WS4 — Lifecycle commands (adr-kit)
**Why:** humans should never hand-edit a status line, a reciprocal link, or the index.
**Commands (each mutates frontmatter + reciprocals + Status History, then runs `adr index`):**
- `adr propose <slug>` — scaffold a Proposed ADR from a template.
- `adr accept <n> [--by maintainer]` — Proposed -> Accepted, appends a Status History entry, reindex.
- `adr supersede <old> --by <new>` — sets `old.status=Superseded`, `old.superseded_by+=new`, `new.supersedes+=old`, both Status Histories, reindex.
- `adr reject <n> --reason ...` — terminal, kept in index as a trail.
**ACs:** `adr supersede 160 --by 164` produces exactly the two-file edit + index update we did by hand this session; no command leaves the index stale (`adr index --check` green afterward).

### WS5 — After-the-fact ADR fast path (adr-kit)
**Why:** 5 ADRs (153/154/159/161/162) sat in Proposed for weeks only because acceptance is a
silent manual step, though every one said "documents a decision already built." Recording
reality should be low-friction and improve, never degrade, the situation.
**Flow:**
- `adr document <slug>` scaffolds an ADR pre-marked `documents_shipped: true` and REQUIRES a
  `verified_in:` evidence pointer (file:symbol or commit) before it will accept.
- `adr accept` gains a fast path: if `documents_shipped: true` AND every `verified_in` pointer
  RESOLVES AND `adr lint` passes AND the quality-gate score >= threshold, it **auto-accepts**
  (records `changed_by: adr-kit auto-accept (documents-shipped)` + the resolved evidence in
  Status History), because it is recording an existing invariant, not deciding a future one.
**Guardrails (so auto-accept can never rubber-stamp a wrong ADR):**
- Eligible ONLY when `documents_shipped: true`; forward-looking ADRs (`false`/absent) always
  keep the human checkpoint.
- Evidence must resolve against the consuming repo at accept time; a dangling pointer blocks.
- Lint + quality-gate thresholds still apply.
- Configurable strictness: `auto` (accept), or `assist` (stage the acceptance + one-line
  confirm) for maintainers who want a final glance.
**ACs:** an ADR authored with `adr document` + a resolving `verified_in` auto-accepts on a clean
lint; the same ADR with a broken pointer or `documents_shipped:false` does NOT auto-accept.

### WS6 — CI integration + git hooks (OTGW-firmware)
**Why:** turn "someone remembers" into "the build fails if not."
**Deliverables:**
- A GitHub Actions job `adr-governance` on PRs touching `docs/adr/**`: runs `adr lint --strict`
  and `adr index --check`; fails on drift. This is the ADR-080 spirit applied to ADRs themselves.
- Pre-commit hook (`.githooks/`) that runs `adr lint` + `adr index --check` on staged ADR
  changes, mirroring the existing prerelease/commit-msg hook pattern.
- `evaluate.py` gains a thin `check_adr_index_fresh` wrapper (or the CI job stands alone) so the
  existing evaluator story stays coherent.
**ACs:** a PR that edits an ADR status without reindexing fails CI; a PR that adds an ADR
without an index entry fails CI; green once `adr index` is run.

### WS7 — Skill auto-trigger + drift/staleness reporting (adr-kit + skills)
**Why:** the user's ask: whenever ADR skills run, check whether the index needs regenerating
and whether an audit is warranted, and act.
**Deliverables:**
- `adr doctor` — one command that runs lint + `index --check` + a staleness report:
  (a) Proposed ADRs whose `verified_in` resolves (i.e. shipped but not accepted -> nudge the
  fast path), (b) Proposed older than N days, (c) Accepted ADRs whose `verified_in` files
  changed since acceptance (a "re-verify me" flag for code drift like ADR-160/147),
  (d) any ADR-080 gate named but absent.
- **Skill/subagent hook:** the ADR skill and the `adr-generator` subagent run `adr doctor` at
  START (surface pending drift before authoring) and `adr index` + `adr lint` at END
  (auto-fix the index, block on lint errors). If `doctor` reports material drift, the skill
  offers/auto-triggers a full audit pass.
- Optional scheduled `adr doctor` (cron/CI) that opens an issue when drift crosses a threshold.
**ACs:** invoking the ADR skill on a repo with a stale index auto-regenerates it; a shipped-but-
Proposed ADR is surfaced as a fast-path candidate; a scheduled run reports the staleness set.

---

## Phased rollout

1. **Phase A (foundation):** WS1 schema + migrate, WS2 lint. Nothing is enforced yet; lint runs
   in `--warn` mode so the baseline is visible.
2. **Phase B (generation):** WS3 index, WS4 lifecycle commands. Regenerate the index once; from
   here the index is tool-owned.
3. **Phase C (flow):** WS5 after-the-fact fast path. Acceptances stop languishing.
4. **Phase D (enforcement):** WS6 CI + hooks flip lint/index-check to blocking. Drift can no
   longer merge.
5. **Phase E (autonomy):** WS7 `adr doctor` + skill hooks. The system self-checks and nudges.

Enforcement (Phase D) lands only after migration (Phase A) so the baseline is clean, mirroring
how `evaluate.py` gates use a baseline-ratchet in this repo.

---

## Risks and guardrails

- **Auto-accept misuse** — bounded by WS5's four guardrails (documents_shipped flag + resolving
  evidence + lint + quality threshold); forward-looking ADRs are never eligible.
- **Generated-index clobbering human prose** — WS3 uses sentinel markers; the tool owns only the
  generated tables, not the narrative sections.
- **Migration churn** — WS1 is body-preserving and idempotent; the one-time migration PR is
  frontmatter-only and reviewable as such.
- **Two-repo coupling** — lint/index resolve gate names and `verified_in` pointers against the
  consuming repo via a configured path, so adr-kit stays repo-agnostic while OTGW-firmware wires
  the specifics in CI.

---

## Immediate follow-through in THIS repo (independent of adr-kit shipping)

Even before adr-kit ships the commands, OTGW-firmware can bank two of the wins:
- Add the `adr-governance` CI job as a thin script that regenerates + diffs the index (WS6 can
  start with a small Python helper, later swapped for `adr index --check`).
- Normalize the 35 prose-only ADRs' frontmatter (WS1's output), landing the machine-readable
  baseline the rest depends on.
