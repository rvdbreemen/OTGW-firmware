# ADR-072 — HA Discovery Friendly-Name Format

## Status

Accepted. 07-05-2026.

## Context

Home Assistant's MQTT discovery payload includes a `name` field that becomes the entity's user-visible label in HA's dashboard, energy view, automation editor, and history graphs. Until `beta.27` the firmware composed this field as `<hostname>_<entity-specific-suffix>` where the suffix was a literal PROGMEM string copy of an internal identifier. Concrete examples that shipped to field testers:

- `OTGW_Status_Master_Memberid_Code`
- `OTGW_ElectricalCurrentBurnerFlame`
- `OTGW_CHPumpOperationHoursg` (carried a stray `g` typo for many releases)
- `OTGW_vh_configuration` and ~25 sibling `vh_*` entities

Three independent UX failures came out of Discord field testing in early May 2026:

1. **Hostname-prefix duplication.** The HA device-card title already shows `OpenTherm Gateway (<hostname>)` once at the top of the device page; prepending the same hostname to every entity name is redundant noise that Andre and Number3 both flagged as the most jarring artefact at first install. (Discord, 2026-05-07.)
2. **Underscore as word separator.** HA renders the `name` field verbatim. Underscores and camelCase glue produce hard-to-read labels in the live UI: `BurnerOperationHours` and `DHWBurnerOperationHours` are visually indistinguishable in the entity grid. SergeantD's `alpha.3` reflash logs and Andre's `beta.26` screenshots both surfaced this as the dominant source of confusion when scanning ~250 entities.
3. **Acronym casing drift.** Internal identifiers were authored in mixed-case forms (`Memberid`, `Dayofweek`, `vh_*`, `dhw_*`, `rbp_*`) that produced visually inconsistent rendering after the first round of fixes (`Memberid` next to `MemberID`, `Dhw Pump` next to `DHW Burner`). Andre flagged this on `beta.27` as the biggest remaining UX defect even after the hostname-and-underscore fix landed.

[ADR-041](ADR-041-jit-ha-discovery.md) defines the JIT discovery publishing pipeline but is silent on what `name` SHOULD look like. Without a written contract every new entity added carried the risk of reintroducing the same defect class — caught only by serial Discord rounds with field testers, weeks after merge.

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

2. **Single authoring path.** All entity-name fields go through the `writeFriendlyName` helper in `mqtt_configuratie.cpp` (`mqtt_configuratie.cpp:1878`). The helper:
   - Replaces `_` with space,
   - Capitalises the first letter of each word,
   - Preserves existing capitals (so acronyms in source stay capitalised),
   - Performs no other transformation.

   The helper does NOT split camelCase or insert underscores. The **source PROGMEM string is the single source of truth for word boundaries**: contributors author `Electrical_Current_Burner_Flame`, never `ElectricalCurrentBurnerFlame`.

3. **Source-string convention** for the `ha_name_*` PROGMEM table:
   - Underscore-separated word boundaries.
   - Acronyms in canonical caps. The recognised set: `VH HB LB OEM ASF TSP RBP FHB CH CH1 CH2 DHW RF OTC CO2 S0 ID OT PIC SAT BLE UB MHz RSSI HC1 HC2 SCU GPIO LED MQTT WS`. The list is illustrative, not exhaustive — extend it when adding entities for new OpenTherm features.
   - Compound product/feature names preserved as glued tokens (e.g. `OTDirect`, `DayTime`).

4. **Hostname belongs to the device card, not entity names.** The device-card title `"OpenTherm Gateway (<hostname>)"` continues to use `writeRam(ctx.hostname)` once. Entity names do NOT prepend hostname. `mqtt_configuratie.cpp` SHALL contain exactly **one** `writeRam(ctx.hostname)` call total — at the device-card builder around `mqtt_configuratie.cpp:1937`, not at any entity-name builder.

