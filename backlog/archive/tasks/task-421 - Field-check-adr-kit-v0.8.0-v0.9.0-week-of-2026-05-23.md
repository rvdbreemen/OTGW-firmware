---
id: TASK-421
title: Field check adr-kit v0.8.0 + v0.9.0 (week of 2026-05-23)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 20:18'
updated_date: '2026-05-26 10:36'
labels:
  - adr-kit
  - external-repo
  - followup
  - field-verification
dependencies: []
priority: low
ordinal: 5000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Planned 4-week follow-up after the v0.8.0 (schema validation + pre-release smoke test) and v0.9.0 (scoped lint with grandfathering) releases on 2026-04-25.

## Why

Both releases were locally validated. The next signal needed is whether the install path stays clean for new users, whether the lint policy works for projects with diverse ADR templates, and whether any edge case slipped through. This task captures the check so it does not get lost in regular noise.

## When

Pick this up the week of **2026-05-23** (28 days after v0.9.0).

## What to check (all read-only, ~15 minutes total)

1. **GitHub issues since 2026-04-25** on `rvdbreemen/adr-kit`:
   ```
   gh issue list -R rvdbreemen/adr-kit --state all --search "created:>=2026-04-25"
   ```
   Flag mentions of install failure, schema validation, lint output, marker parsing, ADVISORY/FAIL confusion.

2. **CI on `main` branch**:
   ```
   gh run list -R rvdbreemen/adr-kit --workflow validate.yml --limit 5
   ```
   If any failed, summarise.

3. **OTGW-firmware lint baseline drift**:
   - Re-run the smoke test (Python script in TASK-420 final summary, or run `/adr-kit:lint` manually).
   - Baseline at v0.9.0 release: 2 PASS / 80 ADVISORY / 5 FAIL on 87 ADRs.
   - If new ADRs were added between then and the check, recompute and note delta.
   - Drift in either direction is informational; large unexplained drift is a signal.

4. **External adoption signals**:
   ```
   gh search issues "adr-kit:lint" --limit 5
   gh search prs --owner rvdbreemen "adr-kit" --limit 5
   ```
   Note any external adopters (good or bad).

5. **Field-verification issue on adr-kit repo**: linked GitHub issue (this task is its private mirror). Close that issue when this one closes, with the same outcome.

## Possible outcomes

- **GREEN**: no field issues. Close this task. Add one-line note in adr-kit `ROADMAP.md` toward the v1.0.0 "90 days field time" criterion.
- **YELLOW**: minor friction observed. Open separate backlog tasks per item; close this one.
- **RED**: regression confirmed. Open a patch-release task and link it from this one before closing.

## What this is not

- Not a place to do speculative refactoring on adr-kit.
- Not a substitute for triaging real issues as they come in (those should be handled when they arrive, not held until 2026-05-23).
- Not blocking on any other work.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. GitHub issues checken (rvdbreemen/adr-kit, >=2026-04-25)\n2. CI status controleren\n3. Lint baseline meten vs v0.9.0 baseline\n4. Externe adoptsie signalen\n5. GitHub issue #6 sluiten\n6. ROADMAP.md bijwerken\n7. Task afsluiten
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Field check uitgevoerd op 2026-05-26, 31 dagen na v0.9.0.

Versies uitgebracht tussen v0.9.0 en nu:
- v0.10.0: deterministische adr-lint CLI voor CI
- v0.10.1: lint skill user-only
- v0.11.0: /adr-kit:migrate skill voor legacy ADR rewrite
- v0.12.0-0.12.2: three-mode workflow, bold-inline Status fix, brace-expansion globs
- v0.13.0: LLM judge default-on in pre-commit hook
- v0.13.1: quiet pre-commit hook
- v0.13.2: FIX pre-commit exits 1 op clean commits (issue #6)
- v0.13.3: FIX LLM pass op Windows (encoding + shlex)

GitHub issues: Issue #1 (veld-check) al gesloten. Issue #6 (pre-commit bug v0.13.1) gemeld door raafayk7, gerepareerd in v0.13.2, nu gesloten.

CI: Alle recente runs (v0.13.3) SUCCESS. Een failure op 2026-05-06 tijdens v0.13.x development, daarna groen.

Lint baseline:
- Dev-worktree (79 ADRs, ADR-001 t/m ADR-080): 79 PASS / 0 ADVISORY / 0 FAIL (was 2/80/5 op 87 ADRs)
- 2.0.0-worktree (111 ADRs): 18 PASS / 79 ADVISORY / 14 FAIL (de 14 FAILs zijn nieuwe ADRs met incomplete secties)

Externe adoptie: Lenivvenil/claude-mini opende issue #138 voor adr-kit MCP integratie (positief signaal). kschlt heeft een fork met MCP-specifieke issues.

Acties uitgevoerd: Issue #6 gesloten met commentaar op v0.13.2 fix. ROADMAP.md bijgewerkt met field check resultaat.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Field check v0.8.0/v0.9.0 — verdict: YELLOW/GREEN.

Verdict: YELLOW voor de 2.0.0-worktree (14 FAILs van nieuwe ADRs met incomplete secties, geen adr-kit regressie), GREEN voor de dev-worktree (79/79 PASS).

Bevindingen:
- adr-kit is doorontwikkeld van v0.9.0 naar v0.13.3 (9 releases in 31 dagen)
- CI groen op v0.13.3
- Issue #6 (pre-commit exits 1 op clean commits, v0.13.1) gemeld door externe gebruiker en opgelost in v0.13.2 binnen 24h; nu gesloten
- Externe adoptiesignaal: Lenivvenil/claude-mini integreert adr-kit via MCP

Acties:
- GitHub issue #6 gesloten (fix was al in v0.13.2)
- ROADMAP.md bijgewerkt in rvdbreemen/adr-kit met field check resultaat
- De 14 FAILs in 2.0.0-worktree zijn separate follow-up (nieuwe ADRs invullen)
<!-- SECTION:FINAL_SUMMARY:END -->
