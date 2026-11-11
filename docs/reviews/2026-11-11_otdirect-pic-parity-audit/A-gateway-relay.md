# TASK-1795 Area A: gateway-mode relay and overrides, OT-Direct vs PIC

Date: 2026-11-11. Branch `dev` (HEAD f64aba399). Read-only audit: nothing in the repo was edited, built or flashed.

Sources opened in this session:
- PIC: `other-projects/otgw-6.6/gateway.asm` (ground truth for PIC behaviour).
- OT-Direct: `src/OTGW-firmware/OTDirect.ino`, `OTDirecttypes.h`, and the callers and the T/B/R/A pairing in `OTGW-Core.ino` (addCommandToQueue :3849-3856, pairing :5873-5885, canonical gate :1952-1965).
- Prior work: `docs/reviews/2026-04-27_otdirect-pic-parity-audit/REVIEW.md` (TASK-436..443, all now Done/archived), `docs/c4/c4-code-otdirect.md`, ADR-064, 066, 087, 118, 179, 208, 223, 224, task notes for TASK-1701, 1707, 1733 and 1750.

Legend. **Fact** means I read it in the code at the cited line. **Opinion** means a severity or likelihood judgement. "Documented (task notes)" means a task records the difference as deliberate but no ADR does.

PIC flag names used below: `NoThermostat` (onoffflags,2), `SysCtrlSetpoint` (controlsetpt1,7; *set* = no CS), `MsgWriteOp` (byte1,4 = the WRITE bit, :299).

---

## A1. Relay path: one thermostat frame, one boiler frame

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| A thermostat request is treated, then sent to the boiler. An unchanged frame is sent as received | HandleRequest/CreateRequest gateway.asm:2384-2403 | loopOTDirect normal path OTDirect.ino:2219-2231, applyOverrides :616-648 | same | Both run the per-ID treatment and forward one frame |
| The thermostat gets its answer after the boiler replied. Timeout or corrupt reply: no answer | HandleResponse :2429-2461, OpenThermMsg :1488-1499 | handleMasterResponse :1692-1695, replyToThermostat :1465-1497 | same | OTD answers only on SUCCESS or genuine DATA-INVALID |
| No own frames between thermostat frames while a thermostat is connected | ThermostatEnd/StandAloneMsg :1793-1816 (only when `NoThermostat`) | otdOwnFramesAllowed :861-863, gate in scheduleMasterRequest :1712 | same (TASK-1701/1707) | Own schedule and queue run only without a thermostat |
| Monitor mode transmits nothing of its own and changes nothing | OpenThermMsg :1490-1491 (MonitorMode skips send), SetParity :2491-2492 | :2176-2180, scheduler gate :2238 | same | OTD re-sends the frame unmodified. Electrically this differs, logically it is the same |
| Alternative slot for an unsupported Data-ID (3 Unknown-DataID strikes or UI=) | CreateRequest/SendAltRequest :2396-2414, Alternative :3390-3471 | otdIsAltSlot :1810-1814, otdAltSlotTaken :1850-1885 | different, documented (TASK-1707 notes (a)) | OTD answers the thermostat Unknown-DataID **at once**. The PIC answers after the boiler replied to the alternative. Log order: OTD T,A,R,B; PIC T,R,B,A |
| Content of an alternative slot | Alternative :3390-3471: admin/init reads, SW/SH init writes, priority msg, counter resets, the EEPROM AA= list | otdAltScheduleIndex :1826-1840 (queue first, then the most overdue schedule entry) | different, documented (TASK-1707, ADR-224) | OTD fills the slot from its own schedule, which is broader than the PIC's lists. Write entries the thermostat writes itself are excluded |
| A slot with no candidate | RestoreRequest :3462 → SendDefaultMsg: the thermostat's frame goes to the boiler unmodified | :1861 returns false → normal path forwards. **UI= IDs** are an exception: local Unknown-DataID with data 0x0000, no boiler frame (:2195-2201) | different (low) | For UI= with nothing to send, OTD sends no boiler frame and zeroes the data. The PIC forwards the frame (CreateMessage :3464-3471 keeps the data bytes) |
| Data-IDs that are never blacklisted | MandatoryID :3383-3388, called for 0 (:2712), 1 (:2798), 3/17/25 (MandatoryWord :3623, table :2540/:2554/:2562), 14 (:2959), 27 as alternative (:3054) | Only MsgID 0 is exempt (handleMasterResponse :1616-1633) | **different (bug candidate B3)** | On every thermostat request the PIC clears the Unknown marks for these IDs. OTD counts them like any other ID |
| SR= response override | WordResponse :3624-3640: the frame goes to the boiler, the PIC replaces the answer (READ_ACK + stored bytes) | SR table :2203-2217: local READ_ACK, no boiler frame | different, documented (TASK-1707 notes (b)) | Side effect on line classification: see A7 |

