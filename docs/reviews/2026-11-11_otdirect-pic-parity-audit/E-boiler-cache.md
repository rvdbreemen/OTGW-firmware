# TASK-1795 Area E: boiler value cache and answer generation, OT-Direct vs PIC

Date: 2026-11-11. Branch `dev` (HEAD f64aba399). Read-only: nothing in the repo was edited, built or flashed.

Opened in this session: `other-projects/otgw-6.6/gateway.asm`, `src/OTGW-firmware/OTDirect.ino`, `src/libraries/OpenTherm/src/OpenTherm.cpp` (:425-455, :518-527), `SATcontrol.ino:2760-2773`, `OTGW-Core.ino:2850-2862`, ADR-064/066/224, `docs/BREAKING_CHANGES.md:25`, reports A, B, CD in this folder, REVIEW.md of 2026-04-27 (command facade only, nothing on the cache). "Fact" = read at the cited line. "Opinion" = severity or likelihood. Nothing was run on a bench: every scenario is constructed from code.

Existing tasks not re-reported: TASK-1797..1806, 1750, 1751, 1707, 1733 (cited where touched). Archived and already fixed: TASK-1177 (OEM ids aliasing onto the cache), TASK-1184 (Unknown-DataID cached as a value).

## Short answer to the brief

- **PIC has no boiler value cache that it answers from.** `StoreValue` (gateway.asm:3574-3607) fills `valuestorage` for the PS=1 summary only (34 fields, `SUMMARYFIELDS` :3573), from boiler READ-ACKs (:2436, `btfss byte1,4`) and thermostat/own writes (:1808, :2385); it is never read back to answer a thermostat. Everything the PIC answers itself is either a modified relay (OverrideUsed) or a fake answer built from the thermostat's own request (`CreateMessage` :3464-3471) or from user settings (SR=, TT=, OT= ...). The `unknownmap` (:545) is a 2-bit strike counter per Data-ID 0-127, not a value store.
- **OT-Direct keeps `otBoilerCache[128]` + `otBoilerCacheValid[128]` (OTDirect.ino:296-297).** It is answered from in exactly one place today: master mode with the slave interface (`handleMasterModeSlaveFrame` :2706-2744). Gateway mode never answers from it. **ADR-224 (Accepted) adds a second place (cache-answered slot for MsgID 28), but it is not implemented in the code**: no `ADR-224` or `1759` string in `src/`, TASK-1759 is To Do, and the cache has no timestamp.
- No age check exists anywhere for the cache. No invalidation exists except an Unknown-DataID reply for that id.

## E1. What is cached, from which frames

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| Frames that fill the store | boiler READ-ACK only: HandleResponse `btfss byte1,4` :2436; thermostat WRITE :2385; own write :1808. Skips manufacturer ids (`rlf byte2,W; skpc` :3575-3576) and Data-Invalid/Unknown (`btfsc byte1,5` :3577) | `otBoilerCacheStore` :1518-1527, called from the SUCCESS branch of handleMasterResponse :1601 and the loopback path :1541 | different (low) | OTD stores **every** SUCCESS reply: READ_ACK, **WRITE_ACK echo** and UNKNOWN (which clears). The PIC stores boiler READ-ACKs only. Effect: `cache[56]` after `SW=` holds the echoed write, not a boiler read. Harmless today (only master-mode reads and `otIsVentSlave`/`otDirectBoilerPresent` use it) |
| Id range | 0-127 for the unknown map (`UnknownMask2` :3491-3499 returns 0 for ids with bit 7); store skips ids >=128 | ids >=128 not stored (:1520), TASK-1177 | same | |
| Boiler Unknown-DataID | store skipped (:3577); counter +1 (:2437-2447) | slot cleared `valid=false` (:1521-1524, TASK-1184) | same (OTD slightly stricter, correct) | UNKNOWN is SUCCESS in the library (OpenTherm.cpp:526), so the clear is reachable. Verified, see Refuted R1 |
| Boiler Data-Invalid | not stored; counter reset (`MsgSupported` :2455-2458) | not stored (:1683-1686, status is INVALID so the SUCCESS branch is skipped); old value stays | same for the cache. **Strike counter differs: see E6** | |
| Thermostat-forwarded replies | stored by HandleResponse for every reply | stored in the same branch for thermostat and gateway origin | same | Cache holds the raw boiler frame, before RM= modifiers and before override replies (:1465-1488) |
| Boot probe / handshake frames (MsgID 0, 2, 3?, 124) | n/a | `initOTDirect` :995-1052 uses `otMaster.sendRequest` and bridges the frames only, no `otBoilerCacheStore` | not cached | cache[0] is first filled by the scheduler (~1 s) |
| Loopback | n/a | synthetic replies stored (:1541) | OTD only | See E3: never cleared |

