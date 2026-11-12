# OT-Direct vs PIC 6.8 parity audit (2026-11-12): new task drafts

> Copied into the repo by otgw-docs on 2026-11-12 from the coordinator's read-only audit, unchanged except for punctuation (no em dashes). G: = `OTGW-other-projects/otgw-6.8/gateway.asm` at 770aa45; O: = `src/OTGW-firmware/OTDirect.ino` at fa5790118. Filed tasks: see the parent task "PIC 6.8 parity audit (2026-11-12)".


Auditor: Fable 5.2 subagent, read-only, on Robert's order. Nothing built, run, flashed, committed or pushed. All findings from reading code, the PIC's own gpsim test scripts (read, not executed), the command reference and the spec. None reproduced on a bench.

Sources:
- PIC 6.8 (G): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-other-projects\otgw-6.8\gateway.asm (+ otgw-6.8\tests\*.stc). Repo OTGW-other-projects @ 770aa45.
- Firmware fa5790118 (O): wt-comms\src\OTGW-firmware\OTDirect.ino (+ OTDirecttypes.h, OTGW-Core.ino, settingStuff.ino, networkStuff.ino).
- Spec: wt-comms\docs\opentherm specification\OpenTherm-Protocol-Specification-v4.2.md
- Commands: OTGW-other-projects\OTGW-documentation\OTGW-Commands.md

6.6 -> 6.8: only gateway.asm differs: new RT= command with MsgID 24 handling, WRITE type for MsgID 16/24 when sent as alternative, upper clamp on the MsgID 125 answer. Everything else from the 6.6 audit (TASK-1795, docs/reviews/2026-11-11_otdirect-pic-parity-audit/) still applies.

## 1. Summary
Areas checked 7 (a-g); differences 60; already covered by a task/ADR 38; corrections to existing tasks 6; new task drafts 16 (8 high, 5 medium, 3 low).

## 2. Already covered
| Difference | Task | Status |
|---|---|---|
| MsgID 0 bits 1,2,4,5,6 rewritten in gateway mode | TASK-1805, ADR-229 | Done (default values wrong, see N3) |
| CH bit 0 only while CS held | TASK-1600, ADR-208 | Done |
| TT/TC answer-only, MsgID 9 answer | TASK-1733, ADR-223; TASK-1718 | Done; In Review |
| MsgID 100 flags (TT b'11', OH high byte) | TASK-1750 | To Do |
| Own frames only in alternative slots, slot content and order | TASK-1701, TASK-1707 | Done; In Review |
| MsgID 28 in a supported slot | TASK-1759, ADR-224 | In Progress |
| PM=28/25 never runs without an unsupported slot (PIC boards) | TASK-1767 | To Do |
| PR=A banner, PR=T format | TASK-1797 | Done |
| Own answers refused by the 50 ms slave gate | TASK-1798 | Done |
| PS=1 line format, port 25238, cadence | TASK-1800 | Done (one field still differs, N12) |
| MM=-, OT>=64, OT=- clear values | TASK-1799 | To Do |
| Mandatory IDs 1,3,14,17,25 blacklisted by 3 strikes | TASK-1801 | To Do |
| 5-6 s silence after failed MsgID 0; 30 s after thermostat loss | TASK-1802 | To Do |
| Runtime thermostat detection | TASK-1728 | To Do (parked) |
| OT=, SW=, SH= through the thermostat's own READ 27/56/57 | TASK-1803 | To Do |
| 16 C setback on thermostat loss; FS=/SB= meaning | TASK-1806 | To Do |
| Stale boiler cache in master mode | TASK-1807 | To Do |
| Empty cache slot answered Unknown-DataID | TASK-1808 | To Do |
| Thermostat TSet in master mode: expiry, TSet 0 | TASK-1811 | To Do |
| Master mode drops thermostat WRITEs of other IDs | TASK-1812 | To Do |
| TT=0 in master mode clears a room setpoint write | TASK-1751 | To Do |
| Mode switches race the loop task | TASK-1761 | To Do |
| MsgID 16 not always acknowledged; boiler Data-Invalid does not clear CS/SW/SH; TT/TC survives thermostat disconnect; CS below 8 expires; UI= without slot candidate; SR= value as proxy A; PR=O shows CS as room override; M=P/S/L ignored; bare error tokens and silent paths; GW=R without banner; GW=<other> echoed; no range checks on CS/C2/MM; MH/MW/M2 BV instead of OR; clamping SB/VS/SW/SH/MI; gap after boiler answer <100 ms; MsgID 0 starved by slow-poll burst; MsgID 5 not read on new fault; no Error 01..04 lines; parity-only frame validation; SR= answers WRITE with READ_ACK | TASK-1804 | To Do |

