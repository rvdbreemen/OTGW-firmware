# TASK-1795 Area B: the PIC serial command interface compared with OT-Direct

Date: 2026-11-11. Branch `dev` at f64aba399. This is a read-only audit: no repository file was changed, nothing was built, and no device was touched.

## Sources

| Source | Path | Role |
|---|---|---|
| PIC firmware 6.6 (Schelte Bron) | `other-projects/otgw-6.6/gateway.asm` | Ground truth for command syntax and replies |
| OT-Direct handler | `src/OTGW-firmware/OTDirect.ino` `handleOTDirectCommand()` :2810-3760 | Implementation under audit |
| Command entry | `OTGW-Core.ino:3849-3856` (`addCommandToQueue` sends straight to `handleOTDirectCommand` when there is no PIC), `:4386-4390` (`sendPICSerial`); port 25238 lines arrive via `OTDirect.ino:720-776` and `sendPICSerial` | Command paths |
| pyotgw 2.2.3 (the engine behind HA `opentherm_gw`, which pins `pyotgw==2.2.3`) | `other-projects/pyotgw-master/pyotgw/` | Client impact |
| HA `opentherm_gw` | `other-projects/core-dev/homeassistant/components/opentherm_gw/` | Client impact |
| OTmonitor 6.6 | `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl` | Client impact |
| Prior audit | `docs/reviews/2026-04-27_otdirect-pic-parity-audit/REVIEW.md` | Built on; its fixed items are listed under Refuted |
| ADRs | ADR-064 (modes), ADR-179 (SAT refuses external CV commands with NG), ADR-223 (TT/TC answer-only), ADR-045/046 (PS=1 parsing; they contain no OT-Direct decision, checked by grep) | Recorded differences |

I opened every `file:line` cited below in this session. "Fact" means I read it in the code. "Opinion" marks severity and impact estimates. I ran no client against a device. The pyotgw findings come from reading the code, plus a one-line Python check of the string index (`'OpenTherm Gateway OTGW32'[18] == 'O'`, `'1/1'[1] == '/'`).

## How the PIC replies, which every row depends on (fact)

- **Prefix first.** `SerialCommand` (gateway.asm:3683-3703) checks two things first: the length is at least 4, and character 3 is `=`. If either check fails it returns a bare `SE`. If both pass, it prints `XX: ` and only then dispatches. The main loop then prints the handler's error string (if any) and a newline (:1425-1432). So every PIC error after a well-formed header reads `XX: NG`, `XX: SE`, `XX: BV`, `XX: OR`, `XX: NS` or `XX: NF`. Only a malformed header gives a bare `SE`, and only a UART overrun gives a bare `OE` (:3683-3684, :3752).
- **Unknown command.** The XOR jump table (:3704-3750) and its sub-dispatchers return `CommandNG`, so the PIC prints `XX: NG`.
- **OT-Direct.** Every error goes out as a bare token through `otDirectBridgeProcessStatus()` (OTDirect.ino:709-713), for example `BV` and not `CH: BV`. A command shorter than 4 characters or without `=` gets no reply at all: `return false` at :2813 and :2819.
- **Client tolerance.** pyotgw accepts both forms: a bare token at commandprocessor.py:52-58, a prefixed token at :65-70. The firmware's own `handlePICStatusToken()` (OTGW-Core.ino:5563-5577) matches only bare tokens.

## Command table

Column key. **Sup**: Y means supported, P means partially, N means not supported. **Val**: whether OT-Direct validates the argument the same way as the PIC. **Reply**: whether the reply line is the same. A reply marked "same" means the same success line; error replies differ for every command, see row X1.