5. **Slug stability.** The `ha_lbl_*` slug strings (used in the MQTT state topic path and HA's `unique_id`) are NOT affected by this decision. Slugs remain stable across friendly-name churn so HA's per-entity entity_id mapping survives an OTA upgrade. [ADR-067](ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md) governs entity-id reconciliation; this ADR is purely about the user-visible `name` field.

## Consequences

### Positive

- Single, mechanical convention — contributors don't need to think about HA UX when adding entities. The source PROGMEM string becomes the authoritative spec for the rendered label.
- Helper output is byte-deterministic; lint and pre-commit rules can mechanically detect "any new `writeRam(ctx.hostname)` call inside a name-field block" as a violation.
- Cross-branch coherence with the 2.0.0 line — dev and 2.0.0 produce byte-identical friendly names for shared entities (codified independently as ADR-099 on the 2.0.0 worktree).
- Tester debugging is easier — entity names match source PROGMEM strings except for `_ → space` and Title Case, which lets Andre and other testers grep the source by what they see in HA.
- Acronyms render consistently regardless of how the source token was originally cased.

### Negative

- One-time HA entity-name churn on upgrade. HA derives `entity_id` from `name` on first discovery and is sticky across renames; existing entities may need manual cleanup of retained `homeassistant/<domain>/<chipid>/<slug>/config` topics if the user wants the new names to be authoritative for the entity_id slot.
- Source-string editor needs to remember the canonical acronym list. New contributors can introduce drift (`memberid` instead of `MemberID`) — caught by code review or field testers unless we add a lint rule.
- The helper has a fixed format; a future "all-uppercase compact" mode (e.g. for industrial-look Lovelace cards) would require either a second helper or a per-entity opt-out flag. Out of scope here.

### Risks

- A contributor adding a new entity could bypass the helper and write `name` directly with `writeRam(ctx.hostname) + literal_suffix`, reintroducing both classes of defect (hostname prefix + glued words). Mitigated by the Enforcement block (count check via `llm_judge`) and code-review checklist.
- The recognised-acronym list is finite. New OpenTherm spec features may introduce acronyms not in the list (e.g. a future `WB`, `CH3`). Mitigation: extend the list when adding the entity. The ADR's list is illustrative; ADR text will not be re-issued for each acronym addition.

## Related

- [ADR-041](ADR-041-jit-ha-discovery.md) — JIT HA discovery publishing pipeline. This ADR fills a gap left by ADR-041 (which describes WHEN discovery is published but not WHAT the `name` field looks like).
- [ADR-067](ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md) — entity-id stability across upgrades; the entity-name churn from this decision flows through that mechanism on first OTA after upgrade.
- TASK-573 — applied this convention to the existing 125 PROGMEM friendly-name strings in `mqtt_configuratie.cpp` (shipped as `beta.29`).
- 2.0.0 sibling: ADR-099 — codifies the same decision on the 2.0.0 worktree (separate file because each worktree has its own ADR numbering; the *decision* is coherent across both).

## Enforcement

```json
{
  "llm_judge": true
}
```

A purely declarative `forbid_pattern` cannot express "exactly one occurrence per file"; the count check is therefore routed through the in-session LLM judge via `/adr-kit:judge` and surfaces as advisory in the pre-commit hook's `bin/adr-judge` output.

LLM judge guidance: Count occurrences of `writeRam(ctx.hostname)` in `src/OTGW-firmware/mqtt_configuratie.cpp`. The post-diff content MUST contain exactly one such call, at the device-card builder (the line immediately following the literal `"OpenTherm Gateway ("`). Two or more calls means a contributor has reintroduced the hostname-as-prefix anti-pattern in an entity-name builder; the diff is in violation. Also flag any new `const char ha_name_*[] PROGMEM = "...";` declaration whose string value contains a glued camelCase word (e.g. `ElectricalCurrentBurnerFlame`, `OEMFaultCode`) or a lowercase acronym fragment (e.g. `Memberid`, `Dayofweek`, `vh_*`, `dhw_*`, `rbp_*`) — those will render defectively after `_` → space + Title Case.