## 3. Corrections to existing tasks
- **TASK-1801 (incomplete):** covers only the 3-strike path. On the PIC, MandatoryID also undoes a manual UI= for IDs 0,1,3,14,17,25 on every request (G:3409-3416, "even (accidental) manual action cannot blacklist"; call sites G:2720, :2806, :2967, :3651). On OT-Direct UI=1, and also UI=0, goes into otUnknownIds (O:3712-3725) and otdIsAltSlot then takes the thermostat's frame for good (O:1893-1897, :2352-2359). After UI=0 the thermostat status never reaches the boiler. Add an AC for the UI= path including ID 0.
- **TASK-1799 (incomplete):** OT=+15.4 clears on OT-Direct (test `!isdigit && != '-'`, O:3141-3144). PIC accepts a leading + (GetFloat G:5118-5127) and documents that form (G:45).
- **TASK-1760 (premise wrong):** boot restore of the vent setpoint is a no-op in every mode, not only after auto-detect. MsgID 71 has no write entry (O:159-167; only R_ENTRY(71) at O:217) and no override slot (O:434-442), so updateWriteCache(71)/setOverride(71) at O:1018-1024 do nothing. See N1.
- **TASK-1750 (AC gap):** description says OH defaults on, ACs do not. PIC default IgnoreErr1 and OverrideHigh set (G:5459) -> `PR: T=11` (tests/serial-2.stc). OT-Direct otOverrideHB=0 (O:535) -> T=10.
- **TASK-1804 (MI item worded wrong):** OT-Direct does not clamp MI; it rejects <100 with OR and echoes unrounded (O:3813-3822). PIC rounds to 5 ms first, so MI=99 answers `MI: 100` (G:4577-4615, tests/interval-1.stc).
- **TASK-1805, TASK-1800 (Done):** each left a gap; filed as N3 and N12 instead of reopening.

## 4. New task drafts

### N1 (high) OT-Direct: VS= writes the ventilation setpoint in the wrong byte, once, and never overrides the thermostat's MsgID 71
- PIC: VS= stores the value (G:4241-4254); every thermostat MsgID 71 request becomes a WRITE, value in LB, HB 0 (MessageID71 G:3210-3217). tests/ventilation-1.stc: VS=15 -> R9047000F; VS=XX -> `VS: -`. Spec v4.2:1514 "LB: Control Setpoint".
- OT-Direct: `data = setpt << 8` (HB) (O:3168; boot O:1019; OTDirecttypes.h:124 comment "MsgID 71 HB"). Own decoder reads LB (OTGW-Core.ino:5425). Goes out once via the queue; no refresh, no override of the thermostat frame.
- Consequence: VS=50 sends 0x3200, read as 0 %. A thermostat writing MsgID 71 overrules the command.
- ACs: 1) host test on the real handler: VS=15 -> data 0x000F; red on current code. 2) With VS held, thermostat MsgID 71 READ/WRITE goes to the boiler as WRITE with the VS value; VS=T restores pass-through and replies `VS: -`. 3) Persisted setpoint applied after boot in gateway and master mode (closes TASK-1760). 4) Bench capture on the OT-Direct unit shows R.047.... with the value in LB.
- Not verified: behaviour of a real ventilation unit.

