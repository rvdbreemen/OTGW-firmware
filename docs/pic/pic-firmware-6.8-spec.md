# PIC firmware 6.8 reference specification

This document describes the OpenTherm Gateway PIC firmware 6.8 by Schelte Bron, as the code implements it. It is a reference for OT-Direct parity work in OTGW-firmware: when OT-Direct and the PIC differ, this page says what the PIC does. It describes the PIC only, not OT-Direct. Differences found so far are in [the 2026-11-12 parity audit](../reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md).

Source, read-only: `OTGW-other-projects/otgw-6.8/gateway.asm` (5525 lines; the OTGW-other-projects repository at commit 770aa45, the `other-projects` submodule of this repository). The gpsim test scripts `otgw-6.8/tests/*.stc` were read as examples of expected frames and replies; none was executed. Nothing was run on a PIC or a bench.

How to read the citations:

- `gateway.asm:N` and `gateway.asm:N-M` are lines of the 6.8 `gateway.asm`. A plain line number in brackets, such as `(gateway.asm:292-299, 227-234)`, continues the same file. `gateway.asm 6.6:N` in section (h) is the 6.6 source in the same repository.
- `tests/<file>.stc:N` is a gpsim test script in `otgw-6.8/tests/`.
- `OTGW-Commands.md` is the command reference in `OTGW-other-projects/OTGW-documentation/`. It is context only: where it disagrees with the code, the code is described and the conflict is listed in section (b).
- Every behavioural claim has a citation of a line that was opened while writing. What the code does not show is written as "not found in the code", and each section ends with a "Not found in the code / unclear" list instead of filled-in guesses.
- The sections were written by three readers, one per area, and assembled; some facts appear in more than one section with their own citations. A second reader checked a random sample of citations (see "Verification").

Conventions:

- The PIC runs at 4 MHz from the internal oscillator (`gateway.asm:186`, `gateway.asm:1025-1027`), so one timer tick at prescaler 1:1 is 1 us.
- Every serial command reply starts with the two command letters, `:` and a space (`gateway.asm:3723-3731`). The handler result follows. A non-zero return value is an EEPROM string address that the main loop prints, then the main loop always ends the line with CR LF (`gateway.asm:1429-1436`, `gateway.asm:1877-1881`).

## Contents

