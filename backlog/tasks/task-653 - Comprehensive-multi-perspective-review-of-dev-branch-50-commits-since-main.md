---
id: TASK-653
title: Comprehensive multi-perspective review of dev branch (50 commits since main)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 05:37'
updated_date: '2026-05-22 05:54'
labels:
  - review
  - dev-branch
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Broad review of origin/main..origin/dev (50 commits, ~17k+/-23k LOC) covering all relevant perspectives: code correctness, security, ESP8266 platform compliance (PROGMEM/RAM/heap/yield), ADR-compliance, frontend/UI, release-/CI-pipeline, docs/guides, and backlog hygiene. Findings to be consolidated into chat summary plus this backlog task; follow-up fixes become separate tasks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Code-correctness review completed and findings documented
- [x] #2 Security review completed and findings documented
- [x] #3 ESP8266-platform review (PROGMEM/RAM/heap/yield/static-buffer) completed and findings documented
- [x] #4 ADR-compliance check against all Accepted ADRs touching changed code completed
- [x] #5 Frontend/UI review (FSexplorer touch fix, HA discovery JS, data/*) completed
- [x] #6 Release- and CI-pipeline review (beta-prerelease workflow, evaluate.py, build.py submodule fix, versioning policy) completed
- [x] #7 Docs/integration-guides review (openHAB, Domoticz, Dutch MQTT cleanup, daily issue reports) completed
- [x] #8 Backlog hygiene review completed (every dev commit traceable to a TASK; completed tasks have Final Summary; no orphan In Progress)
- [x] #9 Findings consolidated in Final Summary with severity + location + suggested follow-up tasks
- [x] #10 Follow-up backlog tasks created for each actionable High/Medium finding
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Gather diff slices per perspective (file groups + key commits).
2. Spawn 4 parallel review agents:
   a. Code correctness + security + ADR-compliance (high-signal engineering)
   b. ESP8266 platform compliance (PROGMEM/RAM/yield/static-buffers)
   c. Frontend/UI + release/CI pipeline
   d. Docs/integration-guides + backlog hygiene
3. Consolidate reports into severity-ranked finding list.
4. Create follow-up backlog tasks for actionable High/Medium findings.
5. Add Final Summary with chat-pasteable consolidation.
6. Mark Done.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Comprehensive multi-perspective review of `origin/main..origin/dev` (50 commits, ~17k+/-23k LOC, large delete is SAT-* relocated to 2.0.0 worktree). Four parallel review agents covered: (A) Code correctness + Security + ADR-compliance, (B) ESP8266-platform discipline, (C) Frontend/UI + Release/CI pipeline, (D) Docs + Backlog hygiene.

## Risk verdict — Medium

Dominated by one HIGH finding: **ADR-074 is Accepted but its mandated code change was never made.** The DHW Control / Thermostat unavailable-flap that ADR-074 exists to fix continues to ship to beta testers. `bin/adr-judge` would have blocked this if installed — but it's not present in this worktree.

ESP8266 platform compliance is clean (evaluator 34/36 pass, 0 fail). No new HTTPS/WSS/TLS, no `String` on hot paths, no ArduinoJson, no PROGMEM-pointer leaks in the disciplined 247-line PIC-control discovery additions (`mqtt_configuratie.cpp`). ADR-075 (proxy-A routing), ADR-076 (rate-gate removal), ADR-078 (HA-core aliases revert) all match their Decision text in code. Cross-worktree hygiene clean — no 2.0.0 task accidentally Done on dev.

## Findings by severity

**HIGH (5 — actionable, follow-up tasks created)**

| # | Finding | Location | Follow-up |
|---|---------|----------|-----------|
| H1 | ADR-074 violated: two `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(...))` writes still present despite Accepted ADR mandating their removal | `OTGW-Core.ino:4044`, `MQTTstuff.ino:1158` | TASK-654 |
| H2 | ADR README index stops at ADR-073; ADR-074/075/076/077/078 missing + 4 meta verification files stranded at `docs/adr/` root + ADR-067 non-standard "Deprecated, withdrawn" label | `docs/adr/README.md` | TASK-655 |
| H3 | Beta-prerelease workflow "draft-first" flips draft→published unconditionally; `workflow_dispatch` accepts unrestricted `ref` input (any PR head can be published) | `.github/workflows/beta-prerelease.yml:57-72, 304-307` | TASK-656 |
| H4 | 830 KB field-log `OTGW_1.6_beta_7.txt` still in repo (commit 67e63d97); `.gitignore` doesn't cover `OTGW_*.txt` pattern that TASK-635 was trying to defend against | repo root | TASK-657 |
| H5 | 5 completed-but-In-Progress tasks (TASK-633, 637, 652, 631, 607) + TASK-626 ADR-075 work shipped in beta.16 still In Progress; 6 long-stale In Progress tasks (397/431/549/556/571/385/387) untouched 14+ days during active window; ~70% of dev commits lack `TASK-NNN` reference in subject | `backlog/tasks/` | TASK-658 (cleanup) + TASK-659 (commit-msg hook) |