### N2 (high) OT-Direct: MsgID 99 data bytes are swapped (MW=, MH=, M2=, HW=P)
- PIC: HB = DHW mode (low nibble) + manual push (bit 4); LB = HC1 (low nibble), HC2 (high nibble). OperModeData G:3247-3250, SetOperModeDHW G:4482-4494, SetOperModeHC1 G:4466-4478, SetOperModeHC2 G:5250-5262. tests/dhw-5.stc HW=P -> R10631000; opermode-1.stc MH=2 -> A40630002. Spec v4.2:2249-2277 same.
- OT-Direct: `data99 = DHW | (CH1 | CH2<<4) << 8` (O:3208-3209, :3319, :3332, :3345): DHW and push in LB, heating in HB. Own decoder uses PIC layout (OTGW-Core.ino:3629-3634).
- Consequence: MW=4 tells the boiler "HC1 reduced"; HW=P sets HC2=Auto and never requests a push.
- ACs: 1) host test on real handlers: MW=4 -> 0x0400, MH=2 -> 0x0002, M2=3 -> 0x0030, HW=P -> bit 12; red on current code. 2) decoded MQTT topics for MsgID 99 match the command sent. 3) bench capture of the R frames.
- Not verified: boiler reaction.

### N3 (high) OT-Direct: CE= and H2= default to off where the PIC defaults to on; CL=0 clears instead of holding 0 %
- PIC: with CL= held, MsgID 0 bit 2 = !CoolModeOff; with C2= held, bit 4 = !CH2ModeOff (readstatus G:2720-2747). Both flags 0 after reset (RAM clear G:1178-1188) -> enable bit 1 until CE=0/H2=0 (G:4446-4452, G:5264-5270). Command ref "Initial value 1". ADR-229 lines 69-70 state the same.
- OT-Direct: coolEnable = otCoolingEnable default false (O:543, :917); ch2Enable = otMasterStatusFlags & 0x10 default 0 (O:64, :919, reset O:2680-2682).
- CL=0: PIC holds 0 % (G:4454-4464; only a bad value clears, reply `-`). OT-Direct clears the override (O:3294-3307).
- Consequence: C2=40 without H2=1 switches the CH2 enable bit off; on the PIC it is forced on. Same for cooling.
- ACs: 1) host test on real otdRelayedStatusHB path: after C2=40 only bit 4 = 1; after CL=50 only bit 2 = 1; H2=0/CE=0 clear them; red on current code. 2) CL=0 holds 0 %; CL=x clears and replies `CL: -`. 3) ADR-229 note/amendment naming the defaults.
- Not verified: master-mode status bits (ADR-066, separate design).

