---
id: TASK-495
title: >-
  docs(release,api,adr,webui): close Phase 3 doc + ADR drift Highs (3B-H1/H2/H3,
  3C-1/2/3)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 17:56'
updated_date: '2026-04-30 18:04'
labels:
  - docs
  - adr
  - webui
  - code-review
  - follow-up
dependencies:
  - TASK-487
  - TASK-488
  - TASK-494
  - TASK-466
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Origin

Six fix-before-release findings from Phase 3 of the comprehensive review
(test/docs/ADR audit) of session work `ace21a48..a61373b9`. Bundled into
one task because they're all docs/ADR work, all small (~minutes to ~30
min each), and all close measurable drift between architectural record
and shipped code.

## Findings being addressed

- **3B-H1**: WebUI tooltip on `iBleInterval` (`data/index.js:5833` and
  `:5948`) still says "scan rate". Post-TASK-494 the actual semantics
  is publish/state-update cadence. Tooltip is now actively misleading.
- **3B-H2**: New per-MAC MQTT topic family
  `OTGW/sat/ble/<mac>/{temp,rh,bat,rssi}` (TASK-488) is absent from
  `docs/api/MQTT.md`. Non-HA integrators have no documented contract.
- **3B-H3**: `CHANGELOG.md` `[Unreleased]` does not mention any of the
  8 session commits, including the user-visible TT/TC behavioural
  change. Release-blocking gap once 2.0.0 cuts a build.
- **3C-1 (ADR-090 drift)**: ADR-090 sub-rule 4 says "no volatile
  required, cooperative single-threaded model" with an explicit
  foresight clause that FreeRTOS would invalidate it. TASK-497 added
  `portMUX_TYPE _bleSensorsMux` cross-task on ESP32-S3. Foresight
  clause fired; ADR is unamended.
- **3C-2 (ADR-092 drift)**: ADR-092 says "Periodic-scan model
  retained" with `BLE_SCAN_DURATION_SEC = 3`. TASK-494 (same release)
  ripped both out for continuous-scan.
- **3C-3 (ADR-068 drift + coalesce semantics)**: ADR-068 documents
  `OT_CMD_QUEUE_SIZE = 8`; code is 12 (pre-existing drift). TASK-494
  added coalesce-by-MsgID semantics, uncovered by any ADR. The
  TASK-494 plan-file claim that "ADR-064 / ADR-087 already cover the
  queue model" was incorrect — ADR-068 is the right neighbour, and it
  does not cover coalesce.

## Files modified

| File | Change |
|---|---|
| `CHANGELOG.md` | Add `[Unreleased]` entries for the 8 session commits, calling out user-visible behaviours (TT/TC PIC-parity, BLE continuous scan, queue coalesce) |
| `src/OTGW-firmware/data/index.js` | Rewrite the `iBleInterval` tooltip (lines 5833 and 5948) to "publish/state-update cadence" |
| `docs/api/MQTT.md` | Add the per-MAC BLE topic family with payload examples |
| `docs/adr/ADR-090-*.md` | Amend sub-rule 4: cross-task FreeRTOS pattern is now in scope; reference TASK-497 portMUX |
| `docs/adr/ADR-092-*.md` | Amend "Periodic-scan model retained" to "Continuous-scan model" with TASK-494 reference; iBleInterval semantics shift documented |
| `docs/adr/ADR-068-*.md` | Amend `OT_CMD_QUEUE_SIZE` to 12; add coalesce-by-MsgID section with safety scope and the queue-is-the-channel principle |

(If ADR-068 file does not exist or has a different number, audit the actual filename via Glob and adjust.)

## Out of scope

- `tests/test_otdirect_override.cpp` (3A-H1) — separate task, ~1-2 h work.
- Documentation Mediums and Lows from Phase 3B — defer to a docs-sweep follow-up.

## Validation

- ESP32 build clean (no firmware behaviour change; only data/index.js + docs).
- ESP8266 build clean.
- python tests/check_otdirect_fixture.py PASS.
- python evaluate.py --quick: zero new violations.
- ADR amendments pass the four verification gates (Completeness, Evidence, Clarity, Consistency) per CLAUDE.md ADR section.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 3B-H1: `data/index.js` `iBleInterval` tooltip on lines 5833 and 5948 reads `publish/state-update cadence` (not `scan rate`)
- [x] #2 3B-H2: `docs/api/MQTT.md` documents the per-MAC topic family `OTGW/sat/ble/<mac>/{temp,rh,bat,rssi}` plus the `<HaPrefix>/sensor/<uniqueId>_ble_<mac>_<kind>/config` discovery topic with payload examples and retain semantics
- [x] #3 3B-H3: `CHANGELOG.md` `[Unreleased]` section lists the 8 session commits with user-visible behaviour changes called out
- [x] #4 3C-1: ADR-090 sub-rule 4 amended with FreeRTOS cross-task case + TASK-497 reference
- [x] #5 3C-2: ADR-092 amended to reflect continuous-scan + iBleInterval semantics shift; TASK-494 referenced
- [x] #6 3C-3: ADR-068 amended (or new ADR-093 created) for `OT_CMD_QUEUE_SIZE = 12` correction + coalesce-by-MsgID semantics + queue-is-the-channel principle
- [x] #7 ESP32 build clean (no firmware behaviour change)
- [x] #8 ESP8266 build clean
- [x] #9 python evaluate.py --quick zero new violations
- [x] #10 All amended ADRs retain Status=Accepted with the four verification gates discharged
- [x] #11 TASK-494 description corrected (or annotated) to reference ADR-068 instead of ADR-064/ADR-087
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-495 — Phase 3 docs + ADR drift Highs closed

Six fix-before-release findings from Phase 3 of the comprehensive
review (test/docs/ADR audit) of session work `ace21a48..a61373b9`,
landed in commit `ccee66ed`.

### Fixes
- **3B-H1** (WebUI iBleInterval tooltip): `data/index.js` lines 5833 + 5948
  rewritten to publish/state-update semantics; explicitly notes the
  continuous-scan model.
- **3B-H2** (per-MAC MQTT topics in API docs): new subsection in
  `docs/api/MQTT.md` documenting `<TopTopic>/sat/ble/<mac>/{temp,rh,bat,rssi}`
  plus the four HA-discovery configs with retain semantics and the
  `bDiscoveryPublished` retry contract.
- **3B-H3** (CHANGELOG): `[Unreleased]` populated with Added / Changed /
  Fixed entries for all 8 session commits.
- **3C-1** (ADR-090 amendment): sub-rule 4's foresight clause about
  FreeRTOS firing — amended with the cross-task case + portMUX snapshot
  pattern + SATble.ino exemplar.
- **3C-2** (ADR-092 amendment): "Periodic-scan model retained" replaced
  with continuous-scan; iBleInterval semantics shift documented.
- **3C-3** (ADR-068 amendment): `OT_CMD_QUEUE_SIZE` corrected from 8 to
  12; new Queue mechanics section covers queue-is-the-channel principle,
  coalesce-by-MsgID semantics, high-water diagnostic, and safety scope.
  TASK-494 plan-file claim about ADR-064/ADR-087 corrected — ADR-068 is
  the right neighbour.

### Verification
- ESP32 build SUCCESS (1m38s, build 3861).
- ESP8266 build SUCCESS (38s, byte-identical RAM/Flash).
- `python tests/check_otdirect_fixture.py` PASS.
- `python evaluate.py --quick` 95.5% health, zero new violations.
- All amended ADRs retain Status=Accepted with the four verification
  gates discharged.

All 11 ACs satisfied.
<!-- SECTION:FINAL_SUMMARY:END -->
