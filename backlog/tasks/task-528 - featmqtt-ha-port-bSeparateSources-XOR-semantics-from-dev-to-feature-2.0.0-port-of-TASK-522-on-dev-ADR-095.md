---
id: TASK-528
title: >-
  feat(mqtt-ha): port bSeparateSources XOR semantics from dev to feature-2.0.0
  (port of TASK-522 on dev / ADR-095)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-03 16:50'
labels:
  - mqtt
  - ha-discovery
  - port-forward
  - adr-095
dependencies:
  - TASK-522
references:
  - >-
    docs/adr/ADR-095-bseparatesources-mutually-exclusive-base-and-source-variants.md
  - 'src/OTGW-firmware/MQTTstuff.ino:1773-1790'
  - 'src/OTGW-firmware/MQTTstuff.ino:1857-1870'
  - 'src/OTGW-firmware/MQTTstuff.h:233'
  - TASK-522 on dev branch (commit 4c95acd8)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the `bSeparateSources` XOR semantics fix from dev to feature-2.0.0. ADR-095 (accepted in this session, commit 35de7691) fully documents the contract; this task is the implementation port-forward.

**Background:**
On `dev`, the original `bSeparateSources` behaviour was strictly **additive** — when enabled, both the base entity and the three source variants (Thermostat / Boiler / Gateway) were published. For source-templated MsgIDs (e.g. MsgID 24, room temperature) this produced four near-identical entities in HA. User `_reuzenpanda_` reported the duplicate-name UX collision on `dev` 1.5.0-beta.5 (Discord 2026-04-30); fix shipped on dev as commit `4c95acd8` (TASK-522 on dev).

The same code path exists on feature-2.0.0 today (`MQTT_HA_FLAG_ANY_SOURCE = 0x07` at `MQTTstuff.h:233`, publish loops at `MQTTstuff.ino:1773-1790` and `:1857-1870`, expansion helper at `MQTTHaDiscovery.cpp:2285`). Without the port, the 2.0.0 release would reproduce the same regression on first contact with HA.

**Scope:** add the helper + two `else if` branches per ADR-095. No design changes; all decisions are settled by ADR-095.

## Implementation per ADR-095

1. **Add helper `msgIdHasAnySourceEntry(uint8_t id)`** in `src/OTGW-firmware/MQTTstuff.ino`, near the publish loops at line 1754 (`doAutoConfigure`) and 1836 (`doAutoConfigureMsgid`). Lazy-built 32-byte bitmap (8 × `uint32_t`), bounded loop over `MQTT_HA_SENSOR_COUNT` (306 entries on this branch, declared at `MQTTHaDiscovery.cpp:583`). Body identical to dev's TASK-522 helper.

2. **Modify both publish loops** (`doAutoConfigure` at `MQTTstuff.ino:1773-1790`, `doAutoConfigureMsgid` at `:1857-1870`) — insert one `else if` branch between the existing ANY_SOURCE branch and the fallback `streamSensorDiscovery` call. The `setMQTTConfigDone(cfg.id)` call must stay outside the if/else-if/else chain to preserve the JIT discovery state-machine invariant (ADR-041). Required pattern:

```cpp
if (cfg.flags & MQTT_HA_FLAG_ANY_SOURCE) {
  if (settings.mqtt.bSeparateSources) {
    expandAndStreamSensorSources(MQTTclient, cfg, ctx);
  }
  // skip source-template entries when separate sources disabled
} else if (settings.mqtt.bSeparateSources && msgIdHasAnySourceEntry(cfg.id)) {
  // skip base entity; source-variants cover this MsgID under bSeparateSources
} else {
  streamSensorDiscovery(MQTTclient, cfg, ctx);
}
setMQTTConfigDone(cfg.id);
```

3. **`expandAndStreamSensorSources()` itself is unchanged** — the behaviour change lives entirely in the caller's branch logic.

## Acceptance Criteria
<!-- AC:BEGIN -->
(see below)

## Verification

- `python evaluate.py --quick` clean (no new failures, ADR-095 references resolve).
- `python build.py --firmware` exit 0 on ESP8266 + ESP32-S3.
- Sketch-size impact comparable to dev's diff (~23 inserts, 0 deletes; +32 bytes static RAM for the bitmap).
- HA discovery side-effect on first deployment: users with `bSeparateSources=true` will see the four-entity duplication clear up after the next discovery republish (ADR-094's wipe-on-OTA is the natural cleanup path; without it the orphan base entity persists on the broker until manual cleanup).
<!-- SECTION:DESCRIPTION:END -->

- [ ] #1 Helper `msgIdHasAnySourceEntry(uint8_t id)` added near the publish loops with lazy 32-byte bitmap; build loop walks `MQTT_HA_SENSOR_COUNT` once per boot
- [ ] #2 `doAutoConfigure()` publish loop at `MQTTstuff.ino:1773-1790` gains the `else if` branch that suppresses base entities for source-templated MsgIDs when `bSeparateSources=true`
- [ ] #3 `doAutoConfigureMsgid()` publish loop at `MQTTstuff.ino:1857-1870` gains the same `else if` branch (symmetric)
- [ ] #4 `setMQTTConfigDone(cfg.id)` stays outside the if/else-if/else chain in both call-sites — verified by code review
- [ ] #5 When `bSeparateSources=false` (default), no behavioural change: `else` branch fires for non-ANY_SOURCE entries, helper bitmap built but never consulted in hot path
- [ ] #6 When `bSeparateSources=true` for source-templated MsgIDs (e.g. MsgID 24), HA receives only the three source variants; the base entity discovery topic is not published
- [ ] #7 Compiles clean on ESP8266 and ESP32-S3 — `python build.py --firmware` exit 0
- [ ] #8 `python evaluate.py --quick` shows no new failures (ADR-095 cross-references resolve)
<!-- AC:END -->
