# ADR-084 — HA Discovery Multi-Device Topology

## Status

Accepted. 02-06-2026.

## Context

Home Assistant's MQTT discovery groups entities visually in exactly two ways:

1. **Per device** — every entity carries a `device` (`dev`) block; HA renders one device card per distinct `identifiers` value and lists that device's entities on its page. Devices can be nested with `via_device`, which references the `identifiers` of a parent device so child devices appear under the parent.
2. **Per `entity_category` within a device** — HA splits a single device's entities into fixed sections: primary (`entity_category` absent), **Diagnostic** (`entity_category: diagnostic`), and **Configuration** (`entity_category: config`). The category set is a closed enum (`HaEntityCat` in `MQTTstuff.h:150`); HA does not support custom categories.

Until now the firmware registers **every** entity under a single device — `"OpenTherm Gateway (<hostname>)"`, `identifiers = <nodeId>` — built by `writeDeviceBlock()` (`mqtt_configuratie.cpp:1926`). The only grouping a user sees is HA's automatic `entity_category` split. As a result, three logically distinct diagnostic sources collapse into one undifferentiated **Diagnostic** list on the gateway card:

- **OpenTherm diagnostics** — OT-message-derived entries (`HaEntityCat::diagnostic`): master/slave configuration, ASF/OEM fault flags, TSP, fault-history buffer, etc. These describe the OpenTherm conversation between thermostat and boiler.
- **ESP / firmware diagnostics** — pseudo-ID 247 (`otgw-firmware/stats/*`, 17 heap/discovery counters, TASK-346) and pseudo-ID 248 (`reboot_count`, `reboot_reason`, `version`, `hostname`, TASK-540). These describe the ESP8266 host, not OpenTherm.
- **PIC firmware** — pseudo-ID 249 (PIC info), 250 (PIC settings, 15 entries), 251 (PIC controls: reset button + GPIO/LED selects), all under the `otgw-pic/` subtree (ADR-065). These describe the OpenTherm Gateway PIC, a separate physical component.

Field feedback (Discord) asks that PIC firmware diagnostics be separated from OpenTherm diagnostics, and that ESP/stats and PIC be distinct from one another. Because `entity_category` is a closed enum, the requested separation cannot be expressed by adding categories — it requires distinct devices.

ADR-041 defines *when* discovery is published and ADR-065 defines the `otgw-pic/` topic subtree, but neither specifies the HA *device topology*. Today's single-device topology is implicit in `writeDeviceBlock()` and undocumented.

## Alternatives Considered

- **Status quo — single device, one Diagnostic list.** Rejected. This is the exact arrangement field testers asked to change; PIC, ESP and OpenTherm diagnostics are indistinguishable in the HA UI.
- **New `entity_category` values (e.g. `pic`, `esp`).** Impossible. `entity_category` is a closed HA enum (`config`/`diagnostic`/none); HA ignores unknown values and would drop the entity from any section. Not a real option.
- **Push grouping to the HA side via labels / areas / template helpers.** Rejected. Invisible to users who do not hand-author HA configuration, divergent per installation, and contrary to ADR-072's principle that the firmware ships clean defaults rather than offloading UX onto every install.
- **Separate top-level devices with no `via_device` link.** Rejected. Three unrelated top-level "OTGW…" devices clutter the device list and lose the visual parent/child relationship; a user would not see that the PIC and ESP devices belong to the same physical gateway.
- **`via_device` sub-devices nested under the gateway — chosen.** Introduces two child devices (`OTGW PIC (<hostname>)`, `OTGW ESP (<hostname>)`) that reference the main gateway's `identifiers` via `via_device`. HA nests them under the gateway card while giving each its own page and its own Diagnostic section. This is the HA-native mechanism for exactly this grouping need and keeps a single visual root.

## Decision

1. **Three devices via `via_device` nesting.** HA discovery registers three devices for one physical gateway:

   | Device | `identifiers` | `via_device` | Entities |
   |---|---|---|---|
   | Main gateway | `<nodeId>` | — | OpenTherm values + OpenTherm diagnostics (all OT message IDs), climate, number, Dallas sensors |
   | PIC firmware | `<nodeId>-pic` | `<nodeId>` | Pseudo-IDs 249 (PIC info), 250 (PIC settings), 251 (PIC controls) |
   | ESP firmware | `<nodeId>-esp` | `<nodeId>` | Pseudo-IDs 247 (heap/discovery stats), 248 (reboot/version/hostname) |

2. **Device-group selection is by message ID.** A single helper maps each discovery message ID to its device group: 247/248 → ESP, 249/250/251 → PIC, all others → main. The mapping is centralised (one function) so contributors adding a new diagnostic pseudo-ID choose its device in exactly one place. The existing `MQTT_HA_FLAG_IS_PIC_ENTRY` flag retains its sole responsibility — prefixing the `otgw-pic/` topic (ADR-065) — and is **not** overloaded for device selection.

3. **Sub-device names.** `OTGW PIC (<hostname>)` and `OTGW ESP (<hostname>)`. The main device name `OpenTherm Gateway (<hostname>)` is unchanged. The single `writeRam(ctx.hostname)` rule of ADR-072 §4 is superseded only to the extent that device-card builders (not entity-name builders) may emit the hostname; entity `name` fields still never prepend hostname.

4. **`entity_category` is retained inside each device.** PIC and ESP entities keep `HaEntityCat::diagnostic`, so within their own device they appear under that device's Diagnostic section. The separation the user asked for is achieved by the *device* boundary, not by changing categories. The `ha_*` PROGMEM tables' `entity_category` column is therefore untouched.

