---
id: TASK-421
title: Field check adr-kit v0.8.0 + v0.9.0 (week of 2026-05-23)
status: To Do
assignee: []
created_date: '2026-04-25 20:18'
updated_date: '2026-04-30 00:37'
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
