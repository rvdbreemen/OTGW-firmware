# SAT commissioning profiles: gas, heat pump and hybrid (2026-11-01)

Deliverable of **TASK-1412.17** ("Define equipment-specific gas, heat-pump and hybrid commissioning profiles"). Review coverage R14, R17 and R42 of the [remediation plan](sat-low-load-remediation-plan-2026-10-28.md) (WP7). It defines the **PH-PROFILE** record of the [verification plan](sat-low-load-verification-plan-2026-10-28.md) (section 5, line 118) and the profile half of gate **LL-015** (line 102). Inputs: the [requirements crosswalk](sat-low-load-requirements-crosswalk-2026-10-31.md) of TASK-1412.2 and its [second read](sat-low-load-requirements-crosswalk-2026-10-31.second-read.md).

This document is a specification of what each profile must record and what SAT does today per source. It changes no behaviour, answers no open question and makes no efficiency, comfort or compressor-protection claim.

## 0. Sources and method

- **Sergeant D entries:** `docs/sat/sergeant-d-corrections.md`, entries up to 191. Cited as "entry N".
- **thermo-nova:** `other-projects/SAT-releases-thermo-nova/custom_components/sat` at the pin `7beaaf5`. Locations marked "opened" were opened in this session. Others are marked "as recorded in entry N, not re-opened".
- **Firmware:** `src/OTGW-firmware` at `dev` HEAD `159f47177`. Every firmware line number was taken from a `grep -n` or read in this session.
- **OpenTherm:** `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` (cited as "OT protocol line N"). AFS text: `OTGW-other-projects/OT-Spec/OT Application Functional Specification v1-1f.md` (cited as "AFS md:N").
- **Status values** used in every table: **decided** (a Sergeant D entry or an Accepted ADR settles it), **firmware default** (what the firmware does today with no recorded decision behind the specific value, or a decision under review), **unknown-blocks-claim** (no source; the claim that depends on it is not allowed until the installation record supplies it).

**Crosswalk correction (found while writing, for the crosswalk owner).** Crosswalk section R14 says the firmware sends MM=100 to a heat pump outside PWM and that TASK-1426 is To Do. That is superseded: commit `90d935536` ("TASK-1426 a heat pump gets the configured max modulation outside PWM too") landed, and `satComputeMM` (`SATcontrol.ino:467-479`) now gives a heat pump and a hybrid the configured maximum in and outside PWM, 100 when that is 0 (`(void)heatPump;` at `:471`). The crosswalk's line numbers for R14 (`:453-460`, `:467-479`) match HEAD; only the MM content is stale. Its period line ("heat pump and hybrid") is correct since commit `4a3ca69a8` (TASK-1531). This document cites HEAD, not the crosswalk.

## 1. The PH-PROFILE record (required fields, all profiles)

Every installation that is used as evidence fills in this record before any actuator test (verification plan line 118). An empty applicable field blocks acceptance of that equipment; "not applicable" needs a reason.

