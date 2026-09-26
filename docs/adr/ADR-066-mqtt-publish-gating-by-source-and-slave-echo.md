---
id: "ADR-066"
title: "MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification"
status: "Accepted"
date: "2026-09-26"
binding: true
gate: "bSlaveEchoesValue"
documents_shipped: true
verified_in:
  - "src/OTGW-firmware/OTGW-Core.ino"
  - "src/OTGW-firmware/MQTTstuff.ino"
  - "src/OTGW-firmware/OTGW-Core.h"
  - "evaluate.py"
supersedes: []
superseded_by: null
topics:
  - "mqtt"
  - "write-ack"
  - "slave-echo"
  - "source-topics"
  - "ps-summary"
aliases:
  - "Write-Ack flapping"
  - "non-echo MsgIDs"
  - "master-topic gate"
components:
  - "canonical MQTT value topics"
  - "source-separated topics (_thermostat / _boiler)"
  - "PS=1 summary publish path"
symbols:
  - "bSlaveEchoesValue"
  - "is_value_valid_for_master_topic"
  - "is_msgid_valid_for_master_topic_in_ps_summary"
  - "publishToSourceTopic"
  - "check_ps_summary_master_topic_gate"
context_scope: "selective"
---
# ADR-066: MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification

## Status

Accepted, 2026-09-26.

Documents shipped behaviour: implemented in TASK-478 (live OpenTherm bus path)
and TASK-483 (PS=1 summary path), extended by TASK-571 (MsgIDs 1 and 8). The text
was brought in line with ADR-071 and ADR-075 on 2026-09-26, before acceptance;
the decision itself is unchanged.

Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Status History

```yaml
status_history:
  - date: 2026-04-28
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal (TASK-478)
    changed_via: unrecorded
  - date: 2026-09-26
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
```

## Context

Every OpenTherm (OT) value the gateway sees can be published on the canonical
MQTT topic `<prefix>/value/<node-id>/<metric>` and, when
`settings.mqtt.bSeparateSources` is on, also on the source-separated sibling
topics `<metric>_thermostat` and `<metric>_boiler` (ADR-040 introduced source
separation; ADR-071 fixed the sibling-suffix shape). What each topic means is set
by ADR-075 (Accepted): canonical carries the boiler-side worldview, `_thermostat`
what the thermostat sees, `_boiler` what the boiler sees.

An OT write is a two-frame exchange: the master sends Write-Data with a value,
the slave answers Write-Ack. For many message IDs (MsgIDs) the OpenTherm v4.2
specification leaves the data field of that Write-Ack undefined, and boilers
typically return `0` there because they do not store the value. Between v1.3.5
and v1.4.1, `is_value_valid()` in `OTGW-Core.ino` was widened to accept Write-Ack
as well as Write-Data for `OT_WRITE` and `OT_RW` MsgIDs, so that source topics
could show boiler-clamped values (for example MaxTSet, which the boiler clamps to
its own range). For MsgIDs where the boiler does not echo, that also routed the
protocol-zero into the published topics, and Home Assistant entities flapped
between the real value and `0`. Field reports named Tr (24), TrSet (16) and
MaxRelModLevelSetting (14); a heat-pump report later added TSet (1).

Two errors converged:

1. Canonical received both the Write-Data value and the Write-Ack protocol-zero,
   two semantically different streams on one topic.
2. The boiler-side source topic accepted Write-Ack values without checking
   whether the boiler's reply carries meaningful data.

The OT v4.2 specification distinguishes message classes that imply the slave
echoes a written value (Configuration, Pre-Defined Remote Boiler Parameters,
Transparent Slave Parameters, read/write counters) from those that do not (Class
4 sensor data sent from master to slave, Class 8 control of special
applications). That per-MsgID knowledge was not encoded in the firmware.

### Constraints

- **Backward compatibility:** existing Home Assistant (HA) discovery entities and
  MQTT subscribers keep their topics and entity IDs.
- **Source separation stays opt-in:** `settings.mqtt.bSeparateSources` defaults
  to `false` (ADR-040).
- **Memory:** at most one byte per MsgID lookup entry.
- **Spec-driven:** each classification is traceable to OT v4.2 text or captured
  boiler logs. The original default was "treat as echoing when in doubt";
  TASK-571 (2026-05-07) narrowed that for write-only control MsgIDs whose
  Write-Ack data the spec leaves undefined, which now default to non-echo.

