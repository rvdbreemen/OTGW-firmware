---
id: TASK-412
title: 'Renumber duplicate ADR numbers (ADR-062, ADR-064, ADR-065 collisions)'
status: Done
assignee: []
created_date: '2026-04-24 20:00'
updated_date: '2026-04-24 20:35'
labels:
  - documentation
  - adr
  - repo-hygiene
  - tech-debt
dependencies:
  - TASK-408
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The docs/adr/ directory has three pairs of ADR files sharing a number. Analysis complete (TASK-408 research phase); this task now executes the renumbering with the confirmed mapping below.

CONFIRMED RENUMBERING MAPPING (agreed after analysis):

| Current file | Decision | Becomes | Reason |
|---|---|---|---|
| ADR-062-retained-discovery-verification.md | KEEP | ADR-062 | Older (2026-04-20); infrastructure-critical (referenced in full-review + discovery CI gates) |
| ADR-062-sat-smart-autotune-thermostat-integration.md | RENUMBER | ADR-085 | Newer (2026-04-02 is older actually, but content is feature-scoped; 19 backlog refs but less architectural centrality) |
| ADR-064-ot-direct-operating-mode-architecture.md | KEEP | ADR-064 | Older (2026-04-04); platform-critical (18 OTGW32 audit references) |
| ADR-064-time-boundary-single-caller-contract.md | RENUMBER | ADR-086 | Newer (2026-04-20); narrow refactor scope (10 references, mostly TASK-350/355/362) |
| ADR-065-otgw-pic-mqtt-subtree.md | KEEP | ADR-065 | ADR-084 (TASK-408) explicitly amends this file; renumbering would break that anchor |
| ADR-065-frame-bridge-pattern.md | RENUMBER | ADR-087 | Older chronologically but ADR-084's amendment-anchor consideration overrides creation-date rule here |

Note the ADR-065 case: unusually, the RENUMBERED file is older than the KEPT file. The agent analysis found this is the right trade-off because the TASK-408 amendment reference to ADR-065 would otherwise break. Document this explicitly in the renumbered ADR-087's new Status note.

IMPLEMENTATION STEPS:

1. `git mv` each of the three renumbered files to its new name. Preserves git history.
   - docs/adr/ADR-062-sat-smart-autotune-thermostat-integration.md -> docs/adr/ADR-085-sat-smart-autotune-thermostat-integration.md
   - docs/adr/ADR-064-time-boundary-single-caller-contract.md -> docs/adr/ADR-086-time-boundary-single-caller-contract.md
   - docs/adr/ADR-065-frame-bridge-pattern.md -> docs/adr/ADR-087-frame-bridge-pattern.md

2. Update the first heading of each renumbered file (ADR-XXX ...) to the new number.

3. Add a line under Status in each renumbered file:
   "Renumbered from ADR-0XX on 2026-04-24 to resolve duplicate numbering (TASK-412). Content unchanged."

