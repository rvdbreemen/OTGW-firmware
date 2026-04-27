# OTDirect PIC-Parity Command Fixture

## Purpose

This file documents the expected command-level behaviour of the OTDirect subsystem
(`src/OTGW-firmware/OTDirect.ino`) in relation to the PIC firmware it emulates.

OTDirect replaces the physical PIC OpenTherm Gateway chip on OTGW32/ESP32 boards.
It must accept the same serial command set and produce compatible responses so that
existing host software (e.g. otmonitor, Home Assistant integrations) keeps working
without modification.

This fixture is **not** a runtime test runner. It is a curated, source-cited parity
table that:

- documents the ground-truth PIC behaviour per command,
- captures the OTDirect-expected behaviour and the rationale for any divergence,
- provides concrete input/expected-output pairs that can later be wired into a host-
  testable harness or a loopback integration test, and
- gives future contributors a single place to check before modifying command dispatch.

The accompanying `check_otdirect_fixture.py` script validates that every table row
contains all required columns (static format check; no firmware execution needed).

## Scope

Commands audited: `MM`, `SW` / `TW`, `GW`, `MI`, `PR=N`, `SR`, `CS`, `C2`, `TC`, `TT`.

Out of scope: LED configuration, GPIO, EEPROM management, OT loopback test frames,
BLE/SAT subsystem, and any command not present in `OTDirect.ino`.

## Limitations

- The PIC firmware (`gateway.asm`) is assembly for the PIC16F1847; it cannot be run
  on a host. All PIC behaviour documented here is read from the assembly source.
- Timer-driven expiry (CS/C2 heartbeat) requires millis()-injection or a real 60 s
  wait for end-to-end verification; the fixture describes the logic without executing
  it.
- OTDirect does not implement every PIC command (e.g. LED, GPIO). Those are silently
  returned as NG.

## Ground-Truth Sources

