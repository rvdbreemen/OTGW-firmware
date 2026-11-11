---
id: TASK-1759
title: >-
  Request MsgID 28 (return temp) when no thermostat reads it (OT-Direct gateway
  + PIC)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T17:08:00Z'
updated_date: '2026-11-11T07:48:00Z'
labels:
  - sat
  - opentherm
  - ot-direct
  - pic
dependencies: []
priority: high
ordinal: 860000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-09 16:09 UTC, #dev-sat-mqtt msg 1569383329470488710: "We need to ask for return temperature" (docs/sat/sergeant-d-corrections.md entry 287). tjfs's capture of 2026-11-09 (beta.161, OT-Direct gateway mode, Ideal Halo Lite thermostat, Ideal Logic boiler, SAT on) holds no MsgID 28 frame: the thermostat reads only IDs 0, 1, 16, 17, 24, 25, the boiler supports all of them, and since TASK-1707 (beta.132) the gateway sends its own reads only in the slot of an unsupported thermostat request, so it never reads ID 28; SAT's tick shows ret=0.0. thermo-nova needs it: the PWM flame-off hold sets CS = return temperature + flame-off offset and skips the override with a warning when the return temperature is missing (heating_control.py:392-399); the cycle tracker uses flow minus return (cycles/tracker.py:262-265). Counterpart of entry 276 (MsgID 25 on PIC boards). Related TASK-1707.