| # | Cmd | PIC (gateway.asm) | OTD (OTDirect.ino) | Sup | Validation | Reply | Verdict |
|---|---|---|---|---|---|---|---|
| X1 | (all) error framing | Prefixed `XX: <code>` :3695-3703, :1429-1431 | Bare token :709-713 | P | n/a | different | different, low (pyotgw tolerates both) |
| X2 | (all) len<4 or no `=` | Bare `SE` :3688-3694 | Silent `return false` :2813, :2819 | P | n/a | none | different, low (BC7) |
| X3 | unknown code | `XX: NG` (table :3715-3750) | Bare `NG` :3754-3757 | Y | same | bare | different, low (X1) |
| X4 | `OE` overrun | :3752-3753 | never emitted (no UART receiver) | n/a | n/a | n/a | not applicable |
| X5 | SAT refusal | n/a | Bare `NG` for an external CS/CH/MM/C2/H2/TT/TC while SAT is on :2827-2831 | n/a | n/a | `NG` | documented by ADR-179 |
| 1 | TT | `SetTempSetPoint` :4058, `GetPosFloatArg` :5072-5085 (a `-` reaches `GetDecimal` and gives SE); `SetSetPoint` :4065; reply `PrintSigned` :4236 | :2887-2896 `atof`; 0 clears | Y | different: a negative value is accepted (the comment at :2867-2871 claims PIC parity, which is wrong); `TT=abc` clears the override where the PIC gives SE | same (`TT: 20.50`) | semantics documented by ADR-223; validation is BC6/BC10 |
| 2 | TC | `SetContSetPoint` :4053 | :2872-2881 | Y | as TT | same | as TT |
| 3 | SB | `SetSetBack` :4124-4131: 0 or negative gives SE | :3612-3617 clamps to 1..30 | Y | different (silent clamp) | same format | different, low (BC12) |
| 4 | CS | `SetCtrlSetpoint` :4187-4200, `GetPercentArg` :5057-5070 (>100 gives OR, negative gives SE) | :2838-2860 `atof`; `floatToF88` clamps to 127 | Y | different: no 0..100 limit, negative and non-numeric accepted | same | different, medium (BC5) |
| 5 | CH | `SetCentralHeat` :4372, `CheckBoolean` :4041-4051 (wrong length gives SE) | :3083-3088 `checkBoolean` :2358 | Y | near: a wrong length gives BV, not SE | same | different, cosmetic |
| 6 | VS | `SetVentSetpoint` :4210-4223: >100 gives OR, trailing characters give SE, a clear replies `-` | :3004-3026 `constrain` 0..100; a clear echoes the input | Y | different (silent clamp) | clear reply differs | different, low (BC12) |
| 7 | MM | `SetMaxModLevel` :4752-4765: integer 0..100, >100 gives OR, `40.5` gives SE, non-numeric (including `-`) clears and replies `-` | :2975-2986: the clear test `!isdigit && != '-'` sends `MM=-` to the write branch, where `atof("-")=0` writes MsgID 14 = 0 and replies `MM: 0`; `-5` and `150` accepted | Y | different | different for `-` | **bug candidate BC9**; MM=0 fixed (R1); range is BC5 |
| 8 | SH | `SetHeatingTemp` :4160-4185: BV outside the boiler's MsgID 49 bounds, persisted in EEPROM | :2960-2973, no bounds | Y | different | same | different, low (BC12) |
| 9 | SW | `SetHotWaterTemp` :4133-4158: BV outside the MsgID 48 bounds | :2945-2958, no bounds | Y | different | same | different, low (BC12) |
| 10 | SC | `SetClock` :4289-4346: hh 0..23 and mm 0..59 (OR), `:` and `/` required (SE), dow 0..7 (BV) | :3028-3038 `siscanf >= 2`, no range checks; a parse failure sends **no reply** | P | different | same on success (`14:05/6`) | different, low (BC7) |
| 11 | VR | `SetVoltageRef` :4383-4402: `0`-`9` only, length 4 | :3732-3736 `atoi` stored, no hardware | P (no-op) | none | same for 0-9 | different, cosmetic |
| 12 | HW | `SetHotWater` :4348-4370: length 4 (SE); `P`, `1`, `0`, else auto; reply `PrintHotWater` | :3045-3081, same mapping, no length check | Y | near | same | same (length check aside) |
| 13 | CL | `SetCoolLevel` :4412-4422 `GetPercentArg`: 0 is a valid 0 %, a bad float clears and replies `-` | :3141-3154: 0 clears | Y | different: 0 clears (the same pattern as the old MM=0 bug) | same | different, low |
| 14 | CE | `SetCoolEnable` :4404 | :3128-3139 | Y | same | same | same (OTD drives bit 2 directly where the PIC has CoolModeOff, outside this area) |
| 15 | RR | `RemoteRequest` :5245-5250: GetDecimal, >255 gives OR | :3201-3209 `atoi` cast to uint8, so 300 wraps to 44 | Y | different (no range check) | same | different, cosmetic |
| 16 | MW | `SetOperModeDHW` :4440-4452: >15 gives OR, non-numeric gives SE | :3175-3186: >15 gives BV, non-numeric becomes 0 | Y | different code | same | different, low (BC11) |
| 17 | MH | `SetOperModeHC1` :4424-4436 | :3161-3173 | Y | as MW | same | as MW |
| 18 | M2 | `SetOperModeHC2` :5208-5220 | :3188-3199 | Y | as MW | same | as MW |
| 19 | LA-LF | `SerialCmdLED` :3941-3994: length 4, `A`-`Z` else BV | :3725-3730 stores any character | P (no-op) | none | same | different, cosmetic |
| 20 | GA/GB | `SerialCmdGPIO` :3996-4039: decimal 0..8 else BV, trailing characters give SE | :3718-3723 stores any character | P (no-op) | none | same | different, cosmetic |
| 21 | TS | `SetSensorFunc` :4740-4750: `O` or `R`, else BV | :3738-3742 any character | P | none | same | different, cosmetic |
| 22 | PR | `PrintSetting` :2150-2185: length 4 (SE); letters A-Z, invalid letter gives `PR: BV` | :3265-3385; `default:` sends **no reply** :3381-3383 | P | different | see the PR table | bug candidates BC1, BC3, BC4 |
| 23 | PS | `PrintSummary` :2341-2358 (length 4, `0`/`1` else BV); one line per request `SummaryReport` :1645-1657; `PrintMessage` hides T/B :1921-1922 | :3393-3405 (any non-`1` turns it off); `emitSummaryLine` :1908-2004 after **every** MsgID 0 :1665 | P | different | `PS: 1` same; the summary line differs | bug candidate BC2 |
| 24 | AA | `SetAlternative` :4617-4636: 0 gives OR, a 32-slot EEPROM ring, NS when full | :3416-3432 re-enables an `otSchedule` entry; 0 or >127 gives BV; NF if absent | P | different | same on success | deviation recorded in a code comment :3411-3415, no ADR |
| 25 | DA | `DelAlternative` :4638-4661: 0 gives OR, NF | :3436-3451: 0 gives BV, NF | P | near | same | as AA |
| 26 | SR | `SetResponse` :4771-4817: ID 0 gives BV, `:` required; reply `PrintByte` `:` `PrintBytes` gives `SR: 18:1/230` | :3487-3537: byte syntax plus a legacy hex form; ID 0 accepted; reply echoes the input `SR: 18:1,230` | Y | near | different (`,` versus `/`) | byte syntax fixed (R3); the reply format is a residual difference, low |
| 27 | CR | `ClearResponse` :4819-4831: 0 gives BV, NF | :3539-3552 | Y | near | same | same |
| 28 | UI | `UnknownDataID` :4848-4865: bit 7 gives BV, EEPROM bitmap | :3555-3568: >127 gives BV; a RAM list that can fill (NS) | Y | near | same | different, low (not persisted) |
| 29 | KI | `KnownDataID` :4867-4884 | :3570-3586 | Y | near | same | same |
| 30 | RS | `ResetCounter` :4942-4982: exact length 6 else SE; consumer `ResetCommand` :3474-3490 (bit n means MsgID 116+n) | :3633-3654: mapping 116-123; invalid gives BV | Y | different code (BV versus SE) | same | ID mapping same (R7); code cosmetic |
| 31 | GW | `SetGatewayMode` :4687-4705: `0`/`1`, `R` resets (:4734-4738, then the boot banner), else BV; `GW: P` means "change pending" (:4707-4710) | :3215-3259: `0`/`M`, `1`, `2`/`S`, `P` (bypass), `L`, `R` (no banner); **any other character is accepted and echoed** | P | different | `GW: R` without a banner | modes from ADR-064 and letters from TASK-438 (no ADR); BC8 |
| 32 | IT | `IgnoreError1` :4911-4925 | :3619-3624 | Y | same | same | same (RAM only) |
| 33 | OH | `SetOverrideHigh` :4927-4933 | :3626-3631 | Y | same | same | same (the default differs: PIC 1 at :5417, OTD 0 at :535) |
| 34 | FT | `SetTStatModel` :4663-4685; reply `PrintRemeha` (D/S/R/C/I) | :3744-3748 echoes the character | P (no-op) | none | different | different, cosmetic |
| 35 | DP | `SetDebugPtr` :4269-4287 (hex) | :3750-3752 no-op echo | P | none | different | not meaningful on OTD |
| 36 | OT | `SetOutsideT` :4225-4236: ≥64 °C or non-numeric clears and replies `-` (`-` alone also reaches `ClrOutsideT` through the GetDecimal SE) | :2988-3000: `OT=99` writes 99 °C to MsgID 27 and to `OTcurrentSystemState.Toutside`; `OT=-` writes 0 °C | Y | different | different for clear values | **bug candidate BC13** |
| 37 | C2 | `SetCtrlSetpt2` :5230-5243 `GetPercentArg` | :2917-2935, no range check | Y | as CS | same (`C2: 50.00`; the prefix comes before `SerialCmdCH2` at :3711) | as CS (BC5) |
| 38 | H2 | `SetCentralHeat2` :5222-5228 (CH2ModeOff) | :3090-3099 (bit 4 directly) | Y | same | same | same reply; flag semantics are outside this area |
| 39 | BS | `SetFakeSetpoint` :4258-4267 `GetPercentArg` | :2903-2915 | Y | no 0..100 limit | same | different, low |
| 40 | BW | `SetDHWBlock` :4527-4533 | :3115-3126 | Y | same | same | same |
| 41 | FS | `SetFailSafety` :5252-5259 | :3667-3673 | Y | same | same | same |
| 42 | MI | `SetInterval` :4535-4573 (rounds to 5 ms; OR, SE); `PrintInterval` :2234-2240 | :3656-3665: 100..1275, otherwise bare OR; echoes unrounded | Y | near | `MI=123`: PIC `125`, OTD `123` | units fixed (R5); rounding is cosmetic |
| 43 | PM | `SetPrioMessage` :4984-4995 (0..255) | :3453-3475: >127 gives BV | Y | near | same | same |
| 44 | SM | `SetSummerMode` :4519-4525 | :3101-3113 | Y | same | same | same |
| 45 | TP | `SlaveParameter` :4454-4517 | :3675-3710 | Y | same (SE/BV/OR at the same places) | same | same |
| E1 | CC | `'C'^'C'=0` goes to `SerialCmd00`, which gives `CC: NG` | :2937-2943 | OTD only | n/a | n/a | OTD extension |
| E2 | RM / CM | `RM` goes to `SerialCmd1F` (NG); `CM` goes to `SerialCmdLED` (NG) | :3593-3609 | OTD only | n/a | n/a | OTD extension |

