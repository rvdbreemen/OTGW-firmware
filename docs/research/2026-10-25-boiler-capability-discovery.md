<!-- Research note for Robert's question on Discord 2026-10-25 13:15 UTC (message 1563903871560974430). Produced by a five-angle exploration, a synthesis and a citation critic (workflow wf_08e546c0). Exploration only: no code was changed. -->

# Research note (corrected): learning boiler capabilities and using them to choose control

**Status:** design exploration only. Read-only on `dev` at `be6c9b5f9`. Nothing edited, built, run or posted.

**Citation keys:**
- `spec:` is `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`.
- Firmware files are under `src/OTGW-firmware/`.
- `gateway.asm` is `other-projects/otgw-6.6/gateway.asm`. Field units run PIC 6.8 (TASK-1344 notes), so every PIC citation comes from a single source, version 6.6.
- thermo-nova paths are relative to `other-projects/SAT-releases-thermo-nova/custom_components/sat`. That snapshot records upstream 7beaaf5.
- `corr:` is `docs/sat/sergeant-d-corrections.md`.
- `capslog` is `other-projects/otmonitor-6.6/otmonitor.vfs/lib/tcl8/8.6/capslog-1.0.tm`.

**Verification:** every citation without "(relayed)" was opened in this session. The ones marked "(relayed)" come from earlier explorations and were not reopened. **Fact**, **Inference** and **Opinion** are labelled.

## 0. Short answer

- **Partly.** OpenTherm v4.2 lets a master learn three things:
  1. **Declared facts:** the MsgID 3 configuration flags, the MsgID 6 remote-parameter flags, and the bounds and capacity in MsgID 15/48/49.
  2. **Reply type per DataID.**
  3. **Behaviour over time.**
- **The bus cannot show:**
  - a boiler that ACKs a write and ignores it;
  - gas boiler vs heat pump;
  - whether the CS is actually followed.
- **Field reports weaken the declared signals.** The PIC source names boilers that misreport MsgID 6 yet return valid 48/49 (gateway.asm:1267-1268, 2888-2891).
- **The firmware already records reply types per ID** in six bitmaps. SAT never reads them (grep). The "acked" and "unsupported" bits can both be contaminated by PIC-generated answers (section 1.6, M3).
- **SAT today chooses its control behaviour from:** a MemberID brand table with shared IDs, manual settings, and fixed caps.
- **Opinion:** a capability profile is worth building, first as information, then as onboarding defaults the user confirms.
  - Letting it change SAT control at runtime is a SAT design change.
  - Under the CLAUDE.md SAT rules, each area needs thermo-nova citations, a check against his entries, and his OK.
  - thermo-nova does not choose a control strategy from boiler capabilities.

## 1. What can be learned, how reliably, and what masks it

### 1.1 Reply types (Fact)

- **Read replies** (spec:1210-1221): READ-ACK (valid); DATA-INVALID (ID recognised, data "not available or invalid"); UNKNOWN-DATAID (ID not recognised).
- **Write replies** (spec:1228-1238): WRITE-ACK; DATA-INVALID (the value sent is invalid); UNKNOWN-DATAID ("does not recognise or does not support").
  - The slave may modify the value it echoes back (spec:1228-1229).
- **General rule** (spec:1388-1389): any non-mandatory ID may be answered UNKNOWN-DATAID, and masters "should be designed to act in a manner consistent with this rule".
- **Stability:** the spec says nothing about it (not found).
  - **Inference:** UNKNOWN-DATAID reads as a property of the device. DATA-INVALID is a state: the spec's own MsgID 25 example is a sensor fault (spec:1372-1374).
  - A PIC-generated Unknown (UI=, blacklist) looks the same on the A side (1.6).

### 1.2 Mandatory IDs (Fact)

The table is at spec:1343-1386.

| MsgID | Required slave reply |
|---|---|
| 0, 3 | READ_ACK |
| 1 | WRITE_ACK |
| 2 | WRITE_ACK; only used when the master needs Smart Power |
| 14 | "Must respond with WRITE_ACK" (spec:1364-1369). Its use is "Not mandatory, Recommended for use in on-off control mode" (spec:1367). |
| 17, 25 | READ_ACK or DATA_INVALID (spec:1370-1374) |
| 93-95 | READ_ACK or DATA_INVALID |
| 125, 127 | READ_ACK (spec:1378-1386) |