### N4 (high) OT-Direct: a CS=/C2= override expires after 60 s; the PIC keeps it at least 62.9 s
- PIC: expiry counts 121 Timer-1 overflows of 524.288 ms (G:157-158, :1104-1112, StartTimer G:4233-4239, CommandExpiry G:1623-1646): lives 62.9 to ~126 s after last command. tests/control-3.stc: CS at 2 s still active at 32 s, gone between 65 and 74 s. Command ref "repeated at least every minute".
- OT-Direct: OT_CSC2_EXPIRY_MS = 60000, checked once a second (O:499-501, :2421-2431).
- Consequence: a client refreshing exactly every 60 s never lapses on the PIC; on OT-Direct the override can drop just before the refresh so the thermostat TSet reaches the boiler briefly.
- ACs: 1) host test with fake clock on real expiry code: last CS= at t=0 still active at t=62 s; red on current code. 2) expiry not earlier than 63 s for CS and C2. 3) bench: CS= every 60 s for 15 min, no thermostat-valued MsgID 1 in the capture.
- Not verified: copy the full 63-126 s window or only the lower bound (Robert's call). NOTE: interacts with TASK-1811 and with SAT's 5 s CS resend (ADR-203/207, SAT exempt from CS expiry per TASK-1150).

### N5 (high) OT-Direct: at boot it sends its own MsgID 0 (CH on) and WRITEs 2, 124, 126 to the boiler in gateway and monitor mode
- PIC: own frames only without a thermostat (StandAloneMsg G:1797-1820, HandleRequest G:2388-2407), never in monitor mode; never sends MsgID 2, 124 or 126 (MessageTable G:1822-1842, AdminMessages G:3438-3462).
- OT-Direct: initOTDirect sends buildStatusRequest() (flags 0x01 CH on, DHW off), then WRITE 2 = 0x0100, READ 3, WRITE 124 = 0x0233, WRITE 126 = 0x0001, in every mode (O:1076-1149, :949-962). 2026-11-11 audit listed this as info (CD report C12), nothing filed.
- Consequence: after every reboot/OTA the boiler gets one status frame CH on/DHW off plus a foreign master config before the thermostat's first frame.
- ACs: 1) in gateway and monitor mode no gateway-made frame leaves before the first thermostat frame or before the no-thermostat decision. 2) host test on the real initOTDirect slice with mock master: zero requests in those modes; red on current code. 3) bench capture of a boot with a thermostat: first frame to the boiler is a T.
- Not verified: whether SAT or the boiler-presence flag (otBoilerCacheValid[3]) depends on the boot READ 3.

### N6 (high) OT-Direct: settings the PIC keeps in EEPROM are lost at reboot
- PIC (EEPROM, restored at boot): HW (SaveConfig G:4408, :4617-4620), SM, GW mode, TS (G:4746); SW, SH (G:4187-4188, :4214-4215; restored and re-sent at boot G:1218-1229); MI (G:4610-4611), SB (G:4160-4161), FS (G:5300), IT and OH (G:4959-4965); UI/KI (G:4900-4905, reinstalled G:1298-1324), AA/DA (G:4677, :4698). Tests: serial-5.stc (SM), thermostat-5.stc (SH), dhw-7.stc (SW across reset).
- OT-Direct: HW, SW, SH, UI, IT, OH RAM only (O:541, :3098-3126, :559-562, :534-535). SM, SB, MI, FS assigned to settings.otd.* without marking settings dirty (O:3263, :3771, :3817, :3827; dirty flag settingStuff.ino:61-64, set only by updateSetting at :804) -> survive only if something else saves later. ADR-229 line 134 claims SM= is persisted.
- Consequence: HW=0 and SW=/SH= silently fall back after reboot/OTA; SM=1 over MQTT usually lost.
- ACs: 1) host test on real handlers: SM=, SB=, MI=, FS= mark settings dirty; red on current code. 2) HW, SW, SH and the UI list restored after reboot and applied. 3) decision per item for IT, OH, AA/DA recorded. 4) bench: HW=0, reboot, PR=W answers W=0.
- Not verified: whether the "boot commands" setting (settingsHelp.h:46) is the intended substitute.

### N7 (high) OT-Direct: GW=0 or GW=1 wipes all command state, even when the mode does not change
- PIC: SetGatewayMode only switches comparators and saves the mode; current mode = no-op (G:4729-4747, :4749-4774). CS, MM, HW, BW, SR=, UI=, TT/TC stay.
- OT-Direct: every GW= -> setOTDirectMode -> applyOTDirectMode -> resetTransientState (O:3368-3416, :2735-2737, :2656-2728); no unchanged-mode check.
- Consequence: a client sending GW=1 (settings help gives `PS=0;GW=1` as boot-command example) drops CS, HW=0, MM, SR=, UI=.
- ACs: 1) GW=1 in gateway mode changes no state; host test red on current code. 2) decide and record (ADR or task note) what a real mode change keeps (PIC parity = everything except what ADR-223 flushes from the queue). 3) host test for GW=0 then GW=1 with HW=0 and an SR= entry set.
- Not verified: which resets master and loopback mode need for their own reasons.