### PR= letters

PIC dispatch is at gateway.asm:2160-2185. The OTD switch is at OTDirect.ino:3268-3384. pyotgw requests A B C D G I L M O P Q R S T V W (vars.py:352-391) at every connect.

| Letter | PIC reply | OTD reply | Verdict |
|---|---|---|---|
| A | `A=OpenTherm Gateway 6.6` (:5434-5435) | `A=OpenTherm Gateway OTGW32` :3271 | **different, high (BC1)** |
| B | `B=` + build timestamp (:5462-5463) | `__DATE__`, e.g. `Nov 11 2026` :3356 | different, cosmetic |
| C | `C=4 MHz` (:5464-5465) | `C=240` :3360 | different, cosmetic |
| D | `O`/`R` :2306-2310 | `otTempSensor` :3344 | same if TS= was valid |
| E | `-` or a sensor value :2312-2320 | **no reply** (`default:`) | not implemented, low (BC7) |
| F | LVP 0/1 :2322-2339 | **no reply** | not implemented, cosmetic |
| G | 2 hex digits :2294-2296 | `otGpioFunctions` (default `00`) | same |
| I | 2 booleans :2287-2292 | `00` | same (no GPIO) |
| L | 6 characters, default `FXOMPC` :5420-5426 | default `XXXXXX` :537 | different default, cosmetic |
| M | `G`/`M` :2224-2228 | `G`/`M`/`P`/`S`/`L` :3274-3285 | ADR-064 modes; OTmonitor `PR: M=[GM]` (otmonitor.tcl:635) ignores P/S/L |
| N | ms with a final 0/5 :2230-2240 | ms :3374 | same (R5; the comment at :3372 is stale) |
| O | `N`; `n` (clearing); `T`/`C` (accepted) or `t`/`c` (pending) + remote setpoint only :2197-2222 | T/C uppercase only; **falls back to the CS flow setpoint as `C<val>`** :3297-3311 | **different, medium (BC3)** |
| P | `Low power`/`Medium power`/`High power` :2257-2264 | `P=M` :3348 | different, cosmetic |
| Q | W/B/P/S/C/O/U/L/E :2242-2247 | shim P/C/W/B/E :3363-3369 | same vocabulary |
| R | `D`/`S`/`R`/`C`/`I` (I = iSense) :2266-2276 | always `R=I` ("internal") :3352 | different: OTD claims an iSense thermostat; cosmetic |
| S | float :2298-2304 | `otSetbackTemp` | same |
| T | 2 booleans, no separator `T=11` :2278-2285 | `T=1/1` :3340 | **different, medium (BC4)** |
| V | 1 hex digit :2192-2195 | `%d` | same for 0-9 |
| W | `0`/`1`/`A` :2249-2255 | same :3321-3326 | same |
| H J K U X Y Z / non-letter | `PR: BV` :2157-2158, :2167-2185 | no reply | different, low (BC7) |