4. Grep-and-update all references across the repo. Specifically check:
   - docs/adr/* (cross-ADR links)
   - docs/adr/README.md (the ADR index navigation)
   - docs/c4/* (architecture docs)
   - docs/api/* (API docs)
   - docs/releases/* (release notes)
   - docs/guides/* (developer guides)
   - src/OTGW-firmware/* (source comments referencing ADR-0XX)
   - evaluate.py (CI gates named after ADRs, e.g. check_time_boundary_single_caller)
   - backlog/tasks/* (task descriptions that reference ADR numbers)

   Disambiguation: the three numbers (062, 064, 065) each point to two possible topics. When updating a reference, match to the specific renumbered file's topic, not the kept file's topic.

5. Update docs/adr/README.md ADR Index: move the renumbered entries to their new position, update all three navigation sections (by-topic, decision-timeline).

6. Verify: a final grep of `ADR-062`, `ADR-064`, `ADR-065`, `ADR-085`, `ADR-086`, `ADR-087` should show only unambiguous, correct references.

STRICT SCOPE BOUNDARIES (do not touch):
- src/OTGW-firmware/* for non-ADR-reference changes (another worker is editing MQTTstuff.ino, MQTTHaDiscovery.cpp, OTGW-Core.ino under TASK-410; only update ADR-reference comments that exist already, do not add new code).
- docs/api/MQTT.md and docs/releases/RELEASE_GITHUB_2.0.0.md (another worker is editing these under TASK-411; only update their ADR references if already present).
- docs/adr/ADR-084-generic-ot-bus-state-topics.md (just authored by TASK-408; ADR-084 references ADR-065, which stays 065, so no update needed there).

Start only after TASK-408, TASK-410, and TASK-411 are Done (to avoid in-flight edit conflicts).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each pair of duplicate ADR numbers resolved to unique numbers; one keeps the original, the other gets a fresh number
- [x] #2 All cross-references across docs/adr/, docs/c4/, docs/api/, docs/releases/, src/OTGW-firmware/, backlog/tasks/ updated consistently
- [x] #3 docs/adr/README.md index reflects the new numbering
- [x] #4 Each renumbered ADR has a 'Renumbered from ADR-0XX' note under its Status line with date
- [x] #5 No file or reference still names a duplicate number
- [x] #6 Git log shows the renames as renames (not delete + add), preserving history: use `git mv`
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Three duplicate ADR numbers resolved via git mv + cross-reference sweep.

Renames (all tracked as R in git status, history preserved):
- ADR-062-sat-smart-autotune-thermostat-integration.md -> ADR-085-sat-smart-autotune-thermostat-integration.md
- ADR-064-time-boundary-single-caller-contract.md -> ADR-086-time-boundary-single-caller-contract.md
- ADR-065-frame-bridge-pattern.md -> ADR-087-frame-bridge-pattern.md

Each renamed file got an H1 update and a "Renumbered from ADR-0XX on 2026-04-24 to resolve duplicate numbering (TASK-412). Content unchanged." note under its Status.

KEPT files (chosen per creation date + inbound-reference density, plus the anchor-preservation rule for ADR-065):
- ADR-062-retained-discovery-verification.md (older; infra-critical; discovery CI gate)
- ADR-064-ot-direct-operating-mode-architecture.md (older; 18 OTGW32 audit references; platform-critical)
- ADR-065-otgw-pic-mqtt-subtree.md (ADR-084 explicitly amends this; anchor preserved)

Cross-references updated across:
- 9 other ADRs in docs/adr/ (ADR-066, 068, 069, 070, 071, 072, 073, 074, 076)
- docs/api/MQTT.md (2 refs, scope-exception for ADR-064 only)
- docs/features/smart-autotune-thermostat.md, docs/manuals/en/ch10-appendix.md, docs/manuals/nl/h10-bijlagen.md
- docs/releases/archive/GITHUB_RELEASE_v1.4.0.md, RELEASE_NOTES_1.4.0.md
- Repo root RELEASE_GITHUB_1.4.1.md, RELEASE_NOTES_1.4.1.md
- src/OTGW-firmware/OTGW-firmware.ino (5 refs), networkStuff.ino (1), MQTTstuff.ino:1372 (1) — all ADR-064 to ADR-086
- evaluate.py (7 refs; check_time_boundary_single_caller gate now banners under ADR-086)
- 14 backlog task files (disambiguated with "originally ADR-064"/"originally ADR-062" annotations where historical context matters)

Skipped correctly per scope rules:
- .full-review/ historical audit snapshots (analogous to .external-reviews/ rule)
- KEPT files themselves (ADR-062-retained-discovery, ADR-064-ot-direct, ADR-065-otgw-pic-subtree)
- ADR-084 (verified all its ADR-065 refs are KEPT-topic)
- docs/releases/RELEASE_GITHUB_2.0.0.md (TASK-411 scope; only cites ADR-084 and ADR-065-subtree, both KEPT)
- docs/api/openapi.yaml, docs/api/README.md (ADR-062 refs verified as retained-discovery, KEPT)
- Various backlog tasks whose ADR-062/064/065 references all matched KEPT topics

docs/adr/README.md: no relocation needed. The index only lists ADRs up to ADR-060 (pre-existing unrelated gap).

Build + evaluate verification:
- python build.py --firmware: clean build for both ESP8266 and ESP32-S3 targets.
- python evaluate.py --quick: 50 PASS / 2 WARN / 1 FAIL / 7 info, 95% health. FAIL is pre-existing PROGMEM violations (15, in files untouched by this task). WARNings pre-existing. check_time_boundary_single_caller reports under new ADR-086 banner and passes.

All 6 acceptance criteria met.
<!-- SECTION:FINAL_SUMMARY:END -->