**Consequences:**
- **Intergas on MsgID 14:** its 14W reply is UNKNOWN-DATAID. The capture is `B700E0000` (gruijter, alpha.504, TASK-1344 notes, line 37). That departs from the v4.2 table.
  - A mandatory list first appears in 1.4A (spec:136). The table itself came in 2.0A (spec:148), where MsgID 14 was also renamed (spec:153). MsgID 125 only came in 2.1A (spec:169).
  - **Inference:** an older boiler may predate the rule, and may not answer 125 either.
- **MsgID 15, 28, 48, 49 and 57 are not in the table.** An Unknown reply is legal.
  - The spec makes sensor-related data "conditional mandatory" (spec:1402-1403, 1409-1416). The Function Matrix that assigns IDs is not in the repo (not found). So "28 is conditional" is an inference.
- **A boiler without modulation reporting should answer MsgID 17 with DATA-INVALID.** An Unknown-only detector misses it.

### 1.3 Declared capabilities (Fact)

**MsgID 3 HB** (spec:1603-1620). It must be read at least at start-up (spec:1359-1360).

| Bit | Meaning |
|---|---|
| 0 | DHW present |
| 1 | Control type: clear = modulating, set = on/off |
| 2 | Cooling supported |
| 3 | DHW storage tank |
| 4 | Master low-off and pump control: **set = not allowed** |
| 5 | CH2 present |
| 6 | Remote water filling |
| 7 | Heat/cool switching done by the slave |

- The LB holds the MemberID (spec:1621).
- There is no bit for a return sensor, modulation reporting, or MsgID 14 acceptance (not found).

**MsgID 6** (spec:2021-2041):
- The HB holds the transfer-enable flags and the LB the read/write flags. Bit 0 is the DHW setpoint (56), bit 1 is max CH setpoint (57).
- An Unknown reply means "no support for remote-parameters" (spec:2023-2025).
- **Field caveat:** Schelte's source names boilers that say MsgID 57 is not supported yet return reasonable MsgID 49 data, and boilers that support 48/49 without MsgID 6 (gateway.asm:1267-1268, 2888-2891). So MsgID 6 is the spec's answer, but not a reliable one.

**Bounds:**
- MsgID 48 holds the DHW bounds and MsgID 49 the MaxTSet bounds (spec:2057-2060, 2381-2382).
- The master "should read" them. If they are missing, "the master can apply default bounds as it chooses" (spec:2016-2019).
- MsgID 57 is the value, not the bounds (spec:2062, 2384).

**Capacity:** MsgID 15 holds max kW (HB) and min modulation % (LB). It is optional (spec:2219-2236).

**Identity:**
- MemberID plus MsgID 125, 127 and 93-95 (spec:1652-1655, 1693-1694, 1737-1793).
- 125 and 127 must be READ_ACK'd (spec:1378-1386). They are the spec's only key for a per-model quirk table. The spec does not define what their values mean.

**Gas vs heat pump:** the spec has no field for it (not found).
- Proxies: bit 2 (cooling) and bit 7.
- The firmware comment: heating-only heat pumps do not set bit 2, and a hybrid is invisible on one OT bus. "Never drive control off this bit" (OTGW-Core.ino:3141-3146; ADR-157 Accepted).

### 1.4 What only behaviour shows

- **Fact:** MsgID 14 is write-only (spec:2355). There is no flag for "MM honoured". The slave "only needs to control to the value" (spec:1570-1574).
- **Opinion:** two behaviour checks could stand in:
  - **MM honoured:** MsgID 17 vs the MM sent. Impossible without MsgID 17.
  - **CS followed:** MsgID 25 and the flame bit vs the CS sent.
- **Prior art (Fact):** thermo-nova learns and persists "modulation reliable" from flame-on samples (device/modulation.py:21-46; device/__init__.py:119-132). It uses the result only to decide the modulation direction (device/__init__.py:171-180).