### PS=1 summary line (row 23 in detail)

The PIC prints 34 fields in `StoreProcTable` order (:1978-2012, `SUMMARYFIELDS=34` :3572), one line per `PS=1`. The OT-Direct `emitSummaryLine()` is at :1908-2004.

| Aspect | PIC | OTD | Verdict |
|---|---|---|---|
| Byte-pair fields 15/48/49 | `PrintBytes` `HB/LB` :2062-2066 | `"%u,%u"` :1947-1948, :1974-1978 | different: **36 commas, not 33** |
| Signed fields (27 Toutside) | `PrintSigned`, no `+` :2039-2041 | `+` prefix for values ≥0 :1928 | different |
| Field 24 (MsgID 71) | stored ID 71 low byte | `s.RelativeVentilation`, which is the MsgID 77 field (OTGW-Core.h:450, OTGW-Core.ino:5189) | wrong field (latent behind the comma bug) |
| Field 25 (MsgID 77) | stored ID 77 low byte | `s.FanSpeed` (MsgID 35, OTGW-Core.h:408) | wrong field (latent) |
| Counters 116-123 order | :2004-2011 | :1991-1998 | same |
| Cadence | one line per `PS=1` (`SummaryDone` clears the request :1656) | after every MsgID 0 response :1665-1667 | different |
| Reaches port 25238 | yes (UART) | **no**: `processOT` only :2003; `processOT` never writes `OTGWstream` (the only writers are OTGW-Core.ino:553 for PIC raw and :4652 for replay) | different |
| T/B frames hidden | yes, `PrintMessage` returns while HideReports :1921-1922 | no, `bridgeFrameToParser` writes the frame to 25238 unconditionally :789 | different |
| Own parser accepts it | yes (33 commas) | no, `processPSSummary` returns on any count other than 24 or 33 (OTGW-Core.ino:5236-5239) | **the firmware rejects its own summary** |

