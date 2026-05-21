# ADR-106: MQTT Topic Naming Mode — New Self-Describing Names by Default, Legacy OT-Spec Names via `settings.mqtt.bUseLegacyOtTopics` (Mutually Exclusive)

## Status

Accepted, 2026-05-21.

## Context

ADR-105 (accepted earlier on 2026-05-21) shipped a default-off `settings.mqtt.bPublishHaCoreAliases` toggle that, when enabled, mirrored 37 capability/state/type/fault binary_sensor publishes under HA-core-style alias topics *alongside* the firmware's existing OT-spec-derived labels. The dual-publish design was a deliberate migration aid: existing users see no change, opt-in users get both name sets.

Within hours of acceptance the maintainer reconsidered the shape of the feature for the 2.0.0 release:

1. **Single source of truth.** Publishing both name sets simultaneously doubles the broker discovery footprint (74 retained discovery configs for the affected bits when the toggle is on), doubles per-update MQTT chatter, and creates two parallel entity sets in HA's registry. Users wanting the new names typically don't want the old ones too — they just want better names.

2. **2.0.0 is a breaking-change release.** ESP32 + SAT + new OTGW32 hardware + three-device HA discovery split (TASK-648) already qualify 2.0.0 as breaking. Renaming topic labels by default — and dropping the legacy names from the default output — is consistent with the 2.0.0 scope and the right window to do it. Users on the 2.0.0 alpha channel are explicitly opt-in to breakage.

3. **Migration support without dual-publish.** A `settings.mqtt.bUseLegacyOtTopics` toggle (default `false`) lets users keep their existing automations by flipping back to legacy mode. This satisfies migration continuity without paying the dual-publish cost ADR-105 designed for. Either the new names *or* the legacy names — never both at the same time.

The dev branch (1.x.x) does *not* receive this design — see **ADR-078** on the dev branch, which reverts ADR-077 (the 1.x.x sibling of ADR-105) and explicitly defers the topic-naming redesign to 2.0.0.

### Implication: this is a breaking change

In the default configuration after this ADR ships:
- 37 OT-spec-derived topic labels stop publishing. The 37 new self-describing labels publish instead.
- HA-side: any automation pinned to one of the 37 legacy labels stops receiving state updates until the user (a) flips `bUseLegacyOtTopics` to `true`, or (b) rewrites the automation against the new label.
- Broker-side: the 37 legacy retained discovery configs and last-known state values linger until cleaned up. The cleanup machinery in Decision step 6 handles this on settings toggle; on initial 2.0.0 upgrade the legacy retained configs from the pre-ADR-106 firmware will still need clearing (handled by the same machinery — see "Initial upgrade cleanup" in Decision step 7).

Breaking-change framing in release notes and the migration recipe is in scope for the 2.0.0 release-notes commit, not this ADR.

### Mapping (carried over from ADR-105)

All 37 alias rows defined by ADR-105 stay exactly as they are. This ADR does NOT renegotiate the names — it only changes the mode in which they're published. The three-category architecture (A. supports_* capability, B. plain-name HA-core verbatim, C. plain-name firmware-coined) is preserved as documented in ADR-105.

Reference summary (full tables in ADR-105):
- **12** Category A `supports_*` aliases (9 HA-core verbatim + 3 firmware-coined).
- **12** Category B plain-name HA-core verbatim aliases.
- **13** Category C plain-name firmware-coined aliases.
- **37 total** alias rows replace **37 legacy** rows in the published set. The remaining 21 legacy rows (flame, cooling, low_water_pressure, air_pressure_fault, electric_production, the MsgID 0 HB master enable bits, the master_configuration topic, slave_memberid_code, etc.) have no alias and always publish under their current name.

### Constraints