## Decision

**Never publish a Write-Ack value on canonical, and publish a boiler Write-Ack on
the source topics only for MsgIDs whose slave echoes the written value, as
recorded by a per-MsgID `bSlaveEchoesValue` flag.**

1. **Canonical** accepts Read-Ack for `READ` and `R/W` MsgIDs and Write-Data for
   `WRITE` and `R/W` MsgIDs. Write-Ack never reaches canonical. This is
   `is_value_valid_for_master_topic()` (`OTGW-Core.ino:1217`), which also carries
   the ADR-075 worldview gates (an answer-override `A` and a gateway-substituted
   `T` do not reach canonical).
2. **Source topics** (`_thermostat`, `_boiler`) keep the wider validity rule of
   `is_value_valid()`, with one gate: a Write-Ack from a real boiler frame (`B`)
   of a MsgID with `bSlaveEchoesValue == false` is not published to any source
   topic (`publishToSourceTopic()`, `MQTTstuff.ino:1552-1554`). Gateway-built
   answer frames (`A`) are not affected, because their value is deliberately
   constructed. Which source topic a frame reaches is governed by ADR-075.
3. **`OTlookup_t` carries `bool bSlaveEchoesValue`** (`OTGW-Core.h:353`),
   populated for every MsgID in `OTmap[]`. The classification and its evidence
   live in `docs/api/MQTT-message-id-echo-audit.md`, which is part of this
   decision and changes in lock-step with `OTmap[]`. Unknown and future MsgIDs
   default to `true`, unless the spec leaves the Write-Ack data undefined for a
   write-only control MsgID, where the defensive default is `false` (see the
   classification below). The audit document, not this ADR, is the authoritative
   list of non-echo MsgIDs.
4. **PS=1 summary mode** follows the same invariant. The PIC's summary line
   carries one value per MsgID with no Write-Data / Write-Ack distinction, so for
   `WRITE` / `R/W` MsgIDs with `bSlaveEchoesValue == false` the summary value is
   not published and does not update `OTcurrentSystemState`
   (`is_msgid_valid_for_master_topic_in_ps_summary()`, `OTGW-Core.ino:1244`,
   used by `publishPSSummaryFieldValue()`). `READ` MsgIDs always publish. Status
   flag MsgIDs keep their own handling. One debug line per suppressed value,
   `PS=1 master-topic gate suppressed MsgID ...`, lets support correlate a
   missing HA value with the telnet log. `setMsgLastUpdated()` still runs.
5. **A continuous-integration (CI) gate** in `evaluate.py`
   (`check_ps_summary_master_topic_gate`, `evaluate.py:1057`) fails when a value
   case in `publishPSSummaryFieldValue()` is not wrapped in the gate, so a new
   case cannot silently reintroduce the flapping.

### Current non-echo classification

At the time of writing `OTmap[]` (`OTGW-Core.h:358` onward) and the audit
document agree on ten MsgIDs with `bSlaveEchoesValue = false`: 1 (TSet),
7 (CoolingControl), 8 (TsetCH2), 14 (MaxRelModLevelSetting), 16 (TrSet),
23 (TrSetCH2), 24 (Tr), 37 (TRoomCH2), 71 (ControlSetpointVH) and 98 (RF sensor
status). The original release had six (14, 16, 23, 24, 37, 98). TASK-571 added
1 after a heat-pump controller returned `0` in the TSet Write-Ack, and added 7, 8
and 71 on 2026-05-07 by analogy, under a defensive-default policy: where the spec
does not require an echo, the Write-Ack is not trusted. For 7 and 71 there is no
field evidence yet. Changing the list needs a captured Write-Data / Write-Ack
pair or spec text, recorded in the audit document.

## Alternatives Considered

### Alternative A: status quo, accept Write-Ack everywhere

Publish every Write-Ack to canonical and to the source topics.

**Rejected** because the field-reported flapping is real, user-visible noise, and
the boiler-side source topic becomes a stream of fake zeros for exactly the most
watched temperature MsgIDs.

### Alternative B: a `switch (msgId)` at each publish call site

