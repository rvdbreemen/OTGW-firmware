# SAT port audit (2026-10-24)

**Purpose.** This report checks the SAT port in the OTGW firmware against the SAT reference code
(thermo-nova) and against Sergeant D's recorded decisions. It lists every difference found, ordered
by priority, so that Sergeant D (George), who designed SAT, can read it and correct it.

**Date.** 2026-10-24.

**Sources.**

- Discord, read only:
  - #alpha-testing: 642 messages, from the start of the channel (2026-06-11) to 2026-10-24.
  - #dev-sat-mqtt: 1331 messages, from the start of the channel (2023-05-09) to 2026-10-20.
- thermo-nova, the SAT reference code, branch `releases/thermo-nova`, pinned at commit `7beaaf5`
  (`7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`). A citation such as `pwm.py:155` is a path relative to
  `custom_components/sat` at that commit.
- `docs/sat/sergeant-d-corrections.md`, entries 1-18. Entry 19 was recorded today. It is the "remove
  the PWM auto switch setting" answer, and the audit treats it as recorded.
- The firmware working tree on branch `dev` on 2026-10-24 (HEAD `6fe82f3b0` plus uncommitted
  changes). A citation such as `SATcontrol.ino:5145` points into `src/OTGW-firmware/`. Line numbers
  are working-tree line numbers.

**Please correct us.** Sergeant D, if any statement here is wrong, please reply with the finding
number (for example "#13 is wrong because ...") and your correction. Each correction will be
recorded in `sergeant-d-corrections.md` before anything changes in the code.

**Important: TASK-1331 is not committed.** The working tree contains the TASK-1331 change: the
end-of-cycle switch between continuous and PWM, a port of thermo-nova `pwm.py:219-227`. That change
is NOT committed, because a review blocked it.

- The reason: the firmware classifier compares the flow at the end of a cycle (the "tail") with the
  control setpoint frozen at flame-on. thermo-nova compares it with the control setpoint of each
  sample (`cycles/tracker.py:267-271`).
- The consequence: the PWM-to-continuous exits during a burn would be reverted at every flame-off,
  because the cycle is then measured against a setpoint that no longer applies.
- Findings that depend on this uncommitted change are marked **[TASK-1331, uncommitted]**. They
  describe what would happen if the change shipped as it is now. They do not describe the firmware
  that testers run today.

**How to read this report.**

- Every finding was found by reading code. No finding was reproduced on a boiler or on a bench.
- "Entry N" means a recorded entry in `sergeant-d-corrections.md`.
- "E20".."E35" (or "entry 20".."entry 35") are his statements found on Discord by this audit and
  recorded on 2026-10-24 as entries 20-35 (they were drafts D2-D17). Section 4 lists them with their Discord links. The quotes in the drafts were checked by a
  script and match his messages word for word.
- "Estimate" marks a number that was worked out from the code but not measured.

---

## 1. Summary

**What was checked.**

- **Discord:** 1973 messages were scanned.
  - 219 of them are statements by Sergeant D: 88 in #alpha-testing and 131 in #dev-sat-mqtt.
  - 32 of the 219 are covered by entries 1-18.
  - 95 feed 17 draft decisions (D1-E35). D1 has since become entry 19, so E20-E35 (16 drafts) are not
    yet recorded.
  - 92 are not decisions (questions, bug reports, UI remarks, small talk).
  - 13 conflicts between his statements, the code and the firmware were found: 8 are open and 5 were
    settled by his own later statements.
- **Areas audited** (10):
  - start-up and restart;
  - the PID;
  - the heating curve and zones;
  - PWM;
  - cycle classification and the end-of-cycle switch;
  - the cold cutoff and the CH bit;
  - the gateway commands (CS, MM, CH, TC, TP, PM, MI);
  - hot water (DHW);
  - safety and fallback;
  - defaults and settings.
- **Counts:**
  - The area audits produced 145 items, and a second reader checked every one of them.
  - 12 items are "checked and matching" notes. One more item (CYC-04) was filed as a note but
    describes a real risk, so it became finding #22.
  - After merging the items that several areas found independently, 69 findings remain (section 2).
- **Priority.**
  - **No finding is rated critical.** None of them can damage the boiler or bypass the boiler's own
    safety limits, and none was reproduced.
  - 9 findings are high, 27 medium and 33 low.
  - Several high findings are about what happens when SAT stops or loses an input. The fallback that
    should keep the house warm does not work as intended.

**Checked and found matching** (no action; audit IDs STARTUP-11, STARTUP-12, F18, F15(curve),
PWM-18, CYC-14, CYC-15, COLDCH-OK, MODS-13, DHW-12, S14, DEF-15):

- **PID core.** The error rounding, the automatic gains, the integral (only inside the 0.1 °C
  deadband, clamped to ± the heating curve), the frozen derivative inside the deadband, the
  derivative filter, the output (curve + P + I + D) and the solar freeze order (since TASK-1323) all
  match `pid.py:67-230` and `climate.py:501`.
- **Heating curve.** The curve formula and the base offsets 27.2 and 20 match `heating_curve.py:28-39`
  and `types.py:116-118`. The firmware does not round to 0.1, which makes at most 0.05 °C difference.
  The solar offset is applied before the clamp, as in `climate.py:391-394`.
- **Ignition hold and BT window.** The cold-start hold is floored at 28.2/21 (entries 7 and 11). The
  180 s BT window uses the EMA 0.3/0.7 (entry 1).
- **PWM duty.** The duty formula, the thresholds and duty ranges 1-4, the BT seeds, and "no flow
  means idle" all match `pwm.py:53-61`, `:122-131` and `:229-294`.
- **Cold cutoff and the CH rule.** On a cold cutoff the firmware leaves PWM, resets the PWM state,
  the guards and the held CS, and sends MM = maximum. This matches `heating_control.py:229-234` and
  entry 12. The CH rule follows entries 9, 11 and 17.
- **CS ownership** (ADR-179). The PIC CS keepalive, the stale-cache fix (TASK-1324) and the TC= push
  rules of entries 14 and 16 behave as recorded.
- **Simulation isolation** is stricter than in thermo-nova. The setpoint ceilings are at least as
  strict.
