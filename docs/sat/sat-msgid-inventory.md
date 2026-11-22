# SAT MsgID inventory per rule (TASK-3037, step 1)

Status: step 1, read-only inventory of origin/dev 9caac3519 (2026-11-22). Written for Sergeant D's decisions on
entries 470/471/472. Entries 470/471: SAT reads ID25 (flow) and the flame bit (ID0 low byte bit 3) and writes ID1
(control setpoint, CS=), ID14 (max modulation, MM=) and ID0 high byte bit 0 (CH enable, CH=). Entry 472: an ID25
Data-Invalid answer or a flow older than 60 s hands control back to the thermostat (TASK-3036, not yet coded).

How it was made: every read of an OpenTherm value in SAT*.ino (directly, or through satGetFlowTemp(),
satGetReturnTemp(), satIsFlameOn(), satGetActualModulation(), satThermostatRoomTemp() and satGetOutsideTemp()),
grouped by the function that reads it. All line numbers are in src/OTGW-firmware/ on origin/dev.
**BEYOND** = the rule reads more than ID25 and the flame bit. "Steers" = yes when it changes ID1, ID14 or ID0 HB0.
Entries and ADRs are named where the code comment at that line names them. "not traced" means this pass did not
open it.

| Rule | Source | OT inputs read | Writes | Fallback when an input is missing | file:line | Steers |
|---|---|---|---|---|---|---|
| Flow feed validity | entries 218/241 | ID25 | - | invalid or stale 60 s = flow missing (0.0): PWM idle, continuous setpoint. Entry 472 replaces this with a hand-back | SATcontrol.ino:3040-3053, 3105-3108 | yes |
| Return feed validity, no-return boiler | entries 217, 45, 149, 263 | **ID28** (BEYOND) | - | invalid or absent 5 min = no return temperature: flow + 18 start-up; before that, the previous CS | SATcontrol.ino:3048-3053, 3112-3138 | yes |
| PWM schedule | (not traced) | flame bit, ID25 | ID1 | flow 0.0 = PWM idle | SATcontrol.ino:1624, 1636-1638 | yes |
| PWM ON setpoint / flame-off hold | thermo-nova flame_off_hold, entries 135/149/151/263 | ID25, **ID28** (BEYOND) | ID1 | no-return boiler: flow + offset; return not known yet: previous CS | SATcontrol.ino:1300-1310, 1864-1890 | yes |
| PWM suppression setpoint/refresh | (not traced) | ID25, flame bit, **ID0 LB bits 0 and 2** (fault, DHW) (BEYOND) | ID1 | fault or DHW active: no refresh | SATcontrol.ino:1319, 1914-1919 | yes |
| Continuous setpoint (start hold, follow) | entry 347 | ID25, flame bit, **ID28** (BEYOND) | ID1 | return not known: no hold, continuous on the PID output | SATcontrol.ino:2679-2697 | yes |
| Continuous clamp refresh | (not traced) | flame bit, **ID0 LB bits 0 and 2** (BEYOND) | ID1 | fault or DHW: no refresh | SATcontrol.ino:2717-2719 | yes |
| No-return start-up flood | entries 45, 142, 149, 151 | ID25, flame bit, **ID0 LB bits 0 and 2** (BEYOND) | ID1 | only for a no-return boiler | SATcontrol.ino:2059-2082 | yes |
| Start gate (PWM first, no exit in the first 60 s) | ADR-252 | flame bit | - | - | SATcontrol.ino:4165, 4181 | yes |
| Continuous -> PWM (sustained overshoot) | ADR-249 | ID25 | ID1 (via the mode) | flow 0.0 = no overshoot | SATcycles.ino:1404-1436 | yes |
| PWM -> continuous exits (underheat, saturation, at-max) | entries 435, 256; 448/456 in TASK-3008 (not landed) | ID25, flame bit | ID1 (via the mode) | flow 0.0 = no underheat | SATcycles.ino:1488-1512 | yes |
| Tolerance band (CH off at target + 0.1) | ADR-250, ADR-253 | room temperature (see Room) | ID0 HB0 | no room temperature: band not active (not traced) | SATcontrol.ino:466, 8175, 8477 | yes |
| Cold cutoff | entry 199 | none of ID25/flame directly | ID0 HB0, ID1 | - | SATcontrol.ino:455, 8450, 8597 | yes |
| CS / MM / CH senders | entries 237, 239, 334, 335 | **ID0 LB bits 0 and 2** in the resend (BEYOND) | ID1, ID14, ID0 HB0 | fault or DHW: resend paused | SATcontrol.ino:536, 696, 758, 781, 804 | yes |
| MM during hot water | entries 21, 220 | **ID0 LB bit 2** (DHW) (BEYOND) | ID14 | - | SATcontrol.ino:7900, 8108, 8186 | yes |
| Boiler fault hand-off | (not traced) | **ID0 LB bit 0** (fault) (BEYOND) | ID1, ID14, ID0 HB0 | - | SATcontrol.ino:8272 | yes |
| Room temperature input | ADR-293 | **ID24** (last in the auto order) (BEYOND); BLE, areas, external are not OT | - (PID input) | none: heating curve only | SATcontrol.ino:2782-2900 | yes |
| Outside temperature input | (not traced) | **ID27** (BEYOND) or weather/MQTT | - (curve, PID input) | default outside temperature | SATcontrol.ino:2978, 3001, 7841 | yes |
| Heating curve | ADR-198 | outside temperature (see above) | - (feeds ID1) | as outside | SATcontrol.ino:1008, 8387 | yes |
| Solar gain offset | (not traced) | **ID17** (BEYOND) | - (lowers the PID output) | not traced | SATcontrol.ino:7654, 8431-8432 | yes |
| Curve self-tune | ADR-205, ADR-228, ADR-234 | flame bit, outside (via the tick) | - (changes the curve coefficient) | not traced | SATcontrol.ino:7195-7345 | yes (slowly) |
| Thermal learning | (not traced) | flame bit, **ID27** (BEYOND) | - | not traced | SATcontrol.ino:7457-7459 | not traced |
| Zone PIDs | (not traced) | **ID27** via the curve (BEYOND) | - (feeds ID1) | as outside | SATcontrol.ino:3737, 3767, 3782 | yes |
| Modulation reliability | thermo-nova device | flame bit, **ID17** (BEYOND) | - | below 8 samples: not reliable | SATcontrol.ino:6321-6342 | no: read only by the boiler status (display) |
| Boiler status | entry 259 | flame bit, **ID17**, ID25 (BEYOND) | - | - | SATcontrol.ino:1045-1062 | no ("Display only", :1062) |
| No-ignition / heating-failure events | TASK-1882 | flame bit, ID25 | - | - | SATcontrol.ino:7963, 8634 | no (diagnostic; TASK-3034 bug) |
| Power and energy | (not traced) | **ID17**, flame bit (BEYOND) | - | - | SATcontrol.ino:6017-6018 | no (display) |
| Pressure health | (not traced) | **ID18** (BEYOND) | - | - | SATpressure.ino:36; SATcontrol.ino:6100-6107 | not traced |
| Setpoint sync check | (not traced) | **ID1 read-back** (BEYOND) | - | - | SATcontrol.ino:6353 | not traced |
| Cycle statistics | Task #227 | ID25, **ID28**, ID24 (BEYOND) | - | return out of 10-100 C: not counted | SATcycles.ino:1250-1260, 1063-1134 | no (statistics) |