Encode the non-echo MsgIDs at the call sites instead of in the lookup table.

**Rejected** because the classification belongs with the MsgID definition. Two
publish paths exist (live bus and PS=1), each would need an identical switch, and
a future path would silently miss it. One byte per lookup entry buys a single
source of truth.

### Alternative C: suppress the source topics entirely for non-echo MsgIDs

Publish nothing on `_thermostat` / `_boiler` for these MsgIDs.

**Rejected** because Write-Data on these MsgIDs is the master's meaningful value
and is exactly what `_thermostat` should show. Only the Write-Ack is the problem,
which is what the chosen gate removes.

### Alternative D (chosen): per-MsgID `bSlaveEchoesValue` plus Write-Ack excluded from canonical

As described in the Decision.

## Consequences

### Positive

- **Regression closed:** Tr, TrSet, MaxRelModLevelSetting and, for heat pumps,
  TSet stop flapping, without any HA entity-ID or topic change.
- **The boiler-side source topic is meaningful:** no fake zeros for non-echo
  MsgIDs, while echoing MsgIDs (for example MaxTSet, remote boiler parameters,
  read/write counters) still show the boiler's stored value, including clamped
  values that differ from the master's request.
- **Spec-traceable:** every classification cites spec text or a boiler capture in
  the audit document, visible in code review.
- **Guarded:** the PS=1 path is protected by a CI check.

### Negative

- **One byte per MsgID** in `OTlookup_t`; negligible.
- **Defaults can be wrong in both directions:** a not-yet-known non-echo MsgID
  publishes its protocol-zero until evidence flips its flag, and a MsgID set to
  non-echo by analogy (7, 71) could hide a real clamped boiler value on
  `_boiler`. Mitigation: a captured Write-Data / Write-Ack pair from the device in
  question is enough to flip either way.
- **Two documents to keep in step:** `OTmap[]` and the audit document.

### Neutral

- With source separation on, non-echo MsgIDs no longer update their source
  topics from a boiler Write-Ack; their last retained value stays until the next
  valid publish. This is intended: the previous values were spurious zeros.
- In PS=1 mode, non-echo `WRITE` / `R/W` MsgIDs stop publishing from the summary;
  their HA entities keep the last retained value.

## Related Decisions

- **ADR-040 (MQTT Source-Specific Topics for OpenTherm Values):** introduced
  source separation; this ADR narrows which frames feed which topic.
- **ADR-052 (MQTT Publish Eligibility and Reconnect Refresh Contract):** this ADR
  refines per-topic eligibility within it.
- **ADR-071 (MQTT Discovery Topic Sibling-Suffix Shape):** the `_thermostat` /
  `_boiler` topic names used here.
- **ADR-075 (MQTT Source-Topic Worldview Routing, Proxy-Answer Refinement):**
  defines what canonical and each source topic mean and which frame reaches
  which; this ADR's Write-Ack gate applies on top of that routing.
- **ADR-051 (Dual Encapsulating Structs):** where `bSeparateSources` lives.

## References

- `docs/api/MQTT-message-id-echo-audit.md`: the per-MsgID classification and its
  evidence.
- `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`.
- Code: `OTGW-Core.ino:1217` (`is_value_valid_for_master_topic`),
  `OTGW-Core.ino:1244` (`is_msgid_valid_for_master_topic_in_ps_summary`),
  `MQTTstuff.ino:1552-1554` (source-topic gate), `OTGW-Core.h:353`
  (`bSlaveEchoesValue`), `evaluate.py:1057` (CI gate).
- TASK-478 (live bus), TASK-483 (PS=1, reported by `_reuzenpanda_` on Discord
  #beta-testing, 2026-04-30), TASK-571 (MsgIDs 1 and 8).

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "bSlaveEchoesValue",
      "path_glob": "src/OTGW-firmware/MQTTstuff.ino",
      "message": "The source-topic Write-Ack gate on bSlaveEchoesValue must stay in publishToSourceTopic() (ADR-066); without it non-echo MsgIDs flap to protocol-zero."
    },
    {
      "pattern": "is_value_valid_for_master_topic",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "Canonical must be gated by is_value_valid_for_master_topic(), which keeps Write-Ack off canonical (ADR-066)."
    }
  ]
}
```