- **No new label strings or row additions.** All 37 alias rows and 37 legacy rows already exist in `mqttHaBinSensors[]` from the ADR-105 implementation. This ADR repurposes them: legacy-with-alias rows get a new flag bit (`MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS = 0x20`) so the discovery streamer can filter them by mode.
- **ADR-105 is `Accepted, 2026-05-21.`** Per the project immutability rule (CLAUDE.md > ADR lifecycle), its content stays untouched; only its Status line flips to `Superseded by ADR-106, 2026-05-21.`
- **Mutual exclusivity is a hard contract.** At no point should the firmware publish both a legacy name *and* its alias for the same wire bit in the same setting cycle. Discovery filtering and publisher dispatch both enforce this; the cleanup machinery removes the *other* set's retained payloads on toggle.
- **Cleanup must survive power loss mid-drain.** The user picked the "build the cleanup machinery" option explicitly. Implementation persists the cleanup bitmap to LittleFS so a power cut between "user flipped toggle" and "broker payloads all cleared" resumes correctly on next boot.
- **ESP8266 + ESP32-S3 memory.** No additional PROGMEM cost vs ADR-105's already-shipped impl. Settings field rename is zero-cost. Flag bit consumes one of four remaining bits in `MqttHaBinSensorCfg::flags`. Cleanup state: 1 byte mode + 5-byte bitmap = 6 bytes RAM + a 6-byte LittleFS file when active.
- **Default off (= new mode).** A fresh 2.0.0 install has `bUseLegacyOtTopics = false`. Existing alpha-channel users upgrading from ADR-105's beta.46/47 see their alias toggle disappear and the new default take over — see Decision step 7 for the upgrade-time cleanup.

## Decision

1. **Settings field renamed and inverted.** Replace `settings.mqtt.bPublishHaCoreAliases` (default `false`) with `settings.mqtt.bUseLegacyOtTopics` (default `false`). Semantics:
   - `false` (default) → publish the new self-describing names. Skip the 37 legacy-replaced rows.
   - `true` → publish the legacy OT-spec-derived names. Skip all 37 alias rows.

   JSON storage key on disk: `MQTTuseLegacyOtTopics` (replaces `MQTTpublishHaCoreAliases`). REST API key: `mqttuselegacyottopics`. Web UI toggle exposed via `sendJsonSettingObj` for automatic rendering.

2. **New flag bit `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS = 0x20`** added to `MQTTstuff.h`. Set on each of the 37 legacy rows that has an alias replacement. The 21 legacy rows without an alias replacement do not get this flag.

3. **Discovery streamer filters by mode.** `streamHaDiscoveryEntries()` in `MQTTstuff.ino`:
   - **New mode (default, `bUseLegacyOtTopics = false`):** walk indexed range, *skip* rows whose flags contain `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS`. Then walk alias tail and publish every row (they all carry `MQTT_HA_FLAG_IS_HA_CORE_ALIAS`).
   - **Legacy mode (`bUseLegacyOtTopics = true`):** walk indexed range and publish every row (legacy-replaced *and* always-legacy). Skip the alias tail entirely.

   The streamer reads `settings.mqtt.bUseLegacyOtTopics` once per dispatch and applies the filter; no per-row hot-path branching in the composer.

4. **Publishers dispatch ONE label per call.** The helper functions `publishStatusBitMQTT()`, `publishStatusVHBitMQTT()`, `publishGatedBitMQTT()` already gained an optional `aliasLabel` parameter from ADR-105. The semantics of that parameter changes:
   - Before (ADR-105): publish `topic` always, additionally publish `aliasLabel` when `bPublishHaCoreAliases`.
   - After (this ADR): publish `aliasLabel` when `aliasLabel != nullptr && !bUseLegacyOtTopics`; otherwise publish `topic`. **Single publish per call.**

   For the direct `sendMQTTData(F("..."), val)` call sites (MsgID 2 HB0, MsgID 3 HB, MsgID 74, MsgID 101), the dual `if (settings.mqtt.bPublishHaCoreAliases)` block becomes a binary choice: each pair becomes a single guarded selection between the legacy label and the alias label.