## Bug candidates that survived the skeptic pass

Severities are opinion. The mechanisms are fact, read in code; none was reproduced on a device.

**BC1. PR=A banner breaks Home Assistant `opentherm_gw` (pyotgw) on OT-Direct. Severity: high.**
- **Fact.** OTD answers `PR: A=OpenTherm Gateway OTGW32` (OTDirect.ino:3271). pyotgw `get_reports()` stores `ver = "OpenTherm Gateway OTGW32"` and then, on every loop pass, evaluates `int(ver[18]) < 5` before it checks `value == "D"` (pyotgw.py:177-183). `ver[18]` is `'O'`, so `int('O')` raises `ValueError`.
- **Fact.** That line sits outside the try/except of `_wait_for_cmd` (:712-723). `connect()` calls `get_reports()` unconditionally unless `skip_init` is set (:53-57). HA calls `connect(device)` with the default (config_flow.py:72, `__init__.py:119`). The config flow catches only `TimeoutError`, `ConnectionError` and `SerialException` (config_flow.py:78-84).
- **Scenario.** A user adds the OpenTherm Gateway integration with `socket://<otgw32-ip>:25238` and port 25238 enabled. The config flow fails with an unhandled ValueError ("Unknown error"), and an existing entry fails setup on every restart.
- **Skeptic.** Users of the firmware's own MQTT discovery are not affected. The pinned pyotgw is the one HA uses (setup.py `version="2.2.3"`, manifest.json `pyotgw==2.2.3`). The task-146 note "PASS, tools match on OpenTherm Gateway" checked only the substring. Not refuted.
- **Also.** The banner contains the board name, which public channels must not name. That is not a protocol issue, but a fix would remove it.

**BC2. The PS=1 summary is unusable on OT-Direct. Severity: medium.**
- **Facts** (see the PS=1 table above):
  1. The line has 36 commas, so the firmware's own `processPSSummary` drops it silently.
  2. The line never reaches port 25238.
  3. T/B frames keep flowing to 25238.
  4. It repeats after every MsgID 0.
  5. Fields 71 and 77 are taken from the wrong state fields, and Toutside carries a `+`.