## E2. When does the gateway answer from the cache?

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| Gateway mode: thermostat READ of a supported id | relay, the thermostat gets the boiler's own reply | relay, `replyToThermostat` :1465-1497 | same | No cache involved on either side |
| Gateway mode: cache-answered slot for MsgID 28 | none (ADR-224 names it a deliberate deviation; `AA=28` is the PIC route, TASK-1767) | ADR-224 Decision; **no code** | **not implemented** (ADR Accepted, TASK-1759 To Do) | Pre-conditions in the ADR (cache age younger than 30 s, one candidate id, 8 B stamp) do not exist yet: `otBoilerCache` has no timestamp. See notes for TASK-1759 below |
| Gateway mode: answer from tables | UI= -> Unknown via alt slot; SR= -> modified answer | UI :2195-2201, SR :2203-2217, alt slot `otdAltSlotTaken` :1850-1885 | see area A (A1) | Not repeated here, except E5 |
| Master mode, thermostat READ | no master mode on the PIC | `handleMasterModeSlaveFrame` :2706-2721: valid -> READ_ACK from cache, else Unknown-DataID | documented (ADR-066 "virtual boiler"), details below | |
| Master mode, thermostat WRITE | n/a | :2722-2743: always WRITE_ACK echo; only ids 1 (>0, not SAT), 16, 24 reach `updateWriteCache` | documented-ish (ADR-066), see E-unclear U1 | |
| Master mode, other types | n/a | :2744-2748 Unknown-DataID with data 0 | same as spec | |

## E3. Freshness, empty cache, invalidation (master-mode answers and presence flags)

| Behaviour | PIC | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| Age check before serving | n/a (no served cache) | none in `handleMasterModeSlaveFrame` :2715. Poll intervals 0.8 s / 10 s / 60 s (:56-62, :157-281) | not implemented | ADR-066:90 assumes "< 2 seconds old given the ~1s polling cycle". That is true for ids 0 only; temperature ids are up to 10 s, config ids up to 60 s old, and unbounded when the boiler is gone (E-cand 1) |
| Empty slot answer | n/a | Unknown-DataID, data 0 (:2718) | **different from ADR-066:94**, bug candidate E-cand 2 | ADR-066 says "respond with DataInvalid flag until cache is populated" |
| Boiler offline | PIC: no boiler reply -> no answer to thermostat (OpenThermMsg timeout, area A "Timeout: no answer") | cache keeps serving; `bOnline=false` only on a MsgID 0 failure (:1686-1692), cache untouched | **bug candidate E-cand 1** | `OTGW-Core.ino:2857-2858` already notes "keeps serving the last flow after the boiler has stopped answering" (for ADR-176 only) |
| Mode switch / GW=R | PIC reset clears all RAM incl. `valuestorage` | `resetTransientState` :2498-2577 clears unknown counters, disabled flags, write caches (`valueSet=false` :2548), response overrides; **not** `otBoilerCache*` | **bug candidate E-cand 3** | grep: only `otBoilerCacheStore` writes the arrays |
| Reboot | RAM cleared | statics zero | same | |
| Presence derived from cache | n/a | `otDirectBoilerPresent()` :324-327 = `valid[3]` outside loopback; used by `satBoilerHardwarePresent()` SATcontrol.ino:2769-2771 | see E-cand 3 | |
| Answer type for cached value | n/a | `buildOTResponse(4,...)` READ_ACK :2716 | correct | A cached value that was a WRITE_ACK echo is also returned as READ_ACK: acceptable |
| ID 0 reply high byte | gateway: thermostat's own HB + boiler LB (`statusreturn` :2745-2750, ADR-208) | master mode: cached HB = **OTD's own** master flags from its last status request | different (low), by design | In master mode OTD, not the thermostat, owns the master flags. Thermostats rarely check the HB echo. Not a candidate |

