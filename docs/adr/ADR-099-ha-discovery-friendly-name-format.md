# ADR-099 — HA Discovery Friendly-Name Format

## Status

Accepted. 07-05-2026.

## Context

Home Assistant's MQTT discovery payload includes a `name` field that becomes the entity's user-visible label in HA's dashboard, energy view, automation editor, and history graphs. Until `alpha.18` the firmware composed this field as `<hostname>_<entity-specific-suffix>` where the suffix was a literal PROGMEM string copy of an internal identifier. Concrete examples that shipped to field testers:

- `OTGW_Status_Master_Memberid_Code`
- `OTGW_ElectricalCurrentBurnerFlame`
- `OTGW_CHPumpOperationHoursg` (carried a stray `g` typo for many releases)
- `OTGW_vh_configuration` and ~25 sibling `vh_*` entities

Three independent UX failures came out of Discord field testing in early May 2026:

1. **Hostname-prefix duplication.** The HA device-card title already shows `OpenTherm Gateway (<hostname>)` once at the top of the device page; prepending the same hostname to every entity name is redundant noise that Andre and Number3 both flagged as the most jarring artefact at first install. (Discord, 2026-05-07.)
2. **Underscore as word separator.** HA renders the `name` field verbatim. Underscores and camelCase glue produce hard-to-read labels in the live UI: `BurnerOperationHours` and `DHWBurnerOperationHours` are visually indistinguishable in the entity grid. SergeantD's `alpha.3` reflash logs and Andre's `alpha.18` screenshots both surfaced this as the dominant source of confusion when scanning ~250 entities.
3. **Acronym casing drift.** Internal identifiers were authored in mixed-case forms (`Memberid`, `Dayofweek`, `vh_*`, `dhw_*`, `rbp_*`) that produced visually inconsistent rendering after the first round of fixes (`Memberid` next to `MemberID`, `Dhw Pump` next to `DHW Burner`). Andre flagged this on `alpha.18` as the biggest remaining UX defect even after the hostname-and-underscore fix landed.

[ADR-077](ADR-077-streaming-mqtt-ha-discovery-architecture.md) defines the streaming HA discovery architecture (separate translation unit `MQTTHaDiscovery.cpp`) but is silent on what `name` SHOULD look like. Without a written contract every new entity added carried the risk of reintroducing the same defect class — caught only by serial Discord rounds with field testers, weeks after merge. The 2.0.0 line additionally introduces SAT and OTDirect telemetry whose new entities each multiply the regression surface.

### Alternatives considered

- **Status quo: identifier-as-label.** Rejected. Field-validated as confusing; required Andre to translate every entity name before describing problems on Discord.
- **Compute friendly name in HA's `name_template` Jinja per entity.** Rejected. Pushes UX complexity onto every HA installation, invisible to users who don't author templates, and divergent across users. Firmware should ship clean defaults.
- **Static lookup table mapping each internal id to a hand-written human label, no transform.** Considered. Equivalent end-result for the current ~250 entities but ~250 hand-written labels would inevitably drift from the canonical slug as new entities are added; we would forget some and the table would rot.
- **Single helper that applies a uniform transform — chosen.** `writeFriendlyName(MqttJsonWriter&, const char *src)` performs `_` → space + first-letter-of-each-word capitalised, preserving existing capitals. The PROGMEM source string is the single source of truth; helper output is byte-deterministic.

## Decision

1. **Output format.** HA discovery `name` fields render as Title Case with spaces — no underscores, no glued camelCase, no hostname prefix, with all-caps preserved for known acronyms.

   | Source PROGMEM | Rendered in HA |
   |---|---|
   | `Status_Master_MemberID_Code` | `Status Master MemberID Code` |
   | `Electrical_Current_Burner_Flame` | `Electrical Current Burner Flame` |
   | `VH_MemberID_Code` | `VH MemberID Code` |
   | `DHW_Pump_Valve_Operation_Hours` | `DHW Pump Valve Operation Hours` |
   | `OTDirect_Flame_Cycles_Per_Hour` | `OTDirect Flame Cycles Per Hour` |
   | `SAT_BLE_Sensor_RSSI` | `SAT BLE Sensor RSSI` |

2. **Single authoring path.** All entity-name fields go through the `writeFriendlyName` helper in `MQTTHaDiscovery.cpp`. The helper:
   - Replaces `_` with space,
   - Capitalises the first letter of each word,
   - Preserves existing capitals (so acronyms in source stay capitalised),
   - Performs no other transformation.

   The helper does NOT split camelCase or insert underscores. The **source PROGMEM string is the single source of truth for word boundaries**: contributors author `Electrical_Current_Burner_Flame`, never `ElectricalCurrentBurnerFlame`.

