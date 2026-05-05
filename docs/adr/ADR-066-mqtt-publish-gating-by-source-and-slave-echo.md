# ADR-066: MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification

**Status:** Proposed
**Date:** 2026-04-28
**Classification:** structural (no CI gate, manual review at PR time)
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)

## Context

ADR-040 introduced opt-in source-separated MQTT topics (`/thermostat`, `/boiler`, `/gateway` subtopics under each metric) while preserving the legacy "base" topic at `<prefix>/value/<node-id>/<metric>` for backward compatibility. ADR-052 established the publish eligibility contract (first-seen OR value-changed OR stale-refresh).

Between v1.3.5 and v1.4.1, `is_value_valid()` in `OTGW-Core.ino` was widened from accepting only `OT_WRITE_DATA` to accepting both `OT_WRITE_DATA` and `OT_WRITE_ACK` for `OT_WRITE` and `OT_RW` message commands. The motivation was to enable source-separated topics to surface boiler-clamped values on the `/boiler` subtopic (e.g. MaxTSet which the slave clamps to its own internal range).

The legacy base topic continued to receive both Write-Data and Write-Ack publications. For OpenTherm message IDs where the slave does not store the master-supplied value (and the OT v4.2 spec defines the Write-Ack data field as undefined, typically returned as `0`), this caused the base topic to flap between the master's actual value and the slave's protocol-zero. Field reports identified Tr (24), TrSet (16), and MaxRelModLevelSetting (14) as user-visible cases.

Two design errors converged into one user-facing regression:

1. The base topic became the union of two semantically different streams (master writes a real value; slave acks with undefined value). The legacy contract from v1.3.5 was "base topic carries the thermostat-side value only".
2. The `/boiler` subtopic accepted Write-Ack values without checking whether the slave's reply had meaningful data. For non-echo MsgIDs the `/boiler` subtopic became a fake-zero stream rather than a useful per-source observability surface.

The OpenTherm v4.2 specification distinguishes message classes that imply slave-echo behavior (Configuration, Pre-Defined Remote Boiler Parameters, Transparent Slave Parameters, R/W counters) from those that do not (Class 4 sensor data sent from master to slave, Class 8 control of special applications). The per-MsgID classification was never encoded in the firmware's `OTlookup_t` metadata table.

### Constraints

- **Backward compatibility:** existing HA discovery entities and MQTT subscribers tied to base topics must continue to work without entity-ID changes.
- **Source-separation opt-in:** ADR-040 made source-separated topics opt-in via `settings.mqtt.bSeparateSources`; that flag default remains `false`.
- **Memory limits:** static-buffer friendly per ADR-004, ADR-044. New per-MsgID metadata adds at most one byte per OTlookup entry.
- **Spec-driven:** classifications must be traceable to OT v4.2 spec text or captured boiler-log evidence; default to "echo=true" (publish) when ambiguous, to avoid suppressing meaningful data.

## Decision

**Gate MQTT publication of OpenTherm Write-Ack values per topic-class:**

1. **Base topic (`<prefix>/value/<node-id>/<metric>`)** publishes the v1.3.5 set: Read-Ack for `READ` and `R/W` commands, Write-Data for `WRITE` and `R/W` commands. Write-Ack is **never** routed to the base topic. This restores the legacy contract that the base topic represents the thermostat-side intent.

2. **Source-specific subtopics (`/thermostat`, `/boiler`, `/gateway`)** continue to publish under the wider validity rule (Read-Ack, Write-Data, Write-Ack), gated additionally by the per-MsgID `bSlaveEchoesValue` flag for Write-Ack publications. When `bSlaveEchoesValue == false`, the `/boiler` subtopic is **not** updated for Write-Ack messages. The `/thermostat` subtopic is unaffected (Write-Data routing unchanged).

3. **`OTlookup_t` struct gains a `bool bSlaveEchoesValue` field**, populated for every MsgID in the OTlookupArr based on the spec audit at `docs/api/MQTT-message-id-echo-audit.md`. Default for unknown or future MsgIDs is `true` (publish). The audit doc is part of this decision and must be updated in lock-step with `OTlookupArr` changes.

### Implementation primitives