## E4. Write cache (`updateWriteCache`) vs the PIC

| Behaviour | PIC file:line | OTD file:line | Verdict | Evidence |
|---|---|---|---|---|
| What it is | no periodic write cache. A user value is applied inside the thermostat's own frame (`OverrideUsed`), or sent in alt slots / stand-alone (:1793-1816, `AdminMessages` :3412-3438) | `updateWriteCache` :650-658 sets `cachedValue/valueSet` on the `W_ENTRY` with that id (1,7,8,14,16,24,27,56,57 :160-168); scheduler re-sends every 15 s (:61, :1756-1770) | different by design (standalone master, TASK-1707/1701) | |
| Id without a `W_ENTRY` | n/a | silent no-op (e.g. 20 SC=, 71 VS=, 100 OVR-flags go out once via the queue) | documented in code comments; no finding | |
| Cleared by | CS expiry :1619-1629, Disconnect :1782-1784 | `clearWriteOverride` :2374-2382 (CS/C2/SC heartbeat :2264-2282, TT=0), `resetTransientState` :2548 | partly same | See E-cand 4 for what is **not** cleared |
| Thermostat writes in master mode | n/a | ids 1/16/24 -> `setOverride` + `updateWriteCache` :2731-2742 | OTD only | No expiry for these (E-cand 4) |
| Setback writes TSet | `FS`/`SB` semantics differ (area A8) | `updateWriteCache(1, f88)` :2675 | see area A8 | not repeated |
| Gateway mode: thermostat writes id 24/16 | forwarded to boiler by the thermostat's own frame | write entry marked "thermostat's" (`otdThermostatWritesEntry` :1819-1823, :1900-1903) so no slot write | same intent (TASK-1701 lesson) | |

## E5. Generated answers: type and data correctness

| Answer | PIC | OTD file:line | Verdict |
|---|---|---|---|
| UI= / 3-strike alt slot: Unknown-DataID with thermostat's data bytes | CreateMessage :3464-3471 (type 0x70, data restored) | alt slot :1873-1874 keeps data; UI branch :2195-2197 and master mode :2718/:2747 zero the data | different (negligible); UI zeroing already in A1 |
| SR= answer type | `messageack` :2668-2677: original request type | 0xC0 -> **READ_ACK for a READ, WRITE_ACK for a WRITE** | SR branch :2205-2208 always `4UL<<28` (READ_ACK), also for a WRITE_DATA frame | **bug candidate E-cand 5 (low)** |
| Overridden write: WRITE_ACK with thermostat data | :2463-2472 | `buildOverriddenReply` :1407-1425 | same (area A) |
| Remote override MsgID 9/100: READ_ACK | :2933-2955, :3272-3304 | `otdRemoteOverrideReply` :1434-1458 | same (ADR-223, TASK-1750 for flags) |
| Boiler Data-Invalid relayed to thermostat | relayed (A frame only when modified) | `boilerDataInvalid` accepted and relayed :1676-1678, :1695 | same |
| Master-mode Unknown for unknown data type (INVALID_DATA req, reserved) | n/a | Unknown-DataID :2744-2748 | fine, spec-conforming |

## E6. Unknown strikes ("unknownmap")

| Behaviour | PIC | OTD | Verdict |
|---|---|---|---|
| 3 strikes, 2-bit counter | `UnknownMask`, :2437-2449; counter +1 only on type 7, reset on **every other** reply incl. Data-Invalid (`MsgSupported` :2455-2458, `btfsc byte1,5; btfss byte1,4`) | `incUnknownCount` :1612-1624; reset :1634-1637 only in the SUCCESS branch | **different (negligible)**: a Data-Invalid reply neither counts nor resets in OTD (:1683-1686). Sequence Unknown, Unknown, Data-Invalid, Unknown blacklists in OTD, not on the PIC. The "three consecutive" comment (:1788) is not strictly true |
| Persistence | UI=/KI= stored in EEPROM (:4858-4881, `StoreEpromData`) | counters in RAM, reset by `resetTransientState` | different (area B/A) |
| Mandatory ids exempt | MandatoryID :3383-3388 | only id 0 | **already reported as B3 in area A**, not repeated |

---

## Bug candidates (survived skeptic pass)

Severities are opinion. All are code-derived, none observed.