## A2. MsgID 0 (status) relay

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| CH enable (bit 0) forced while CS is active, from `CH=` | readstatus :2712-2740 (mask bit 0 when `SysCtrlSetpoint` clear; value `~CHModeOff`) | otdRelayedStatusFrame :837-845, otdEffectiveMasterFlags :827-830 | same (ADR-208) | Rule: CS active → bit 0 = !CHModeOff |
| DHW enable (bit 1) from `HW=0/1` / DHW push | readstatus :2716-2726 (`HotWaterEnable`, `HotWaterSwitch`, `dhwpushspoof`) | HW= :3044-3078 changes only `otMasterStatusFlags`, which feeds only buildStatusRequest :866-879 | **different (bug candidate B1)** | Nothing applies these bits to the relayed frame |
| Cooling (bit 2) while CC is active; CH2 (bit 4) while C2 is active; summer (bit 5) `SM=`; DHW blocking (bit 6) `BW=` | readstatus :2727-2736 | CE= :3128-3138, H2= :3089-3097, SM= :3100-3110, BW= :3115-3125: all write only `otMasterStatusFlags` | **different (bug candidate B1)** | Same as above |
| Only bit 0 may be rewritten | n/a | ADR-208 "Must Not rewrite any MsgID 0 bit other than bit 0" | documented by ADR-208, but the ADR's premise no longer holds | ADR-208 Must keeps "the gateway's own MsgID 0 schedule" and Must Not "suppress the gateway's own MsgID 0 while a thermostat is connected". otdOwnFramesAllowed :861 does exactly that (TASK-1701 hotfix, Robert's choice 2026-11-07; ADR-224 confirms the removal). **ADR/code conflict, ADR not superseded** |
| Reply to the thermostat: boiler low byte + thermostat high byte | statusreturn :2745-2750 + SendAltResponse/CreateMessage :2463-2472, :3464-3471 | buildOverriddenReply :1411-1416, otdStatusReplyData :848-850 | same (ADR-208) | |
| Boiler non-READ_ACK on a rewritten MsgID 0 | SendAltResponse → READ_ACK with the thermostat's data bytes | :1414 returns the boiler frame as it came | different (negligible) | A boiler that Data-Invalids MsgID 0 was not observed. Not pursued |
| On/off thermostat (short circuit = heat demand) | ShortCircuit/HeatDemand :1762-1765, MessageID0 :2722, MessageID1 :2805-2813 | none | not implemented on OT-Direct | Hardware feature of the PIC board. Out of scope |