- **Scenario (pyotgw, once BC1 is fixed).** `get_status()` sends `PS=1`, reads `PS: 1`, and takes the next non-frame line in the command queue as the summary (commandprocessor.py:72-75). T/B frames go to the message processor instead (protocol.py:111-115). Two outcomes follow:
  - **(a) Timeout.** With no other line arriving, the call times out after 3 s and returns None. pyotgw then skips its follow-up `PS=0` (pyotgw.py:259-263), so the OTD board stays with `otSummaryMode` and `otHideReports` set until something sends `PS=0`.
  - **(b) Crash.** Every command reply goes to 25238 through `synthesizeResponse` (:2337-2343), so while SAT is on, a CS reply such as `CS: 45.00` can arrive and be taken as the summary. It splits into 1 field, so `process_statusfields_v4` (pyotgw.py:769-781) raises `IndexError` on `device_status[1]`. That is outside any try/except, so it is a second uncaught crash path.
  - **Not traced.** The effect on our own MQTT under (a): frames are enqueued with `suppressOutput` (:808), which skips the raw-frame output path.
- **Scenario (OTmonitor or a manual client).** `PS=1` replies `PS: 1` and then no summary ever arrives.
- **Skeptic.** No test covers it: `emitSummaryLine` is stubbed to `{}` in tests/test_otd_gateway_alt_slots.py:218. ADR-045 and ADR-046 contain no OT-Direct decision. Not refuted.

**BC3. PR=O reports the CS flow setpoint as a constant room-setpoint override. Severity: medium.**
- **Fact.** With no TT/TC active and a CS override active, OTD answers `PR: O=C<flow setpoint>` (OTDirect.ino:3297-3311). The PIC `PrintOverride` reports only the remote room-setpoint state (`override` flags plus setpoint1/2, gateway.asm:2197-2222) and never the control setpoint.
- **Scenario.** While SAT is active it always holds CS (ADR-203 and ADR-207 resend it). pyotgw `get_reports()` then sets `room_setpoint_ovrd = 55.0` and `setpoint override mode = C` (pyotgw.py:205-209, :243-248), so the HA climate entity shows a room override of about 55 °C. The same happens after any external `CS=`.
- **Skeptic.** The behaviour was intended in task-146 notes, but no ADR records it, and ADR-223 covers only TT/TC. Not refuted.
- **Scope.** pyotgw and HA only. The firmware's own `handlePRresponse` acts only on registers M and E (OTGW-Core.ino:1315-1365), so MQTT-discovery users see nothing.
- **Related (fact).** The same function ignores `M=P`, `M=S` and `M=L` with only a debug line (:1346-1348). So on OTD the MQTT `gateway_mode` is not updated in bypass, master or loopback mode. Low.

**BC4. PR=T uses a `/` separator. Severity: medium (masked by BC1).**
- **Fact.** OTD answers `T=%d/%d` (OTDirect.ino:3340). The PIC prints two digits with no separator (gateway.asm:2278-2285).
- **Fact.** pyotgw does `int(tweaks[1])` (pyotgw.py:224-230), and on `"1/1"` that is `int('/')`, a ValueError. This is the same uncaught path as BC1, so it becomes the next crash once BC1 is fixed. OTmonitor's patterns `PR: T=[01]` and `PR: T=[01][01]` (otmonitor.tcl:628-634) match neither, so IT and OH stay unknown.
- **Skeptic.** This refutes the 2026-04-27 REVIEW.md claim that "no external client dependency was found".

**BC5. No range check on CS, C2 and MM; negative values accepted. Severity: low to medium.**
- **Fact.** The PIC `GetPercentArg` rejects values above 100 with `OR` and negative values with `SE` (gateway.asm:5057-5085). MM accepts integers 0..100 only (:4752-4763).
- **Fact.** OTD passes `atof` straight to `floatToF88`, which clamps to -40..127 (OTDirect.ino:2839-2850, :2918-2925, :2980). So `CS=150` writes TSet 127 °C, `CS=-5` writes -5 °C, and `MM=150` writes 127 %.
- **Skeptic.** HA and SAT send values in range, and most boilers clamp to MaxTSet. Only a typo or a hostile 25238 client hits this, which keeps the severity low. It still survives, because an out-of-range OpenTherm write is sent to the boiler.

**BC6. A non-numeric argument clears an override where the PIC gives `SE`. Severity: low.**
- **Fact.** For TT, TC, CS, C2, SW, SH, BS and CL, `atof("abc")` returns 0, which runs the clear branch. Examples: `TT: 0.00` clears the remote override, and `CS=x` releases the CS override. The PIC returns `XX: SE` and keeps its state (`GetDecimal` gives `CarrySyntaxErr`, :5003-5007, :5145-5146).