### N8 (high) OT-Direct: SC= writes MsgID 20 to the boiler and does not answer the thermostat's clock request
- PIC: SC= only stores the time (G:4331-4388); used to answer the thermostat's MsgID 20 READ/WRITE for one expiry window (MessageID20 G:3048-3056); after that a boiler Unknown/Data-Invalid becomes Data-Invalid for READ, WRITE_ACK for WRITE (G:2694-2706). tests/thermostat-2.stc: A50147532, AC0147532, later A5014892A, A60140000. Nothing written to the boiler.
- OT-Direct: enqueueWriteCommand(20,...) WRITEs the boiler (O:3181-3191); its "override" does not exist (O:2433-2437); the thermostat gets the boiler's answer (O:1548-1582).
- Consequence: with "send time" on the firmware sends SC= every minute (networkStuff.ino:1447): boiler clock written every minute, thermostat never sees the time.
- ACs: 1) SC= puts no frame on the bus; host test red on current code. 2) while SC valid, thermostat READ/WRITE of MsgID 20 answered with the SC value; after expiry Data-Invalid/WRITE_ACK as the PIC, also once blacklisted. 3) bench capture with a thermostat that reads MsgID 20, or the slave simulator.
- Not verified: expiry window length (see N4).

### N9 (medium) OT-Direct: the PIC 6.8 command RT= (room temperature for the boiler) is missing
- PIC 6.8: RT=<temp> stores a fake room temperature, 0 clears (G:3849-3856, :4300-4309, :372-374, :1288). Thermostat MsgID 24 gets that value (MessageID24 G:3061-3068); as alternative (AA=24) it goes out as WRITE (G:2670-2673). tests/heatpump-1.stc RT=25.41 -> R10181969. Reply `RT: 25.41`.
- OT-Direct: no RT branch -> NG (O:3911-3915); MsgID 24 has no override slot (O:434-442); MQTT room temperature path writes MsgID 24 as a gateway frame (O:2269-2273).
- ACs: 1) RT=25.41 replies `RT: 25.41`; a thermostat WRITE 24 reaches the boiler with 0x1969; RT=0 stops it; host test red on current code. 2) thermostat gets WRITE_ACK with its own data. 3) OTD_PIC_COMPAT_VERSION (O:533) goes to 6.8 once this and the MsgID 125 part of N10 have landed.
- Not verified: for a thermostat frame with parity bit 1 the PIC appears to answer Unknown-DataID (setbyte1 G:2680-2686, SendAltResponse G:2467-2477); read from code only. OT-Direct chose not to copy this for 56/57 (O:1482-1488).

### N10 (medium) OT-Direct: answers the PIC always gives the thermostat for MsgID 2, 6, 125 and an unsupported 56/57
- PIC: MsgID 2 always acknowledged with the thermostat's own member ID and only the Smart Power bit (G:2837-2844; thermostat-3.stc AD002010B). MsgID 6 always READ_ACK with transfer-enable bits 0 and 1 set (G:2900-2918). MsgID 125: below 3.0 or no answer -> 3.0; new in 6.8, >=5.0 -> 4/51 (G:3327-3344; otversion-1/2/4.stc AC07D0300, A407D0433). MsgID 56/57 answered Unknown-DataID -> acknowledged with SW/SH value or upper bound, default 80/90 (G:3149-3161, :3185-3197, :262-265). These answers also come once blacklisted (fake answer via TreatMessage G:2467-2477).
- OT-Direct: replyToThermostat rewrites only MsgID 0, 9, 100 and overridden writes (O:1548-1582, :1490-1507). After 3 strikes the thermostat gets Unknown-DataID from the slot (O:1893-1897).
- Consequence: a thermostat behind a boiler without these IDs sees "not supported" and may stop offering DHW/max-CH setpoints.
- ACs: 1) host tests on the real reply path, one per ID, with the frames from the PIC test scripts; red on current code. 2) same answers after 3 Unknown strikes. 3) bench with slave simulator where possible.
- Not verified: acknowledging Smart Power commits the slave interface to medium/high power; the library tracks it (OpenTherm.cpp:446, :540-544) but the hardware side was not checked. Decide MsgID 2 separately if in doubt.

