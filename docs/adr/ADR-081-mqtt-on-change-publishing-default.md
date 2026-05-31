# ADR-081: MQTT on-change publishing as the default with one-time interval migration

## Status

Accepted, 2026-05-31.

## Context

Through v1.6.0 the MQTT publish interval (`settings.mqtt.iInterval`, file key `MQTTinterval`) defaulted to `0`, which ADR-006 and ADR-052 define as legacy "publish every observed OpenTherm frame" mode. On a busy OT bus that is multiple publishes per second per message ID. For the common Home Assistant use case this is wasteful broker traffic and was a contributing factor in a HEAP-CRITICAL burst incident (GeorgeZ83 transcript, addressed separately by the timer-jitter work in TASK-786).

The desired product default is on-change publishing: publish immediately when a value changes, and refresh unchanged values on a periodic heartbeat so Home Assistant does not flip entities to `unavailable`. ADR-052 already specifies this heartbeat semantics for `MQTTinterval > 0`; what is missing is making it the default and migrating existing installations.

A first attempt (TASK-787, commit `15e7a207`) introduced this but was reverted (TASK-788, commit `44ca8fdc`) for three implementation defects, not because the feature was wrong:

1. It called `writeSettings()` from inside `readSettings()` during boot, violating the documented "loading must NOT trigger a rewrite" invariant at `src/OTGW-firmware/settingStuff.ino:363-365`.
2. The new flag was never emitted by `sendDeviceSettings()` (`restAPI.ino`), so the Web UI could not display or round-trip it.
3. `updateSetting()` re-derived the flag from the interval on every interval write, coupling two settings that should be independent.

Separately, the device reboot loop observed during that period was traced to an unrelated `getParam()` varargs over-read in `safeTimers.h` (TASK-789, commit `7e73d79f`), not to this feature. With that fixed, the on-change default can be re-introduced cleanly.

### Forces

- **Backward compatibility.** Existing installs store `MQTTinterval=0`. A silent jump to a throttled default must be a deliberate, documented behaviour change, and users must retain an escape hatch back to publish-every-frame.
- **Disambiguation.** "interval 0" alone cannot distinguish an old config (should migrate to 60) from a user who deliberately wants the firehose (leave alone). A persisted boolean whose absence means "old config" provides that signal.
- **ESP8266 flash wear and boot safety.** Persisting the migration must not write during `setup()` and must not rewrite on every boot. The existing deferred-write path (`flushSettings()` in `loop()`, `settingStuff.ino:60`) writes only when `settingsDirty` and only after `state.bSetupComplete`.
- **Immutable accepted ADRs.** ADR-006 and ADR-052 are Accepted; their text cannot be edited. A new ADR is required to change the default-behaviour decision they document.

## Decision

Introduce a persisted boolean setting `MQTTonChangePublishing` (struct field `settings.mqtt.bOnChangePublishing`) with a firmware default of `true`, and change the fresh-install `MQTTinterval` default to `60` seconds.

1. **Default.** Fresh installs get `bOnChangePublishing = true`, `iInterval = 60`.
2. **Migration (one-time, at settings load).** After the existing dirty-state clear in `readSettings()`, when `bOnChangePublishing == true` and `iInterval == 0`, set `iInterval = 60` and set `settingsDirty = true`. The deferred `flushSettings()` persists it on the first `loop()` pass after setup completes. `readSettings()` itself performs no write and sets no service-restart side effects. Because the flag defaults to `true`, any pre-1.6.1 config (which lacks the key) is migrated; a user who has explicitly set `MQTTonChangePublishing=false` is not.
3. **Runtime gate (flag-authoritative).** A helper `mqttOnChangePublishingActive()` returns `bOnChangePublishing && iInterval > 0`. When false the firmware stays in legacy publish-every-message mode; when true it publishes on change with a heartbeat every `iInterval` seconds. The flag and interval are stored independently; `updateSetting()` writes each verbatim with no cross-coupling.
4. **UI coupling (presentation only).** In the Web UI, ticking `mqttonchangepublishing` reveals the interval row and sets it to `60` (when `0`/empty); unticking hides the row and sets the interval to `0`. Both values are POSTed and persisted through the normal settings path. This presentation coupling lives only in `data/index.js`; the firmware does not infer one setting from the other.

