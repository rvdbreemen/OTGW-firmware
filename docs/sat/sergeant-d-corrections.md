# SAT: corrections and decisions by Sergeant D

Sergeant D designed SAT. What he states here ranks above the thermo-nova code and above
`thermo-nova-spec.md` (see the "SAT ground truth" block in CLAUDE.md). Each entry quotes him,
links the Discord message, compares his statement with the pinned thermo-nova code
(`releases/thermo-nova` @ `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, paths relative to
`custom_components/sat`) and with the firmware, and names what is still open.

## 1. PWM duty cycle: the boiler temperature (BT) in the formula (2026-10-22)

**Source:** #alpha-testing, 2026-10-22 13:58 UTC,
https://discord.com/channels/812969634638725140/1514720723980259460/1562827601834418319
(and 12:27 UTC, message 1562804695909535837, for the formula itself).

**Quoted:**

> The core PWM calculation is the below formula ( Requested Setpoint - 27.2 ) / ( BT - 27.2 ).

> Exactly, use the limit 28.2 just to avoid divide by zero or negative values. The BT is smoothed
> (filtered) at the start of the ON cycle because it fluctuates during start of the cycle. So for
> the first 30 seconds of flame ON of each ON cycle we use the filtered value and then we use the
> raw BT value.

**Settled by this statement:**

- The duty cycle is `(requested setpoint - base) / (BT - base)` with base 27.2 °C for radiators
  (thermo-nova `pwm.py:238`, firmware `SATcontrol.ino:734` at that time). Both match.
- The floor `base + 1` (28.2 °C) on BT exists only to avoid division by zero or a negative value
  (`pwm.py:235`). It is not a placeholder for an unknown BT.
- BT is a real boiler (flow) temperature from the first ON cycle on. The firmware's fixed
  `base + 20` (47.2 °C) used while nothing is learned (`SATcontrol.ino:731-732`), and the learned
  value that starts at 0 after every boot and is never seeded (`SATcontrol.ino:686`), are wrong and
  go.

**Where his words and the thermo-nova code differ (open, his call):**

| Point | Sergeant D | thermo-nova code | firmware today |
|---|---|---|---|
| Filter window after flame-on | 30 s | 180 s (`HEATER_STARTUP_TIMEFRAME`, `const.py:18`, used at `pwm.py:148`) | 30 s (`SATcontrol.ino:750`) |
| BT after the window | raw flow temperature | the filtered value stays as it is until the next ON phase or ignition resets it to the raw flow (`pwm.py:166-167`, `pwm.py:191`); no switch to raw inside a phase | the filtered value stays (never reset) |
| Seed at flame-on | (implied: the raw flow) | raw flow at first flame (`pwm.py:130-131`), at ON start with flame (`166-167`), at ignition sync (`191`) | none |
| Filter | "smoothed" | EMA 0.3 new / 0.7 old (`pwm.py:149`) | EMA 0.3 / 0.7 (`SATcontrol.ino:752`) |

**Resolved 2026-10-22 16:12-16:14 UTC** (messages 1562861280308625409 and 1562861690322817025):

> Correct the startup time frame is 180 seconds

> Ok, keep the `thermo-nova` solution

So: filter window 180 s (`HEATER_STARTUP_TIMEFRAME`), EMA 0.3 new / 0.7 old during that window
after flame-on, BT seeded from the live flow temperature at the first flame, at an ON-phase start
with the flame already on, and at the ignition sync; after the window the filtered value is held
until the next ON phase or ignition resets it to the live flow (`pwm.py:130-131`, `148-149`,
`166-167`, `191`); floor `base + 1`. The firmware's 30 s window, its unseeded start value and its
`base + 20` placeholder all change to this. Implementation: TASK-1273, first PWM item.

## 2. Follow thermostat and push setpoint (2026-10-22)

**Source:** 14:01 UTC, message 1562828251364069377.

> I don't own an OpenTherm thermostat I can't help on this. Robert is the appropriate person to ask.

**Meaning:** the interaction of "follow thermostat" with "push setpoint" (TASK-1298, firmware-only
feature) is the maintainer's decision, not a thermo-nova question.

**Maintainer decision (Robert, 2026-10-22, in session):** with push on, a confirmed thermostat
setpoint change is adopted when SAT has pushed at least once since boot and the change is not an
echo of SAT's own push (within 0.25 °C). The earlier precondition that the thermostat's previous
value matched the last push is dropped, because thermostats such as the Honeywell Round never
mirror an accepted override in their own setpoint (MsgID 16).

## 3. Base temperature (2026-10-22)

**Source:** 14:15 UTC, message 1562831881341505670.

> The base temperature is everywhere in the code because it is the basis value of SAT.

**Meaning:** the base offset (27.2 °C radiators, 20.0 °C underfloor; `heating_curve.py:39`,
`SATcontrol.ino:49-50`) is a design constant that every SAT formula refers to, not a tunable.

## 4. Validation of the three SAT documents (2026-10-22)

**Source:** 18:01 UTC, message 1562888583004688629.

> I checked the files and everything looks good so far. Good job.

**Meaning:** `thermo-nova-spec.md`, `firmware-sat-spec.md` and `sat-comparison.md` passed his
first reading without a correction. Later corrections land here as numbered entries; the
documents are not rewritten silently.

## 5. PWM OFF: CH=0 together with CS=10 (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 10:21 UTC,
https://discord.com/channels/812969634638725140/1514720723980259460/1563135315991072789

**Quoted:**

> @BlueFlame Bot the CS is following the BT correctly now! Great job 👏
>
> We have another issue at the moment.
> When the PMW goes to OFF, correctly you send the CS=10 command, but you should also send the
> command CH=0.

**Settled by this statement:**

- On the build he ran, the control setpoint (CS) follows the boiler temperature (BT) as he
  intends (he does not name the build).
- In the PWM OFF phase SAT sends CS=10 and clears the CH enable bit (CH=0).

**Compared with thermo-nova and the firmware:**

- thermo-nova: in the PWM OFF phase the override sets `_control_setpoint = MINIMUM_SETPOINT`
  (10.0; `heating_control.py:388-389`, `const.py:35`). After every control step it sends the
  setpoint and then the heater state, ON only when `_control_setpoint > COLD_SETPOINT`
  (`heating_control.py:236-238`, `COLD_SETPOINT = 22` at `const.py:34`); the serial (OTGW)
  coordinator turns that into the CH enable bit (`coordinator/serial/__init__.py:212`). So
  thermo-nova sends CS=10 with CH=0 on PWM OFF, as he says.
- Firmware, **correction of our own claim:** at 10:32 UTC (message 1563138080003588119) we wrote
  that the firmware sends CS=10 on PWM OFF without CH=0. That was wrong. At HEAD the PWM OFF tick
  already queued CH=0 right after CS=10: `satApplyPWM` returned `SAT_MIN_SETPOINT` and the main
  control block set CH from that same setpoint (TASK-1316 notes, 13:25). We corrected this at
  13:18 UTC (message 1563179730084171818). The real difference with thermo-nova was the CH
  threshold: the firmware cleared CH only at or below 10, thermo-nova at or below 22 (entry 7).

**Task:** TASK-1316 (2.0.0-alpha.471, commit 812cca580).

**Still open:**

- What he saw is not explained by the code. We asked which board it was (PIC gateway or the board
  without a PIC) and for a capture (13:18 UTC); he did not answer that part.
- TASK-1316 AC3, his confirmation on his own setup. His capture of 18:48 UTC (message
  1563262919527895071, alpha.474) is not that confirmation: that it shows the CH rule working
  (CS=10 with CH off while the PID asks for nothing) is our reading of it (reply 1563266176807866471).

## 6. Return temperature (Tret): only needed in the PWM start-up phase (2026-10-23)

**Source:** #alpha-testing, 2026-10-23, in a thread with gruijter. The 10:28 UTC message came
first; gruijter then asked whether Tret should not get more attention (10:30 UTC, message
1563137561273176185), and Sergeant D answered that at 10:32, 10:35 and 10:38 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1563136971893772291
https://discord.com/channels/812969634638725140/1514720723980259460/1563138126787117079
https://discord.com/channels/812969634638725140/1514720723980259460/1563138804225806418
https://discord.com/channels/812969634638725140/1514720723980259460/1563139533405552691

**Quoted:**

> @gruijter the Tret is only needed from SAT during the PWM start up phase, SAT doesn’t monitor it
> all the time

> Yes Tret is very important. But it is the derivative of the control loop operation. When we
> suppress the flow water temperature we also keep the return water temperature as less hot as
> possible.

> Moreover we use the Tret value at the start up because it is pretty stable, while the Tflow
> changes dramatically. Tret gives the needed stability for boilers like yours

> Then, by keeping the Tflow as low as possible we force the boiler to work at its lowest
> modulation for longer intervals ( low-load control ). So this is the idea for keeping the Tret
> low. Note that Intergas by default does not even report the Tret ( lame ).

In the same thread, at 10:30 UTC (message 1563137459477545021), on how to compare SAT with a
Honeywell Round over a day:

> Actually longer runs and lower Tret values are better. Moreover which one has better room
> temperature regulation

**Settled by this statement:**

- SAT uses Tret only in the PWM start-up phase; it does not watch it all the time.
- His reasons: at start-up Tret is stable while the flow temperature swings; suppressing the flow
  temperature also keeps the return cool; a low flow temperature keeps the boiler at its lowest
  modulation for longer (low-load control).
- Intergas boilers do not report Tret by default.

(Context, not from him: gruijter's boiler is an Intergas whose Tret comes from a sensor wired to
the OTGW board, per the TASK-1314 description.)

**Compared with thermo-nova and the firmware:**

- thermo-nova, control: the return temperature is read only in the PWM override while the flame
  is off, as the flame-off hold `clamp(return_temperature + flame_off_offset, COLD_SETPOINT,
  maximum_setpoint)` (`heating_control.py:393-398`). A search for `return_temperature` under
  `custom_components/sat` finds no other control use (the other hits are the coordinators, the
  device types and the cycle tracker). This matches his statement.
- Scope difference: thermo-nova also records the return temperature in its cycle statistics
  (flow-return delta `cycles/tracker.py:262-265`, percentiles `cycles/tracker.py:285-303`). Whether
  those statistics feed a control decision was not traced in this run.
- Firmware, control: `satPwmFlameOffHold` (`SATcontrol.ino:704-709`, return + `fFlameOffOffset`)
  is the only control use of `satGetReturnTemp()`; the firmware also records the flow-return delta
  in `satCycleSample` (`SATcycles.ino:859-861`), like thermo-nova's tracker.
- His reasons (stability of Tret, low-load control) are not written in the code; the code shows
  only the mechanism: the hold on the return before the flame, then flow minus the suppression
  offset (`heating_control.py:441`).

**Task:** context for TASK-1314 (Tret from the wired sensor every minute on PIC boards). The faster
Tret mostly serves the graph and MQTT; for SAT it matters only for the flame-off hold. No SAT
behaviour change follows from this entry.

**Still open:** nothing for SAT. TASK-1314 AC2 (the live loop on a board with the sensor wired)
waits on the reporter, not on Sergeant D.

## 7. CH threshold: the per-system cold setpoint instead of 22, and the cold-start hold (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 12:49 UTC, answer to our question of 10:32 UTC (message
1563138080003588119: should CH=0 follow thermo-nova exactly, at or below 22, or only on the PWM
OFF moment?):
https://discord.com/channels/812969634638725140/1514720723980259460/1563172561162862714
and 13:31 UTC, answer to our two questions of 13:18 UTC (message 1563179730084171818):
https://discord.com/channels/812969634638725140/1514720723980259460/1563182969345282071

**Quoted:**

> @BlueFlame Bot below 28.2 not 22. Other than that follow the thermo-nova exactly

His message of 13:31 repeats our two questions as quotes; only his own answers are given here.
To "Do you mean CH=0 below that per-system cold setpoint (28.2 radiators, 21 underfloor)?":

> You are correct

To our risk that the flame-off hold (return + offset) can be below 28.2 on a cold start, so CH=0
would block ignition:

> We can limit the cold start value to 28.2 and 21 for underfloor

**Settled by this statement:**

- CH is cleared below the per-system cold setpoint: 28.2 °C for radiators, 21 °C for underfloor
  (firmware `SAT_COLD_SETPOINT_RAD` / `SAT_COLD_SETPOINT_FLOOR`, `SATcontrol.ino:57-58`, picked by
  `satGetColdSetpoint`), not at thermo-nova's `COLD_SETPOINT = 22` (`const.py:34`).
- Everything else about the CH bit follows thermo-nova.
- The flame-off hold of the PWM start-up phase never goes below the cold setpoint. thermo-nova
  clamps it at `COLD_SETPOINT` 22 (`heating_control.py:398`); the firmware floors it at
  `satGetColdSetpoint()` in `satPwmFlameOffHold` (`SATcontrol.ino:706-707`). That floor already
  existed (TASK-891.2) before he said this; we told him at 13:47 UTC (message 1563187099484557367).

**Where his words and the code differ (open, his call):**

- Strictly below or at or below. He wrote "below 28.2" (and "lower than the thresholds" at 16:47,
  entry 11). thermo-nova uses at or below: the heater state is ON only when
  `requested_setpoint > COLD_SETPOINT` (`heating_control.py:72`) and the PWM override returns when
  `requested_setpoint <= COLD_SETPOINT` (`heating_control.py:385`). The firmware follows thermo-nova
  since TASK-1323: `satColdCutoff` is `requestedSetpoint <= coldSetpoint` (`SATchEnable.h`). So at
  exactly 28.2 / 21.0 his wording gives CH=1 and the firmware gives CH=0. Following thermo-nova's
  comparison here is our choice; he has not confirmed it.
- "Follow the thermo-nova exactly" includes sending the CH bit after every control step
  (`heating_control.py:238`). The firmware sends it when it changes plus every 5 minutes
  (`satEnqueueIfChangedCH`; the 5-minute refresh as stated in our 13:18 message, not re-opened in
  this run). We named this at 13:18 UTC; he did not answer, so it is kept without an
  explicit OK (the related resend gap is TASK-1324).

**Task:** TASK-1316 (2.0.0-alpha.471, commit 812cca580).

## 8. During PWM the CS only keeps the flame on; the duty cycle lives in pwm.py (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 14:51, 14:53 and 14:54 UTC, replies to our questions of
13:47 UTC (message 1563187099484557367: the solar offset during ignition, and the suppression
setpoint dropping below the cold setpoint mid-burn):
https://discord.com/channels/812969634638725140/1514720723980259460/1563203205633020027
https://discord.com/channels/812969634638725140/1514720723980259460/1563203729652457585
https://discord.com/channels/812969634638725140/1514720723980259460/1563204066262130719

**Quoted:**

> @BlueFlame Bot The CS we send to the boiler is decoupled to the duty cycle calculation.The CS
> that we send to the boiler during PWM just keeps the flame on at the minimum modulation level.
> The hard job of the cycle duration is in the pwm.py file

> @BlueFlame Bot  If the ( requested setpoint - base temperature ) / ( BT - base temperature ) is
> suitable it will start heating. The pwm.py file describes the mechanism of the duty cycles.

> @BlueFlame Bot we analyze the percentage of the duty cycles that then are translated to seconds
> on and off

**Settled by this statement:**

- The CS sent during PWM is separate from the duty-cycle calculation: it only keeps the flame on
  at minimum modulation.
- How long the burner runs comes from the duty cycle, `(requested setpoint - base) / (BT - base)`
  (entry 1), as a percentage that is turned into on and off seconds.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `_calculate_duty_cycle` (`pwm.py:229`) computes the percentage at `pwm.py:238`
  from the requested setpoint, the base offset and the effective boiler temperature (floor
  base + 1, `pwm.py:235`) and returns on and off seconds (the low-duty special cases at
  `pwm.py:247-260`). The CS during PWM ON comes from a separate override in `heating_control.py`:
  the hold on the return before the flame (`:398`), then flow minus the suppression offset
  (`:441`). Two separate paths, as he says.
- Firmware: the same split, `satPwmFlameOffHold` and `satPwmSuppressionSetpoint` for the CS; the
  duty-cycle formula is covered by entry 1.
- This was not a direct answer to our two questions. From it we drew a four-point CH rule and
  asked for a yes or no (entry 9).

**Task:** TASK-1316.

## 9. CH rule during PWM: read as confirmed (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 15:08 UTC, reply to our four-point proposal of 15:00 UTC
(message 1563205445781422121):
https://discord.com/channels/812969634638725140/1514720723980259460/1563207530275152037

**Quoted:**

> @BlueFlame Bot great understanding! As you understand the continuous and pwm buttons should never
> be present in the SAT dashboard. Because the software decides when every mode will be in charge.
> Slowly you understand the philosophy behind SAT. A powerful software that auto detects the mode
> and acts accordingly

Our proposal (our words, not his): 1. PWM ON phase: CH=1 whatever CS we send. 2. PWM OFF phase:
CS=10 and CH=0. 3. Continuous mode: CH=0 when the setpoint we send is below the cold setpoint
(28.2 radiators / 21 underfloor), otherwise CH=1. 4. Solar offset: decide CH on the setpoint before
the solar offset is subtracted.

**Interpretation (ours):** "great understanding!" is not an explicit yes. We read it as yes to all
four points and implemented TASK-1316 on that reading (2.0.0-alpha.471, commit 812cca580). The
buttons part of the message is entry 10.

**Settled by this statement (on our reading):**

- PWM OFF phase: CS=10 and CH=0.
- PWM ON phase: CH stays 1 whatever CS keeps the flame on.
- Points 3 and 4 as proposed, later replaced by entry 11.

**Compared with thermo-nova and the firmware:**

- Point 1 departs from thermo-nova. thermo-nova sets CH from `_control_setpoint > COLD_SETPOINT`
  after every step (`heating_control.py:238`), so its low suppression CS (flow minus the offset,
  `heating_control.py:441`) at or below 22 would clear CH during a burn. Point 1 keeps CH on in the
  PWM ON phase. This deviation rests only on our reading of this message, while at 12:49 (entry 7)
  he asked to follow thermo-nova exactly apart from the threshold.
- Point 2 is thermo-nova's behaviour (entry 5).
- Points 3 and 4 were replaced the same day by entry 11 (TASK-1323): the solar offset now lowers
  the requested setpoint before the PWM or continuous step, and CH is decided on that requested
  setpoint, not on the sent setpoint before solar. The firmware today: CH=0 when the requested
  setpoint is at or below the cold setpoint, or when the PWM OFF phase drives the CS; otherwise
  CH=1 (`satChEnableDecide` in `SATchEnable.h`, called from the main control block,
  `SATcontrol.ino:5044`).

**Task:** TASK-1316.

**Still open:** his confirmation of point 1 (CH stays on in the PWM ON phase, unlike thermo-nova);
TASK-1316 AC3, his confirmation on his own setup.

## 10. SAT dashboard: no Continuous/PWM buttons, the active mode stays visible (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 15:08 UTC (the message of entry 9), and 16:16, 16:16 and
16:17 UTC, replies to our questions of 16:13 UTC (message 1563223868334805013: the two TASK-1323
edge cases, whether the active mode should stay visible read-only, and whether the force-PWM
setting itself should stay):
https://discord.com/channels/812969634638725140/1514720723980259460/1563207530275152037
https://discord.com/channels/812969634638725140/1514720723980259460/1563224512634298432
https://discord.com/channels/812969634638725140/1514720723980259460/1563224636551077960
https://discord.com/channels/812969634638725140/1514720723980259460/1563224794067894455

**Quoted:**

> As you understand the continuous and pwm buttons should never be present in the SAT dashboard.
> Because the software decides when every mode will be in charge. Slowly you understand the
> philosophy behind SAT. A powerful software that auto detects the mode and acts accordingly

> Follow the thermo-nova about how it is done

> @BlueFlame Bot if you have questions then ask me again

> @BlueFlame Bot Yes the active mode can stay visible

**Settled by this statement:**

- The SAT dashboard has no manual Continuous/PWM buttons; SAT picks the mode itself.
- The active mode may stay visible, read-only.
- Open questions go back to him rather than being settled by us.

**Interpretation (ours):** "Follow the thermo-nova about how it is done" replied to the whole
16:13 message. We read it as the answer to the force-PWM question (and, for TASK-1323, to the
solar question, entry 11). In thermo-nova, forcing PWM is a configuration option, not a dashboard
control: `CONF_FORCE_PULSE_WIDTH_MODULATION` (`const.py:80`, default False at `const.py:149`, in the
options schema at `config_flow.py:728`). So the force-PWM setting stays under SAT tuning.

**Firmware:** TASK-1322 (2.0.0-alpha.472, commit f5c09a8f3): the Control view shows the mode as
text, the buttons are gone, the force-PWM setting stays. Keeping `POST /api/v2/sat/mode` for API
users is our decision; we did not ask him.

**Task:** TASK-1322 (buttons, active mode, force-PWM setting) and TASK-1323 (the 16:16 reply, see
entry 11).

**Still open:** nothing from him.

## 11. Solar offset on the requested setpoint, 28.2/21 everywhere, integral freeze (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 16:16 UTC ("Follow the thermo-nova about how it is done",
entry 10) and 16:47 UTC, reply to our proposal and three questions of 16:36 UTC (message
1563229693308833825):
https://discord.com/channels/812969634638725140/1514720723980259460/1563232315969249376

**Quoted:**

> 1\. The switch thresholds are 28.2/ 21. If the requested setpoint is lower than the thresholds
> then we send CH=0 CS=10
>
> 2\. Yes we use 28.2/21
>
> 3\.yes, follow thermo-nova

(In his message the three answers are on consecutive lines.)

Our proposal and questions (our words, not his): A. Subtract the solar offset from the requested
setpoint before the PWM/continuous step, as thermo-nova does, instead of from the setpoint we send;
also cut off at "equal to" the cold setpoint. 1. With overshoot protection off in PWM, thermo-nova
resends the last setpoint and CH stays on at 24; with the 28.2 rule CH would be off at 24: keep
that, or keep CH on? 2. thermo-nova uses 22 in more places (the PWM gate, the hold floor, the PWM
enable/disable guards): should 28.2/21 apply everywhere? 3. Freeze the PID integral during solar
only when the option is on, and skip the integral reset, as thermo-nova does?

**Settled by this statement:**

- Answer 1: the thresholds are 28.2 / 21; a requested setpoint below them gives CH=0 and CS=10.
  CH thus follows the requested setpoint, not the held CS: with protection off, a held CS of 24 and
  a requested setpoint above 28.2 give CH=1. That consequence for question 1 is our reading of
  answer 1 (TASK-1323 notes).
- Answer 2: 28.2 / 21 wherever thermo-nova uses `COLD_SETPOINT` 22, including the heater state
  (`heating_control.py:72`), the PWM enable/disable guards (`:280`, `:317`), the PWM gate
  (`:385`), the hold floor (`:398`) and the CH bit after each step (`:238`).
- This entry replaces points 3 and 4 of entry 9: CH is now decided on the requested setpoint after
  the solar offset, not on the sent setpoint before it.
- Answer 3: follow thermo-nova. The integral is frozen only when solar gain is active and the
  freeze option is on (`climate.py:501`), and while frozen the whole integral update is skipped,
  reset included (`pid.py:167-168`; the reset is at `pid.py:187-188`).
- Proposal A, **interpretation (ours):** he did not answer it separately. We took it as accepted
  because he did not correct it and had said at 16:16 to follow thermo-nova. thermo-nova subtracts
  the solar offset from the requested setpoint (`climate.py:391-392`) before the clamp
  (`climate.py:394`), so before the PWM or continuous step; the flame-off hold
  (`heating_control.py:398`) and the suppression setpoint (`heating_control.py:441`) have no solar
  term.

**Firmware:** TASK-1323 (2.0.0-alpha.474, commit 9ec1b87e2): the solar offset is subtracted from
the PID output before the clamp and stored as the requested setpoint (`SATcontrol.ino:4968`); the
cold cutoff `satColdCutoff` uses at or below (`SATcontrol.ino:5030`); CH comes from
`satChEnableDecide`; the integral freeze is `if (state.sat.bSolarGainActive &&
settings.sat.bSolarFreezeIntegral) return;` (`SATpid.ino:157`), default on (`SATtypes.h:527`).

**Task:** TASK-1323.

**Still open:**

- Strictly below versus at or below (entry 7): "lower than the thresholds" against the firmware's
  `<=`.
- Answer 2 "everywhere": our 16:36 message said the firmware already uses 28.2/21 at the PWM
  enable/disable guards; that was not re-checked in this run.
- Not verified on a real boiler (the bench unit has no room temperature, so SAT never reaches the
  CH step); his confirmation on his own setup is outstanding.

## 12. Cold cutoff: switch PWM off and reset the setpoint, as thermo-nova does (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 18:13 UTC, reply to our question of 18:10 UTC (message
1563253204521721987: thermo-nova switches PWM off and resets the setpoint to 10 on the cutoff, we
stay in PWM and keep the last setpoint; shall we do it the thermo-nova way?):
https://discord.com/channels/812969634638725140/1514720723980259460/1563254104346730600

**Quoted:**

> @BlueFlame Bot yes keep the thermo-nova solution

**Interpretation (ours):** an OK to mirror thermo-nova on the cutoff.

**Settled by this statement:**

- When the requested setpoint drops to the cold setpoint or below, SAT switches PWM off and resets
  the control setpoint to the minimum, as thermo-nova does.
- thermo-nova: the heater state is OFF when the HVAC mode is not heat, the valves are closed, or
  the requested setpoint is at or below `COLD_SETPOINT` (`heating_control.py:66-72`). `update()`
  then takes the else branch: `self._pwm.disable()` (`heating_control.py:230`), which resets PWM
  and disables it (`pwm.py:106-109`), resets the PWM disable guards (`:231`), sets
  `_control_setpoint = MINIMUM_SETPOINT` (`:233`) and the maximum relative modulation (`:234`).
- Firmware today (TASK-1327 description): SAT stays in PWM mode, the PWM phase timers keep running
  on cold ticks and the held CS keeps its pre-cutoff value, so after a long cold period (now more
  likely, because solar lowers the requested setpoint) SAT can resume with a stale CS such as 45.

**Task:** TASK-1327.

**Still open:** TASK-1327 is not implemented yet (To Do). The cutoff uses 28.2 / 21 instead of 22
(entry 11, answer 2).

## 13. Force PWM: remove it completely (2026-10-23)

**Source:** #alpha-testing, 2026-10-23 19:44 and 19:45 UTC, reply to our question of 19:43 UTC
(message 1563276791723073720: with Force PWM on, should the cold cutoff keep the mode on PWM, or
switch to Continuous like thermo-nova?):
https://discord.com/channels/812969634638725140/1514720723980259460/1563277074553376889
https://discord.com/channels/812969634638725140/1514720723980259460/1563277188630061057

**Quoted:**

> @BlueFlame Bot  Honestly, I don’t even know why we still have it in the code

> It is better to be removed completely

**Settled by this statement:**

- The Force PWM option goes, as a setting and as code. SAT picks PWM or Continuous itself
  (entries 8 and 10).
- thermo-nova only stores the option and never reads it: it is defined (`const.py:80`, default
  `False` at `const.py:149`), offered in the options flow (`config_flow.py:728`) and copied into
  the entry data (`entry_data.py:58`, `entry_data.py:223`); nothing in the package reads
  `force_pulse_width_modulation` afterwards. Removing it matches thermo-nova's behaviour.
- In the firmware it is live today: `settings.sat.bForcePWM` (`SATtypes.h`) forces
  `SAT_MODE_PWM` on every control tick (`satControlLoopRun`), keeps PWM mode on the cold cutoff
  (`satPwmDisableOnHeaterOff`, TASK-1327), and is exposed as the setting `satforcepwm` (REST
  GET/POST, settings file key `SATforcepwm`, the v2 SAT settings 'PID / PWM' group), the MQTT
  command `force_pwm` and the Home Assistant entity `sat/force_pwm_enable`.

**Changes an earlier entry:** entry 10 kept Force PWM as a setting under SAT tuning (our reading
of "follow the thermo-nova" at 16:16 UTC). This entry replaces that part of entry 10.

**Task:** TASK-1332 (removal). TASK-1327's Force PWM branch goes with it: on the cold cutoff SAT
always leaves PWM mode, as thermo-nova does.

**Open:** none from him. On our side: a settings file that still holds `SATforcepwm` must load
without error (removed keys are ignored, ADR-192), and the retained Home Assistant discovery
config of the entity must be removed so it does not linger.

## 14. Push setpoint: push on a target change, re-push only when the override is gone (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 03:18 UTC, reply to our question of 01:56 UTC
(message 1563370497188560928: thermo-nova pushes TC= once on a user or preset target change, the
firmware re-pushes every 5 minutes and the override flaps on a Honeywell Round; Robert's choice
for his OK):
https://discord.com/channels/812969634638725140/1514720723980259460/1563391276022366249

**Quoted:**

> @BlueFlame Bot yes it is ok

**Settled by this statement:**

- SAT pushes the room setpoint to the thermostat (TC=) when the target changes, as thermo-nova
  does: `async_set_target_temperature(temperature, cascade=True)` calls
  `async_set_control_thermostat_setpoint()` when `push_setpoint_to_thermostat` is set
  (`climate.py:641-657`), which sends `TC={value}` (`coordinator/mqtt/opentherm.py:206-209`). A
  change that came from the thermostat itself is not pushed back (`climate.py:765`,
  `cascade=False`).
- The firmware's unconditional 5-minute re-push (`satEnqueueIfChangedTC`, `SAT_CMD_REFRESH_MS`
  in `SATcontrol.ino`) goes. thermo-nova has no periodic re-push (not found in the code).
- Firmware addition, the maintainer's choice (Robert, 2026-10-24) that Sergeant D approved here:
  re-push only when the gateway reports the override gone (MsgID 9 TrOverride reads 0) while
  SAT's push should still be active. This covers a gateway (PIC) reset, which clears the override.

**Changes an earlier entry:** entry 2 called push-setpoint a firmware-only feature. The push
itself exists in thermo-nova (above); only the periodic re-push was firmware-only. Entry 2's
follow-thermostat adopt rule is unchanged.

**Task:** TASK-1272.

**Open:** none from him. On our side: OT-Direct boards never cancel a TC= override by themselves
(OTDirect.ino), so the MsgID 9 re-push condition matters mainly on PIC boards.

## 15. PWM re-enable at the end of a cycle: port thermo-nova's on_cycle_end, margin 2 (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 08:10 UTC, answers 1a-1d to our consolidated question list
(message 1563458366712578159, 07:45 UTC; the details were in message 1563337168548536433 of
2026-10-23 23:43 UTC):
https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840

**Quoted:**

> 1a. Follow the thermo-nova solution
> 1b. Yes margin should be 2
> 1c. Can you expand on this?
> 1d. Yes

**Interpretation (ours):** "Follow the thermo-nova solution" to the question "only OVERSHOOT -> PWM,
or also UNDERHEAT -> continuous?" reads as: port both branches of `pwm.py:on_cycle_end`, as in
thermo-nova. A continuous cycle classified OVERSHOOT enables PWM (`pwm.py:219-223`), a PWM cycle
classified UNDERHEAT disables it (`pwm.py:225-227`).

**Settled by this statement:**

- Port both on_cycle_end branches (1a, as interpreted above).
- The overshoot margin for this trigger is 2.0, not thermo-nova's 3.0
  (`cycles/const.py:10`, `OVERSHOOT_MARGIN_CELSIUS`): a deliberate deviation he chose (1b). It
  matches the 2.0 he approved for the TASK-1268 plan on 2026-10-20.
- Align our cycle classifier with `cycles/classifier.py` first, in the same change (1d): hot water
  in the tail makes a cycle UNCERTAIN, OVERSHOOT only for continuous cycles and tested before the
  short-cycle test.

**Open:** 1c (should the firmware's "PWM auto switch" setting also block this path?). He asked us to
expand; we explained and asked again. Until he answers, the port is not implemented.

**Task:** TASK-1331. Prerequisite done: the tail percentile sort fix (TASK-1338).

## 16. Push setpoint: exactly thermo-nova, no re-push on MsgID 9 (2026-10-24)

**Source:** same message, answer 2:
https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840

**Quoted:**

> 2. Yes

**Settled by this statement:** to our question "push on a target change only
(`climate.py:641-657`), never push back what the thermostat set (`climate.py:765`), without the
MsgID 9 re-push. Is that OK?": yes.

**Changes an earlier entry:** entry 14 approved, as a firmware addition, a re-push when MsgID 9
reads 0 while a push is active. That bullet is dropped: on PIC boards a gateway reset and a
thermostat that refuses the override give the same signal (MsgID 9 reads 0 after the PIC clears the
override, `gateway.asm` ~3022-3036), so the re-push would flap more (review of TASK-1272). The rest
of entry 14 stands: push on a target change, no 5-minute re-push.

**Task:** TASK-1272. **Open:** none.

## 17. CH during PWM confirmed on his installation (2026-10-24)

**Source:** same message, answer 3:
https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840

**Quoted:**

> 3. Yes this is how it works. When we are in PWM OFF m, CS=10 CH=0

**Settled by this statement:** on his own installation CH stays 1 during the PWM ON phase and the
PWM OFF phase sends CS=10 with CH=0, as implemented in TASK-1316. This confirms entry 9 (CH rule
during PWM) and entry 5 (PWM OFF: CH=0 together with CS=10) on real hardware.

**Task:** TASK-1316 (acceptance criterion 3). **Open:** none.

## 18. The end-of-cycle PWM switch ignores the PWM auto switch setting (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 08:26 UTC, reply to our explanation of question 1c
(message 1563467698837389343, 08:22 UTC: should the firmware's "PWM auto switch" setting also block
the new end-of-cycle switch (A), or should it run anyway like thermo-nova (B)?):
https://discord.com/channels/812969634638725140/1514720723980259460/1563468667210170383

**Quoted:**

> @BlueFlame Bot It should follow the thermo-nova solution

**Interpretation (ours):** option B. thermo-nova has no auto-switch setting (a grep of
`custom_components/sat` for auto_switch/autoswitch finds nothing), and `pwm.py:on_cycle_end`
(`pwm.py:219-227`) always acts. So the ported end-of-cycle switch runs whatever the firmware's
`settings.sat.bPwmAutoSwitch` says.

**Settled by this statement:** the end-of-cycle switch of entry 15 is not gated on
`bPwmAutoSwitch`. Completes entry 15 (question 1c).

**Task:** TASK-1331.

**Open:** whether the "PWM auto switch" setting should remain at all (it still gates the existing
60 s sustained-overshoot switch, `SATcycles.ino` `satCycleCheckAutoSwitch`; thermo-nova has no such
setting). Asked as a follow-up; it does not block TASK-1331.

## 19. Remove the "PWM auto switch" setting (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 08:47 UTC, reply to our follow-up of 08:45 UTC (message
1563473408203694150: should our "PWM auto switch" setting stay at all? It only blocks the 60-second
overshoot switch, and thermo-nova has no such setting):
https://discord.com/channels/812969634638725140/1514720723980259460/1563474015409012747

**Quoted:**

> @BlueFlame Bot remove it

**Settled by this statement:** the firmware setting "PWM auto switch" (`settings.sat.bPwmAutoSwitch`,
default true, `SATtypes.h`) is removed, as Force PWM was (entry 13). SAT always picks the mode
itself, as thermo-nova: the sustained-overshoot switch (`heating_control.py:280-304`) and the
end-of-cycle switch (`pwm.py:219-227`, entries 15 and 18) are never blocked by a user setting.
thermo-nova has no such setting (a grep of `custom_components/sat` for auto_switch/autoswitch finds
nothing).

**Task:** TASK-1341 (removal: setting, REST field, MQTT command/HA entity if any, UI field, docs;
old settings files keep loading).

**Open:** none. (Question Q1 in `docs/sat/open-questions.md`; re-asked at 09:18 UTC before we saw
this answer.)

<!-- Entries 20-35 were recorded on 2026-10-24 from the Discord harvest of the SAT port audit (docs/sat/sat-port-audit-2026-10-24.md, drafts D2-D17). Each quote was checked by script against the harvested Discord text; they cover statements from 2023-05 to 2026-10 that earlier sessions had not recorded. -->

## 20. Overshoot protection (start-up + suppression): on by default for all heating systems (2026-10-20)

**Source:** #alpha-testing, 2026-10-20, one thread on TASK-1268:

- 18:26 UTC, answer a) to our question of 18:18 (message 1562168252719824938: should the PWM CS
  override be gated by `overshoot_protection`, default off, as in thermo-nova?):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562170196897038447
- 19:36 UTC, answer b) to our plan of 19:27 (message 1562185620615462934: boilers that ignore MM):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562187666328649729
- 19:45, 20:01, 20:04 and 20:13 UTC (brand list, heat pumps, all boilers, all heating systems):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562190051071361119
  https://discord.com/channels/812969634638725140/1514720723980259460/1562193995403235479
  https://discord.com/channels/812969634638725140/1514720723980259460/1562194736612253752
  https://discord.com/channels/812969634638725140/1514720723980259460/1562197154586435707
- 20:09 UTC, what to check in graphs:
  https://discord.com/channels/812969634638725140/1514720723980259460/1562196197261705306
- 20:27 UTC, OK on our final plan of 20:17 (message 1562198065505501349):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562200634340679742
- Earlier: #dev-sat-mqtt, 2026-04-07 05:35 and 05:52 UTC (suppression only in PWM ON):
  https://discord.com/channels/812969634638725140/1105556725714649128/1490948103753302158
  https://discord.com/channels/812969634638725140/1105556725714649128/1490952343502590085

**Quoted:**

> a) Should be the same as the original code from the thermo-nova branch. Keep the behavior identical.

> b) Intergas and Ideal boilers ignore MM commands. So these two should use always the suppression
> mechanism.

> Yes nefit should be also on that list.

> So in this list add all heat pumps also

> I think that the thermo nova uses the always-suppress mode to all boilers not only on the list

> Keep in mind that maybe TRVs in the system that can change the boiler flow water temperature
> without saturating sensing it. So the start up + suppression should be the option for all the
> heating systems

> Moreover, apart from the start up phase, the relative modulation should be as close to 0 as
> possible. By this way we comply the OpenTherm Specs for the workaround method.

> This looks ok

From April:

> The modulation suppression logic is used when SAT is  on PWM mode. That’s why tries to keep the
> modulation low, by suppressing the CS lower than the boiler flow water temperature.

> Modulation suppression should be ONLY used when we are in PWM ON time

**Interpretation (ours):** "This looks ok" approves item 1 of our final plan: the start-up and
suppression sequence becomes a setting `overshoot_protection` that is on by default for all
heating systems and can be switched off. "Should be the option for all the heating systems" (20:13)
replaced his 18:26 "keep the behavior identical", which in the code means off by default.

**Settled by this statement:**

- The PWM setpoint override (PWM OFF: CS=10; PWM ON with the flame off: return + offset hold;
  after the hold: flow minus offset) runs only in PWM, and in the ON phase only after ignition for
  the suppression part.
- It is on by default for every heating system and boiler brand. The user can switch it off.
- Apart from the start-up phase the boiler should sit at its lowest modulation (relative
  modulation near 0).

**Compared with thermo-nova and the firmware:**

- thermo-nova: the override is skipped when `not config.overshoot_protection`
  (`heating_control.py:385`); the default is `False` (`const.py:122`). The flag sits in the entry
  data (`entry_data.py:150-151`); a migration moves an old option value into the data
  (`__init__.py:160-162`), so installs that had it on keep it on. So "on by default for all" is a
  deliberate deviation from a fresh thermo-nova install.
- The override itself: `heating_control.py:380-448` (OFF -> `MINIMUM_SETPOINT` at `:388-389`;
  flame off -> return + `flame_off_setpoint_offset_celsius` clamped to `COLD_SETPOINT`..max at
  `:392-405`; hold for `modulation_suppression_delay_seconds` at `:415-433`; then flow minus
  `modulation_suppression_offset_celsius` at `:439-448`). Defaults 18.0 / 20 / 1.0
  (`const.py:151-153`).
- The override only runs when the control mode is PWM (`heating_control.py:224-225`), which
  matches "only in PWM".
- Firmware: `bOvershootProtection = true` (`SATtypes.h:465`, comment cites TASK-1268). So the
  firmware already follows this draft.

**Changes an earlier entry:** none. Note the chain inside this thread: 18:26 "identical" (off by
default), 19:36 brand list (Intergas, Ideal), 19:45 + Nefit, 20:01 + heat pumps, 20:04 "all
boilers", 20:13 "all heating systems", 20:27 OK. Only the last state counts.

**Task:** TASK-1268 (done, alpha.444 per our release note 1562338639730442344).

**Open:** nothing from him. Entry 7 and 11 replaced thermo-nova's `COLD_SETPOINT` 22 in the hold
clamp with 28.2 / 21.

## 21. Maximum relative modulation (MM): 0 in PWM, never 0 for heat pumps and hybrids, Geminox at least 10 (2026-04-05 to 2026-10-20)

**Source:**

- #dev-sat-mqtt, 2026-04-05 20:21, 20:33 and 20:39 UTC (heat pump), 20:48 UTC (special handling),
  2026-04-06 03:49 UTC (manufacturer list), 2026-04-07 06:08 UTC (Task #85 ACs), 16:26 UTC:
  https://discord.com/channels/812969634638725140/1105556725714649128/1490446199092674793
  https://discord.com/channels/812969634638725140/1105556725714649128/1490449270917038110
  https://discord.com/channels/812969634638725140/1105556725714649128/1490450926572666931
  https://discord.com/channels/812969634638725140/1105556725714649128/1490453111108665364
  https://discord.com/channels/812969634638725140/1105556725714649128/1490559126118334646
  https://discord.com/channels/812969634638725140/1105556725714649128/1490956333040668732
  https://discord.com/channels/812969634638725140/1105556725714649128/1491112038787842258
- #dev-sat-mqtt, 2026-06-20 19:17 UTC:
  https://discord.com/channels/812969634638725140/1105556725714649128/1517971664112062564
- #alpha-testing, 2026-10-19 17:53 UTC (Intergas), 2026-10-20 19:36, 19:56, 19:58, 19:58 and
  20:14 UTC (replies to our TASK-1268 messages 1562185620615462934, 1562192960651665440 and
  1562193864255868940):
  https://discord.com/channels/812969634638725140/1514720723980259460/1561799494855565428
  https://discord.com/channels/812969634638725140/1514720723980259460/1562187666328649729
  https://discord.com/channels/812969634638725140/1514720723980259460/1562192908277383169
  https://discord.com/channels/812969634638725140/1514720723980259460/1562193209147400313
  https://discord.com/channels/812969634638725140/1514720723980259460/1562193341091946521
  https://discord.com/channels/812969634638725140/1514720723980259460/1562197328797114390

**Quoted:**

> @BlueFlame Bot first of all we need to keep the modulation at 100% when a heat pump is selected (
> we need to create a dropdown list of heating source type ). And the PWM cycles should be 2 per
> hour or 1.5 per hour.

> @BlueFlame Bot in releases/thermo-nova we use a suppression logic to force boilers or heat pumps
> work better. From our research we found out that the Elga Ace heat pump when you send the MM=0
> never starts heating. Which means that it is safer to let the max modulation to 100% and the
> suppression logic will force the heat pump work at its lowest modulation

> @BlueFlame Bot The suppression logic is the same for gas boiler and heat pumps. The key
> differences are PWM cycles ( heat pump 1.5 or 2 ) ( gas boilers 3 and 4 ) and Max modulation
> limits ( heat pumps always 100% ) ( gas boilers 0% when the boiler is on in PWM )

> @BlueFlame Bot here is another one special handling
> <https://github.com/Alexwijn/SAT/blob/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat/coordinator/mqtt/opentherm.py#L216>

> Yes we know that. That’s why we also have an “innocent”manufacturer dropdown list for the user to
> confirm the brand. Since we have the user confirmation SAT can proceed the special handling of
> some brands.

> @BlueFlame Bot ACs reviewed,now everything is correct. Great job.

> @BlueFlame Bot remember when heat pump is selected ALWAYS THE MAX RELATIVE MODULATION SHOULD
> REMAIN AT 100%

> @BlueFlame Bot when the heat pump option is selected, it should affect ONLY the max modulation
> management. MUST always stay at 100%, never send the 0%

> Unfortunately this is Intergas fault. Intergas is playing dirty regarding the Maximum modulation
> management. You cannot set the max modulation  ( MM command ), we had to follow the workaround
> method to control intergas boilers. You limit the output of the boiler through the boiler
> settings. If you change the Max modulation to a value let’s say 35%, it won’t affect the boiler
> operation. It’s a pity but it is the reality

> b) Intergas and Ideal boilers ignore MM commands. [...] Also in the thermo-nova branch is another
> boiler brand that do not accept the MM=0 commands and stops the boiler. So for this particular
> boiler we use the value MM=9 if I remember correctly. Can you look at the code and confirm this?

> @BlueFlame Bot wait we have another MM limitation. Elga Ace does not support MM=0 it just stops the
> heat pump, so we need to keep modulation at 100% or the desired max modulation limit the user
> selects always for heat pumps. Let’s keep the heat pump rule generic. Because we don’t know how
> they will behave at MM=0

> GEMINOX should have MM=10 instead of MM=0

> This is the only change, otherwise Robert is correct

> @BlueFlame Bot  also hybrid systems should be treated as heat pumps.

**Interpretation (ours):** "ACs reviewed, now everything is correct" (2026-04-07) approved Task #85,
whose ACs included "PWM enabled: MM=0", "Continuous mode: MM=max_modulation", "DHW active:
MM=max_modulation" and "Idle states: CS=10 + MM=max + CH=0". "This is the only change, otherwise
Robert is correct" (19:58) means: Geminox gets 10, and the rest of our 19:57 rule stands.

**Settled by this statement:**

- Gas boilers: MM=0 during PWM; MM = the user's maximum in continuous mode, with DHW active, and
  when idle.
- Heat pumps, and hybrids treated the same: MM never 0. It stays at 100 % or the user's maximum in
  every phase, PWM included. The suppression sequence (entry 20) still runs for them.
- Geminox: MM never below 10.
- Intergas (and Ideal) ignore MM; for them only the suppression sequence limits the output (entry 20).
- Brand handling relies on the manufacturer the user confirms.

**Compared with thermo-nova and the firmware:**

- thermo-nova, gas: MM is `MINIMUM_RELATIVE_MODULATION` (0, `const.py:37`) when the state is OFF
  (`heating_control.py:517-518`), and the state is OFF whenever PWM is enabled and no hot water is
  active (`heating_control.py:134-140`), so in both PWM phases. Otherwise MM =
  `maximum_relative_modulation` (`heating_control.py:519-520`). On heater OFF it is set to the
  maximum (`heating_control.py:234`). This matches the gas rule above, and replaced the April AC
  "MM=max in PWM OFF" (section 3, C10).
- thermo-nova, Geminox: `max(10, value)` (`heating_control.py:522-523`). The floor is 10, not 9, as
  he asked.
- thermo-nova, Immergas (the link he gave): before MM it also sends `TP=11:12={min(value, 80)}`
  (`coordinator/mqtt/opentherm.py:217-218`).
- thermo-nova, Ideal / Intergas / Geminox / Nefit are marked as not reporting relative modulation
  (`coordinator/__init__.py:304-313`); MM is still sent for them
  (`supports_relative_modulation_management` returns True, `coordinator/__init__.py:316-322`).
- thermo-nova, heat pumps: a rule that keeps MM at 100 for heat pumps or hybrids is **not found in
  the code**. `HeatingSystem.HEAT_PUMP` is used only for the cycles-per-hour options
  (`config_flow.py:686-693`) and an old overshoot-protection setpoint table (`const.py:189-193`).
  So the heat-pump and hybrid MM rule is his own rule on top of thermo-nova.
- thermo-nova, manufacturer: the config flow offers a manufacturer select
  (`config_flow.py:531`).
- Firmware: `satComputeMM` (`SATcontrol.ino:379-385`): heat pump outside PWM -> 100; PWM -> user
  maximum for heat pump or hybrid, else 0; a heat pump or hybrid never gets 0. The Geminox floor and
  the Immergas TP command were not checked in this run.

**Changes an earlier entry:** none.

**Task:** TASK-85 (CH/MM commands), TASK-1268 (heat pump, hybrid, Geminox 10).

**Open:**

- Firmware: in continuous mode a heat pump gets 100, not "the desired max modulation limit the user
  selects" (his 19:56 words allow either). Ask whether 100 or the user maximum is meant.
- Whether the Immergas `TP=11:12` command is ported was not checked in this run.

## 22. Continuous to PWM on overshoot: cold start in continuous, margin 2.0 °C for 60 s (2026-10-20)

**Source:** #alpha-testing:

- 2026-10-20 20:27 UTC, cold start description:
  https://discord.com/channels/812969634638725140/1514720723980259460/1562200581961941054
- 20:30 and 20:31 UTC, reply to our message of 20:28 (1562200968425242776: thermo-nova switches
  when the flow stays at or above requested setpoint + 3 °C for 60 s):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562201355316232316
  https://discord.com/channels/812969634638725140/1514720723980259460/1562201731113422919
- 2026-10-21 05:55 UTC, reply to our TASK-1268 release note (1562338639730442344):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562343655648399422

**Quoted:**

> @BlueFlame Bot `thermo-nova` branch has another way of handling cold starts. When the boiler is
> Cold the CS is above the BT. So starts the boiler in normal mode ( sends CH=1 CS= The output of
> the Heating curve + PID MM=100 ), when the BT exceeds the CS ( BT -CS >= 2 ) starts a timer for
> 60 seconds   ( I am not sure for the interval ) and then proceeds to PWM.

> Change the overshoot margin Celsius to 2.0.

> sAT will identify the overshoot earlier and will apply the PWM sooner

> Why switcing to 10? Since it is a continuous burning, the CS = Heat Curve + PID, then when SAT
> detects an overshoot ( BT - CS >= 2 for 60seconds ) SAT enters in the PWM mode. ( CS = BT - 1 ).
> Remember the CS that we send to the boiler is not the same as the duty cycle duration. The ON/OFF
> intervals are being calculated in the PWM file of the `thermo-nova` branch.

**Interpretation (ours):** "CS" in "BT - CS >= 2" is the heating curve + PID output, which is the
requested setpoint. So the switch compares the flow with the requested setpoint, not with the
setpoint after the continuous clamp. "CS = BT - 1" in PWM is the suppression setpoint of entry 20.

**Settled by this statement:**

- A cold start runs in continuous mode first: CH=1, CS = heating curve + PID, MM = maximum.
- SAT enters PWM when the flow stays at or above requested setpoint + 2.0 °C for 60 s.
- Margin 2.0 instead of thermo-nova's 3.0, so the overshoot is seen earlier.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `_maybe_enable_pwm_on_sustained_overshoot` (`heating_control.py:262-305`): threshold
  `requested_setpoint + OVERSHOOT_MARGIN_CELSIUS` (`:288`), flow below it resets the timer
  (`:289-291`), switch after `OVERSHOOT_SUSTAIN_SECONDS` 60 (`:296`, `const.py:21`), then
  `self._pwm.enable()` (`heating_control.py:304`). Margin 3.0 (`cycles/const.py:10`). Skipped with hot water, within
  the DHW guard, heater off, or requested setpoint at or below `COLD_SETPOINT`
  (`heating_control.py:268-282`). He was "not sure" of 60 s; the code has 60 s.
- Firmware: compares flow with the requested setpoint (`SATcycles.ino:1002`), margin
  `fOvershootMargin` default 2.0 (`SATtypes.h:454`), 60 s (`SATcycles.ino:29`), DHW guard
  (`SATcycles.ino:1015-1017`). Matches this draft. Firmware uses `>` where thermo-nova uses `>=`
  (`flowTemp > setpoint + margin` at `SATcycles.ino:1018` against `flow < threshold` resets at
  `heating_control.py:289`); a 0.1 °C edge, not raised with him.

**Changes an earlier entry:** entry 15 uses margin 2.0 for the end-of-cycle trigger and refers to
this 2026-10-20 approval. This draft records the approval itself for the 60 s runtime switch.

**Task:** TASK-1268 (margin default 2.0, compare with the requested setpoint).

**Open:** whether the 2.0 margin also applies to the cycle classifier OVERSHOOT test
(`cycles/classifier.py:74`) or only to the switches; the firmware uses one setting for both
(`SATtypes.h:454` comment). Earlier margin statements disagree (section 3, C2).

## 23. Ignition hold: a settable delay, then straight to flow minus 1, refreshed every few seconds (2026-10-20)

**Source:** #alpha-testing, 2026-10-20 19:27 UTC (his description) and 19:38 UTC, reply to our
message of 19:34 (1562187355941769338: the code holds 20 s, not 18, and then sets flow - 1 at once,
no gradual step):
https://discord.com/channels/812969634638725140/1514720723980259460/1562185474288652301
https://discord.com/channels/812969634638725140/1514720723980259460/1562188371965517906
Also #dev-sat-mqtt, 2026-04-07 16:14 and 16:19 UTC:
https://discord.com/channels/812969634638725140/1105556725714649128/1491108815826059477
https://discord.com/channels/812969634638725140/1105556725714649128/1491110089514549379
And the 20:27 UTC OK on our final TASK-1268 plan, item 2 "CS refresh every ~3 s" (entry 20 source).

**Quoted:**

> @BlueFlame Bot In original theme-nova branch when SAT detects that the  fire is ON keeps the
> return temperature + 18 for 18 seconds and then falls back slowly by reducing the CS gradually
> anti clamps to boiler temperature - 1

> @BlueFlame Bot Hold time
> Actually it is a slider in the original code that can change.
>
> After the hold
> Ok we can go straight to BT-1, not an issue at all

> @BlueFlame Bot check the releases/thermo-nova branch about how we handle the CS command during
> start up in PWM ON. It is crucial to follow the procedure step by step, otherwise cycling will
> start. When you find it, describe it.

> @BlueFlame Bot Exactly. The procedure is complicated and you should be very careful when you will
> apply it. Now you are free to work on it.

**Settled by this statement:**

- The hold after ignition is a setting (default 20 s), not a fixed 18 s.
- After the hold the CS goes straight to flow minus the offset (1 °C); no gradual step-down.
- His 19:27 description ("18 seconds", "falls back slowly") is replaced by his own 19:38 answer.
- On 2026-04-07 he confirmed the four-step sequence as we described it: PWM OFF CS=10; ON with the
  flame off return + offset; hold after ignition; then flow minus offset.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `modulation_suppression_delay_seconds` default 20 (`const.py:152`), offered in the
  options flow (`config_flow.py:769`); hold at `heating_control.py:415-433`; then
  `flow_temperature - modulation_suppression_offset_celsius` at once (`:439-448`), recomputed on
  every control update. No gradual step: not found in the code.
- Refresh rate: thermo-nova recomputes on each control update; the 2-3 s figure came from his
  documentation (our 20:17 plan cites "spec page §2.1.1.2.5"); the interval itself was not traced in
  the code in this run.
- Firmware: hold until the delay after ignition, then flow - 1, refreshed about every 3 s (our
  release note 1562338639730442344); the code was not re-opened in this run.

**Changes an earlier entry:** none.

**Task:** TASK-1268.

**Open:** nothing from him.

## 24. Heating curve coefficient default 2.0; stored values stay; the convex curve is deliberate (2026-10-20)

**Source:** #alpha-testing, 2026-10-20 18:26 UTC, answer b) to our question of 18:18
(1562168252719824938: keep cold cutoff 28.2 and coefficient 1.5, or follow Python 22 and 2.0?), and
18:54 UTC, reply to our follow-up of 18:53 (1562176959922966691: migrate stored 1.5 to 2.0 once, or
leave existing settings alone?):
https://discord.com/channels/812969634638725140/1514720723980259460/1562170196897038447
https://discord.com/channels/812969634638725140/1514720723980259460/1562177212155826199
And #dev-sat-mqtt, 2026-05-08 05:42 UTC, reply to number3nl ("why do the curves converge?"):
https://discord.com/channels/812969634638725140/1105556725714649128/1502183820454006835

**Quoted:**

> b) Cold cutoff 28.2,
>      Curve Coefficient 2.0

> Leave existing settings alone

> Oh you noticed that 🙃. good catch. Real case scenarios shown that convex curves are performing
> better even on mild and extreme conditions. I concluded in that type of formula after a lot of
> conversations with users. And it looks they are happy with SAT performance.

**Settled by this statement:**

- Default heating curve coefficient 2.0, as in thermo-nova (`const.py:182`).
- Units that already store a coefficient keep it; no migration.
- Cold cutoff 28.2 (later extended to 28.2 / 21, entries 7 and 11).
- The curve is convex on purpose: thermo-nova
  `4 * (target - 20) + 0.03 * (outside - 20) ** 2 - 0.4 * (outside - 20)` (`heating_curve.py:30`),
  scaled as `base_offset + (coefficient / 4) * value` (`heating_curve.py:39`).

**Compared with the firmware:** `fHeatingCurveCoeff = 1.5f` (`SATtypes.h:442`). The default is still
1.5. TASK-1268 put the coefficient out of scope ("curve coefficient default 2.0" is listed for
TASK-1273), and TASK-1273 lists it as a remaining item. Our 18:53 message told him "curve
coefficient default becomes 2.0"; that has not happened yet (section 3, C5).

**Changes an earlier entry:** none.

**Task:** TASK-1273 (remaining item).

**Open:** the firmware default change. Not a question for him.

## 25. The thermostat as user interface: two-way room setpoint, on change only (2026-10-19 and 2026-10-21)

**Source:** #alpha-testing:

- 2026-10-19 09:13 UTC, reply to gruijter's feature request (1561662629615771710):
  https://discord.com/channels/812969634638725140/1514720723980259460/1561668633283928136
- 09:47 UTC, reply to gruijter (1561670125663293523: "I would like it to be 2-directional"):
  https://discord.com/channels/812969634638725140/1514720723980259460/1561677173134336072
- 17:49 UTC, reply to our TASK-1259 message (1561794709654081677):
  https://discord.com/channels/812969634638725140/1514720723980259460/1561798500923080906
- 2026-10-21 18:29 UTC, answers to our TASK-1259 plan (1562523233524121862: default on when push is
  on? clear an active preset? minimum time between adopted changes?):
  https://discord.com/channels/812969634638725140/1514720723980259460/1562533353125707929
- Earlier, superseded: #dev-sat-mqtt, 2023-05-10 19:36 and 19:37 UTC, replies to Alex ("one-way or
  two-way sync?", "SAT -> Thermostat?"):
  https://discord.com/channels/812969634638725140/1105556725714649128/1105941562065702952
  https://discord.com/channels/812969634638725140/1105556725714649128/1105941693825548458

**Quoted:**

> Great question!
>
> The thermostat should only be in sync with SAT for the room setpoint, other than that, when SAT is
> active, SAT should drive the boiler. When SAT is inactive or the software detects a malfunction in
> SAT, then should fallback to thermostat ( normal ) operation.

> I agree on that! @BlueFlame Bot the 2-directional room setpoint is a great suggestion. Can you add
> it?

> @BlueFlame Bot yes, it should be triggered only on room setpoint change, not in intervals.

> Open points:
> 1) yes
> 2) yes, because the override happens for a reason
> 2) no minimum time. Since most thermostats have a step of 0.5, then SAT a default step should
> have also 0.5

2023 (superseded):

> Let’s keep the one way sync.

> Yes, as it is now

**Interpretation (ours):** the three numbers answer our three open points in order (his third line
is numbered "2" again): 1) the option defaults to on when push setpoint is on; 2) a thermostat
change clears an active preset; 3) no minimum time between adopted changes, and SAT's default target
step is 0.5 °C.

**Settled by this statement:**

- The room setpoint syncs both ways between the thermostat and SAT. Nothing else syncs: while SAT is
  active it drives the boiler.
- A thermostat change is adopted only when it changes, never on a timer.
- Default on when push setpoint is on.
- An adopted thermostat change clears an active preset.
- No minimum time between adopted changes.
- SAT's default target step is 0.5 °C.

**Compared with thermo-nova and the firmware:**

- thermo-nova follows a configured HA thermostat entity: it listens for state changes
  (`climate.py:730-734`) and adopts a changed target with `cascade=False`, so it is not pushed back
  (`climate.py:750-765`). Edge-triggered, as he says. The push the other way is entry 14 / 16
  (`climate.py:641-657`).
- Target step default 0.5 (`const.py:169`, used at `climate.py:346-348`).
- Clearing a preset on an adopted thermostat change: **not found in the code**
  (`async_set_target_temperature`, `climate.py:641-667`, does not touch the preset). So point 2 is
  his rule on top of thermo-nova.
- Firmware: `bFollowThermostat` default false, but units with no stored value get the value of
  `bPushSetpoint` (`SATtypes.h:461` comment). That matches point 1. The preset, step and edge
  behaviour were not checked in this run.

**Changes an earlier entry:** entry 2 records Robert's adopt rule and says the interaction with
push setpoint is the maintainer's decision. This draft adds Sergeant D's own answers from before
entry 2; it does not change entry 2. His 2023 "keep the one way sync" is superseded (section 3, C9).

**Task:** TASK-1259 (follow thermostat), TASK-1298 (late adoption on a Honeywell Round).

**Open:** whether the firmware clears the preset on an adopted change and uses a 0.5 step; not
checked in this run.

## 26. SAT owns the control setpoint while enabled; the thermostat is the fallback (2026-04-05 to 2026-10-23)

**Source:**

- #dev-sat-mqtt, 2026-04-05 11:23, 11:25, 11:26 and 11:39 UTC (SAT + DHW integration discussion,
  bot messages 1490306906873401414 and 1490306964759117876, then number3nl's three-mode proposal):
  https://discord.com/channels/812969634638725140/1105556725714649128/1490310838181953598
  https://discord.com/channels/812969634638725140/1105556725714649128/1490311385186439270
  https://discord.com/channels/812969634638725140/1105556725714649128/1490311558373310535
  https://discord.com/channels/812969634638725140/1105556725714649128/1490314992413835366
- #dev-sat-mqtt, 2026-05-29 08:32 UTC, point 1, reply to our log analysis (1509833131950342300: two
  writers on TSet, 45 and 10):
  https://discord.com/channels/812969634638725140/1105556725714649128/1509836827815575602
- #alpha-testing, 2026-10-19 09:13 UTC (quoted in entry 25) and 2026-10-23 10:24 UTC, reply to gruijter
  (1563130401290194955: "Does that mean SAT cannot work as full replacement of a physical
  thermostat?", after our 10:00 explanation that with a PIC the thermostat stays the bus master):
  https://discord.com/channels/812969634638725140/1514720723980259460/1563136062254088242

**Quoted:**

> The physical thermostat and the SAT climate entities should be separated but also should be in
> sync. If for any reason the SAT is not working the physical thermostat will continue heating as a
> back up solution.

> That way the user will have the time to troubleshoot what is going on while the heating will
> continue heating the come. It is like an advanced fall back mechanism. Like integrating 2 FADECs
> exactly how commercial jet engines do.

> Yes but always sits in between

> Yes looks good

> 1. When SAT is enabled, ONLY SAT should drive TSet. Otherwise a conflict will occur.

> @gruijter this is how SAT should work. And it works properly now.

**Interpretation (ours):** "this is how SAT should work" (2026-10-23) answers gruijter's question
after our explanation: with a PIC gateway the thermostat stays the OpenTherm master and SAT changes
what goes to the boiler. We read it as: SAT in gateway position is the intended design. It could
also refer to his own 10:21 message (CS follows BT); the reply target is gruijter, so we take the
first reading.

**Settled by this statement:**

- While SAT is enabled, only SAT writes the control setpoint (MsgID 1, TSet). No second writer.
- The physical thermostat and SAT stay separate but in sync (room setpoint, entry 25). When SAT is off or
  faulted, the thermostat controls the boiler again.
- In gateway position SAT sits between the thermostat and the boiler.

**Compared with thermo-nova and the firmware:**

- thermo-nova is a Home Assistant integration that sends commands through the gateway
  (`TC=`, `CH=`, `MM=` in `coordinator/mqtt/opentherm.py:206-222`). A fallback to the thermostat
  when SAT fails is **not found in the code**; with the OTGW the fallback is the gateway dropping
  SAT's override. This is a firmware design rule, not a thermo-nova behaviour.
- Firmware: TASK-1150 (single TSet owner while SAT is enabled) and the CS keepalive rule in
  CLAUDE.md (SAT's own CS via `satEnqueueOwnCS()`); not re-opened in this run.

**Changes an earlier entry:** none.

**Task:** TASK-1150.

**Open:** nothing from him.

## 27. Room temperature input: stale timeout, a way to push it, redundancy, two decimals (2026-04-05 to 2026-09-24)

**Source:**

- #alpha-testing, 2026-09-24 14:03 UTC, reply to our alpha.376 release note (1552436795046625463:
  "SAT trips when the room temperature goes stale ... 5 minutes"):
  https://discord.com/channels/812969634638725140/1514720723980259460/1552681842749079614
- #dev-sat-mqtt, 2026-04-05 11:32, 11:35 and 11:39 UTC; 2026-04-06 11:58 UTC; 2026-05-29 08:32 UTC
  point 2:
  https://discord.com/channels/812969634638725140/1105556725714649128/1490313103060107355
  https://discord.com/channels/812969634638725140/1105556725714649128/1490314021239394335
  https://discord.com/channels/812969634638725140/1105556725714649128/1490314854064590879
  https://discord.com/channels/812969634638725140/1105556725714649128/1490682192425255085
  https://discord.com/channels/812969634638725140/1105556725714649128/1509836827815575602

**Quoted:**

> Maybe we should increase the room temperature stale-detection timeout to 20 minutes instead of 5.
> Some sensors do not report frequently enough, so the new state may not be received within 5
> minutes.

> Maybe you could create a special command, similar to the OT command. At the moment, Home
> Assistant can feed the outside temperature to the board through the OT command. So, a new command
> for the room temperature would be greatly appreciated.
>
> We could call it the RTS command (Room Temperature Sensor), and this command would be responsible
> for feeding the room temperature to the SAT.

> That’s the best solution

> Yes, this will make the OTGW32 standalone and the HA or any smart home ecosystem won’t be mandatory

> Moreover it would be great if we can combine more than one ble sensors as inside temperature
> sensors. That way we can ensure redundancy. And fall-back mechanism

> @BlueFlame Bot when you will implement the ble sensors. They should display 2 decimals, not just
> one. Sat is very sensitive on this ok?

> 2. A nice feature we can add is to use another temperature sensor as a fallback. So if the first
> one fails the firmware will use the second one. Like a fail-safe mechanism.

**Interpretation (ours):** the 20-minute timeout is a suggestion ("Maybe"), not an order. The RTS
request asks for a way to push the room temperature; the MQTT topic `sat/indoor_temp` and
`POST /api/v2/sat/externaltemp` (which he praised on 2026-10-03, message 1555963692644835398) may
already meet it. His 2026-10-18 idea to use the PIC `RT` command was withdrawn by him the same
evening (two boards, not a decision).

**Settled by this statement:**

- A BLE room sensor is the preferred way to make SAT standalone.
- More than one room sensor, with a fallback to the next when one fails.
- Room temperature shown and used with two decimals.
- Proposed: stale timeout 20 minutes instead of 5.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `sensor_max_value_age` default `"06:00:00"` (6 h, `const.py:170`). When the inside
  sensor is older than that, `control_pid` resets the PID and skips the control step
  (`climate.py:468-478`); there is no trip. Area sensors use the same age (`area.py:46`, `:76-83`).
- Firmware: `SAT_STALE_TEMP_MS` 5 min for the thermostat Tr (`SATcontrol.ino:141`, TASK-1157),
  `SAT_STALE_TEMP_BLE_MS` 5 min (`SATcontrol.ino:140`), area 5 min (`SATcontrol.ino:162`), BLE
  5 min (`SATble.ino:42`). The 20-minute suggestion is not applied (section 3, C8).
- Two decimals and the sensor fallback chain in the firmware were not checked in this run.

**Changes an earlier entry:** none.

**Task:** TASK-1157 (stale trip). Others: none yet.

**Open:** ask him to choose the timeout: his 20 min, thermo-nova's 6 h, or the current 5 min, and
whether a stale room temperature should trip SAT (firmware) or only reset the PID and hold
(thermo-nova).

## 28. The SAT PID: integral only inside the deadband, derivative frozen there; filter the sensor, not the PID (2026-04-05 to 2026-10-03)

**Source:**

- #dev-sat-mqtt, 2026-04-05 11:54 and 12:23 UTC, replies to our summary (1490317177448828928, which
  said TASK-8 made the integral active outside the deadband):
  https://discord.com/channels/812969634638725140/1105556725714649128/1490318738774556785
  https://discord.com/channels/812969634638725140/1105556725714649128/1490326104429301782
- 19:42, 19:48 and 19:54 UTC:
  https://discord.com/channels/812969634638725140/1105556725714649128/1490436555050057789
  https://discord.com/channels/812969634638725140/1105556725714649128/1490437913413029970
  https://discord.com/channels/812969634638725140/1105556725714649128/1490439501552812093
- 2026-05-08 17:41 UTC, reply to our alpha.28 note (1502364375862739144: integral clamp now
  symmetric ±curve):
  https://discord.com/channels/812969634638725140/1105556725714649128/1502364735666913320
- 2026-06-21 07:53, 07:56, 08:12 and 08:35 UTC (derivative swing 0 to 7):
  https://discord.com/channels/812969634638725140/1105556725714649128/1518162039929966623
  https://discord.com/channels/812969634638725140/1105556725714649128/1518162754270396569
  https://discord.com/channels/812969634638725140/1105556725714649128/1518166704231022672
  https://discord.com/channels/812969634638725140/1105556725714649128/1518172435835523214
- #alpha-testing, 2026-10-03 07:49 and 08:42 UTC (the 08:42 reply answers our 08:29 question
  1555859428584980561):
  https://discord.com/channels/812969634638725140/1514720723980259460/1555849318299209791
  https://discord.com/channels/812969634638725140/1514720723980259460/1555862708765593692

**Quoted:**

> @BlueFlame Bot revert task-8. Integral should work inside the deadband. This is a custom PID
> controller. The derivative should be working outside the deadband and imitates the heating curve
> move up/down function

> Integral works as an external heating sources compensator ( sun, cooking, activity ) when the
> controller is in the deadband. @BlueFlame Bot are you ok with these informations?

> This is how integral in the release works in the thermo-nova branch
> <https://github.com/Alexwijn/SAT/blob/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat/pid.py#L185>
>
> this is how the integral is designed to work.
>
> @BlueFlame Bot  remember sat is not a tradition heating curve + PID controller.
>
> Some more context, a jet engine while in start up sequence uses a PID controller to control the
> engine, if for any reason the integral is stucked the engine start process ends up to a hot or hung
> start. Integral is always tricky. In buildings we can use the heating curve as the integral
> replacement. Which means we can eliminate any integral issues ( open doors, instant domestic hot
> water during initial heating of the building ) and replace it with the heating curve. So sat
> outside the deadband works as a P + Heating Curve + Deravative. While in deadband works as a P +
> Heating Curve + Integral. Integral is smooth and works as compensator. Is it clear now, or you need
> some more explanation?

> And this is how the derivative works
> <https://github.com/Alexwijn/SAT/blob/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat/pid.py#L199>.
>
> Remember SAT keeps the last derivative value outside deadband and then acts as a heating curve move
> up/down in the deadband. Which means it stays freezed inside deadband. Is it clear @BlueFlame Bot?

> Yes, update them

> @BlueFlame Bot the integral clamps between the deadband, outside deadband is 0

> @BlueFlame Bot I choose the option 1. Moreover thoroughly examine the PID.py file from the
> thermostat-nova branch and confirm that your implementation is identical.

> @BlueFlame Bot it is crucial to implement correctly the pip.py file. SAT uses a custom PID
> controller. Please be very careful.

> @BlueFlame Bot Regarding the 0 -> 7 swing. The KD reduction won’t solve the issue. A stronger low
> pass filter may is one solution to eliminate the sensor’s error. Or it is better to apply a
> low-pass filter to the sensor’s raw data before pass it to the PID controller?

> @BlueFlame Bot Yes implement it. Leave the pid untouched and add a filter to the raw sensor’s
> data.
>
> I am not so familiar with the solar gain since I don’t use it and it is a new development. I am
> sorry I can’t help about that

> @BlueFlame Bot then maybe it is better to apply the same filter both on the indoor temperature
> sensor and on the calculation of the derivative. So all the calculations will be as smooth as
> possible

> @BlueFlame Bot No, don’t change anything. Everything looks good now

**Interpretation (ours):** "the integral clamps between the deadband, outside deadband is 0" reads
as: the integral accumulates only inside the deadband and is 0 outside it. The clamp bound in the
code is the heating curve value, not the deadband. "Option 1" (2026-06-21) was our proposal to add
the deadband skip to the derivative; we found minutes later that the firmware already had it.

**Settled by this statement:**

- Outside the deadband: P + heating curve + derivative. The integral is 0.
- Inside the deadband: P + heating curve + integral; the derivative keeps its last value (frozen).
- The integral compensates for other heat sources (sun, cooking, activity).
- The PID must be identical to `pid.py`. Sensor noise is handled by a low-pass filter on the raw
  room temperature before the PID, not by changing the PID.
- 2026-10-03: the current filter chain (room EMA before the PID, plus the derivative's own
  low-pass) stays as it is.
- Solar gain: he does not use it and cannot advise (2026-06-21).

**Compared with thermo-nova and the firmware:**

- thermo-nova: `_update_integral` sets the integral to 0 when `abs(error) > DEADBAND`
  (`pid.py:185-189`), else adds `ki * error * PID_UPDATE_INTERVAL` and clamps to ± the heating curve
  value (`pid.py:191-192`). `DEADBAND = 0.1` (`const.py:16`).
- thermo-nova: `_update_derivative` returns without changing the derivative when
  `abs(error) <= DEADBAND` (`pid.py:205-207`), so the last value is kept. Outside: raw slope
  `-temperature_delta / delta_time` (`pid.py:220`), cap (`:222-225`), low-pass
  `alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time)` (`pid.py:228-230`). Matches his account.
- A room-temperature filter before the PID: not in thermo-nova (it reads the HA entity,
  `climate.py:271-279`). It is a firmware addition he asked for (TASK-894).
- Firmware PID code was not re-opened in this run; the 2026-06-21 bot audit claimed term-by-term
  parity, which this run did not verify.

**Changes an earlier entry:** none. Entry 11 (answer 3) settles the integral freeze during solar
gain.

**Task:** TASK-894 (EMA on the raw sensor), TASK-1195 (derivative timer).

**Open:** the two firmware-only PID extras named on 2026-06-21 (absolute integral cap ±20 and a
10 s minimum between updates) never got an answer from him.

## 29. "Python always wins", and the rulings of 2026-06-20 on saturation, DHW status and OPV calibration

**Source:** #dev-sat-mqtt, 2026-06-20 18:31 UTC, reply to our documentation review
(1517959395680714865: the docx and the Python disagree in 3 places; which is ground truth?), and
18:55 UTC, answers to our five questions (1517961095472484462):
https://discord.com/channels/812969634638725140/1105556725714649128/1517960092085911582
https://discord.com/channels/812969634638725140/1105556725714649128/1517966230223327322

**Quoted:**

> @BlueFlame Bot Python always wins.

> 2. Since it is a ram issue, better address @Robert van den Breemen
>
> 3. Match python code, it is tested and works reliably.
>
> 4. Follow Python
>
> 5. Since it is not in the Python, remove it completely, it is an obsolete development.

(Answer 1 of the same message, the cold setpoint, is recorded in entries 7, 11 and 12.)

**Settled by this statement:**

- When his documentation and the thermo-nova code disagree, the code wins. This matches the
  ground-truth order in CLAUDE.md (his recorded decisions first, then the code).
- 2: the classifier depth on the ESP is a RAM question for Robert.
- 3: the saturation guard follows Python: PWM is disabled when the PWM off time stays 0 for 300 s.
- 4: a distinct "heating hot water" status (flame on with DHW) as in Python.
- 5: the old OPV calibration is removed completely.

**Compared with thermo-nova and the firmware:**

- 3: `_maybe_disable_pwm_on_sustained_saturation` (`heating_control.py:358`),
  `SATURATION_SUSTAIN_SECONDS = 300.0` (`const.py:23`). Firmware: `satPwmLastOffTimeMs() == 0`
  sustained for `SAT_SATURATION_SUSTAIN_SEC` (`SATcycles.ino:1051-1062`).
- 4: the DHW status in `device/status.py` was not opened in this run.
- 5: our question said OPV calibration is "not in the Python at all". That is not fully true:
  thermo-nova keeps the constants (`const.py:187-196`: required dataset, setpoints per system,
  storage key, service names) and a migration that copies a stored `overshoot_protection_value`
  into `minimum_setpoint` (`__init__.py:118-119`). No calibration procedure was found in the code.
  So his ruling stands, but it rested on a slightly wrong premise (section 3, C11).

**Exceptions he made later to "Python always wins"** (each is his explicit decision):

- cold setpoint 28.2 / 21 instead of 22 (entries 7, 11);
- overshoot margin 2.0 instead of 3.0 (entry 22, entry 15);
- overshoot protection on by default for all systems (entry 20);
- heat pumps and hybrids never MM=0 (entry 21);
- clearing the preset on an adopted thermostat change (entry 25);
- removal of Force PWM matches thermo-nova (it never reads the option, entry 13), so it is not an
  exception.

**Changes an earlier entry:** none.

**Task:** TASK-891 (parity remediation of 2026-06-20).

**Open:** whether "Python always wins" still holds in the same words now that entries 1-18 exist.
The CLAUDE.md order (his decisions, then the code) already expresses it.

## 30. Device type and heating system are separate settings; heat pump cycles (2026-04-05 to 2026-06-20)

**Source:**

- #dev-sat-mqtt, 2026-04-05 20:21, 20:24 and 20:39 UTC:
  https://discord.com/channels/812969634638725140/1105556725714649128/1490446199092674793
  https://discord.com/channels/812969634638725140/1105556725714649128/1490447036276015375
  https://discord.com/channels/812969634638725140/1105556725714649128/1490450926572666931
- 2026-06-20 19:15, 19:17, 19:31, 19:48 and 20:33 UTC (replies to our 19:09 and 19:45 messages
  1517969766013468672 and 1517978621392982077, and to number3nl 1517990467432747099):
  https://discord.com/channels/812969634638725140/1105556725714649128/1517971076897050745
  https://discord.com/channels/812969634638725140/1105556725714649128/1517971664112062564
  https://discord.com/channels/812969634638725140/1105556725714649128/1517975087528018150
  https://discord.com/channels/812969634638725140/1105556725714649128/1517979352304976085
  https://discord.com/channels/812969634638725140/1105556725714649128/1517990916881776801
- 2026-06-20 21:26, 21:30 and 21:32 UTC, reply to our A/B question (1518002534734630966) and to
  number3nl:
  https://discord.com/channels/812969634638725140/1105556725714649128/1518004246493139014
  https://discord.com/channels/812969634638725140/1105556725714649128/1518005177758515200
  https://discord.com/channels/812969634638725140/1105556725714649128/1518005735605145852

**Quoted:**

> @BlueFlame Bot since you already suggested the auto detection of the heating system, you can use
> this solution instead of the dropdown list.

> @BlueFlame Bot regarding 1 good question. It is not gas boiler or heat pump related. It is related
> to heating system

> @BlueFlame Bot even the base offset should be only heating system related.  It was a small mistake
> since the beginning. The heating system ( radiators or underfloor) should only affect the base
> offset

> @BlueFlame Bot I think we need to distinguish the heating source and the heating system.
>
> 1. ~~Heating source~~ As you @BlueFlame Bot  mentioned the correct naming Device Type should be
>   a. Gas boiler
>   b. Heat pump
>
> When a heat pump is selected the Max modulation should always be 100% even in PWM
>
> We are able to auto detect whether a boiler or a heat pump is present by the ID3:LB2. If the bit
> is 1 means that cooling is enabled to the slave. Only heat pumps are able to provide cooling. When
> a gas boiler is present ID3:LB2 should be 0. @Robert van den Breemen can you validate my
> assumption please with your Elga Ace?
>
> 2. Heating system options:
>   a. Radiators or Mixed Systems
>   b. Underfloor

> Heating source or device type sound good both to me.

> @BlueFlame Bot  A is the answer. Moreover we can have 2 options for the cycles.
> a. 2 cycles/hour ( 1800sec) ( default )
> b. 1.5 cycles/hour ( 2400sec )

> There are just 2 options. The user will decide. Default value is 2 cycles/hour.

> Yes, like the gas boiler’s option. We have the options 3 cycles/hour and 4 cycles/hour

(The 2026-04-05 20:21 and 20:39 quotes are in entry 21. The 19:17 quote "it should affect ONLY the max
modulation management" is in entry 21.)

**Interpretation (ours):** "A is the answer" chose option A of our 21:20 question: heat pumps keep
their own timing (long minimum on time, at most 2 cycles per hour, max setpoint 40 °C) and in
addition never get MM=0. This conflicts with his 19:17 "ONLY the max modulation" (section 3, C3).

**Settled by this statement:**

- Two separate settings: device type (gas boiler, heat pump; heating source is also fine as a name)
  and heating system (radiators or mixed, underfloor).
- The heating system alone sets the base offset (27.2 radiators, 20 underfloor) and the cold setpoint
  (28.2 / 21, entries 7 and 11).
- Device type heat pump: MM never 0 (entry 21), plus heat-pump timing (option A).
- Heat pump cycles: 2 per hour (default) or 1.5 per hour, user choice. Gas boiler: 3 or 4 per hour.
- Proposed auto-detection of a heat pump from MsgID 3 low byte bit 2 (cooling supported), to be
  confirmed by Robert on his Elga Ace.

**Compared with thermo-nova and the firmware:**

- thermo-nova has one axis: `HeatingSystem` = unknown / heat_pump / radiators / underfloor
  (`types.py:110-114`). The base offset is 20 for underfloor and 27.2 for everything else, heat pump
  included (`types.py:116-118`). A separate device type: **not found in the code**.
- thermo-nova cycles options: heat pump 2 ("Normal") or 3 ("High") per hour
  (`config_flow.py:686-693`); radiators 3 or 4 (`config_flow.py:695-701`); default 4 (`const.py:119`);
  a migration sets heat pump to 2 and radiators to 3 (`__init__.py:171-176`). The period is
  `3600 / cycles_per_hour` (`pwm.py:54`). A 1.5 per hour option: **not found in the code**
  (section 3, C4).
- Auto-detection from MsgID 3 bit: **not found in the code**.
- Firmware: `iCyclesPerHour` default 3, range 2-6 (`SATtypes.h:524`). The device type / heating
  system split (TASK-891.8 per our 19:45 message) was not checked in this run.

**Changes an earlier entry:** none.

**Task:** TASK-891, TASK-22 (heating system type with heat pump support).

**Open:** C3 and C4 in section 3. Whether Robert confirmed the ID3:LB2 detection on the Elga Ace is
not in the harvest.

## 31. PWM duty ranges come from the cycles per hour; 180 s minimum flame time; the low-duty special case (2026-04-05 and 2026-04-06)

**Source:** #dev-sat-mqtt, 2026-04-05 20:05 UTC (our Task #14 AC list, 1490440757226504212),
2026-04-06 09:26, 09:30, 09:31, 09:35, 09:47 and 10:16 UTC (our messages 1490642887048958014,
1490644129112391730, 1490647999653613569, 1490649351297564794):
https://discord.com/channels/812969634638725140/1105556725714649128/1490442317692407999
https://discord.com/channels/812969634638725140/1105556725714649128/1490643874488782930
https://discord.com/channels/812969634638725140/1105556725714649128/1490644818131423295
https://discord.com/channels/812969634638725140/1105556725714649128/1490645186878111764
https://discord.com/channels/812969634638725140/1105556725714649128/1490646035834343505
https://discord.com/channels/812969634638725140/1105556725714649128/1490649142333276311
https://discord.com/channels/812969634638725140/1105556725714649128/1490656440401137674

**Quoted:**

> @BlueFlame Bot  The #2 acceptance criterion is the most important. Ensures that when the flame is
> on, SAT always keeps the flame on for at least 180 seconds in order to prevent short cycling. So
> give me more explanation of this #2 AC

> @BlueFlame Bot in the PWM.py file we developed a math formula that correlates the duty cycles per
> hour with the on/off times

> @BlueFlame Bot yes it is very important since SAT is a super dynamic controller.

> @BlueFlame Bot moreover the ultra low function actually keeps the boiler off means eg. 0 sec on
> 900 sec off

> @BlueFlame Bot there is only a special handling when the the flame is not yet on for 180 secs and
> the controller instructs 0 sec on 900 off. Then the controller keeps the flame on for 180 seconds
> and then falls back to the 0 sec on 900 sec off. Do you see it in the PWM.py file?

> @BlueFlame Bot Yes it is crucial. The ultra low range is a special case. Normally the ultra low
> means keep the boiler off for the specific interval. Can you please show me the updated PWM
> function?

> @BlueFlame Bot The 900 seconds is used for 4x cycles per hour. En the 3x cycles per hour is
> selected then the cycle seconds should change to 1200. The PWM.py explains how it is working

**Interpretation (ours):** his "#2 acceptance criterion" text describes the 180 s minimum on time;
in our list that was AC #4 ("Min on-time: 180s"), AC #2 was the five ranges. We read his words, not
the number.

**Settled by this statement:**

- Once the flame is on, it stays on at least 180 s (no short cycling).
- The duty thresholds come from the cycles per hour, not fixed percentages: period
  `3600 / cycles_per_hour` (900 s at 4, 1200 s at 3).
- Very low duty: the boiler stays off. If the flame is already on, it first completes 180 s.

**Compared with thermo-nova and the firmware:**

- thermo-nova: minimum on time 180 (`pwm.py:53`), period `3600 / max(1, cycles_per_hour)`
  (`pwm.py:54`), maximum `2 * period` (`pwm.py:55`), lower duty `180 / period`, upper `1 - lower`,
  minimum `lower / 2`, maximum `1 - minimum` (`pwm.py:58-61`).
- thermo-nova low-duty case (`pwm.py:245-260`): flame on and no hot water -> on 180, off
  `2 * period - 180`; otherwise on 0, off `2 * period`. At 4 cycles per hour the off time is 1800 s,
  not 900 s as he wrote (section 3, C6).
- Firmware: our 09:48 message showed the same mapper; the current code was not re-opened in this
  run.

**Changes an earlier entry:** none. Entry 8 covers the duty-cycle formula and on/off seconds in
general; entry 1 covers the boiler temperature in the formula.

**Task:** none found by number (our Task #14 of 2026-04-05; not looked up).

**Open:** C6 (900 vs 1800 s) is a wording point; the code is clear.

## 32. Idle states: valves closed means CS=10; humidity only with the comfort option; Summer Simmer stays (2026-04-06 to 2026-04-09)

**Source:** #dev-sat-mqtt, 2026-04-06 21:02 UTC, reply to our Task #29 note (1490818210306461948:
"When valves closed: sends CS=0"); 2026-04-07 05:35 UTC, review of our layer summary
(1490944952601935962); 2026-04-09 10:46 and 12:04 UTC:
https://discord.com/channels/812969634638725140/1105556725714649128/1490819012899377444
https://discord.com/channels/812969634638725140/1105556725714649128/1490948103753302158
https://discord.com/channels/812969634638725140/1105556725714649128/1491751274687889528
https://discord.com/channels/812969634638725140/1105556725714649128/1491770702439649360

**Quoted:**

> @BlueFlame Bot when all valves are closed we keep the CS=10 not 0. Which actually means we do not
> start heating because all valves are closed and we stay at idle. Is it clear?

> Regarding Layer 3:
> Humidity comes to play ONLY when summmer summer index is enabled. Otherwise you use only the
> temperature sensor values.

> I see in your summary that you only manipulate the CS value, how about the CH command? Keep in
> mind that when the heating is starting we also need to send the CH=1 command and when the heater
> goes to idle send the CH=0 command. Otherwise the boiler won’t start/stop heating.

> @BlueFlame Bot you can drop the summer summer index implementation to save some memory.

> Ok then. We can keep it

**Settled by this statement:**

- All valves closed: CS=10 (idle), not 0; SAT does not heat.
- Humidity is used only when the Summer Simmer (thermal comfort) option is on.
- CH=1 when heating starts, CH=0 when idle (the detailed CH rules are entries 5, 7, 9, 11, 17).
- Summer Simmer stays (he first offered to drop it for memory, then kept it once number3nl said it
  was not the memory problem).

**Compared with thermo-nova and the firmware:**

- thermo-nova: heater state OFF when no valve is open (`heating_control.py:69-70`), and heater OFF
  sets `_control_setpoint = MINIMUM_SETPOINT` (10, `heating_control.py:233`, `const.py:35`) and CH
  from that setpoint (`heating_control.py:238`). Matches "CS=10, idle".
- thermo-nova: the room temperature becomes the Summer Simmer index only when
  `presets.thermal_comfort` is on (`climate.py:276-277`), default off (`const.py:131`). Otherwise
  humidity is only shown as attributes (`climate.py:242-246`). Matches his statement.
- Firmware: not re-opened in this run (TASK-1282 notes the comfort adjust runs only with Summer
  Simmer on).

**Changes an earlier entry:** none.

**Task:** TASK-29 (valves), TASK-1282 (humidity source).

**Open:** nothing from him.

## 33. DHW: one slider bounded by the boiler, a HW switch only for a storage tank (2023-05-10 to 2026-05-02)

**Source:** #dev-sat-mqtt, 2023-05-10 04:08 and 04:29 UTC, 2023-09-12 19:44 UTC; 2026-04-05 10:17
and 10:18 UTC; 2026-04-09 06:17 and 06:36 UTC; 2026-04-28 06:07 UTC; 2026-05-02 19:52, 19:54 and
20:09 UTC (replies to our TASK-516 messages 1500203598695633008 and 1500225532959264888):
https://discord.com/channels/812969634638725140/1105556725714649128/1105707977882734602
https://discord.com/channels/812969634638725140/1105556725714649128/1105713242493431818
https://discord.com/channels/812969634638725140/1105556725714649128/1151241951270678529
https://discord.com/channels/812969634638725140/1105556725714649128/1490294287475408997
https://discord.com/channels/812969634638725140/1105556725714649128/1490294496792154223
https://discord.com/channels/812969634638725140/1105556725714649128/1491683481657409626
https://discord.com/channels/812969634638725140/1105556725714649128/1491688337419927613
https://discord.com/channels/812969634638725140/1105556725714649128/1498566333447798844
https://discord.com/channels/812969634638725140/1105556725714649128/1500223464932380956
https://discord.com/channels/812969634638725140/1105556725714649128/1500224007134380052
https://discord.com/channels/812969634638725140/1105556725714649128/1500227713380651199

**Quoted:**

> SAT not only controls the DHW temperature setpoint, but respects the DHW boundary values. Alex did
> his magic tricks 😎

> We use a slider instead of a climate entity, moreover we have boundary values ( MsgID 48 hb-lb )

> Which means the slider is custom tailored for each boiler

> @BlueFlame Bot The DHW slider needs the boundary values. Boilers report these values through
>
> TdhwSetUBTdhwSetLB_value_l
>
> TdhwSetUBTdhwSetHB_value_l
>
> The current value is the TdhwSet.
> And the precision is 1°C. Boilers handle DHW setpoint as integers.
> When a DHW setpoint temperature change occurs you need to send the SW={value} command to the
> boiler.
> About the DHW switch.
>
> You need to follow the domestichotwater in order to know the state of the switch.
> When the users change the state of the switch then you need to send the new state through the HW=
> 0/1 command. Tell me your opinion on this?

> @BlueFlame Bot let me be straight.
> If the ID3:HB3 equals to 0 we need to expose only the DHW slider.
> Otherwise we need to expose the DHW slider AND a DHW enable switch which will manipulate DHW
> through the HW command ( DHW switch is off -> HW=0 is sent to the boiler -> DHW is disabled, DHW
> switch is on -> HW=1 is sent to the boiler -> DHW is enabled ). Is it clear?

> I think (b). What do you think @Robert van den Breemen ?

**Settled by this statement:**

- DHW is set with one slider, bounded by the boiler's MsgID 48 limits, current value TdhwSet,
  1 °C steps (integers). A change sends `SW={value}`.
- MsgID 3 high byte bit 3 = 0 (no storage tank): only the slider. Bit 3 = 1: the slider plus a DHW
  enable switch; the switch sends HW=0 / HW=1 and its state follows `domestichotwater`.
- For TASK-516 he chose option (b), continuous tank hold-up setpoint; number3nl agreed
  (2026-05-02 20:02, message 1500226013643411456).

**Compared with thermo-nova and the firmware:**

- thermo-nova (OTGW MQTT coordinator): DHW setpoint sent as `SW={value}`
  (`coordinator/mqtt/opentherm.py:201-203`); bounds from `TdhwSetUBTdhwSetLB_value_lb/_hb`
  (`coordinator/mqtt/opentherm.py:30-31`, read at `:92-103`); DHW state from `domestichotwater`
  (`coordinator/mqtt/opentherm.py:22`, `:69`).
- A HW= enable switch gated on MsgID 3 bit 3: **not found in the code**. It is his firmware design.
- Firmware: TASK-234, TASK-516 and TASK-1232 (slider bounds from MsgID 48); not re-opened in this run.

**Changes an earlier entry:** none.

**Task:** TASK-234, TASK-516, TASK-1232.

**Open:** nothing from him.

## 34. SAT onboarding asks the questions of the HA SAT config flow (2026-07-04)

**Source:** #alpha-testing, 2026-07-04 15:35 UTC, reply to crashevans' guided-setup proposal
(1522981711162507448):
https://discord.com/channels/812969634638725140/1514720723980259460/1522989168186232882

**Quoted:**

> I think we should match the config flow when you initially set up SAT in home assistant. We
> already use there these questions as @crashevans mentioned before

**Settled by this statement:** a SAT setup assistant in the firmware follows the questions of the
thermo-nova config flow, not a new question list.

**Compared with thermo-nova:** the config flow lives in `config_flow.py` (for example the
manufacturer select at `config_flow.py:531`, the cycles options at `config_flow.py:686-701`). The full step list was not
traced in this run.

**Changes an earlier entry:** none. **Task:** none yet (not searched by number).
**Open:** nothing from him.

## 35. Multiple areas: port upstream PR #172; any discovered sensor can be an area sensor (2026-05-08 and 2026-06-21)

**Source:** #dev-sat-mqtt, 2026-05-08 17:55 UTC and 2026-06-21 11:04 UTC:
https://discord.com/channels/812969634638725140/1105556725714649128/1502368354214871170
https://discord.com/channels/812969634638725140/1105556725714649128/1518210120503263293

**Quoted:**

> @BlueFlame Bot IN sat repo we have an open pull request. Please add it to the code. It is related
> to multiroom <https://github.com/Alexwijn/SAT/pull/172>

> @BlueFlame Bot I think the ds18b20 area sensor mapping should be renamed to just  Area sensor
> mapping. I think it is better to include all the discovered sensors in the areas dropdown. Since
> we are talking for other rooms it is a bit difficult to hardwire a ds18b20, but it is pretty
> simple to add a ble sensor.

**Settled by this statement:**

- The multi-room change of upstream PR #172 is wanted in the port.
- The area sensor setting is named "Area sensor mapping" and offers every discovered sensor, BLE
  included (number3nl agreed, 2026-06-21 16:04).

**Compared with thermo-nova:** areas are in `area.py` (for example the overshoot margin 0.3 at
`area.py:33` and the stale check at `:76-83`). Whether PR #172 is part of the pinned commit cannot be
told from the local copy: not checked.

**Changes an earlier entry:** none. **Task:** none yet (not searched by number).
**Open:** whether PR #172 was ported, and whether it is in the pinned commit.