5. **`unique_id` is unchanged.** Entity `uniq_id` stays `<nodeId>-<label>`; only the `dev` block changes. On OTA upgrade HA re-homes each affected entity to its new device while preserving entity history (the device-id field of an existing entity is mutable in HA without recreating the entity). MQTT discovery topic paths (`<haPrefix>/<component>/<nodeId>/<object_id>/config`) are unchanged, so retained-discovery verification (ADR-062) is unaffected.

6. **Sub-device blocks are self-contained.** Each PIC/ESP entity payload carries the full sub-device block (`identifiers`, `name`, `via_device`) rather than the main device's "first entity full, rest minimal" optimisation, because drip publishing (ADR-041) emits entities individually and retained configs are processed independently by HA. The main device keeps its first-entity-full / subsequent-minimal optimisation.

## Consequences

### Positive

- PIC, ESP, and OpenTherm diagnostics become three visually distinct groups in HA, nested under one gateway card — exactly the field-requested separation, using the HA-native mechanism.
- Device selection is centralised in one message-ID→group helper; adding a future diagnostic pseudo-ID is a one-line change.
- No change to `unique_id`, topic paths, or `entity_category` values, so entity history survives and retained-discovery verification (ADR-062) needs no change.
- The `otgw-pic/` topic subtree (ADR-065) and the device topology are now independent concerns, each with a single responsibility.

### Negative

- One-time device re-homing on OTA upgrade: affected entities move from the gateway card to the new PIC/ESP cards. Existing dashboards that reference entities by `entity_id` are unaffected; dashboards that reference the *device* will need the two new devices added.
- Each PIC/ESP entity payload is a few dozen bytes larger (full sub-device block on every entity). For the ~21 ESP entities and ~25 PIC entities this is a small, bounded increase well within the discovery heap budget (`MQTT_DISCOVERY_HEAP_MIN`).
- Three device cards instead of one is marginally more to scan for a user who preferred everything in one place; mitigated by the `via_device` nesting that keeps them under a single root.

### Risks

- A contributor adding a new diagnostic pseudo-ID could forget to extend the message-ID→group helper, leaving the entity on the main device. Mitigated by the helper being the single decision point and by code review.
- HA processes `via_device` lazily; if a sub-device entity is discovered before any main-device entity, HA creates a stub parent and links it once the main device arrives. Drip order is message-ID ascending, so the main device (low IDs) is always discovered before the 247+ pseudo-IDs, but the design does not *depend* on that ordering — `via_device` resolves regardless of arrival order.

## Related Decisions

- [ADR-041](ADR-041-jit-ha-discovery.md) — JIT HA discovery pipeline; defines *when* discovery publishes. This ADR fills the gap of *what device topology* it publishes into.
- [ADR-065](ADR-065-otgw-pic-mqtt-subtree.md) — `otgw-pic/` topic subtree; `MQTT_HA_FLAG_IS_PIC_ENTRY` keeps its topic-prefix responsibility and is not reused for device selection.
- [ADR-067](ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md) — entity-id stability across upgrades; the device re-homing from this decision flows through that mechanism on first OTA after upgrade.
- [ADR-072](ADR-072-ha-discovery-friendly-name-format.md) — friendly-name format and the single-`writeRam(ctx.hostname)` rule; narrowed here so device-card builders (not entity-name builders) may emit hostname for the two new sub-devices.
- TASK-346 — heap/discovery statistics (pseudo-ID 247), now homed on the ESP device.
- TASK-540 — diagnostic discovery (pseudo-IDs 248/249/250), split across ESP (248) and PIC (249/250) devices.
- TASK-810 — implements this decision on the dev (1.5.x/1.6.x) branch.
- 2.0.0 sibling: a coherent ADR on the 2.0.0 worktree codifies the same topology there (separate file because each worktree has its own ADR numbering; the decision is coherent across both).

## References

- `src/OTGW-firmware/mqtt_configuratie.cpp:1926` — `writeDeviceBlock()`, the single device-block builder to be extended.
- `src/OTGW-firmware/MQTTstuff.h:150` — `HaEntityCat` closed enum (why categories cannot be added).
- `src/OTGW-firmware/MQTTstuff.h:267` — `HaDiscoveryContext`, to carry the per-entity device group.
- `src/OTGW-firmware/mqtt_configuratie.cpp:1045-1097` — pseudo-ID 247/248/249/250 sensor-table entries.
- `src/OTGW-firmware/MQTTstuff.ino:1746` — pseudo-ID 251 PIC controls (button/select) discovery path.
- Home Assistant MQTT discovery `device`/`via_device` documentation.

## Enforcement

```json
{
  "llm_judge": false
}
```

This decision governs the *shape* of the JSON `device` block emitted at runtime, which a static line-diff regex cannot validate (the topology is correct only when `writeDeviceBlock` routes each group to the right `identifiers`/`via_device`, observable only in the published payload). It is therefore manual-review-only: reviewers confirm that (1) the message-ID→group mapping in `doAutoConfigureMsgid` (`MQTTstuff.ino`) still routes 247/248→esp and 249/250/251→pic, and (2) `writeDeviceBlock`/`writeSubDeviceBlock` emit `via_device` for sub-devices. Field validation in Home Assistant (entities appear under the OTGW PIC / OTGW ESP cards nested under the gateway) is the behavioural test.
