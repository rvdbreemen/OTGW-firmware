---
id: TASK-625
title: >-
  feat-2.0.0: port TASK-624 — HA PIC-control entities (resetgateway button +
  GPIO/LED selects) on 2.0.0 line
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-18 07:54'
updated_date: '2026-05-18 08:38'
labels: []
dependencies:
  - TASK-624
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the PR#576 feature (TASK-624) to the feature-dev-2.0.0-otgw32-esp32-sat-support line. The 2.0.0 line keeps HA discovery in MQTTHaDiscovery.cpp (different file/conventions from dev's mqtt_configuratie.cpp) and is at version alpha.36. Apply the same architectural decision as TASK-624: publish the PIC-control discovery configs unconditionally (no isPICEnabled() gate) matching that branch's existing PIC pseudo-ID convention, so there is no infinite-retry. Use a free pseudo-ID on that branch (PR#576 description suggested 244; verify against MQTTHaDiscovery.cpp before choosing). Delivered on a separate branch off feature-dev-2.0.0-otgw32-esp32-sat-support with its own draft PR targeting that branch.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Feature ported into MQTTHaDiscovery.cpp using the 2.0.0 branch's streaming/discovery conventions: resetgateway button + gpioa/gpiob/leda-ledf selects + MQTT set-commands (GA/GB/LA-LF/SC + resetgateway)
- [x] #2 Discovery configs publish unconditionally (no isPICEnabled() gate) consistent with the 2.0.0 branch's existing PIC pseudo-ID handling; no infinite-retry
- [x] #3 A free pseudo-ID is chosen and verified unused on the 2.0.0 branch; ESP8266 + ESP32-S3 considerations reviewed
- [x] #4 Version bumped per policy on the 2.0.0 line (alpha.36 -> alpha.37)
- [ ] #5 build green for the 2.0.0 target; evaluator shows no new failures vs that branch baseline
- [x] #6 docs/api/MQTT.md (2.0.0) updated with the new entities/commands
- [x] #7 Delivered on a dedicated branch off feature-dev-2.0.0-otgw32-esp32-sat-support with a draft PR targeting that branch; cross-referenced to TASK-624 / PR#576
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-firmware.h: add OTGWpiccontrolsid=244 + comment
2. MQTTstuff.h: add HaIcon restart/pin/led_outline before _count; declare streamButtonDiscovery + streamSelectDiscovery
3. MQTTHaDiscovery.cpp: add haIconStr restart/pin/led-outline cases; ADD MISSING haEntityCatStr config case (2.0.0 bug); append composeButtonPayload/streamButtonDiscovery + composeSelectPayload/streamSelectDiscovery (port of dev code, 2.0.0 helper API)
4. MQTTstuff.ino: add s_reset + s_cmd_*/s_otgw_* PROGMEM + setcmds rows; resetgateway->resetOTGW() branch in handleMQTT dispatch; OTGWpiccontrolsid into markAllMQTTConfigPending (parity with 249/250/diag200 — NOT publishNonOT, matching 2.0.0 convention); doAutoConfigureMsgid pseudo-ID 244 block WITHOUT isPICEnabled gate
5. docs/api/MQTT.md (2.0.0): set-commands + PIC Control Entities section
6. bin/bump-prerelease.sh (alpha.36->alpha.37)
7. build + evaluate for 2.0.0 target
8. commit, push -u origin claude/port-pr576-2.0.0-GvEv0, draft PR -> feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PR #597 review/CI follow-up: mirrored the dev PR #596 all-or-nothing fix to the pseudo-ID 244 block (commit f9dc483c, bump alpha.37->alpha.38). #597 CI failures (evaluate.py, pio run -e esp8266, Spec-driven OT v4.2 audit, pio run -e esp32) are PRE-EXISTING on the 2.0.0 line: PR #585 — whose merge commit c21f28db is this port branch base — shows the identical four red checks and was merged anyway. esp8266 build verified locally green; evaluator identical to pristine baseline (97.1%/1 pre-existing PROGMEM fail). No new failures introduced.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported PR#576 / dev PR #596 to the 2.0.0 line on branch claude/port-pr576-2.0.0-GvEv0; draft PR #597 -> feature-dev-2.0.0-otgw32-esp32-sat-support.

Changes: streamButtonDiscovery + streamSelectDiscovery in MQTTHaDiscovery.cpp (2.0.0 lambda-compose idiom, modeled on streamSatSelectDiscovery); pseudo-ID OTGWpiccontrolsid=244 (only free slot; 245-255 taken); wired into doAutoConfigureMsgid + markAllMQTTConfigPending. setcmds[] gains gpioa/gpiob->GA/GB, leda-ledf->LA-LF; resetgateway via new s_reset type -> resetOTGW() (#if HAS_PIC-guarded, OTGW32 no-op). Discovery publishes unconditionally (no isPICEnabled() gate) per the 2.0.0 TASK-543 decision; same fix as dev. docs/api/MQTT.md updated.

Deliberate divergences from #596: setclock omitted (2.0.0 already has timesync->SC, different payload); no HaIcon/haEntityCatStr changes (2.0.0 writes icon/category inline via PSTR).

Validation: build.py --firmware --target esp8266 succeeded (2.0.0-alpha.37+c21f28d). evaluate.py --quick = 97.1% / 1 failed = IDENTICAL to pristine baseline (pre-existing PROGMEM violation, unrelated) -> no new failures. Version bumped alpha.36->alpha.37.

BLOCKING AC (AC5 esp32): ESP32 target could NOT be built here — ESP32 Arduino core download from github.com blocked by sandbox network policy (toolchain fetch, not a code defect). Changes are platform-neutral; resetOTGW() #if HAS_PIC-guarded. ESP32 build deferred to PR CI (networked). Task left In Progress pending that CI green.

Tooling note: 2.0.0 git worktree cannot host signed commits (/tmp/code-sign returns 400 "missing source" in a linked worktree); port committed from the main checkout path where signing works. No --no-gpg-sign bypass used.
<!-- SECTION:FINAL_SUMMARY:END -->