5. **Cleanup machinery on toggle (~65 LOC + 6 bytes persistence).** When `bUseLegacyOtTopics` flips value, the firmware:
   1. Detects the transition in the settings save handler (`processSettingsLine` in `settingStuff.ino`) by reading the old field value before assignment, comparing to the new value, and arming cleanup state on differ.
   2. Persists a `/topic_cleanup.bin` file in LittleFS containing one mode byte (`1` = clear aliases / new→legacy transition; `2` = clear legacy-replaced / legacy→new transition) and a 5-byte bitmap (40 bits; 37 are used, 3 unused; bit set = still pending).
   3. Drains the bitmap from `doBackgroundTasks()` via a new helper `runTopicCleanupStep()` that publishes one empty retained payload per call to the next pending discovery topic, gated by `MQTTclient.connected()` + `canPublishMQTT()`. Up to 1–2 publishes per loop tick to spread MQTT buffer pressure.
   4. On every successful publish, clears the bit and rewrites `/topic_cleanup.bin` (writes are 6-byte append-style, well under LittleFS wear concerns at the expected ~once-per-toggle rate).
   5. When the bitmap is all-zero, deletes `/topic_cleanup.bin` and clears the mode byte. Cleanup is complete.
   6. On MQTT disconnect mid-drain: `MQTTclient.connected()` returns false, the drain step no-ops, and the next `onMqttConnect()` event re-arms the drain implicitly (the bitmap is still in LittleFS).
   7. On firmware reboot mid-drain: `readTopicCleanupState()` at boot reads `/topic_cleanup.bin` and resumes from the persisted bitmap.

   The cleanup *only* clears retained discovery topics (not retained state-topic payloads). State-topic payloads stale-out via the LWT/availability fall — HA marks the entities `unavailable` and the user can purge them from the entity registry. Adding state-topic cleanup would double the drain work for a marginal user-experience gain.

6. **Initial 2.0.0 upgrade cleanup.** Users upgrading from an ADR-105 build (alpha.46/47) where `bPublishHaCoreAliases` was *on* will have 37 retained alias discovery configs. After upgrade, `bUseLegacyOtTopics` is false (new mode), so the alias topics are still actively published — no cleanup needed there. The 37 retained *legacy* topics from the pre-ADR-105 era stay published in the broker too; those rows have `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS` and are no longer published by the firmware. The settings save handler in step 5 only detects *runtime* toggles; on a fresh upgrade from a pre-ADR-106 firmware, the user must either flip the toggle once-to-legacy and back-to-new to trigger cleanup, or run `mosquitto_pub -t '<haPrefix>/binary_sensor/<gw>/<legacy_label>/config' -n -r` for each affected label. **A boot-time first-run cleanup is explicitly out of scope** for this ADR; if field-test feedback shows this is painful, a follow-up ADR can add it (probably keyed off a `version` field in the settings file detecting an upgrade across the ADR-106 boundary).

7. **ADR-105 supersession.** Flip ADR-105 Status from `Accepted, 2026-05-21.` to `Superseded by ADR-106, 2026-05-21.` Content of ADR-105 unchanged per the immutability rule.

### Why this choice

- **One source of truth on the wire.** No deployment ever sees both names for the same bit at the same time. Cleaner mental model for users, cleaner discovery footprint for the broker, cleaner entity_registry for HA.
- **Right window.** 2.0.0 is the breaking-change release; topic-label changes are exactly the kind of contract change that earns a major-version bump. Doing this on dev (or in a 1.6.x minor) would violate the maintenance-line conservatism — and ADR-078 codifies that constraint on the dev side.
- **Reversible per user.** Anyone whose automations break can flip one bool back to legacy mode and recover. The cleanup machinery makes the round-trip clean.
- **No new code paths in the hot loop.** The discovery filter is a single flag-bit test per row; the publisher dispatch is a single null-check on the alias param. ADR-076's per-slot heartbeat semantics carry over unchanged because the publishers still call the same `publishMQTTOnOff()` underneath.
- **Cleanup state confined to LittleFS.** 6 bytes when active, file deleted when idle. No RAM cost at rest, no PROGMEM cost.

## Alternatives Considered

### Alternative 1 — Keep ADR-105's dual-publish, just flip the default to `true`

Make the alias toggle default-on, but still publish both names when on. Less breaking (legacy names still work for everyone) but the user explicitly chose "mutually exclusive".

