---
id: TASK-624
title: >-
  Fix PR#576 — HA PIC-control entities (pseudo-ID 251): rebase on current dev +
  fix infinite-retry bug
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-18 07:53'
updated_date: '2026-05-18 07:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PR#576 adds HA MQTT discovery for resetgateway (button), gpioa/gpiob + leda-ledf (selects) plus the matching MQTT set-commands. It is stuck: (a) mergeable_state dirty — built against old dev (beta.3); the conflict is purely the per-file version-stamp drift vs current dev (beta.8); (b) an unresolved reviewer finding: pseudo-ID 251 discovery is gated behind isPICEnabled() in doAutoConfigureMsgid, so on PIC-disabled devices doAutoConfigureMsgid returns false forever and loopMQTTDiscovery retries 251 every drip tick indefinitely. Re-apply the feature cleanly on current dev and fix the bug by publishing 251 discovery unconditionally, matching the established PIC pseudo-ID 249/250 convention (MQTT_HA_FLAG_IS_PIC_ENTRY: entity always discovered, data PIC-gated at source per ADR-065).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PR#576 feature re-applied on current dev: resetgateway button + gpioa/gpiob/leda-ledf select discovery (pseudo-ID OTGWpiccontrolsid=251) and MQTT set-commands (GA/GB/LA-LF/SC + resetgateway via s_reset->resetOTGW()) integrated with current dev's MqttJsonWriter streaming infra
- [ ] #2 Infinite-retry bug fixed: pseudo-ID 251 discovery publishes unconditionally (no isPICEnabled() gate in doAutoConfigureMsgid); loopMQTTDiscovery clears the pending bit on success; behaviour parallels existing PIC pseudo-IDs 249/250
- [ ] #3 Version bumped per policy via bin/bump-prerelease.sh (beta.8 -> beta.9); version.h + data/version.hash staged with the firmware change
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures vs dev baseline
- [ ] #6 docs/api/MQTT.md updated with new set-commands + PIC Control Entities (pseudo-ID 251) section
- [ ] #7 Committed and pushed to claude/fix-pr576-port-2.0.0-GvEv0; draft PR opened targeting dev; PR#576 cross-referenced
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. MQTTstuff.h: add HaIcon restart/pin/led_outline before _count; declare streamButtonDiscovery + streamSelectDiscovery
2. mqtt_configuratie.cpp: add haIconStr cases (restart/pin/led-outline) + haEntityCatStr config case; add composeButtonPayload/streamButtonDiscovery + composeSelectPayload/streamSelectDiscovery (PR576 code, current-dev helper API)
3. MQTTstuff.ino: add s_reset + s_cmd_*/s_otgw_* PROGMEM + setcmds rows; resetgateway->resetOTGW() branch in handleMQTTcallback; OTGWpiccontrolsid wired into publishNonOTDiscoveryConfigs + markAllMQTTConfigPending + clearMQTTConfigPending; doAutoConfigureMsgid pseudo-ID 251 block WITHOUT isPICEnabled() gate (fix)
4. OTGW-firmware.h: add OTGWpiccontrolsid=251 + comment
5. docs/api/MQTT.md: set-commands rows + PIC Control Entities section
6. bin/bump-prerelease.sh (beta.8->beta.9); stage version.h + data/version.hash
7. python build.py --firmware (exit 0); python evaluate.py --quick (no new failures)
8. commit, push -u origin claude/fix-pr576-port-2.0.0-GvEv0, draft PR -> dev, cross-ref #576
<!-- SECTION:PLAN:END -->