### N11 (high) OT-Direct: the DHW push (HW=P) differs from the PIC
- PIC: HW=P leaves stored HW state alone (G:4410-4412); any other HW= aborts the push (G:4398-4399); DHW enable forced only when the boiler does not take the push (MsgID 99 unsupported/Unknown/acked without push bit, G:3229-3238, :3273-3279); once started the push ends when the boiler's DHW mode bit clears, before that 8 status answers with flame off end it (G:2783-2797). tests/dhw-5.stc: with boiler support and HW=0 status stays R00000000.
- OT-Direct: HW=P sets otDHWOverride=1 (O:3202-3203); push end sets it to auto (O:1733, :1745) -> a previous HW=0 is gone; DHW enable always forced during a push (O:910-915); 30 s wall-clock timeout also cuts a started push (O:1728-1735); only HW=0 aborts (O:3218-3225); thermostat's own MsgID 99 not handled.
- ACs: 1) HW=0, HW=P, push ends: PR=W answers W=0 during and after, bit 1 = 0 afterwards; host test red on current code. 2) a started push is not cut at 30 s. 3) DHW enable forced only when the boiler refuses or lacks the push. 4) bench capture of one push.
- Not verified: on the PIC MH=/MW=/M2= also set dhwpushpending (PushOperModes G:3281-3288), which looks like it can start a spoofed push after a plain mode command; read from code only, check before copying. Depends on N2.

### N12 (medium) OT-Direct: PS=1 does not show the OT= value in the MsgID 27 field
- PIC: OT= writes the value straight into the summary store (StoreOutTemp G:4276-4287, slot G:568-569, index 15 = MsgID 27).
- OT-Direct: store filled only from frames (T WRITE, B READ_ACK, R WRITE without thermostat; O:2097-2106). With a thermostat connected OT=7.5 never reaches field 16 (O:3141-3153). Rest of the line matches the PIC print routines.
- ACs: 1) host test: thermostat connected, OT=7.5 then PS=1 -> field 16 = 7.50; red on current code. 2) line stays byte-identical to tests/summary-1.stc for the same frames.
- Not verified: for an ID with an SR= entry the PIC still stores the boiler's answer while OT-Direct sends no boiler frame (TASK-1707 note b), so that field can differ too.

### N13 (medium) OT-Direct: PM= does not read the TSP, fault-history or string entries, and refuses IDs above 127
- PIC: after PM=10 (also 12, 88, 90, 105, 107) the answer's count starts reads of every entry of the next ID (G:3364-3378, :3389-3397, index G:3464-3470); PM=93/94/95 reads every string character (G:3399-3407); range 0-255 (G:5026-5037). tests/tsp-1.stc, str-1.stc. Command ref describes this.
- OT-Direct: one READ with data 0, or a schedule entry forced due; IDs > 127 -> BV (O:3610-3632).
- ACs: 1) host test: PM=10 against a mock boiler with 3 TSPs produces the R frames of tsp-1.stc; red on current code. 2) PM=93 reads all characters. 3) PM=131 accepted.
- Not verified: interaction with the PM slot rule of TASK-1572 and TASK-1767.

### N14 (medium) OT-Direct: PR= reports that are not the PIC's text
| Letter | PIC | OT-Direct |
|---|---|---|
| P | Low power / Medium power / High power (G:2261-2268, :5499-5502) | fixed P=M (O:3508-3511); library knows the level (OpenTherm.cpp:540-544) |
| R | D by default; S, R, C, I (G:2270-2280) | fixed R=I (= iSense) (O:3512-3515); FT= ignored |
| O | lowercase t/c until thermostat accepted; n while clearing (G:2201-2226) | always uppercase (O:3446-3475) |
| C | C=4 MHz (G:5506-5507) | C=240 (O:3520-3523) |
| B | HH:MM DD-MM-YYYY (tools/build.tcl:83) | __DATE__ (O:3516-3519) |
| L | default FXOMPC (G:5462-5468) | default XXXXXX (O:537) |
- ACs: 1) host test per letter, byte-exact; red on current code. 2) PR=P follows the real power level. 3) decision for C, B and L recorded (PIC text or agreed OT-Direct value).
- Not verified: which clients parse P, R, C, B; pyotgw only stores them.

