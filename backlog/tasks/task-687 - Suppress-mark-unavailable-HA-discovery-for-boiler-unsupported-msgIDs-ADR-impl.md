---
id: TASK-687
title: >-
  Suppress / mark-unavailable HA discovery for boiler-unsupported msgIDs (ADR +
  impl)
status: To Do
assignee:
  - '@claude'
created_date: '2026-05-24 06:47'
updated_date: '2026-06-20 12:24'
labels:
  - ha-discovery
  - opentherm
  - observability
  - user-experience
  - adr-required
  - milestone-2.1.0
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 59000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
**Context.** The OT spec already provides a per-msgID 'I don't implement this' signal — msg-type 7 Unknown-Data-Id from the slave. TASK-684/685 records this in a per-msgID, per-direction bitmap, and TASK-686 surfaces the list via REST, MQTT, and the WebUI. The bitmaps are now an authoritative source of 'what the boiler does NOT support'.

Today the firmware still publishes Home Assistant discovery configs for EVERY msgID it knows how to decode, regardless of whether the actual boiler implements them. Concrete consequences observed in crashevans's beta.20 capture:
- HA gets a 'Texhaust' sensor that will never receive a real value. Cards / automations referencing it show 'unknown' or 'unavailable' indefinitely.
- HA gets a 'Toutside' sensor that the boiler ignores. Some users wire that to a weather automation and silently lose the connection.
- Read-direction Tr / Tdhw / ElectricalCurrentBurnerFlame entities exist but never update on this boiler.
- The dashboard noise breeds 'why is my OTGW broken?' support questions on Discord.

**Decision space (the ADR will pick one).** This is architecturally significant and requires an ADR because it touches the HA discovery contract governed by ADR-070 / ADR-071. The candidate approaches:

A. **Skip discovery entirely** for msgIDs flagged unsupported. Cleanest UX (entity never appears in HA), but the most stateful — needs a recovery path if the boiler later reports the msgID as supported (e.g. after a PIC firmware upgrade or thermostat replacement).

B. **Publish discovery, set availability_topic to 'offline'.** HA still shows the entity but renders it as unavailable. Less surprising to users who already have automations bound to the entity. Recovery is automatic (when the boiler answers, the availability flips back to online).

C. **Publish discovery once, but with state_class='unsupported_by_boiler'** or a similar marker. Most explicit, but introduces a non-standard HA convention and may not render usefully in stock HA cards.

D. **Hybrid:** Suppress discovery for unsupported entities for the FIRST boot of a given installation (clean dashboard), but if the entity is already known to MQTT (retained discovery from a previous firmware version), flip it to availability=offline instead of unpublishing — avoiding the 'why did my entity disappear?' surprise on upgrade.

The recovery problem is shared by A, B, and D: if a previously-unsupported msgID later starts returning data, the bitmap entry must be re-evaluated and the discovery config re-published. The most likely trigger is a PIC firmware swap; the test plan needs to cover that.

**Why this is parked for 2.1.0, not done now.**
- The decision is irreversible in MQTT/HA users — once a discovery topic is unpublished, retained-payload consumers either auto-clean (good HA installs) or accumulate stale config (older HA installs). The ADR needs to spell out the migration story for existing 1.5.x / 1.6.x installations that have older topics retained on the broker.
- Discovery shape is governed by ADR-070 (sibling-suffix source topics) and ADR-071. The new ADR needs to cross-reference these and either fit inside their constraints or supersede the relevant section.
- A revert path matters — if a beta tester finds a corner case where 'support detected as false' is wrong (e.g. a thermostat that pre-flights every msgID at boot, gets Unknown-Data-Id, then never asks again, so the entity is permanently hidden even though the boiler would actually answer if asked), we need a runtime override.

**Scope of this task when it is picked up:**
1. Draft an ADR proposing one of the four approaches (or a fifth that emerges from discussion). The ADR follows the standard ADR-Kit gates (Completeness, Evidence, Clarity, Consistency) and the four-gate review in .claude/adr-kit-guide.md.
2. Land the ADR as Proposed -> Accepted after maintainer review.
3. Implement the chosen approach in MQTTstuff.ino / mqtt_configuratie.cpp where the discovery publisher lives. Reuse the unknownLoggedRead/Write bitmaps from TASK-684/685 — zero net new RAM.
4. Add a runtime override (e.g. an MQTT command 'force-publish-discovery <id>') for the corner-case escape hatch.
5. Field-test on at least three boiler models with different msgID-support profiles via Discord beta-testing.
6. Update the HA integration docs / README to describe the new behaviour and how users can override it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR drafted in docs/adr/ with Status: Proposed. The ADR cites the four candidate approaches, picks one with explicit rationale, and cross-references ADR-070/071. The decision section is concrete (no 'consider' / 'should we' hedging).
- [ ] #2 ADR reaches Status: Accepted after maintainer review. The four ADR-Kit gates (Completeness, Evidence, Clarity, Consistency) pass.
- [ ] #3 Discovery publisher in MQTTstuff.ino / mqtt_configuratie.cpp honours the unknownLoggedRead / unknownLoggedWrite bitmaps from TASK-684/685 according to the accepted ADR. Zero net new RAM for the new state.
- [ ] #4 Runtime override mechanism (MQTT command 'force-publish-discovery <id>' or equivalent) is implemented and documented so testers can recover from a false-positive 'unsupported' classification.
- [ ] #5 Recovery path is covered: if a previously-unsupported msgID receives Read-Ack/Write-Ack at runtime (bitmap bit clears), the discovery topic / availability state for that entity is re-published within one drip cycle.
- [ ] #6 Field-validated by at least three Discord beta testers with distinct boiler models. Each tester confirms: (a) unsupported entities are correctly hidden / marked-unavailable in HA, (b) supported entities still receive values, (c) the override works as documented.
- [ ] #7 Docs updated: docs/guides/ and the README HA section describe the new behaviour and the override mechanism.
- [ ] #8 python build.py --firmware exits 0.
- [ ] #9 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Session blocked by tool-output channel failure (Bash/PowerShell/Grep-content/Read all returning empty or truncated). Could not read MQTTHaDiscovery.cpp or the unsupported-msgID tracking in OTGW-Core.ino. No ADR or code written. OTGW-Core.ino also reported modified externally mid-session (possible concurrent agent). Needs a fresh session.