**Rejected:** the user's framing was clear: "Het MOET het één of de ander zijn, dus of de nieuwe begrijpelijke namen OF de OT legacy topic. Nooit beide tegelijk." (It MUST be one or the other.) Dual-publish doesn't satisfy that contract.

### Alternative 2 — Document the rename, no toggle

Just rename the 37 labels in `mqttHaBinSensors[]` to the new names, drop the alias rows, drop the legacy publishers. Users with automations on legacy names must rewrite them. Smallest code surface (~50 LOC delete).

**Rejected:** removes the user's escape hatch. The maintainer explicitly wanted the `bUseLegacyOtTopics` toggle so existing automations have a one-bool migration. Hard-rename without toggle is fine for fresh installs but punishes upgraders. The toggle costs little and removes the upgrade pain.

### Alternative 3 — Migrate via an `expire_after: 7200` attribute on legacy entities (no cleanup machinery)

Add `expire_after: 7200` (2 hours) to the 37 legacy discovery configs. After a toggle, the OLD entities expire automatically in HA. No firmware drain code needed.

**Rejected:** the cleanup-on-toggle promise to the user was "the broker is clean after toggle". `expire_after` only marks the entity unavailable in HA; the retained discovery payload still sits in the broker, visible to any other MQTT client that resubscribes (e.g. a new HA instance, a Node-RED flow, an mqtt-explorer session). The user explicitly chose "build the cleanup machinery" from the three options; this is what they get.

### Alternative 4 — Backport the new design to dev (1.x.x)

Make 1.6.x ship the new naming-mode toggle too, in parallel with 2.0.0.

**Rejected:** dev is a maintenance line. ADR-078 (dev side) documents this constraint explicitly. Backporting a breaking topic-name change to dev would surprise stable-channel users. If demand for the new names on dev emerges later, a future ADR can re-open that decision.

### Alternative 5 — Use the existing `MQTT_HA_FLAG_IS_HA_CORE_ALIAS` bit to drive both filters

Don't introduce `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS`. Instead, the streamer infers which legacy rows are "replaced" by walking the alias tail and matching by OT id + bit position.

**Rejected:** OT id + bit position isn't enough to identify the sibling — different bits on the same OT id (e.g. MsgID 3 HB0 dhw_present vs HB7 heat_cool_mode_control) are different rows and the streamer would need explicit metadata. An explicit `LEGACY_REPLACED_BY_ALIAS` flag is one byte of struct waste per replaced row and one line of conditional in the streamer, vs a runtime lookup that's both slower and more error-prone. Easy choice.

### Alternative 6 — Bidirectional aliases (publish both, gated independently)

Two toggles: `bPublishLegacyOtTopics` (default true) and `bPublishNewHaTopics` (default true). Users can pick either, both, or neither.

**Rejected:** four states (00, 01, 10, 11) with one user-visible mode means 3 of 4 combinations are footguns. "Both" recreates ADR-105's dual-publish. "Neither" silently disables half the binary_sensors. The maintainer's "one or the other" contract is correct; one bool is the right shape.

### Alternative 7 — Defer to v2.1.0 (after the three-device split lands)

TASK-648's three-device split will re-parent the binary_sensors to boiler/thermostat/gateway devices. Doing the topic-naming change in the same window means users see one big migration instead of two consecutive ones.

**Rejected:** v2.1.0 timing is uncertain and the topic-naming change is independently valuable now. Coupling them creates schedule risk for the topic-naming work. ADR-106 ships standalone in 2.0.0; v2.1.0 inherits the new names + the toggle and adds device re-parenting on top.

## Consequences

### Positive