| Label | Path | Notes |
|---|---|---|
| `gateway.asm` | `other-projects/otgw-6.6/gateway.asm` | PIC firmware assembly; primary ground truth |
| `MANUAL` | `docs/MANUAL.md` | Firmware user manual |
| `API-CH09` | `docs/manuals/en/ch09-api-reference.md` | API reference chapter |
| `OT-v4.2` | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md` | OpenTherm message ID reference |
| `gui.tcl` | `other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl` | otmonitor GUI command examples |
| `otmonitor.tcl` | `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl` | otmonitor CS heartbeat / commandexpired logic |

---

## Command Tables

Column definitions:

- **Command**: The serial command token (e.g. `MM=40`).
- **PIC behaviour**: What `gateway.asm` does (with line-number citations).
- **OTDirect-expected behaviour**: What `OTDirect.ino` must do to be PIC-compatible.
- **MsgID**: OpenTherm Data-ID used on the bus.
- **OT data shape**: OpenTherm payload encoding per `OT-v4.2`.
- **Test fixture**: `input -> expected response` pair (one per row; `->` separates serial input from serial echo).
- **Notes**: Divergences, open issues, or cross-references.

---

### MM= — Maximum Relative Modulation Level

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `MM=0` | `SetMaxModLevel` (gateway.asm:4752): calls `GetDecimalArg`; value 0 passes range check (0..100); stores 0 in `MaxModLevel` and clears `SysMaxModLevel` flag — writes 0 to bus as a valid level. **Does NOT clear the override.** Clearing requires a non-numeric argument. | Must enqueue `WRITE_DATA MsgID 14` with data `0x0000` (f8.8 = 0%). Must NOT treat value 0 as a clear operation. | 14 (`Max-rel-mod-level-setting`) | f8.8 (OT-v4.2 row 285): integer part in HB, fraction in LB. 0% = `0x0000`. | `MM=0` -> `MM: 0` | TASK-436 audit finding. gateway.asm:4764 (`ClrMaxModLevel`) is only reached by `btfss` carry-set path (non-numeric arg), not by value==0. |
| `MM=40` | Value 40 passes range (0..100), stores 40 in `MaxModLevel`. PIC multiplies by 256 when building OT frame (f8.8 encoding). | Enqueue `WRITE_DATA MsgID 14`, data `0x2800` (40 * 256 = 0x2800). Echo `MM: 40`. | 14 | f8.8: `0x2800` | `MM=40` -> `MM: 40` | Nominal happy path. |
| `MM=101` | Returns `OutOfRange` (gateway.asm:4760 `sublw 100; skpc; retlw OutOfRange`). | Return `OR` (Out of Range). | 14 | — | `MM=101` -> `OR` | Range check 0..100 inclusive. |
| `MM=A` (non-numeric) | Carry set by `GetDecimalArg`; jumps to `ClrMaxModLevel` (gateway.asm:4764), sets `SysMaxModLevel` flag (disables user override). | Clear MsgID 14 write override; echo the argument back. | 14 | — | `MM=A` -> `MM: A` | Non-numeric value = clear. |

---

### SW= — DHW Setpoint / TW= — Rejected

Citation: `gateway.asm` `SetHotWaterTemp` (line 4133), `constant MSG_DHWSETPOINT=56` (line 253), `otmonitor.vfs/gui.tcl` line 1138: `sercmd SW=$dhwsetpoint`.

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `SW=60.0` | `SetHotWaterTemp` (gateway.asm:4133) parses float, clamps to `dhwsetpointmin`..`dhwsetpointmax` (defaults 20..80), stores in `dhwsetpoint1/2`, schedules `MSG_DHWSETPOINT` (56) `WRITE_DATA` frame. | Enqueue `WRITE_DATA MsgID 56`, data = f8.8 encoding of 60.0 = `0x3C00`. Echo `SW: 60.00`. | 56 (`TdhwSet`) | f8.8 (OT-v4.2 row 252): 60.0 = `0x3C00`. | `SW=60.0` -> `SW: 60.00` | otmonitor gui.tcl:1138 uses `sercmd SW=$dhwsetpoint` to set DHW temperature. |
| `SW=0` | `GetFloatArg` returns zero/carry; jumps to `HotWaterReset` (gateway.asm:4149) which clears `initflags,INITSW`. No OT frame is sent; DHW setpoint reverts to boiler default. | Clear MsgID 56 write override. Echo `SW: 0.00`. | 56 | — | `SW=0` -> `SW: 0.00` | Value 0 = clear override in OTDirect (mirrors PIC reset path). |
| `TW=60.0` | `TW=` is not a PIC command. There is no `SerialCmdXX` dispatch entry for it (`gateway.asm` SerialCmdTable dispatches T-prefixed commands only to `TC` via SerialCmd17, line 3742). A non-matching command falls through to `CommandNG`. | Unknown command: return `NG`. | — | — | `TW=60.0` -> `NG` | TASK-437 audit finding. `TW=` has no PIC mapping. OTDirect must not register or implement it. Confirmed absent from `OTDirect.ino` command dispatch. |

---

### GW= — Gateway Mode Control

Citation: `gateway.asm` `SetGatewayMode` (line 4687), `otmonitor.vfs/gui.tcl` lines 1181-1183.

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `GW=0` | `SetGatewayMode` (gateway.asm:4687): `CheckBoolean` returns 0 (monitor mode), clears `MonModeSwitch`, calls `SetMonitorMode`. In monitor mode the comparator connects input directly to output; OTGW does not inject messages. Echoes `GW: 0`. | Switch to OTD_MODE_MONITOR (transparent pass-through). Echo `GW: 0`. | — | — | `GW=0` -> `GW: 0` | otmonitor gui.tcl:1183 sends `GW=0` to enter monitor mode. |
| `GW=1` | `CheckBoolean` returns 1, sets `MonModeSwitch`, calls `SetMonitorMode`. Gateway mode: OTGW intercepts and modifies OT frames as configured. Echoes `GW: 1`. | Switch to OTD_MODE_GATEWAY. Echo `GW: 1`. | — | — | `GW=1` -> `GW: 1` | otmonitor gui.tcl:1181 sends `GW=1` to enter gateway mode. |
| `GW=R` | `SetGatewayMode` (gateway.asm:4735): subtracts 'R', equals zero so calls PIC `reset` instruction. Full hardware reset of the PIC. | Execute `resetTransientState()`, restart OT master/slave interfaces (software reset equivalent; no MCU reboot). Echo `GW: R`. | — | — | `GW=R` -> `GW: R` | PIC does a hard hardware reset. OTDirect does a soft state reset. The echo `GW: R` mirrors PIC behaviour from gateway.asm:5470 reset-reason table. |
| `GW=P` | No PIC equivalent; `SetGatewayMode` would return `BadValue` for 'P'. | OTDirect-only extension: engage hardware bypass relay (thermostat directly to boiler). Returns `NG` if board has no bypass relay or setting disabled. Echo `GW: P` on success. | — | — | `GW=P` (no relay) -> `NG` | TASK-438 audit finding. `GW=P` is an OTDirect alias deliberately not shared by PIC so that `GW=0` from existing PIC tools cannot accidentally engage the relay. |
| `GW=X` (unknown) | `SetGatewayMode` (gateway.asm:4697): `CheckBoolean` and `sublw 'R'` both fail; returns `BadValue`. | Return `BV` (Bad Value). | — | — | `GW=X` -> `BV` | gateway.asm returns `BadValue` for any value not 0/1/R. |

---

### MI= — Message Interval / PR=N — Report Interval

Citation: `gateway.asm` `SetInterval` (line 4535), `PrintInterval` (line 2234), `PrintSettingN` (line 2230). Timer 6 fires every 5 ms; `MsgInterval` holds count of 5 ms ticks; default `MessageInterval` EEPROM = 200 (200 * 5 ms = 1000 ms).

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `MI=500` | `SetInterval` (gateway.asm:4535): strips last digit, divides remaining by 5 (rounded), stores tick count. 500 ms / 5 ms = 100 ticks. `PrintInterval` (gateway.asm:2234) reconstructs ms: `(ticks >> 1)` hundreds + '0' or '5' tens = "500". Returns `MI: 500`. | Parse as milliseconds (100..1275 valid). Store as `otMinIntervalMs = 500`. Echo `MI: 500`. | — | — | `MI=500` -> `MI: 500` | TASK-440 audit finding. PIC `SetInterval` input and `PrintInterval` output are both in milliseconds. Internally PIC stores 5 ms ticks but the serial interface shows ms on both sides. |
| `MI=99` | Value 99 ms: minimum is 100 ms (`movlw 100/5; subwf temp,W; skpc; retlw OutOfRange`). Returns `OutOfRange`. | Return `OR`. | — | — | `MI=99` -> `OR` | Range: 100..1275 ms. |
| `MI=1275` | 1275 / 5 = 255 ticks; fits in one byte. Maximum accepted. | Accept; echo `MI: 1275`. | — | — | `MI=1275` -> `MI: 1275` | Upper bound: 255 ticks * 5 ms. |
| `PR=N` | `PrintSettingN` (gateway.asm:2230): reads `MsgInterval` (5 ms ticks), reconstructs ms via `PrintInterval`. Output format `N: <ms>`. | Echo `PR: N=<ms>` where ms matches last `MI=` value (or default). | — | — | (after `MI=500`) `PR=N` -> `PR: N=500` | PIC echoes the stored interval in milliseconds, not in ticks or centiseconds. OTDirect must match this unit. |

---

### SR= — Set Response Override (Date/Time Sync)

Citation: `gateway.asm` `SetResponse` (line 4788), `otmonitor.vfs/otmonitor.tcl` lines 1978 and 1986. OpenTherm v4.2: MsgID 21 = Date (u8 Month, u8 Day), MsgID 22 = Year (u16).

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `SR=21:4,27` | `SetResponse` (gateway.asm:4788): parses `<DataID>:<highByte>,<lowByte>` as decimal bytes. DataID 21, HB = 4, LB = 27. Stores in response table: data = (4 << 8) OR 27 = 0x041B. Echoes `SR: 21:4,27`. | Parse decimal byte-pair; store `msgId=21`, `data=0x041B`. Echo `SR: 21:4,27`. | 21 (Date) | u8 Month (HB=4=April), u8 Day (LB=27) | `SR=21:4,27` -> `SR: 21:4,27` | TASK-441 audit finding. otmonitor.tcl:1978 sends `SR=21:month,day` for date sync (e.g. April 27 = `SR=21:4,27`). |
| `SR=22:7,234` | DataID 22, HB = 7, LB = 234. data = (7 << 8) OR 234 = 0x07EA = 2026. Echoes `SR: 22:7,234`. | Parse; store `msgId=22`, `data=0x07EA`. Echo `SR: 22:7,234`. | 22 (Year) | u16 combined: 2026 | `SR=22:7,234` -> `SR: 22:7,234` | otmonitor.tcl:1986 sends `SR=22:yearHi,yearLo` (year 2026 = HB 7, LB 234 since 7*256+234 = 2026). |
| `SR=21:4` (single byte) | `SetResponse`: no comma found; treats value after colon as single byte for LB (HB = 0). DataID 21, data = `0x0004`. | Parse as single decimal byte for LB; HB = 0. Store `data=0x0004`. Echo `SR: 21:4`. | 21 | u8 LB=4, HB=0 | `SR=21:4` -> `SR: 21:4` | PIC one-byte form: `OneByteResponse` path (gateway.asm:4804 `movfw float2; movwi -32[FSR1]` with float1=0). |
| `SR=0:1,2` | `SetResponse` (gateway.asm:4792): `skpnz; retlw BadValue` — DataID 0 is rejected. | Return `BV`. | — | — | `SR=0:1,2` -> `BV` | DataID 0 is always invalid per PIC. |
| `SR=128:1,2` | DataID > 127 is checked by the caller (`btfsc float1,7; retlw BadValue`). Returns `BadValue`. | Return `BV`. | — | — | `SR=128:1,2` -> `BV` | MsgID must be in range 1..127. |
| `SR=21` (no colon) | Colon is mandatory; `xorlw ':'; skpz; retlw SyntaxError`. Returns `SyntaxError`. | Return `BV` (Bad Value — no colon). | — | — | `SR=21` -> `BV` | Colon separator is required. |

---

### CS= and C2= — Control Setpoint Expiry (Heartbeat)

Citation: `gateway.asm` `CommandExpiry` (line 1619), `heartbeatflags` (line 493-496), `StartTimer` (line 4205). `otmonitor.vfs/otmonitor.tcl`: `commandexpired` proc (line 1997), heartbeat timer arm at line 578 (`after 62915 commandexpired`).

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `CS=10.0` | `SetCtrlSetpoint` (gateway.asm:4187): stores setpoint in `controlsetpt1/2`, sets `HeartbeatCS` flag, calls `StartTimer` (line 4205). Timer is `minutetimer` counting down from 120 (120 * 0.5 s = 60 s). On expiry `CommandExpiry` clears the override by setting `SysCtrlSetpoint`. | Store f8.8 value; enqueue `WRITE_DATA MsgID 1`; record `otCSLastCommandMs = millis()`. After ~60 s without repeat, clear MsgID 1 override. Echo `CS: 10.00`. | 1 (`TSet`) | f8.8: 10.0 = `0x0A00` | `CS=10.0` -> `CS: 10.00` | TASK-442 audit finding. PIC uses a 60 s window; repeat command before expiry resets timer. |
| `CS=10.0` (repeat within 60 s) | `SetCtrlSetpoint` calls `StartTimer`; timer is reloaded only if already expired (`btfss Expired; return`). If not expired the flag `HeartbeatCS` is set which prevents expiry at the next minute boundary. | Refresh `otCSLastCommandMs = millis()`. Override stays active. Echo `CS: 10.00`. | 1 | f8.8 | `CS=10.0` (repeat) -> `CS: 10.00` | Heartbeat model: each repeat within the window extends the deadline by another 60 s from the new timestamp. |
| `CS=0` | `iorwf float1,W; skpnz; bra ClrCtrlSetPoint` — zero value calls `ClrCtrlSetPoint` which sets `SysCtrlSetpoint` (disables user override). Timer is stopped. otmonitor.tcl:2004 sends `CS=0` explicitly via `commandexpired` proc. | Clear MsgID 1 write override. Set `otCSLastCommandMs = 0` (expiry sentinel). Echo `CS: 0.00`. | 1 | — | `CS=0` -> `CS: 0.00` | Explicit clear: no expiry timer needed. |
| `C2=10.0` | Parallel to CS: `controlsetpt3/4`, `HeartbeatC2`, `SysCH2Setpoint`. `CommandExpiryJ1` (gateway.asm:1630) checks `HeartbeatC2`. | Same expiry logic; MsgID 8 (TsetCH2). Echo `C2: 10.00`. | 8 (`TsetCH2`) | f8.8 | `C2=10.0` -> `C2: 10.00` | TASK-442. Parallel CH2 setpoint; same 60 s heartbeat window as CS. |
| `C2=0` | Clears `SysCH2Setpoint`. | Clear MsgID 8 write override; `otC2LastCommandMs = 0`. Echo `C2: 0.00`. | 8 | — | `C2=0` -> `C2: 0.00` | Explicit clear. |
| (60 s without CS repeat) | `CommandExpiry` fires (timer counts to 0); `btfss HeartbeatCS; skpnz; bra CommandExpiryJ1` — if CS < 8 and no heartbeat, `bsf SysCtrlSetpoint` clears override. | After `OT_CSC2_EXPIRY_MS` (60000 ms) from last `CS=` without repeat, `clearWriteOverride(1)`. | 1 | — | (time-injectable check: `otCSLastCommandMs` set 61 s ago) | Verification requires millis() injection or hardware wait; see How to Verify. |

---

### TC=, TT=, CS= — Setpoint Mapping Distinction

Citation: `gateway.asm` header comment lines 46-52, `SerialCmd17` (line 3893) dispatching `TC` to `SetContSetPoint` (line 4053), `SetContSetPoint` calling `SetSetPoint` with `b'01'` (manual-only remote override). `SetCtrlSetpoint` (line 4187) dispatching CS to `controlsetpt1/2` and `MSG_CTRLSETPT=1`. OpenTherm v4.2: MsgID 1 = `Tset` (control/flow setpoint), MsgID 9 = `TrOverride` (remote override room setpoint), MsgID 16 = `TrSet` (room setpoint).

| Command | PIC behaviour | OTDirect-expected behaviour | MsgID | OT data shape | Test fixture | Notes |
|---|---|---|---|---|---|---|
| `CS=15.0` | `SetCtrlSetpoint`: writes `controlsetpt1/2`; PIC injects WRITE_DATA for `MSG_CTRLSETPT` (MsgID 1, `Tset`) on every thermostat request. Flow/CH water control setpoint. | Enqueue `WRITE_DATA MsgID 1`; data = f8.8 of 15.0 = `0x0F00`. Echo `CS: 15.00`. | 1 (`Tset`) | f8.8: `0x0F00` | `CS=15.0` -> `CS: 15.00` | CS = boiler flow setpoint override. Uses MSG_CTRLSETPT (1). |
| `TC=+16.0` | `SetContSetPoint` (gateway.asm:4053): calls `GetPosFloatArg`; passes `b'01'` (manual-only) to `SetSetPoint`. `SetSetPoint` stores `setpoint1/2`, sets `OverrideReq`. PIC injects override into MsgID 9 (`TrOverride`) frames from thermostat. | Route to MsgID 16 (`TrSet`, room setpoint override). OTDirect does not implement the full PIC remote-override state machine. Enqueue `WRITE_DATA MsgID 16`; data = f8.8 of 16.0 = `0x1000`. Echo `TC: 16.00`. | 16 (`TrSet`) | f8.8: `0x1000` | `TC=+16.0` -> `TC: 16.00` | TASK-443 audit finding. TC must NOT map to MsgID 1. PIC maps TC to thermostat room setpoint override path (MsgID 9/16), not to boiler flow setpoint (MsgID 1). OTDirect minimal fix: route TC and TT identically to MsgID 16 (TrSet). |
| `TT=18.5` | `SetTempSetPoint` (gateway.asm:4057): `GetPosFloatArg`; passes `b'11'` (manual + program) to `SetSetPoint`. Same room-setpoint override path as TC but allows auto-restore by thermostat program. | Route to MsgID 16 (`TrSet`); data = f8.8 of 18.5 = `0x1280`. Echo `TT: 18.50`. | 16 (`TrSet`) | f8.8: `0x1280` | `TT=18.5` -> `TT: 18.50` | TT = temporary, TC = continuous; OTDirect treats both as MsgID 16 because the PIC remote-override state machine is not modelled. Limitation documented in code comment at OTDirect.ino:2141. |
| `TC=+0.0` | `GetPosFloatArg` zero: clears `setpoint1/2`, calls `ClearSetPoint`. Override cancelled. | Clear MsgID 16 write override. Echo `TC: 0.00`. | 16 | — | `TC=+0.0` -> `TC: 0.00` | Zero = clear override (PIC and OTDirect). |
| `TT=0` | Same zero path as TC=0. Clears thermostat setpoint override. | Clear MsgID 16 write override. Echo `TT: 0.00`. | 16 | — | `TT=0` -> `TT: 0.00` | Zero = clear. |

---

## How to Verify

### 1. Static format check (host, no firmware)

Run the accompanying validator to check every table row has all required columns:

```
python tests/check_otdirect_fixture.py
```

Zero errors = fixture is structurally valid. This is a format check; it does not
execute any firmware code.

### 2. Loopback mode (hardware, `GW=L`)

OTDirect supports a loopback test mode where it acts as both OT master and a
simulated boiler. This mode is activated by `GW=L` (OTDirect extension, no PIC
equivalent). In loopback mode the command dispatch still executes, so all commands
in this fixture can be sent and the responses compared without a real boiler or
thermostat connected.

Steps:

1. Flash OTGW32 with the 2.0.0 branch firmware.
2. Open a TCP connection to port 25238 (OTDirect bridge port).
3. Send `GW=L` to enter loopback mode.
4. Send each test input from the fixture and compare the actual serial echo to the
   expected response column.

### 3. CS/C2 expiry without a 60-second wait

The expiry timer `otCSLastCommandMs` is a `uint32_t` storing `millis()`. On a live
device:

- Send `CS=10.0`. Confirm echo `CS: 10.00`.
- Wait 61 seconds.
- Send any command that triggers the expiry check (e.g. `PR=M`).
- Confirm via Telnet debug log (port 23) that `OTD: CS heartbeat expired, clearing
  MsgID 1 override` appears and that subsequent OT frames no longer carry the CS
  override.

For automated testing without a 61-second wait, the `otCSLastCommandMs` variable
would need to be injectable (e.g. wrap in a getter/setter or expose via a test
backdoor command). This is currently not implemented; adding it is a future task.

### 4. SR= decimal byte-pair (host, `check_otdirect_fixture.py`)

The byte-pair encoding for SR=21 and SR=22 can be verified arithmetically:

- `SR=21:4,27`: expected data = `(4 << 8) | 27 = 1051 = 0x041B`. Month=April, Day=27.
- `SR=22:7,234`: expected data = `(7 << 256) | 234`... wait, `(7 << 8) | 234 = 1792 + 234 = 2026 = 0x07EA`. Year 2026.

These byte values match what otmonitor.tcl sends on the sync path (lines 1978/1986).

### 5. MM=0 data value verification

`MM=0` must produce OT frame `WRITE_DATA MsgID 14 data 0x0000`, not a clear
operation. Verify in loopback mode by inspecting the Telnet debug log for the
enqueued frame. A clear operation would produce no OT frame and a log line
containing `clearWriteOverride(14)`.