## A3. MsgID 1 (CS=), 8 (C2=), 14 (MM=) and CH=

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| `CS=x` replaces the data of the thermostat's MsgID 1 | MessageID1 :2796-2820 | CS= :2837-2859 → enqueueWriteCommand :2312-2329 → setOverride; applyOverrides :636-647 | same | |
| A thermostat READ of 1/8/14 is turned into a WRITE | `bsf MsgWriteOp` :2804, :2927, :2965 | applyOverrides :634 returns on non-WRITE | different (negligible) | Thermostats always WRITE these IDs |
| Reply for an overridden write: WRITE_ACK with the thermostat's own data, whatever the boiler said | SendAltResponse :2463-2472 + CreateMessage :3464-3471 | buildOverriddenReply :1407-1425 | same | |
| A boiler Data-Invalid on MsgID 1 clears the user CS | ctrlsetptreturn :2822-2825 | no equivalent (handleMasterResponse :1677-1679 only logs) | **different (bug candidate B5)** | |
| CS expiry: unrefreshed CS cleared after the minute timer, **except CS < 8 °C** | CommandExpiry :1619-1629 (`andlw b'11111000'`) | :2255-2265 expires any value, not while SAT owns MsgID 1 | different. The SAT exemption is documented by ADR-179; the < 8 °C rule is not mentioned | Low. A CS=1..7 hold expires on OTD |
| `CH=0/1` sets CHModeOff, active only with CS | SetCentralHeat :4372-4378 | :3082-3087 | same | |
| `MM=0..100`; non-numeric clears | SetMaxModLevel :4752-4765 (OutOfRange above 100; a '-' is non-numeric → clear) | :2974-2986 (no range check; '-' accepted; floatToF88 clamps to 127) | different (low) | `MM=150` writes 127 % on OTD where the PIC answers OR. `MM=-5` writes a negative value where the PIC clears |
| MM applied only to the thermostat's own MsgID 14 | MessageID14 :2957-2969 | applyOverrides | same | Neither side injects MsgID 14 when the thermostat does not send it (OTD only in an alternative slot) |

## A4. MsgID 56 (SW=) / 57 (SH=)

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| A thermostat WRITE 56/57 gets the user value | MessageID56 :3114-3127, MessageID57 :3150-3163 | applyOverrides | same | |
| **A thermostat READ 56/57 is turned once into WRITE + user value** (`INITSW`/`INITSH`, set by SW=/SH= when the boiler allows writes) | :3116-3121 `btfss byte1,4; btfsc initflags,INITSW` → SetWaterSet `setbyte1 T_WRITE`; SetHotWaterTemp :4150-4151 | READ passes (applyOverrides :634) | **different (bug candidate B2)** | The PIC also sends INITSW/INITSH in an alternative slot (AdminMessages :3419-3424) |
| Boiler WRITE_ACK / Unknown / Data-Invalid → thermostat answer | hotwaterreturn :3128-3140, maxchsetptret :3164-3176 | buildOverriddenReply :1419-1423 | same | |
| Data-Invalid zaps SW/SH (EEPROM cleared) | InvalidHotWater :3141-3148, InvalidHeating :3177-3182 | none | **different (B5)** | |
| Without SW/SH set, a boiler Unknown on 56/57 still becomes an ACK (bounds value) | hotwaterreturn path runs on every response :3134-3140 | relays the boiler's Unknown | different (low) | Thermostat-visible only |
| SW/SH range check against the MsgID 48/49 bounds; values persist in EEPROM | SetHotWaterTemp :4133-4158, SetHeatingTemp :4160-4185 | :2944-2972: no range check; resetTransientState :2501-2577 clears them, nothing persists | different (low) | |

## A5. MsgID 16, 9, 100: TT=, TC=, BS=, OH=

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| TT/TC are answer state only, nothing written to the boiler | SetSetPoint :4065-4097, MessageID9 :2933-2955, MessageID100 :3272-3304 | setRemoteOverride :2398-2430, otdRemoteOverrideReply :1434-1458 | same (ADR-223) | |
| Release rule after MsgID 9 was read; 0.25 °C tolerance | MessageID16 :2976-3037 (0.125 °C) | onThermostatMsgID16 :2470-2499 | documented difference (ADR-223 sub-point 3) | |
| MsgID 100 flags: TT b'11', TC b'01'; OH= copies them to the high byte | :4061, :4055; MessageID100 :3285-3289; SetOverrideHigh :4927-4933 | :1476 (TT 0x0002, TC 0x0001); otOverrideHB set at :3628 but never read for the reply | different, **already tracked in TASK-1750** (To Do, needs-maintainer-ok) | Not re-filed |
| Answer of 0 after a clear | OverrideClr: answers 0 until the thermostat's next MsgID 16 (roomsetptclear :3013-3018) | one MsgID 9 read gets 0 (otRemoteOverrideClearPending, :1446-1448). Also set by a TT=0 when nothing was active (:2442) | different (low) | |
| **The thermostat's MsgID 16 is always acknowledged** ("make sure we always return an acknowledgement") | MessageID16 response → missingdata :2976-2979, :2688-2693 | Without BS=: the boiler's answer is relayed (:1471-1478). After 3 Unknown strikes MsgID 16 becomes an alternative slot (otdIsAltSlot :1810-1814, only 9/100 excluded) | **different (bug candidate B4)** | |
| BS= fake room setpoint replaces the thermostat's MsgID 16 | roomsetptchange :3019-3024 | BS= :2903-2915 → setOverride(16) | same | |
| Celcia / iSense special cases | :4098-4111, :3029, :3291 | none | not implemented on OT-Direct | Thermostat-model hacks (FT=). Not assessed further |