- The default 2.0.0 experience publishes self-describing topic names — `supports_hot_water` instead of `dhw_present`, `fault_indication` instead of `fault`, `ventilation_bypass_position` instead of `vh_bypass_position`. No HA-spec or vh_* jargon for new users.
- Existing alpha-channel users keep migration continuity: one toggle to `bUseLegacyOtTopics = true` restores all 37 legacy names. Cleanup on toggle keeps the broker tidy.
- HA-core integration users migrating to this firmware get HA-core-style names by default — automations carry over without manual rewriting for the 21 HA-core-matched bits.
- One source of truth on the wire — broker discovery footprint stays flat (95 rows minus the 37 inactive ones = 58 active discovery configs in either mode).
- The cleanup machinery is reusable: future ADRs that need on-toggle topic cleanup (e.g. ADR-106's hypothetical TASK-648 follow-up) can extend the `topic_cleanup` infrastructure rather than reinventing it.

### Negative

- **Breaking change for the 2.0.0 alpha channel.** Users upgrading from alpha.46/47 see their topic names change by default. Documented in the 2.0.0 release notes; manual cleanup recipe provided. The user accepted this as the right trade-off for the 2.0.0 milestone.
- **Initial-upgrade cleanup is manual.** The cleanup machinery (Decision step 5) only triggers on runtime toggle, not on first-boot of new firmware. Users upgrading from pre-ADR-106 firmware will have 37 stale retained legacy discovery configs on their broker until they either (a) toggle the setting once-then-back, or (b) manually purge. A future ADR can add boot-time first-run cleanup if field-test feedback shows it's needed.
- **One additional `MqttHaBinSensorCfg::flags` bit consumed** (`0x20`). Three of four remaining bits stay free for future use.
- **Settings rename is itself a breaking change** for users with hand-edited `settings.ini`. The JSON key `MQTTpublishHaCoreAliases` (from ADR-105) is no longer parsed — users who set it manually will see it ignored on next boot, and the new key `MQTTuseLegacyOtTopics` defaults to `false`. Documented in release notes.
- **The 37 alias publishes now happen unconditionally in new mode** (where ADR-105 only published them when the toggle was on). On boilers that emit MsgID 0 status updates at ~1 Hz, this is a measurable per-second MQTT publish increment vs the ADR-105 default-off shape. ESP8266 + AceTime budgets handle it (the publishers ride on the same ADR-076 per-slot heartbeat gating as the legacy publishes did before), but worth noting for field testers.

### Risk

The risk profile changes vs ADR-105: where ADR-105 was "additive, default-off" (lowest risk), this ADR is "renaming, default-new" (medium risk because the default behaviour changes for everyone). Mitigations:

- The 2.0.0 alpha channel is the right test population for this. Field testers explicitly opt in to breakage; their feedback informs whether the cleanup machinery + release-notes recipe are sufficient before 2.0.0 stable.
- The toggle gives a one-bool rollback path for any deployment whose automations break.
- The cleanup machinery is small enough to audit (~65 LOC) and persists progress across reboots — power loss mid-drain doesn't corrupt the broker state.

## Related Decisions

- **ADR-105** — sibling on this branch, the original dual-publish design. Superseded by this ADR; content immutable, Status updated.
- **ADR-077 / ADR-078** — dev (1.x.x) branch. ADR-077 was the dev sibling of ADR-105; ADR-078 supersedes ADR-077 and reverts the dev implementation. The naming-mode design (this ADR) is intentionally not backported to dev.
- **ADR-052** — MQTT publish eligibility contract. The new-mode publisher still uses `sendMQTTData()` and inherits the per-slot heartbeat semantics. The cleanup machinery does NOT enter the status fan-out; it publishes via the same `MQTTclient.publish()` path on its own background-task cadence.
- **ADR-076** — per-slot heartbeat. Each publisher call now sends one label (instead of one or two), so the heartbeat math is simpler: per-bit-slot timestamps tick on the active mode's label.
- **ADR-098** — sibling-suffix source-separated discovery. Source-separation orthogonal to topic-naming mode; if `bSeparateSources = true`, the active mode's labels get the source suffix the same way.
- **ADR-074** — retained availability heartbeat. The cleared discovery configs (during a toggle drain) do not affect the availability topic — HA marks the *entities* unavailable, the device itself stays online.
- **TASK-648** — v2.1.0 three-device split. Will be designed on top of the new mode's labels (the default); legacy mode in v2.1.0 still works but the device-split UX assumes the new names.

## References

- `docs/adr/ADR-105-mqtt-ha-core-capability-flag-aliases.md` — original dual-publish design, now superseded.
- `docs/adr/ADR-077-mqtt-ha-core-capability-flag-aliases.md` (dev branch) — dev sibling of ADR-105, also superseded.
- `docs/adr/ADR-078-defer-ha-core-aliases-to-2-0-0-revert-from-dev.md` (dev branch) — codifies the no-backport-to-dev policy that scopes this ADR to 2.0.0 only.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:1250` — `mqttHaBinSensors[]` array. ADR-105 added 37 alias rows; this ADR marks 37 legacy rows with the new flag bit (no row additions or label additions).
- `src/OTGW-firmware/MQTTstuff.h:277` — `MqttHaBinSensorCfg` struct; the `flags` field gains the `0x20` bit.
- `src/OTGW-firmware/MQTTstuff.h:172-176` — flag bit definitions; new `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS = 0x20`.
- `src/OTGW-firmware/MQTTstuff.h:41-62` — `MQTTSettingsSection`; field rename `bPublishHaCoreAliases` → `bUseLegacyOtTopics`.
- `src/OTGW-firmware/MQTTstuff.ino streamHaDiscoveryEntries()` — discovery iterator filters by `LEGACY_REPLACED_BY_ALIAS` in new mode, by `IS_HA_CORE_ALIAS` in legacy mode.
- `src/OTGW-firmware/OTGW-Core.ino` — `publishStatusBitMQTT`, `publishStatusVHBitMQTT`, `publishGatedBitMQTT` and the direct-publish sites at MsgID 2/3/74/101 dispatch to one label based on `bUseLegacyOtTopics`.
- `src/OTGW-firmware/settingStuff.ino` — JSON key rename `MQTTpublishHaCoreAliases` → `MQTTuseLegacyOtTopics`; settings-save transition detector arms cleanup.
- New: `topicCleanup` helpers in `MQTTstuff.ino` — `armTopicCleanup()`, `runTopicCleanupStep()`, `readTopicCleanupState()`, `writeTopicCleanupState()`, `clearTopicCleanupState()`. Persistence in `/topic_cleanup.bin` (LittleFS, 6 bytes).
- `src/OTGW-firmware/OTGW-firmware.ino doBackgroundTasks()` — calls `runTopicCleanupStep()` once per loop tick (gated by MQTT connected + heap available).
- Home Assistant core `homeassistant/components/opentherm_gw/binary_sensor.py` — source of the HA-core translation_keys for Categories A + B (mapping table per ADR-105).

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "bPublishHaCoreAliases", "path_glob": "src/OTGW-firmware/**", "message": "ADR-106: bPublishHaCoreAliases (from superseded ADR-105) is renamed to bUseLegacyOtTopics with inverted polarity. Update the reference accordingly." },
    { "pattern": "MQTTpublishHaCoreAliases", "path_glob": "src/OTGW-firmware/**", "message": "ADR-106: settings JSON key renamed from MQTTpublishHaCoreAliases to MQTTuseLegacyOtTopics." }
  ],
  "require_pattern": [],
  "llm_judge": true
}
```

Sonnet judge reviews diffs touching `MQTTHaDiscovery.cpp`, `MQTTstuff.h`, `MQTTstuff.ino`, `OTGW-Core.ino`, and the cleanup helpers for:

(a) Publishers that emit both the legacy and alias label in the same call (mutual-exclusivity violation).
(b) Discovery rows that have both `MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS` and `MQTT_HA_FLAG_IS_HA_CORE_ALIAS` (the flags are mutually exclusive: a row is either the legacy-being-replaced or the alias-replacing-it, never both).
(c) Cleanup state writes that do not persist to `/topic_cleanup.bin` (memory-only state would lose drain progress on reboot).
(d) Cleanup state reads on boot that do not resume the drain (correctness regression on mid-drain reboot).
(e) Renames of any of the 21 always-legacy labels (e.g. `flame`, `cooling`, `low_water_pressure`, `air_pressure_fault`) — those have no alias replacement and renaming them would be a separate breaking change requiring its own ADR.