3. **Source-string convention** for the `ha_name_*` PROGMEM table:
   - Underscore-separated word boundaries.
   - Acronyms in canonical caps. The recognised set: `VH HB LB OEM ASF TSP RBP FHB CH CH1 CH2 DHW RF OTC CO2 S0 ID OT PIC SAT BLE UB MHz RSSI HC1 HC2 SCU GPIO LED MQTT WS`. The list is illustrative, not exhaustive — extend it when adding entities for new OpenTherm or 2.0.0-only (SAT, OTDirect) features.
   - Compound product/feature names preserved as glued tokens (e.g. `OTDirect`, `DayTime`).

4. **Hostname belongs to the device card, not entity names.** The device-card title `"OpenTherm Gateway (<hostname>)"` continues to use `writeRam(ctx.hostname)` once. Entity names do NOT prepend hostname. `MQTTHaDiscovery.cpp` SHALL contain exactly **one** `writeRam(ctx.hostname)` call total — at the device-card builder around `MQTTHaDiscovery.cpp:2095`, not at any entity-name builder.

5. **Slug stability.** The `ha_lbl_*` slug strings (used in the MQTT state topic path and HA's `unique_id`) are NOT affected by this decision. Slugs remain stable across friendly-name churn so HA's per-entity entity_id mapping survives an OTA upgrade. [ADR-094](ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md) governs entity-id reconciliation; this ADR is purely about the user-visible `name` field.

## Consequences

### Positive

- Single, mechanical convention — contributors don't need to think about HA UX when adding entities. Particularly relevant for the 2.0.0 line where SAT, OTDirect, and BLE features each introduce new entity families.
- Helper output is byte-deterministic; lint and pre-commit rules can mechanically detect "any new `writeRam(ctx.hostname)` call inside a name-field block" as a violation.
- Cross-branch coherence with the dev (1.5.x) line — dev and 2.0.0 produce byte-identical friendly names for shared entities (codified independently as ADR-072 on the dev worktree). The 2.0.0-only entities (`OTDirect_Flame_*`, `SAT_BLE_*`, `SAT_CH_*`) follow the same convention.
- Tester debugging is easier — entity names match source PROGMEM strings except for `_ → space` and Title Case.
- Acronyms render consistently regardless of how the source token was originally cased.

### Negative

- One-time HA entity-name churn on upgrade. HA derives `entity_id` from `name` on first discovery and is sticky across renames; existing entities may need manual cleanup of retained `homeassistant/<domain>/<chipid>/<slug>/config` topics. The 2.0.0 line was already going to incur entity-name churn from the OTGW32 hardware introduction; piggy-backing this format change on the same upgrade window minimises tester pain.
- Source-string editor needs to remember the canonical acronym list. New contributors can introduce drift — caught by code review or field testers unless we add a lint rule.
- The helper has a fixed format; a future "all-uppercase compact" mode (e.g. for industrial-look Lovelace cards) would require either a second helper or a per-entity opt-out flag. Out of scope here.

### Risks

- A contributor adding a new entity could bypass the helper and write `name` directly with `writeRam(ctx.hostname) + literal_suffix`, reintroducing both classes of defect. Mitigated by the Enforcement block (count check via `llm_judge`) and code-review checklist.
- The recognised-acronym list is finite. New 2.0.0 features (SAT BLE protocol revisions, ESP32-S3 board variants, future OpenTherm spec extensions) may introduce acronyms not in the list. Mitigation: extend the list when adding the entity. The ADR's list is illustrative; ADR text will not be re-issued for each acronym addition.

## Related

- [ADR-077](ADR-077-streaming-mqtt-ha-discovery-architecture.md) — streaming HA discovery architecture (`MQTTHaDiscovery.cpp` separate translation unit). This ADR fills a gap left by ADR-077 (which describes HOW discovery is streamed but not WHAT the `name` field looks like).
- [ADR-094](ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md) — entity-id stability across upgrades; the entity-name churn from this decision flows through that mechanism on first OTA after upgrade.
- TASK-574 — applied this convention to the existing 125 shared PROGMEM friendly-name strings in `MQTTHaDiscovery.cpp` (shipped as `alpha.20`); 2.0.0-only OTDirect_Flame_* + SAT_BLE_* + SAT_CH_* strings already conformed.
- dev sibling: ADR-072 — codifies the same decision on the dev (1.5.x) worktree (separate file because each worktree has its own ADR numbering; the *decision* is coherent across both).

## Enforcement

```json
{
  "llm_judge": true
}
```

A purely declarative `forbid_pattern` cannot express "exactly one occurrence per file"; the count check is therefore routed through the in-session LLM judge via `/adr-kit:judge` and surfaces as advisory in the pre-commit hook's `bin/adr-judge` output.