RETRACTION of prior note: earlier 'tool channel failure' was a false alarm from parallel speculative search calls, not a real blocker. Investigation is proceeding normally; MQTTHaDiscovery.cpp and unsupported-msgID tracking located and being read.

INVESTIGATION COMPLETE (design), IMPL BLOCKED by tool-result channel dropping output mid-session (Read/Grep/Bash returning empty since ~mid-session; ADR-077/088 + restAPI + OTGW-Core tracking were read successfully earlier).

== Unsupported tracking (source of truth) ==
OTGW-Core.ino:3996-4025: boilerUnsupportedRead[32], boilerUnsupportedWrite[32] bitmaps (256 bits each). Set in processOT at OTGW-Core.ino:4214-4224 on OT_UNKNOWN_DATA_ID (msg-type 7), direction from boilerLastMasterWasWrite. Query hooks already exist & are exported in OTGW-firmware.h:218-219: isBoilerMsgIdUnsupportedRead(id), isBoilerMsgIdUnsupportedWrite(id). NO NEW QUERY HOOK NEEDED. NO NEW RAM NEEDED (AC#3).
Recovery primitive MISSING: bits are set on unknown-data-id but there is no path that CLEARS a bit when a previously-unsupported id later returns READ_ACK/WRITE_ACK (OTGW-Core.ino:4204-4213 sets boilerAckedRead/Write but never clears boilerUnsupported*). AC#5 (recovery within one drip cycle) requires adding a clear-on-ack in that same block.

== Discovery publisher path ==
Drip lives in MQTTstuff.ino (OUT OF SCOPE for me): MQTTautoCfgPendingMap[8] bitmap, markAllMQTTConfigPending(), loopMQTTDiscovery() drips one OTid/tick -> doAutoConfigureMsgid(OTid). The per-entity stream/compose functions live in MQTTHaDiscovery.cpp (IN SCOPE). The unsupported-gate belongs at the per-id compose entry in MQTTHaDiscovery.cpp so MQTTstuff.ino drip loop is untouched and ADR-088 burst-windowing/ADR-077 streaming stay intact.

== Design recommendation: Option B (mark-unavailable via availability_topic), NOT suppress ==
Rationale: (a) avoids the retained-topic-cleanup + re-discovery problem that A/D carry (suppress => must publish empty retained payload to delete, and must re-publish full config on recovery); (b) recovery is automatic+cheap: flip availability OFF->ON, no re-stream of the ~300-1200B config; (c) no surprise entity-disappear on upgrade for users with existing automations (matches feedback: minimal surprise); (d) bit-set is a heuristic (some thermostats pre-flight every id once) -> unavailable is the safe, reversible classification, suppress is destructive on a false positive. Cost: HA still creates the entity (dashboard not 100% clean) -> mitigated by HA grouping unavailable entities and by the existing Statistics-panel/REST surface. Maintainer call point: if a pristine dashboard is valued over reversibility, choose A (suppress) instead — that is the genuine values trade-off for the maintainer.

== Open maintainer decisions ==
1. Suppress (A) vs mark-unavailable (B). I recommend B.
2. availability mechanism: per-entity availability_topic + an availability publish driven by the unsupported bitmap, vs a shared expire_after. Per-entity availability_topic is cleaner but adds one retained topic per gated entity; expire_after is zero-extra-topic but HA marks unavailable only after the window and cannot distinguish 'boiler offline' from 'msgid unsupported'.
3. Runtime override (AC#4): MQTT command force-publish-discovery <id>. Dispatch table lives in MQTTstuff.ino (out of my scope) — needs coordination/maintainer assignment.
4. Milestone is 2.1.0 (not 2.0.0) per task header; confirm this should ship now vs stay parked.

No ADR file or code written this session due to the channel failure (writing without re-Read verification would violate the edit-then-verify contract). Next session: re-read MQTTHaDiscovery.cpp doAutoConfigureMsgid/compose entry, draft docs/adr/ADR-NNN, gate compose on isBoilerMsgIdUnsupportedRead/Write, add clear-on-ack in OTGW-Core.ino:4204-4213.

2026-06-01T18:25:49+02:00: MAINTAINER DECISION 7b — approach B (publish discovery + availability_topic offline; HA renders unavailable, auto-recovers when boiler answers). DEFER to milestone 2.1.0 (task is already milestone 2.1.0). Not pulled into 2.0.0. When picked up: implement B in MQTTHaDiscovery.cpp honouring the unknownLogged bitmaps, + the runtime force-publish override, per the ADR this task still owes (AC#1).

Moved In Progress -> To Do (maintainer decision 2026-06-20): explicitly PARKED for milestone 2.1.0. Only ADR-142 (Proposed) exists; no implementation (availability-list gating, clear-on-ack recovery, runtime override, CI gate, docs all absent). Parking in To Do so the In Progress column reflects actual active work. Unpark when 2.1.0 starts: first drive ADR-142 to Accepted via adr-kit, then implement.
<!-- SECTION:NOTES:END -->