## A6. MsgID 27 (OT=)

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| **`OT=` value: the thermostat's READ 27 becomes WRITE 27 to the boiler, and the thermostat's answer is READ_ACK with the OT= value** | MessageID27/UserOutsideTemp :3050-3067 | OT= :2987-3000 only enqueues a WRITE 27 (alternative slot only with a thermostat). MsgID 27 is not in otOverrides :435-443. Nothing touches the thermostat's read or its answer | **different (bug candidate B2)** | |

## A7. Line classification 'T'/'B'/'R'/'A'

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| T = received thermostat frame, B = received boiler frame | PrintRcvdMsg :1918-1932 | bridgeSentRequest :1504-1507, handleMasterResponse :1592 | same | |
| R = modified/own request, A = modified/own answer | SetParity :2491-2496 | bridgeSentRequest (T then R), replyToThermostat :1490-1496 ('A' when reply != boiler) | same | Parser pairing OTGW-Core.ino:5873-5885 sees the same T→R / B→A pairs |
| Gateway's own frame without a thermostat logged as R | StandAloneMsg → CreateRequest → SetParity :2401-2402 | origin GATEWAY → 'R' :1506 | same | |
| SR= answer | T, B, A (A pairs with B → override A, not canonical) | T, A only (:2210-2211). OTGW-Core :5873 finds no B to pair, so the A is a *proxy A* and passes the canonical gate :1958 | different (low), consequence of the documented SR difference | The SR= value lands in canonical as if the boiler sent it. On the PIC canonical holds the boiler's real B |
| Alternative slot | T,R,B,A; R/T and A/B differ in ID, so nothing pairs | T,A,R,B; nothing pairs | same classification, different order (documented, TASK-1707 (a)) | |

## A8. Thermostat stops talking

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| Detection | Electrical: line open/short, CheckThermostat :1728-1772 (fast) | Timeout `iSetbackTimeout` (default 30 s, OTDirecttypes.h:111), checkThermostatTimeout :2654-2692 | different (hardware). Bus-silence risk documented in TASK-1707 notes | Up to 30 s after the last frame the boiler gets no frame at all (own frames still blocked) |
| On disconnect: forget the TT/TC override | Disconnect :1772-1787 (`clrf override, setpoint1/2`) | otRemoteOverride is not cleared on disconnect | **different (bug candidate B6, low)** | |
| On disconnect: run the own schedule | StandAloneMsg :1796-1816 | own frames allowed again (:861) | same | |
| Fail-safe / setback | `FS=` is the WDT-reset fail-safe (:1067-1070, SetFailSafety :5252); `SB=` is the GPIO AWAY *room* setpoint via SetSetPoint (:5390-5398) | Thermostat timeout + `FS` → setOverride(1, fSetbackTemp=16 °C) as **TSet (flow)** :2669-2678 | OT-Direct extension, not PIC behaviour; no ADR found (ADR-064/066/068 checked) | Observation for Robert, not counted as a bug: the "setback temperature" is a room value used as a flow setpoint |
| Thermostat reconnects while in master mode | n/a (PIC has no master mode) | stays master, answers from cache :2166-2173 | documented (ADR-066) | |

---

## Bug candidates (survived the skeptic pass)