### E-cand 1 (medium-low). Master-mode "virtual boiler" serves frozen values after the boiler stopped answering
- **Fact.** `handleMasterModeSlaveFrame` answers READ_ACK from `otBoilerCacheValid[id]` with no age or online test (OTDirect.ino:2715-2716). `bOnline` goes false only after a failed MsgID 0 (:1686-1692); nothing invalidates the cache then. `bEnableSlave` defaults to true (OTDirecttypes.h:114).
- **Fact.** The PIC answers nothing when the boiler does not reply (area A: OTD gateway mode also does not). `OTGW-Core.ino:2857-2858` already acknowledges the symptom ("keeps serving the last flow after the boiler has stopped answering"), but only as a guard for the DHW water total; no task or ADR covers the thermostat side (grep of `backlog/` for `otBoilerCache` finds archived 1177/1184 and TASK-1759 only).
- **Scenario.** Master mode, thermostat on the slave port, boiler loses power or its OT cable is pulled. The thermostat keeps reading ID 0 and gets READ_ACK with the last slave status (e.g. flame on, CH active, fault bit clear). It never sees the timeout a real boiler loss would give. A boiler that died with the fault bit set keeps showing it after recovery until the next poll, which is harmless.
- **Skeptic.** Tried to refute with "ADR-066 accepts staleness". ADR-066:90 bounds staleness to < 2 s *assuming the poll runs*; it does not address a dead boiler. Tried "master mode with a thermostat is rare": true, but `bEnableSlave=true` is the default and the thermostat-appears-later path (ADR-066 auto-detect) is the designed use. Survives.
- **Fix direction (not proposed as a patch).** Do not answer (let the thermostat time out) while `!state.otBus.bOnline`, or stamp the cache and refuse entries older than a bound.

### E-cand 2 (low-medium). An empty cache slot is answered Unknown-DataID, including for MsgID 0 and mandatory ids; ADR-066 says Data-Invalid
- **Fact.** :2717-2718 answers type 7 for any id without a valid slot. ADR-066:94 states the mitigation "respond with DataInvalid flag until cache is populated".
- **Fact.** First-poll timing: `scheduleMasterRequest` sends an entry when `now - lastSentMs >= interval` (:1758) with `lastSentMs = 0` at boot. At boot the 10 s tier is first polled at uptime >= 10 s and the 60 s tier (ids 3, 5, 6, 9, 15, 20-22, 33, 34, 37, 39, 48-63 ...) at uptime >= 60 s. After `resetTransientState` (mode switch, GW=R) `lastSentMs` is zeroed (:2524) and everything is due at once, so the window exists only for a master-mode **boot**.
- **Scenario.** A unit saved in master mode with a thermostat on the slave port is rebooted. The thermostat asks MsgID 3 (slave configuration) and MsgID 5, 48, 49 in its start-up and gets Unknown-DataID for up to 60 s, usually three or more times. A thermostat that stops asking an id after repeated Unknown answers (the PIC source names exactly this "overclever thermostat" case, see area A, B4) never learns the slave configuration or DHW bounds.
- **Skeptic.** Tried "the window is a few seconds": refuted by the 60 s tier arithmetic above. Tried "the thermostat would just retry": plausible for many, not for the class of thermostat the PIC comment describes. Also tried "Unknown vs Data-Invalid makes no difference": it does, because Unknown means "id not supported" (spec v4.2 4.3) and thermostats cache that, while Data-Invalid means "supported, no value now". Survives, low-medium because the configuration (master + thermostat + reboot) is narrow and no thermostat model was observed to misbehave.
- **ID 0 specifically** is polled at 0.8 s so its gap is ~1 s; low impact.

