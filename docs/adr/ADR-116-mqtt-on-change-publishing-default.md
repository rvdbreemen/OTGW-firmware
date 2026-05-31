---
id: ADR-116
title: MQTT On-Change Publishing as the Default with One-Time Interval Migration
status: Proposed
date: 2026-05-31
tags: [mqtt, settings, home-assistant, migration, defaults]
supersedes: []
superseded_by: []
related: [ADR-006, ADR-052, ADR-088]
deciders: [Robert van den Breemen]
---

# ADR-116: MQTT On-Change Publishing as the Default with One-Time Interval Migration

## Status

Proposed. Date: 2026-05-31.

**Decision Maker:** User: Robert van den Breemen (port of the dev 1.5.x decision into the 2.0.0 line; awaiting acceptance).

## Status History

status_history:
  - date: 2026-05-31
    status: Proposed
    changed_by: Claude (TASK-791)
    reason: Ports the dev-branch MQTT on-change-publishing default into 2.0.0; documents the 2.0.0 implementation (MQTTstuff.h struct, deferred flushSettings migration, flag-aware value-topic gate)
    changed_via: adr-kit

## Context

Through the current 2.0.0 line the MQTT publish interval
(`settings.mqtt.iInterval`, file key `MQTTinterval`, defined in
`src/OTGW-firmware/MQTTstuff.h`) defaulted to `0`. ADR-006 and ADR-052 define
`iInterval == 0` as legacy "publish every observed OpenTherm frame" mode. On a
busy OT bus that is multiple publishes per second per message ID, which is
wasteful broker traffic for the common Home Assistant use case and adds avoidable
load to the MQTT publish path that ADR-088 windows.

The desired product default is on-change publishing: publish immediately when a
value changes, and refresh unchanged values on a periodic heartbeat so Home
Assistant does not flip entities to `unavailable`. ADR-052 already specifies that
heartbeat semantics for `iInterval > 0`. What was missing on the 2.0.0 line is
(a) making it the default for fresh installs and (b) migrating existing
installations that still carry `iInterval == 0`.

The matching change has already been made on the 1.5.x `dev` line. This ADR
records the same decision for the 2.0.0 architecture, where the MQTT settings
struct lives in `MQTTstuff.h` (per ADR-079/081 component-header pattern) rather
than in `OTGW-firmware.h`.

### Forces

- **Backward compatibility.** Existing installs store `MQTTinterval=0`. A silent
  jump to a throttled default must be a deliberate, documented behaviour change,
  and users must retain an escape hatch back to publish-every-frame.
- **Disambiguation.** `iInterval == 0` alone cannot distinguish an old config
  (should migrate to 60) from a user who deliberately wants the firehose (leave
  alone). A persisted boolean whose absence means "old config" provides that
  signal, because an absent key takes the struct default `true`.
- **Boot safety and flash wear.** Persisting the migration must not write during
  `setup()` and must not rewrite on every boot. The existing deferred-write path
  (`flushSettings()` at `src/OTGW-firmware/settingStuff.ino:60`, invoked from
  `loop()` at `src/OTGW-firmware/OTGW-firmware.ino:632` via `timerFlushSettings`,
  guarded by `state.bSetupComplete` in `doBackgroundTasks()`) writes only when
  `settingsDirty` and only after setup completes.
- **Status-heartbeat independence (TASK-400).** The per-status-bit heartbeat in
  `OTGW-Core.ino` (`shouldPublishTrackedStatusBit()`,
  `STATUS_HEARTBEAT_INTERVAL_SEC`) is intentionally independent of
  `settings.mqtt.iInterval`. This change must not couple to it: the flag gates
  only the value-topic publish paths.
- **Immutable accepted ADRs.** ADR-006 and ADR-052 are Accepted; their text
  cannot be edited. A new ADR is required to change the default-behaviour
  decision they document.

## Decision

Introduce a persisted boolean setting `MQTTonChangePublishing` (struct field
`settings.mqtt.bOnChangePublishing` in `MQTTstuff.h`) with a firmware default of
`true`, and change the fresh-install `MQTTinterval` default to `60` seconds via a
new `MQTT_DEFAULT_PUBLISH_INTERVAL_SEC` define (60) in `MQTTstuff.h`.

1. **Default.** Fresh installs get `bOnChangePublishing = true`,
   `iInterval = MQTT_DEFAULT_PUBLISH_INTERVAL_SEC` (60).
2. **Migration (one-time, at settings load).** In `readSettings()`, after the
   existing dirty-state clear (`settingStuff.ino:517`) and the missing-value
   defaulting block, when `bOnChangePublishing == true` and `iInterval == 0`,
   set `iInterval = 60` and set `settingsDirty = true`. The deferred
   `flushSettings()` persists it on the first `loop()` pass after setup
   completes. `readSettings()` itself performs no write and triggers no service
   restart. Because the flag defaults to `true`, any pre-existing config (which
   lacks the key) is migrated; a user who has explicitly set
   `MQTTonChangePublishing=false` is not. The migration is idempotent: if the
   device reboots before the first flush, it simply re-runs.
3. **Runtime gate (flag-authoritative).** A helper
   `mqttOnChangePublishingActive()` in `OTGW-Core.ino` returns
   `bOnChangePublishing && iInterval > 0`. When false the firmware stays in
   legacy publish-every-message mode; when true it publishes on change with a
   heartbeat every `iInterval` seconds. This helper gates only the value-topic
   paths (`shouldPublishMQTTForID`, `shouldPublishMQTTForPSField`, the four
   status-byte `logWorthy` log gates, and the status-bit decision log). The
   TASK-400 status-bit heartbeat (`shouldPublishTrackedStatusBit`,
   `STATUS_HEARTBEAT_INTERVAL_SEC`) is left unchanged. The flag and interval are
   stored independently; `updateSetting()` writes each verbatim with no
   cross-coupling.