### 1.5 When each signal can be learned (Fact)

**At boot:**
- **SAT PM chain:** asks for 3, 15 and 48 one at a time on a PIC, with a 60 s timeout and a second pass. On OT-Direct they go out directly (OTGW-Core.ino:3898-3911; SATcontrol.ino:4055-4057).
- **The PIC itself:** queues 6, 48 and 49 (plus operating modes) at init (gateway.asm:1269) and sends them in alternative slots (gateway.asm:3410-3418).
- **OT-Direct polling:** 3, 6, 15, 48 and 49, among others (OTDirect.ino:162-177).

**At runtime:**
- Reply types for every ID somebody asks for.
- Write support shows only when somebody writes: CS=, MM=, SW=, SH=.

**After failures:**

| Path | Behaviour |
|---|---|
| PIC | 3 consecutive Unknowns per ID, counted in RAM (gateway.asm:538-545, 2437-2451). UI= marks are stored in EEPROM and reloaded at boot (4858-4863, 1298-1312). |
| OT-Direct | 3 strikes per schedule entry. Only ID 0 is exempt (OTDirect.ino:1450-1471). |
| ESP | Latches on the first Unknown (OTGW-Core.ino:5438-5450). Persists in `/ot-boiler.json`. Retracted only by a genuine B ACK (5422-5437). |

### 1.6 What masks the truth

**M1. The PIC answers for the boiler (Fact):**
- A boiler Unknown on 57 or 56 becomes an ACK to the thermostat, carrying the SH/SW value or the bound (gateway.asm:3164-3176, 3128-3140).
- MsgID 6: the A frame always shows read access (gateway.asm:2906-2909).
- MsgID 125: forced to at least 3.0 (3306-3316).
- MsgID 28: replaced by the board sensor when one is configured as return sensor (3074-3088).
- For a blacklisted or UI= ID X, the boiler receives an alternative Y. The thermostat gets an A(X) built by the PIC: an Unknown, or an ack with the PIC's own value (2396-2399, 2463-2473).

**M2. The PIC never blacklists 0, 1, 3, 14, 17 and 25 (Fact).** For 27 this holds when an alternative is used.
- Callers: gateway.asm:2712, 2798, 2540, 2959, 2554, 2562, 3053-3054. MandatoryID itself is at 3383-3388.
- So an Intergas 14W Unknown shows again as a real B frame on every MM= write.

**M3. Inference, read in code, no capture yet: PIC-built A frames can contaminate both halves of the ESP bitmap.**
- B and A pair up only when they have the same ID within 500 ms (OTGW-Core.ino:5326-5328). An unpaired A keeps `bAnswerOverride` false (5336) and counts as boiler evidence (5401).
- In `T(57) R(Y) B(Y) A(57 ack)` that A sets `boilerAckedRead` for 57 (5402-5406). An earlier "unsupported" for 57 stays set, because only a B retracts it (5422-5426).
- A UI= or blacklist A(X Unknown) sets `boilerUnsupported*` for X, because the Unknown branch has no B-only gate (5438-5450).

**M4. Data-Invalid is not tracked per ID (Fact).** OTGW-Core.ino:1699 is only a log label.

**M5. Passive learning sees only IDs that somebody asks for.**

**M6. PS=1 hides raw frames** (gateway.asm:2341-2358). The summary behaviour is relayed.

**M7. Three different counters (Fact):** PIC 3 strikes in RAM, OT-Direct 3 strikes, ESP latch on the first Unknown.

**Prior art (Fact):** otmonitor's capslog is a deliberate capability survey.
- It splits IDs into `needed` {0,1,2,3,14,17,25}, `preferred` and `wronly` (capslog:11-17).
- It sets GW=1, uses UI= to free a slot for probing, sends PM= for every ID not yet seen, and restores its changes with KI= (capslog:218-316).

## 2. Concept: a boiler capability profile (Opinion)

One record per boiler. Each fact has a source, a confidence and an age.