Severities are **opinion**. Each scenario is constructed from the cited code, not observed on a bench or in a capture.

### B1 (medium). HW=, SM=, BW=, CE=, H2= do nothing in gateway mode with a thermostat, yet are acked
- **Fact.** These commands change only `otMasterStatusFlags` (:3044-3138), and only buildStatusRequest :866 reads it.
- **Fact.** buildStatusRequest has two call sites: the boot probe :995 and the schedule entry `R_ENTRY(0)` at :1761. While `state.otd.bThermostatConnected` holds in gateway mode, otdOwnFramesAllowed :861 blocks the schedule, and the alternative slots skip MsgID 0 (:1831).
- **Fact.** The relayed frame gets only bit 0 (:837-845).
- **PIC.** readstatus :2712-2740 rewrites bits 1, 2, 4, 5 and 6 of the thermostat's own MsgID 0.
- **Scenario.** An HA automation sends `HW=0` (the MQTT `hotwater` topic) to block DHW. OT-Direct answers `HW: 0`. The boiler keeps seeing the thermostat's DHW enable = 1. `HW=P` also sets a bit that never goes out, and queues MsgID 99, which leaves only in an alternative slot.
- **ADR conflict.** ADR-208 forbids rewriting bits other than bit 0, and it also requires the gateway's own MsgID 0 that TASK-1701 removed. The ADR's decision rested on "the gateway's own frame carries the other bits", and that is no longer true. This needs Robert: either supersede ADR-208 (relayed MsgID 0 gets the PIC readstatus mask), or document that these commands are no-ops in gateway mode and refuse them with NG.

### B2 (medium). OT=, SW=, SH= depend on alternative slots; the PIC delivers them through the thermostat's own frame
- **Fact.** The PIC turns the thermostat's READ 27 into WRITE 27 = OT value and answers the thermostat READ_ACK = OT value (:3057-3067). It turns a READ 56/57 into a WRITE once after SW=/SH= (:3116-3121, :3152-3157).
- **Fact.** applyOverrides :634 never touches a READ, and MsgID 27 has no override entry (:435-443).
- **Fact.** With a thermostat, the queue and the schedule write entries go out only in alternative slots (:1712, :1855-1868).
- **Scenario 1.** A thermostat whose every Data-ID the boiler supports offers no slots. `OT=8.5` and `SW=55` are acked but never reach the boiler.
- **Scenario 2.** A weather-compensating thermostat that reads MsgID 27 never sees the HA outside temperature on OT-Direct, whatever the slots.
- **Slot rate.** tjfs's stream had about one slot per 30 s (TASK-1707 notes). Delivery there is slow, not absent, which is why this is medium and not high.
- **Not refuted by ADR-224.** ADR-224 only reads MsgID 28 in supported slots; it does not cover writes or MsgID 27.

### B3 (medium: high impact, low likelihood). PIC "mandatory" IDs (1, 3, 14, 17, 25) can be blacklisted permanently on OT-Direct
- **Fact.** The unknown counter in handleMasterResponse :1616-1633 counts every Unknown-DataID reply except MsgID 0, relayed thermostat frames included.
- **Fact.** At 3 strikes otdIsAltSlot :1813 makes the ID a slot.
- **Fact.** otdAltScheduleIndex :1826-1840 picks the most overdue entry, not only an entry past its interval, so a candidate almost always exists and the thermostat's frame stops reaching the boiler.
- **Fact.** clearUnknownCount runs only on a non-Unknown boiler reply (:1634-1637), which can no longer arrive. Only GW=R, KI= or a mode change clears the count.
- **PIC.** MandatoryID :3383-3388 clears the mark on every thermostat request for 0/1/3/14/17/25.
- **Scenario.** A boiler answers Unknown-DataID to MsgID 1 three times in a row during a reset or lockout. From then on the thermostat's TSet never reaches the boiler and the thermostat gets Unknown-DataID. Only a reboot, GW=R or a mode change recovers.
- **Unverified.** Whether real boilers do this transiently for MsgID 1, 17 or 25: no capture checked.