## Open for Sergeant D

Each question is about a rule that steers ID1, ID14 or ID0 HB0 and reads something beyond ID25 and the flame bit.

1. **ID28 (return) in the flame-off hold** (SATcontrol.ino:1300-1310, 1887, 2684). The PWM ON setpoint and the continuous
   start hold use return + offset, or flow + offset for a no-return boiler. With entries 470/471, should the hold always use
   flow + offset (drop ID28), or keep ID28 where the boiler answers it?
2. **ID28 in the no-return detection and the start-up flood** (SATcontrol.ino:3048-3053, 2059-2082; entries 217, 45, 149).
   If ID28 is dropped, should the no-return path (flow + 18 start-up) become the only path for every boiler?
3. **ID17 (modulation) in the solar gain offset** (SATcontrol.ino:7654, 8431-8432). Solar gain lowers the PID output when
   modulation is below 20 %. Keep it on ID17, replace the condition, or switch solar gain off under 470/471?
4. **ID0 low byte bits 0 (fault) and 2 (DHW)** pause the resend and the refresh paths, and drive the hot-water MM
   (SATcontrol.ino:804, 1914, 2717, 8108, 8186, 8272). These are bits of the same ID0 frame as the flame bit. Are they
   inside "the flame bit" of 470/471, or must they go too?
5. **ID24 (thermostat room) and ID27 (outside)** are room and curve inputs (SATcontrol.ino:2782-2900, 2978-3001). These are
   inputs to the PID and the curve, not boiler feedback: confirm they stay outside the scope of 470/471.

ID17 users that do not steer control (modulation reliability, boiler status, power and energy) need no decision: they
are display only (SATcontrol.ino:1062: "nothing in the control path reads eBoilerStatus").

Not done in this step (TASK-3037 ACs, after Wednesday): the per-rule host tests, the findings list (rules without an
entry, entries without a rule), and Sergeant D's review of this page.
