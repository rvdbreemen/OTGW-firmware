# OTGW-firmware v1.4.1 Release Notes

**Release date:** 2026-04-21
**Branch:** 1.4.1 (from dev)
**Compare:** [v1.4.0...v1.4.1](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.4.0...v1.4.1)

## Overview

v1.4.1 is a stability and robustness release focused on ESP8266 heap pressure during Home Assistant MQTT auto-discovery, plus a new retained-discovery self-heal mechanism. The release also refactors the firmware's time-boundary dispatch into a single-caller contract (ADR-064) and publishes an hourly heap-diagnostic MQTT topic for operators who want to keep an eye on long-running heap health.

There are no breaking changes. All new behaviour is additive, with conservative defaults chosen so existing integrations keep working untouched.

## What's new

### Heap pressure reduction during HA discovery drip

Home Assistant auto-discovery publishes roughly 80 retained config messages after first boot (or after `markAllMQTTConfigPending()`). On ESP8266, those back-to-back publishes plus the regular Status-frame fan-out were the most common trigger for heap exhaustion and watchdog resets reported on Discord.

This release tightens the drip loop in several layers (TASK-338, TASK-339, TASK-340, TASK-342, TASK-344, TASK-347):

- **Drip interval slowed from 1 s to 2 s** under normal heap, and the slow-mode path went from 30 s to 10 s. The new 10 s slow-mode is still slow enough to guarantee recovery but keeps the full drip bounded at around 13 minutes worst-case instead of stretching into multiple hours.
- **HEAP_LOW is now the backoff trigger** for the discovery drip, wider than the previous HEAP_WARNING gate. The drip backs off earlier and rarely gets close to HEAP_CRITICAL on a loaded gateway.
- **Fragmentation-aware publish gates** use `getMaxFreeBlockSize()` in addition to raw free heap, so the firmware skips a publish when free heap looks fine on paper but the largest contiguous block is too small for the MQTT frame plus discovery payload.
- **Heap guard thresholds lowered** based on tester log data (CrashEvans' multi-day logs showed the old thresholds were too pessimistic and cost useful publish slots). `HEAP_CRITICAL`, `HEAP_WARNING` and `HEAP_LOW` were retuned with measured margin.
- **Status-burst cooldown window**: after an OpenTherm Status-frame fan-out (MsgID 0, typically every 1 to 3 seconds from the thermostat), the discovery drip holds off for a short cooldown so the two publishers don't compete for the same MQTT outbound buffer.

The net effect on a fresh flash is a slower but reliably-completing discovery cycle on ESP8266, measured on field devices that previously hit resets mid-discovery.

### Retained MQTT discovery verification and republish

TASK-349, TASK-351 and ADR-062 introduce a self-heal mechanism for the retained HA discovery state on the broker. The firmware can now sample the broker to confirm that every discovery topic it thinks it published is actually present, and re-announce any that are missing.

The verification cycle works as follows:

- A node-scoped wildcard subscribe (`<haprefix>/+/<uniqueid>/+/config`) is opened for a short window.
- Incoming retained configs are counted against the per-source bitmap `MQTTautoConfigMap`.
- Topics the firmware believes are published but the broker does not echo back are marked pending, and the next drip loop re-announces them.
- The subscribe is torn down cleanly at window end so the firmware is not left subscribed to a high-traffic wildcard.

**Daily auto-heal** is controlled by a new setting `MQTTdiscoveryAutoVerify` (default `true`). When enabled, the verify pass runs once per day at a low-activity hour; it can be disabled for shared brokers where a per-node wildcard subscribe is inconvenient.

**On-demand control** exposes three surfaces:

- REST: `GET /api/v2/discovery` returns the current published/pending counters and verification state. `POST /api/v2/discovery/verify` starts a verify window immediately. `POST /api/v2/discovery/republish` forces all configs pending and lets the drip re-announce them.
- Telnet debug: the `V` key from the debug console starts a verify window (useful when the Web UI is not reachable).
- MQTT: verification state is reflected in the hourly heap-diagnostic payload (see below).

The mechanism is RAM-tuned so the short-lived buffer resize stays within the ESP8266 envelope. ADR-062 documents the reasoning and the trade-offs considered.

### Hourly heap diagnostic MQTT topic

TASK-346 adds a retained MQTT topic `<topTopic>/otgw-firmware/stats/heap` published once per hour. The payload is a 17-field JSON object covering:

- Current, minimum, maximum and average free heap over the reporting window.
- Largest contiguous block (the fragmentation signal).
- Cumulative counters for each heap tier transition (OK, LOW, WARNING, CRITICAL) and for dropped publishes per category.
- Discovery state: published count, pending count, last verify outcome, last verify timestamp.

Because the topic is retained, any fresh subscriber (including Home Assistant on restart, or a NodeRED flow started on demand) can read the most recent snapshot without waiting for the next hour boundary. The topic sits in the existing MQTT tree under the same top topic used for normal OT publishing, so no new ACL rules are required.

Operators who track long-term heap health can graph the min-free and largest-block fields to spot heap leaks or fragmentation drifts over multiple days.

### Unified time-boundary dispatcher

TASK-350 and ADR-064 consolidate the firmware's four time-boundary helpers (`minuteChanged()`, `hourChanged()`, `dayChanged()`, `yearChanged()` in `helperStuff.ino`) under a single caller contract. Before this release, those helpers were consumed at multiple sites, each responsible for its own cadence and guard flags; a change in one caller could silently double-fire another.

After the refactor:

- Exactly one dispatcher site (in `OTGW-firmware.ino`) calls `minuteChanged()` and fans out the hour/day/year transitions from there.
- All minute-aligned periodic work (nightly restart, daily discovery verify, hourly heap diagnostic publish, year-rollover bookkeeping) consumes the boundary flags from the dispatcher instead of polling `minute()`/`hour()` directly.
- **Nightly restart** (TASK-345) was migrated to the `hourChanged` hook as part of this refactor. It still honours `settings.iRestartHour` but is now wall-clock aligned through the dispatcher rather than polling `minute() == 0` on its own schedule.

ADR-064 captures the single-caller contract and the reasoning for it. Functionally the behaviour is identical for well-behaved call sites; the benefit is that future periodic jobs have one obvious hook to attach to.

## Behavioural notes for users

- **First-boot HA discovery now takes roughly 2x longer under normal heap**: about 80 IDs times 2 seconds equals around 3 minutes end-to-end. This is intentional. The old 1 s cadence completed faster on paper but stalled or reset partway through on devices under heap pressure. The new cadence trades headline speed for completion reliability.
- **Ventilation (VH) boilers**: the heap-pressure improvements apply to the non-VH Status fan-out paths immediately. VH-specific burst coverage (wrapping `publishMasterStatusVHState`, `publishSlaveStatusVHState` and `publishStatusVHBitMQTT` in the same `beginStatusBurst`/`endStatusBurst` quiesce) is tracked in TASK-354 and is being validated on live VH hardware. Non-VH users see the full benefit today.
- **Nightly restart**: still triggers at the configured `settings.iRestartHour`. Timing is now wall-clock aligned through the unified dispatcher (ADR-064) instead of each site polling `minute() == 0` on its own. Practically: the restart happens at the top of the chosen hour as before; you should not notice a change.
- **Retained discovery auto-verify**: enabled by default via `MQTTdiscoveryAutoVerify`. On brokers where a per-node wildcard subscribe is undesirable (shared broker, tight wildcard ACLs, very high cross-device retained-message volume) set this to `false` in settings. The on-demand REST and telnet paths remain available either way.

## Known limits and trade-offs

A release note should tell you what the release does *not* do, so:

- **Heap-diag retained payload at boot**: the first publish at the next hour boundary after a reset will briefly carry near-zero cumulative counters because the counters start fresh at boot. The retained message is overwritten at the following hour boundary once real activity has accumulated. This is visible only in the narrow window between first publish and the next hour rollover.
- **Status-burst cooldown default**: `STATUS_BURST_COOLDOWN_MS` was set to 2000 ms (TASK-353) after field analysis showed that longer values (10000 ms was tested) permanently deferred the drip under the typical 3-second Status-frame cadence, which would have defeated the feature. 2000 ms gives the Status fan-out room to complete without starving the drip.
- **Verify window heap budget**: the verify path enforces a minimum free-heap threshold (`VERIFICATION_MIN_HEAP_START`) before it will start a new window. Under sustained heap pressure (concurrent WebSocket + device info fetch at boot) the verify may be skipped; the next cycle will retry.
- **ADR-062 CI gates**: the ADR proposes instrumentation checks (`check_discovery_counter_instrumented`, `check_publishedtopic_counter_reset`) to keep the counter discipline enforceable in the build. Those gates are planned but not wired into CI yet; tracked in TASK-364.
- **VH burst-quiesce**: see above. Tracked in TASK-354, pending hardware validation.

## Upgrade notes

- Flash both firmware and filesystem. Frontend changes (heap diagnostics panel and discovery verification controls in the Web UI) require the filesystem image.
- Hard-refresh the browser after flashing (Ctrl+F5). The bundled JS in `data/index.js` was updated for the new diagnostics UI.
- No settings migration is required. The new `MQTTdiscoveryAutoVerify` setting defaults to `true` on upgrade.
- If you run on a shared MQTT broker and prefer not to subscribe to a node-scoped wildcard once per day, set `MQTTdiscoveryAutoVerify` to `false` in the MQTT settings page. On-demand verify via REST or telnet remains available regardless.

## Breaking changes

No breaking changes vs v1.4.0. See [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md) for the cumulative log.

## Links

### Architecture Decision Records

- [ADR-062: Retained discovery verification](docs/adr/ADR-062-retained-discovery-verification.md) (Proposed)
- [ADR-064: Time-boundary single-caller contract](docs/adr/ADR-064-time-boundary-single-caller-contract.md) (Proposed)

### Backlog tasks shipped in this release

- [TASK-338: Slow MQTT discovery drip from 1 s to 2 s](backlog/tasks/task-338%20-%20Slow-MQTT-discovery-drip-interval-from-1s-to-2s.md)
- [TASK-339: Widen heap-pressure backoff trigger to HEAP_LOW](backlog/tasks/task-339%20-%20Widen-MQTT-discovery-heap-pressure-backoff-trigger-to-HEAP_LOW.md)
- [TASK-340: getMaxFreeBlockSize in MQTT/WebSocket publish gates](backlog/tasks/task-340%20-%20Use-getMaxFreeBlockSize-in-MQTT-WebSocket-publish-gates-for-fragmentation-awareness.md)
- [TASK-342: Quiesce discovery drip during Status-frame burst](backlog/tasks/task-342%20-%20Quiesce-MQTT-discovery-drip-during-Status-frame-burst.md)
- [TASK-344: Lower heap guard thresholds (tuned on log data)](backlog/tasks/task-344%20-%20Lower-heap-guard-thresholds-tuned-on-Crashevans-log-data.md)
- [TASK-345: Refactor nightly restart to use hourChanged hook](backlog/tasks/task-345%20-%20Refactor-nightly-restart-to-use-hourChanged-hook.md)
- [TASK-346: Cumulative heap-health drop statistics + hourly MQTT publish](backlog/tasks/task-346%20-%20Cumulative-heap-health-drop-statistics-with-hourly-MQTT-publish.md)
- [TASK-347: Post-Status-burst cooldown for discovery drip](backlog/tasks/task-347%20-%20Post-Status-burst-cooldown-window-for-MQTT-discovery-drip.md)
- [TASK-348: Fix discovery drip limbo on publish failure](backlog/tasks/task-348%20-%20Fix-discovery-drip-limbo-on-publish-failure.md)
- [TASK-349: On-demand discovery verification and republish](backlog/tasks/task-349%20-%20On-demand-MQTT-discovery-verification-and-republish.md)
- [TASK-350: Unify time-boundary dispatcher single-caller contract](backlog/tasks/task-350%20-%20Unify-time-boundary-dispatcher-single-caller-contract.md)
- [TASK-351: Daily automatic discovery verification](backlog/tasks/task-351%20-%20Daily-automatic-discovery-verification.md)

### Follow-ups tracked for v1.4.2

- TASK-352 (heap-diag JSON buffer size), TASK-353 (cooldown default 2000 ms), TASK-354 (VH burst-quiesce), TASK-355 (ADR governance cleanup), TASK-359 (verify preconditions), and the remaining backlog items raised by the 1.4.1 code review are tracked in `backlog/tasks/`.

## Thank you

Thanks to **CrashEvans** for the multi-day log captures that made the heap threshold retuning possible, and to everyone on Discord who reported the mid-discovery resets that kicked off this branch. Field data beats armchair tuning every time.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.4.0](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.4.0)