### B4 (low-medium). MsgID 16 is not always acknowledged
- **Fact.** The PIC always answers the thermostat's MsgID 16 with WRITE_ACK (:2976-2979 → missingdata :2688-2693), also after the ID is blacklisted, because SendAltResponse :2470-2471 runs TreatMessage on the fake answer.
- **Fact.** OT-Direct relays the boiler's Unknown-DataID, and after 3 strikes answers Unknown-DataID from the slot.
- **Scenario.** A boiler without TrSet support (no capture checked for how common that is). A thermostat that stops sending MsgID 16 after Unknown-DataID (the PIC comment names this "überclever thermostat" case) then starves the ADR-223 TT/TC release rule, so an override is never released by the thermostat.

### B5 (low). Boiler Data-Invalid does not clear CS=, SW= or SH=
- **Fact.** PIC ctrlsetptreturn :2822-2825, InvalidHotWater :3141-3148 and InvalidHeating :3177-3182 drop the user value. OT-Direct keeps substituting it (:1677-1679 only logs).
- **Scenario.** `SW=75` above the boiler's limit. The PIC drops it after one Data-Invalid. OT-Direct keeps rewriting every thermostat WRITE 56 to 75, and the boiler keeps refusing it, so the thermostat's own valid setpoint never gets through.

### B6 (low). The TT/TC override survives a thermostat disconnect
- **Fact.** PIC Disconnect :1782-1784 clears `override` and `setpoint1/2`. OT-Direct checkThermostatTimeout :2654-2692 leaves otRemoteOverride as it was.
- **Scenario.** The thermostat is swapped or re-powered. The new thermostat gets the old remote override on its first MsgID 9 read.

### Low items, listed without a separate task proposal
- **MM= range.** `MM=150` and `MM=-5` are accepted (A3).
- **CS < 8 °C.** Expires on OT-Direct but not on the PIC (A3).
- **SR= in canonical.** The SR= value reaches canonical as a proxy A (A7).
- **UI= with no candidate.** No boiler frame in that slot, and the data bytes are zeroed (A1).
- **SW/SH.** Not range-checked and not persisted (A4).

---

## Refuted (with reason)

- **"Queued commands never drain in gateway mode and fill the ring until writes are refused."** Refuted. otCmdEnqueue :389-401 coalesces per MsgID, so depth is bounded by the number of distinct MsgIDs in use (about 8 with SAT plus OT/SW/SH/SC), below the 12 slots. Remaining effect, very low: a frame queued before `CS=0` can still go out later in a slot, once.
- **"TC= writes MsgID 1."** Refuted: TASK-443 is Done, and TC= now goes to setRemoteOverride :2861-2879 (ADR-223).
- **"The rewritten MsgID 0 reply loses the boiler's slave status."** Refuted: buildOverriddenReply :1411-1416 has the ADR-208 case.
- **"An alternative-slot A frame pollutes canonical."** Refuted as a difference: the PIC slot (T,R,B,A with differing IDs) also yields an unpaired proxy A, and an Unknown-DataID A carries no valid value (is_value_valid_for_master_topic :1959-1964).
- **"Monitor mode transmits on OT-Direct."** Refuted as a behavioural difference. OT-Direct must re-drive the frame because it sits in the line. It injects nothing (:2176-2180, :2238).
- **"The TT flags value 0x0002 is a new finding."** Not new: already TASK-1750. Only cited.
- **"CS heartbeat expiry is missing."** Refuted: TASK-442 is implemented (:2255-2265). The SAT exemption is ADR-179.

## Not verified

- No bench or field capture: every scenario above is derived from reading code.
- I did not check how often boilers answer Unknown-DataID to MsgID 1, 14, 16, 17 or 25 (B3, B4 likelihood).
- I did not check whether the boiler acts on `dhwupdate`/`maxchupdate`, the PIC's MsgID 6 write permissions that gate INITSW/INITSH. That could make B2 smaller on boilers without remote-parameter write support.
- The OTD-only thermostat-timeout setback is reported as an observation; no ADR was found for it in ADR-064/066/068. A full grep of all ADRs for "setback" was not done.