- **[TASK-1331, uncommitted] classifier rules.** These match `cycles/classifier.py`:
  - DHW in the tail gives UNCERTAIN;
  - PWM cycles are never OVERSHOOT;
  - continuous OVERSHOOT is tested first;
  - both on_cycle_end branches run and are not gated (entries 15 and 18).

  The inputs to the classifier still differ (findings #8 and #54).

---

## 2. Prioritized findings

Columns:

- **SD?** = needs a decision or an answer from Sergeant D.
- **"plan OK"** = thermo-nova is clear, but the fix changes how SAT controls the boiler, so the plan
  still needs his OK (project rule).
- **Robert** = a maintainer decision.

The original audit IDs in the sources column (for example STARTUP-3, F1) let anyone trace a finding
back to the area audit.

| # | Priority | Area | Finding | Impact (concrete) | SD? | Sources |
|---|---|---|---|---|---|---|
| 1 | high | commands | No SAT path ever releases the MM override. MM=0 from PWM stays after a disable, a trip, the end of the MQTT fallback and an ESP reboot. | The thermostat gets the boiler back with maximum modulation 0 %. If the boiler honours MsgID 14, it then runs at minimum power until the PIC is power-cycled. | no | MODS-01; SATcontrol.ino:503, 2348-2351; gateway.asm:4752-4765; coordinator/serial/__init__.py:228-233 |
| 2 | high | safety | The MQTT-loss fallback turns SAT on for every unit with MQTT enabled, also units that never used SAT. A settings save during the outage makes it permanent. | After any broker outage longer than 5 min, SAT takes over the boiler with default settings and refuses CS= from Home Assistant. If a settings write falls in that window, SAT stays on after the next reboot. | yes | S1; SATcontrol.ino:4706-4729; settingStuff.ino:317; climate.py:544-545 |
| 3 | high | commands | OT-Direct: SAT's last CH=0 stays in the gateway's own MsgID 0 after a disable or trip. | On an OT-Direct board, the board's heating curve and the thermostat-timeout setback then run with CH disabled. The house cools silently. | no | MODS-02; OTDirect.ino:2755-2760, 756-764 |
| 4 | high | safety | A lost or missing room temperature trips SAT after about 10 ticks and keeps it off. thermo-nova keeps heating on curve + P. The age limits also differ per source. | About 5 min after boot without a room value (or 5 min after the sensor stops), SAT stops and stays off until a manual re-enable or reboot. On a BLE-only OT-Direct install, heating likely stops. | yes | STARTUP-2, F12, S2, S3, DEF-12; SATcontrol.ino:4836-4850, 4747; climate.py:466-482; entry 27, conflict C8 |
| 5 | high | curve | The outside temperature is never treated as missing. It is 0 °C at boot (at least 300 s), 0 °C forever without a source, and a weather value never expires. | Heating on the 0 °C curve: 34.7 °C flow in summer (estimate, radiators, coefficient 1.5). An old weather value is used for days after an internet outage. One unneeded burn per reboot on mild days. | yes | F01, STARTUP-4, S4; SATcontrol.ino:1451-1471; SATweather.ino:418-420; climate.py:544-545 |
| 6 | high | pwm | The PWM ON phase is timed from the last real flame-on, not from the start of the ON phase. | While the boiler waits to ignite, CH and CS toggle every 30 s. After a switch to PWM during a long burn, the first ON phase ends within one tick. Breaks entry 17 (CH=1 during ON). | plan OK | PWM-01; SATcontrol.ino:935-944; pwm.py:154-205 |
| 7 | high | defaults | The setup wizard writes heating curve coefficient 1.4 (radiators), 0.6 (underfloor) or 0.8 (heat pump), also over a stored value on "migrate". | Underfloor at 0.6: 23.0 °C flow at 0 °C outside against 30.0 °C at 2.0, and about 4x weaker P. Slow recovery, room below target. | no | DEF-1; data/v2.js:5368, 5426-5434; const.py:182; entry 24 |
| 8 | high | cycles | **[TASK-1331, uncommitted]** The classifier compares the tail flow with the setpoint frozen at flame-on (0.0 at boot or enable), so cycles are wrongly classified OVERSHOOT and switch to PWM. | After every reboot, OTA, enable or fallback during a burn, and after a rising request, a wrong switch to PWM follows: extra burner starts and reduced heat until an underheat exit. This is the review blocker. | plan OK | STARTUP-1, CYC-01, CYC-02; SATcycles.ino:744, 701, 874-877; cycles/tracker.py:267-271 |
| 9 | high | pwm | Heat pump: min-on 1800/2400 s with 2 cycles per hour collapses the duty mapper (**mapper fixed by TASK-1366**, entries 40/50). With TASK-1331, a heat pump can also enter PWM but cannot use the runtime exits. | At 1800 s, PWM becomes 30 min on / 30 min off or continuous. At 2400 s, lower demand gives more run time. **[TASK-1331, uncommitted]** A heat pump in PWM can stay there with no runtime exit. | answered for the mapper (entries 40, 50); still yes for the runtime exits (C3), hybrids and the hourly cap | PWM-02, CYC-03, DEF-6; SATcontrol.ino:345-374, 875-944, 5298 (TASK-1366 working tree); pwm.py:53-61; entries 30, 40, 50, C3, C4 |
| 10 | medium | dhw | During hot water, the firmware sends no MM, so MM=0 from PWM stays on the boiler. thermo-nova sends the maximum. | Every tap draw during PWM. If the boiler applies MsgID 14 to DHW (unknown), showers get minimum burner power. | no | DHW-1, MODS-03; SATcontrol.ino:4801-4818; heating_control.py:136-137, 517-520; entry 21 |
| 11 | medium | dhw | The 300 s guard after hot water never arms, and DHW does not reset the sustain timers. | After a shower, the flow can trigger a switch to PWM within 30-60 s, where thermo-nova waits 300 s. This happens on most draws. | no | PWM-07, DHW-2, S6; SATcycles.ino:1008-1017; SATcontrol.ino:4801-4818, 5124 |
| 12 | medium | pwm | The sustain timers count the first tick as 30 s. The 60 s switch fires after about 30 s, and a tick above 60 s disables all three switches. | Entry into PWM after 30 s of overshoot. Underheat exit at about 150 s, saturation at about 270 s. | C1 only | PWM-08, S7; SATcycles.ino:993-1028; helpers.py:51-56; conflict C1 |
| 13 | medium | pid | The first PID call after a boot, a disable or a flush returns 0, so the first one or two ticks send CS=10, CH=0. At boot the CS=0 also hands TSet to the thermostat for 30 s. | About 60-90 s of boiler-off after each reboot, OTA or enable. With a thermostat calling for heat, one extra short burn per restart. | plan OK | STARTUP-3, F1; SATpid.ino:266-288; SATcontrol.ino:3873-3877, 5145-5159; pid.py:126-132 |
| 14 | medium | pid | The PID runs on the control tick (30 s, settable 10-300 s), not on a fixed 60 s. The integral adds ki × error × 60 per update. | Inside the deadband the integral grows 2x faster at 30 s and up to 6x faster at 10 s (estimate). More risk of oscillation around the target. | yes | F4, DEF-8; SATpid.ino:169, 279-288; climate.py:712-716; pid.py:191 |
| 15 | medium | pid | A target change, an HVAC mode change, a window event or reset_integral does not reset the PID as thermo-nova does. A preset change clears only the published copy of I. | After a schedule or thermostat change, a frozen D term (about -2 °C at Kp 17.35, estimate) is carried into the new target. | no | F2, F3, F10(curve); SATcontrol.ino:1161-1171, 2071-2102; climate.py:641-660 |
| 16 | medium | pwm | "Request reaches the maximum, so disable PWM" compares with 62 °C, but the request is clamped to the boiler's MaxTSet. | With MaxTSet below 62, PWM does not end through this rule. About 5 min of throttled output on the coldest moments. | no | PWM-09; SATcontrol.ino:5083-5096; heating_control.py:203-204 |
| 17 | medium | pwm | The first phase after PWM is enabled is OFF, timed from the last flame-off. thermo-nova starts ON at once. | Example: about 550 s with no heat where thermo-nova relights within 5 s. **[TASK-1331, uncommitted]** makes this the normal path after every overshoot cycle. | yes | STARTUP-6, PWM-05; SATcontrol.ino:957-961, 1064-1080; pwm.py:87-100, 155 |
| 18 | medium | pwm | The hourly start cap counts every flame-on, hot-water ignitions included. thermo-nova's cap never takes effect. | On a combi in PWM, three taps in an hour can block the next heating burn for up to about an hour. | yes | PWM-06, DHW-7; SATcycles.ino:271-303, 749-751; pwm.py:156 |
| 19 | medium | pwm | An ON phase can start with an on-time below 180 s (even 0), so ultra-low duty still fires burns. | Mild days near 28.2 °C: one unwanted burn about every 40 min. thermo-nova keeps the boiler off. | no | PWM-03; SATcontrol.ino:904-914, 957-988; pwm.py:155, 247-260; entry 31 |
| 20 | medium | pwm | The ignition timeout ends the ON phase after 180 s. thermo-nova keeps asking until on_time ends. The code comment claims the opposite. | A boiler that lights late is cut to CS=10, CH=0. Matters mainly after #6 is fixed. | no | PWM-04; SATcontrol.ino:945-956; pwm.py:194-205 |
| 21 | medium | pwm | The boiler-temperature EMA is sampled on the 30 s tick (about 6 samples) instead of every 5 s (about 36). | BT ends a few degrees low, so ON phases are about 20 % longer (estimate). | no | PWM-10; SATcontrol.ino:865-872; climate.py:706-710; entry 1 |
| 22 | medium | cycles | **[TASK-1331, uncommitted]** Margin 2.0 equals the continuous flow offset of 2.0. With thermo-nova's per-sample reference, almost every continuous cycle would become OVERSHOOT. | Fixing #8 the thermo-nova way makes end-of-cycle switches to PWM routine unless the margin is reconsidered. | yes | CYC-04; SATtypes.h:454, 466; heating_control.py:484-489; cycles/const.py:10; entry 15 |
| 23 | medium | cycles | Units that saved settings between 2026-06-20 and 2026-10-21 still run margin 3.0. | On those alpha units, PWM is entered later than entry 15 asks, and test reports do not reflect the approved behaviour. | no | CYC-05; commits 475b046e7 and a7bbaccb6; settingStuff.ino:334, 1014 |
| 24 | medium | commands | CS, MM and CH are sent on a change plus every 300 s, on a 30 s tick. thermo-nova sends all three every 5 s. | After a PIC reset, MM and CH can be wrong for up to 5 min. An external MM or CH write stays in force for 5 min. PWM edges come up to 30 s late. | yes | MODS-05, COLDCH-8; SATcontrol.ino:427, 465-521; heating_control.py:236-238; entry 7 |
| 25 | medium | commands | OT-Direct gateway mode with a thermostat: SAT's CH bit is not reliably applied (TASK-1238). | In the PWM OFF phase and on a cold cutoff, the boiler alternately sees CH=1 and CH=0. The pump may run and the CH state changes back and forth. | no | MODS-08; OTDirect.ino:2996-3004, 2121-2122; ADR-179 |
| 26 | medium | commands | Immergas: the firmware writes the flow setpoint into TP=11:12 and caps MM at 80. thermo-nova writes min(MM, 80) into TP=11:12. | Every tick writes a temperature into a modulation-limit register. The boiler's reaction is unknown. | no | MODS-04, COLDCH-9; SATcontrol.ino:387, 5162-5166; coordinator/mqtt/opentherm.py:216-220 |
| 27 | medium | safety | PWM ignition hold without a return temperature: the firmware sends the 28.2/21 floor with CH=1, while thermo-nova keeps the previous setpoint. | Boilers without Tret (Intergas by default, entry 6) fire late or not at all in PWM ON phases. Neither behaviour is good. | yes | S5; SATcontrol.ino:802-808, 1499-1503; heating_control.py:392-395 |
| 28 | medium | defaults | The coefficient default is still 1.5 where entry 24 decided 2.0. The range is 0.1-5 where thermo-nova allows 0.1-12. | Units that skip the wizard get a flatter curve: 2.5 °C lower flow and about 30 % weaker gains at 0 °C outside (estimate). | no | F9, F04, DEF-2; SATtypes.h:442; settingStuff.ino:999; const.py:182; entry 24 |
| 29 | medium | defaults | The maximum flow setpoint is fixed at 62/45/40 °C with no user setting. thermo-nova uses 55/50 and lets the user choose 10-100. The source comment about heat pumps is false. | Radiators up to 62 °C (less condensing). Underfloor capped 5 °C lower than SAT. Nobody can change either cap. | yes | DEF-5; SATcontrol.ino:293-327; helpers.py:97-99; config_flow.py:612-613 |
| 30 | medium | defaults | Cycles per hour: default 3 (range 2-6). thermo-nova uses 4 on a fresh install, with the options 3 or 4. | PWM period 1200 s against 900 s. Values 2, 5 and 6 have no counterpart for gas boilers. | yes | PWM-14, DEF-7; SATtypes.h:524; settingStuff.ino:1105; const.py:119; entries 30, E31 |
| 31 | medium | dhw | DHW setpoint: SW= is sent only at the start of a draw, behind an extra toggle (off by default). The slider shows the stored setting, not TdhwSet. The bounds ignore MsgID 48. | With defaults the slider does nothing. On a storage tank a raised setpoint may never reach the boiler. Conflicts with entry 33. | yes | DHW-3, DHW-4, DHW-5; SATcontrol.ino:4806-4814, 2564; number.py:47-74; entry 33 |
| 32 | medium | curve | Zones: four differences from area.py (replace instead of max, headroom added, overshoot subtracted instead of capped, gate >10 instead of requires_heat). The zone PID has no D term and a faster integral. | Only with 2 or more zones. Example: rooms at 40 and 45 °C give 50 °C flow, where thermo-nova gives 45 °C. A room 2.5 °C too warm does not stop heating. | yes | F02, F05, F14(pid), COLDCH-7; SATcontrol.ino:1974-2035, 5013-5066; area.py:166-175, 273-333; climate.py:381-389; entry 35 |
| 33 | medium | curve | Thermal comfort (SSI) can use outdoor weather humidity, and it applies only to the thermostat's room temperature. | With the option on: the room looks about 2.7 °C too warm on damp days, so the house stays cold. BLE or MQTT room sensors get no SSI. | no | F07; SATcontrol.ino:1432-1440, 4983-4988; climate.py:270-287; TASK-1282 |
| 34 | medium | curve | The multi-area weighted average is replaced by any fresh BLE sensor, including the area sensors themselves. | With BLE area sensors, SAT regulates on whichever sensor is first in the list, for example a bedroom. | no | F08; SATcontrol.ino:1376-1390; SATble.ino:817-857 |
| 35 | medium | coldch | A heat pump set up with the wizard gets the radiator cold setpoint 28.2 °C, because the wizard never asks for the distribution system. | A heat pump on underfloor heating gets no heat demand for requested flows of 22-28.2 °C on mild days. | confirm | COLDCH-1; data/v2.js:5368, 5438-5439; SATcontrol.ino:339-343; entry 30 |
| 36 | medium | docs | The reference docs (sat-comparison.md, firmware-sat-spec.md) contradict the code and present questions he has already answered as open. | Sergeant D may be asked again, or may "correct" behaviour that is already fixed. | no | COLDCH-11, COLDCH-12, F16, F12(curve), PWM-17, CYC-12, CYC-13, STARTUP-10, MODS-12, DHW-11, S13 |
| 37 | low | records | Approved deviations with no recorded entry: overshoot protection on by default (E20), margin 2.0 for the 60 s switch (E22), the room-temperature filter (E28), the OFF-zone exclusion (E35). | No boiler effect. A later session may revert them to thermo-nova. | no | PWM-16, DEF-10, S13, F8, F09(curve) |
| 38 | low | pid | Firmware-only PID extras: a settable deadband (0.05-2.0), deadband widening during estimation, a ±20 integral cap, no rounding, a 0.001 "no change" test. | No effect at the defaults. A user who sets deadband 2.0 gets a mostly-integral controller. The widening leak (F5) is latent: about 1e-6 °C per tick. | yes | F6, DEF-9, F5; settingStuff.ino:1000; SATpid.ino:157-212; const.py:16; entry 28 |
| 39 | low | pid | The firmware-only auto-tune rewrites the coefficient (and all gains) every hour. | Off by default. When on, the curve moves 1-2 % per hour based on cycle classes that TASK-1331 changes. | yes | F10(pid), F11(curve), CYC-10; SATcontrol.ino:4552-4660 |
| 40 | low | pid | Derivative: the reference temperature moves only with the derivative timestamp (TASK-1195), and the time base is the first PID call that sees a change. | D can be larger than in thermo-nova. Not quantified. | yes | F7; SATpid.ino:49-56, 205-264; pid.py:171, 202-232 |
| 41 | low | pid | The PID is frozen while SAT does not control (DHW, summer, valves closed, fault). thermo-nova keeps updating it every 60 s. | After a long draw or a valves-closed period, I and D resume from old values. Small effect. | yes | F13, COLDCH-5; SATcontrol.ino:4882-4957, 5006; climate.py:466-502, 712-716 |
| 42 | low | pid | PID restore needs NTP and an age of at most 30 min, and is zeroed on every disable. The deferred retry can overwrite a running integral. | After an outage longer than 30 min, the room settles a few minutes later. One-off step in I and D. | yes | STARTUP-8, F11(pid); SATcontrol.ino:2161-2213, 4681-4685; pid.py:141-157 |
| 43 | low | startup | After a restart the firmware always starts continuous. thermo-nova restores PWM-enabled, CS and MM. | One extra overshoot burn per restart in the PWM season. | yes | STARTUP-7, PWM-15; SATcontrol.ino:3856-3858, 3888-3889, 4783-4785; pwm.py:111-115 |
| 44 | low | startup | A runtime enable (wizard, REST, MQTT) sends no PM=3/15/48 and no MI=500 until the next reboot. | Boot priority reads are missing until the next reboot. | no | STARTUP-9; SATcontrol.ino:3888-3899, 4782-4785 |
| 45 | low | startup | MI=500 is effectively never sent: the manufacturer is AUTO and detection has not run yet at initSAT(). | Intergas, Ideal and Nefit users do not get MI=500. The effect on the boiler is unknown. | yes (why MI=500) | STARTUP-5; SATtypes.h:471; SATcontrol.ino:3894-3898; coordinator/mqtt/opentherm.py:166-172 |
| 46 | low | pid | Manual PID gains cannot be reached in the firmware, and their defaults differ (5 / 0.0005 / 0 against 45 / 0 / 6000). | Nobody can use fixed gains. | yes | F15, DEF-4; SATtypes.h:517-520; const.py:113-115 |
| 47 | low | curve | Heating mode ECO is stored but never read by the control code. | The UI shows "eco", but zones and the solar reduction still apply. | yes | F03; climate.py:377-379 |
| 48 | low | curve | The firmware "summer simmer" is an outdoor heating lockout, not the index. The humidity comfort offset depends on that lockout, and the offset and the PV boost shift the PID target. | Off by default. When on: heating stops after warm hours even with a cold room, and humidity can count twice. | yes | F06(curve), COLDCH-3, COLDCH-4, DEF-14; SATcontrol.ino:3391, 4472-4520, 4930-4942, 4998-5006; entry 32 |
| 49 | low | coldch | The summer cutoff sends CS=10, CH=0 but does not leave PWM or reset the PWM state. | One odd PWM period when summer mode ends. | no | COLDCH-2; SATcontrol.ino:4930-4942, 1094-1103 |
| 50 | low | commands | The heater-off and idle paths send the raw maximum MM, so the heat-pump "never 0" rule is skipped. | Only when a user sets the maximum MM to 0: MM=0 goes to a heat pump at each cutoff. | no | MODS-10; SATcontrol.ino:1102, 4907, 4937, 4951; entry 21 |
| 51 | low | commands | A heat pump outside PWM gets MM=100, whatever maximum the user set. | A user's limit (for example 60 % for noise) is honoured only in PWM. | yes | MODS-11; SATcontrol.ino:382; entry 21 |
| 52 | low | commands | TC= is pushed again after each cache reset (boot, enable, OT bus back online). | A thermostat dial change made during a reboot can be overwritten. Rare. | Robert | MODS-07; SATcontrol.ino:443, 539-549; climate.py:655-657; entries 2, 14, 16 |
| 53 | low | commands | PM=25 is never sent after a CS. The thermo-nova MQTT coordinator sends it, the serial coordinator does not. | The suppression CS can follow an old flow value with slow thermostats. Not measured. | yes | MODS-09; coordinator/mqtt/opentherm.py:195-197; coordinator/serial/__init__.py:192-196 |
| 54 | low | cycles | Smaller classifier differences: no warm-up cut, no "kind UNKNOWN" or "PWM IDLE" guards, the cycle kind ignores the CH bit, plus label-only SHORT and UNCERTAIN rules. | Mostly labels. Rarely a mode switch on a cycle that thermo-nova would call UNCERTAIN (only with TASK-1331). | yes | CYC-06, CYC-07, CYC-08, DHW-9; SATcycles.ino:546-570, 663-721; cycles/classifier.py:19-90; cycles/tracker.py:103-116, 314-324; entry 15 |
| 55 | low | cycles | The last cycle class never expires. thermo-nova drops it after 6 h. | Home Assistant keeps showing an old "cycle problem" for as long as the unit runs. | no | CYC-09; SATcycles.ino:588; cycles/history.py:48-57 |
| 56 | low | pwm | Duty range 5 returns before the PWM state machine runs: no ON expiry, no ignition wait, no BT reseed. | Short window for gas boilers. For heat pumps, see #9. | no | PWM-11; SATcontrol.ino:898-903; pwm.py:285-301 |
| 57 | low | commands | On a mode-switch tick, CS (and CH) use the old mode and MM uses the new mode. | For one tick per switch: continuous CS with MM=0, or PWM CS with MM at the maximum. | no | PWM-12, MODS-06; SATcontrol.ino:5099-5140; heating_control.py:202-228 |
| 58 | low | pwm | The "PWM auto switch" setting still blocks the overshoot, underheat and saturation switches (entry 19 removes it; TASK-1341). | No effect at the default. A user who switched it off has lost all three switches. | no | PWM-13, CYC-11, S8, DEF-11; SATcycles.ino:991; entry 19 |
| 59 | low | dhw | During DHW the PWM schedule is frozen, and DHW flame edges move the OFF timer. | After a draw, the next heating burn can be up to one off-time later. | no | DHW-8; SATcontrol.ino:935, 960; pwm.py:162, 188, 208 |
| 60 | low | dhw | Boiler status HEATING_HOT_WATER can never be reached. | The status shows "heating" or "idle" during a shower. Label only. | no | DHW-6; SATcontrol.ino:648-651, 4981; device/status.py:87-88 |
| 61 | low | dhw | The per-cycle DHW sample counters are 16-bit and are counted on every loop() pass, so they can wrap. | Wrong DHW fraction and cycle kind on long cycles, if the loop is fast enough (not measured). | no | DHW-10; SATcycles.ino:117-118, 961-965 |
| 62 | low | safety | Boiler fault bit: SAT stops and keeps the last setpoint, with no trip and no message. | On a fault that does not stop the burner, SAT holds an old setpoint with no warning. | yes | S9; SATcontrol.ino:4862-4866 |
| 63 | low | safety | The thermal-estimate fallback and the 2 h safe setpoint cannot start in the normal room-loss case. | The documented safety net does not work. A trip (#4) happens instead. | no | S10; SATcontrol.ino:1406-1446, 4269-4279, 4896-4911 |
| 64 | low | safety | Flame status STUCK_ON and "tripped means STUCK_OFF" cannot be reached. | Home Assistant flame_health can never report these cases. | no | S11; SATcontrol.ino:3804-3825, 4747 |
| 65 | low | safety | When a real boiler ends simulation, SAT continues with the PID and cycle state learned in simulation. | The first minutes of real control start from simulated state. Rare. | no | S12; SATcontrol.ino:1610-1631 |
| 66 | low | curve | The published heating curve shows the last zone's curve when zones are on. | Display only. Misleads curve tuning. | no | F14(curve); SATcontrol.ino:598, 1996 |
| 67 | low | defaults | The wizard's tuning screen presents a setting that is never used (satautogains) as "learning" and as manual tuning. | No control effect. Misleads users. | no | DEF-3; data/v2.js:5440, 5531, 5540; SATpid.ino:120-141 |
| 68 | low | defaults | Preset temperatures and the window open time differ from thermo-nova and have narrower ranges. "Eco" exists only in the firmware. | Away 15 instead of 10 °C. The window reaction comes 45 s later. User preference only. | no | DEF-13; SATtypes.h:446-464; const.py:172-179 |
| 69 | low | docs | ADR-069 and ADR-071 (Accepted) state wrong values. ADR-195 marks the "<=" boundary as settled when it is not, and its "CH only in satChEnableDecide" rule does not hold. | No boiler effect. Readers of the ADRs get wrong values. | boundary only | F17, F13(curve), COLDCH-6, COLDCH-10 |

---

## 3. Findings in detail

Each paragraph gives:

- what thermo-nova does;
- what the firmware does;
- which entry or draft applies;
- the impact;
- the recommended action.

### High

**#1. MM override is never released.**

- **thermo-nova:** the serial coordinator releases both overrides when it stops: CS=0, then MM=-
  (`coordinator/serial/__init__.py:228-233`). While running, it sends MM after every control step
  (`heating_control.py:237`).
- **Firmware:**
  - The only MM sender writes a number (`SATcontrol.ino:503`), and no non-numeric MM= exists.
  - `satDisable()` and `initSAT()` send only CS=0 (`SATcontrol.ino:2348-2351`, `:3873-3876`).
    Every exit path goes through `satDisable()`: switching SAT off, the end of the MQTT fallback,
    the room-temperature trip and the PIC trip.
  - On the PIC, MM stays in force until a non-numeric MM= arrives (`gateway.asm:4752-4765`). Only a
    PIC reset clears it (`gateway.asm:1281`).
  - On OT-Direct, the override clears only on a non-numeric MM= (`OTDirect.ino:2886-2890`).
- **Applies:**
  - entry 26 (the thermostat is the fallback): this bug breaks that fallback.
  - entry 21: MM=0 is sent in both PWM phases, so this state is common.
- **Impact:** when SAT stops during PWM, the boiler keeps a maximum modulation of 0 %.
- **Note:** thermo-nova is not consistent here. The serial coordinator releases MM; the MQTT
  coordinator does not. This report recommends the serial behaviour.
- **Action:** send MM= (release) together with CS=0 in `satDisable()` and `initSAT()`.

**#2. The MQTT-loss fallback turns SAT on everywhere, and can make it permanent.**

- **thermo-nova:** never switches itself on. With HVAC OFF it returns before any write
  (`climate.py:544-545`).
- **Firmware:**
  - When SAT is off, MQTT is enabled and was connected, and MQTT has been gone for more than 300 s,
    the firmware sets `settings.sat.bEnabled = true` in RAM (`SATcontrol.ino:4706-4720`).
  - Nothing else is checked: not whether SAT was ever configured, and not whether an external SAT
    controller was ever active.
  - The fallback flag lives only in RAM (`SATtypes.h:235`), but `writeSettings()` stores
    `SATenabled` from RAM (`settingStuff.ino:317`). The hourly coefficient save, auto-tune or any
    user save during the outage therefore stores `true` on disk.
  - While SAT is enabled, external CS= is refused (ADR-179).
- **Applies:** entry 26 says the opposite ("If for any reason the SAT is not working the physical
  thermostat will continue heating as a back up solution"). No entry approves this mode.
- **Impact:** every broker outage longer than 5 min. Sometimes SAT stays on for good, on a boiler
  that was never set up for it.
- **Action:**
  - Arm the fallback only on units where SAT was in use.
  - Never persist the fallback state.
  - Ask Sergeant D whether this fallback should exist at all.

**#3. OT-Direct: CH=0 stays after a disable or trip.**

- **thermo-nova:** recomputes CH after every step (`heating_control.py:238`) and has no OT-Direct
  board.
- **Firmware:**
  - SAT's idle paths send CH=0 (`SATcontrol.ino:4938`, `:4952`, `:5159`).
  - On OT-Direct, CS=0 clears only the MsgID 1 override and leaves the CH bit alone
    (`OTDirect.ino:2755-2760`). The bit goes into every gateway-built MsgID 0
    (`OTDirect.ino:756-764`).
  - After the disable, the OT-Direct heating curve resumes (`OTDirect.ino:2191-2198`) and the
    thermostat-timeout setback overrides only TSet (`OTDirect.ino:2586-2591`). Both run with CH 0.
  - On the PIC this does not happen: the PIC applies its CH flag only while a CS override is active
    (`gateway.asm:2713-2723`).
- **Applies:** entry 26. Entries 5, 7, 9, 11 and 17 make CH=0 a frequent SAT state.
- **Impact:** OT-Direct boards only. After a trip during a PWM OFF phase or a cold cutoff, the
  boiler does not heat. The UI shows the curve as active.
- **Action:** restore CH (to the board's own decision) in `satDisable()` on OT-Direct.

**#4. Lost room temperature: trip and latch.**

- **thermo-nova:**
  - When the inside sensor is older than `sensor_max_value_age` (default 6 h, `const.py:170`), it
    resets the PID and skips the PID update (`climate.py:466-478`). `reset()` keeps the last error,
    so the output stays curve + P (`pid.py:126-137`).
  - The control loop keeps writing (`climate.py:544-545`).
  - There is one age limit for all sensors.
- **Firmware:**
  - A missing or out-of-range room temperature makes the tick return without a command. After 10
    such ticks (`SATcontrol.ino:143`) SAT sets `bSafetyTripped` and calls `satDisable()`, which
    sends CS=0 (`SATcontrol.ino:4836-4850`).
  - Every later pass stops at `SATcontrol.ino:4747`. The trip clears only on an explicit enable or a
    reboot.
  - Age limits: 300 s for BLE, the thermostat Tr and areas (`SATcontrol.ino:140-141`, `:162`), 6 h
    for the MQTT room value, 600 s for the MQTT outside value.
  - At boot, a room source that is slower than 5 min trips SAT at about I+300 s.
- **Applies:** entry 27 / conflict C8 (open since 2026-09-24: he proposed 20 min). entry 26 fits a
  hand-back to the thermostat on PIC boards. It does not cover the latch or a board without a
  thermostat.
- **Impact:**
  - SAT does not come back by itself when the sensor returns.
  - On a BLE-only OT-Direct install, heating likely stops. What MsgID 1 then carries was not traced.
  - When BLE has a gap, the firmware first uses the next source (thermostat Tr), if there is one.
- **Action:** ask Sergeant D three questions, one at a time:
  1. Trip, or hold like thermo-nova?
  2. 5 min, 20 min or 6 h?
  3. Should SAT resume by itself when a fresh room reading arrives?

**#5. Outside temperature is never missing.**

- **thermo-nova:** skips stale, unknown and unavailable outside sources and returns None
  (`climate.py:317-343`). With None, the control loop sends nothing (`climate.py:544-545`).
- **Firmware:**
  - `Toutside` starts at 0.0 (`OTGW-Core.h:82`). `satGetOutsideTemp()` uses the weather value only
    when `Toutside == 0.0` (`SATcontrol.ino:1451-1471`).
  - The weather loop does nothing for 300 s after boot (`SATweather.ino:27`, `:418-420`).
  - `weather.bValid` is never cleared (`SATweather.ino:377`).
  - 0.0 passes the -40..50 check (`SATcontrol.ino:4887-4889`).
- **Applies:** no entry. sat-comparison.md item 1 lists it, but it was never put to him.
- **Impact:**
  - Arithmetic from `heating_curve.py:28-39` with coefficient 1.5: 34.7 °C at 0 °C against 29.1 °C
    at 12 °C. At 16 °C the true curve (28.0) is below the cold cutoff, so SAT should not heat. It
    does, for the boot minutes.
  - Without any outside source, SAT heats on the 0 °C curve all year.
  - An old weather value is used for as long as the unit runs.
- **Action:**
  - Treat "never received" and "too old" as missing.
  - Ask Sergeant D whether SAT should then stop writing (thermo-nova), hold, or use a fixed value.

**#6. PWM ON phase timed from the wrong flame-on.**

- **thermo-nova:**
  - Starting the ON phase sets `_last_update` and the flame-wait flag (`pwm.py:162-164`).
  - It waits up to 180 s for ignition and restarts the timer when the flame lights
    (`pwm.py:185-196`).
  - The ON phase ends when the time since that anchor reaches on_time (`pwm.py:205`).
- **Firmware:**
  - The ON phase ends on `millis() - satCycleGetFlameOnStartMs()` (`SATcontrol.ino:935-944`), the
    cycle tracker's last real flame-on, with no flame-wait test.
  - The OFF phase uses the previous real flame-off (`SATcontrol.ino:960-961`).
  - Sequence when the boiler does not light within one 30 s tick:
    1. Tick 1 starts ON: return + 18, CH=1.
    2. Tick 2 ends ON, because the "flame-on" is from the previous burn: CS=10, CH=0.
    3. Tick 3 starts ON again, and so on.
  - After a switch to PWM during a long burn, the first ON phase ends at the next tick.
  - The comment at `SATcontrol.ino:939` claims parity with pwm.py. It is wrong.
- **Applies:** entry 8 (follow pwm.py) and entry 17 (CH=1 during ON). Sergeant D has already
  decided both.
- **Impact:**
  - Boilers with long pre-purge or anti-cycling get a heat request that keeps switching on and off
    (estimate).
  - Every PWM entry during a burn is cut short.
- **Action:** time the ON phase from its own start, or from ignition, as in pwm.py. Share the plan
  with Sergeant D first.

**#7. The wizard writes 1.4 / 0.6 / 0.8 as the coefficient.**

- **thermo-nova:**
  - The coefficient defaults to 2.0 for every system (`const.py:182`).
  - The setup flow never asks for it. It is an option only, 0.1-12 (`config_flow.py:616-617`).
- **Firmware:**
  - The wizard has slopes 1.4 / 0.6 / 0.8 (`data/v2.js:5368`). On Finish with "Automatic" (the
    default) it writes them to `satcoefficient` (`data/v2.js:5426-5434`).
  - Enabling SAT from the UI always opens the wizard (`data/v2.js:2736`, `:4959`).
  - Existing units see a "migrate" run (`data/v2.js:4441`). That run overwrites their coefficient
    only if the user presses Finish; it can be dismissed.
- **Applies:**
  - entry 24 (2026-10-20: "Curve Coefficient 2.0", "Leave existing settings alone").
  - entry 34 (the wizard asks the questions of the HA SAT config flow, which has no coefficient).
- **Impact:** the underfloor arithmetic is in the table. Units enabled through the UI end at
  1.4 or 0.6.
- **Action:**
  - The wizard writes 2.0, or nothing, for "Automatic".
  - It leaves a stored value alone on "migrate".
  - Sergeant D has already decided this.

**#8. [TASK-1331, uncommitted] Classifier reference setpoint.**

- **thermo-nova:**
  - Computes the tail error per sample as flow minus the control setpoint of that sample
    (`cycles/tracker.py:118-119`, `:267-271`; samples carry `control_setpoint` at
    `heating_control.py:250-260`).
  - The continuous control setpoint follows the boiler down with offset 2.0
    (`heating_control.py:450-489`).
  - A sample without a control setpoint is skipped.
- **Firmware:**
  - Uses one value per cycle: `fFinalSetpoint` at flame-on (`SATcycles.ino:744`). That value is 0.0
    at boot (`SATtypes.h:159`) and after `satDisable()` (`SATcontrol.ino:2338`).
  - Flame edges are processed before the first control tick (`SATcontrol.ino:4757-4762`), so a
    burning flame at boot, at enable or at the MQTT fallback starts a cycle against 0.0.
  - OVERSHOOT is tested first (`SATcycles.ino:701`), and TASK-1331 then switches to PWM
    (`SATcycles.ino:874-877`).
  - The same happens when the request rises during a burn, and on a mixed cycle (started in PWM,
    ended in continuous).
  - A partial rescue: if the second tick still sends the cold cutoff, the firmware leaves PWM again
    (`SATcontrol.ino:5145-5150`). That only works when the room temperature did not change between
    ticks 1 and 2 (`SATpid.ino:286-288`).
- **Applies:** entry 15 (port on_cycle_end, align the classifier first) and entry 18. thermo-nova
  cannot produce this switch, so the firmware does not follow what entry 15 intends. entry 22 (cold
  start runs in continuous) is defeated at boot.
- **Impact:**
  - A wrong switch to PWM after reboots, OTA updates, enables and rising requests. Alpha testers
    flash often.
  - This is the reason TASK-1331 is blocked.
  - Priority high, because the problem happens every time those events occur during a burn,
    although only once the change ships.
- **Action:**
  - Store the control setpoint per sample (a 180-float ring is 720 bytes; entry 29 point 2 leaves
    the RAM call to Robert).
  - Do not classify a cycle that started before SAT's first heating command.
  - See #22 before choosing the margin.

**#9. Heat pump PWM.**

- **thermo-nova:**
  - The minimum on-time is a fixed 180 s for every system (`pwm.py:53`) and the period is
    3600 / cycles per hour (`pwm.py:54`).
  - Heat pumps get 2 or 3 per hour (`config_flow.py:686-693`).
  - All mode switches run for every system.
- **Firmware:**
  - For a heat pump: 2 cycles per hour (`SATcontrol.ino:350`) and min-on 1800-2400 s
    (`SATcontrol.ino:363`, `SATtypes.h:440`).
  - Then dutyLower = 1.0 or 1.33 and dutyMin = dutyMax (`SATcontrol.ino:889-892`), so duty ranges
    2-4 can never be reached.
  - The runtime exits are skipped for heat pumps (`SATcontrol.ino:5124`).
  - **[TASK-1331, uncommitted]** The end-of-cycle switch has no heat-pump gate.
- **Applies:**
  - entry 30: "2 cycles/hour ( 1800sec)" and "1.5 cycles/hour ( 2400sec )". These read as periods,
    not as minimum on-times.
  - Conflicts C3 ("ONLY max modulation" against "A is the answer") and C4 (1.5 / 2 against 2 / 3)
    are open.
- **Impact:**
  - At 1800 s: duty of 0.5 or more gives continuous ON. Below 0.5: 30 min on / 30 min off.
  - At 2400 s: lower demand gives more run time.
  - With TASK-1331: a heat pump in PWM leaves only through UNDERHEAT at a cycle end, the maximum
    check, or the cold cutoff.
- **Action:** ask Sergeant D whether 1800 / 2400 s are periods or minimum on-times, and which heat-
  pump rules besides MM he wants. Until he answers, gate the end-of-cycle switch for heat pumps.
- **[TASK-1366] Duty mapper fixed:** he answered (entries 40 and 50): minimum ON 180 s, and 1800 /
  2400 s is the PWM period. `satGetMinOnTimeSec()` returns 180 for every source and the new
  `satGetPwmPeriodSec()` gives the period (`SATcontrol.ino:357-374`, `:875-877`); host test
  `tests/test_sat_hp_pwm.py`. Still open: the runtime exits skipped for heat pumps (C3), hybrids
  (entries 50/52) and the hourly cap (Q12). Side effect: heat-pump duty 0.05-0.95 (1800 s) now runs
  through the ON/OFF state machine, where the pre-existing ON-end timing from the previous flame-on
  edge (`SATcontrol.ino:949-958`) ends an ON phase one tick after its start when the unit has not lit
  yet (thermo-nova waits up to 180 s, `pwm.py:181-199`). Reachable for a heat pump only through the
  manual `control_mode` "pwm" command; open, to be asked.

### Medium

**#10. MM during hot water.**

- **thermo-nova:** returns the HOT_WATER state before the PWM check (`heating_control.py:136-137`)
  and sends the configured maximum MM (`:517-520`) on every update.
- **Firmware:** the DHW branch returns before any MM write (`SATcontrol.ino:4801-4818`). It still
  publishes `modulation_state = HOT_WATER` (`:3195-3196`).
- **Applies:** entry 21. He approved the Task #85 acceptance criteria, which include "DHW active:
  MM=max_modulation". That reading of his "ACs reviewed, now everything is correct" is ours.
- **Impact:** every draw during PWM. Two reviewers disagreed on the priority: high, or medium
  because neither code base shows whether a boiler applies MsgID 14 to DHW. We chose medium. It is
  high if boilers do apply it.
- **Action:** send the maximum MM while DHW is active.

**#11. The DHW guard never arms.**

- **thermo-nova:** blocks the overshoot switch, and resets the disable guards, during DHW and for
  300 s after it (`heating_control.py:268-274`, `:313-315`, `const.py:19`).
- **Firmware:**
  - The guard is refreshed only inside `satCycleCheckAutoSwitch()` when DHW is active
    (`SATcycles.ino:1008-1011`).
  - That function is called after the DHW early return (`SATcontrol.ino:4817`, `:5124`), so the
    guard never arms.
  - Accumulated overshoot seconds from before the draw survive it.
  - The comment at `SATcontrol.ino:5122-5123` (3.0 °C margin, working guard) is stale.
- **Applies:** entry 22 cites this guard as matching thermo-nova. That claim is wrong in effect.
- **Impact:** needless switches into PWM after showers on combi boilers.
- **Action:** port the guard as in thermo-nova.

**#12. Sustain counting.**

- **thermo-nova:** the first qualifying sample starts the timer at 0 (`helpers.py:51-56`). The
  switch happens after 60 s (`heating_control.py:296`).
- **Firmware:** adds the gap since the previous call, so the second 30 s tick reaches 60
  (`SATcycles.ino:993-1028`). A gap above 60 s returns early, so a control interval above 60 s
  disables all three switches.
- **Applies:** conflict C1. He asked for 3 minutes on 2026-04-07 and said 60 s on 2026-10-20 ("not
  sure for the interval"). He never withdrew the 3-minute ruling.
- **Impact:** listed in the table.
- **Action:**
  - Count from the first qualifying tick, as in thermo-nova.
  - Ask C1 once.

**#13. First PID call returns 0.**

- **thermo-nova:** computes the output on read as curve + P + I + D (`pid.py:126-132`) and sends it
  on the first pass, about 1 s after start (`climate.py:133-144`, `:530`).
- **Firmware:**
  - `satPidReset()` sets the output to 0. The first update returns the stored 0, because elapsed is
    0 s (`SATpid.ino:266-283`), and tick 2 often returns 0 again (`:286-288`).
  - 0 is clamped to 10, and the cold cutoff sends CS=10, CH=0 (`SATcontrol.ino:5086`, `:5145-5159`).
  - At boot the firmware sends CS=0 even with SAT enabled (`SATcontrol.ino:3873-3877`).
  - A flush is manual only.
- **Applies:**
  - entry 22 (cold start sends "CH=1 CS= The output of the Heating curve + PID MM=100").
  - entry 26 ("ONLY SAT should drive TSet").
  - entry 28 (PID identical to pid.py).
- **Impact:** listed in the table.
- **Action:** compute an output on the first call. Plan OK needed.

**#14. PID cadence.**

- **thermo-nova:** runs the PID every 60 s (`climate.py:712-716`, `pid.py:20`) and adds
  ki × error × 60 each time (`pid.py:191`).
- **Firmware:**
  - Runs the PID on each control tick when the error changed by 0.001 or more (`SATpid.ino:279-288`)
    and adds the same 60 s step (`:169`).
  - The room filter changes the error on almost every tick.
  - ADR-069 claims that the PID does not depend on the interval. That claim is false.
- **Applies:** entry 28 ("identical to pid.py"). He was asked about the 10 s gate on 2026-06-21
  and did not answer.
- **Impact:** inside the ±0.1 °C band only (see the table).
- **Action:** a fixed 60 s PID step, or a step based on elapsed time. Ask him, because his 2026-10-03
  "don’t change anything" could be read as covering this.

**#15. PID resets.**

- **thermo-nova:** resets the integral and the raw derivative on every target change, every HVAC
  mode change, window changes and the reset_integral service (`climate.py:641-660`, `:570-583`,
  `:808-832`, `:217-222`; `pid.py:134-137`).
- **Firmware:**
  - The target paths call no reset (`SATcontrol.ino:2071-2102`, `settingStuff.ino:998`).
  - The window and reset_integral paths clear only the integral (`SATpid.ino:86-91`).
  - A preset change zeroes only the published `fPidI` (`SATcontrol.ino:1161-1171`).
- **Applies:** entry 28. No entry approves this.
- **Impact:** listed in the table. This happens on every schedule or thermostat change.
- **Action:** align with thermo-nova.

**#16. PWM maximum check cannot fire.**

- **thermo-nova:** clamps the request to `limits.maximum_setpoint` and tests the same value
  (`climate.py:394`, `heating_control.py:203-204`).
- **Firmware:** clamps to the lowest of MaxTSet, the system cap, the hard cap and 65
  (`SATcontrol.ino:315-327`), but tests against 62/45/40 (`:5093`).
- **Applies:** entry 8 (follow pwm.py and thermo-nova).
- **Impact:** listed in the table.
- **Action:** compare with the same effective maximum.

**#17. First PWM phase is OFF.**

- **thermo-nova:** leaves PWM IDLE after reset, and IDLE starts the ON phase without waiting
  (`pwm.py:87-100`, `:155`).
- **Firmware:** resets to the OFF phase and waits off_time from the last flame-off
  (`SATcontrol.ino:1064-1080`, `:957-961`).
- **Applies:**
  - Entries 15 and 18 (follow thermo-nova's on_cycle_end).
  - entry 22 ("SAT enters in the PWM mode. ( CS = BT - 1 )").
  - Nothing approves the wait.
- **Impact:** listed in the table. The firmware behaviour is gentler on the boiler, but it is not the
  reference.
- **Action:** ask Sergeant D, because the change adds burner starts.

**#18. Hourly start cap.**

- **thermo-nova:** counts only PWM ON starts and compares them with `duty_cycle_seconds` (780)
  (`pwm.py:156`, `entry_data.py:219-220`). In practice the cap never takes effect, which looks like
  an upstream bug.
- **Firmware:** counts every real flame-on, DHW included (`SATcycles.ino:749-751`), against 3 per
  hour, and holds the OFF phase (`SATcontrol.ino:962-973`).
- **Applies:** no statement by him on a hard cap for gas boilers. The line "should not cycle more
  than 2 times per hour ever" is Robert's, not his.
- **Impact:** listed in the table.
- **Action:** ask whether a cap is wanted, and whether DHW ignitions should count.

**#19. ON phase with a short on-time.**

- **thermo-nova:** starts ON only when on_time is 180 s or more (`pwm.py:155`). At ultra-low duty
  without a flame it returns on=0 (`pwm.py:247-260`).
- **Firmware:** the OFF branch starts ON whatever the on-time is (`SATcontrol.ino:957-988`).
- **Applies:** entry 31 ("the ultra low means keep the boiler off").
- **Action:** add the 180 s condition.

**#20. Ignition timeout.**

- **thermo-nova:** after 180 s without a flame it only clears the wait flag and keeps the ON phase
  (`pwm.py:194-199`).
- **Firmware:** ends the ON phase (`SATcontrol.ino:945-956`). The comment there claims the opposite.
- **Applies:** entries 8 and 17.
- **Action:** align. Fix the comment together with #6.

**#21. BT EMA cadence.**

- **thermo-nova:** applies the EMA on every 5 s update while ON within 180 s (`pwm.py:143-149`,
  `climate.py:706-710`).
- **Firmware:** applies it once per 30 s tick (`SATcontrol.ino:865-872`).
- **Applies:** entry 1. The formula and the window match; the number of samples does not.
- **Impact:** listed in the table (estimate, not measured).
- **Action:** sample BT in the 3 s refresh, or on a 5 s timer.

**#22. [TASK-1331, uncommitted] Margin and flow offset.**

- **thermo-nova:** its clamp keeps the error near +2.0, below the margin 3.0
  (`heating_control.py:484-489`, `const.py:154`, `cycles/const.py:10`).
- **Firmware:** margin 2.0 (`SATtypes.h:454`) and flow offset 2.0 (`SATtypes.h:466`). With a per-
  sample reference, almost every clamped sample is at 2.0 or above.
- **Applies:** entry 15 (1b) set 2 "for this trigger". That interaction was not raised with him.
  entry 22 leaves open whether 2.0 also applies to the classifier.
- **Action:** put both together to Sergeant D when the #8 fix is planned.

**#23. Stored margin 3.0.**

- **Firmware:** commit 475b046e7 (2026-06-20) set the default to 3.0, and a7bbaccb6 (2026-10-21)
  returned it to 2.0, "stored values kept". There is no migration (`settingStuff.ino:1014`).
- **Applies:** entry 15.
- **Action:** a one-time migration of 3.0 to 2.0, or ask the testers to check.

**#24. Command cadence.**

- **thermo-nova:** sends CS, MM and CH after every step, every 5 s (`heating_control.py:236-238`,
  `climate.py:706-710`).
- **Firmware:**
  - Sends them on a change, or after 300 s (`SATcontrol.ino:427`, `:465-521`), on a 30 s tick.
  - The PIC keepalive repeats only CS. A short PIC reset clears MM but not the cache.
  - The MQTT `maxmodulation` and `chenable` commands reach the queue without a guard
    (`MQTTstuff.ino:509`, `:512`).
- **Applies:** entry 7. We named this cadence; he did not answer, and it is "kept without an
  explicit OK".
- **Action:** ask him. Until then, at least resend MM and CH after a PIC reset.

**#25. OT-Direct gateway-mode CH.**

- **thermo-nova:** sends CH= to the gateway, which rewrites the thermostat's MsgID 0.
- **Firmware:** CH= changes only the gateway's own MsgID 0 (`OTDirect.ino:2996-3004`, `:756-764`).
  In gateway mode, the thermostat's frames and the gateway's frames alternate.
- **Applies:** entries 5, 7, 9, 11 and 17. Sergeant D confirmed the behaviour on a PIC board.
- **Action:** TASK-1238.

**#26. Immergas TP=11:12.**

- **thermo-nova:** sends TP=11:12=min(MM, 80) and then MM uncapped
  (`coordinator/mqtt/opentherm.py:216-220`).
- **Firmware:** caps MM at 80 (`SATcontrol.ino:387`) and writes the flow setpoint into TP=11:12
  (`:5162-5166`).
- **Applies:** entry 21. He pointed at exactly this thermo-nova line.
- **Action:** align.

**#27. Ignition hold without Tret.**

- **thermo-nova:** logs a warning and keeps the setpoint (`heating_control.py:392-395`), which after
  an OFF phase is 10 with CH=0.
- **Firmware:** uses `Tret`, which is 0.0 when the boiler never reports it, so the hold becomes
  0 + 18, floored at 28.2/21 (`SATcontrol.ino:802-808`, `OTGW-Core.h:83`).
- **Applies:** entry 6 ("Intergas by default does not even report the Tret") and entry 7. Neither
  covers a missing Tret.
- **Action:** ask Sergeant D what to do without Tret.

**#28. Coefficient default.**

- **thermo-nova:** 2.0 (`const.py:182`), range 0.1-12.
- **Firmware:** 1.5 (`SATtypes.h:442`), range 0.1-5 (`settingStuff.ino:999`).
- **Applies:** entry 24 (2026-10-20 18:26 / 18:54) and conflict C5. TASK-1273 lists it as a
  remaining item.
- **Action:** set the default to 2.0 without migrating stored values, as he decided.

**#29. Maximum setpoint.**

- **thermo-nova:** 55 °C, or 50 °C for underfloor, with a user option of 10-100
  (`helpers.py:97-99`, `entry_data.py:226-239`, `config_flow.py:612-613`).
- **Firmware:** 62 / 45 / 40 (`SATcontrol.ino:303-310`) with no setter for `fMaxSetpoint`. The
  comment "C++ 40C matches Python's heat pump cap" (`SATcontrol.ino:302`) is false for this path.
- **Applies:** entry 30 option A (40 °C for heat pumps). No approval for 62/45, or for the missing
  setting.
- **Action:** ask Sergeant D.

**#30. Cycles per hour.**

- **thermo-nova:** default 4, radiator options 3 or 4 (`const.py:119`, `config_flow.py:695-701`).
- **Firmware:** default 3, range 2-6 (`SATtypes.h:524`, `settingStuff.ino:1105`). The same number
  is the start cap (#18).
- **Applies:** entry 30 ("We have the options 3 cycles/hour and 4 cycles/hour") and entry 31. No
  default was chosen.
- **Action:** ask for the default, and limit the range to 3-4 for gas boilers.

**#31. DHW setpoint.**

- **thermo-nova:**
  - Sends SW= at once when the user changes the value (`number.py:72-74`).
  - Shows the boiler's TdhwSet (`number.py:47-60`).
  - Uses fixed bounds of 30/60.
- **Firmware:**
  - Sends SW= only at the off-to-on edge of a draw, only with `bDhwEnabled` (default off), and
    truncates the value (`SATcontrol.ino:4806-4814`).
  - Shows its own stored setting (`:2564`).
- **Applies:**
  - entry 33: "When a DHW setpoint temperature change occurs you need to send the SW={value}
    command", "The current value is the TdhwSet", and use the MsgID 48 bounds.
  - Robert decided to keep the persistent setting (TASK-1232). That is a maintainer decision, not
    his.
  - His MsgID 48 bounds differ from thermo-nova's fixed 30/60.
- **Action:** put E33 and the TASK-1232 decision side by side to both of them.

**#32. Zones.**

- **thermo-nova:**
  - COMFORT mode takes max(main, areas), then the room overshoot cap (`climate.py:381-389`).
  - The areas use the 75th percentile with +5 headroom, limited to the highest room
    (`area.py:273-301`).
  - A room counts when its output is above 22 or its valve is at least 10 % (`area.py:175`).
  - The cap is 22 + 10 - 4 × degrees over, with a floor of 22 (`area.py:304-333`).
  - Each area has a full PID on its own 60 s timer.
- **Firmware:**
  - Counts a zone above 10.
  - One active zone replaces the main output. Two or more give percentile + 5 minus the largest
    overshoot (`SATcontrol.ino:5013-5066`).
  - The zone PID has P and I only, stepped every tick (`SATcontrol.ino:1974-2035`).
- **Applies:**
  - entry 35 (port PR #172). Porting PR #172 does not approve these deviations.
  - Entry 11 answer 2 ("28.2/21") listed only heating_control.py sites. Whether it also covers
    `area.py:175/322/331` (and `cycles/classifier.py`, `device/status.py:121`) is open.
- **Action:** ask the scope of entry 11 first, then align with area.py.

**#33. SSI scope.**

- **thermo-nova:** applies the SSI to the inside sensor, using indoor humidity only
  (`climate.py:270-287`). Without humidity, there is no room temperature.
- **Firmware:**
  - Applies the SSI only to the thermostat Tr (`SATcontrol.ino:1432-1440`).
  - Copies the outdoor `relative_humidity_2m` into the humidity value (`:4983-4988`,
    `SATweather.ino:262`).
- **Applies:** entry 32. TASK-1282 waits for his OK.
- **Action:** use indoor humidity only, for every room source.

**#34. BLE overrides the multi-area average.**

- **thermo-nova:** has no weighted average.
- **Firmware:** returns fresh BLE before the average (`SATcontrol.ino:1376-1390`). Area-mapped BLE
  sensors still count as "first fresh" (`SATble.ino:817-857`).
- **Applies:** entry 35. Whether the weighted average should exist at all is open (sat-comparison).
- **Action:** exclude area sensors from the main BLE choice.

**#35. Heat pump cold setpoint.**

- **thermo-nova:** one heating-system axis, COLD_SETPOINT 22 (`types.py:110-118`, `const.py:34`).
- **Firmware:** returns 21 only for underfloor, else 28.2 (`SATcontrol.ino:339-343`). The wizard
  writes system AUTO for a heat pump (`data/v2.js:5368`), and AUTO maps to radiators.
- **Applies:** entry 30. Device type and heating system are separate, and the system sets the cold
  setpoint.
- **Action:** the wizard asks for the distribution system. A confirmation from him is enough.

**#36. Reference docs drift.** The working-tree docs still say, among other things:

- "below 28.2", "CH=1 when the final setpoint is above 10", "modulation fixed 100", and "the mode
  stays PWM and nothing is reset" on the cold cutoff (sat-comparison.md lines 59, 167, 361-364, 446,
  447, 483). Entries 7, 9, 11, 12 and 13 settled these.
- The solar offset applied after the clamp, and "bSolarFreezeIntegral is not read"
  (firmware-sat-spec.md 643, 1041, 1132, 1163, 1424, 1447, 1537, 1637, 1857; sat-comparison.md 354,
  355, 397). TASK-1323 changed both.
- Valves closed leave PWM unchanged (firmware-sat-spec.md 1572, 1896). This is now true only for
  summer mode.
- A heat pump enters PWM "only by force or the manual command" (firmware-sat-spec.md around 1434 and
  1897). Force PWM is gone (entry 13).
- "The firmware has no such rule" for the cycle verdicts (sat-comparison.md 74), against the rows
  updated for TASK-1331.
- Only PM=48 reaches a PIC at boot (firmware-sat-spec.md 3497, 3802; sat-comparison.md 100).
  TASK-1315 now sends 3, 15 and 48 one at a time.
- The adopt rule with a 0.25 °C window (firmware-sat-spec.md 2651), which entry 2 dropped. The open
  preset question in sat-comparison.md row 41, which entry 25 answered.
- The cold setpoint in the classifier listed as "align or ADR" (sat-comparison rows 29, 34), which
  entries 7 and 11 approved.
- The DHW branch at the old lines `SATcontrol.ino:4625-4652` (now 4787-4818).

**Action:** correct these docs with a dated note per change (entry 4), not silently.

### Low

**#37. Approved deviations with no recorded entry.**

- **Overshoot protection on by default for all systems.** thermo-nova defaults to off
  (`const.py:122`). entry 20: "start up + suppression should be the option for all the heating
  systems", then "This looks ok". sat-comparison.md item 1 still asks him to decide.
- **Margin 2.0 for the 60 s runtime switch.** entry 22 approves it; entry 15 covers only the end-of-
  cycle trigger.
- **The room filter (tau 45 s) before the PID.** entry 28: "Yes implement it. Leave the pid
  untouched and add a filter to the raw sensor’s data". sat-comparison row 25 still says "confirm".
- **Excluding OFF zones.** This follows PR #172, which is not in the pinned commit. Taking entry 35
  as approval is our reading; we did not open PR #172.

**Action:** record E20, E22, E28 and E35, then update the docs.

**#38. PID extras.**

- **thermo-nova:** DEADBAND is a constant 0.1 (`const.py:16`). The integral clamp is ± curve only.
  P, I and D are rounded, and the output is rounded to 0.1. The "no change" test is exact.
- **Firmware:**
  - The deadband is a setting, 0.05-2.0 (`settingStuff.ino:1000`).
  - The deadband widens by 0.5 °C per hour of estimation (`SATcontrol.ino:4913-4917`).
  - Extra ±20 integral cap; no rounding; a 0.001 test (`SATpid.ino:166-212`).
  - The widening skips its restore on two early returns. A second reader showed that the leak is
    about 1e-6 °C per tick, so it is a latent defect, not a real drift.
- **Applies:** entry 28 ("the integral clamps between the deadband").
- **Action:** ask whether to keep the setting. Fix the restore anyway.

**#39. Auto-tune.**

- **thermo-nova:** has no auto-tune code.
- **Firmware:** off by default (`SATtypes.h:510`). When on, it scales the coefficient by up to ±2 %
  per hour and saves it (`SATcontrol.ino:4552-4660`). TASK-1331 changes its input, so in periods
  with much PWM the score moves toward undershoot.
- **Applies:** entry 29 ("Python always wins").
- **Action:** ask whether to keep or remove it.

**#40. Derivative reference.**

- **thermo-nova:** overwrites `_last_temperature` on every update and times the slope by
  `last_changed` (`pid.py:171`, `:202-232`).
- **Firmware:** moves the reference only together with the timestamp (`SATpid.ino:49-56`, `:205-249`).
- **Applies:** TASK-1195 (2026-10-03 07:21 UTC) came before his 08:42 UTC "No, don’t change
  anything. Everything looks good now". Whether the question he answered covered this was not
  checked.
- **Action:** ask Sergeant D.

**#41. PID frozen while SAT does not control.**

- **thermo-nova:** the 60 s PID timer runs regardless of HVAC or heater state (`climate.py:712-716`).
- **Firmware:** the early returns come before `satPidUpdate` (`SATcontrol.ino:4882-4957` vs `:5006`).
- **Applies:** entry 32 covers only "valves closed means CS=10".
- **Action:** ask Sergeant D.

**#42. PID restore.**

- **thermo-nova:** restores five values with no age check (`pid.py:141-157`) and saves after every
  update.
- **Firmware:**
  - Restores only I and D, and only with NTP and an age of 1800 s or less
    (`SATcontrol.ino:2161-2213`).
  - Saves every 300 s and saves zeros on disable.
  - The one-time retry after NTP can overwrite a running integral (`SATcontrol.ino:4681-4685`).
- **Action:** ask whether the age limit is wanted. Fix the retry.

**#43. Restart state.**

- **thermo-nova:** restores PWM-enabled, CS and MM (`pwm.py:111-115`, `heating_control.py:181-190`).
- **Firmware:** starts continuous with no previous CS (`SATcontrol.ino:3856-3858`, `:3888-3889`,
  `:4783-4785`).
- **Applies:** entry 22 describes the cold start as continuous. It does not say whether that also
  applies to a warm restart.
- **Action:** ask Sergeant D.

**#44. Runtime enable.**

- **thermo-nova:** sends its boot commands on every coordinator start.
- **Firmware:** sends PM and MI only in `initSAT()` (`SATcontrol.ino:3888-3899`).
- **Action:** also send them at a runtime enable.

**#45. MI=500.**

- **thermo-nova:** sends MI=500 for a configured Ideal, Intergas or Nefit boiler
  (`coordinator/mqtt/opentherm.py:166-172`). The manufacturer is a required setup question
  (`config_flow.py:531`).
- **Firmware:** the default is AUTO (`SATtypes.h:471`), and detection has not run yet when
  `initSAT()` checks (`SATcontrol.ino:3894-3898`). sat-comparison calls this a "match", which is
  wrong.
- **Extra risk:** on OT-Direct, MI= is also saved as a setting (`OTDirect.ino:3577`).
- **Action:** ask why MI=500 is needed. Send it once detection resolves a quirk brand.

**#46. Manual gains.**

- **thermo-nova:** lets the user switch automatic gains off and enter P/I/D, with defaults 45 / 0 /
  6000 (`const.py:113-115`, `config_flow.py:490-505`).
- **Firmware:** has the code path but no setter. Its defaults are 5 / 0.0005 / 0
  (`SATtypes.h:517-520`).
- **Action:** ask whether manual mode is wanted. If it is, use thermo-nova's defaults.

**#47. ECO mode.**

- **thermo-nova:** in ECO it returns the main PID output before the zones and solar
  (`climate.py:377-379`).
- **Firmware:** stores the mode and never reads it.
- **Action:** ask whether to implement or remove it.

**#48. Summer simmer, comfort offset and PV boost.**

- **thermo-nova:** SummerSimmer is only an index that replaces the room temperature with the comfort
  option on (`climate.py:276-277`). It has no outdoor lockout, no humidity offset and no PV boost.
- **Firmware:**
  - `bSummerSimmer` is an outdoor-hours heating lockout (`SATcontrol.ino:4472-4520`).
  - The index sits under `bThermalComfort`.
  - The humidity offset requires `bSummerSimmer` (`SATcontrol.ino:3391`).
  - The offset and the PV boost shift both the curve and the PID target (`:4998-5006`).
- **Applies:** entry 32 ("We can keep it") was about the index. The bot announced the lockout in
  April without an answer. When the offset was tied to `bSummerSimmer`, the bot told him and he did
  not object. Silence is not approval.
- **Action:** ask whether to keep the lockout, and under which name. Tie the offset to the comfort
  option, or remove it.

**#49. Summer cutoff and PWM state.**

- **thermo-nova:** every heater-off case runs `pwm.disable()` and resets the guards
  (`heating_control.py:229-234`).
- **Firmware:** the summer path does not call `satPwmDisableOnHeaterOff()`
  (`SATcontrol.ino:4930-4942`).
- **Applies:** entry 12 (as for the cold cutoff).
- **Action:** call it on the summer path too.

**#50. Raw MM on the idle paths.**

- **thermo-nova:** sends the raw maximum on heater off (`heating_control.py:234`).
- **Firmware:** does the same (`SATcontrol.ino:1102`, `:4907`, `:4937`, `:4951`), so the heat-pump
  rule is skipped.
- **Applies:** entry 21 ("never send the 0%" for heat pumps).
- **Action:** apply the heat-pump rule on these paths.

**#51. Heat pump MM=100 outside PWM.**

- **Firmware:** `SATcontrol.ino:382`. thermo-nova has no heat-pump MM rule.
- **Applies:** entry 21 quotes him two ways ("it is safer to let the max modulation to 100%" and "100% or the
  desired max modulation limit the user selects").
- **Action:** ask which reading is meant.

**#52. TC= after a cache reset.**

- **thermo-nova:** pushes only when the target changes (`climate.py:655-657`).
- **Firmware:** also pushes after boot, enable and a bus gap (`SATcontrol.ino:443`).
- **Applies:** entries 14 and 16 cover push-on-change only. This is Robert's call under entry 2, not
  Sergeant D's.
- **Action:** Robert confirms or removes it.

**#53. PM=25.**

- **thermo-nova:** inconsistent. The MQTT coordinator sends PM=25 after every CS; the serial
  coordinator does not.
- **Firmware:** never sends it.
- **Action:** ask whether it is needed (entry 23: the suppression CS follows the flow every few
  seconds).

**#54. Classifier details.**

- **thermo-nova:**
  - Cuts the first min(60 s, 25 %) of a cycle from the tail (`cycles/tracker.py:113-116`).
  - Returns UNCERTAIN for kind UNKNOWN and for PWM IDLE (`cycles/classifier.py:22-29`).
  - Uses the CH bit for the kind (`cycles/tracker.py:103-108`, `:314-324`).
- **Firmware:**
  - None of the above (`SATcycles.ino:563-570`, `:663-721`).
  - Adds label-only rules: continuous SHORT under 60 s, UNCERTAIN under 120 s, PWM SHORT whatever
    the phase.
  - Records cycles under 3 samples as INSUFFICIENT.
- **Applies:** entry 15 (1d) asks to align with classifier.py. The thermo-nova coordinators disagree
  on which bit means "central heating" (serial: master CH enable; MQTT: slave CH mode). We should
  not pick one silently.
- **Action:** align the guards. Ask which CH bit is meant.

**#55. Last cycle class.**

- **thermo-nova:** expires it after 6 h (`cycles/history.py:48-57`).
- **Firmware:** never expires it (`SATcycles.ino:588`).
- **Action:** add the 6 h expiry.

**#56. Duty range 5.**

- **thermo-nova:** only above max does it give on = 2 × period, and the state machine still runs
  (`pwm.py:285-301`).
- **Firmware:** `>=` and an early return (`SATcontrol.ino:898-903`).
- **Action:** align.

**#57. Mode-switch order.**

- **thermo-nova:** switches before it computes MM and CS (`heating_control.py:202-228`).
- **Firmware:** computes CS first, then switches, then computes MM (`SATcontrol.ino:5099-5140`).
- **Action:** move the switch before the setpoint step.

**#58. "PWM auto switch" setting.**

- **Firmware:** still gates all three runtime switches (`SATcycles.ino:991`).
- **Applies:** entry 19 removes it. Our question in entry 19 said it "only blocks the 60-second
  overshoot switch". It also blocks the underheat and saturation exits. His answer very likely
  stands, but he should be told (conflict C13).
- **Action:** TASK-1341, and tell him about C13.

**#59. DHW and the PWM schedule.**

- **thermo-nova:** times PWM phases from its own `_last_update` (`pwm.py:162`, `:188`, `:208`).
- **Firmware:** times the OFF wait from the last flame-off, including DHW flame-offs
  (`SATcontrol.ino:960`).
- A second reader corrected the original claim that a draw can end an ON phase early: a draw moves
  the anchor later, not earlier.
- **Action:** use PWM's own phase timer.

**#60. HEATING_HOT_WATER.**

- **thermo-nova:** sets this status with the flame on and hot water active
  (`device/status.py:87-88`).
- **Firmware:** the status function runs after the DHW return (`SATcontrol.ino:4981`), so the status
  cannot occur. TASK-891.5 marked it Done.
- **Applies:** entry 29 point 4.
- **Action:** evaluate the status before the DHW return.

**#61. 16-bit counters.** `SATcycles.ino:117-118`, counted on every loop() pass (`:961-965`). The
loop rate was not measured.

- **Action:** use 32-bit counters, or count on the 1 Hz sample.

**#62. Boiler fault freeze.**

- **thermo-nova:** has no fault check in control.
- **Firmware:** returns on SlaveStatus bit 0 (`SATcontrol.ino:4862-4866`), while the PIC keepalive
  repeats the last CS.
- **Action:** ask Sergeant D. At least tell the user.

**#63. Thermal estimate unreachable.**

- **Firmware:**
  - The estimate is used only for an out-of-range Tr (`SATcontrol.ino:1417`), but a lost Tr is NAN.
  - The learning code stores the estimate as the last known room value (`:4269-4279`), so the 2 h
    timer never runs out.
- **Action:** fix the NAN path, or remove the feature and its docs.

**#64. Flame status dead branches.** `SATcontrol.ino:3804-3825` and `:4747`.

- **Action:** remove the branches, or make them reachable.

**#65. Simulation end.** `SATcontrol.ino:1610-1631`.

- **Action:** reset the PID and the cycle state when a real boiler appears.

**#66. Published curve.** `SATcontrol.ino:598`, also called per zone at `:1996`.

- **Action:** publish only the main curve.

**#67. Wizard tuning text.**

- **thermo-nova:** `automatic_gains_value` is stored but never read (`entry_data.py:212`).
- **Firmware:** the same holds (`SATpid.ino:120-141`). But the wizard says "SAT learns your home's
  response over ~2 weeks" and offers a multiplier field with no effect (`data/v2.js:5531`, `:5540`).
- **Action:** fix the text and remove the field.

**#68. Preset defaults.**

- **thermo-nova:** away 10, home 18, sleep 15, comfort 20, window 15 s (`const.py:172-179`).
- **Firmware:** away 15, sleep 16, comfort 21, window 60 s (`SATtypes.h:446-464`).
- **Action:** Robert decides. Optionally ask him one batched question.

**#69. ADR drift.**

- ADR-069 states a deadband of 0.25 (code 0.1), a clamp of [0, curve] (code symmetric), interval
  independence (false, #14), and a reset on coefficient change (no such call).
- ADR-071 states a coefficient default of 1.0, a range of about 0.4-2.0, and an "OT-Thing" origin.
- ADR-195 marks the "<=" boundary as settled by entry 12, but entry 12 answered a different question,
  and entries 7 and 11 still list the boundary as open. Its rule "decide CH only in
  satChEnableDecide" does not hold for three early-return paths.

**Action:**

- Write superseding ADRs through adr-kit, because Accepted ADRs are not edited.
- Ask Sergeant D one line about "<=": thermo-nova uses `<=` (`heating_control.py:72`, `:385`).

---

## 4. Sergeant D statements recorded by this audit (entries 20-35)

**Count:** 16 statements, recorded as entries 20-35 (audit drafts D2-D17). Draft D1 ("remove it", PWM auto switch) is now entry 19. The full
entries, with quotes and citations, are in `sergeant-d-corrections.md`. They are now
recorded in `sergeant-d-corrections.md` as entries 20-35 (D2 = entry 20 ... D17 = entry 35).

Link prefixes:

- #alpha-testing = `https://discord.com/channels/812969634638725140/1514720723980259460/<id>`
- #dev-sat-mqtt = `https://discord.com/channels/812969634638725140/1105556725714649128/<id>`

| Entry | Topic | Date | Main links |
|---|---|---|---|
| E20 | Overshoot protection (ignition hold + suppression) on by default for all heating systems | 2026-10-20 | [1562170196897038447](https://discord.com/channels/812969634638725140/1514720723980259460/1562170196897038447), [1562187666328649729](https://discord.com/channels/812969634638725140/1514720723980259460/1562187666328649729), [1562190051071361119](https://discord.com/channels/812969634638725140/1514720723980259460/1562190051071361119) |
| E21 | MM: 0 in PWM, never 0 for heat pumps and hybrids, Geminox at least 10; DHW active means MM = maximum | 2026-04-05 to 2026-10-20 | [1490446199092674793](https://discord.com/channels/812969634638725140/1105556725714649128/1490446199092674793), [1490449270917038110](https://discord.com/channels/812969634638725140/1105556725714649128/1490449270917038110), [1490450926572666931](https://discord.com/channels/812969634638725140/1105556725714649128/1490450926572666931) |
| E22 | Continuous to PWM on overshoot: cold start in continuous, margin 2.0 °C for 60 s | 2026-10-20 | [1562200581961941054](https://discord.com/channels/812969634638725140/1514720723980259460/1562200581961941054), [1562201355316232316](https://discord.com/channels/812969634638725140/1514720723980259460/1562201355316232316), [1562201731113422919](https://discord.com/channels/812969634638725140/1514720723980259460/1562201731113422919) |
| E23 | Ignition hold: a settable delay, then straight to flow minus 1, refreshed every few seconds | 2026-10-20 | [1562185474288652301](https://discord.com/channels/812969634638725140/1514720723980259460/1562185474288652301), [1562188371965517906](https://discord.com/channels/812969634638725140/1514720723980259460/1562188371965517906), [1491108815826059477](https://discord.com/channels/812969634638725140/1105556725714649128/1491108815826059477) |
| E24 | Heating curve coefficient default 2.0; stored values stay; the convex curve is deliberate | 2026-10-20 | [1562170196897038447](https://discord.com/channels/812969634638725140/1514720723980259460/1562170196897038447), [1562177212155826199](https://discord.com/channels/812969634638725140/1514720723980259460/1562177212155826199), [1502183820454006835](https://discord.com/channels/812969634638725140/1105556725714649128/1502183820454006835) |
| E25 | The thermostat as user interface: two-way room setpoint, on change only; an adopted change clears the preset | 2026-10-19 and 2026-10-21 | [1561668633283928136](https://discord.com/channels/812969634638725140/1514720723980259460/1561668633283928136), [1561677173134336072](https://discord.com/channels/812969634638725140/1514720723980259460/1561677173134336072), [1561798500923080906](https://discord.com/channels/812969634638725140/1514720723980259460/1561798500923080906) |
| E26 | SAT owns the control setpoint while enabled; the thermostat is the fallback | 2026-04-05 to 2026-10-23 | [1490310838181953598](https://discord.com/channels/812969634638725140/1105556725714649128/1490310838181953598), [1490311385186439270](https://discord.com/channels/812969634638725140/1105556725714649128/1490311385186439270), [1490311558373310535](https://discord.com/channels/812969634638725140/1105556725714649128/1490311558373310535) |
| E27 | Room temperature input: stale timeout (20 min suggested), a way to push it, redundancy, two decimals | 2026-04-05 to 2026-09-24 | [1552681842749079614](https://discord.com/channels/812969634638725140/1514720723980259460/1552681842749079614), [1490313103060107355](https://discord.com/channels/812969634638725140/1105556725714649128/1490313103060107355), [1490314021239394335](https://discord.com/channels/812969634638725140/1105556725714649128/1490314021239394335) |
| E28 | The PID: integral only inside the deadband, derivative frozen there; filter the sensor, not the PID | 2026-04-05 to 2026-10-03 | [1490318738774556785](https://discord.com/channels/812969634638725140/1105556725714649128/1490318738774556785), [1490326104429301782](https://discord.com/channels/812969634638725140/1105556725714649128/1490326104429301782), [1490436555050057789](https://discord.com/channels/812969634638725140/1105556725714649128/1490436555050057789) |
| E29 | "Python always wins", and the rulings of 2026-06-20 on saturation, DHW status and OPV calibration | 2026-06-20 | [1517960092085911582](https://discord.com/channels/812969634638725140/1105556725714649128/1517960092085911582), [1517966230223327322](https://discord.com/channels/812969634638725140/1105556725714649128/1517966230223327322) |
| E30 | Device type and heating system are separate settings; heat pump cycles | 2026-04-05 to 2026-06-20 | [1490446199092674793](https://discord.com/channels/812969634638725140/1105556725714649128/1490446199092674793), [1490447036276015375](https://discord.com/channels/812969634638725140/1105556725714649128/1490447036276015375), [1490450926572666931](https://discord.com/channels/812969634638725140/1105556725714649128/1490450926572666931) |
| E31 | PWM duty ranges come from the cycles per hour; 180 s minimum flame time; the low-duty special case | 2026-04-05 and 2026-04-06 | [1490442317692407999](https://discord.com/channels/812969634638725140/1105556725714649128/1490442317692407999), [1490643874488782930](https://discord.com/channels/812969634638725140/1105556725714649128/1490643874488782930), [1490644818131423295](https://discord.com/channels/812969634638725140/1105556725714649128/1490644818131423295) |
| E32 | Idle states: valves closed means CS=10; humidity only with the comfort option; Summer Simmer stays | 2026-04-06 to 2026-04-09 | [1490819012899377444](https://discord.com/channels/812969634638725140/1105556725714649128/1490819012899377444), [1490948103753302158](https://discord.com/channels/812969634638725140/1105556725714649128/1490948103753302158), [1491751274687889528](https://discord.com/channels/812969634638725140/1105556725714649128/1491751274687889528) |
| E33 | DHW: one slider bounded by the boiler, SW= on change, a HW switch only for a storage tank | 2023-05-10 to 2026-05-02 | [1105707977882734602](https://discord.com/channels/812969634638725140/1105556725714649128/1105707977882734602), [1105713242493431818](https://discord.com/channels/812969634638725140/1105556725714649128/1105713242493431818), [1151241951270678529](https://discord.com/channels/812969634638725140/1105556725714649128/1151241951270678529) |
| E34 | SAT onboarding asks the questions of the HA SAT config flow | 2026-07-04 | [1522989168186232882](https://discord.com/channels/812969634638725140/1514720723980259460/1522989168186232882) |
| E35 | Multiple areas: port upstream PR #172; any discovered sensor can be an area sensor | 2026-05-08 and 2026-06-21 | [1502368354214871170](https://discord.com/channels/812969634638725140/1105556725714649128/1502368354214871170), [1518210120503263293](https://discord.com/channels/812969634638725140/1105556725714649128/1518210120503263293) |

Some "main links" point to our question in the thread rather than to his answer. The drafts name
the exact answer message.

**Open conflicts in his statements** (from the drafts; each needs one question to him, or a firmware
fix):

- **C1.** The continuous-to-PWM sustain time: 3 minutes (2026-04-07) or 60 s (2026-10-20). Finding
  #12.
- **C3.** Heat pump: "ONLY max modulation" (2026-06-20 19:17) or "A is the answer", which keeps the
  heat-pump timing (21:26). Finding #9.
- **C4.** Heat pump cycles: 1.5 / 2 per hour (his options) or 2 / 3 (thermo-nova). Finding #9.
- **C5.** Coefficient 2.0 was decided but not implemented. A firmware fix, not a question. Finding
  #28.
- **C6.** Very low duty: "0 sec on 900 sec off" against thermo-nova's 2 × period. Probably an
  example.
- **C8.** Stale room temperature: 5 min trip, 20 min, or 6 h reset. Finding #4.
- **C11.** The OPV calibration removal rested on our wrong premise that OPV "is not in the Python at
  all". His ruling probably stands.
- **C13.** "Remove it" (entry 19) answered an incomplete description of the setting. Finding #58.

---

## 5. Refuted and corrected findings

No area finding was fully refuted: the input list of refuted findings is empty. A second reader
opened both sides of every finding, and the corrections below came out of that pass. The report
uses the corrected versions.

- **Deadband widening leak (F5).** First described as a leak that adds several °C in 10 minutes. The
  elapsed time used for the widening is refreshed on almost every pass, so the real leak is about
  1e-6 °C per tick. Kept as a latent defect only (#38).
- **Stale room sensor, BLE (F12).** "Several times a week" was dropped. When BLE has a gap, the firmware
  first uses the thermostat Tr, if there is one (#4).
- **DHW and the ON phase (DHW-8).** "A draw can end an ON phase early" was dropped. A DHW ignition
  moves the timer later, not earlier (#59).
- **Classifier at boot (STARTUP-1).** The firmware can leave PWM again at tick 2 when the PID still
  returns 0. The priority was discussed (medium against high). We kept high, merged with CYC-01 and
  CYC-02, because the reference error also hits rising requests and mixed cycles (#8).
- **MM during DHW (MODS-03, DHW-1).** Rated medium, because neither code base shows that a boiler
  applies MsgID 14 to DHW (#10).
- **First PID call (F1).** "Automatic flush" was wrong: a flush is manual only. Lowered from high to
  medium (#13).
- **Heat pump cold setpoint (COLDCH-1).** Lowered from high to medium. It affects only wizard-set
  heat pumps on underfloor heating, and the user can fix the setting (#35).
- **Summer simmer lockout (COLDCH-3).** Lowered to low, because it is off by default (#48).
- **Comfort offset (COLDCH-4).** We added that the bot told him in April that the offset would be
  gated on `bSummerSimmer`, and that he did not object (#48).
- **Wizard coefficient (DEF-1).** "Almost every unit" was too strong. The migrate dialog can be
  dismissed (#7).
- **PID cadence (DEF-8).** "Not documented" was wrong. sat-comparison row 18 covers the 6x case
  (#14).
- **Hour cap (DHW-7).** The line "no more than 2 per hour ever" is Robert's, not Sergeant D's (#18).
- **TC= after a reset (MODS-07).** The owner of this decision is Robert (entry 2), not Sergeant D
  (#52).
- **Coefficient default (F04).** Lowered to low in that area audit and merged into #28 at medium.
  The citation is `const.py:182`, not `:181`.
- **PWM restore (PWM-15).** The firmware citation was wrong (`SATcontrol.ino:2400-2420` is the
  command handler). The start path is `:4783-4785` and `:3889`. The conclusion is unchanged (#43).
- **Doc drift comment (CYC-12, item 5).** "Guards cleared like thermo-nova's enable/disable paths"
  can be defended: the heating_control enable and disable paths do reset the guards. The comment is
  only inaccurate for the on_cycle_end path.
- **Heat pump enters PWM (CYC-03).** Lowered from high to medium in that area audit (heat pumps on
  OTGW are rare). It is now part of #9, which is high because of the duty mapper collapse, which
  affects heat pumps today.
- **DHW guard (S6).** Found to be worse than first stated. The tick right after a short draw can add
  the whole gap as sustain time and switch at once (#11).
- **Doc drift around the solar freeze (F16).** Found to be wider than reported. More firmware-sat-
  spec.md passages are stale (#36).
- **Draft texts (E21, E22, E24, E26, E27, E30, E32, E33, E35).** Several area reviewers wrote that they
  could not find these texts "in the repo". That is because they are drafts in the audit workspace,
  not repo files. Their quotes were checked by script and match his messages word for word.

---

## 6. What was not checked

- **No hardware.** Nothing was run on a boiler, a bench unit, a PIC or an OT-Direct board. No build
  was made and no test was run for this report.
- **Boiler behaviour** that neither code base shows:
  - whether boilers apply MsgID 14 to DHW (#10);
  - what Immergas does with TP=11:12 (#26);
  - why MI=500 is needed (#45);
  - what a boiler does with CH=1 and CS=10 alternating with CH=0 (#25).
- **OT-Direct details:**
  - what MsgID 1 carries in master mode after a SAT trip (#4);
  - whether the boot handshake fills the boiler cache in simulation (S14).
- **PIC details:**
  - whether the PIC keeps an SW= value after a reset (#31);
  - the PIC behaviour in #1 and #3 comes from one source (the assembly).
- **Timing not measured:** the ESP loop rate (#61), the BT error (#21), the PID growth rates (#14),
  and the startup timings (#13). The numbers are estimates from the code.
- **Home Assistant:** whether the HA room temperature is published retained, so that it arrives on
  subscribe (#4).
- **Upstream PR #172:** the content of SAT upstream PR #172 was not opened (#32, #37).
- **Classifier internals:** the internals of `cycles/classifier.py` were only partly re-opened by the
  second readers.
- **Second-reader pass on drafts:** the pass that CLAUDE.md requires before a claimed difference is
  published has not been done for the draft conflicts C5, the `>` against `>=` edge in E22, E21/C3
  ("no heat-pump MM rule in thermo-nova") and D1/C13.
- **Skipped messages:** 190 messages by Sergeant D that the harvest left out (mqtt.cfg, UI, BLE
  flashing, small talk). They were scanned once and judged to hold no SAT decisions. They were not
  audited further.
- **Thread context not read:**
  - the text of our 2026-10-03 08:29 question that he answered with "No, don’t change anything"
    (#40);
  - the TASK-3 acceptance criterion about SW= at init and on change (#31).
- **Line numbers:** firmware line numbers refer to the working tree, which includes the uncommitted
  TASK-1331 change in `SATcycles.ino` and `SATcontrol.ino`. They will move when that change is
  committed or reverted.