- A new helper `is_value_valid_for_master_topic(OT, OTlookup)` mirrors `is_value_valid` minus the Write-Ack acceptance for `OT_WRITE` / `OT_RW` commands. Used by the base-topic publish call.
- `is_value_valid(OT, OTlookup)` is unchanged (still accepts Write-Ack) and is used by the source-separation publish path.
- `publishToSourceTopic` adds an early return when `OT.type == OT_MSGTYPE_WRITE_ACK && !OTlookup.bSlaveEchoesValue`.

### Per-MsgID classification

Encoded in `docs/api/MQTT-message-id-echo-audit.md`. Initial release marks 6 MsgIDs as `bSlaveEchoesValue = false`:

- 14 (Max-rel-mod-level-setting), 16 (TrSet), 23 (TrSetCH2), 24 (Tr), 37 (TrCH2), 98 (RF sensor status info)

All other MsgIDs default to `bSlaveEchoesValue = true`. For MsgIDs without write support (`R/-`) the field is moot but set to `true` for consistency.

## Consequences

### Positive

- **Regression closed:** the user-visible flapping on `Tr`, `TrSet`, and `MaxRelModLevelSetting` since v1.4.1 stops without breaking any existing HA entity-ID, MQTT topic path, or YAML configuration.
- **`/boiler` subtopic becomes meaningful:** for the 6 non-echo MsgIDs, no fake-zero readings pollute the per-source observability surface. For MsgIDs where the slave does echo (most notably MaxTSet, Class 5 remote parameters, Class 6 transparent slave parameters, R/W counters), `/boiler` continues to surface the slave's actual stored value, including clamped variants distinct from the master's request.
- **Spec-traceable:** the audit doc records the rationale per MsgID with a citation back to OT v4.2 reference. Future MsgID additions or classification changes are visible in code review.
- **Backwards compatible:** ADR-040's "base topic always published" property is amended in scope, not removed. The base topic still publishes for every Read and every Write-Data; only Write-Ack routing changes. No HA discovery `unique_id` changes; no retained-value invalidation strategy needed (next Write-Data overwrites).

### Negative

- **One additional metadata field per MsgID** in `OTlookup_t`. Memory cost: 1 byte per entry on ESP8266, negligible.
- **Conservative defaults may publish protocol-zero on yet-unknown non-echo MsgIDs** until evidence or spec analysis flips the flag. Mitigation: drift-monitoring via user reports; the audit doc has a "Future extensions" section listing low-confidence candidates.
- **Coupling between publish-time check and per-MsgID metadata.** The added gate in `publishToSourceTopic` introduces a dependency on `OTlookup` access at call site. The signature change (or addition of a free helper that consults OTlookupArr) must be done with care to avoid hot-path lookup overhead.

### Neutral

- **Existing source-separation users (`bSeparateSources=true`)** see behavior change for the 6 non-echo MsgIDs only: their `/boiler` subtopic for those metrics stops receiving updates. Retained values on those subtopics remain stale until manually cleared. This is the intended outcome (the prior values were spurious zeros).

## Verification gates

1. **Completeness:** all four sections (Status, Context, Decision, Consequences) populated; all referenced ADRs (-040, -052) and TASK references valid; audit doc cited and present.
2. **Evidence:** field-report logs (Intergas, dev branch 1.5.0-beta+cd30617) showing the three confirmed non-echo cases retained in TASK-478 description and audit doc. Spec citation to `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md` for each non-echo entry.
3. **Clarity:** the decision is implementable from the text alone (function names, struct field, call-site changes spelled out). The audit doc is unambiguous per MsgID.
4. **Consistency:** does not contradict ADR-040 (extends it), does not contradict ADR-052 (refines per-topic eligibility within it). No conflict with ADR-006 (MQTT integration), ADR-038 (OT data flow pipeline), ADR-049 (no String in protocol path), ADR-051 (settings/state encapsulation).

## Related

- **ADR-040:** MQTT Source-Specific Topics for OpenTherm Values (this ADR amends scope of "base topic always published" rule).
- **ADR-052:** MQTT Publish Eligibility and Reconnect Refresh Contract (this ADR refines per-topic-class eligibility).
- **ADR-051:** Dual Encapsulating Structs (provides the `state.*` / `settings.*` separation that `bSeparateSources` lives in).
- **TASK-478:** Implementation task tracking the code changes that realize this decision.
- **`docs/api/MQTT-message-id-echo-audit.md`:** the per-MsgID classification table.
- **OpenTherm v4.2 specification reference:** `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`.