### N15 (low) OT-Direct: success and clear replies that differ from the PIC
- SC=: PIC two-digit hours `SC: 09:05/3` (G:4374-4388, PrintDecimal G:2086-2101); OT-Direct %d (O:3188).
- SR=: PIC `18:1/230`, `70:0/14` (G:4856-4859); OT-Direct echoes input (O:3693).
- FT=: PIC prints D, S, R, C or I (G:4705-4727); OT-Direct echoes any character (O:3901-3905).
- VS= clear: PIC `VS: -` (G:4253-4254); OT-Direct echoes input (O:3158-3160).
- CL= clear: see N3.
- ACs: 1) host test per reply, byte-exact; red on current code. 2) one line per item in the final summary.
- Not verified: client impact; none found.

### N16 (low) OT-Direct: no "Thermostat disconnected"/"Thermostat connected" lines and no power-level lines
- PIC prints these on the serial line (G:1761-1764, :1776-1778, :5490-5491; power changes G:1532-1551; tests/standalone-1.stc, thermostat-3.stc).
- OT-Direct sends only a WebSocket status and a debug line (O:2807-2843). The firmware's event table knows the PIC tokens (OTGW-Core.ino:5528-5532) but never gets them on OT-Direct.
- ACs: 1) on thermostat loss and return the PIC lines go to port 25238 and processOT. 2) host test on the real checkThermostatTimeout.
- Not verified: timing depends on TASK-1728 and TASK-1802.

## 5. Intentional differences
- Extra modes (master, bypass, loopback) and their GW=/PR=M letters: ADR-064, TASK-438, TASK-1716.
- Protocol-level thermostat detection, master mode as virtual boiler: ADR-066.
- Schedule table, 800 ms status, 3-strike constants: ADR-068.
- Alternative slots from the whole schedule, thermostat answered at once, SR=/UI= answered locally without a boiler frame, AA=/DA= as enable/disable: TASK-1707 notes (a)-(d), code comment O:3568-3572. No ADR.
- MsgID 28 in a supported slot answered from cache: ADR-224.
- External CS/CH/MM/C2/H2/TT/TC refused with NG while SAT is on; SAT exempt from CS expiry: ADR-179, TASK-1150.
- TT/TC release tolerance 0.25 C vs PIC 0.125: ADR-223.
- VS= persisted across reboots (PIC keeps it in RAM): archived TASK-584.
- CC=, RM=, CM= are OT-Direct extensions.
- LED, GPIO (HOME, AWAY, DHW block, DS18x20), VR=, DP=, on/off-thermostat short circuit, hardware fail-safe after watchdog reset, Celcia/iSense workarounds: no hardware equivalent; recorded only in code comment O:3869-3872, no ADR.

## 6. Not checked
- No bench or capture; PIC test scripts read as expected values, not executed (no gpsim).
- backlog CLI search slow; coverage checked by grepping backlog/tasks and backlog/archive/tasks in wt-comms (no backlog/completed folder there).
- interface.asm, selfprog.asm, ds1820.asm, diagnose.asm identical 6.6/6.8, not compared.
- PIC boilercom rule (5 s without boiler-side frame clears output, G:1559-1564) still unclear.
- Smart-power bit-level behaviour of the vendored library and TASK-1804 error-detection details not re-audited.
- Older task line numbers refer to f64aba399; this audit cites fa5790118. TASK-1804 items not all re-verified against current code.