| Fact | Source | Confidence |
|---|---|---|
| Control type, DHW, tank, CH2 | MsgID 3 HB, B frame | High (mandatory) |
| Cooling / heat-cool switching | MsgID 3 HB bits 2, 7 | High as a bit; **low as a heat-pump signal** (OTGW-Core.ino:3141-3146) |
| Brand / model | MemberID + MsgID 125/127 (+93-95) | MemberID is ambiguous: 4 is shared by Atag, Baxi, Brotge, DeDietrich and Geminox; 27 by Immergas and Sime (SATcontrol.ino:196-214). AUTO picks the first match (249-264). |
| MaxTSet / DHW setpoint writable | MsgID 6 LB bits 1/0, **B frame only** | **Medium.** Named boilers misreport it (gateway.asm:1267-1268, 2888-2891). |
| MaxTSet and DHW bounds | MsgID 49 / 48, B frame | High if valid; otherwise defaults (spec:2016-2019) |
| Capacity / min modulation | MsgID 15 | High if valid |
| MM write accepted | B reply to MM= | High (the PIC never blacklists 14) |
| MM honoured | Behaviour: MsgID 17 vs MM | Medium; none without MsgID 17 |
| Modulation reported | MsgID 17: Unknown / Data-Invalid / varying values | Medium, in the thermo-nova style |
| Return temperature present | MsgID 28 B reply; must tell the TS=R board sensor apart | Unknown-from-B: high |
| Heating source | User (ADR-157) | The bus cannot tell |

**Rules (Opinion):**
1. Only genuine B frames, not local and not replayed, may set **or** retract a verdict.
2. Keep read and write verdicts separate.
3. Use three states per ID: supported / unsupported / no value now.
4. A verdict flips only after N consistent answers. Offer a reset when MemberID or MsgID 127 changes.
5. Show the source and age of each fact, and allow a user override.

**Already in the firmware (Fact):**
- the six bitmaps (OTGW-Core.ino:5063-5093);
- `/ot-boiler.json`;
- REST `boiler-support` (restAPI.ino:811-856, unsupported only) and `ot-support` (857+, all six bitmaps);
- MQTT `boiler/unsupported_msgids` (MQTTstuff.ino:1826-1845);
- the stored MsgID 6 and 49 words (OTGW-Core.h:60, 95).

**Missing:**
- B-only setting of verdicts;
- Data-Invalid tracking;
- B-vs-A provenance for the stored 6/48/49 values;
- a profile view;
- any SAT consumer (grep of SAT* finds no `isBoilerMsgId*`).

## 3. Mapping the profile to control choices

**Ground rule (Fact, CLAUDE.md "SAT ground truth"):** his recorded entries, then thermo-nova, are the ground truth for SAT. The profile may **inform** a choice an entry already fixes. It may change one only with his OK.

**thermo-nova (Fact):**
- Its support flags are fixed per coordinator. The OTGW MQTT coordinator returns True for all four (coordinator/mqtt/opentherm.py:43-57).
- The base class sets `supports_relative_modulation` False for Ideal, Intergas, Geminox and Nefit (coordinator/__init__.py:304-313). MM management is always True (316-322).
- The flags gate which entities exist (number.py:17-20, binary_sensor.py:59-62), not the control strategy.
- Control branches on the presence of Tret (heating_control.py:392-395), and modulation reliability is learned (device/modulation.py:21-46).
- No MsgID 49 handling was found. It reads `MaxTSet`, the 57 value (coordinator/mqtt/opentherm.py:18).
- **So:** choosing a control strategy from capabilities would be our own design.

### 3.1 Entries that bear on this