**MEDIUM (3 — actionable, follow-up tasks created)**

| # | Finding | Location | Follow-up |
|---|---------|----------|-----------|
| M1 | `evaluate.py` `sendMQTTheapdiag` check WARN→PASS unconditionally when no `char[N]` buffer found — future regression with `snprintf` re-added without buffer would PASS silently; `excused_markers` regex matches any line containing "2.0.0" (escape hatch too wide); `.githooks/pre-commit` bump-check doesn't enforce `data/version.hash` is staged when `version.h` `_VERSION_PRERELEASE` changes | `evaluate.py:616-625, 1060-1068`, `.githooks/pre-commit:60-77` | TASK-660 |
| M2 | `resetgateway` MQTT command unauthenticated + payload-blind + un-rate-limited — any LAN client can induce PIC hardware reset; consistent with ADR-032 but expands blast radius from "tune behaviour" to "induce reset" | `MQTTstuff.ino:704-707` | TASK-661 |
| M3 | `mqttPendingSlot` commit-or-discard pattern counts ANY publish success in delta window — sibling source-topic success can incorrectly commit canonical pending slot when canonical send itself failed, deferring missed canonical to next interval | `OTGW-Core.ino:3719-3735, 4205-4213` | rolled into TASK-660 scope (annotate or fix) |

**LOW (5 — listed in findings appendix below, no individual tasks)**

| # | Finding | Location |
|---|---------|----------|
| L1 | `FSexplorer.html` 20 KB / `index.html` 15 KB past 10KB "must stream" threshold; deltas zero in this window but rule auditable | `data/FSexplorer.html`, `data/index.html` |
| L2 | Daily issue reports overwrite single shared `docs/daily-issue-report.md`; 2026-05-17/18/19 recoverable only via `git log` — pick "ignore" or "archive per day" | `docs/daily-issue-report.md` |
| L3 | openHAB/Domoticz guides reference non-existent "migration notes" heading in `docs/api/MQTT.md` | `docs/guides/OPENHAB.md:70`, `DOMOTICZ.md:72` |
| L4 | First OT message after boot (`cntOTmessagesprocessed==1`) bypasses override-detection path; one-shot cosmetic drift at boot only | `OTGW-Core.ino:4070-4106` |
| L5 | `delayms()` fix is correct (commit 05e777bf) — pre-fix used `DECLARE_TIMER_MS` which captured interval once via `static`, freezing the wait at first caller's `delay_ms`; no callers depended on broken behaviour | `helperStuff.ino:1180-1188` |

**INFO / CLEAN**

- ESP8266 platform (10 rules): 0 violations
- ADR-075 / ADR-076 / ADR-078 / ADR-070 / ADR-071 / ADR-072 / ADR-042: clean
- No new HTTPS / WSS / TLS / String on hot paths / ArduinoJson / `new` / `malloc`
- Cross-worktree backlog: no 2.0.0 task Done on dev
- FSexplorer media-query fix (#575) browser-safe (CSS L4 baseline)
- `version.hash` cache-bust gate auto-rewritten by `autoinc-semver.py` — works
- Submodule auto-init in `build.py` scoped + safe (not `--recursive`)
- `.gitignore` `/logfile.*` + `/.codex` patterns correctly anchored

## Follow-up tasks created

- TASK-654 (HIGH) — Implement ADR-074
- TASK-655 (HIGH) — ADR README + meta-files + ADR-067 status
- TASK-656 (HIGH) — Beta-prerelease workflow hardening
- TASK-657 (HIGH) — Repo hygiene `OTGW_*.txt` + `.gitignore`
- TASK-658 (HIGH) — Backlog admin: 6 flip-to-Done + 6 stale triage
- TASK-659 (HIGH) — commit-msg hook for `TASK-NNN`
- TASK-660 (MEDIUM) — `evaluate.py` + bump-check hardening
- TASK-661 (MEDIUM) — `resetgateway` MQTT hardening

## Ship recommendation

**Block beta.17 on TASK-654 (ADR-074 implementation).** Everything else can land sequentially. The ADR-judge omission (TASK-659 covers the structural fix) is a process gap that allowed this to slip — installing the hook will prevent recurrence.

Build green: `python evaluate.py --quick` reports Health 100% (34 pass / 0 fail / 2 info). `python build.py --firmware` not run in this review session (read-only).
<!-- SECTION:FINAL_SUMMARY:END -->