**BC7. Silent paths: commands that never get a reply. Severity: low.**
- **Fact.** These inputs produce no reply at all:
  - a malformed header (:2813, :2819);
  - PR with an unknown letter, or `E` or `F` (:3381-3383);
  - an `SC=` parse failure (:3030, no else branch);
  - a full ring for CS, C2, SW, SH, CC, MM, OT or VS. These return false with no token (for example :2856, :2931).
- **Fact.** The PIC always answers.
- **Scenario.** A client waits for its timeout. pyotgw's `set_clock` waits 3 s, and OTmonitor waits for its own command timeout.
- **Skeptic.** pyotgw does not request PR=E or PR=F, which keeps the severity low.

**BC8. GW=R emits no banner, and GW=<anything else> is accepted. Severity: low.**
- **Fact.** pyotgw `set_mode("R")` waits for `OpenTherm Gateway \d+` (commandprocessor.py:60-63). OTD sends only `GW: R` (:3245-3258), so the call times out.
- **Fact.** For `GW=X`, no branch of :3219-3256 matches, but :3257-3258 still echoes `GW: X`. The PIC gives `GW: BV` (:4734-4737).
- **Note.** OTD's `GW: P` means bypass, while for the PIC `GW: P` means "mode change pending". OTmonitor answers it with `PR=M` (otmonitor.tcl:644-646), which is harmless.

**BC9. `MM=-` writes 0 % maximum modulation instead of clearing. Severity: medium to high.**
- **Fact.** OTD's clear test is `!isdigit(value[0]) && value[0] != '-'` (OTDirect.ino:2976). For `MM=-` that test is false, so the write branch runs. `atof("-")` is 0, so it writes MsgID 14 = 0 through `enqueueWriteCommand`. That also puts the value in the write cache and override table, so the scheduler keeps refreshing it (:2312-2329). The reply is `MM: 0`.
- **Fact.** The PIC's `GetDecimalArg` fails on `-`, which branches to `ClrMaxModLevel` and replies `MM: -` (gateway.asm:4752-4754, :4764-4765).
- **Fact.** HA sends exactly this command:
  - `cleanup()` calls `set_max_relative_mod("-")` on every unload and stop (`__init__.py:112-115`);
  - the `set_max_modulation` service maps level -1 to `"-"` (services.py:264-271, services.yaml:147-152).
- **Fact.** pyotgw then stores `int("0")` as the boiler max modulation (pyotgw.py:552-559).
- **Scenario.** A user stops HA, or calls "clear max modulation". The OTGW32 then caps the boiler at 0 % maximum modulation, the boiler runs at minimum power, and this lasts until reboot, `MM=<non-digit>` or GW=R. Today BC1 stops pyotgw from connecting, which masks this. Any 25238 or MQTT `MM=-` hits it directly.
- **Skeptic.** SAT's own release uses `MM=T` (docs/api/MQTT.md:350), which does clear. While SAT is on, an external `MM=` is refused (ADR-179). A grep of `src/`, `tests/`, `backlog/` and `docs/adr/` found no recorded decision that `-` means 0. TASK-436 fixed only the numeric-0 case. Not refuted.

**BC13. OT= never clears through the PIC clear values, so HA's "clear" writes 99 °C. Severity: medium.**
- **Fact.** The PIC clears the outside temperature for any value of 64 °C or more, and for non-numeric input, replying `OT: -` (gateway.asm:4225-4234). pyotgw documents "above 64 (suggestion: 99) to clear" (pyotgw.py:124-141). HA's `set_outside_temperature` allows up to 99 (services.py:137, services.yaml:154-166).
- **Fact.** OTD writes any numeric value to MsgID 27 and to `OTcurrentSystemState.Toutside` (OTDirect.ino:2988-2999). `OT=-` writes 0 °C, as in BC9.
- **Scenario.** HA calls `set_outside_temperature` with 99 to clear. The OTGW32 then reports Toutside = 99 °C to the boiler, which affects a boiler with weather-dependent control, and to the firmware's state. pyotgw stores 99.0.
- **Not checked.** Whether SAT reads this state value as an outside-temperature source.
- **Skeptic.** Masked for HA by BC1 today. Not refuted.

**BC10. The negative TT/TC comment is wrong. Severity: low.**
- **Fact.** The comment at OTDirect.ino:2867-2871 says negative values match PIC behaviour. The PIC `GetPosFloatArg` sends a leading `-` into `GetDecimal`, which returns SE (gateway.asm:5072-5086).
- **Consequence.** OTD sets a negative remote override. In gateway mode under ADR-223 that is answer-only, so the impact is small.