| # | Field | What must be recorded | Source of the requirement |
|---|---|---|---|
| P1 | Source type | gas boiler, heat pump (on/off or modulating), hybrid; as set in `SATsource` | WP7.1; R17 |
| P2 | Emitter | radiators or underfloor (`SAT_HSYS_*`), separate axis from P1 | entry 30 title ("Device type and heating system are separate settings") |
| P3 | Native control mode | what the appliance does on its own: room or weather compensation, its own anti-cycle, own modulation | WP7.1, WP7.3 |
| P4 | Interface capabilities | PIC or OT-Direct (actual detected mode, verification plan section 5); ID3 HB bits 0-6 (OT protocol lines 1603-1615); ID14 accepted, rejected or ignored; ID15, ID17, ID25, ID28, ID49, ID57 support; manufacturer quirk confirmed by the user | WP7.1; LL-010; entry 21 (manufacturer confirmation) |
| P5 | Minimum output | ID15 low byte if reported (`OTGW-Core.h:70`, `:389`); manufacturer minimum output in kW; what MM=0 does on this unit (tested, not assumed) | WP7.1, WP7.2 |
| P6 | ON/OFF/start limits | manufacturer minimum ON, minimum OFF/restart, maximum starts per hour, startup characteristics (compressor soft-start, pre-purge) with units and source | verification plan line 118 |
| P7 | Hydraulic limits | minimum water flow or volume, mandatory pump or bypass, buffer tank | verification plan line 118; WP7.1 |
| P8 | Temperature ceiling | manufacturer maximum supply temperature; the installer's MsgID 57 value before SAT ran; SAT's resulting CS cap | verification plan line 118; entries 48, 51, 152, 155 |
| P9 | Measurement availability | which signal shows real burner or compressor operation (ID0 LB3, an external meter, the appliance's own interface); flow and return sensors; energy meter | verification plan section 5 step 4; WP7 acceptance ("record actual compressor state where flame status is not meaningful") |
| P10 | Native protections | defrost, DHW priority, backup heater, anti-cycling, frost protection: present or not, and how each is observed | AC3 of TASK-1412.17; WP7.3 |
| P11 | Hand-back state | the appliance and thermostat settings before SAT was enabled, so rollback can be checked (section 6) | AC3 of TASK-1412.17 |

## 2. Gas boiler profile

### 2.1 Gas combi boiler versus gas system boiler

**Not distinguished.** SAT control has one gas path. `satGetEffectiveHeatingSource()` returns `SAT_SRC_GAS_BOILER` for both (`SATcontrol.ino:297-302`). The only place where a storage tank changes anything is the DHW user interface: ID3 HB bit 3 (DHW config, storage tank, OT protocol lines 1608-1609) adds a DHW enable switch next to the slider (entry 33). No Sergeant D entry and no thermo-nova code distinguishes control timing by boiler build (thermo-nova `HeatingSystem` has only UNKNOWN, HEAT_PUMP, RADIATORS, UNDERFLOOR, `types.py:110-114`, opened). One table therefore covers both. Whether a system boiler needs its own profile is Open decision OD-7.

### 2.2 Table

| Field | Value or rule today | Source | Status |
|---|---|---|---|
| P1 source | `SATsource` 1 (gas boiler); `SATsource` 0 (AUTO) also resolves to gas | `SATcontrol.ino:297-302`; ADR-157 | decided |
| P1 detection hint | ID3 HB bit 2 (cooling supported) sets `iDetectedHeatingSource` for telemetry only; it never selects the profile | `OTGW-Core.ino:3197`; `SATcontrol.ino:290-296` (comment); second read row 52 (HB bit 2, not LB2) | decided (ADR-157) |
| P2 emitter | radiators or underfloor, separate setting | entry 30 | decided |
| P3 SAT mode choice | SAT chooses continuous or PWM itself; ID3 HB bit 1 (on/off control type) does not force PWM | entry 102 | decided |
| P3 boiler's native control | the boiler's own room or weather compensation, own modulation range and own switch-off hysteresis: not known to SAT | WP7.1 | unknown-blocks-claim (installation record) |
| P4 interface | PIC or OT-Direct. Brand handling from the user-confirmed manufacturer: Geminox MM floor 10, Immergas MM cap 80, Intergas/Ideal ignore MM | entry 21; `SATcontrol.ino:215` (table), `:467-479` | decided |
| P4 Immergas `TP=11:12` value | the firmware sends `TP=11:12=<final setpoint>` (`SATcontrol.ino:6772-6777`); thermo-nova sends `TP=11:12={min(value, 80)}` with the max relative modulation as value (`coordinator/mqtt/opentherm.py:215-217`, opened), as entry 21 records | entry 21 | firmware default; differs from thermo-nova, needs a second reader and a task (not in this task's scope) |
| P4 ID3 HB4 (master low-off&pump) | not read by SAT control today; LL-010 gate | crosswalk R01, LL-010 | unknown-blocks-claim (QD-1..QD-3) |
| P5 MM in PWM | 0 in both PWM phases | entry 21; thermo-nova `heating_control.py:134-140`, `:515-520` (opened); `SATcontrol.ino:472` | decided |
| P5 MM outside PWM | configured maximum (`satmaxmodulation`) | entry 21; `SATcontrol.ino:473` | decided |
| P5 meaning of MM=0 | AFS: relative modulation 0 % is the minimum modulation level, not off (AFS md:660-668, opened: "0% y %") | OT-DEF1 | decided for the definition; per-unit behaviour unknown-blocks-claim until tested (verification plan section 5 step 6) |
| P5 minimum output kW | ID15 low byte is read for the self-tune input only (`SATcontrol.ino:6813-6814`), not for control | firmware | unknown-blocks-claim (installation record) |
| P6 minimum ON (requested) | 180 s | entry 31 via entry 40; thermo-nova `pwm.py:53` (opened); `SATcontrol.ino:445-448` | decided |
| P6 PWM period | 3600 / cycles per hour; choices 3 or 4, default 3 | entries 30, 52; `SATcontrol.ino:434-441`, `:453-460`; `settingStuff.ino:1171` | decided |
| P6 hourly start limit | 3 or 4 PWM ON starts per rolling hour, only PWM's own ON phases count | entries 59, 81; `SATcycles.ino:368-375` | decided |
| P6 shortest requested OFF | 180 s in duty range 4 (`offTimeMs = minOnMs`); range 5 is continuous ON | `SATcontrol.ino:1274-1278`, `:1237-1252` | firmware default (follows thermo-nova's mapper, `pwm.py:53-61` opened) |
| P6 manufacturer minimum OFF and anti-cycle | not known to SAT; the boiler's own anti-cycle governs | verification plan line 118 | unknown-blocks-claim (installation record) |
| P7 hydraulic | nothing in SAT control (grep of `SAT*.ino` for pump, bypass, flow rate finds no control use) | firmware | unknown-blocks-claim (installation record) |
| P8 ceiling | user MaxTSet, else 62 (radiators) or 50 (underfloor) default; bounds from ID49, else 20..80; `SH=` only after the user chose a value | entries 48, 51, 67, 100, 110, 148, 152; `SATcontrol.ino:308-310`, `:330-359`, `:382-398`, `:403-417` | decided |
| P9 burner signal | ID0 LB3 flame (`satIsFlameOn`, `SATcontrol.ino:2284-2288`; OT protocol line 1446) | firmware | firmware default; an injected flame bit is not a physical measurement (verification plan section 5 step 4) |
| P9 flow / return | ID25 / ID28, not mandatory, may be Data-Invalid (OT protocol lines 1372, 1880, 1883) | crosswalk OT-M5, R09 | unknown-blocks-claim per unit (LL-011) |

## 3. Heat-pump profile

### 3.1 On/off versus modulating

**Not distinguished in today's firmware.** SAT ignores ID3 HB bit 1 (control type) for control (entry 102), and there is no setting for a modulating versus on/off heat pump. A split exists only as a proposal in Q-1527a: entry 181 records the first version ("heat pump controls itself" for a modulating unit, a long time-proportional PWM for an on/off unit); the Q-1527a row in `docs/sat/open-questions.md` (line 163) records that after round 13 the direction for on/off heat pumps moved to a degree-minute mode. Q-1527a is open and not re-posed here, and none of its numbers is a value of this profile. One table therefore covers both; the rows that Q-1527a would change are marked "under review".

### 3.2 Table

| Field | Value or rule today | Source | Status |
|---|---|---|---|
| P1 source | `SATsource` 2, set manually. Detection never selects it (ADR-157). A heating-only heat pump does not set the cooling bit, so the hint misses it (`SATcontrol.ino:292-295`, comment) | ADR-157; R17 | decided |
| P2 emitter | radiators or underfloor, separate setting | entry 30 | decided |
| P3 native mode | SAT runs the same PID and PWM as for gas; the heat pump's own weather compensation, room influence and modulation are not used as a baseline today | WP7.3, WP7.4 | unknown-blocks-claim (installation record); behaviour change open in Q-1527a |
| P4 interface | as gas; manufacturer quirk confirmed by user (entry 21). Elga Ace stops on MM=0 (entry 21, Sergeant D's words) | entry 21 | decided for Elga Ace; per model unknown-blocks-claim |
| P5 MM in and outside PWM | configured maximum, 100 when that is 0, never 0 | entries 21, 113 (Q54 B); `SATcontrol.ino:467-479` (commit `90d935536`) | decided |
| P5 meaning of MM on a heat pump | the AFS defines MM against boiler modulation (AFS md:660-668) and gives no heat-pump rule (crosswalk R14 Clause). Entry 21: "we don't know how they will behave at MM=0" | entry 21; OT-DEF1 | unknown-blocks-claim (test the interface per model, WP7.2) |
| P5 minimum output | ID15 low byte read for the self-tune input only (`SATcontrol.ino:6813-6814`); whether a heat pump reports ID15 at all is not known; minimum compressor power in kW | firmware | unknown-blocks-claim (installation record) |
| P6 minimum ON (requested) | 180 s, as gas | entries 40, 50; `SATcontrol.ino:445-448` | decided; under review in Q-1527a (entries 181, 182) |
| P6 PWM period | `iHpCycleSeconds` 1800 s (default) or 2400 s; other values snap to the nearer one | entries 40, 50, 52; `SATcontrol.ino:453-460`; `settingStuff.ino:1050` | decided; under review in Q-1527a. A 7200 s option is a study request (entry 184), not a decision |
| P6 hourly start limit | none: starts are counted and reported, never refused | entry 81 (Q36 A); `SATcycles.ino:366-375` | decided |
| P6 PWM entry | automatic, through the 60 s sustained-overshoot switch, as every source | entry 87 (Q45 A); thermo-nova `heating_control.py:262-305` (opened: no heat-pump case); `SATcontrol.ino:6683-6694` | decided; under review in Q-1527a (entry 182 keeps entry 87 in force until Sergeant D revises it) |
| P6 compressor minimum ON, minimum OFF, starts per hour | **not known to SAT.** The 180 s is a requested minimum ON; the PWM OFF time is a request too. The generic 180 s "cannot stand in for a compressor manufacturer's rule" (verification plan line 118) | WP7.1 | **unknown-blocks-claim**: no compressor-protection claim (AC2) |
| P7 hydraulic | nothing in SAT control | firmware | unknown-blocks-claim (installation record) |
| P8 ceiling | 40 C default while the user chose no value; a chosen value replaces it, within ID49 bounds and the underfloor 50; no `SH=` while not chosen | entry 155; `SATcontrol.ino:310`, `:352-359`, `:370-373`, `:382-398` | decided |
| P9 compressor signal | SAT reads ID0 LB3 "flame" (`SATcontrol.ino:2284-2288`). What a heat pump reports in that bit is not found in the OT protocol (it names heat pumps only in the scope, line 458) | WP7 acceptance | **unknown-blocks-claim** |
| P9 flow / return | ID25 / ID28 as for gas; not mandatory, may be Data-Invalid | OT protocol lines 1372, 1880, 1883; crosswalk R09 | unknown-blocks-claim per unit (LL-011) |

### 3.3 Q37, Q45 and the entry 21/52 exceptions per profile (AC2)

| Item | Gas boiler | Heat pump | Hybrid |
|---|---|---|---|
| **Q37** (hybrid PWM timing) | n/a: the question covers hybrids only (open-questions.md Q37 row) | n/a, same reason | answered A (entry 82: gas timing); the period part is replaced by entry 188 (heat-pump cycle 1800/2400 s); 180 s minimum ON and the 3/4 hourly limit stay (section 4.1). Implemented: `SATcontrol.ino:453-460`, `SATcycles.ino:373` |
| **Q45** (heat-pump exclusion from the sustained switch) | n/a: gas always used the switch | answered A (entry 87): exclusion removed, implemented (`SATcontrol.ino:6683-6694`); decided, under review in Q-1527a (entry 182) | n/a in substance: hybrids already passed the gate before entry 87 (entry 87, firmware comparison) and still use the switch |
| **entry 21** (MM exception) | no exception: MM=0 in PWM, maximum outside (`SATcontrol.ino:472-473`) | never 0; configured maximum in and outside PWM, 100 when 0 (entry 113; `:467-479`) | never 0, same code path (entries 21, 188; `:6740-6746`) |
| **entry 52** (cycles per hour default) | default 3 per hour, choices 3 or 4 (`settingStuff.ino:1171`) | default 2 per hour = 1800 s cycle (entry 50); no longer also a start cap (entry 81) | entry 82 closed entry 52's Open as "gas default 3 per hour"; entry 188 then moved the period to the heat-pump cycle. Today a hybrid has a default period of 1800 s (`iHpCycleSeconds`) and a default hourly limit of 3 (`iCyclesPerHour`, `SATcontrol.ino:434-441`). That this combination is intended is our reading of entry 188, which names the hourly limit 3 or 4 explicitly |
| **compressor protections** | n/a (no compressor) | unknown for every model: claim blocked until P6 supplies manufacturer values and P9 shows compliance | unknown: claim blocked, and additionally the active generator is not observable (section 4.2) |

## 4. Hybrid profile

### 4.1 Rule

Hybrid (`SATsource` 3) takes **only** the heat-pump PWM cycle; everything else stays as for a gas boiler (entry 188, Q-1531a A, narrowing entry 186). Entry 82 (Q37 A, 2026-10-28) gave hybrids the gas cycle. Entries 186 and 188 replace that for the period. Entry 188 does not name entry 82 as the entry it changes; the rest of entry 82 (180 s minimum ON, the gas hourly limit and default) agrees with entry 188, so only the period line of entry 82 is superseded. This reading is ours.

### 4.2 Table

| Field | Value or rule today | Source | Status |
|---|---|---|---|
| P1 source | `SATsource` 3, set manually; invisible on one bus, so detection cannot find it (`SATcontrol.ino:292-295`, comment) | ADR-157 | decided |
| P2 emitter | radiators or underfloor, separate setting, as gas | entry 30 | decided |
| P3 SAT mode choice | as gas (entry 102 row) | entry 102 | decided |
| P3 native control | how the hybrid's own controller splits load between heat pump and boiler, and its own compensation: not known to SAT | WP7.1 | unknown-blocks-claim (installation record) |
| P4 interface | as gas (PIC or OT-Direct, manufacturer quirks, HB4 per LL-010) | entry 21; crosswalk R01 | as the gas rows |
| P5 minimum output | ID15 as gas (self-tune only); minimum output of each generator in kW | `SATcontrol.ino:6813-6814` | unknown-blocks-claim (installation record) |
| P5 meaning of the configured MM | per generator, not tested; the gas MM=0 definition does not transfer (section 7) | entry 21; OT-DEF1 | unknown-blocks-claim |
| P5 MM | never 0: configured maximum, 100 when that is 0, in and outside PWM. Entry 113 does not cover hybrids; entry 188 keeps the existing never-0 rule | entries 21, 188; `SATcontrol.ino:467-479`, call site `:6740-6746` | decided |
| P6 minimum ON (requested) | 180 s | entries 82, 188; `SATcontrol.ino:445-448` | decided |
| P6 PWM period | heat-pump cycle, 1800 or 2400 s | entry 188 (Q-1531a A); `SATcontrol.ino:453-460` (commit `4a3ca69a8`, TASK-1531) | decided |
| P6 hourly start limit | 3 or 4 per rolling hour, as gas (only `SAT_SRC_HEAT_PUMP` is exempt) | entries 59, 82, 188; `SATcycles.ino:373` | decided |
| P6 PWM entry | sustained switch, as every source | entry 87 | decided |
| P8 ceiling | gas defaults 62 / 50; the 40 C heat-pump cap does not apply (`satGetMaxSetpoint` tests `== SAT_SRC_HEAT_PUMP` only, `SATcontrol.ino:370-373`, `:408`) | entry 188 | decided |
| P6 shortest requested OFF | 180 s in duty range 4, as gas, at the heat-pump period | `SATcontrol.ino:1274-1278` | firmware default |
| P6 gas-side manufacturer minimum OFF and anti-cycle | not known to SAT | verification plan line 118 | unknown-blocks-claim (installation record) |
| P6 compressor limits when the heat pump runs | not known to SAT | verification plan line 118 | **unknown-blocks-claim** |
| P7 hydraulic | nothing in SAT control; minimum flow of each generator, mandatory pump or bypass, buffer | firmware | unknown-blocks-claim (installation record) |
| P9 flow / return | ID25 / ID28 as gas | OT protocol lines 1372, 1880, 1883 | unknown-blocks-claim per unit (LL-011) |
| P9 which generator runs | not observable on one OT bus (`SATcontrol.ino:292-295`, comment). So the 180 s minimum ON and the hourly limit say nothing about compressor protection while the heat pump is the active generator | firmware | **unknown-blocks-claim** |
| Coordination of the two generators | out of SAT's scope today (`SATtypes.h:52`: "coordination = TASK-892") | firmware | unknown-blocks-claim |

## 5. Native protections and how SAT must not override them (AC3)

| Protection | What SAT does today | Source | Status |
|---|---|---|---|
| **DHW priority** | While `state.sat.bDhwActive` is set (ID0 slave status LB bit 2, `SATcontrol.ino:6414`; a simulated draw at `:6411-6412`), the control tick returns early and sends nothing for CH; the PWM ON clock is frozen and resumed after the draw; a 300 s guard follows the draw for the sustained switches | entries 95, 103, 108; `SATcontrol.ino:6421-6446`; thermo-nova `heating_control.py:268-274` (as recorded in the crosswalk R25, not re-opened) | decided. Whether a heat pump reports DHW in ID0 LB2 is **unknown** (unknown-blocks-claim for heat pumps) |
| **Defrost** | **not found** in the SAT firmware (grep `defrost` in `SAT*.ino`, `SATtypes.h`), in thermo-nova (grep in `custom_components/sat`) or in the OT protocol (grep) | grep, this session | unknown-blocks-claim |
| **Backup heater** (electric element of a heat pump, or the gas side of a hybrid) | **not found** in the SAT firmware, thermo-nova or the OT protocol | grep, this session | unknown-blocks-claim |
| **Anti-cycling** | SAT does not enforce one. `BS_ANTI_CYCLE_MIN_OFF_MS` = 180 s (`SATcontrol.ino:743`, `:801-802`) only labels the status `anti_cycling` when the flame is off with demand, as thermo-nova (`device/status.py:171-186`, `device/const.py:10`, opened). The appliance's own anti-cycle governs | firmware; thermo-nova | firmware default (status label only) |
| **Max supply temperature** | SAT caps every CS at its MaxTSet ceiling (P8 rows) | entries 51, 152, 155 | decided for SAT's own cap; the manufacturer maximum is unknown-blocks-claim |

**Rule for SAT (from WP7.3 and AC3, not a Sergeant D entry):** SAT's control requests are CS, CH and MM; it also makes the persistent writes listed in section 6.2 (`SH=`, `SW=`, `TP=`, `MI=`) and the thermostat override `TC=`. None of these may be used to defeat a native protection. Concretely, an acceptance run for a profile passes only when the record shows (a) no SAT request during DHW (`SATcontrol.ino:6421-6446` already returns early), (b) the appliance's own minimum OFF, defrost and backup-heater events logged from the appliance or a meter and none suppressed by a SAT request, (c) no competing room thermostat. Points (b) and (c) cannot be checked from SAT's telemetry today (no defrost or backup signal, section 5 table), so for heat pumps and hybrids they need P9/P10 instrumentation. How strict this gate is, is Open decision OD-3.

## 6. Hand-back and rollback (AC3)

### 6.1 What SAT sends when it lets go

`satDisable()` (`SATcontrol.ino:3359-3415`), reached from a user switch-off, a safety trip, an OpenTherm loss (`:3417-3482`, entries 39, 47, 79) and a missing outside temperature (`:3495-3566`, entries 56, 57, 121):

1. Resets mode, PWM, PID and the overshoot start; persists zeros for the PID file (unless `persistPid` is false, entry 115).
2. Sends `CS=0` through `satEnqueueOwnCS` (`:3397`): the control-setpoint override is cleared, so the thermostat's own setpoint reaches the boiler again (comment `:3388-3391`).
3. Sends `CH=1` (`:3402`, TASK-1346): restores the default CH-enable override flag.
4. Sends `MM=T` (`:3407`, TASK-1344): clears SAT's MM override on the PIC and on OT-Direct (comment `:3403-3406`).
5. During an OpenTherm or outside-temperature loss the three commands went out once already and are not repeated (`:3369-3374`, `:3392-3396`).

The same three commands are the boot hand-back when no valid restored state exists (`:3541-3543`, entry 121). On OT-Direct, `CS=0` clears the MsgID 1 override only (`OTDirect.ino:2778-2784`).

### 6.2 What is not rolled back (documented gap)

SAT also sends writes that stay in the appliance after `satDisable()`. None of them is reverted there (the function sends only CS, CH and MM, `:3397-3407`):

| Write | Where | Effect after disable |
|---|---|---|
| `SH=` (MsgID 57, max CH setpoint) | `satMaxTSetSendStep`, `SATcontrol.ino:382-398`; comment `:377-381`: "the boiler keeps the last user value" | the installer's MsgID 57 value is not restored; for a heat pump this is its maximum supply temperature |
| `SW=` (DHW setpoint) | `satDhwSetpointSync`, `SATcontrol.ino:6248-6260` | the last SAT DHW setpoint stays |
| `TP=11:12=` (Immergas quirk) | `SATcontrol.ino:6772-6777` | the transparent parameter keeps SAT's last value |
| `MI=500` (PIC only) | `SATcontrol.ino:1590-1600` | gateway setting, stays until changed |
| `TC=` (remote room setpoint override to the thermostat, with push setpoint on) | `SATcontrol.ino:643-666`; event-driven, entry 14 | `satDisable()` sends no `TC=0`; a thermostat that accepted SAT's override keeps it after hand-back (not tested on a thermostat) |

Rollback today therefore depends on P11: the installation record must hold the values from before SAT, and commissioning restores them by hand. Whether firmware should restore them is Open decision OD-2.

### 6.3 Not traced

- On an OT-Direct board in standalone master mode (no thermostat, `OTDirect.ino:988-1007`, `:2634-2636`), what MsgID 1 value the board sends after `CS=0` was **not traced**. The hand-back is only known to work as described where a thermostat is present.
- Whether a heat pump or hybrid accepts `MM=T` as "no limit" the way a boiler does was **not verified** (P5 heat-pump row).

## 7. Gas MM=0 evidence does not apply to heat pumps (AC3)

Gas evidence for MM=0 (minimum relative output, not burner off, AFS md:660-668; verification plan section 5 step 6) **is not evidence for a heat pump or a hybrid**. Reasons, all recorded:

- Entry 21: Elga Ace "when you send the MM=0 never starts heating", and "we don't know how they will behave at MM=0". SAT never sends MM=0 to a heat pump or hybrid (entries 21, 113, 188; `SATcontrol.ino:467-479`).
- Entry 113 sets the heat-pump MM to the configured maximum in and outside PWM; it is a deviation from thermo-nova, which sends 0 in PWM for every boiler (`heating_control.py:134-140`, `:515-518`, opened).
- The AFS gives no heat-pump rule for relative modulation (crosswalk R14 Clause; WP7.2).

So: a gas bench or field result never closes a heat-pump or hybrid gate (WP7 acceptance: "gas results do not close HP gates"). Each heat-pump model's interface behaviour for the configured MM is tested on its own (WP7.2).

## 8. R42: no measured claims from this profile

This profile supports **no** efficiency, comfort or compressor-start claim. The heat-pump numbers in entry 181 (round 12) and entry 184 (round 14) are simulation results that entry 181 itself calls "indicative". A claim needs the pre-registered matched comparison of PH-FIELD and LL-016 (verification plan sections 6 and LL-016 row; remediation plan R42), with real starts from P9 and delivered heat with its uncertainty.

## 9. LL-015 per-profile checks (what a run must show)

| Check | Gas | Heat pump | Hybrid |
|---|---|---|---|
| Automatic PWM entry through the sustained switch, full loop | required | required (entry 87, under review) | required |
| Duty boundaries at the profile's period | 1200 / 900 s | 1800 / 2400 s | 1800 / 2400 s |
| Hourly limit refuses the 4th or 5th start | required | must never refuse (entry 81) | required |
| MM per phase | 0 in PWM, max outside | configured max always | configured max always |
| Real restart delay and exits | measured from P9 | blocked until P9 compressor signal exists | blocked for the heat-pump side until P9 shows the active generator |
| Native protections observed (section 5) | DHW | DHW, defrost, backup, anti-cycle | DHW, defrost, backup, anti-cycle, generator switch-over |

Existing partial host tests (crosswalk section 10): `tests/test_sat_hp_pwm.py` (including `test_hybrid_uses_the_heat_pump_cycle`), `tests/test_sat_pwm_start_limit.py::test_hp_has_no_hourly_start_limit`, `tests/test_sat_pwm_override.py::test_heat_pump_and_hybrid_never_zero`. Not re-run for this document.

## 10. Open decisions

Each is an A/B question for the named person. None is resolved here. Q-1527a (heat-pump behaviour, entry 181) is already asked and is referenced, not re-posed.

- **OD-1 (Robert, then Sergeant D for any control change). Compressor limits in SAT.** A) the commissioning record only stores the manufacturer minimum ON/OFF and starts per hour, and the heat-pump claim stays blocked until a measurement shows compliance; SAT behaviour is unchanged. B) SAT gets settings for these limits and enforces them in PWM (a behaviour change, needs Sergeant D and depends on Q-1527a).
- **OD-2 (Robert; Sergeant D for the SAT hand-back sequence of entry 47). Rollback of persistent writes.** A) the firmware records the appliance's MsgID 57 (and DHW setpoint) before the first `SH=`/`SW=`, restores them in `satDisable()`, and clears a pushed `TC=` there. B) no firmware change; commissioning records the values (P11) and restores them by hand.
- **OD-3 (Robert). Native-protection gate for heat pumps and hybrids.** A) acceptance requires an external or appliance-side log of defrost, backup-heater and anti-cycle events (P9/P10). B) acceptance relies on SAT telemetry only and states that native protections were not observed.
- **OD-4 (Robert). Heat-pump compressor signal.** A) ID0 LB3 is accepted as compressor state only after a per-model bench check against a meter. B) a heat-pump run always needs an external compressor or power signal.
- **OD-5 (Robert). Hybrid evidence scope.** A) a hybrid run is accepted for the gas-side behaviour only, until the active generator is observable. B) hybrid runs require an external signal of the active generator before any acceptance.
- **OD-6 (Sergeant D). Per-model MM test for heat pumps.** A) every heat-pump model is bench-tested for the configured MM (and `MM=T` on hand-back) before it counts as supported. B) entry 21's generic rule ("keep the heat pump rule generic") is enough; no per-model test gate.
- **OD-7 (Robert). Gas system boiler profile.** A) keep one gas profile (no control difference exists today). B) split combi and system boiler profiles in the commissioning record (control unchanged).

## 11. Not verified

- No build, flash, bench or field run was done. Host tests were not re-run.
- thermo-nova `pwm.py:156`, `entry_data.py:220`, `const.py:167` (entry 81) and `heating_control.py:268-274` (DHW guard) are cited as recorded, not re-opened.
- The thermo-nova versus firmware differences named here (entry 113 heat-pump MM; heat-pump and hybrid period; no hybrid type in thermo-nova; the Immergas `TP=11:12` value, section 2.2 P4) still need the second reader required by CLAUDE.md before they are published.
- Whether a thermostat keeps a SAT `TC=` override after hand-back (section 6.2) was read from the code only.
- OT-Direct standalone behaviour after `CS=0`, and heat-pump handling of `MM=T`, were not traced (section 6.3).