Open questions before code, to Sergeant D per the SAT rules (sat-ground-truth, open-questions.md): the method (on OT-Direct gateway mode which slot; on PIC the equivalent of entry 276's own request), the interval, and SAT-only or always.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Open questions (method, interval, SAT-only or always) asked to Sergeant D and his answers recorded as entries in docs/sat/sergeant-d-corrections.md before any code
- [ ] #2 OT-Direct gateway mode: the gateway reads MsgID 28 when no thermostat reads it, as decided; shown in a test and in a bench or tester capture
- [ ] #3 PIC boards: the same, as decided; shown in a test and in a capture
- [ ] #4 SAT sees a valid return temperature on such an installation (tick ret != 0.0) and the flame-off hold uses it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN (draft by otgw-firmware-030509, 2026-11-09; no code before the open points are answered). Basis: entry 287 (Sergeant D: we need to ask for return temperature), entry 290 (maintainer: A on OT-Direct + C on PIC), Q-1759a (asked, msg 1569468785407299627). Code read at origin/dev 392eb64c6.

WAITING FOR SERGEANT D (Q-1759a stays asked; entry 290 is the maintainer's):
- W1 the interval (proposal 30 s);
- W2 SAT only or always (proposal: SAT only);
- W3 his agreement with A+C.
Robert / review also decide W4: which thermostat read may be replaced, and how old a cached answer may be (proposal below).

PART A, OT-Direct gateway mode (OTDirect.ino)
A1 Where. The relay path of a pending thermostat frame (OTDirect.ino ~2183-2228). Today, in order: otdAltSlotTaken() (TASK-1707, only for a thermostat ID the boiler does not support), then UI table, then SR table, then forward with overrides. New step "otdRetSlotTaken()" right AFTER otdAltSlotTaken() and BEFORE the UI/SR/forward branches. An unsupported-ID slot keeps priority: the schedule then reads MsgID 28 there already (R_ENTRY(28, OT_TEMP_INTERVAL_MS 10 s), OTDirect.ino:154+/59), so A acts only when no such slot comes.
A2 When (all must hold):
- otCurrentMode == OTD_MODE_GATEWAY and a thermostat is connected (state.otd.bThermostatConnected);
- otMaster.isReady();
- SAT enabled (W2; if Sergeant D says always, this gate goes);
- MsgID 28 is not fresh: no valid boiler MsgID 28 reply within the interval W1 (state.otBus.feedTret sampleMs, the same stamp satFeedClassify() reads, SATcontrol.ino:2607);
- the boiler does not answer MsgID 28 Unknown-DataID (isBoilerMsgIdUnsupportedRead(28) false, OTGW-Core.ino:5612; and not boilerMsgId28UnknownConfirmed(), :5608);
- at least W1 since the last replaced frame (own timestamp), so it is at most one replaced frame per interval.
A3 Which thermostat frame (W4 proposal):
- only READ_DATA (type 0). Never WRITE_DATA: a write must reach the boiler (TASK-1701 lesson).
- Never MsgID 0 (status / CH enable), never 9 or 100 (remote override, otdRemoteOverrideHoldsSlot), never an ID with an active SR=/UI= entry, never 28 itself.
- Only an ID with a valid cached boiler answer: otBoilerCache[id] / otBoilerCacheValid[id] (OTDirect.ino:296-297, filled by otBoilerCacheStore :1517-1527) with a new age stamp no older than W1 (see A5).
- Of the eligible reads, take the one the thermostat repeats most often. Simplest version: the first eligible frame after the interval ran out; frequent IDs come up first by themselves. In tjfs's 2026-11-07 log the eligible reads are 25 (116x in 6 min) and 17 (68x); 0 and the writes 1/16/24 are excluded.
A4 What goes out:
- To the boiler: READ_DATA MsgID 28 via sendMasterRequestAsync(..., OT_DIRECT_ORIGIN_GATEWAY). Its reply is a boiler reply: it stamps feedTret and fills Tret / MQTT 'Tret' through the normal path (processOT); that a reply to an OT_DIRECT_ORIGIN_GATEWAY request stamps feedTret (OTGW-Core.ino:6111) is to be verified in the host test.
- To the thermostat: READ_ACK for its own ID with otBoilerCache[id] (as the SR branch builds srResp, :2203-2212), logged 'T' and 'A' through bridgeFrameToParser.
- The schedule entry for 28 gets lastSentMs = now.
A5 New state: an age stamp per cache slot, otBoilerCacheMs[128], set in otBoilerCacheStore(). Today the cache has no age (only a valid flag), so 'cache age' needs it. Size: 512 B BSS. The alternative is one stamp for the replaced ID only; that is smaller, but then A3 can only check that one ID.
A6 Frame budget:
- The boiler still gets exactly one frame per thermostat frame: TASK-1707 AC1 holds (the replaced frame IS the slot).
- At 1 thermostat frame/s (tjfs: 369 frames in 369 s) and W1 = 30 s, 1 frame in ~30 is replaced (~3 %).
- Of ID 25 (every ~3.2 s), about 1 read in 10 is answered from the cache, with a value at most W1 old. In practice it is ~3 s old: the cache is refreshed by the thermostat's own previous read of that ID.
A7 Deviation to record: the PIC never answers a supported request from a cache. This is a deliberate OT-Direct deviation (entry 290), to be written in the OT-Direct docs next to TASK-1707.

PART C, PIC boards (SATcontrol.ino, as the PM=25 logic, ~1890-1930)
C1 Command: AA=28 when SAT becomes active on a PIC in gateway mode; DA=28 when SAT goes off or is disabled. Both go through addCommandToQueue(), with the answer checked ('AA: 28' / 'NS' NoSpaceLeft / 'NF' NotFound).
C2 Duplicates: SetAlternative (gateway.asm:4617-4636) does not check for an existing entry, so repeated AA=28 fills the 32-place EEPROM list. Therefore at enable send DA=28 first (NF ignored), then AA=28: at most one entry. After a crash or reflash the entry stays in EEPROM; the next enable's DA/AA pair cleans it up. Caveat: a user's own AA=28 is removed at SAT off; this must be documented.
C3 EEPROM wear: only at SAT on/off transitions, not per interval.
C4 Limit, to state in the docs and the UI: an alternative only goes out in the slot of a thermostat request the boiler does not support (TASK-1767; gateway.asm:2387-2414). With a thermostat that asks only supported IDs, C reads nothing. SAT then keeps the flow + offset fallback (entries 135/149/151).
C5 Interval on the PIC: not ours to set; the PIC rotates its alternatives list. W1 applies to A only.

TESTS (before any bench)
- A, host test on the real OTDirect.ino slices: extend tests/test_otd_gateway_alt_slots.py and the tjfs replay (tests/otd_gateway_replay_tjfs.py).
  - with tjfs's stream (all IDs supported): MsgID 28 reaches the boiler once per W1;
  - boiler frames == thermostat frames;
  - only READ frames of eligible IDs are replaced;
  - the thermostat gets READ_ACK with the cached value;
  - no replacement while 28 is fresh, while 28 is Unknown, outside gateway mode, without SAT (W2), or when the cached value is older than W1.
  - Red on the current code, green after.
- C, host test of the command sequence: enable -> DA=28, AA=28; disable -> DA=28; no AA while the list answer is NS.
- SAT: with a valid Tret the flame-off hold uses return + offset, not the flow fallback (existing LL-018 tests).

BENCH / FIELD: OT-Direct gateway mode with a thermostat cannot be built on the bench today (TASK-1707 AC4), so the proof is a tjfs capture. The PIC part also needs a capture from a PIC user whose thermostat offers an unsupported slot.

ADDITIONS (coordinator review, 2026-11-09):
STEP 0, before any code: an ADR via /adr-kit:adr, status Proposed, for option A. It is a deliberate deviation from the PIC model: OT-Direct gateway mode answers a supported thermostat read from the boiler cache instead of relaying it 1:1. That touches ADR-223 and the TASK-1707 slot rule (one boiler frame per thermostat frame, own reads only in unsupported slots).
Trade-off to write down:
- Gain: a return temperature for SAT where none exists today. Today SAT runs the flow + offset fallback (entries 135/149/151).
- Cost: about 1 in 10 reads of ID 25 gets a value ~3 s old (at most W1). The 1:1 relay becomes 1:1 per frame, but not per value.
Alternatives in the ADR:
- B, own extra frames (removed by TASK-1701/1707);
- do nothing (keep the fallback);
- only the unsupported slots (status quo; reads nothing on tjfs's setup).
The ADR is decided by Robert; Sergeant D answers W1-W3.
BUDGET ITEM (A5): otBoilerCacheMs[128] (uint32_t) = 512 B of static RAM (BSS, internal DRAM) on every board, including the no-PSRAM OTGW32 .61 (heap tiers TASK-1736/1757). It is accepted as small, but stated explicitly. The smaller alternative is one stamp for the replaced ID (8 B), at the cost of checking only that ID's age.
Check: the BSS delta in the build output (RAM: line) before/after.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 07:xx taken over by otgw-firmware-09 (coordinator, a4's lane orphaned): lane-1759a reset to origin/dev 99e85b61c+ and only the feature commit 0d6e9f8de cherry-picked (853e9efb4 dropped: its cherry-pick 058a6f50d is on dev); banner-only hunks and version files taken from upstream; old HEAD kept as branch lane-1759a-pre-takeover (and NAS refs/backup/2026-11-11/lane-1759a). Tests on the rebased tree: test_otd_return_temp_slot 9/9, test_otd_return_temp_slot_source 2/2, test_otd_gateway_alt_slots 12 OK (1 skip), test_override_reply OK; evaluate --quick 86/0. Bumped to beta.175.

2026-11-11 audit 1795 area E notes for TASK-1759 checked against the code: (1) the age stamp is set only in otdRetNoteReply from handleMasterResponse for a thermostat READ_DATA answered READ_ACK; the loopback path never calls it (comment corrected) -> no stamp from a WRITE_ACK echo or a loopback reply: OK. (2) the cached reply goes through applyResponseModifiers (OTDirect.ino otdRetSlotTaken): OK. (3) GAP FOUND AND FIXED: otBoilerCacheStore keeps a Data-Invalid (or WRITE_ACK) value and only Unknown-DataID clears validity, while the candidate stamp from an earlier READ_ACK stayed fresh, so the slot could answer the thermostat READ_ACK with an invalid value. Fix: any non-READ_ACK reply for the candidate ID, from any origin, clears the stamp. New host case R9 in tests/test_otd_return_temp_slot.py (harness knob g_invalidFrom in test_otd_gateway_alt_slots.py: boiler answers 17/25 Data-Invalid 0xDEAD from 100 s): before the fix cachedbad=1 (FAILED), after 0 (cached=3 in the first 100 s); file 10/10, _source 2/2, alt_slots 12 OK (1 skip); evaluate --quick 86/0.

2026-11-11 08:50 otgw-firmware-09: part A shipped as 2.0.0-beta.175+b5459f8 (origin/dev e3b4229df..b5459f863), zip posted to #alpha-testing msg 1569981951643750426. AC1 ticked: Sergeant D entries 287/291 and Robert entry 290 in docs/sat/sergeant-d-corrections.md predate the code (ADR-224 Accepted). Host evidence AC2: tests/test_otd_return_temp_slot.py 10/10 (R1 r28=10 cached=10 in 6 min, R9 Data-Invalid red before the audit fix, green after), _source 2/2, test_otd_gateway_alt_slots 12 OK (1 skip), evaluate --quick 86/0, full sweep 199/200 (the 1 is the run_tests false negative TASK-1810). Bench .61 (beta.175+cd9d50e, same code, ino.bin 2128992 B identical to b5459f8): OTA app+fs OK, settings 164 keys unchanged, 11 min soak no reboot, heap ~71 KB, MQTT up, sched 104/104. NOT verified on bench: .61 has no boiler and no thermostat and runs master mode, so the slot behaviour is host-test-proven only. Open: AC2 field/tester capture in OT-Direct gateway mode with a thermostat (tjfs), AC3 PIC part C (Q-1759b), AC4 SAT tick ret != 0.0 in the field.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-09T22:04:00Z

---

Q-1759a (msg 1569468785407299627) answered by the maintainer: Robert 2026-11-09 21:52:17 UTC, "I think A+C would be best, for each hardware setup the optimal solution." -> docs/sat/sergeant-d-corrections.md entry 290: A on OT-Direct (replace one frequent supported read by a MsgID 28 read, answer the thermostat from cache), C on PIC boards (AA=28 while SAT is on, DA=28 when off; slot-limited, TASK-1767). Still open (Q-1759a stays asked): interval (30 s proposed), SAT only or always, which read is replaced / cache age, Sergeant D's agreement (entry 287 is his). No code before.

---

created: 2026-11-09T22:39:00Z

---

STEP 0 done as a draft: docs/adr/ADR-224 (Proposed, not accepted), 'OT-Direct gateway mode reads the return temperature in a supported thermostat slot and answers that read from the boiler cache'. Proposed cache variant: one stamp (~8 B, candidate ID, KISS); per-slot otBoilerCacheMs[128] (512 B) as fallback. Open questions in the ADR: W1-W3 (Sergeant D), cache variant and the ADR-223 deviation (Robert). adr-lint --strict: pass (1 advisory).

---

created: 2026-11-10T05:51:00Z

---

Robert, 2026-11-10 06:40 (his words via AskUserQuestion in the coordinator's session, relayed to 030509): ADR-224 'Direction OK, 8 B cache'. So the deviation from ADR-223 is accepted for this one cache-answered read per interval, and the cache-age variant is the single stamp (~8 B, candidate ID); the 512 B per-slot otBoilerCacheMs[128] is dropped from the plan (A5) unless review reopens it. Recorded in ADR-224 (open questions answered via adr answer). ADR-224 stays Proposed and there is no code until Sergeant D answers W1 (interval), W2 (SAT only or always) and W3 (agreement A+C) under Q-1759a.

---

created: 2026-11-10T08:36:00Z

---

Sergeant D answered Q-1759a (corrections entry 291): 'the answer is A' (2026-11-10 08:29:17 UTC, msg 1569614915222773762) and 'yes' (09:24:44 UTC, msg 1569628868648767570) to our explicit check. So A on OT-Direct gateway mode, every 30 s, only while no valid MsgID 28 arrived for 30 s, and only while SAT is on. ADR-224's open questions are now all answered (W1-W3 via adr answer); acceptance of ADR-224 needs Robert's explicit yes in a session (adr accept --confirm), not done. C on PIC: not answered by Sergeant D; queued as Q-1759b; C stays Robert's maintainer decision (entry 290). Code for A may start after ADR-224 is accepted.

---

created: 2026-11-10T15:50:00Z

---

C part changed by Sergeant D (corrections entry 296; 2026-11-10 16:38:27 UTC, 'option 3 is my choice', msg 1569738014878605403): on PIC boards SAT sends PM=28 every 30 s, not AA=28/DA=28. The plan items C1-C3 (AA/DA, EEPROM, duplicates) drop; C4 (slot limit, TASK-1767) stays: PM= also goes out only in an unsupported slot. Our reading (not asked for PIC): same conditions as A (only while no valid MsgID 28 for 30 s, SAT on, not while MsgID 28 is Unknown), modelled on the PM=25 logic (SATcontrol.ino satPm25Decide, TASK-1412.5). Robert may confirm the change to entry 290. The C part is a separate change from A (ADR-224, OT-Direct only).

---

created: 2026-11-10T16:55:00Z

---

Robert decided (2026-11-10 ~17:0x, via AskUserQuestion in the coordinator's session, relayed to 030509): 'Yes, PM=28 every 30 s'. He agrees with Sergeant D's choice (corrections entry 296), which replaces the C part of his entry 290 (AA=28/DA=28). So the PIC part of this task is PM=28 every 30 s. This is a maintainer decision recorded here, not a new corrections entry. Still our interpretation (entry 296): the same conditions as A (only while no valid MsgID 28 for 30 s, SAT on, not while MsgID 28 is Unknown), modelled on the PM=25 logic.

---

created: 2026-11-10T16:56:00Z

---

Correction to the note above: 'Yes, PM=28 every 30 s' is the coordinator's relay of Robert's AskUserQuestion answer; 030509 did not see Robert's own text. Recorded in entry 296 as a relayed, not first-hand, maintainer confirmation.

---

created: 2026-11-10T14:51:00Z

---

Implementation A (ADR-224 Accepted, scope OT-Direct), lane-1759a on origin/dev fc5809372:
- OTDirect.ino otdRetSlot block, inside the TASK-1707 region:
  - otdRetEligibleId: never 0, 28, >=128, 9/100, UI=/SR=, or an unsupported slot.
  - otdRetNoteReply, called from handleMasterResponse: an unmodified thermostat READ answered READ_ACK stamps the candidate. A candidate quiet for more than 30 s is replaced.
  - otdRetWanted: gateway mode + thermostat; satIsEnabled() && sat.bActive && !sat.bSimulation (as the PM=25 gate); not isBoilerMsgIdUnsupportedRead(28); not satFeedFresh(feedTret, 30 s); 30 s since the last slot.
  - otdRetSlotTaken in the relay path, right after otdAltSlotTaken: the thermostat gets READ_ACK with applyResponseModifiers(cache), logged T/A; READ_DATA 28 goes to the boiler (origin GATEWAY); the schedule entry 28 restarts. An overridden read is relayed as usual (applyOverrides check).
- OTGW-firmware.h: satIsEnabled() declared.
- State: 1 B candidate ID + 2 x 4 B stamps (9 B + padding), static. Real size to be read from the ELF.
- Fact checked: OTGW-Core.ino:6104-6111 stamps feedTret from every boiler 'B' frame for MsgID 28, also the reply to a gateway 'R' request; an 'A' frame (bLocalAnswer) stamps nothing.
Tests (host, Windows):
- tests/test_otd_return_temp_slot.py (real code via the TASK-1707 harness; the boiler model stamps feedTret on a 28 READ_ACK as processOT does): new 9/9; old OTDirect.ino 2 fail (R1 count, R1 cache answers; the 7 guard cases also pass on old).
  - R1 tjfs stream 6 min: tframes = bframes = replies = 360; MsgID 28 10x (about every 36 s: the 30 s interval plus the wait for the next candidate read); 10 cache answers, 0 wrong.
  - R2 SAT off / R3 28 fresh / R4 28 unsupported / R5 SAT simulation / R6 candidate slower than 30 s: 0 slots.
  - R7 with an unsupported thermostat ID: 24 TASK-1707 slots still; the cache slot fills in 11x (the shared schedule read 28 only 3x in 6 min there).
  - R8: the ID 25 boiler-read gap is at most 12 s (< SAT_FLOW_VALID_MS 60 s).
- tests/test_otd_return_temp_slot_source.py: the two rules of ADR-224 (30 s; order right after otdAltSlotTaken), kept as a test instead of an Enforcement block, per the coordinator: an Accepted ADR is not edited. New OK; old 2 fail.
- tests/test_otd_gateway_alt_slots.py: harness extended (SAT/feed stubs, inert defaults, feedTret stamp in the boiler model, sentAt times). 12 OK on old and new code.
- Docs: docs/c4/c4-code-otdirect.md, gateway-mode section.
Not done yet: bump (next free number after beta.170's tag), build (slot after the release), the RAM delta from the ELF.
Bench .61: regression check only (OTA, settings diff, a few boots). Without a thermostat in gateway mode MsgID 28 does not come through this slot (TASK-1707 AC4).
Field proof: awaiting a tjfs capture.

---
<!-- COMMENTS:END -->
