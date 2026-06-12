---
id: TASK-648
title: >-
  feat-2.0.0: HA discovery three-device split (boiler/gateway/thermostat)
  aligned with HA core opentherm_gw
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 08:42'
updated_date: '2026-06-04 05:37'
labels:
  - milestone-v2.1.0
  - deferred-from-v2.0.0
  - enhancement
  - ha-discovery
  - mqtt
dependencies: []
priority: medium
ordinal: 45000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Home Assistant core 2024.10 (PR #124869) split the `opentherm_gw` integration's entities across three distinct devices: `boiler`, `gateway`, and `thermostat`. Each entity is now assigned to whichever device logically owns it, and the device-registry identifiers follow the pattern `(DOMAIN, f"{hub_id}-{device}")`. The release blog called it out as a breaking change: *"To modernize the OpenTherm Gateway integration, all entities have been split into different devices."*

OTGW-firmware currently emits all 309+ HA discovery entries under a single device (identified by `settings.mqtt.sUniqueid`). This means HA users running the firmware see one large device card mixing thermostat-side and boiler-side data, while users running HA core's `opentherm_gw` over serial/TCP see three nicely grouped devices. The mental model diverges.

This task aligns the firmware's HA discovery topology with HA core's three-device split. Topic names DO NOT change — only the `device:` block inside discovery payloads. Existing MQTT consumers (subscribing to topics) are unaffected. HA users on existing automations are minimally affected (entity_ids stay the same; only the device grouping changes).

Why v2.1.0 and not v2.0.0:
- Cosmetic, not functional; not needed for the platform-release scope of v2.0.0
- Needs a migration story for users with existing retained discovery messages
- Wants an ADR documenting the per-entity device assignment so reviewers can audit it
- Better landed alongside other discovery-shape improvements (e.g. device-bundle discovery from HA 2024.11 if that work is also scoped)

Reference research: see in-conversation HA OTGW dashboard inventory (2026-05-21) for the full per-device entity table from HA core dev branch `entity.py:44-48`, `__init__.py` device records, and `sensor.py` / `binary_sensor.py` / `switch.py` / `select.py` / `button.py` / `climate.py` entity_description tables.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setting `settings.mqtt.bSplitHaDevices` added (default false, opt-in)
- [ ] #2 When enabled, HA discovery emits three `device:` blocks with identifiers `{nodeId}-boiler`, `{nodeId}-gateway`, `{nodeId}-thermostat`
- [ ] #3 Boiler device block: manufacturer from DATA_SLAVE_MEMBERID, model_id from DATA_SLAVE_PRODUCT_TYPE, hw_version from DATA_SLAVE_PRODUCT_VERSION, sw_version from DATA_SLAVE_OT_VERSION
- [ ] #4 Thermostat device block: manufacturer from DATA_MASTER_MEMBERID, model_id from DATA_MASTER_PRODUCT_TYPE, hw_version from DATA_MASTER_PRODUCT_VERSION, sw_version from DATA_MASTER_OT_VERSION
- [ ] #5 Gateway device block: manufacturer="Schelte Bron", model="OpenTherm Gateway", sw_version from PIC `PR=A` about-string
- [ ] #6 via_device set so HA shows boiler+thermostat grouped under the gateway device (matches HA core grouping)
- [ ] #7 Per-entity device routing matches HA core's mapping (see research table in conversation: thermostat side gets master_* flags, climate, room_setpoint_override; boiler side gets slave_* fault/capability flags; gateway gets OTGW_* internals like LED/GPIO modes)
- [ ] #8 Sensors that appear on both boiler and thermostat in HA core (control_setpoint, ch_water_temp, dhw_temp, etc.) keep their current single-entity behaviour to avoid duplicate unique_ids; document the deviation from HA core in the ADR
- [ ] #9 ADR drafted explaining the trade-off (one device vs three; deviation on bilateral sensors); status Proposed pending review
- [ ] #10 Migration story documented: on first boot after enabling the setting, firmware republishes all discovery configs; old single-device discovery entries are cleared via the existing ADR-070-style orphan cleanup path
- [ ] #11 Setting OFF preserves existing single-device behaviour byte-for-byte (no incidental payload changes when disabled)
- [ ] #12 MQTT.md updated with the new setting and the per-entity device routing table
- [ ] #13 build.py --firmware exits 0; evaluate.py --quick shows no new findings
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-03: re-scoped to 2.0.0 and to a FIVE-device model (was 3, deferred-v2.1.0). Brainstorm complete; spec at docs/superpowers/specs/2026-06-03-ha-discovery-five-device-split-design.md. Key decisions: 5 devices (boiler/thermostat/gateway/ESP-firmware/SAT); MODERN is the 2.0.0 default (not opt-in); ONE umbrella legacy switch (bLegacyMode, subsumes ADR-106 bUseLegacyOtTopics) restores 1.x.x byte-for-byte; modern unique_ids {nodeId}-{device}-{label} (HA-core scheme), legacy {nodeId}-{label}; bilateral OT values REPLICATED on both boiler+thermostat (HA-core-exact); gateway device = PIC xor OTDirect; NO via_device (HA core dropped it -> overrides original AC#6); HA core authoritative for OT-entity device routing. Supersedes the original 3-device ACs.

2026-06-04: IMPLEMENTATION COMPLETE (all 8 plan phases, commits 64f22ab0..77e7c6dd, alpha.154-159, pushed). T1 umbrella bLegacyMode (subsumes bUseLegacyOtTopics, 8 alias reads repointed); T2 HaDevice dimension + per-device device block (+ second SAT-zone writer); T3 modern {nodeId}-{device}-{label} unique_ids at 17 sites + drip-persistent g_haDeviceIntroduced; T4 per-entity routing + bilateral OT double-emit + device segment in config topics (no collision); T5 per-device metadata via ctx.devMeta (OT product IDs, PIC/OTDirect gateway identity, ESP board, SAT); T6 topology migration + orphan-cleanup state machine (bilateral-aware, reboot-resume safe); T8 ADR-122 + MQTT.md + golden-test harness + HA-core routing research. Build: esp32+esp8266 firmware+filesystem clean; evaluate.py --quick 0-fail. Footprint: esp8266 flash 85.4%/RAM 91.5%, esp32 flash 96.1% - bilateral fits both, no fallback (T7). Legacy mode is byte-identical by construction at every layer. BENCH-PENDING (cannot verify without device/broker, NOT done): (a) golden-file legacy byte-identity capture from a real discovery dump; (b) HA entity-removal/migration on a live broker; (c) on-device five-device grouping + PIC/OTDirect identity confirmation; (d) discovery-burst behaviour under ADR-088 on ESP8266. Status kept In Progress pending these bench/field validations.

2026-06-04 REFINEMENT (alpha.160, d617e83c): maintainer split the umbrella back into TWO independent axes. (1) Device topology bLegacyMode: default modern five-device for EVERYONE (new + 1.x.x upgraders); legacy single-device is an advanced escape. (2) OT-topic naming bUseLegacyOtTopics (ADR-106, standalone again, NOT subsumed): upgrade-aware default via key-presence: a 1.x.x config lacks the MQTTuseLegacyOtTopics key (predates ADR-106) so readSettings defaults it to LEGACY topics (existing MQTT automations keep working, opt-in to new); fresh installs + existing 2.0.0 configs carry the key so honour stored/default = NEW topics (no alpha-tester regression). doAutoConfigureMsgid split: bilateral/device-segment on bLegacyMode, ADR-106 row-filtering (skipReplaced/alias-tail) on bUseLegacyOtTopics; OTGW-Core.ino publish-label reads reverted to bUseLegacyOtTopics. Net upgrade UX: 5-device cards for all (entities re-register once) but old state-topic names preserved for upgraders. Both envs build clean, evaluator 0-fail. Docs (ADR-122 + MQTT.md) being updated to the two-axes model.
<!-- SECTION:NOTES:END -->