## Amendment 1 — PS=1 summary path (TASK-483, 2026-05-02)

The original decision (sections above) closed the live OT-bus path: `print_f88` / `print_s16` / `print_s8s8` / `print_u16` and `publishToSourceTopic` now gate on the master-topic invariant.

A regression report from `_reuzenpanda_` on Discord `#beta-testing` (2026-04-30) revealed that v1.5.0-beta.4 still showed flapping on Tr / TrSet for users running with the PIC in `PS=1` (Print Summary) mode. Code-path analysis confirmed `publishPSSummaryFieldValue()` in `OTGW-Core.ino` was the only remaining ungated writer to the non-echo MsgID set; it bypassed both the master-topic publish gate and the `OTcurrentSystemState` write gate.

The `PS=1` stream emits one value per MsgID, chosen by the PIC from its most recent observation. For MsgIDs with `bSlaveEchoesValue=false`, the PIC may have captured either the meaningful Write-Data or the per-spec undefined Write-Ack byte; the `PS=1` layer cannot distinguish these. The amendment therefore suppresses publication entirely for non-echo MsgIDs in PS=1 mode, rather than attempting per-frame disambiguation.

### Amendment decision

1. **A new helper `is_msgid_valid_for_master_topic_in_ps_summary(OTlookup)`** mirrors the master-topic invariant for the PS=1 context. It evaluates `true` for `OT_READ` MsgIDs (slave's Read-Ack always meaningful) and falls back to `OTlookup.bSlaveEchoesValue` otherwise. There is no `OpenthermData_t` parameter because `PS=1` carries no Write-Data / Write-Ack distinction at this layer.

2. **`publishPSSummaryFieldValue()` computes `validForMaster` once** at function entry, using the global `OTlookupitem` already populated by the caller (`PROGMEM_readAnything(&OTmap[msgid], OTlookupitem)` at `OTGW-Core.ino:3653`). Each value-bearing case (`ot_f88`, `ot_s16`, `ot_u16`, `ot_s8s8`, `ot_u8u8`, `ot_u8`) gates `sendMQTTData(label, ...)` and the `OTcurrentSystemState` writes on `validForMaster`. The `ot_flag8flag8` case is untouched (status-flag semantics: per-MsgID switch already inside the case, parallel to the live-bus exception for `OT_Statusflags` / `OT_StatusVH` / `OT_SolarStorageMaster`).

3. **`setMsgLastUpdated()` remains called regardless** of `validForMaster`. The epoch tick is cosmetic (drives WebUI freshness), consistent with the live-bus path at `OTGW-Core.ino:4034` where `setMsgLastUpdated` is gated only on the broader `is_value_valid` (not on the master-topic invariant).

4. **One `DebugTln` trace per suppressed call** at function entry, format `"PS=1 master-topic gate suppressed MsgID %u (%s): bSlaveEchoesValue=false"`. Lets support correlate symptom ("value missing in HA") with port-23 telnet logs without enabling extra debug categories.

### Effect on existing PS=1 users

- **Boilers that echo all relevant MsgIDs (`bSlaveEchoesValue=true` for everything in their summary):** unchanged behaviour.
- **Boilers where Tr / TrSet / TrSetCH2 / TRoomCH2 / MaxRelModLevelSetting / RFsensorStatus appear in the PS=1 summary:** these six MsgIDs stop publishing to the base topic and stop updating `OTcurrentSystemState`. HA entities tied to those base topics will go stale; their last retained value remains until cleared. This is the intended outcome (the prior PS=1 values were the same protocol-zero garbage the live-bus path already suppresses).

### CI gate

Per ADR-080, this amendment is binding-pattern-level: a new `evaluate.py` check `check_ps_summary_master_topic_gate` ensures any future case added to `publishPSSummaryFieldValue` is wrapped in the `validForMaster` guard. Without the CI gate, a future contributor could add a new `case ot_xxx:` and silently re-introduce the regression.

## Future amendments

If a user reports flapping on a MsgID currently set to `bSlaveEchoesValue=true`, a captured Write-Data / Write-Ack pair from the boiler in question, plus this ADR amended (or a successor ADR), is sufficient to flip the flag. The audit doc is the source of truth; the OTlookupArr initializers are kept in sync at PR-review time.