- [Open questions for the PIC author](#open-questions-for-the-pic-author)
- [(a) Frame relay and rewritten fields](#a-frame-relay-and-rewritten-fields)
- [(b) Serial commands](#b-serial-commands)
- [(c) PR= reports and the PS= summary](#c-pr-reports-and-the-ps-summary)
- [(d) Modes, alternative slots, PM=, boiler store, Data-Invalid vs Unknown-DataID, blacklisting](#d-modes-alternative-slots-pm-boiler-store-data-invalid-vs-unknown-dataid-blacklisting)
- [(e) Timing](#e-timing)
- [(f) Error and status lines](#f-error-and-status-lines)
- [(g) Start-up and reset](#g-start-up-and-reset)
- [(h) Changes from 6.6 to 6.8](#h-changes-from-66-to-68)
- [Verification](#verification)

## Open questions for the PIC author

Two code readings look like defects in the PIC firmware itself. Both were found independently by two readers and re-opened by the assembling session. They come from reading the code only; no test script covers them and nothing was run. They are questions for Schelte Bron, not established bugs.

1. **C2= sends the DataID as the CH2 setpoint integer.** `MsgID8Change` calls `setbyte3` without loading `controlsetpt3` first (`gateway.asm:2935-2938`). `setbyte3` writes W into byte3 (`gateway.asm:2688-2691`), and W still holds the DataID 8 from `movfw byte2` in `TreatMessage` (`gateway.asm:2539-2540`). The MsgID 1 handler does load its setpoint first (`movfw controlsetpt1` at `gateway.asm:2824-2825`). Read literally, `C2=40.5` sends HB 0x08 and the fraction in LB. The same code is in 6.6 (`gateway.asm 6.6:2927-2930`). `tests/command-2.stc` checks the reply text, not the frame.
2. **The fail-safe check after a watchdog reset reads the wrong bank, and its sense is the reverse of its name.** `conf` (with `NoFailSafe` at bit 6) is in `Bank0data` (`gateway.asm:315`, `:516`, `:522`), but bank 1 is selected at `gateway.asm:1050` and no `banksel` follows before `btfss NoFailSafe` at `gateway.asm:1072`. `mode` is in shared RAM (`UDATA_SHR`, `gateway.asm:286`, `:308`), so `FailSafeMode` itself is reached. Apart from the bank, `FS=0` sets `NoFailSafe` (`gateway.asm:5294-5299`, "Inverse logic"), and with `NoFailSafe` set the code skips the `bcf FailSafeMode` and keeps fail-safe after a watchdog timeout (`gateway.asm:1070-1073`). That is the opposite of the bit's comment "No fall back to fail safe mode" (`gateway.asm:522`). No test sends `FS=`.

Further open points are listed at the end of each section.

## (a) Frame relay and rewritten fields

Source: `gateway.asm` of PIC firmware 6.8 (version string at gateway.asm:6), repo OTGW-other-projects at 770aa45. Test script citations (`tests/<file>.stc:N`) are gpsim scripts. They were read as examples of expected frames. They were not executed.

Frame notation in the test scripts and on the serial line: a letter plus 8 hex digits. `T` is a frame received from the thermostat. `B` is a frame received from the boiler. `R` is a request the PIC changed before sending it to the boiler. `A` is an answer the PIC changed before sending it to the thermostat (gateway.asm:35-39). The first byte holds the parity bit (bit 7), the message type (bits 4-6) and spare bits. The second byte is the DataID. The third and fourth bytes are the data high byte (HB) and low byte (LB) (gateway.asm:292-299, 227-234).

### a.1 Flags that steer every rewrite

Four flags decide what reaches the boiler and what reaches the thermostat.

| Flag | Meaning | Set / cleared |
|---|---|---|
| `AlternativeUsed` (`mode,2`) | The frame was modified, or a slot (alternative) was used. Forces a parity recalculation and an `R`/`A` print. | Cleared per thermostat request at gateway.asm:2396. Set by `setbyte1` (gateway.asm:2683), `setbyte3` (gateway.asm:2688-2690), `setbyte4` (gateway.asm:2702-2704) when a byte really changes, and by `SendAltRequest` (gateway.asm:2410). Definition gateway.asm:312. |
| `OverrideUsed` (`stateflags,6`) | Only data bytes were changed. | Set per request at gateway.asm:2397. Cleared by `setbyte1` (gateway.asm:2684) and by `SendAltRequest` (gateway.asm:2411). `setbyte3`/`setbyte4` leave it alone (gateway.asm:2688-2692, 2702-2706). Definition gateway.asm:403. |
| `BoilerResponse` (`stateflags,7`) | The frame being treated is a real boiler answer. | Set at gateway.asm:2457, cleared at gateway.asm:2475 before a synthesised answer is treated. Definition gateway.asm:404. |
| `MsgResponse` (`byte1,6`) | Bit 6 of the type: the frame is an answer. | gateway.asm:297. |

A handler that sets the write bit directly with `bsf MsgWriteOp` (for example gateway.asm:2812, 2923, 2935, 2973, 3215) changes the type to WRITE without touching either flag. `MsgUnknown` is byte1 bit 5 (gateway.asm:298). That bit is set for Invalid-Data (0x20), Data-Invalid (0x60) and Unknown-DataID (0x70). Where the code must tell 0x60 from 0x70 it also tests bit 4 (gateway.asm:2442-2443, 3150-3153, 3186-3189).

`TreatMessage` (gateway.asm:2537-2668) is the per-DataID dispatcher. It runs in three situations (comment at gateway.asm:2522-2535):
1. a request from the thermostat (`MsgResponse`=0);
2. an answer from the boiler (`MsgResponse`=1, `BoilerResponse`=1);
3. a synthesised answer for the thermostat after a slot was used or the type was changed (`MsgResponse`=1, `BoilerResponse`=0).

DataIDs 128-255 skip the table and go to `WordResponse` (gateway.asm:2537-2538).

### a.2 The request path, thermostat to boiler, in time order

1. **Reception.** The comparator interrupt shifts bits into `byte1..byte4`. After the stop bit and an even-parity check it sets `MessageRcvd` (`input1int` gateway.asm:688-732, `storebit` gateway.asm:757-777). A bad stop bit, a parity error, a missing bit or more than one mid-bit transition ends in Error 02, 04, 03 or 01 (gateway.asm:956-1003). Error 01 is not raised for repeated mid-bit transitions when `IgnoreErr1` is set (gateway.asm:958-960). That setting is on in the EEPROM default (gateway.asm:5459).
2. **Report.** `MainLoop` calls `OpenThermMsg` (gateway.asm:1376-1378). That routine first prints the raw frame as a `T` line (`PrintRcvdMsg` gateway.asm:1489, 1922-1936). It then calls `HandleMessage` (gateway.asm:1491).
3. **`HandleRequest`** (gateway.asm:2388-2418):
   - A Write-Data request is stored in the value store (`StoreValue`, gateway.asm:2388-2389; see d.5).
   - The original type and data bytes are saved in `originaltype`, `databyte1` and `databyte2` (`StoreRequest` gateway.asm:2390, 2423-2429).
   - `repeatcount` is cleared when the DataID differs from the previous request. The DataID is saved in `originalreq` (gateway.asm:2391-2395).
   - `AlternativeUsed` is cleared and `OverrideUsed` is set (gateway.asm:2396-2397).
   - `TreatMessage` applies the per-ID rewrite (gateway.asm:2398).
   - `boilercom` is cleared (gateway.asm:2399). `clrf` also leaves the Z flag set.
   - If the DataID is blacklisted (`UnknownMask` returns Z=1), or if this is a user slot in stand-alone mode (`SendUserMessage`), the request is replaced via `SendAltRequest` (gateway.asm:2400-2403, 2408-2418; see d.2).
   - An unmodified request goes to the boiler unchanged, without a new parity and without an `R` line (gateway.asm:2404-2407).
   - A modified request goes through `SetParity`. That routine recomputes even parity and prints an `R` line unless monitor mode is active (gateway.asm:2478-2500).
4. **Transmit to the boiler.** Back in `OpenThermMsg`, with a thermostat connected and in gateway mode, `SendMessage` starts at once with `outputmask` = `SlaveMask`. The target is the boiler side, because `MsgResponse`=0 (gateway.asm:1492-1495, 1505-1528).
5. **Thermostat pre-emption.** If the thermostat line becomes active while the PIC is sending, or while it is receiving a boiler frame, the current operation is aborted so the thermostat is served (`inputabort` gateway.asm:734-755, entered from gateway.asm:700-706).

### a.3 The answer path, boiler to thermostat, in time order

1. The boiler frame is received the same way (`input2int` gateway.asm:671-685, then `inputmark` gateway.asm:707-732). It is printed as a `B` line (gateway.asm:1489, 1922-1924).
2. **`HandleResponse`** (gateway.asm:2433-2466):
   - `BoilerAlive` is set and the message-interval timer starts (gateway.asm:2433-2434, `StartInterval` gateway.asm:2514-2520).
   - An answer whose DataID equals `prioritymsgid` clears `PriorityMsg` (gateway.asm:2435-2438).
   - A Read-Ack (bit 4 clear) is stored in the value store (gateway.asm:2439-2440).
   - The Unknown-DataID strike counter is updated (gateway.asm:2441-2456; see d.7).
   - `BoilerResponse` is set (gateway.asm:2457).
   - If `Unsolicited` is set (Celcia 20 path), the answer is replaced by `RemoteCommand`: a Write-Ack for MsgID 4 with data 170 / `celciasetpoint` (gateway.asm:2458-2459, 2502-2512).
3. **Request was not modified** (`AlternativeUsed`=0). `TreatMessage` runs on the boiler answer. If a handler modified it, `SetParity` recomputes parity and prints an `A` line. Otherwise the boiler answer is relayed unchanged (gateway.asm:2460-2466).
4. **Request was modified** (`AlternativeUsed`=1). `SendAltResponse` runs (gateway.asm:2467-2477):
   - `TreatMessage` first processes the real boiler answer, which is not sent to the thermostat. Then `AlternativeDone` is set (gateway.asm:2467-2468).
   - In stand-alone mode nothing is returned (gateway.asm:2469-2470).
   - With `OverrideUsed`=1 (only data bytes were changed), the thermostat gets type `0x40 | originaltype`: a Read-Ack or Write-Ack of its own request type. It carries the saved original data bytes (gateway.asm:2471-2474, `CreateMessage` gateway.asm:3492-3500). This holds whatever the boiler answered, including Data-Invalid or Unknown-DataID. Handlers can refresh `databyte1`/`databyte2` first: MsgID 0 copies the boiler's slave status (gateway.asm:2757-2758), and MsgID 56/57 use `UpdateRequest` (gateway.asm:3151, 3187, 2420-2421).
   - With `OverrideUsed`=0 (the type was changed or a slot was used), the thermostat's original request is turned into Unknown-DataID (`0x70 | originaltype`) with the original data and DataID (gateway.asm:2473-2474, 3492-3500). `BoilerResponse` is cleared, and `TreatMessage` runs again on this synthesised answer (situation 3), where handlers or an SR= entry may turn it into an Ack or Data-Invalid (gateway.asm:2475-2477).
   - `SetParity` recomputes parity and prints an `A` line (gateway.asm:2478-2500).
5. **Transmit to the thermostat.** `SendMessage` uses `MasterMask` because `MsgResponse`=1 (gateway.asm:1505-1520).

**Stand-alone mode** (no thermostat): boiler answers are not forwarded (gateway.asm:2464-2465, 2469-2470). `MessagePtr` only advances when the answer's DataID matches the request (gateway.asm:1494-1502). Requests come from `StandAloneMsg` and `MessageTable` (gateway.asm:1797-1842). The trigger is `MsgTrigger`, set once a second by `TickTimer` (gateway.asm:1553-1568) or when the message-interval timer runs out (`MessageTimer` gateway.asm:1570-1577, `MsgInterval` from EEPROM, default 200 x 5 ms, gateway.asm:1235-1244, 5474).

**Watchdogs on the relay:**
- `repeatcount` counts seconds while `BoilerAlive` is set (gateway.asm:1553-1558). It is cleared when the thermostat requests a different DataID (gateway.asm:2391-2394), and on every frame in monitor mode or on a matching stand-alone answer (gateway.asm:1503). At 64 the PIC restarts with reset reason `L`, "Stuck in a loop (same message for 64 sec)" (gateway.asm:1403-1404, 1017-1019, 5515).
- `boilercom` counts seconds since the last request was prepared for the boiler, without rolling over. From 5 s on it clears `SlaveOut`, with the comment "Prevent unwanted heating" (gateway.asm:1559-1564). It is cleared at gateway.asm:2399.

### a.4 Per-MsgID rewrite table

"Req" is a request on its way to the boiler. "Ans" is an answer on its way to the thermostat. The ack type for the thermostat is built by `messageack`: `originaltype | 0xC0`, with bit 7 cleared again when the received frame had parity bit 0. `SetParity` recomputes parity afterwards (gateway.asm:2676-2679, 2478-2494). `messageinv` sets type Data-Invalid 0x60 (gateway.asm:2674-2675). `missingdata` answers Data-Invalid to a READ and Write-Ack with the thermostat's own data to a WRITE (gateway.asm:2696-2706).

| MsgID | Dir | What changes | Condition | Citation |
|---|---|---|---|---|
| 0 Status | Req | `MandatoryID` clears any blacklisting first. | Always. | gateway.asm:2720, 3411-3416 |
| 0 | Req | HB bit 0 (CH enable) := NOT `CHModeOff` (CH=). | CS= active (`SysCtrlSetpoint` clear) or `HeatDemand` (short-circuited on/off thermostat). | gateway.asm:2721-2731, 4414-4420, 1766-1769 |
| 0 | Req | HB bit 1 (DHW enable) := `HotWaterEnable` OR `dhwpushspoof`. | HW=0 or HW=1 (`HotWaterSwitch`), or a spoofed DHW push. | gateway.asm:2723-2726, 2732-2734, 4390-4409 |
| 0 | Req | HB bit 2 (cooling enable) := NOT `CoolModeOff` (CE=). `CoolModeOff` is clear after reset (RAM clear), so the bit is 1 until CE=0. | CL= active (`SysCoolLevel` clear). | gateway.asm:2721-2722, 2735-2736, 4446-4452, 1178-1188 |
| 0 | Req | HB bit 4 (CH2 enable) := NOT `CH2ModeOff` (H2=). `CH2ModeOff` is clear after reset (RAM clear), so the bit is 1 until H2=0. | C2= active (`SysCH2Setpoint` clear). | gateway.asm:2721-2722, 2737-2738, 5264-5270, 1178-1188 |
| 0 | Req | HB bit 5 (summer mode) := 1. | SM=1. With SM=0 the thermostat bit is passed. | gateway.asm:2739-2740, 4561-4567 |
| 0 | Req | HB bit 6 (DHW blocking) := 1. | BW=1, or the GPIO block function. With BW=0 the thermostat bit is passed. | gateway.asm:2741-2742, 4569-4575, 5444-5451 |
| 0 | Req | Bits 3, 7 and the LB are never changed. The change is applied with `setbyte3` only, so `OverrideUsed` stays set. | - | gateway.asm:2743-2747 |
| 0 | Ans | Read-Ack with the thermostat's original HB and the boiler's slave-status LB. | The request was modified and the boiler answer is valid. | gateway.asm:2753-2758, 2471-2474 |
| 0 | Ans | Read-Ack with the thermostat's original HB and LB. | The request was modified and the boiler answered with bit 5 set (Data-Invalid or Unknown-DataID). | gateway.asm:2753-2754, 2471-2474 |
| 0 | Ans | No change to the frame. Side effects: LEDs, a new fault schedules a priority read of MsgID 5, the DHW push check. | Valid boiler answer. | gateway.asm:2753-2781, 2783-2797 |
| 1 TSet | Req | `MandatoryID` first. Type := WRITE. Data := CS= value. | CS= active. | gateway.asm:2804-2815, 2822-2828, 4218-4231 |
| 1 | Req | Type := WRITE. Data := SH= value if set, else the boiler's max CH bound (`chsetpointmax`, default 90). LB := `maxchsetpoint2`. | Stand-alone or slot use, no CS=, `HeatDemand` set. | gateway.asm:2807-2809, 2813-2821, 1280-1281 |
| 1 | Req | Type := WRITE, data := 0.0. | Stand-alone or slot use, no CS=, no `HeatDemand`. | gateway.asm:2807-2809, 2813-2815, 2822-2828 |
| 1 | Ans | Frame unchanged. When the boiler answer has bit 5 set, the CS= override is cleared (`SysCtrlSetpoint` set). With a CS= rewrite the thermostat still gets a Write-Ack with its own TSet (a.3 step 4). | Boiler bit 5 set. | gateway.asm:2830-2833 |
| 1 | timer | CS= expires when no CS= arrived during a full run of the expiry timer. The timer is 121 Timer-1 overflows (`minutetimer` 120 counting down to bit 7) of 524.288 ms each, so roughly 63 s. The window restarts while CS=, C2= or SC= keep coming. Read from code only: CS= stays active between about 63 s and 127 s after the last CS=. Values with HB below 8 never expire. | Timer 1 overflow. | gateway.asm:157-158, 1612-1614, 1623-1646, 4227-4228, 4233-4239 |
| 2 Master config | Req | No frame change. Records the Smart Power bit and Remeha auto-detection (MemberID 11). | Always. | gateway.asm:2845-2861 |
| 2 | Ans | Type := ack of the request. HB := thermostat HB AND 0x01 (Smart Power only). LB := thermostat MemberID. | Every answer, whatever the boiler sent. | gateway.asm:2837-2844; example tests/thermostat-3.stc:8 (`AD002010B`) |
| 3, 17, 25 | Req | `MandatoryID` only (never blacklisted). | Always. | gateway.asm:2544, 2558, 2566, 3651 |
| 3, 17, 25 | Ans | SR= entry if one exists (see d.4). | SR= set. | gateway.asm:3651-3668 |
| 4 Remote request | Req | Not modified (handler returns for requests). | - | gateway.asm:2866-2867 |
| 4 | Ans | Clears the pending remote request (`INITRR`) when the answer echoes the request code (RR= value), or when the answer has bit 5 set (Data-Invalid or Unknown-DataID). An Ack with a different code leaves it pending. If byte3 = 170: ack with LB := `celciasetpoint`. | Answer with code 170. | gateway.asm:2868-2879, 259, 5287-5292 |
| 5 ASF flags | Req | Treats the request as acceptance of a pending override clear, then runs `roomsetptclear`. When a BS= value is set, `roomsetptchange` writes it into this MsgID 5 frame and makes it WRITE (read from code only, see the unclear list). | `OverrideClr` set and `OverrideWait` clear. | gateway.asm:2886-2891, 3021-3033 |
| 5 | Ans | SR= entry if one exists. | SR= set. | gateway.asm:2886-2887, 3652-3668 |
| 6 Remote param flags | Ans | Type := ack. HB := boiler HB OR 0x03 (DHW and max-CH transfer enable). LB unchanged. The boiler's LB r/w bits are recorded in `dhwupdate`/`maxchupdate` (0 when the answer is not a Read-Ack). On the first answer, pending SW=/SH= writes are dropped when the boiler cannot write them. | Every answer. | gateway.asm:2893-2918, 452-453 |
| 7 Cooling control | Req | Type := WRITE. Data := CL= value. | CL= active. | gateway.asm:2920-2927, 4454-4464 |
| 8 TSetCH2 | Req | Type := WRITE. LB := `controlsetpt4`. HB := the W register at that point (see the unclear list: read literally, that is the DataID 8, not the C2= integer part). In a slot without C2= the same WRITE goes out with whatever `controlsetpt4` holds. | C2= active, or slot use. | gateway.asm:2929-2938, 2539-2540 |
| 8 | Ans | SR= entry if one exists. | - | gateway.asm:2929-2930 |
| 9 Remote override setpoint | Ans | Type := ack. Data := TT=/TC= setpoint, or 0.0 when the override is being cleared. With a Celcia 20 and an override not yet accepted, `Unsolicited` is set, so the next boiler answer becomes the MsgID 4 frame of a.3. | An override is requested, or the boiler answered with bit 5 set. | gateway.asm:2940-2962 |
| 14 Max rel. modulation | Req | `MandatoryID` first. Type := WRITE. HB := MM= value, or 100 when no MM= is set. LB := 0. | MM= set, or slot use, or stand-alone. | gateway.asm:2965-2978, 4794-4807 |
| 16 Room setpoint | Req | Data := BS= value and type WRITE (via `messagewrite`). The TT/TC override tracking compares the thermostat value with the requested setpoint (tolerance 0.125) and sets or abandons the override. | BS= set (`NoFakeSetpoint` clear). | gateway.asm:2988-3045, 4289-4298; example tests/control-5.stc:14-22 |
| 16 | Ans | Always `missingdata`: a WRITE gets a Write-Ack with the thermostat's own data, a READ gets Data-Invalid. | Every answer, whatever the boiler sent. | gateway.asm:2984-2987, 2696-2706 |
| 20 Day/time | Ans | Type := ack. Data := SC= clock. | SC= time valid (`InvalidTime` clear). | gateway.asm:3047-3056, 4331-4388 |
| 20 | Ans | Boiler answer relayed. | No valid SC= time and a valid boiler answer. | gateway.asm:3050-3051, 2694-2695 |
| 20 | Ans | READ: Data-Invalid. WRITE: Write-Ack with the thermostat's data. | No valid SC= time and boiler bit 5 set. | gateway.asm:2694-2706; example tests/thermostat-2.stc:8-20 |
| 20 | timer | `InvalidTime` is set at an expiry-timer run without SC=. | Timer 1 overflow. | gateway.asm:1626-1627, 4372-4373 |
| 24 Room temperature (new in 6.8) | Req | Data := RT= value. Type := WRITE when the frame was modified. | RT= set (`NoFakeRoomTemp` clear). | gateway.asm:3058-3068, 2670-2673, 4300-4309, 1288 |
| 24 | Ans | SR= entry if one exists. Read from code only: when the thermostat sends a WRITE 24 with parity bit 1 and a different value, `setbyte1` sees byte1 as changed, so the parity bit counts as a change. It clears `OverrideUsed`, and the thermostat gets Unknown-DataID unless SR=24 is set. | - | gateway.asm:3061-3063, 2680-2686, 2471-2477, 3652-3668 |
| 27 Outside temp | Req | Type := WRITE. Data := OT= value. | OT= set and no external outside sensor. | gateway.asm:3071-3073, 3078-3088, 4256-4287 |
| 27 | Req | Type := WRITE, data := sensor value. With an invalid reading the type becomes Invalid-Data (0x20). When sent in a slot, `MandatoryID` runs first. | DS18x20 configured for outside temp (`ExternalSensor`, TS=O). | gateway.asm:3071-3077, 3089-3092; example tests/sensor-7.stc:25-36 |
| 27 | Ans | Type := ack, data := OT= or sensor value. Data-Invalid when the sensor reading is invalid. | As above. | gateway.asm:3078-3090 |
| 28 Return water | Ans | Type := ack, data := sensor value. Data-Invalid on an invalid reading. | DS18x20 configured for return water (TS=R). | gateway.asm:3094-3108; example tests/sensor-4.stc:16-25 |
| 48, 49 Bounds | Ans | No frame change. The boiler's bounds are stored for SW=/SH= range checks and for MsgID 56/57/1. Clears `INITHW`/`INITCH`. SR= entry if one exists. | Valid answer. | gateway.asm:3110-3132, 4164-4216 |
| 56 DHW setpoint | Req | Type := WRITE. Data := SW= value. The handler returns MsgID 56 when used in a slot. | SW= set and (thermostat WRITE, or a write still pending in `INITSW`). | gateway.asm:3134-3148 |
| 56 | Ans | Type := ack. Data := SW= value, or the boiler's DHW upper bound when SW= is not set. | Boiler answered Unknown-DataID (also for the synthesised Unknown-DataID of a.3). | gateway.asm:3149-3161 |
| 56 | Ans | SW= cleared in RAM and EEPROM. The answer is not rewritten here. | Boiler answered Data-Invalid. | gateway.asm:3162-3168, 3204-3207 |
| 57 Max CH setpoint | Req/Ans | Same as 56, with SH=, `INITSH`, `chsetpointmax`, `MaxCHSetting`. | As for 56. | gateway.asm:3170-3207 |
| 70, 101 | Ans | SR= entry; the thermostat's own HB is ORed with the SR HB (`ByteResponse`). | SR= set. | gateway.asm:2611, 2642, 3646-3650, 3656-3668 |
| 71 Vent setpoint | Req | Type := WRITE. LB := VS= value (0-100), HB := 0. In a slot without VS= the frame is still a WRITE, with LB 0 (`ventsetpoint` = 0), so 0 %. | VS= set, or slot use. | gateway.asm:3209-3217, 4241-4254; example tests/ventilation-1.stc:6-19 |
| 99 Operating modes | Req | READ becomes WRITE with the PIC's operating-mode bytes. | A mode write is pending (`opermodewrite`). | gateway.asm:3252-3256, 3281-3288 |
| 99 | Req | A thermostat WRITE is stored as the PIC's operating modes. Push bit 4 starts a DHW push. | Thermostat WRITE. | gateway.asm:3268-3279 |
| 99 | Ans | Type := ack. Data := PIC operating modes (`operatingmode1`/`2`). The push bit may be forced on while the PIC spoofs a push. | Read-Ack, Write-Ack, Data-Invalid and Unknown-DataID answers. | gateway.asm:3219-3250, 3258-3265 |
| 100 Remote override function | Ans | Type := ack. LB := override function flags. HB := LB when OH=1 (EEPROM default on), else 0. An SR= entry for 100 takes precedence. | Override requested, or boiler bit 5 set. | gateway.asm:3290-3324, 5459, 4969-4975 |
| 125 OT version slave | Ans | Type := ack, data 3.0. | Boiler bit 5 set (Data-Invalid or Unknown-DataID). | gateway.asm:3326-3334, 3342-3344; example tests/otversion-1.stc:8-18 |
| 125 | Ans | Data := 3.0. | Boiler major version below 3. | gateway.asm:3332-3334, 3342-3344 |
| 125 | Ans | Unchanged. | Boiler major version 3 or 4. | gateway.asm:3335-3337 |
| 125 | Ans | Data := 4.51 (HB 4, LB 51). New in 6.8. | Boiler major version 5 or higher. | gateway.asm:3335-3341; example tests/otversion-4.stc:6-19 |
| 126 Master product version | Req | No frame change. Remeha model detection (Celcia 20 / iSense). | Remeha thermostat detected. | gateway.asm:3346-3360 |
| 10, 12, 88, 90, 105, 107 (TSP/FHB size) | Ans | Thermostat-bound: SR= entry. PIC-initiated (slot): the entry count starts priority reads of the next DataID. | - | gateway.asm:3362-3378 |
| 11, 89, 106 (TSP read/write) | Req | Type := WRITE, LB := TP= value. | PIC-initiated TP= write. | gateway.asm:3380-3388, 4496-4559 |
| 13, 91, 108 and 11, 89, 106 | Ans | Index advances; the next entry is scheduled while index < count. No frame change. | PIC-initiated. | gateway.asm:3389-3397 |
| 93-95 (strings) | Ans | Same as above; the count comes from the answer's HB. | PIC-initiated. | gateway.asm:3399-3407 |
| 116-123 (counters) | Req | RS= sends WRITE with data 0 in a slot. | RS= pending. | gateway.asm:3471-3474, 3502-3517 |
| All other IDs 0-127, and 128-255 | Ans | SR= entry if one exists, otherwise unchanged. | SR= set. | gateway.asm:2537-2668, 3652-3668 |

Examples from the test scripts:
- CS=49.72 gives `R100131B9` and status `R00000300` (tests/control-1.stc:8-12).
- CL=94.38 with AA=7 gives `R10075E62` (tests/heatpump-2.stc:15-20).
- SR=21 and SR=22 are answered as `A4015010D` and `A401607E5` (tests/thermostat-2.stc:9-15).

### Not found in the code / unclear (a)

1. **No answer when the boiler is silent.** Not found in the code: a reply to the thermostat when the boiler does not answer a relayed request. The paths read only send to the thermostat from `HandleResponse` (gateway.asm:2433-2512, 1505-1520).
2. **MsgID 8 high byte.** `MsgID8Change` calls `setbyte3` without loading `controlsetpt3` first (gateway.asm:2935-2938). W still holds the DataID loaded at gateway.asm:2539. Read literally, the HB sent is 8, not the C2= integer part. No test script was found that checks an `R` frame for MsgID 8. This may be a defect; it was not verified in gpsim.
3. **BS= in MsgID 5 frames.** Read from code only: `MessageID5` -> `roomsetptclear` -> `roomsetptchange` would put a BS= value into a MsgID 5 request (gateway.asm:2886-2891, 3021-3033). Whether this happens in practice is unclear.
4. **The `boilercom` 5 s rule.** It clears `SlaveOut` (PORTA,3, gateway.asm:587, 1559-1564). `Disconnect` sets the same pin as "Output at idle level" (gateway.asm:1782). What line level `bcf SlaveOut` produces on the boiler interface is not settled from the code alone.
5. **MsgID 24 parity quirk.** Taken from reading `setbyte1` (gateway.asm:2680-2686). No test checks it.
6. **Override tracking.** The TT/TC state machine for MsgID 9/16/100 is only summarised here. The TT=/TC= command handlers were not opened in this session.

## (b) Serial commands

This section describes every serial command that PIC firmware 6.8 accepts (`gateway.asm`, repo at 770aa45). It describes the PIC only. All line numbers refer to `gateway.asm` unless a test file is named. Test files are the gpsim scripts in `otgw-6.8/tests/`.

### b.1 Line framing

- The serial port runs at 9600 baud, 8 bits, no parity (gateway.asm:171-173, gateway.asm:194-195).
- A received LF (0x0A) is ignored. A CR (0x0D) ends the command (gateway.asm:1470-1476).
- An empty command (CR only) is ignored and gives no reply (gateway.asm:1482-1484).
- The receive buffer is 16 bytes (gateway.asm:577). The write pointer stops at 15. Further characters overwrite position 15, and the CR then clears that position (gateway.asm:1477-1485). In effect the command is cut to its first 15 characters.
- While a finished command waits for processing, new characters are dropped (gateway.asm:1468-1469). The main loop processes the command at once (gateway.asm:1426-1431).
- A UART overrun sets a flag (gateway.asm:1419-1425). The next command is then answered with `OE` only, and the command itself is not run (gateway.asm:3711-3712, gateway.asm:3780-3781).
- A command is at least 4 characters and the third character must be `=`. Otherwise the reply is a bare `SE` without command echo (gateway.asm:3713-3722). Examples: `CS=` gives `SE` (tests/command-4.stc), `FOO=42` gives `SE` (tests/command-5.stc).
- Once the length and `=` checks pass, the PIC prints the two command letters, a colon and a space (`XX: `) before it looks at the command (gateway.asm:3723-3731). Every later reply, including error codes, follows this echo.
- After the handler returns, a non-zero return value is the EEPROM address of a reply string, which is printed. Then CR LF is printed (gateway.asm:1432-1436, gateway.asm:1877-1881). Then the debug pointer report runs (gateway.asm:1438, see DP).

### b.2 Dispatch

The two command letters are combined with XOR (gateway.asm:3732-3736).

- If the XOR result has any bit above bit 4 set, the command goes to the C2/H2/M2 handler. That handler accepts only a second letter `2` with first letter `C`, `H` or `M`. Anything else gives `NG` (gateway.asm:3737-3739, gateway.asm:5235-5248).
- Otherwise the XOR result (0 to 31) indexes a jump table (gateway.asm:3740-3778). Each slot then compares the first letter against fixed uppercase letters (gateway.asm:3783-3970). A miss gives `NG`.
- XOR slots 0x0C, 0x16, 0x18 and 0x19 give `NG` directly (gateway.asm:3755, gateway.asm:3769, gateway.asm:3771-3772).
- Slot 0x13 is `WD` in a `__DEBUG` build only. It jumps to itself (`goto $`), which ends in a watchdog reset. In a release build slot 0x13 gives `NG` (gateway.asm:3762-3766).
- All first-letter compares use uppercase literals, so lowercase commands give `NG` (gateway.asm:3783-3970).
- `ZZ=15.3` gives `ZZ: NG` (tests/command-3.stc).

The release build accepts 52 commands: AA, BS, BW, C2, CE, CH, CL, CR, CS, DA, DP, FS, FT, GA, GB, GW, H2, HW, IT, KI, LA, LB, LC, LD, LE, LF, M2, MH, MI, MM, MW, OH, OT, PM, PR, PS, RR, RS, RT, SB, SC, SH, SM, SR, SW, TC, TP, TS, TT, UI, VR, VS (gateway.asm:3743-3778, gateway.asm:5235-5248).

### b.3 Reply codes

The reply strings are stored in data EEPROM (gateway.asm:5492-5498).

| Code | Meaning | Where it is produced |
|---|---|---|
| `NG` | Unknown command | Dispatch misses (gateway.asm:3755, gateway.asm:3796-3970, gateway.asm:3975, gateway.asm:4030, gateway.asm:5238, gateway.asm:5248) |
| `SE` | Syntax error | Length or `=` check (gateway.asm:3718, gateway.asm:3722); a non-digit where a digit is expected (`CarrySyntaxErr`, gateway.asm:5187-5188); a bad hex digit (gateway.asm:5080-5097); trailing characters or a wrong separator in many handlers |
| `BV` | Bad value | Handler-specific value checks, for example CH=2 (tests/command-3.stc) |
| `OR` | Out of range | A decimal number above 255 (gateway.asm:5057-5066); handler-specific range checks |
| `NS` | No space left | AA with a full list (gateway.asm:4676); SR with all 16 slots used (gateway.asm:4849) |
| `NF` | Not found | DA for an ID that is not in the list (gateway.asm:4703); CR for an ID without a response (gateway.asm:4871) |
| `OE` | Overrun | A UART overrun before the command (gateway.asm:3711-3712, gateway.asm:3780-3781). Printed without echo |

### b.4 Shared parsers

Most error behaviour comes from four parsers. Each command row below names the parser it uses.

- **GetDecimal** (gateway.asm:5043-5068). The first character must be a digit, otherwise `SE`. Digits are read until the first non-digit. A value above 255 gives `OR`. On success carry is clear and the pointer stays on the first non-digit. The caller decides whether trailing characters are allowed.
- **GetFloat** (gateway.asm:5118-5185). An optional `-` or `+` sign, then a GetDecimal integer part (0 to 255). An optional `.` with digits follows. The first two fraction digits are kept and the third digit rounds (5 and up rounds up). Further digits must be digits but are ignored (gateway.asm:5138-5159). Any other character gives `SE` (gateway.asm:5135-5137, gateway.asm:5143-5146). The fraction is converted to 1/256 steps and rounded up when not exact (gateway.asm:5169-5178). A negative value is stored as two's complement (gateway.asm:5160-5168, gateway.asm:5184-5185). Examples: `OT=9.999` replies `OT: 10.00`, `OT=-9.999` replies `OT: -10.00`, `OT=-0.004` replies `OT: 0.00` (tests/command-6.stc).
- **GetPosFloat** (gateway.asm:5114-5117). Same as GetFloat, but only `+` is skipped. A leading `-` reaches GetDecimal and gives `SE`.
- **GetPercent** (gateway.asm:5099-5112). GetPosFloat, then a value above 100.00 gives `OR`. A parse error returns with the DC flag set, which CL uses (gateway.asm:5100).
- **CheckBoolean** (gateway.asm:4072-4082). The command must be exactly 4 characters (`SE` otherwise). The value must be `0` or `1` (`BV` otherwise). It returns Z set for `0`.

Float replies are printed by `PrintSigned`/`PrintFloat` with two decimals (gateway.asm:2043-2064), for example `TT: 16.10` (tests/override-1.stc).

### b.5 Room setpoint and room-side values

| Cmd | Syntax and parser | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| TT | `TT=<temp>`, GetPosFloat; no range check beyond 0 to 255.99 | Remote room setpoint override, temporary. Sets function bits 0b11 (manual and program change priority) for MsgID 100 (gateway.asm:4089-4094, gateway.asm:4096-4121) | No | `TT: 16.10` (tests/override-1.stc) | `TT=0`: cancels at once, or sends a clear first when an override was active (gateway.asm:4144-4153). Reply `TT: 0.00` | SE (sign `-`, bad syntax, empty value), OR (integer above 255) |
| TC | `TC=<temp>`, GetPosFloat | As TT, but function bits 0b01 (manual change priority only) (gateway.asm:4084-4088) | No | `TC: 16.20` (tests/override-2.stc) | `TC=0`, as TT. tests/override-4.stc shows the MsgID 9 answer back at `AC0090000` | As TT |
| SB | `SB=<temp>`, GetPosFloat | Setback temperature for GPIO functions HOME (5) and AWAY (6) (gateway.asm:4155-4162, gateway.asm:5420-5443) | Yes, AwaySetpoint, default 16.00 (gateway.asm:5461). Read from EEPROM on use (gateway.asm:5436-5440) | `SB: 18.23` (format from gateway.asm:4162; value seen with PR=S in tests/gpio-5.stc) | None. `SB=0` gives `SE` (gateway.asm:4158-4159) | SE, OR |
| BS | `BS=<temp>`, GetPercent (0 to 100.00) | Fake room setpoint written into thermostat MsgID 16 requests to the boiler (gateway.asm:4289-4298, gateway.asm:3027-3033). tests/control-5.stc: `BS=24.6` gives `R1010189A` | No | `BS: 24.60` (tests/control-5.stc) | `BS=0`: stop, reply `BS: 0.00` (gateway.asm:4295-4297, tests/control-5.stc) | SE, OR |
| RT | `RT=<temp>`, GetPercent (0 to 100.00) | Fake room temperature written into MsgID 24 requests (gateway.asm:4300-4309, gateway.asm:3061-3068). tests/heatpump-1.stc: `RT=25.41` with `AA=24` gives `R10181969` | No | `RT: 25.41` (format from gateway.asm:4309) | `RT=0`: stop, reply `RT: 0.00` (gateway.asm:4306-4309) | SE, OR |
| OT | `OT=<temp>`, GetFloat (signed) | Outside temperature for MsgID 27. Requests become WRITE with the value, responses become an ACK with the value (gateway.asm:4256-4267, gateway.asm:3071-3088). tests/command-7.stc: `OT=-1.4` gives `R101BFE9A` and `A401BFE9A` | No | `OT: -1.40` (format from gateway.asm:4267) | Any parse error, or a positive value of 64 or more, clears it. Reply `OT: -` (gateway.asm:4257-4265) | None. Every parse failure becomes the clear reply |

Notes on TT and TC:

- The value is copied to the override setpoint, the override LED starts blinking, and the request is flagged for MsgID 9 and MsgID 100 (gateway.asm:4107-4115).
- MsgID 9 responses to the thermostat carry the setpoint as an ACK (gateway.asm:2941-2953). MsgID 100 responses carry the function bits in the low byte, and also in the high byte when OH=1 (gateway.asm:3299-3311). tests/override-1.stc shows `AC009101A` and `A40640303` for TT=16.1. tests/override-2.stc shows `A40640101` for TC=16.2.
- The PIC compares the thermostat's MsgID 16 room setpoint with the override. A match within 0.125 degree sets the override as accepted and lights the LED (gateway.asm:2997-3020). A mismatch counts up to three attempts and then cancels the override (gateway.asm:3034-3045).
- Remeha Celcia 20: the setpoint times 5 is also sent as an unsolicited MsgID 4 remote command 170 (gateway.asm:4116-4143, gateway.asm:2502-2512).
- Remeha iSense: an old setpoint is cleared before a new one is set (gateway.asm:4100-4106).
- No expiry timer applies to TT or TC. CommandExpiry handles only SC, CS and C2 (gateway.asm:1623-1646).

Notes on OT:

- The value goes into the summary store at the MsgID 27 slot, and the OutsideTemp flag is set (gateway.asm:4276-4287, gateway.asm:568-569).
- When a DS1820 on GPIO B is configured for outside temperature (GB=7, TS=O), the sensor readings overwrite this value (gateway.asm:4269-4279, gateway.asm:3071-3077).
- The range check for -40 is commented out (gateway.asm:4262). No lower bound exists. Code reading only: an integer part of 128 to 255 without a sign sets bit 7 and is stored and printed as a negative value.

### b.6 Boiler control overrides

| Cmd | Syntax and parser | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| CS | `CS=<temp>`, GetPercent | Control setpoint. MsgID 1 requests become WRITE with this value (gateway.asm:4218-4229, gateway.asm:2804-2828). tests/control-1.stc: `CS=49.72` gives `R100131B9`. A Data-Invalid boiler answer drops the value (gateway.asm:2830-2833). Expiry, see below | No | `CS: 49.72` (tests/control-1.stc) | `CS=0`: the thermostat value passes again, reply `CS: 0.00` (gateway.asm:4230-4231) | SE, OR |
| C2 | `C2=<temp>`, GetPercent | Control setpoint CH2 for MsgID 8 (gateway.asm:5272-5285, gateway.asm:2929-2938). See the finding below | No | `C2: 45.60` (tests/command-2.stc) | `C2=0`, reply `C2: 0.00` (gateway.asm:5284-5285) | SE, OR |
| CH | `CH=0` or `CH=1`, CheckBoolean | CH enable bit (MsgID 0 master status bit 0). The bit is forced only while CS is active or an on/off thermostat demands heat (gateway.asm:4414-4420, gateway.asm:2720-2731). tests/control-1.stc: after `CH=0` the frame `R00000300` no longer appears | No (RAM statusflags, gateway.asm:464-468) | `CH: 0` (tests/control-1.stc) | None | SE (length), BV (`CH=2` gives `CH: BV`, tests/command-3.stc) |
| H2 | `H2=0` or `H2=1`, CheckBoolean | CH2 enable bit (MsgID 0 bit 4), forced while C2 is active (gateway.asm:5264-5270, gateway.asm:2737-2738) | No | `H2: 0` (tests/command-2.stc) | None | SE, BV |
| CL | `CL=<pct>`, GetPercent | Cooling control signal. MsgID 7 requests become WRITE with this value (gateway.asm:4454-4462, gateway.asm:2920-2927). tests/heatpump-2.stc: `CL=94.38` with `AA=7` gives `R10075E62` | No | `CL: 94.38` (format from gateway.asm:4462) | A non-numeric value clears it, reply `CL: -` (gateway.asm:4455-4456, gateway.asm:4463-4464). `CL=0` keeps 0 % active | OR (above 100). A parse error clears instead of SE |
| CE | `CE=0` or `CE=1`, CheckBoolean | Cooling enable bit (MsgID 0 bit 2), forced while CL is active (gateway.asm:4446-4452, gateway.asm:2735-2736) | No | `CE: 1` (format from gateway.asm:4452, gateway.asm:4070) | None | SE, BV |
| MM | `MM=<0..100>`, GetDecimal | Maximum relative modulation. MsgID 14 requests become WRITE with this value (gateway.asm:4794-4805, gateway.asm:2965-2978). Without an MM value, stand-alone mode sends 100 % (gateway.asm:2975-2976) | No | `MM: 40` (format from gateway.asm:4805) | Any GetDecimal failure clears it, reply `MM: -` (gateway.asm:4795-4796, gateway.asm:4806-4807). So `MM=300` clears | SE (trailing characters, gateway.asm:4797-4799), OR (101 to 255, gateway.asm:4800-4802) |
| VS | `VS=<0..100>`, GetDecimal | Ventilation setpoint. MsgID 71 requests become WRITE with the value in the low byte and 0 in the high byte (gateway.asm:4241-4252, gateway.asm:3210-3217). tests/ventilation-1.stc: `VS=15` gives `R9047000F` | No | `VS: 15` (tests/ventilation-1.stc) | Any GetDecimal failure clears it, reply `VS: -` (gateway.asm:4242-4243, gateway.asm:4253-4254, tests/ventilation-1.stc) | SE (trailing characters, gateway.asm:4244-4246), OR (101 to 255, gateway.asm:4247-4249) |
| SH | `SH=<temp>`, GetFloat | Maximum CH water setpoint for MsgID 57 (gateway.asm:4191-4216, gateway.asm:3171-3184). With an on/off thermostat demanding heat it is also the control setpoint (gateway.asm:2812-2821) | Yes, MaxCHSetting (gateway.asm:4214-4215, gateway.asm:5473). Restored and re-sent at boot (gateway.asm:1224-1229) | `SH: 60.00` (format from gateway.asm:4216) | `SH=0`: no value, reply `SH: 0.00` (gateway.asm:4194-4195, gateway.asm:4206-4216) | SE, OR, BV (outside the boiler's MsgID 49 bounds, gateway.asm:4196-4205) |
| SW | `SW=<temp>`, GetFloat | DHW setpoint for MsgID 56 (gateway.asm:4164-4189, gateway.asm:3135-3148). tests/dhw-7.stc: `SW=55.7` gives `R903837B4` | Yes, DHWSetting (gateway.asm:4187-4188, gateway.asm:5472). Restored and re-sent at boot (gateway.asm:1218-1223, tests/dhw-7.stc) | `SW: 55.70` (format from gateway.asm:4189) | `SW=0`: no value, reply `SW: 0.00` (gateway.asm:4167-4168, gateway.asm:4179-4189) | SE, OR, BV (outside the boiler's MsgID 48 bounds, gateway.asm:4169-4178) |
| HW | `HW=<c>`, exactly 4 characters | `0`: DHW enable forced off. `1`: forced on. `P`: start a manual DHW push. Any other character: the thermostat controls the bit (gateway.asm:4390-4412, gateway.asm:2724-2734) | Yes for 0, 1 and other (Configuration byte, gateway.asm:4408, gateway.asm:4617-4620). `P` is not stored | `HW: 0`, `HW: 1`, `HW: A`, `HW: P` (gateway.asm:2254-2259, gateway.asm:4410-4412, tests/dhw-3.stc) | Any character other than 0, 1 or P, reply `HW: A` (tests/dhw-3.stc) | SE (length only). No BV |
| BW | `BW=0` or `BW=1`, CheckBoolean | DHW blocking. `BW=1` forces MsgID 0 bit 6 on. `BW=0` leaves the thermostat's bit alone (gateway.asm:4569-4575, gateway.asm:2741-2742). tests/dhw-9.stc: `BW=1` gives `R00004200` | No | `BW: 1`, `BW: 0` (tests/dhw-9.stc) | `BW=0` | SE, BV |
| SM | `SM=0` or `SM=1`, CheckBoolean | Summer mode. `SM=1` forces MsgID 0 bit 5 on. `SM=0` leaves the thermostat's bit alone (gateway.asm:4561-4567, gateway.asm:2739-2740). tests/serial-5.stc: `SM=1` gives `R00002200` | Yes, Configuration byte (gateway.asm:4746, gateway.asm:519, tests/serial-5.stc) | `SM: 1` (tests/serial-5.stc) | `SM=0` | SE, BV |
| MH | `MH=<n>`, GetDecimal, n 0 to 15 | Operating mode HC1, low nibble of the MsgID 99 low byte. Queues a MsgID 99 WRITE (gateway.asm:4466-4478, gateway.asm:3281-3288). tests/opermode-1.stc: `MH=2` gives `A40630002` | No | `MH: 2` (tests/opermode-1.stc) | `MH=0` | SE, OR (16 to 255) |
| MW | `MW=<n>`, GetDecimal, n 0 to 15 | Operating mode DHW, low nibble of the MsgID 99 high byte (gateway.asm:4482-4494) | No | `MW: 3` (format from gateway.asm:4493-4494) | `MW=0` | SE, OR |
| M2 | `M2=<n>`, GetDecimal, n 0 to 15 | Operating mode HC2, high nibble of the MsgID 99 low byte (gateway.asm:5250-5262) | No | `M2: 5` (format from gateway.asm:5261-5262) | `M2=0` | SE, OR |

**CS, C2 and SC expiry.** Timer 1 overflows every 524.288 ms (gateway.asm:157-158, gateway.asm:1104-1112). Each overflow counts `minutetimer` down (gateway.asm:1612-1614, gateway.asm:1623-1625). StartTimer loads 120 and clears the three heartbeat flags, but only when the timer had expired (gateway.asm:4233-4239). It runs out after 121 overflows, about 63.4 s. At expiry (gateway.asm:1623-1646):

- If no SC arrived in the window, the clock becomes invalid.
- If no CS arrived in the window and the CS integer part is 8 or more, CS is dropped. The same rule applies to C2.
- If any of the three commands arrived, the timer restarts.

The lifetime of a single command depends on the timer state when it arrives:

- **Timer idle.** The command sets its heartbeat flag, and the StartTimer call that follows clears it again (gateway.asm:4227-4228, gateway.asm:4372-4373, gateway.asm:5281-5282, gateway.asm:4233-4239). At the end of that window the value is dropped. Lifetime about 63 s.
- **Timer already running** (started by an earlier SC, CS or C2). StartTimer does nothing, so the flag survives. At the end of the window the value is kept and the timer restarts with the flags cleared (gateway.asm:1640-1644). The value is then dropped one window later. Lifetime between about 63 s and about 126 s, depending on the timer phase.

Repeating the command at least once per window keeps it active. tests/control-3.stc: `CS=54.3` sent at 2 s (timer idle) is still active at 32 s and gone between 65 s and 74 s. CS below 8 does not expire: tests/control-4.stc (`CS=5`, still `R10010500` after 74 s).

**Finding, code reading only: C2 high byte.** `MsgID8Change` calls `setbyte3` without loading `controlsetpt3` first (gateway.asm:2935-2938). W still holds the MsgID that was loaded for the jump table, which is 8 (gateway.asm:2539-2540). The integer part sent in MsgID 8 then looks like it is always 8, while the low byte carries the C2 fraction. `controlsetpt3` is only read at gateway.asm:1634 and written at gateway.asm:5275. tests/command-2.stc checks only the reply text, not the frame.

**HW=P (DHW push).** It calls StartDHWPush. If no push is running, the PIC sets the push bit and queues a MsgID 99 WRITE. If MsgID 99 is known to be unsupported, the PIC fakes the push by forcing the DHW enable bit instead (gateway.asm:3273-3279, gateway.asm:2724-2734). Any other HW value aborts a running push (gateway.asm:4398-4399). The push ends when the boiler's DHW mode bit clears. After 8 status answers without flame it ends as failed (gateway.asm:2783-2797).

**MH, MW, M2.** These have no end-of-string check, so `MH=2x` is taken as 2 (gateway.asm:4466-4471, gateway.asm:4482-4487, gateway.asm:5250-5255). The values sit in `operatingmode1` and `operatingmode2`. A thermostat MsgID 99 WRITE overwrites both bytes (gateway.asm:3267-3271). Code reading only: PushOperModes also sets `dhwpushpending` (gateway.asm:3285-3287). A boiler WRITE-ACK without the push bit then appears to start a spoofed push (gateway.asm:3233-3238). No test covers this.

**SW and SH details.**

- The bounds come from the boiler's MsgID 48 and MsgID 49 responses (gateway.asm:3111-3132). Until then the defaults apply: DHW 20 to 80, CH 10 to 90 (gateway.asm:262-265, gateway.asm:1274-1281). The integer part must be within the bounds. The maximum itself is accepted only without a fraction (gateway.asm:5228-5233, gateway.asm:4173-4178).
- A new value is pushed to the boiler only when MsgID 6 says the parameter is writable (gateway.asm:4181-4182, gateway.asm:4208-4209, gateway.asm:2900-2918).
- At boot a stored non-zero value schedules a WRITE (gateway.asm:1218-1229, gateway.asm:3447-3452).
- An Unknown-DataID answer is turned into an ACK with the user value toward the thermostat (gateway.asm:3149-3161, gateway.asm:3185-3197).
- A Data-Invalid answer erases the value in RAM and EEPROM (gateway.asm:3162-3168, gateway.asm:3198-3207).

### b.7 Clock

| Cmd | Syntax | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| SC | `SC=HH:MM/D`. HH by GetDecimal 0 to 23, MM 0 to 59, D 1 to 7 | Stores time and day. The PIC answers thermostat MsgID 20 reads and writes with them (gateway.asm:4331-4373, gateway.asm:3048-3056). tests/thermostat-2.stc: `SC=21:50/3` gives `A50147532` and `AC0147532`. The PIC never advances the clock: `clock1`/`clock2` are written only by SC (gateway.asm:4334, gateway.asm:4347, gateway.asm:4371). Valid for about 63 s to about 126 s, depending on the timer phase (see b.6) | No | `SC: 21:50/3`. Hours and minutes are always two digits (gateway.asm:4374-4388, gateway.asm:2084-2101) | None. The value lapses at expiry. Then a boiler Unknown or Data-Invalid answer becomes Data-Invalid for a read and a WRITE-ACK for a write (gateway.asm:2694-2706) | OR (hour above 23, minute above 59), SE (missing `:` or `/`, bad minutes, trailing characters), BV (day 0 or above 7) (gateway.asm:4335-4368) |

### b.8 Service requests

| Cmd | Syntax | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| RR | `RR=<code>`, GetDecimal 0 to 255; no end-of-string check | Queues a MsgID 4 WRITE with the code in the high byte (gateway.asm:5287-5292, gateway.asm:3456-3462). The PIC repeats it until the boiler answers with the same code or with Data-Invalid/Unknown (gateway.asm:2866-2872). tests/service-2.stc: `RR=1` gives `R90040100` once | No | `RR: 1` (format from gateway.asm:5292) | None | SE, OR (above 255) |
| RS | `RS=XYZ`, exactly 6 characters. X = H or W, Y = B or P, Z = S or H | Clears one boiler counter: one WRITE of 0 to MsgID 116 to 123 (gateway.asm:4984-5018, gateway.asm:3502-3517, gateway.asm:3418-3421). HBS=116, HPS=117, WPS=118, WBS=119, HBH=120, HPH=121, WPH=122, WBH=123 (bit mapping at gateway.asm:4978-5016) | No | `RS: HBS` (gateway.asm:5019-5024) | None | SE (length or letter) |
| TP | `TP=<id>:<idx>` reads, `TP=<id>:<idx>=<val>` writes. GetDecimal for all three numbers | Reads or writes one TSP or FHB entry as a priority message (gateway.asm:4496-4559, gateway.asm:3380-3388). Valid IDs: TSP 11, 89, 106 (read and write) and FHB 13, 91, 108 (read only) (gateway.asm:4503-4517, gateway.asm:4534-4537). tests/tsp-4.stc: `TP=11:1` gives `R000B0100`. tests/tsp-5.stc: `TP=11:1=39` gives `R900B0127` | No | `TP: 11:1` or `TP: 11:1=39` (gateway.asm:4547-4559) | None | BV (ID not in the list), SE (missing `:`, `=` on an FHB, trailing characters; `TP=13:1=39` gives `TP: SE`, tests/tsp-8.stc), OR |
| PM | `PM=<id>`, GetDecimal 0 to 255 | One-off priority READ of that ID (gateway.asm:5026-5037, gateway.asm:3464-3470). If it is a TSP/FHB count (10, 12, 88, 90, 105, 107) the PIC reads every entry next. For 93, 94 and 95 it reads every string character (gateway.asm:3364-3378, gateway.asm:3389-3407). tests/tsp-1.stc: `PM=10` gives `R000A0000`, then `R800B0000`, `R000B0100`, `R000B0200` | No | `PM: 10` (format from gateway.asm:5037) | None | SE, OR |

Notes:

- RR, RS, TP and PM frames go out when the PIC picks an alternative message. That happens when the thermostat sends an ID that is known to be unsupported, or in a stand-alone user slot (gateway.asm:2398-2418, gateway.asm:1822-1842). The order is: admin messages first, then the priority message, then a pending counter reset, then the alternative list (gateway.asm:3418-3489).
- TP writes the new ID into `prioritymsgid` and clears the pending priority flag before it validates (gateway.asm:4499-4501). A TP that ends in BV or SE therefore cancels an earlier PM or TP that was still pending.

### b.9 Message manipulation

| Cmd | Syntax | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| AA | `AA=<id>`, GetDecimal 1 to 255 | Adds the ID to the first free slot of the 32-entry alternative list. Duplicates are allowed (gateway.asm:4659-4678) | Yes, AlternativeCmd. The default list is 116 to 123 (gateway.asm:5520-5524). Read from EEPROM on use (gateway.asm:3480-3486) | `AA: 24` (format from gateway.asm:4678) | DA | OR (`AA=0`, gateway.asm:4662-4663), SE, NS (list full, gateway.asm:4676) |
| DA | `DA=<id>`, GetDecimal 1 to 255 | Deletes the first occurrence (gateway.asm:4680-4703). The PIC also deletes an ID itself once the boiler has rejected it three times (gateway.asm:2450-2451) | Yes | `DA: 116` (format from gateway.asm:4691) | n/a | OR (`DA=0`), SE, NF |
| UI | `UI=<id>`, GetDecimal | Marks the ID as unsupported by the boiler at once, so the PIC sends alternatives in its place (gateway.asm:4890-4907, gateway.asm:3519-3538) | Yes, UnknownFlags bitmap (gateway.asm:5518-5519). Re-installed at boot (gateway.asm:1298-1324) | `UI: 27` (format from gateway.asm:4907) | KI | BV (ID 128 to 255, gateway.asm:4895-4896), SE, OR |
| KI | `KI=<id>`, GetDecimal | Clears the unsupported mark and the failure count (gateway.asm:4909-4926) | Yes | `KI: 27` (format from gateway.asm:4907) | n/a | BV (128 to 255), SE, OR |
| SR | `SR=<id>:<hb>,<lb>` or `SR=<id>:<lb>`, GetDecimal for each number | Fixed answer to the thermostat for that ID, in one of 16 RAM slots (gateway.asm:4813-4859, gateway.asm:574-576). One byte goes into the low byte, with 0 in the high byte (gateway.asm:4823, gateway.asm:4841). The bytes are ORed into the response as an ACK (gateway.asm:3651-3668). tests/thermostat-2.stc: `SR=21:1,13` gives `A4015010D` | No | `SR: 18:1/230`; one byte gives `SR: 70:0/14` (gateway.asm:4856-4859, gateway.asm:2066-2071) | CR | BV (`SR=0:...`), SE (missing `:` or `,`, trailing characters), OR, NS (16 slots used) |
| CR | `CR=<id>`, GetDecimal | Releases the SR slot (gateway.asm:4861-4873) | No | `CR: 18` (format from gateway.asm:4873) | n/a | BV (`CR=0`), SE, NF |
| MI | `MI=<ms>`, decimal | Stand-alone message interval after each boiler answer, in 5 ms steps (gateway.asm:4577-4615, gateway.asm:2514-2520, gateway.asm:1570-1577) | Yes, MessageInterval, default 200 = 1000 ms (gateway.asm:5474). Restored at boot (gateway.asm:1235-1240) | `MI: 100` (tests/interval-1.stc: `MI=99` gives `MI: 100`) | None | SE (last character not a digit, other characters), OR (one digit only, result below 100 ms, or overflow; `MI=97` gives `MI: OR`, tests/interval-2.stc) |

Notes:

- **SR scope.** SR answers apply only to IDs whose handler ends in WordResponse or ByteResponse (gateway.asm:2541-2668, gateway.asm:3646-3668). IDs with their own handler and no WordResponse call ignore the SR table. Examples are 0, 1, 9, 14 and 20 (gateway.asm:2718-2751, gateway.asm:2804-2833, gateway.asm:2941-2962, gateway.asm:2965-2978, gateway.asm:3048-3056). IDs 128 to 255 go through WordResponse (gateway.asm:2537-2538).
- **UI=0 and KI=0, code reading only.** For value 0, `skpz` skips the store into `float1`, so the handler works on whatever `float1` held before (gateway.asm:4893-4894, gateway.asm:4912-4913). The RAM mark then lands on ID 0, which MandatoryID unmarks again on the next MsgID 0 request (gateway.asm:2720, gateway.asm:3411-3416). The EEPROM bit lands on the stale `float1` ID. The result is not defined by the code.
- **MI rounding.** The last digit is split off. The rest R gives 2R, plus 1 if the last digit is 3 or more, plus 1 more if it is 8 or 9. This is the value divided by 5, rounded (gateway.asm:4596-4605). The result must be 20 or more, that is 100 ms (gateway.asm:4606-4609). R of 128 or more gives OR (gateway.asm:4597-4599). Code reading only: 1278 and 1279 wrap the byte to 0 and give OR, so the largest accepted interval is 1277, which is stored as 1275 ms. The reply prints the stored value times 5 (gateway.asm:2238-2244). The 1-second TickTimer also sets MsgTrigger (gateway.asm:1554-1568), so an interval above 1000 ms looks like it has no extra effect (code reading only).

### b.10 Configuration and hardware

| Cmd | Syntax | Effect | EEPROM | Success reply | Clear form | Errors |
|---|---|---|---|---|---|---|
| GW | `GW=0`, `GW=1`, `GW=R` | 1 = gateway mode, 0 = monitor mode. The bit is MonModeSwitch with inverse logic (gateway.asm:4729-4747). `R` resets the PIC (gateway.asm:4776-4780) | Yes, Configuration byte. Applied at boot (gateway.asm:1211-1216) | `GW: 1` (tests/command-1.stc), `GW: 0`. `GW: P` when timer 2 is running; the switch then happens later (gateway.asm:4743-4752, gateway.asm:1588-1589). Fail-safe mode forces monitor mode (gateway.asm:4753-4756) | n/a | SE (length), BV (other characters, gateway.asm:4776-4779). `GW=R` prints only `GW: ` before the reset; afterwards PR=Q reports `C` (tests/reset-2.stc, gateway.asm:5512) |
| LA to LF | `Lx=<F>`, exactly 4 characters, F = A to Z | Assigns function F to LED x (gateway.asm:3972-4025) | Yes, FunctionLED1 to FunctionLED6, defaults F, X, O, M, P, C (gateway.asm:5463-5468). Restored at boot (gateway.asm:1329-1353) | `LA: F` (gateway.asm:3990-3991) | n/a | SE (length), BV (not an uppercase letter, gateway.asm:3980-3985) |
| GA, GB | `Gx=<n>`, GetDecimal 0 to 8 | GPIO function. 0 none, 1 GND, 2 VCC, 3 LED E, 4 LED F, 5 HOME, 6 AWAY, 7 DS1820 (GB only), 8 DHW block (gateway.asm:4027-4070, gateway.asm:5308-5316) | Yes, FunctionGPIO. GA is the low nibble, GB the high nibble (gateway.asm:4041-4057). Restored at boot (gateway.asm:1355-1359) | `GA: 3` (gateway.asm:4066-4070) | `Gx=0` | SE (trailing characters), BV (above 8, gateway.asm:4038-4040), OR. `GA=7` gives BV from `gpio_invalid` after the EEPROM write (gateway.asm:4052-4065, gateway.asm:5346-5356). At boot the same check clears it again |
| TS | `TS=O` or `TS=R`; no length check | DS1820 used for outside temperature (MsgID 27) or return water (MsgID 28) (gateway.asm:4782-4792, gateway.asm:3071-3108) | Yes, Configuration byte (gateway.asm:4746, gateway.asm:518) | `TS: O` (gateway.asm:4966-4967) | n/a | BV (other characters, gateway.asm:4786-4788) |
| VR | `VR=<0..9>`, exactly 4 characters | Comparator reference voltage, 0.832 V to 1.984 V (gateway.asm:4422-4444) | Yes, SavedSettings bits 0 to 4. Default 5 (gateway.asm:5459). Restored at boot (gateway.asm:1205-1209). tests/serial-1.stc: `VR=3` leaves EEPROM byte 0 = 0x73 | `VR: 3` (gateway.asm:2197-2199) | n/a | SE (length), BV (not a digit) |
| IT | `IT=0` or `IT=1`, CheckBoolean | 1 = ignore extra mid-bit transitions, so no Error 01 (gateway.asm:4948-4967) | Yes, SavedSettings bit 5. Default 1 (gateway.asm:5459). tests/serial-2.stc: PR=T goes from 11 to 01 after `IT=0` and a reset | `IT: 0` (gateway.asm:4966-4967) | n/a | SE, BV |
| OH | `OH=0` or `OH=1`, CheckBoolean | 1 = copy the MsgID 100 override flags into the high byte as well (gateway.asm:4969-4975, gateway.asm:3309-3311) | Yes, SavedSettings bit 6. Default 1 (gateway.asm:5459) | `OH: 1` (gateway.asm:4966-4967) | n/a | SE, BV |
| FT | `FT=<c>`, exactly 4 characters | C = Celcia 20, I = iSense, S = standard. Any other character restores auto-detect (gateway.asm:4705-4727) | Yes, TStatModel (gateway.asm:4725-4726). Restored at boot (gateway.asm:1294-1296). tests/thermostat-6.stc: EEPROM 0x0D = 0x10 after `FT=S` | `FT: S`; the reply is D, S, R, C or I (gateway.asm:2270-2280, tests/thermostat-6.stc) | Any character other than C, I, S | SE (length). No BV |
| FS | `FS=0` or `FS=1`, CheckBoolean | Fail-safe behaviour after a watchdog reset (gateway.asm:5294-5301, gateway.asm:1066-1073). See the note below | Yes, Configuration byte bit 6 (gateway.asm:522, gateway.asm:5300) | `FS: 1` (gateway.asm:4966-4967) | n/a | SE, BV |

**FS, code reading only, unsettled.** `FS=0` sets NoFailSafe (`conf,6`) and `FS=1` clears it (gateway.asm:5296-5299, gateway.asm:522). The boot code uses it after a watchdog reset (gateway.asm:1066-1073):

1. It sets FailSafeMode (in shared RAM, gateway.asm:308-313).
2. It runs `btfss NoFailSafe`, then `bcf FailSafeMode`. The flag therefore stays set only when the tested bit is 1.

Two problems make the real behaviour unclear:

- **Bank.** `conf` is in bank 0 RAM (gateway.asm:315, gateway.asm:516). Bank 1 is selected at gateway.asm:1050, and no bank switch follows before gateway.asm:1072. The PCON and `resetreason` accesses at gateway.asm:1056-1065 and gateway.asm:1074-1075 are bank 1 as well. The test at gateway.asm:1072 then appears to read a bank-1 byte at the offset of `conf`, not `conf` itself. Bank warnings are suppressed (gateway.asm:18). Under this reading FS has no defined effect.
- **Polarity.** If the bit tested were `conf,6`, fail-safe after a watchdog reset would be on with `FS=0` and off with `FS=1`. The default Configuration byte is 0 (gateway.asm:5471), which would mean no fail-safe by default. That is the reverse of the bit name, the comment at gateway.asm:1072 and the command reference.

No test in the folder sends `FS=`. Check this on the bench before relying on FS.

### b.11 Diagnostics and reports

| Cmd | Syntax | Effect | EEPROM | Success reply | Errors |
|---|---|---|---|---|---|
| DP | `DP=<hh>` or `DP=<hhh>`, hex (upper or lower case) | Debug pointer. After each command and after each processed message the PIC prints `<addr>=<value>` (gateway.asm:4311-4329, gateway.asm:1949-1966, gateway.asm:1438, gateway.asm:1500-1504). Only the low byte is tested, so any address with low byte 00 switches it off (gateway.asm:1949-1951) | No | Always three hex digits: `DP=73` replies `DP: 073` (gateway.asm:4326-4329, gateway.asm:1883-1892) | SE (length not 5 or 6, bad hex digit) |
| PR | `PR=<letter>` | Prints one information item. See section (c) | n/a | see section (c) | see section (c) |
| PS | `PS=0` or `PS=1` | Summary mode. See section (c) | n/a | see section (c) | see section (c) |

### b.12 Persistence summary

| Stored in EEPROM and restored at boot | RAM only, lost at reset |
|---|---|
| SW, SH (gateway.asm:1218-1229); MI (gateway.asm:1235-1240); VR, IT, OH (gateway.asm:1205-1209); GW, TS, SM, HW (except P), FS (Configuration byte, gateway.asm:1211-1216, gateway.asm:516-522); LA to LF (gateway.asm:1329-1353); GA, GB (gateway.asm:1355-1359); FT (gateway.asm:1294-1296); UI and KI (gateway.asm:1298-1324); AA and DA (list read on use, gateway.asm:3480-3486); SB (read on use, gateway.asm:5436-5440) | TT, TC, BS, RT, OT, CS, C2, CH, H2, CL, CE, MM, VS, BW, MH, MW, M2, SC, RR, RS, TP, PM, SR, CR, DP, HW=P. At boot RAM banks 0 to 3 are cleared (gateway.asm:1177-1188) and the overrides are marked unset (gateway.asm:1283-1289) |

Defaults after a reset:

- `statusflags` is cleared, so the CH, CE and H2 bits are sent as 1 while their override is active (gateway.asm:2721-2722, gateway.asm:1177-1188).
- No CS, C2, CL, MM, BS or RT value is set, and the clock is invalid (gateway.asm:1283-1289).

### b.13 Conflicts with OTGW-Commands.md

The command reference is context only. gateway.asm wins.

1. TT and TC: the reference gives a range of 0.0 to 30.0. The code only limits the integer part to 255 (GetPosFloat, gateway.asm:4084-4094).
2. OT: the reference says -40 to +64. The -40 check is commented out (gateway.asm:4262). A positive value of 64 or more clears instead of giving an error (gateway.asm:4259-4265).
3. CS=0: the reference says the gateway "falls back to its internal default control setpoint of 10 degrees" and clears CH enable. No 10-degree default was found in the code. CS=0 sets the "no user setpoint" flag (gateway.asm:4230-4231). Stand-alone or alternative frames then carry 0, unless an on/off thermostat demands heat (gateway.asm:2812-2828). In stand-alone mode `CS=0` gives `R10010000` and `R00000000`, so the CH enable bit is 0 there (tests/standalone-7.stc; also `R10010000` in tests/thermostat-5.stc). With a thermostat the CH enable bit is the thermostat's own bit (gateway.asm:2729-2731). The "clears CH enable" part matches stand-alone mode; the 10 degrees does not.
4. SC, CS, C2 lifetime: the reference says "approximately 61 seconds" for SC. The code window is 121 x 524.288 ms, about 63.4 s, and a command can live up to about two windows depending on the timer phase (gateway.asm:157-158, gateway.asm:4233-4239, gateway.asm:1623-1646). The reference's "repeat at least every minute" is a safe minimum, not a conflict.
5. MH, MW, M2: the reference lists modes 0 to 6. The code accepts 0 to 15 and has no end-of-string check (gateway.asm:4466-4471).
6. RR: the reference lists codes 0 to 12. The code accepts 0 to 255 and has no end-of-string check (gateway.asm:5287-5292).
7. CL: the reference names only a non-numeric clear. CL=0 keeps 0 % active (gateway.asm:4454-4462).
8. MM and VS: a number above 255 also clears, because GetDecimal's OR takes the clear branch (gateway.asm:4241-4243, gateway.asm:4794-4796).
9. SB=0 gives SE (gateway.asm:4158-4159). The reference does not say so.
10. DP: the reference says `DP=00` disables. Any address with low byte 00 disables (gateway.asm:1949-1951). Reports also follow each command, not only each message (gateway.asm:1438).
11. FS: the reference says `FS=1` (default) means fall back to monitor mode after a watchdog reset. The code does not confirm this. The boot-time test appears to read the wrong RAM bank. If it read `conf`, the polarity would be the reverse. See b.10.
12. RT is a 6.8 command (gateway.asm:3749, gateway.asm:4300-4309). The reference does not list it.
13. HW: the reference says "any other single character" hands control to the thermostat. That matches the code. The reply is then `HW: A`, whatever character was sent (gateway.asm:2254-2259, tests/dhw-3.stc).
14. SR with one byte (`SR=70:14`): the code puts that byte in the low byte and 0 in the high byte (gateway.asm:4823, gateway.asm:4841). The reference does not say which byte.

### b.14 Not found in the code / unclear

- C2 high byte in MsgID 8: the code appears to send 8 as the integer part instead of the C2 integer part (gateway.asm:2935-2938). No test checks the MsgID 8 frame. Unclear whether this is a bug or something not found in the code.
- FS after a watchdog reset: the test at gateway.asm:1072 runs with bank 1 selected (gateway.asm:1050), while `conf` is in bank 0 (gateway.asm:516). If it did read `conf`, the polarity would be the reverse (gateway.asm:1070-1073, gateway.asm:5296-5299). No test covers it. Unclear.
- Which bank-1 byte sits at the offset of `conf`: not found in the code. It depends on the linker map.
- UI=0 and KI=0 act on a stale `float1` (gateway.asm:4893-4894, gateway.asm:4912-4913). The effect depends on earlier commands. Unclear.
- MH, MW and M2 may start a spoofed DHW push through `dhwpushpending` (gateway.asm:3233-3238, gateway.asm:3285-3287). Not covered by a test. Unclear.
- Whether the `GW: ` echo reaches the host before the `GW=R` reset: not found in the code. The echo is queued (gateway.asm:3724-3731) and the reset follows at once (gateway.asm:4780).
- The RS response handling (what the PIC does with the boiler's answer to the MsgID 116 to 123 WRITE): no handling specific to RS was found in the code. These IDs go through WordResponse (gateway.asm:2657-2664).
- An expiry or vigilance timer for TT, TC, BS, RT, OT, MM, VS, CL: not found in the code. Only SC, CS and C2 expire (gateway.asm:1623-1646).
- A lower or upper limit for SB beyond "not zero, integer part at most 255": not found in the code.
- Whether OT values with an integer part of 128 to 255 (no sign) are meant to be rejected: not found in the code. They are stored with bit 7 set (gateway.asm:4259-4261).
- The exact reply for an MI value that only overflows in the final increments (1278, 1279) comes from code reading. No test covers it.
- Replies marked "format from gateway.asm" were derived from the print routine. No test checks them: SB, RT, OT, CL, CE, MM, SH, SW, MW, M2, SC, RR, PM, AA, DA, UI, KI, CR, DP, TS, OH, FS.

## (c) PR= reports and the PS= summary

### PR= parsing

1. `PR` is dispatched through `SerialCmd02` to `ReportSetting`, which jumps to `PrintSetting` (`gateway.asm:3745`, `gateway.asm:3807-3810`, `gateway.asm:4809`).
2. The command must be exactly 4 characters (`PR=x`), else the reply is `SE` (`gateway.asm:2154-2157`).
3. The argument must be an uppercase `A`..`Z`, else `BV` (`gateway.asm:2158-2162`).
4. An indexed jump selects the letter (`gateway.asm:2163-2189`). Letters H, J, K, U, X, Y, Z answer `BV` (`gateway.asm:2171`, `gateway.asm:2173-2174`, `gateway.asm:2184`, `gateway.asm:2187-2189`). EEPROM codes: `SE`, `BV` and the others are defined at `gateway.asm:5492-5498`.
5. Most handlers first print `<letter>=` via `PrintSettingID` (`gateway.asm:2191-2194`). Letters A, B, C and L instead return an EEPROM string that already begins with `A=`, `B=`, `C=`, `L=` (`gateway.asm:2164-2166`, `gateway.asm:2175`).

So every reply has the form `PR: <letter>=<value>` CR LF. Tests confirm the shape: `PR: Q=P` (`tests/reset-1.stc:12`), `PR: M=G` (`tests/command-1.stc:14`), `PR: N=1000` (`tests/interval-2.stc:19`), `PR: T=11` (`tests/serial-2.stc:11`), `PR: V=5` (`tests/serial-2.stc:30`), `PR: R=I` / `PR: R=S` (`tests/thermostat-6.stc:10`, `tests/thermostat-6.stc:20`).

### PR= letters

| Letter | Exact reply value | Source of the value | Citation |
|---|---|---|---|
| A | `A=OpenTherm Gateway 6.8` | EEPROM string `"A="` runs on into `GreetingStr` = `"OpenTherm Gateway ", version`. `phase`/`patch`/`bugfix` are appended only in a debug build or when `patch` is defined; in 6.8 `patch` and `bugfix` are commented out. | `gateway.asm:5476-5488`, `gateway.asm:6-9` |
| B | `B=<tstamp>` | EEPROM `"B="` then `TimeStamp de tstamp`. `tstamp` comes from the generated `build.asm`; its format is not in gateway.asm. | `gateway.asm:5504-5505`, `gateway.asm:10` |
| C | `C=4 MHz` | EEPROM `"C="` then `SpeedString` = `mhzstr, " MHz"`, `mhzstr` = `"4"`. | `gateway.asm:5506-5507`, `gateway.asm:186` |
| D | `D=O` or `D=R` | `TempSensorFunc` (conf bit 2): `O` outside temperature, `R` return water (set by `TS=`). | `gateway.asm:2310-2314`, `gateway.asm:518`, `gateway.asm:4782-4792` |
| E | `E=-` or `E=<temperature>` | `-` when no external sensor is configured or the last sensor reading was invalid. Otherwise the value in summary slot 15 (MsgID 27, printed signed) or slot 16 (MsgID 28, printed unsigned), chosen by `TempSensorFunc`. The sensor writes those slots, but `StoreValue` also writes them from OT traffic for MsgID 27/28. | `gateway.asm:2316-2324`, `gateway.asm:1968-1999`, `gateway.asm:4269-4285`, `gateway.asm:568-571` |
| F | `F=0` or `F=1` | Reads configuration word 2 (0x8008) from config space; `1` when the LVP bit (bit 5 of the high byte) is set. Interrupts are disabled for the read. | `gateway.asm:2326-2343` |
| G | `G=<hex><hex>` | `swapf GPIOFunction` then `PrintHex`: first digit = GPIO A function (low nibble), second = GPIO B function (high nibble). | `gateway.asm:2298-2300`, `gateway.asm:4041-4057`, `gateway.asm:5323-5330` |
| I | `I=<a><b>` | Two `0`/`1` digits: first RA6 (GPIO A), then RA7 (GPIO B), read live from PORTA. `PrintBoolean` prints the carry as a digit. | `gateway.asm:2291-2296`, `gateway.asm:2032-2033`, `gateway.asm:477-478` |
| L | `L=FXOMPC` (default) | EEPROM `"L="` is followed directly by the six stored LED function bytes `FunctionLED1..6` and a NUL. The reply shows the stored letters (`LA=`..`LF=` update them). | `gateway.asm:5462-5469`, `gateway.asm:3986-3989` |
| M | `M=G` or `M=M` | `MonitorMode` flag: `G` gateway, `M` monitor. | `gateway.asm:2228-2232` |
| N | `N=<ms>` | `MsgInterval` (units of 5 ms). Prints `MsgInterval>>1` in decimal followed by `0` or `5`, which is `MsgInterval x 5` ms. Default 200 -> `1000`. | `gateway.asm:2234-2244`, `gateway.asm:5474`, `tests/interval-2.stc:19` |
| O | `O=N`, `O=n`, or `O=<C|T|c|t><float>` | `N`: no override flags. `n`: override flags set but setpoint is 0 (waiting to clear). Else type letter `T` (ProgChangePrio set) or `C`, lowercase while the thermostat has not yet accepted the override (`OverrideSet` clear), then the setpoint as a float. The handler also rewrites the lower nibble of `RemOverrideFunc` when `OverrideFunc` is clear. | `gateway.asm:2201-2226` |
| P | `P=Low power`, `P=Medium power`, `P=High power` | `TransmitMode` clear -> `Low power`; else `Medium`, or `High` when `ReceiveModeT` is also set. `"Low"` has no NUL, so it runs into `" power"`. | `gateway.asm:2261-2268`, `gateway.asm:5499-5502` |
| Q | `Q=<char>` | Reset reason: high nibble of `mode` indexes `ResetReasonStr`: `W` watchdog, `B` brown-out, `P` power on, `S` BREAK on serial, `C` command `GW=R`, `O` stack overflow, `U` stack underflow, `L` stuck in a loop, `E` external reset. See (g) for how the index is set. | `gateway.asm:2246-2251`, `gateway.asm:5508-5516`, `tests/reset-1.stc:12`, `tests/reset-2.stc:15`, `tests/reset-3.stc:18`, `tests/reset-4.stc:13`, `tests/reset-5.stc:20` |
| R | `R=D`, `S`, `R`, `C` or `I` | Thermostat model flags, later tests win: `D` default (auto, nothing detected), `S` manual standard, `R` Remeha detected, `C` Celcia 20, `I` iSense. | `gateway.asm:2270-2280`, `gateway.asm:481-486` |
| S | `S=<float>` | Setback temperature read from EEPROM `AwaySetpoint` (default 16, 0 -> `16.00`). | `gateway.asm:2302-2308`, `gateway.asm:5461` |
| T | `T=<it><oh>` | Two `0`/`1` digits: `IgnoreErr1` (settings bit 5, `IT=`), then `OverrideHigh` (settings bit 6, `OH=`). EEPROM default sets both -> `T=11`. | `gateway.asm:2282-2289`, `gateway.asm:510-512`, `gateway.asm:5459`, `tests/serial-2.stc:11`, `tests/serial-2.stc:26` |
| V | `V=<hex digit>` | `((settings >> 1) - 6) & 0x0F`. Default settings low 5 bits = 23 gives `5`. | `gateway.asm:2196-2199`, `gateway.asm:5459`, `tests/serial-2.stc:30` |
| W | `W=0`, `W=1`, `W=A` | `A` when `HotWaterSwitch` is clear (thermostat controls DHW); else `1` if `HotWaterEnable`, else `0`. | `gateway.asm:2253-2259`, `gateway.asm:520-521` |

The comment block at the top of the file lists only A, B, C, G, I, L, M, O, P, R, S, T, V, W (`gateway.asm:102-116`). D, E, F, N and Q exist in the jump table but are not in that list (`gateway.asm:2167-2169`, `gateway.asm:2177`, `gateway.asm:2180`).

### PS=0 / PS=1

**Command parsing.** `PS` goes to `ReportSummary` -> `PrintSummary` (`gateway.asm:3746`, `gateway.asm:3816-3819`, `gateway.asm:4811`). Length must be 4, else `SE`; the argument must be `0` or `1`, else `BV` (`gateway.asm:2345-2353`). The reply is `PS: 0` or `PS: 1` (`gateway.asm:2362`, `tests/summary-1.stc:44`).

**What PS=1 switches.**
1. `valueindex` is cleared, `SummaryRequest` and `HideReports` are set (`gateway.asm:2356-2358`).
2. `HideReports` suppresses every `PrintMessage` line: the `T`/`B` received-message lines, the `R`/`A` modified-message lines, and the `E` frame dump after an error (`gateway.asm:1922-1926`, `gateway.asm:2495-2500`, `gateway.asm:1945-1947`).
3. It does not suppress `Error NN`, power level, thermostat (dis)connect lines or command replies; those are printed without a `HideReports` test (`gateway.asm:1938-1944`, `gateway.asm:1548-1549`, `gateway.asm:1763-1764`, `gateway.asm:1777-1778`).

**What PS=0 switches.** `SummaryRequest` and `HideReports` are cleared (`gateway.asm:2360-2361`), so message lines return (`tests/summary-1.stc:47-50`).

**When the summary is printed (time order).**
1. The command reply `PS: 1` and its CR LF are printed first (`gateway.asm:2362`, `gateway.asm:1433-1436`).
2. On later main loop passes, while `SummaryRequest` is set, `SummaryReport` runs (`gateway.asm:1400-1401`).
3. Each pass prints one field, but only when at least 20 bytes are free in the 80-byte transmit buffer (`gateway.asm:1648-1652`, `gateway.asm:581`).
4. After each field except the last a `,` is printed (`gateway.asm:1653-1659`).
5. After field 34 (`SUMMARYFIELDS`), `SummaryRequest` is cleared and CR LF is printed (`gateway.asm:1660-1662`, `gateway.asm:3600`).
6. `HideReports` stays set. The summary is printed once per `PS=1`; a new summary needs another `PS=1`. The header comment describes this as "only print a summary line (1) on request" (`gateway.asm:117-118`).

**Field order and format.** Field n is the stored value of the n-th MsgID in `StoreProcTable`, printed with that row's routine (`gateway.asm:1968-2016`). Values are stored per MsgID by `StoreValue` using `StoreFlagTable`/`StoreOffsTable` (`gateway.asm:3541-3638`).

| # | MsgID | Format routine | Output |
|---|---|---|---|
| 1 | 0 | PrintStatus | `hhhhhhhh/llllllll` binary, MSB first |
| 2 | 1 | PrintFloat | f8.8 unsigned, 2 decimals |
| 3 | 6 | PrintStatus | binary/binary |
| 4 | 7 | PrintFloat | |
| 5 | 8 | PrintFloat | |
| 6 | 14 | PrintFloat | |
| 7 | 15 | PrintBytes | `hb/lb` decimal |
| 8 | 16 | PrintFloat | |
| 9 | 17 | PrintFloat | |
| 10 | 18 | PrintFloat | |
| 11 | 19 | PrintFloat | |
| 12 | 23 | PrintFloat | |
| 13 | 24 | PrintFloat | |
| 14 | 25 | PrintFloat | |
| 15 | 26 | PrintFloat | |
| 16 | 27 | PrintSigned | f8.8 signed, `-` prefix |
| 17 | 28 | PrintFloat | |
| 18 | 31 | PrintFloat | |
| 19 | 33 | PrintSigShort | signed 16-bit decimal |
| 20 | 48 | PrintBytes | `hb/lb` |
| 21 | 49 | PrintBytes | `hb/lb` |
| 22 | 56 | PrintFloat | |
| 23 | 57 | PrintFloat | |
| 24 | 70 | PrintStatus | binary/binary |
| 25 | 71 | PrintLowByte | low byte decimal |
| 26 | 77 | PrintLowByte | low byte decimal |
| 27-34 | 116..123 | PrintShort | unsigned 16-bit decimal |

Number formats:
- PrintStatus: two 8-bit binary strings joined by `/` (`gateway.asm:2018-2033`).
- PrintFloat: integer part in decimal without leading zeros, `.`, then two fraction digits = high byte of `lowbyte x 100`, rounded up when bit 7 of the product low byte is set. A low byte of 0xFF first rounds the integer up (`gateway.asm:2046-2064`, `gateway.asm:2085-2101`, `gateway.asm:2364-2380`). The value is not treated as signed.
- PrintSigned: two's-complement negate and print `-` when bit 7 of the high byte is set, then PrintFloat (`gateway.asm:2035-2044`).
- PrintBytes / PrintLowByte / PrintByte: decimal bytes, no leading zeros (`gateway.asm:2066-2101`).
- PrintShort / PrintSigShort: 16-bit binary to BCD, leading zeros suppressed, `0` printed for zero (`gateway.asm:2106-2152`).

The test confirms the full line, 34 comma-separated fields, for example `00000010/00001100,10.00,...,-3.30,...,-28,60/40,...,136` (`tests/summary-1.stc:45`).

**Where the stored values come from.**
- Thermostat requests with the Write bit set (`byte1` bit 4) (`gateway.asm:2388-2389`).
- Boiler responses with bit 4 clear, that is Read-Ack (`gateway.asm:2439-2440`).
- Write-Data messages the gateway generates in stand-alone mode (`gateway.asm:1811-1812`).
- `StoreValue` skips MsgIDs 128-255 and any message with `byte1` bit 5 set (Invalid-Data, Unknown-DataID, Data-Invalid) (`gateway.asm:3602-3605`).
- The DS18x20 sensor writes slot 15 or 16 (`gateway.asm:4269-4285`).
- Slots never filled read as zero, because bank 2 (where `valuestorage` lives) is cleared at start-up (`gateway.asm:565-567`, `gateway.asm:1183-1185`).

**Overridden writes.**
- In gateway mode, `StoreValue` runs on the thermostat request before `TreatMessage` changes it (`gateway.asm:2388-2398`). The boiler's Write-Ack is not stored (`gateway.asm:2439-2440`).
- So for written IDs such as MsgID 1, 16 and 24, the summary shows the thermostat's original value, even when `CS=`, `BS=` or `RT=` changed what went to the boiler.
- In stand-alone mode, `StoreValue` runs after `CreateRequest`, so the gateway's own modified write is stored (`gateway.asm:1808-1812`).

**Interleaving.** The summary goes out one field per main-loop pass (`gateway.asm:1648-1659`). Command replies, `Error NN` and power lines are printed in the same loop (`gateway.asm:1380-1381`, `gateway.asm:1594-1596`, `gateway.asm:1426-1436`), so they can land in the middle of the summary line. This is an inference from the loop structure; no test covers it.

### Not found in the code / unclear
- Format of `tstamp` (PR=B): defined in the generated `build.asm`, not in gateway.asm.
- PR=E reads summary slots that OT traffic for MsgID 27/28 also overwrites. Whether that is intended is not stated in the code.
- No end-of-summary marker other than CR LF. No header or field names are printed.

---

## (d) Modes, alternative slots, PM=, boiler store, Data-Invalid vs Unknown-DataID, blacklisting

### d.1 Gateway mode, monitor mode and GW=

**Hardware difference.**
- Comparator 1 receives thermostat requests and comparator 2 receives boiler answers. In monitor mode the comparator outputs drive the opposite interface directly (gateway.asm:150-153).
- The only difference between the two comparator settings is bit 5 `CxOE`, output on the pin: `MONITORMODE`=b'10100110', `GATEWAYMODE`=b'10000110' (gateway.asm:215-225).
- `SetupCompModule` writes the setting into both comparators (gateway.asm:4753-4774).

**Which mode at boot.**
- Monitor mode is chosen when `MonModeSwitch` (`conf` bit 0, inverse logic) is set, or when `FailSafeMode` is active (gateway.asm:4753-4756, 517).
- `conf` is read from EEPROM at boot (gateway.asm:1211-1216). Its default 0 means gateway mode (gateway.asm:5471).
- At every reset `FailSafeMode` is first set (gateway.asm:1070). It is cleared again unless the reset was a watchdog time-out. After a watchdog time-out it is cleared only when `conf` bit 6 (`NoFailSafe`) is 0 (gateway.asm:1071-1073, 522). At that point `conf` still holds its value from before the reset; it is reloaded from EEPROM later (gateway.asm:1211-1213). FS=0 sets `NoFailSafe`, FS=1 clears it (`SetFailSafety` gateway.asm:5294-5301). Read literally, FS=0 keeps fail-safe after a watchdog reset and FS=1 drops it. That conflicts with the bit name and the comment at gateway.asm:1072; see the unclear list.
- In fail-safe, `FailSafeMode` is cleared again when `NoThermostat` is set (gateway.asm:1170-1175). That flag also still holds its value from before the reset: RAM is cleared only at gateway.asm:1178-1188.
- While `FailSafeMode` is on, the main loop skips all OpenTherm processing (gateway.asm:1373-1374). The header describes this as falling back to monitor mode on a watchdog timeout (gateway.asm:29-30).

**What still runs in monitor mode.**
- `OpenThermMsg` prints the `T`/`B` line and runs `HandleMessage` on every frame (gateway.asm:1489-1491). So the value store, the strike counter, `MandatoryID` and the slot state machine are still updated.
- The routine then jumps to `MonModeCommOK` and transmits nothing (gateway.asm:1492-1493, 1503-1504).
- `SetParity` returns before printing an `R`/`A` line (gateway.asm:2495-2496).
- Stand-alone frames are not generated (gateway.asm:1797-1799).
- Smart Power has separate monitor handling (gateway.asm:923-924, 939-954).

**GW= command** (`SetGatewayMode` gateway.asm:4729-4747). The command dispatch is at gateway.asm:3879-3885.
1. Anything other than 4 characters gives `SE` (gateway.asm:4729-4732).
2. A non-boolean argument goes to `ResetGateway`. `GW=R` executes `reset`; anything else gives `BV` (gateway.asm:4733-4735, 4776-4780, 4072-4082). Reset reason `C` stands for "By command (GW=R)" (gateway.asm:5512).
3. `GW=0`/`GW=1` set `MonModeSwitch` (gateway.asm:4736-4737).
4. If the requested mode equals the current mode, only the EEPROM is updated (gateway.asm:4738-4741, 4746).
5. Otherwise `ChangeMode` is set and `SetMonitorMode` is tried at once (gateway.asm:4742-4743). While Timer 2 runs, so while a frame is on the line, it refuses and returns `P`. That `P` replaces the argument in the echo, giving `GW: P` (gateway.asm:4744-4745, 4749-4752). `IdleTimer` retries on every Timer 0 overflow while `ChangeMode` is set (gateway.asm:1579-1589).
6. The mode is always saved in EEPROM (`SaveConfig`, gateway.asm:4746, 4617-4620). The reply echoes character 3 of the buffer (gateway.asm:4747, 4966-4967); example tests/command-1.stc:8-10.
7. Apart from the comparators, `ChangeMode` and `conf`, nothing is reset by `SetGatewayMode`/`SetupCompModule` (gateway.asm:4729-4774).

`PR=M` reports `G` for gateway mode and `M` for monitor mode (gateway.asm:2228-2232); example tests/command-1.stc:12-14.

### d.2 Alternative slots: when a slot is used

A "slot" is a request to the boiler that the PIC fills with a message of its own choice. A slot is opened in two cases:

1. **Blacklisted thermostat request.** After `TreatMessage` on a thermostat request, `UnknownMask` checks the DataID. If its 2-bit counter is 3 (Z=1), `SendAltRequest` runs instead of the relay (gateway.asm:2398-2403, 3519-3538). `MandatoryID` runs inside `TreatMessage` before this check, so mandatory IDs never open a slot (d.8).
2. **Stand-alone user slot.** `MessageTable` has 16 entries. Three of them are `UserMsg`, which sets `SendUserMessage` and returns MsgID 0 (gateway.asm:1822-1842). With `SendUserMessage` set, the `UnknownMask` call is skipped. Z is still 1 from `clrf boilercom` (gateway.asm:2399-2403).

`SendAltRequest` (gateway.asm:2408-2418) does this:
- it saves the original request when this is a user slot;
- it sets `AlternativeUsed` and clears `OverrideUsed`;
- it calls `Alternative` to pick the DataID.

If `Alternative` finds nothing, `RestoreRequest` puts back the original type, data and DataID and clears `AlternativeUsed` (gateway.asm:3490-3500). The original request then goes to the boiler as if it were not blacklisted (`SendDefaultMsg`, gateway.asm:2413-2414, 2404-2407). In a user slot that is the MsgID 0 status request (gateway.asm:1841-1842).

When a candidate is found, its DataID is put in byte 2 and `TreatMessage` treats it as a request (gateway.asm:2415-2418). So per-ID rules apply to slot frames, and several handlers turn a slot frame into a WRITE:
- 1 (gateway.asm:2807-2809)
- 7 with CL= (gateway.asm:2920-2927)
- 8 (gateway.asm:2931-2933)
- 14 (gateway.asm:2968-2970)
- 24 with RT= (gateway.asm:3061-3068)
- 27 with OT= or sensor (gateway.asm:3071-3088)
- 56/57 with a pending write (gateway.asm:3137-3148, 3173-3184)
- 71 (gateway.asm:3210-3217)
- 99 with a pending mode write (gateway.asm:3252-3256)
- TSP writes (gateway.asm:3380-3388)

`Alternative` starts with a READ with data 0 (gateway.asm:3418-3421).

### d.3 The alternative message table and its order

`Alternative` (gateway.asm:3418-3500) chooses in this fixed priority order:

1. **Admin messages**, while any `initflags` bit is set (gateway.asm:3422-3436, `AdminMessages` gateway.asm:3438-3462). The order is:
   1. MsgID 6 (`INITRP`)
   2. MsgID 48 (`INITHW`)
   3. MsgID 49 (`INITCH`)
   4. MsgID 56 as WRITE with SW= (`INITSW`)
   5. MsgID 57 as WRITE with SH= (`INITSH`)
   6. MsgID 99 (`INITOM`)
   7. MsgID 4 as WRITE with `requestcode` (`INITRR`, set by RR=, gateway.asm:5287-5292)

   An admin ID known to be unsupported is skipped and its flag cleared (gateway.asm:3429-3434). At boot `INITRP`, `INITHW`, `INITCH` and `INITOM` are set (gateway.asm:1270-1273). `INITSW`/`INITSH` are set when the EEPROM holds an SW/SH value (gateway.asm:1218-1229). Each flag is cleared by the matching answer (gateway.asm:2915, 3113, 3125, 3149, 3185, 3245, 2872). Example: with UI=48 in stand-alone mode, `R00060000` and `R80310000` appear but MsgID 48 is not sent (tests/standalone-5.stc:9-15).
2. **The priority message (PM=)**, when `PriorityMsg` is set (gateway.asm:3464-3470; see d.6).
3. **Counter resets (RS=)**, one per slot, WRITE to the first DataID 116-123 whose reset flag is set (gateway.asm:3471-3474, 3502-3517).
4. **The EEPROM alternative list**, when `NoAlternative` is clear (gateway.asm:3475-3489).
   - The list has 32 one-byte slots at `AlternativeCmd`. The default is 116, 117, 118, 119, 120, 121, 122, 123 followed by 24 empty (0) slots (gateway.asm:5520-5524).
   - The search is round robin from `alternativecmd` (5-bit index), skipping empty slots, for at most 32 steps (gateway.asm:3477-3488).
   - The pointer advances one slot only when the previous modified exchange got an answer: `AlternativeDone` is cleared when a slot frame is sent (gateway.asm:2415) and set in `SendAltResponse` (gateway.asm:2468, 3479-3480). Without an answer the same slot entry is retried.
   - When all 32 slots are empty, `NoAlternative` is set (gateway.asm:3489). It stays set until an AA= (gateway.asm:4667). `alternativecmd` is complemented at boot to compensate for the first increment (gateway.asm:1269).

**AA=** (`SetAlternative` gateway.asm:4659-4678):
- AA=0 gives `OR`; trailing characters give `SE`.
- The ID goes into the first empty EEPROM slot, so the list survives a reset. The same ID can be added more than once: no duplicate check was found in gateway.asm:4667-4677.
- When the list is full the reply is `NS`. On success the ID is echoed.
- Dispatch: gateway.asm:3783-3789.

**DA=** (`DelAlternative`/`DeleteAlt` gateway.asm:4680-4703):
- DA=0 gives `OR`.
- The first slot holding the ID is set to 0 in EEPROM. The reply is `NF` when the ID is not in the list.
- The test setup removes the default list with DA=116..123 (tests/setup.stc:85-95).

`DeleteAlt` also runs automatically: an ID that is already at 3 strikes and is answered Unknown-DataID again is removed from the EEPROM list (gateway.asm:2450-2451; see d.7).

The answer the thermostat gets for a request replaced by a slot frame is described in d.6.1 and d.7.

### d.4 SR= / CR= answer table

- SR= stores a fixed answer for a DataID in RAM: 16 entries, with DataID, HB and LB in `ResponseValues1/2/3` (gateway.asm:573-576, `SetResponse` gateway.asm:4813-4859, `FindResponse` gateway.asm:4875-4888).
- SR=0 gives `BV`. SR= with one byte sets the HB to 0 (gateway.asm:4823, 4841). The reply is `NS` when the table is full (gateway.asm:4846-4849).
- Bank 3, where the table lives, is cleared at boot, so SR= does not survive a reset (gateway.asm:1186-1188).
- CR= frees the entry; the reply is `NF` when there is none (gateway.asm:4861-4873).

The entry is applied in `WordResponse`/`ByteResponse` to answers bound for the thermostat (gateway.asm:3640-3668):
- a relayed boiler answer, when the request was not modified;
- or a synthesised answer (situation 3).

The type becomes the ack of the original request, and the data come from the entry. So an SR= entry overrides even a valid boiler answer for any DataID whose handler ends in `WordResponse`/`ByteResponse`. Examples are tests/thermostat-2.stc:9-15.

### d.5 The boiler value store

- `valuestorage` holds 2 bytes for each of 34 DataIDs (gateway.asm:565-571, 3600).
- The IDs are fixed by `StoreFlagTable` and `StoreOffsTable`: 0, 1, 6, 7, 8, 14, 15, 16, 17, 18, 19, 23, 24, 25, 26, 27, 28, 31, 33, 48, 49, 56, 57, 70, 71, 77 and 116-123 (gateway.asm:3540-3600). The comment says the three tables must be kept in sync (gateway.asm:3562).

The store is written by `StoreValue` (gateway.asm:3602-3638), only for IDs 0-127 and only for frames with bit 5 clear (gateway.asm:3602-3605). The writers are:
- thermostat Write-Data requests (gateway.asm:2388-2389);
- boiler Read-Ack answers (gateway.asm:2439-2440);
- the PIC's own stand-alone Write-Data requests (gateway.asm:1811-1812);
- OT= and a DS18x20 reading, written straight into the MsgID 27 / 28 slots (gateway.asm:4266-4287, 568-571).

Readers found: the PS=1 summary line (`PrintStoredVal` gateway.asm:1968-1984, `SummaryReport` gateway.asm:1648-1662), the PR=E sensor report (gateway.asm:2316-2324) and the MsgID 28 sensor answer (gateway.asm:3103-3108). PS=1 also suppresses the per-frame `T`/`B`/`R`/`A` lines (`HideReports`, gateway.asm:2345-2362, 1925-1926).

Not found in the code: any path that answers the thermostat from `valuestorage`. A thermostat request is always relayed to the boiler, or replaced by a slot frame. The thermostat gets the boiler's answer, a synthesised answer, or an SR= entry. The only `valuestorage` references are gateway.asm:567-571, 1971, 2321-2323, 3103-3108, 3629 and 4273-4279.

### d.6 PM= (priority message)

- `SetPrioMessage` stores the DataID in `prioritymsgid`, sets `PriorityMsg`, clears `PrioMsgWrite` and resets `TSPCount`/`TSPIndex` (gateway.asm:5026-5037). Dispatch is at gateway.asm:3951-3957.
- Any value 0-255 accepted by `GetDecimal` is taken. Values above 255 give `OR` (gateway.asm:5044-5068). No further range check was found.

The priority message only goes out in a slot. Before it can go out, three things must hold:
- a slot opens (d.2);
- all admin `initflags` must be clear (gateway.asm:3422-3424);
- `PriorityMsg` must be set (gateway.asm:3464-3465).

It is sent as READ, with byte 3 = `TSPIndex` (0 after PM=) and DataID `prioritymsgid` (gateway.asm:3418-3421, 3464-3470). Without blacklisted thermostat requests and without stand-alone user slots, a PM= request is never sent: `Alternative` is only called from `SendAltRequest` (gateway.asm:2412).

`SendPriorityMsg` sets `INITNR`, so a successful answer does not reset the strike counter of that ID (gateway.asm:3466, 2453-2456). An Unknown-DataID answer is still counted (gateway.asm:2441-2449).

`PriorityMsg` is cleared by any boiler answer carrying `prioritymsgid` (gateway.asm:2435-2438). Without an answer it stays pending and is sent again at the next slot.

**Follow-up reads.**
- For a TSP/FHB size ID (10, 12, 88, 90, 105, 107) the count in the answer schedules priority reads of the next DataID, one entry per slot (gateway.asm:3364-3378, 3389-3397).
- For strings (93-95) one read per character follows (gateway.asm:3399-3407).
- Example: PM=10 gives `R000A0000`, `R800B0000`, `R000B0100`, `R000B0200` and nothing more (tests/tsp-1.stc:4, 12-19).

**Other users of the same mechanism.**
- A new boiler fault schedules MsgID 5 (gateway.asm:2759-2767).
- An operating-mode or DHW-push change schedules a MsgID 99 WRITE, unless MsgID 99 is blacklisted (`PushOperModes` gateway.asm:3281-3288).
- TP= reads and writes (gateway.asm:4496-4559).

One `prioritymsgid` exists (gateway.asm:358). A later scheduler overwrites an earlier pending one; read from code only.

#### d.6.1 Data-Invalid vs Unknown-DataID: which answer the PIC gives

**The PIC answers Unknown-DataID (0x70)** when:
- the thermostat's request had its type changed by a handler (`setbyte1`), or was replaced by a slot frame (`OverrideUsed`=0); and
- the handler of the original DataID and SR= leave the synthesised answer alone in situation 3.

The answer carries the thermostat's own DataID and data (gateway.asm:2471-2477, 3492-3500). For a slot frame this happens whatever the boiler answered to the slot frame. So for a blacklisted ID without a special handler or SR= entry, the thermostat gets Unknown-DataID.

**Synthesised Unknown-DataID converted to an Ack:**
- MsgID 2 (gateway.asm:2837-2844)
- 6 (gateway.asm:2900-2918)
- 9 and 100 while an override is requested, or because bit 5 is set (gateway.asm:2941-2947, 3293-3299)
- 16 for a WRITE (gateway.asm:2984-2987)
- 20 with a valid clock, or for a WRITE (gateway.asm:3048-3056, 2694-2706)
- 27 with OT= or a valid sensor (gateway.asm:3078-3088)
- 28 with a valid sensor (gateway.asm:3095-3108)
- 56 and 57 (gateway.asm:3149-3161, 3185-3197)
- 99 (gateway.asm:3219-3231)
- 125 (gateway.asm:3327-3344)
- any ID with an SR= entry (gateway.asm:3652-3668)

**Synthesised Unknown-DataID converted to Data-Invalid (0x60):**
- MsgID 16 for a READ (gateway.asm:2696-2697)
- MsgID 20 for a READ without a valid clock (gateway.asm:2694-2697)
- MsgID 27/28 with an invalid sensor reading (gateway.asm:3089-3090, 3100-3101)

Example: tests/thermostat-2.stc:19-20 shows `A5014892A` and `A60140000` after the clock expired.

**The PIC answers an Ack where the boiler sent Data-Invalid or Unknown-DataID** when only data bytes were changed (`OverrideUsed`=1). The answer is always `0x40 | originaltype` (gateway.asm:2471-2474). Examples: MsgID 1 with CS=, MsgID 0 with a status rewrite, MsgID 14 with MM=, MsgID 71 with VS=. For MsgID 1 the CS= value is also dropped on such an answer (gateway.asm:2830-2833).

**A boiler Data-Invalid on an unmodified request** is relayed unchanged, except for the IDs whose handlers rewrite answers with bit 5 set:
- 2 (gateway.asm:2837-2844)
- 6 (gateway.asm:2900-2918)
- 9 (gateway.asm:2945-2947)
- 16 (gateway.asm:2984-2987)
- 20 (gateway.asm:2694-2706)
- 56/57: Data-Invalid clears SW/SH without rewriting the answer (gateway.asm:3162-3168, 3198-3207)
- 99 (gateway.asm:3228-3231)
- 100 (gateway.asm:3297-3299)
- 125 (gateway.asm:3329-3334)
- an SR= entry

A boiler Data-Invalid does not count as a strike; it resets the counter (gateway.asm:2442-2444, 2453-2455).

### d.7 Blacklisting: the strike rule, UI= and KI=

**The map.** `unknownmap` has 2 bits per DataID for IDs 0-127, 32 bytes (gateway.asm:541-548). `UnknownMask` returns the pointer and mask; Z=1 means both bits are set, so the ID counts as unsupported (gateway.asm:3519-3538). IDs 128-255 are never blacklisted: the mask is 0 (gateway.asm:3526-3527, 2431-2432). Bank 1, which holds the map, is cleared at boot, so learned strikes do not survive a reset (gateway.asm:1180-1182).

**The strike rule** (`HandleResponse` gateway.asm:2441-2456), applied to every boiler answer:
1. An Unknown-DataID answer (bits 5 and 4 set) with a count below 3 adds 1 to the 2-bit count (gateway.asm:2442-2449).
2. An Unknown-DataID answer when the count is already 3 removes the ID from the EEPROM alternative list (`MsgUnsupported`, gateway.asm:2445-2446, 2450-2451). This only happens when the ID itself goes out, for example as a slot frame or a priority message.
3. Any other answer (Ack or Data-Invalid) resets the count to 0, except while `INITNR` is set for a priority message (gateway.asm:2453-2456, 3466).

So 3 consecutive Unknown-DataID answers blacklist an ID (comment gateway.asm:541-547). From the next thermostat request for that ID, a slot frame is sent instead (d.2). Read from code only: the strike counter also runs in monitor mode, because `HandleMessage` runs there (gateway.asm:1489-1491).

**UI=** (`UnknownDataID` gateway.asm:4890-4907, dispatch gateway.asm:3945-3948):
- IDs with bit 7 set give `BV`; trailing characters give `SE`.
- The ID's both bits are set at once (count 3, gateway.asm:4900-4901), and its bit is set in the EEPROM `UnknownFlags` bitmap (gateway.asm:4902-4905, 5518-5519).
- At boot every flagged ID is marked unsupported again (gateway.asm:1298-1324).
- Example: tests/sensor-4.stc:16 and tests/standalone-5.stc:9.

**KI=** (`KnownDataID` gateway.asm:4909-4926, dispatch gateway.asm:3807-3813) clears both RAM bits and the EEPROM bit.

### d.8 MandatoryID

`MandatoryID` clears both strike bits of the current DataID in RAM. It does this only for requests (gateway.asm:3409-3416). The comment states the purpose: "so even (accidental) manual action cannot blacklist the DataID" (gateway.asm:3409-3410).

It runs inside `TreatMessage` for requests, before the blacklist check at gateway.asm:2401, for:
- MsgID 0 (gateway.asm:2720)
- MsgID 1 (gateway.asm:2806)
- MsgID 14 (gateway.asm:2967)
- MsgIDs 3, 17 and 25 via `MandatoryWord` (gateway.asm:2544, 2558, 2566, 3651)
- MsgID 27, but only when 27 is processed as a slot frame while an external DS18x20 outside sensor is configured (gateway.asm:3071-3075)

Effect:
- These IDs are always relayed to the boiler and never replaced by a slot frame, even after three Unknown-DataID answers or a UI=.
- `MandatoryID` does not touch the EEPROM bitmap. After UI=1, for example, the RAM bits are set again at every boot (gateway.asm:1298-1324) and cleared again on the next thermostat request for that ID (gateway.asm:3411-3416).
- `MandatoryID` runs only on requests, so an Unknown-DataID answer can still raise the count between two requests (gateway.asm:2441-2449).

### Not found in the code / unclear (d)

1. **UI=0 and KI=0.** When the value is 0, `skpz` skips `movwf float1` (gateway.asm:4893-4894, 4912-4913; `GetDecReturn` sets Z, gateway.asm:5067-5068). W stays 0, so the RAM mark or unmark applies to MsgID 0. The EEPROM update and the echo use whatever `float1` held before (gateway.asm:4900-4907, 4919-4926). For UI=0 the RAM mark is undone by `MandatoryID` on the next status request (gateway.asm:2720). The EEPROM side effect is not settled.
2. **One pending priority message.** Read from code only: later schedulers overwrite `prioritymsgid` (gateway.asm:3375-3377, 5032-5033). No queue was found.
3. **Whether a priority answer that arrives on a thermostat-initiated request clears `PriorityMsg`.** The code clears it on any boiler answer with that DataID (gateway.asm:2435-2438). Whether that is intended is not stated in the code.
4. **`AlternativeDone` for modified requests.** `AlternativeDone` is also set when only data bytes of a relayed request were changed (gateway.asm:2467-2468), not only after a slot frame. How that affects the round-robin advance in mixed traffic was not traced.
5. **A MsgID 0 slot frame after an unanswered priority frame.** Read from code only. `INITNR` is set when a priority frame goes out (gateway.asm:3466). It is cleared only by a boiler answer (gateway.asm:2456). The other writes to `initflags` keep bit 7 (gateway.asm:2909, 3433). `clrf initflags` is reached only when `initflags` is already 0 (gateway.asm:3422-3424, 3471). So if the next slot opens before any boiler answer, `initflags` is non-zero. `AdminMessages` then finds no admin flag and returns 0 (gateway.asm:3456-3458), and the slot carries READ MsgID 0 with data 0 (gateway.asm:3418-3421, 3435-3436). `readstatus` then rewrites it (gateway.asm:2720-2747). In stand-alone mode `MessagePtr` does not advance on a missing answer, so the same user slot repeats (gateway.asm:1497-1502). Not exercised by any test script that was read.
6. **Fail-safe sense.** Read literally, FS=0 keeps fail-safe after a watchdog reset and FS=1 drops it (gateway.asm:1070-1073, 5294-5301). That is the opposite of the name `NoFailSafe` and of the comment "Fail safe mode is disabled by user" (gateway.asm:522, 1072). Not checked in gpsim.
7. **PrintBytes.** The SR= reply format after `ID:` comes from `PrintBytes` (gateway.asm:4856-4859). That routine was not opened in this session.
8. **Monitor mode and slots.** `HandleMessage` runs in monitor mode, including `Alternative` for blacklisted IDs, without a frame going out (gateway.asm:1489-1493). Its effect on `alternativecmd`, `initflags` and `PriorityMsg` after a switch back to gateway mode was not traced.
9. **Not tested here.** No gpsim run was made in this session. Every claim comes from reading the code and the test scripts.

## (e) Timing

### Timers

| Timer | Configuration | Period | Use | Citation |
|---|---|---|---|---|
| Timer 0 | Fosc/4, 1:64 prescaler | 64 us per tick, 16.384 ms per overflow. Reloaded with `PRELOADT0 = 256 - 10000/64` on every line event, so it overflows about 10 ms after the last line event. | Line idle detection, error print timing, error LED time base, A/D start, BREAK hold-off | `gateway.asm:155-156`, `gateway.asm:188-203`, `gateway.asm:1077-1085`, `gateway.asm:775-776` |
| Timer 1 | Fosc/4, 1:8, 16-bit | 524.288 ms | `FlashTimer`: command expiry countdown and override LED flash | `gateway.asm:157-158`, `gateway.asm:1102-1112`, `gateway.asm:1612-1621` |
| Timer 2 | PR2 = 249, 1:1 | 250 us | OpenTherm bit timing (quarter-bit counter `quarter`) | `gateway.asm:159-164`, `gateway.asm:197-198`, `gateway.asm:1087-1100` |
| Timer 4 | 1:16, PR4 = 249, 1:5 postscaler | 20 ms | `TickTimer`; `SecCounter` = 50 gives a 1 s tick | `gateway.asm:205-209`, `gateway.asm:1246-1250`, `gateway.asm:1553-1568` |
| Timer 6 | 1:4, PR6 = 249, 1:5 postscaler, starts off | 5 ms | Message interval countdown `IntervalCounter` | `gateway.asm:1238-1244`, `gateway.asm:1570-1577` |

All timer flags except Timer 2 are polled in the main loop, not handled in the interrupt (`gateway.asm:1383-1393`). Timer 2 and both comparators are interrupt driven (`gateway.asm:638-651`).

The comment at `gateway.asm:1105` says Timer 1 blinks LEDs "approximately once per second". The constants give a 524.288 ms overflow, so one toggle per overflow, matching `gateway.asm:157-158`.

### OpenTherm line timing (interrupt level)

- A transition less than 750 us (3 quarters) after the last bit is a mid-bit transition (`gateway.asm:710-713`). A second one gives Error 01 unless `IgnoreErr1` is set (`gateway.asm:956-964`).
- During a message, no bit transition for 2 ms (`quarter` bit 3) gives Error 03 (`gateway.asm:420`, `gateway.asm:864-865`).
- Outside a message, a pulse shorter than 250 us is ignored as a spike (`gateway.asm:806-808`).
- A line level held 5 ms (20 quarters) outside a message toggles the receive inversion (Smart Power level change) (`gateway.asm:849-852`, `gateway.asm:909-919`).
- 20 ms (80 quarters) without a line event stops Timer 2 and raises `PowerReport` when a Smart Power change happened (`gateway.asm:853-862`).
- A thermostat change back to low within 2.5..12.5 ms while in High Power is taken as Medium Power (`gateway.asm:798-802`, `gateway.asm:823-837`).

### Message interval and MI=

1. Every boiler response calls `StartInterval`: `IntervalCounter = MsgInterval`, Timer 6 cleared and started (`gateway.asm:2433-2434`, `gateway.asm:2514-2520`).
2. Every 5 ms `IntervalCounter` is decremented. At zero, `TriggerMessage` sets `MsgTrigger` (`gateway.asm:1570-1577`, `gateway.asm:1565`).
3. Independently, the 1 s tick also sets `MsgTrigger` (`gateway.asm:1553-1568`).
4. `MsgTrigger` is consumed only on the stand-alone path (`ThermostatEnd`, reached when `NoThermostat` is set and the gateway is not in monitor mode) (`gateway.asm:1793-1799`). That path builds the next request, stops Timer 6 and clears `MsgTrigger` (`gateway.asm:1800-1820`).
5. `CheckThermostat`, which leads to that path, runs on A/D completion. The A/D is started at each Timer 0 overflow while Timer 2 is off (line idle) (`gateway.asm:1395-1396`, `gateway.asm:1580-1586`).

So in stand-alone mode the next request goes out at the first A/D completion after either `MsgInterval x 5 ms` since the last boiler response, or the next 1 s tick, whichever comes first. With a thermostat connected, the thermostat sets the pace and `MsgTrigger` is not used.

`MI=<ms>` (`gateway.asm:3747`, `gateway.asm:3822-3834`, `gateway.asm:4577-4615`):
- The last character must be a digit, else `SE`. A one-digit argument gives `OR`.
- The value is `round(ms / 5)`: `2 x (ms div 10)`, plus 1 if the last digit is 3..7, plus 2 if it is 8 or 9.
- A result below 20 (100 ms) gives `OR`. Doubling overflow gives `OR`.
- The value is written to EEPROM `MessageInterval` and to `MsgInterval`. The reply prints the stored value back in ms.
- Default: 200 = 1000 ms (`gateway.asm:5474`).
- Tests: `MI=99` -> `MI: 100` (`tests/interval-1.stc:10-14`); `MI=97` -> `MI: OR` (`tests/interval-2.stc:11-13`).

### Command expiry (CS=, C2=, SC=)

1. `CS=` (non-zero), `C2=` (non-zero) and `SC=` set their heartbeat flag and call `StartTimer` (`gateway.asm:4218-4229`, `gateway.asm:5272-5283`, `gateway.asm:4331-4373`). No other command sets a heartbeat flag (`gateway.asm:497-499`).
2. `StartTimer` acts only when the timer is not running (`Expired` = `minutetimer` bit 7 set). It then clears all three heartbeat flags, including the one just set, and loads `minutetimer = 120` (`gateway.asm:4233-4239`).
3. Each Timer 1 overflow (524.288 ms) while not expired runs `CommandExpiry`, which decrements `minutetimer` (`gateway.asm:1612-1614`, `gateway.asm:1623-1625`).
4. The timer expires on the 121st decrement (120 -> 0 -> 255). That is 120 to 121 x 524.288 ms after the start, about 62.9 to 63.4 s, because Timer 1 free-runs.
5. At expiry (`gateway.asm:1626-1646`):
   - No `SC=` in the window: `InvalidTime` is set, so MsgID 20 is no longer injected (`gateway.asm:3047-3056`).
   - No `CS=` in the window and the user control setpoint is 8 or higher: `SysCtrlSetpoint` is set (setpoint dropped). A setpoint below 8 is kept.
   - Same rule for `C2=` with `SysCH2Setpoint`.
   - If any heartbeat flag is set, `StartTimer` restarts the window. Otherwise the timer stays stopped.
6. Consequence: a single `CS=` expires about 63 s later. Keeping it alive needs another `CS=` inside each window. The test sends `CS=54.3`, then only `SC=`, and the `R` frames for MsgID 1 stop by 65..74 s (`tests/control-3.stc:8-21`). `CS=5` does not expire (`tests/control-4.stc:8-18`).
7. At start-up `minutetimer` is 0, so the first Timer 1 overflow runs one expiry pass with no effect beyond setting already-set flags (`gateway.asm:1178-1179`, `gateway.asm:1283-1289`).

### Retries and counters

- Unknown-DataID: a 2-bit counter per MsgID counts consecutive Unknown-DataID answers. At 3 the ID is treated as unsupported and alternatives are sent in its place. A supported answer resets the counter (`gateway.asm:541-548`, `gateway.asm:2441-2455`).
- Room setpoint override: 3 attempts before abandoning (`gateway.asm:438`, `gateway.asm:3034-3037`).
- DHW push: up to 8 attempts (3-bit counter) (`gateway.asm:489`, `gateway.asm:2790-2793`).
- Stand-alone re-send: an unanswered request is sent again with the same MsgID on the next 1 s tick, because `MessagePtr` advances only on a matching response (`gateway.asm:1494-1502`). An unanswered alternative is retried with the same entry, because `AlternativeDone` is set only when the boiler answers (`gateway.asm:2415`, `gateway.asm:2467-2468`, `gateway.asm:3479-3480`). See "Loss of the boiler".
- In gateway mode the PIC does not retransmit a frame itself. Repeats come from the thermostat.

### Loss of the thermostat

1. The thermostat line voltage is measured on AN0 whenever the line is idle (`gateway.asm:175-180`, `gateway.asm:1202-1203`, `gateway.asm:1580-1586`).
2. `CheckThermostat` ignores a reading that differs by 4 or more counts from the previous one (`gateway.asm:1732-1743`).
3. Above `VoltOpen` -> `OpenCircuit` (`HeatDemand` cleared). Below `VoltShort` -> `ShortCircuit` (`HeatDemand` set, on/off heat demand) (`gateway.asm:1744-1751`, `gateway.asm:1766-1773`). The thresholds are scaled from the FVR measurement at start-up (`gateway.asm:267-283`, `gateway.asm:1190-1200`, `gateway.asm:1844-1864`).
4. If `NoThermostat` was clear, `Disconnect` runs (`gateway.asm:1774-1792`):
   - prints `Thermostat disconnected`;
   - stops Timer 2;
   - clears the thermostat Smart Power info;
   - sets `SlaveOut` ("Output at idle level");
   - clears the auto-detected Remeha model unless it was set manually;
   - clears override and setpoint;
   - switches off LED function `P`.
5. From then on the stand-alone path sends requests from `MessageTable`: 0, 25, 1, user, 17, 27, 28, 14, user, 0, 25, 1, 56, 57, user, 18. A "user" slot sends an alternative or init message (`gateway.asm:1822-1842`). `MessagePtr` advances only when the response ID matches the request, modulo 16 (`gateway.asm:1494-1503`). Test: after disconnect, `Thermostat disconnected` and `R00060000` (`tests/standalone-1.stc:5-11`).
6. Reconnect: while `NoThermostat` is set, a stable reading inside the window and below `VoltLow` (OpenTherm low level) clears the `CONNECTMASK` flags and prints `Thermostat connected` (`gateway.asm:1752-1765`, `gateway.asm:460-462`).

### Silent thermostat (connected, but no requests)

1. Every second, `boilercom` is incremented, saturating at 255 (`gateway.asm:1559-1560`). It is cleared each time a request is built for the boiler, in `CreateRequest` (`gateway.asm:2391-2399`). It therefore counts seconds since the last request sent to the boiler.
2. At 5 s or more, `SlaveOut` (RA3) is cleared, commented "Prevent unwanted heating" (`gateway.asm:1561-1564`, `gateway.asm:587`).
   - Background: the boiler must take a low-voltage line with no valid frame for 5 s as a heat demand, and without a thermostat the default idle level is that low-voltage state (`gateway.asm:1706-1725`).
   - So this guard fires when nothing reaches the boiler for 5 s. The typical case is a thermostat that is still electrically connected but has gone silent.
3. `repeatcount` counts seconds once any boiler message has ever been received (`BoilerAlive`) (`gateway.asm:1557-1558`, `gateway.asm:2433`). It is cleared:
   - when a request has a different MsgID than the previous one (`gateway.asm:2391-2394`);
   - on every message in monitor mode;
   - on a matching response in stand-alone mode (`gateway.asm:1492-1503`).
4. When `repeatcount` reaches 64, the firmware restarts with reset reason `L` (`gateway.asm:1403-1404`, `gateway.asm:1017-1021`, `gateway.asm:5515`).
   - The test sends the same MsgID 0 every second and reads `Q=L` after 72 s (`tests/reset-4.stc:3-13`).
   - With `BoilerAlive` set and no traffic at all, nothing clears `repeatcount`, so a silent thermostat also leads to the `L` restart after 64 s. This is derived from the code; no test covers it.

### Loss of the boiler

**Gateway mode with a thermostat.** The PIC itself does not detect boiler loss.
- No response timeout or status line for a missing boiler answer was found.
- Each thermostat request is still forwarded and clears `boilercom` (`gateway.asm:2399`).
- `repeatcount` is cleared whenever the thermostat's MsgID changes (`gateway.asm:2391-2394`).

**Stand-alone mode.** Traced from the code; no test covers it.
1. `StandAloneMsg` stops Timer 6 (`gateway.asm:1815-1816`). Only `HandleResponse` restarts it (`gateway.asm:2433-2434`, `gateway.asm:2514-2520`). Without answers, only the 1 s tick sets `MsgTrigger` (`gateway.asm:1553-1568`).
2. `MessagePtr` advances only when the response ID matches `originalreq` (`gateway.asm:1494-1502`). So the same table entry is sent again every second.
3. On a fixed-ID entry, `byte2` equals `originalreq` on every send, so `repeatcount` is not cleared (`gateway.asm:2391-2394`, `gateway.asm:1813-1814`). After 64 s the gateway restarts with reason `L` (`gateway.asm:1403-1404`).
4. Edge case, a user slot (table entries 3, 8, 14; `gateway.asm:1822-1842`):
   - `SelectMessage` puts MsgID 0 in `byte2`, while `originalreq` still holds the alternative ID of the previous send (`gateway.asm:2395`, `gateway.asm:2408-2416`, `gateway.asm:1813-1814`).
   - When that alternative ID is non-zero, the compare differs, so `repeatcount` is cleared every second and no `L` restart follows.
   - When no alternative is available, `RestoreRequest` leaves MsgID 0 and `originalreq` 0 (`gateway.asm:3471-3500`), so the counter runs and `L` follows after 64 s.

### Other timeouts

- Error LED: on for `TWOSEC` = 122 Timer 0 overflows, about 2 s. Decremented on each Timer 0 overflow, so the real time stretches while the line is busy (`gateway.asm:210-213`, `gateway.asm:999-1002`, `gateway.asm:1598-1603`).
- BREAK hold-off: a BREAK is acted on only after 32 Timer 0 overflows (about 0.5 s) without a BREAK (`gateway.asm:555-560`, `gateway.asm:1010-1016`, `gateway.asm:1580-1583`).
- Boot loader window: about 1 s for a programmer `<STX>` after reset (`gateway.asm:622-626`, `selfprog.asm:18-20`, `selfprog.asm:144-151`).
- Start-up LED flash: one Timer 0 overflow, about 16 ms (`gateway.asm:1114-1119`).

### Not found in the code / unclear
- The watchdog period: gateway.asm never writes WDTCON (grep for `WDTCON` found nothing in the .asm files). It uses the device default plus configuration word 1 (`gateway.asm:15`). The value is not found in the code.
- The electrical meaning of `bcf SlaveOut` after 5 s versus `bsf SlaveOut` on thermostat disconnect (`gateway.asm:1564`, `gateway.asm:1782`).
  - The comments say the default idle OT level is the low-voltage state (`gateway.asm:1719-1720`) and call `bsf SlaveOut` "Output at idle level" (`gateway.asm:1782`).
  - Together these suggest that `bcf SlaveOut` gives the high-voltage, no-heat-demand state. This is an inference; the hardware inversion is not in the code.
- No explicit "boiler not responding" status line or flag was found. Only the 64 s `L` restart in stand-alone mode.

---

## (f) Error and status lines

All lines end with CR LF (`gateway.asm:1877-1881`).

| Line | Trigger | Citation |
|---|---|---|
| `OpenTherm Gateway 6.8` | Start-up, after RAM clear and EEPROM restore (also `PR=A` with `A=` prefix) | `gateway.asm:1326-1327`, `gateway.asm:5477-5488`, `tests/serial-2.stc:22` |
| `WDT reset!` | Start-up after a watchdog reset, only when fail-safe mode is entered (see (g)) | `gateway.asm:1170-1173`, `gateway.asm:5489` |
| `Thermostat disconnected` | Stable thermostat line voltage above `VoltOpen` or below `VoltShort` while a thermostat was considered present | `gateway.asm:1771-1778`, `gateway.asm:5490`, `tests/standalone-1.stc:10` |
| `Thermostat connected` | Stable OpenTherm low level while `NoThermostat` is set | `gateway.asm:1757-1764`, `gateway.asm:5491` |
| `Low power` / `Medium power` / `High power` | Smart Power level changed (see below) | `gateway.asm:1532-1551`, `gateway.asm:2262-2268`, `gateway.asm:5499-5502`, `tests/thermostat-3.stc:12-26` |
| `Error 01` .. `Error 04` | OpenTherm receive error (see below) | `gateway.asm:1938-1944`, `gateway.asm:5503` |
| `E` + 8 hex digits | After `Error 02` and `Error 04` only: the complete received frame. Both errors are raised at the stop bit, after all 32 data bits are in. Errors 01 and 03 happen mid-frame, so no frame is printed for them. | `gateway.asm:1945-1947`, `gateway.asm:715-720` |
| `T`/`B` + 8 hex | Received request (thermostat) / response (boiler) | `gateway.asm:1922-1936`, `gateway.asm:1489` |
| `R`/`A` + 8 hex | Modified request to the boiler / modified answer to the thermostat (gateway mode only) | `gateway.asm:2495-2500`, `gateway.asm:35-39` |
| `<H><LL>=<VV>` | Debug pointer dump after every OT message and command when `DP=` is set | `gateway.asm:1949-1966`, `gateway.asm:1529`, `gateway.asm:1438` |
| `XX: <result>` | Command reply; error codes `NG`, `SE`, `NS`, `BV`, `NF`, `OR`, `OE` | `gateway.asm:3723-3731`, `gateway.asm:5492-5498` |

### Error lines in detail

Detection (interrupt level):
- Error 01: more than one mid-bit transition in a bit, unless `IgnoreErr1` (`IT=1`, the EEPROM default) (`gateway.asm:956-964`, `gateway.asm:5459`).
- Error 02: stop bit not logical 1 (`gateway.asm:715-718`, `gateway.asm:966-968`).
- Error 03: a bit not received within 2 ms (`gateway.asm:864-865`, `gateway.asm:970-972`).
- Error 04: parity error (`gateway.asm:719-720`, `gateway.asm:974-975`).

Recording (`errorcleanup`):
- Only the first error is kept (`errornum` written only when zero) (`gateway.asm:977-979`).
- The receive is aborted and the RX/TX LEDs switched off (`gateway.asm:980-995`).
- The `E` LED function is switched on for about 2 s unless errors are being suppressed (`gateway.asm:996-1002`).

Suppression:
- A boiler line change while the gateway is listening to the thermostat sets `errornum = 0xFF` (`gateway.asm:681-685`).
- The next Timer 0 overflow clears that value without printing (`gateway.asm:1591-1593`).

Printing (time order):
1. The interrupt returns through `activity`, which reloads Timer 0 (`gateway.asm:1003`, `gateway.asm:775-777`).
2. At the next Timer 0 overflow, after about 10 ms of line idle (`IDLETIME` 10000 us and the `PRELOADT0` preload, `gateway.asm:190-191`, `:203`, reloaded at `:775-776`), `IdleTimer` first clears an `errornum` of 0xFF (errors suppressed) to 0 (`gateway.asm:1591-1593`), then calls `PrintError` when `errornum` is non-zero (`gateway.asm:1594-1596`).
3. `PrintError` returns without printing while a power report is pending, and `errornum` stays set (`gateway.asm:1938-1939`).
4. Otherwise it prints `Error ` + two hex digits, CR LF. For even numbers (02, 04) it also prints `E` + `byte1..byte4`, the complete frame, through `PrintMessage`, which `HideReports` can suppress. Then it clears `errornum` (`gateway.asm:1940-1948`).

### Power level lines in detail

1. Interrupt: after 20 ms without a line event, `PowerChange` raises `PowerReport` (`gateway.asm:857-861`).
2. Main loop: `ReportPower` clears `PowerReport` (`gateway.asm:1380-1381`, `gateway.asm:1532`).
3. In High Power with the line voltage above `VoltOpen` (thermostat gone), no report is made (`gateway.asm:1533-1541`).
4. A report is made only when the Smart Power bits differ from the last reported value, which is remembered in `gpioflags` (`gateway.asm:1542-1547`, `gateway.asm:471`, `gateway.asm:475`).
5. Text: see PR=P in (c).

### Not found in the code / unclear
- No other `Error NN` numbers than 01..04 were found (`gateway.asm:962-975`).
- `OE` (serial overrun) is not printed spontaneously. It replaces the reply of the next complete command (`gateway.asm:1419-1425`, `gateway.asm:3711-3712`, `gateway.asm:3780-3781`).
- The boot loader sends a raw `<ETX>` (0x04) byte at every non-WDT reset before the banner (`selfprog.asm:44-45`, `selfprog.asm:144-145`). It is not a text line. Hosts may see it as a stray byte.
- Possible loss of the `WDT reset!` text (inference from code reading, not verified on hardware or by a test). It is printed before `ClearRam` (`gateway.asm:1170-1188`). Characters that do not fit into TXREG directly go into the TX buffer (`gateway.asm:1895-1920`). `ClearRam` then zeroes `txavailable`/`txpointer` in bank 0 (`gateway.asm:325-326`, `gateway.asm:356`). The buffered remainder of the line may therefore never be sent.

---

## (g) Start-up and reset

### Timeline from reset to normal operation

1. **Reset vector.** At 0x0000 the firmware calls `SelfProg`, then jumps to `Start` (`gateway.asm:630-632`).
2. **Boot loader (selfprog.asm).**
   - After a watchdog time-out it returns at once (`selfprog.asm:75-77`).
   - Otherwise it disables interrupts and sets 4 MHz (`selfprog.asm:79-83`).
   - It sets the comparators to pass-through so thermostat and boiler keep talking (`selfprog.asm:88-109`).
   - It opens the serial port at 9600 baud (`selfprog.asm:124-130`), sends `<ETX>`, and waits up to about 1 s for `<STX>` (`selfprog.asm:18-20`, `selfprog.asm:144-151`). On a timeout or any other byte it returns (`selfprog.asm:150-156`).
3. **Start: LEDs and pins.**
   - `LATB` cleared ("Flash the LED's on startup") (`gateway.asm:1022-1024`).
   - Oscillator set to 4 MHz (`gateway.asm:1025-1027`).
   - Analog pins, USART pin mapping, TRIS set (`gateway.asm:1029-1054`).
4. **Reset reason** into the high nibble of `mode` (`gateway.asm:1055-1069`):
   - POR suppresses BOR (`gateway.asm:1056-1057`).
   - The low nibble of `~PCON` gives the index. Stack overflow gives 5 and underflow 6 (`gateway.asm:1058-1063`).
   - When no PCON flag is set (software restart through `Restart1`/`Restart2`), the index is `resetreason + 1`: BREAK sets bit 1 -> 3 `S`; loop sets bits 2 and 1 -> 7 `L` (`gateway.asm:1064-1065`, `gateway.asm:1017-1020`).
   - A WDT time-out forces 0 `W` (`gateway.asm:1066-1067`).
   - Writing `mode` also clears `MonitorMode`, `ChangeMode` and `AlternativeUsed` (`gateway.asm:1068-1069`, `gateway.asm:307-313`).
   - The letter table is at `gateway.asm:5508-5516`. Confirmed by tests for P, C, E, L, S (`tests/reset-1.stc:12`, `tests/reset-2.stc:8-15`, `tests/reset-3.stc:18`, `tests/reset-4.stc:13`, `tests/reset-5.stc:20`).
5. **Fail-safe decision** (`gateway.asm:1070-1073`):
   - `FailSafeMode` is set, then cleared unless the reset was a WDT time-out and `NoFailSafe` (conf bit 6) is set.
   - At this point `conf` is still the RAM value from before the reset; EEPROM is read later at `gateway.asm:1211-1213`.
   - PCON flags are then re-armed (`gateway.asm:1074-1075`).
6. **Timers configured.** Timer 0 1:64 (`gateway.asm:1077-1085`). Timer 2 250 us, stopped (`gateway.asm:1087-1100`). Timer 1 1:8, running (`gateway.asm:1102-1112`).
7. **LED flash.** Wait for one Timer 0 overflow, about 16 ms, with the watchdog cleared (`gateway.asm:1114-1119`).
8. **Serial port.** 9600 baud, transmit enabled. Receive is not enabled yet (`gateway.asm:1121-1129`).
9. **FVR, DAC, ADC.** FVR 2.048 V, DAC on FVR, wait for FVR ready, ADC measures the FVR buffer. Comparator and Timer 2 interrupt enables are set in PIE, but GIE is still off (`gateway.asm:1131-1168`).
10. **Fail-safe banner.** If `FailSafeMode`: print `WDT reset!`. If the pre-reset `NoThermostat` flag was set, fail-safe is cancelled (`gateway.asm:1170-1175`).
11. **RAM clear.** Banks 0..3, addresses 0x20..0x6F of each bank (`gateway.asm:1177-1188`, `gateway.asm:1441-1455`). Not cleared: shared RAM (`mode`, `byte1..4`, and so on, `gateway.asm:285-313`), bank 4 `txbuffer` (`gateway.asm:579-582`), bank 8 `MsgInterval`/`IntervalCounter` (`gateway.asm:601-603`).
12. **Thresholds.** `VoltShort`, `VoltLow`, `VoltOpen` from the FVR reading. ADC switched to AN0 (`gateway.asm:1190-1203`).
13. **EEPROM restore, part 1.**
    - `settings` -> RAM and `DACCON1` (reference voltage) (`gateway.asm:1205-1209`).
    - `conf` (`gateway.asm:1211-1213`).
    - Comparator module: monitor mode if conf bit 0 (`MonModeSwitch`) is set or `FailSafeMode` is set, else gateway mode (`gateway.asm:1215-1216`, `gateway.asm:4753-4774`).
    - DHW setpoint and max CH setpoint; a non-zero value schedules sending it to the boiler (`INITSW`/`INITSH`) (`gateway.asm:1218-1229`, `gateway.asm:5471-5473`).
14. **Serial receive** enabled unless fail-safe (`gateway.asm:1231-1233`).
15. **Interval timers.** `MsgInterval` from EEPROM. Timer 6 configured, off. Timer 4 (20 ms) started (`gateway.asm:1235-1250`).
16. **Line state and interrupts.**
    - Comparator state copied into `msgflags`. PORTB all high (LEDs off). RA3/RA4 set to mirror the line state (`gateway.asm:1252-1265`).
    - PEIE on. GIE on unless fail-safe (`gateway.asm:1266-1268`).
17. **Defaults.**
    - `INITRP`, `INITHW`, `INITCH`, `INITOM` scheduled: MsgID 6, 48, 49, 99 (`gateway.asm:1269-1273`, `gateway.asm:3438-3455`).
    - Default DHW limits 20..80 and CH limits 10..90 (`gateway.asm:1274-1281`, `gateway.asm:261-265`).
    - "No user value" flags for control setpoint, max modulation, cooling, CH2, fake room setpoint, fake room temperature, time (`gateway.asm:1283-1289`).
    - `SecCounter` = 50 (`gateway.asm:1291-1292`).
18. **EEPROM restore, part 2.**
    - Thermostat model (`gateway.asm:1294-1296`).
    - Manually configured unsupported MsgIDs (`UI=`) re-marked from the EEPROM bitmap (`gateway.asm:1298-1324`, `gateway.asm:5518-5519`).
19. **Banner** `OpenTherm Gateway 6.8` (`gateway.asm:1326-1327`).
20. **LED functions and GPIO.** LED functions 1..6 from EEPROM, defaults F X O M P C (`gateway.asm:1329-1353`, `gateway.asm:5463-5468`). GPIO functions from EEPROM, `gpio_init` (`gateway.asm:1355-1359`, `gateway.asm:5318-5376`).
21. **Fail-safe end state.** If fail-safe: LED function `M` on (`gateway.asm:1361-1364`). The main loop then only clears the watchdog and drains the TX buffer (`gateway.asm:1368-1374`). With GIE and serial receive off and the comparators in monitor mode, the gateway passes the line through in hardware and accepts no commands.
22. **Normal main loop** (`gateway.asm:1368-1439`):
    - clear WDT and send queued TX;
    - handle a received OT message;
    - power report;
    - Timer 0, 1, 4, 6 handlers;
    - A/D result (thermostat check);
    - GPIO;
    - summary;
    - 64 s loop check;
    - serial input.

### First frames

- **Gateway mode with a thermostat.** Nothing is sent until the thermostat sends a request. Each request is reported (`T`), treated, and forwarded (`gateway.asm:1489-1495`, `gateway.asm:2386-2418`).
- **Init messages** (MsgID 6, 48, 49, 99, the DHW/CH setpoint writes, the remote request) are not sent at boot. They are sent in place of a request for an unsupported MsgID, or in a stand-alone "user" slot, through `Alternative` -> `InitCmdLoop` (`gateway.asm:2398-2418`, `gateway.asm:3418-3462`).
- **Stand-alone.** `NoThermostat` is 0 after the RAM clear. The stand-alone sequence starts only after `CheckThermostat` has seen an open or shorted line and printed `Thermostat disconnected` (`gateway.asm:1771-1799`). See (e) for the table and pacing.

### Reset commands and restart paths

| Trigger | Mechanism | Reason | Citation |
|---|---|---|---|
| `GW=R` | `GW` with a non-boolean argument goes to `ResetGateway`. `R` executes the `reset` instruction; any other letter gives `BV`. Length must be 4, else `SE`. | `C` | `gateway.asm:4729-4735`, `gateway.asm:4776-4780`, `tests/reset-2.stc:8-15` |
| BREAK on serial | Framing error with a received 0 byte -> `Break`. If `NoBreak` (about 0.5 s without a BREAK) -> `Restart2`: set `resetreason` bit 1, call `SelfProg`, fall into `Start`. Otherwise restart the hold-off timer and continue. A software restart, not a hardware reset. | `S` | `gateway.asm:1409-1415`, `gateway.asm:1010-1022`, `tests/reset-5.stc:20` |
| Same MsgID / no change for 64 s | `repeatcount` bit 6 -> `Restart1`: set bit 2, then `Restart2`. | `L` | `gateway.asm:1403-1404`, `gateway.asm:1017-1021`, `tests/reset-4.stc:13` |
| Watchdog time-out | Hardware reset. `SelfProg` returns at once. Possibly fail-safe (step 5). | `W` | `gateway.asm:1066-1067`, `selfprog.asm:75-77` |
| Reset button (MCLR) | Hardware reset | `E` | `gateway.asm:1035`, `tests/reset-3.stc:18` |
| Power on / brown-out | Hardware reset | `P` / `B` | `gateway.asm:1056-1058`, `tests/reset-1.stc:12` |

`GW=0` / `GW=1` (not a reset): stored in conf bit 0 with inverse logic and applied through `SetMonitorMode`. If Timer 2 is running, the switch waits for the next idle Timer 0 overflow (`ChangeMode`) and the reply shows `P` (`gateway.asm:4736-4752`, `gateway.asm:1588-1589`).

`FS=0` / `FS=1`: `FS=0` sets `NoFailSafe` (conf bit 6), `FS=1` clears it. The value is saved to EEPROM and echoed (`gateway.asm:3919-3921`, `gateway.asm:5294-5301`, `gateway.asm:4072-4082`).

### Not found in the code / unclear
- **Fail-safe polarity: documented intent and code conflict (needs Robert or Schelte, or a bench test).** Unresolved.
  - Tracing the code: fail-safe is entered after a WDT reset only when conf bit 6 is set, that is after `FS=0` (`gateway.asm:1070-1073`, `gateway.asm:5297-5299`).
  - The comments name the bit "No fall back to fail safe mode" and the test line "Fail safe mode is disabled by user" (`gateway.asm:522`, `gateway.asm:1072`), which suggests the opposite.
  - The file header says the gateway switches to monitor mode after a watchdog timeout, as default behaviour (`gateway.asm:29-30`).
  - With the EEPROM default `Configuration` = 0 (`gateway.asm:5471`), the code trace gives no fail-safe after a WDT reset by default, which contradicts that header.
  - No test covers `FS=` or a WDT reset.
- The fail-safe test at step 5 and the `NoThermostat` test at step 10 read RAM from before the reset, not EEPROM (`gateway.asm:1072`, `gateway.asm:1174`). Whether that is intended is not stated.
- The PCON bit layout behind the `~PCON` index (BOR=1, POR=2, RESET instruction=4, MCLR=8) comes from the PIC16F1847 register map in `p16f1847.inc`, which was not opened in this session. The mapping is confirmed only indirectly by the reset tests for P, C, E.
- The exact boot loader wait (the `Pause` routine) was not traced. Only the "1 second" comments were read (`selfprog.asm:18-20`, `selfprog.asm:146`).

---

## (h) Changes from 6.6 to 6.8

### Files compared

`git diff --no-index` and `diff -rq` between `otgw-6.6/` and `otgw-6.8/`, run this session:
- `diagnose.asm`, `ds1820.asm`, `interface.asm`, `selfprog.asm`: identical (empty `--stat`).
- `gateway.asm`: 48 insertions, 6 deletions.
- Also different: `Makefile` (comment only: `# # Include` -> `# Include`), `build.ctl` (`version 6.6` -> `version 6.8`), `readme.txt` (download examples now say `otgw-6.7`, not 6.8).
- Only in 6.8: `.github/workflows/firmware.yaml` and 20 new test scripts: `command-7`, `heatpump-1`, `heatpump-2`, `interval-1`, `interval-2`, `otversion-1..4`, `roomtemp-1`, `sensor-7..12`, `service-2`, `str-1..3`.
- The two 6.6 copies (`OTGW-other-projects/otgw-6.6` and `OTGW-firmware/other-projects/otgw-6.6`) are identical (`diff -rq` empty).

### gateway.asm differences

| # | Change | 6.6 | 6.8 |
|---|---|---|---|
| 1 | Version string `"6.6"` -> `"6.8"`. Changes the banner and `PR=A`. | `gateway.asm 6.6:6` | `gateway.asm:6`, `gateway.asm:5477` |
| 2 | New RAM `fakeroomtemp1`, `fakeroomtemp2`, flag `NoFakeRoomTemp` = `fakeroomtemp1` bit 7 | after `gateway.asm 6.6:369-371` | `gateway.asm:372-374` |
| 3 | Start-up sets `NoFakeRoomTemp` (no fake room temperature) | after `gateway.asm 6.6:1284` | `gateway.asm:1288` |
| 4 | MsgID 24 dispatch `WordResponse` -> `MessageID24` | `gateway.asm 6.6:2561` | `gateway.asm:2565` |
| 5 | New helper `messagewrite`: if the message was modified (`AlternativeUsed`), set `byte1 = T_WRITE` | inserted before `gateway.asm 6.6:2666` | `gateway.asm:2670-2673` |
| 6 | `roomsetptchange` (fake room setpoint, `BS=`) now calls `setbyte4` and ends with `messagewrite`, so a modified MsgID 16 request goes out as Write-Data | `gateway.asm 6.6:3019-3024` | `gateway.asm:3027-3033` |
| 7 | New `MessageID24` handler: on a request, when `NoFakeRoomTemp` is clear, replace the data with `fakeroomtemp1/2` and make it Write-Data through `messagewrite`. Responses and the off state go to `WordResponse` as before. | none (MsgID 24 went to `WordResponse`) | `gateway.asm:3058-3068` |
| 8 | `MessageID125` (OT version to the thermostat). 6.6: major below 3 -> answer 3.0, else pass. 6.8: below 3 -> 3.0; 3 or 4 -> pass; 5 or higher -> major 4, minor byte 51 (0x33), comment "OTGW does not support OT v5.0". | `gateway.asm 6.6:3305-3316` | `gateway.asm:3326-3344` |
| 9 | Serial dispatch comment `CE, GA` -> `CE, GA, RT` | `gateway.asm 6.6:3721` | `gateway.asm:3749` |
| 10 | `SerialCmd06` adds `RT` -> `SetRoomTemp` | `gateway.asm 6.6:3821-3825` | `gateway.asm:3849-3856` |
| 11 | New `SetRoomTemp` (`RT=`): `GetPercentArg` (0..100, non-negative), stores the value. `RT=0` sets `NoFakeRoomTemp` (off). Echoes the value as a float. | inserted after `gateway.asm 6.6:4258-4267` (`SetFakeSetpoint`) | `gateway.asm:4300-4309`, `gateway.asm:5099-5112` |

Unchanged neighbours, opened for orientation: `MessageID20` (`gateway.asm 6.6:3039-3047`, `gateway.asm:3048-3056`) and `SetFakeSetpoint` (`gateway.asm 6.6:4258-4267`, `gateway.asm:4289-4298`).

### Effects shown by the new 6.8 tests

- `RT=25.41` with `AA=24` in stand-alone mode sends `R10181969`: Write-Data, MsgID 24, 25.41 in f8.8 (`tests/heatpump-1.stc:9-22`). This shows changes 5, 7 and 11: the alternative was a Read (`gateway.asm:3418-3421`) and goes out as a Write.
- A boiler answer of version 5.0 is passed to the thermostat as `A407D0433`, that is 4.0x33 (`tests/otversion-4.stc:6-19`). A boiler 2.x is raised to 3.0 (`tests/otversion-2.stc:6-19`). A missing version gives 3.0 (`tests/otversion-1.stc:18`). A 4.x boiler is passed through (`tests/otversion-3.stc:19-20`).
- `roomtemp-1` checks that MsgID 24 passes through without `RT=` (`B501817E8`) (`tests/roomtemp-1.stc:5-20`).

### Not found in the code / unclear
- `RT=` and `BS=` are missing from the header command list (`gateway.asm:44-145`) in both versions.
- `RT=` has no command expiry (it does not set a heartbeat flag; compare `gateway.asm:4300-4309` with `gateway.asm:4227`). The value is not stored in EEPROM, so it is lost on reset.
- Minor byte 51 (0x33) is about 0.2 in f8.8, so the reported version reads as 4.2 (inference). The code comment does not say why 51 was chosen.
- The `readme.txt` in 6.8 refers to `otgw-6.7` tarballs. Whether a 6.7 release exists is not visible in this tree.

## Verification

A second reader (a separate agent on the standard model, not the one that wrote the sections) checked 15 citations drawn at random (seed 20261112) from the 1131 `gateway.asm` citations in sections (a) to (g). It opened each cited range and the flags, banks and routines it depends on, and tried to refute the claim. Result: 14 confirmed, 1 partly right, 0 refuted. The partly-right item (#15, the `PrintError` condition and the 10 ms idle time) is corrected above with the extra lines it named. The two open questions at the top were also re-opened by the assembling session (`gateway.asm:1044-1077`, `:2532-2545`, `:2688-2692`, `:2922-2943`, `:5288-5303`, `:286`, `:308-313`, `:315`, `:516-522`, and `gateway.asm 6.6:2927-2930`).

Source: otgw-6.8/gateway.asm (read with line numbers). Each verdict covers only the part of the claim that the cited lines are meant to support.

| # | Citation | Verdict | Reason |
|---|---|---|---|
| 1 | gateway.asm:4912-4913 (KI=0) | CONFIRMED | `skpz` / `movwf float1` at 4912-4913. GetDecimal ends at GetDecReturn `movfw temp` / `return` (5067-5068) with C clear and Z set for value 0, so the store into `float1` is skipped and 4914 onward test and use the stale `float1`. W is still 0 at `pcall UnknownMask2` (4919, 3520), so the RAM bit is for ID 0. For KI the RAM operation clears the bit (4920-4921), so "MandatoryID unmarks it again" is a no-op for KI; that applies to UI only. |
| 2 | gateway.asm:1056-1057 | CONFIRMED | `btfss PCON,NOT_POR` / `bsf PCON,NOT_BOR`: when NOT_POR is 0 (power-on reset) NOT_BOR is set to 1, so the BOR indication is cleared before the reset reason is built at 1058-1069. |
| 3 | gateway.asm:5474 | CONFIRMED | `MessageInterval de 200`. Loaded into MsgInterval at 1235-1240; Timer 6 is configured for a 5 ms interrupt (comment 1238, PR6=249, 1:4/1:5 at 1241-1244; MessageTimer comment 1570), so 200 x 5 ms = 1000 ms. |
| 4 | gateway.asm:2941-2947 | CONFIRMED | MessageID9 acts on responses only (2941-2942), goes to MsgID9Change when `OverrideReq` (override,3, line 436) is set, else when `byte1,5` is set, and MsgID9Change calls `messageack` (2947). Note: in the context of the list (synthesised Unknown-DataID, 0x70) bit 5 is always set, so for that case the override condition does not matter; bit 5 is also set for a Data-Invalid answer. |
| 5 | gateway.asm:516-522 | CONFIRMED | `conf res 1 ;Saved in EEPROM` with MonModeSwitch (conf,MONMODEBIT = bit 0, line 309), TempSensorFunc, SummerMode, HotWaterSwitch, HotWaterEnable and NoFailSafe (conf,6). Loaded from EEPROM Configuration at 1211-1213. |
| 6 | gateway.asm:2997-3020 | CONFIRMED | Integer difference must be 0 (after one's complement for a negative result, 3003-3007) and the fraction difference masked with b'11100000' must be 0 (3010-3013), i.e. under 32/256 = 0.125. On a match `OverrideSet` is set and the 'O' LED switched on (3018-3020); if `OverrideSet` was already set it just returns (3016-3017). The comparison runs only for requests with OverrideReq set, OverrideWait clear and OverrideClr clear (2984-2992). |
| 7 | gateway.asm:5471 | CONFIRMED | `Configuration de 0`. With conf,6 = 0, the WDT path 1071-1073 does not skip `bcf FailSafeMode`, so no fail-safe by default. The bit tested really is conf,6 (line 522), and SetFailSafety writes it with inverse logic (5297-5299), consistent with the polarity remark. |
| 8 | gateway.asm:1580-1586 | CONFIRMED | IdleTimer (called on TMR0IF, 1383-1384) calls StartConversion when `T2CON,TMR2ON` is clear (1584-1586); CheckThermostat is called on `PIR1,ADIF` (1395-1396). Timer 2 stopped means line idle per the comments at 660 and 781. |
| 9 | gateway.asm:2915 | CONFIRMED | `bcf initflags,INITRP` in MessageID6, reached for every response to MsgID 6 (2900-2901), both via the ReadAck path and via `goto parameterupdate` for other response types (2903-2905, 2911-2915). |
| 10 | gateway.asm:3048-3056 | CONFIRMED | MessageID20 acts on responses only; with `InvalidTime` (clock2,7, line 335) clear it calls `messageack` and puts clock1/clock2 in the data bytes. `messageack` keeps the original request type (2676-2679), so a read gets READ-ACK and a write gets WRITE-ACK with the clock data, matching `AC014...` and `A5014...`. |
| 11 | gateway.asm:1877-1881 | CONFIRMED | PrintStringNL falls through to NewLine, which prints 0x0d then 0x0a. The only `movlw 0x0d`/`0x0a` for output in the file are 1878 and 1880 (1471-1474 are input parsing), so "all lines" holds as far as a grep shows; not every print path was traced. |
| 12 | gateway.asm:1174 | CONFIRMED | `btfsc NoThermostat` (onoffflags,2, RAM, line 454) at 1174 runs before ClearRam (1178-1188). NoFailSafe is conf,6 (RAM), tested at 1072 before conf is reloaded from EEPROM at 1211-1213. No write to conf or onoffflags between 1056 and 1174 (only `movwf conf` is at 1213). |
| 13 | gateway.asm:2718-2751 | CONFIRMED | MessageID0 request path (readstatus) ends in `goto SwitchLED` (2751) and the response path statusreturn (2753-2781) ends in `goto SwitchLED`; neither calls WordResponse or ByteResponse (3646-3668). |
| 14 | gateway.asm:3309-3311 | CONFIRMED | After `setbyte4` (which ends `retlw 0`, 2702-2706) W is 0; `btfsc OverrideHigh` / `movfw byte4` loads the flags only when settings,6 is set, then `setbyte3`. So OH=1 copies the flags into the high byte, OH=0 writes 0. OverrideHigh = settings,6 (line 512), default set in SavedSettings (5459). |
| 15 | gateway.asm:1594-1596 | PARTLY | `tstf errornum` / `skpz` / `call PrintError` is there, but just before it (1591-1593) an errornum of 0xFF ("Suppressing errors") is cleared to 0, so PrintError is called for non-zero values other than 0xFF, not for every non-zero value. The "about 10 ms of line idle" timing is not supported by these lines (it depends on IDLETIME/PRELOADT0, 200-203, 775). |

14 confirmed, 1 partly, 0 refuted