## Alternatives Considered

### Alternative 1: Interval-only, no stored flag (keep the synthetic UI checkbox)
Derive everything from `iInterval` (`0` = firehose, `>0` = on-change) and keep the existing UI-only synthetic checkbox. Rejected: without a persisted flag the migration cannot distinguish an old `interval=0` config (migrate) from a deliberate firehose choice (leave), so it would either migrate users who want the firehose or migrate no one.

### Alternative 2: Persist the migration by calling `writeSettings()` during `readSettings()` (the TASK-787 approach)
Rejected: violates the "loading must not rewrite" invariant, writes during `setup()` before services are up, and was a smell in the reverted implementation. The deferred-write path achieves the same persistence safely after setup.

### Alternative 3: Hard cut-over — change the default to 60 with no flag and no migration
Rejected: a fresh-install default of 60 leaves existing `interval=0` installs on the firehose (no migration), so the product goal "existing devices move to on-change" is not met, and there is no clean UI affordance or escape hatch.

## Consequences

### Positive
- On-change publishing becomes the default for new and upgraded installs, cutting steady-state MQTT traffic while keeping Home Assistant entities fresh via the heartbeat.
- Old `interval=0` installs are migrated once to a 60 s heartbeat and the result is persisted safely, without boot-time writes or per-boot flash wear.
- The flag is a first-class, UI-visible, REST-addressable setting; users can return to legacy publish-every-message by unticking it.
- No edits to immutable Accepted ADRs; the decision is recorded here and refines (does not replace) the default-behaviour clause of ADR-006 and ADR-052.

### Negative / risks
- Behaviour change for upgrading users who relied on every-frame republication; documented in `docs/BREAKING_CHANGES.md` for v1.6.1.
- The flag is partly redundant with `iInterval` under normal UI use (flag off ⇔ interval 0); the redundancy is intentional, justified by migration disambiguation, and must not be re-coupled in `updateSetting()` (the TASK-787 defect).
- The migration relies on the deferred-write path firing; if the device reboots before the first `flushSettings()`, the in-RAM migration simply re-runs next boot (idempotent), so the only cost is a delayed persist, not data loss.

## Enforcement

No mechanical `forbid_pattern`/`require_pattern` rule expresses this decision well: it is about default values, a load-time migration, and a flag-authoritative gate spread across firmware, REST and UI. This ADR is **manual-review only**. Reviewers should confirm (a) `readSettings()` contains no `writeSettings()` call, (b) `sendDeviceSettings()` emits `mqttonchangepublishing`, and (c) `updateSetting()` does not derive the flag from the interval or vice versa.

## Related Decisions

- Refines the default-behaviour clause of [ADR-006](ADR-006-mqtt-integration-pattern.md) (`settingMQTTinterval = 0` as the default) and [ADR-052](ADR-052-mqtt-publish-eligibility-contract.md) (`XX = 0` legacy default). This is a narrow amendment of the *default value and migration*, not a replacement: both ADRs remain Accepted and authoritative for the rest of the MQTT publish contract, so their Status lines are intentionally left unchanged.
- TASK-787 (`15e7a207`) first attempt; TASK-788 (`44ca8fdc`) revert; TASK-789 (`7e73d79f`) the unrelated reboot-loop fix; TASK-790 implements this ADR.
- Code: `src/OTGW-firmware/OTGW-firmware.h` (struct default), `settingStuff.ino` (read/write/migrate), `OTGW-Core.ino` (`mqttOnChangePublishingActive()` gate), `restAPI.ino` (settings feed + whitelist), `data/index.js` (UI coupling).

## References

- Reverted attempt diff: commit `15e7a207`; revert: commit `44ca8fdc`.
- Deferred-write mechanism: `src/OTGW-firmware/settingStuff.ino:60` (`flushSettings`), invoked at `src/OTGW-firmware/OTGW-firmware.ino:438`.
- Heartbeat semantics: ADR-052 "Interpretation of `XX = 0`" and the per-slot `mqttlastsent[256]` packing described in ADR-006.