**BC11. MH, MW and M2 return the wrong error code. Severity: low.**
- **Fact.** For a value above 15, OTD answers BV (:3163, :3177, :3190); the PIC answers OR (:4427-4429).
- **Fact.** For a non-numeric value, OTD's `atoi` gives 0, which is accepted and sent as an MsgID 99 write. The PIC answers SE.

**BC12. Silent clamping instead of rejection. Severity: low.**
- **Fact.** OTD clamps where the PIC rejects:
  - `SB` is clamped to 1..30; the PIC gives SE for 0 or below.
  - `VS` is clamped to 0..100; the PIC gives OR.
  - `SW` and `SH` have no check against the boiler's MsgID 48/49 bounds; the PIC gives BV.
  - `MI` is echoed unrounded.

## Refuted, or fixed since 2026-04-27

| # | Claim | Why it is dropped |
|---|---|---|
| R1 | MM=0 clears the override (REVIEW.md F1) | Fixed by TASK-436: numeric 0 now writes 0 (OTDirect.ino:2976-2985); only non-numeric input clears, as in the PIC (:4752-4754). |
| R2 | GW=0 means bypass (F3) | Fixed by TASK-438: `0` and `M` mean monitor (:3219-3221), bypass moved to `P`. |
| R3 | SR= rejects the PIC byte syntax (F6) | Fixed by TASK-441 (:3493-3519). Only the reply format remains a difference (row 26). |
| R4 | TC= writes MsgID 1 (F8) | Fixed: answer-only, ADR-223 (:2872-2881, :2398-2430). |
| R5 | MI and PR=N report centiseconds (F5) | Fixed by TASK-440 (:3374, :3663). Only the stale comment at :3372 remains. |
| R6 | C2 and H2 replies lack the `XX:` prefix (pyotgw comment commandprocessor.py:117-120) | In 6.6 the prefix is printed at :3695-3703, before the `SerialCmdCH2` branch at :3711. PIC and OTD both send `C2: 50.00`, so they are the same. |
| R7 | The RS counter mapping differs | `ResetCommand` (:3474-3490) maps bit n to MsgID 116+n. Traced through `ResetCounter` (:4942-4976), HBS=116, HPS=117, WPS=118, WBS=119, HBH=120, HPH=121, WPH=122, WBH=123, which matches OTD :3638-3645. |
| R8 | The PR= reply never reaches port 25238 | `otDirectBridgeProcessPRResponse` writes to the bridge (:2350-2355, TASK-454). |
| R9 | The `NG` on an external CS= while SAT is on is a parity bug | Recorded decision ADR-179 (:127, :152, :212). |
| R10 | OTD lacks some PIC commands | All 45 PIC commands in the header (:45-145) and the dispatch table (:3715-3750, :5193-5259) have a branch in OTD. WD is debug builds only. |

## Outside Area B (one line each, not assessed)

- The PIC exempts a CS or C2 below 8 °C from heartbeat expiry (gateway.asm:1624-1635); OTD does not.
- HW=A forces bit 1 on (:3075) where the PIC passes the thermostat's bit.
- CH2, CE and H2 drive status bits directly where the PIC uses ModeOff flags.
- OH defaults to 0 on OTD (:535) and to 1 on the PIC (:5417).

## Counts

Main table: 45 PIC commands, 5 cross-cutting rows and 2 OTD-only rows.
- Same (success reply and validation): 13. These are CE, CR, IT, OH, KI, H2, BW, FS, PM, SM, TP, HW and RS; HW and RS count as same apart from their length or error-code details.
- Different, bug candidate: 7. These are PR (BC1, BC3, BC4, BC7), PS (BC2), MM (BC9, BC5), OT (BC13), CS and C2 (BC5), and GW (BC8).
- Different, low or cosmetic: 21. These are SB, CH, VS, SH, SW, SC, VR, CL, RR, MW, MH, M2, LA-LF, GA/GB, TS, SR, UI, FT, DP, BS and MI.
- Documented or recorded deviation: 4. TT and TC are covered by ADR-223, apart from validation. AA and DA are recorded in a code comment only, with no ADR. Cross-cutting rows add the GW modes (ADR-064) and the SAT NG (ADR-179).
- Not implemented: PR=E, PR=F, and the PR invalid-letter BV.
- Check: 13 + 7 + 21 + 4 = 45.

Bug candidates that survived: 13 (BC1-BC13). By severity, which is opinion:
- high: BC1;
- medium to high: BC9;
- medium: BC2, BC3, BC4, BC13;
- low to medium: BC5;
- low: BC6, BC7, BC8, BC10, BC11, BC12.

Refuted or fixed: 10 (R1-R10).