4. **UI coupling (presentation only).** In the Web UI (`data/index.js`), ticking
   `mqttonchangepublishing` reveals the interval row and sets it to `60` (when
   `0`/empty); unticking hides the row and sets the interval to `0`. Both values
   are POSTed and persisted through the normal settings path. This presentation
   coupling lives only in `data/index.js`; the firmware does not infer one
   setting from the other.

## Alternatives Considered

### Alternative 1: Interval-only, no stored flag (keep the synthetic UI checkbox)
Derive everything from `iInterval` (`0` = firehose, `>0` = on-change) and keep
the pre-existing UI-only synthetic checkbox. Rejected: without a persisted flag
the migration cannot distinguish an old `interval=0` config (migrate) from a
deliberate firehose choice (leave), so it would either migrate users who want the
firehose or migrate no one. The synthetic checkbox also could never round-trip
through REST.

### Alternative 2: Persist the migration by calling `writeSettings()` during `readSettings()`
Rejected: violates the "loading must not rewrite" invariant documented at
`settingStuff.ino:515-518`, writes during `setup()` before services are up, and
adds per-boot flash wear if the condition is re-evaluated on every load. The
deferred-write path achieves the same persistence safely after
`state.bSetupComplete`.

### Alternative 3: Hard cut-over — change the fresh-install default to 60 with no flag and no migration
Rejected: a fresh-install default of 60 leaves existing `interval=0` installs on
the firehose (no migration), so the product goal "existing devices move to
on-change" is not met, and there is no clean UI affordance or escape hatch back
to legacy mode.

## Consequences

**Positive:**
- On-change publishing becomes the default for new and upgraded installs, cutting
  steady-state MQTT traffic while keeping Home Assistant entities fresh via the
  heartbeat.
- Old `interval=0` installs are migrated once to a 60 s heartbeat and the result
  is persisted safely, without boot-time writes or per-boot flash wear.
- The flag is a first-class, UI-visible, REST-addressable setting; users can
  return to legacy publish-every-message by unticking it.
- No edits to immutable Accepted ADRs; the decision is recorded here and refines
  (does not replace) the default-behaviour clause of ADR-006 and ADR-052.

**Negative / risks:**
- Behaviour change for upgrading users who relied on every-frame republication.
- The flag is partly redundant with `iInterval` under normal UI use (flag off ⇔
  interval 0); the redundancy is intentional, justified by migration
  disambiguation, and must not be re-coupled in `updateSetting()`.
- The migration relies on the deferred-write path firing; if the device reboots
  before the first `flushSettings()`, the in-RAM migration re-runs next boot
  (idempotent), so the only cost is a delayed persist, not data loss.

## Enforcement

Manual-review only (guideline-level under ADR-080). No mechanical
`forbid_pattern`/`require_pattern` rule expresses this decision well: it is about
default values, a load-time migration, and a flag-authoritative gate spread
across firmware, REST and UI. Reviewers should confirm (a) `readSettings()`
contains no `writeSettings()` call for this migration, (b) `sendDeviceSettings()`
emits `mqttonchangepublishing`, (c) `updateSetting()` does not derive the flag
from the interval or vice versa, and (d) `shouldPublishTrackedStatusBit()` and
`STATUS_HEARTBEAT_INTERVAL_SEC` remain untouched.

## Related Decisions

- Refines the default-behaviour clause of ADR-006 (MQTT integration pattern,
  `iInterval = 0` as the default) and ADR-052 (MQTT publish eligibility contract,
  `iInterval = 0` legacy default). This is a narrow amendment of the *default
  value and migration*, not a replacement: both ADRs remain Accepted and
  authoritative for the rest of the MQTT publish contract, so their Status lines
  are intentionally left unchanged.
- ADR-088 (MQTT status-burst windowing): unaffected; this change reduces
  value-topic publish volume but does not alter burst windowing or the TASK-400
  status heartbeat.
- Ports the equivalent dev 1.5.x decision (dev ADR-081) into the 2.0.0 line. The
  numbering differs because the two worktrees number ADRs independently.

## References

- Code: `src/OTGW-firmware/MQTTstuff.h` (`MQTT_DEFAULT_PUBLISH_INTERVAL_SEC`
  define, `bOnChangePublishing` struct field, `iInterval` default),
  `src/OTGW-firmware/settingStuff.ino` (read/write/migrate, `updateSetting`),
  `src/OTGW-firmware/OTGW-Core.ino` (`mqttOnChangePublishingActive()` gate),
  `src/OTGW-firmware/restAPI.ino` (settings feed + `knownSettings` whitelist +
  debug dump), `src/OTGW-firmware/data/index.js` (UI coupling, labels, tooltips).
- Deferred-write mechanism: `src/OTGW-firmware/settingStuff.ino:60`
  (`flushSettings`), invoked at `src/OTGW-firmware/OTGW-firmware.ino:632`
  (`timerFlushSettings`), gated by `state.bSetupComplete`
  (`OTGW-firmware.ino:575`).
- Loading-must-not-rewrite invariant: `src/OTGW-firmware/settingStuff.ino:515-518`.
- Status heartbeat left untouched: `STATUS_HEARTBEAT_INTERVAL_SEC` at
  `src/OTGW-firmware/OTGW-Core.ino:233`, `shouldPublishTrackedStatusBit()` at
  `OTGW-Core.ino:1455`.
- Heartbeat semantics: ADR-052 "Interpretation of `iInterval = 0`".