| Entry | What it fixes | What the profile may do |
|---|---|---|
| 6 (corr:135) | Tret only in the PWM start-up. "Intergas by default does not even report the Tret" (corr:159). gruijter's Tret comes from a board sensor (corr:175-176). | Inform |
| 13 (corr:483) | Force PWM removed; SAT picks the mode itself (corr:497-500) | **Conflicts with "on/off boiler → PWM only".** Ask him. |
| 20 (corr:677) | Start-up + suppression on by default for all systems (corr:735-740). Intergas and Ideal "ignore MM" (corr:703). | Inform only |
| 21 (corr:768) | MM=0 in PWM; heat pump and hybrid never 0; Geminox at least 10. Brands rely on what the user confirms (corr:857). Intergas: "You cannot set the max modulation" (corr:823-827). | Propose a brand; the user confirms |
| 29 (corr:1392) | When his documentation and the code disagree, the code wins (corr:1416-1417) | Not a bar on deviations. The CLAUDE.md rule is what requires his OK. |
| 30 (corr:1452) | Device type and heating system are separate settings. He proposed auto-detecting a heat pump from "ID3:LB2" (corr:1493-1496); the spec says HB bit 2 (spec:1606). Validation on the Elga Ace is open (corr:1548). ADR-157 makes the manual setting authoritative. | Hint only. **Report the LB/HB mismatch and the open validation.** |
| 33, 53 (corr:1670, 2749) | DHW slider bounded by MsgID 48; HW switch only with a tank (bit 3); SW= at once on every change | Already capability-driven |
| 44 (corr:2383) | CS, MM and CH go out on every control update, as thermo-nova (corr:2401-2402) | **MM= keeps going out even when 14W is Unknown** |
| 45 (corr:2439) | No Tret: skip the hold, flood CS and PM=25 for 20 s, MI=500. "How 'no return temperature' is detected" is open (corr:2509). | Supplies the evidence; **the definition is his question.** thermo-nova's own definition: `return_temperature is None` (heating_control.py:393; coordinator/mqtt/opentherm.py:114-118). |
| 48 (corr:2590) | MaxTSet slider "bounded by the MsgID 57 values (Lower/Higher)", default 20/80, sent to the boiler (corr:2600). Open: is a 57 write safe on boilers that reject it (corr:2625)? | **Conflict with the spec:** bounds are MsgID 49 (spec:2059-2060). thermo-nova has no 49 handling (not found). The PIC range-checks SH= against 49 or the defaults 10/90 (gateway.asm:264-265, 3107-3110, 4165-4174), sends it on its own only when ID6:LB1 is set (450, 4177), and stores it in EEPROM (4183-4184). Report; do not resolve. |
| 51, 54 (corr:2687, 2777) | SAT caps every CS at the user's MaxTSet, ignition hold included | SAT clamps itself whether or not 57 is writable |

### 3.2 Candidate mappings (Opinion; each needs his OK where it touches SAT)

1. **Continuous vs PWM.**
   - Today the choice comes from the PID and cycle logic (SATcontrol.ino:5264-5300). No capability signal is involved.
   - An on/off boiler (MsgID 3 bit 1) suggests PWM. The spec only says MsgID 14 is recommended in on/off control (spec:1367).
   - **This is a question for him** (entry 13). Whether any real boiler sets this bit was not checked.
2. **MM.**
   - Today `satComputeMM` (SATcontrol.ino:387-397) depends on the mode, the heating source, the configured maximum and the Geminox/Immergas quirks.
   - **MM= keeps going out (entry 44).** A "14W unsupported" verdict changes only what the UI says ("MM has no effect on this boiler").
   - It also tells us that an MM release cannot be confirmed at the boiler (TASK-1344).
3. **Ignition without Tret.**
   - Today `satPwmFlameOffHold` floors at the cold setpoint (SATcontrol.ino:817-823).
   - The profile can supply "Tret present" for entry 45 (TASK-1371), once he answers how it is detected.
4. **CS ceiling and MaxTSet.**
   - Today `satEffectiveMaxSetpoint` uses MaxTSet, or 75 °C below 30, then the caps (SATcontrol.ino:314-326). SAT sends no SH= (grep).
   - Entries 51/54: SAT clamps itself. 49 and 6 tell whether SH= is meaningful. The bounds question is his (entry 48).
5. **DHW.**
   - SAT sends SW= only within 30..60 (SATcontrol.ino:4987-4992, SATtypes.h:458-459). The v2 slider intersects that range with MsgID 48 (v2.js:4512-4525).
   - Entries 33 and 53 already set the direction.