### E-cand 3 (low-medium). The cache survives loopback to real, GW=R and a mode change, so `otDirectBoilerPresent()` and `otIsVentSlave()` can be true with no boiler
- **Fact.** `resetTransientState` (:2498-2577) never clears `otBoilerCache*`; `applyOTDirectMode` (:2582-2640) only calls it. Loopback fills `valid[3]` with the synthetic 0x000B (:1246-1247, :1541).
- **Fact.** `otDirectBoilerPresent()` is `!loopback && valid[3]` (:324-327). `satBoilerHardwarePresent()` (SATcontrol.ino:2769-2771) returns true on it, which gates SAT simulation (REST 409 guard, MQTT enable-reject, edge auto-disable per the SATcontrol comment :2745-2758). It also never goes false when a real boiler later disappears.
- **Scenario A (bench rig).** A bench unit runs loopback with simulation, the user switches to gateway or master mode with nothing on the boiler terminals. `valid[3]` stays true, `satBoilerHardwarePresent()` says a boiler is present and the simulation can no longer be enabled until a reboot. TASK-1138's comment treats the loopback/presence asymmetry as a decision, but it assumed the flag distinguishes a real answer; it cannot after leaving loopback.
- **Scenario B (field).** The boiler is unplugged for service; presence stays true, so nothing re-arms the "no boiler" path in SAT.
- **PIC comparison.** A PIC reset clears its store, and the PIC presence signal is a live timer (`BoilerAlive`, set in HandleResponse :2432, cleared by timeout).
- **Skeptic.** Tried "who switches out of loopback?" (a developer feature; real, TASK-795 F7 bench flows do). Tried "simulation 409 is cosmetic": it blocks a test path, not safety. Hence low-medium and mostly a bench-integrity issue. Survives.

### E-cand 4 (medium-low). Master mode: a thermostat's TSet/TrSet/Tr become sticky; no expiry, and a later TSet 0 is ignored
- **Fact.** :2731-2742: a thermostat WRITE of id 1 > 0 (SAT off) calls `setOverride(1, v)` and `updateWriteCache(1, v)`; id 16 and 24 call `updateWriteCache`. A write of 0 on id 1 is skipped (documented, BREAKING_CHANGES.md:25 "a later write of 0 is ignored").
- **Fact.** The CS heartbeat that would expire the override runs only if `otCSLastCommandMs != 0` (:2264), and `otCSLastCommandMs` is set only by the `CS=` command (:2852). The thermostat path never sets it. `otdForgetThermostatWrites` (:1899-1903, called :2664) clears only the alt-slot flags. `checkThermostatTimeout` engages setback in gateway mode only (:2672).
- **Fact.** BREAKING_CHANGES.md:25 documents "a later write of 0 is ignored" and the 60 s expiry for `CS=`; it does not say the thermostat-set override never expires, and no ADR covers it.
- **Scenario 1.** Master mode, thermostat writes TSet = 55 at 07:00 and TSet = 0 at 08:00 (room warm). OT-Direct keeps writing 55 every 15 s and keeps CH enable on (buildStatusRequest enables CH while a CS override exists). The thermostat's stop demand is acknowledged (WRITE_ACK) but has no effect. The room overheats until a reboot, `CS=0` or a mode change.
- **Scenario 2.** The thermostat battery dies. The boiler keeps heating at TSet 55 with the stale TrSet/Tr (ids 16/24 are also re-sent every 15 s forever, a stale room temperature to boilers that modulate on it). The PIC clears override and setpoints on thermostat disconnect (gateway.asm:1782-1784, area A B6).
- **Skeptic.** Tried "documented, so by design": only half. The "0 ignored" part is documented in BREAKING_CHANGES.md (not an ADR) and is a deliberate trade-off against flip-flops with a second controller; the no-expiry and the stale 16/24 are not mentioned anywhere found. Tried "master mode with thermostat is rare": it is the hybrid mode of ADR-066, and the failing case needs only an ordinary heating cycle, not a fault. Needs Robert's decision (this changes documented behaviour), so it is a candidate for a `needs-maintainer-ok` task, not a silent fix.

### E-cand 5 (low). SR= answers a WRITE_DATA with READ_ACK
- **Fact.** The SR branch (:2203-2217) builds `(4UL << 28)` for every thermostat frame whose id matches, regardless of the frame's type. The PIC's `messageack` (gateway.asm:2668-2677) returns the request's own type with the response bit (READ -> READ_ACK, WRITE -> WRITE_ACK) and keeps the id.
- **Scenario.** `SR=` for an id the thermostat writes (e.g. 16). The thermostat WRITE gets READ_ACK, a spec-invalid answer to a write (spec v4.2 4.3.1: WRITE-DATA must be answered WRITE-ACK).
- **Skeptic.** Tried "nobody sets SR= on a write id": SR= is meant for ids the thermostat reads (date/time 20-22); also OTD does not forward the frame at all (area A "SR documented", TASK-1707 (b)). The type fault only matters for write ids. Low; survives because the fix is one line (`type = request type | 4`).

### E-cand 6 (negligible). A boiler Data-Invalid neither counts nor resets the Unknown-DataID strike counter
- See table E6. Alternating Unknown/Data-Invalid sequences are not expected from a real boiler; included so the "consecutive" claim in the code comment (:1788) is correct or corrected. No task proposed.

## Unclear / design questions (not counted as bugs)

- **U1. Master-mode WRITE_ACK without a write.** The thermostat's WRITE of ids other than 1/16/24 (56, 57, 14, 8, 7, 9, 100, 20-22 ...) is acknowledged and dropped (:2722-2743). The thermostat believes its DHW setpoint or max-CH value is set. Plausibly intended (standalone master owns the boiler; ADR-066 "hybrid"), but ADR-066 does not say so. Ask Robert whether the thermostat's DHW setpoint should reach the boiler in master mode.

## Notes for TASK-1759 / ADR-224 implementation (forward-looking, not bugs today)

1. The cache has no age. The ADR's single 8 B stamp is new state; it must be stamped in `otBoilerCacheStore` for the READ_ACK of the **candidate** id only, and must **not** be stamped by a WRITE_ACK echo (E1 row 1) or by a loopback reply.
2. The cache-answered READ_ACK must go through `applyResponseModifiers` (RM=), as `replyToThermostat` does for relayed answers (:1475), otherwise an RM= entry on the candidate id is skipped in that slot. ADR-224 excludes ids 9/100 and SR=/UI= ids but does not mention RM=.
3. The cached slot is cleared by Unknown-DataID but **not** by Data-Invalid (E1 row 4): a candidate id whose last reply was Data-Invalid keeps an old valid value and an old stamp. The ADR's "younger than the interval" test covers it, as long as the stamp is not refreshed by Data-Invalid.

## Refuted (with reason)

- **R1. "Unknown-DataID is cached as a value (outside temperature 0.0 C)."** Refuted. TASK-1184 is archived; `otBoilerCacheStore` clears the slot (:1521-1524) and the clear is reachable because `isValidResponse` classes UNKNOWN as SUCCESS (OpenTherm.cpp:526). The same SUCCESS gate means a Data-Invalid cannot be cached (INVALID, :1683).
- **R2. "OEM ids 128-255 alias onto 0-127."** Refuted. TASK-1177; guard at :1520 and :2715 (`msgId < sizeof(...)`).
- **R3. "A late or mismatched boiler reply poisons another id's cache slot."** Refuted. The slot index is the response's own id (:1519), so a stray reply fills its own id with its own data. The library does not check that the reply id equals the request id (OpenTherm.cpp:444, 518-527), which could matter for the unknown counters, but that is area D.
- **R4. "WRITE_ACK in the cache makes master mode answer reads with write echoes, which is wrong."** Refuted as a bug: for ids the boiler accepts as write, the echo is the value the boiler holds; kept as a table row (low difference).
- **R5. "Master-mode status reply carries the wrong high byte."** Refuted as a bug: the gateway owns the master flags in master mode; documented by design in ADR-066/064.
- **R6. "Gateway mode answers the thermostat from the cache today (ADR-224)."** Refuted: not implemented; no `ADR-224`/`1759` reference in `src/`.
- **R7. "`updateWriteCache` is a no-op for ids 20/71/100, so SC=/VS=/OVR-flags are never sent."** Refuted: they are sent once through `otCmdEnqueue`; `updateWriteCache` is only the periodic-refresh hook, and these ids are not meant to be refreshed (SC has its own heartbeat expiry :2275-2279).
- **R8. "Mode switch leaves stale write values."** Refuted: `resetTransientState` sets `valueSet=false` for all write entries (:2548) and flushes the queue (:2504-2507).

## Not verified

- No bench or capture. E-cand 2's boot-window arithmetic is derived from `scheduleMasterRequest` (:1758) and `lastSentMs=0` initialisers (:150-152); I did not time it on a unit.
- Whether any thermostat model blacklists an id after repeated Unknown-DataID (E-cand 2) or reads ID 0 HB (E3 row 8): not checked against captures.
- Whether boilers alternate Unknown and Data-Invalid for one id (E-cand 6): not checked.
- `satBoilerHardwarePresent` consumers beyond SATcontrol.ino:2760-2773 and the REST/MQTT guards named in its comment were not opened.