6. **Heat pump vs gas.** It stays a user setting (ADR-157). The bit 2/7 hint is shown at onboarding. Entry 30's auto-detect proposal is open with Robert.
7. **Brand quirks.**
   - MI=500 is applied by brand in thermo-nova (coordinator/mqtt/opentherm.py:171-172) and in the firmware (SATcontrol.ino:4058-4061).
   - The profile proposes a brand and the user confirms it (entry 21).

**Candidate difference to verify with a second reader:** the firmware forces `bModulationReliable = false` for `SAT_QUIRK_NO_REL_MOD` brands (SATcontrol.ino:5140-5143). thermo-nova learns it from data only (device/modulation.py:21-46). The TP=11:12 difference is already recorded in entry 44 (corr:2421).

**Stale citations in entry 45:** `SATcontrol.ino:808-814` is now 817-823, and `3950-3953` is now 4058-4061.

## 4. Risks

- **R1. An ACK is not action** (spec:1228-1229). Intergas "cannot set the max modulation" (corr:823-827), and the capture shows the Unknown reply. Both are consistent. The 14W reply type for Ideal and Nefit was not found.
- **R2. Silence is legal** (spec:1388-1389).
- **R3. Masking** (M1-M3). This includes the EEPROM-persisted UI= marks.
- **R4. Oscillation.** Use hysteresis, decide after N answers, and freeze the profile during a PWM cycle. Data-Invalid never counts as "unsupported".
- **R5. Stale persistence after a boiler swap.**
- **R6. Probing has side effects.**
  - SH= changes the boiler's MaxTSet and is stored in the PIC's EEPROM.
  - capslog frees slots with UI=.
  - **Opinion:** no write probing.
- **R7. User override.** The installer knows things the bus does not.
- **R8. Two evidence paths.** OT-Direct uses local answers (`bLocalAnswer`, OTGW-Core.ino:5397-5401) and its own 3-strike table.
- **R9. SAT governance:** citations, a check against the entries, one question at a time.

## 5. Options

**A. Passive profile, shown only.**
- B-only setting and retracting of verdicts, Data-Invalid tracking, B-vs-A provenance for 6/48/49, one profile view.
- Pros: no control change, useful for support, produces field data.
- Cons: SAT still uses its brand table.

**B. The profile proposes onboarding defaults; the user confirms each one.**
- Pros: matches entry 21 and thermo-nova's config flow; no runtime oscillation.
- Cons: a boiler swap needs a re-offer, and entries 45 and 48 need his answers.

**C. Runtime adaptation.**
- Pros: covers boilers nobody configured.
- Cons: our own design; conflicts with entries 13 and 44 as sketched; the masking risk feeds straight into control; entry 20 already covers the biggest case.

**Recommendation (Opinion):**
1. A now. Bench-capture M3 first.
2. Then B.
3. C only per area, after his answer.
4. Queue the questions as Q40 and Q41 in `docs/sat/open-questions.md`, asked one at a time after Q12:
   - **Q40 (Tret detection):** A) never reported, `None` as in thermo-nova (heating_control.py:393); B) Unknown on MsgID 28; C) Data-Invalid or stale; D) present only through the board sensor.
   - **Q41 (MaxTSet bounds):** entry 48 says the MsgID 57 bounds. The spec puts bounds in 49 and writability in 6, and named boilers misreport 6. Which applies?
5. **Q42, a third question, before any "on/off → PWM" mapping:** does entry 13 rule out a capability-forced PWM?

## 6. Not verified / open

- M3 (both halves of the bitmap) was read in code only. It needs a capture.
- The Ideal and Nefit reply types on 14W were not found.
- No capture was checked for MsgID 3 bit 1 or MsgID 6 flags.
- The Elga Ace bit 2 check is open (corr:1548).
- The effect of MI=500 in gateway mode is open (corr:2498-2500).
- The PIC citations are 6.6; field units run 6.8.
- OT-Thing, pyotgw-master/ and core-dev/ opentherm_gw were not opened.
- The Function Matrix is not in the repo.

---

Follow-up task for M3: TASK-1391. Questions queued as Q40-Q42 in docs/sat/open-questions.md.
