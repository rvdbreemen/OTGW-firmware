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

## 36. The end-of-cycle switch waits for a per-sample control setpoint in the tail (2026-10-24)

**Source:** #alpha-testing, 2026-10-24, answer to Q2 of our overview of all open SAT questions
(20:53 UTC, messages 1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1563657900973490277

**Quoted:**

> Q2: yes

**Interpretation (ours):** Q2 (`docs/sat/open-questions.md`, "## Q2") offered three options: A
(store the control setpoint per tail sample, as thermo-nova, and only then switch modes at the end
of a cycle; recommended), B (ship the end-of-cycle switch with the per-cycle setpoint and accept the
ping-pong risk), C (keep the end-of-cycle switch off until A is done). "yes" names no option, so we
read it as yes to our recommendation, option A. If he meant B or C, this entry is wrong and must be
followed by a correcting entry.

**Settled by this statement:**

- The cycle classifier compares each tail sample's flow with that sample's own control setpoint,
  as thermo-nova does, not with one setpoint taken when the flame came on.
- The end-of-cycle mode switch of entry 15 ships only together with, or after, that change, so a
  switch out of PWM in the middle of a burn cannot make the cycle look like an overshoot and send
  SAT straight back to PWM at flame-off.

**Compared with thermo-nova and the firmware:**

- thermo-nova: the tail error is computed per sample as `flow_temperature - sample.control_setpoint`
  (`cycles/tracker.py:267-271`), over the samples from `tail_start` (`cycles/tracker.py:237-241`,
  `tail_start = max(observation_start, end_time - 180.0)` at `cycles/tracker.py:116`). The
  continuous classifier tests its p90 against the overshoot margin and its p50 against the
  underheat margin (`cycles/classifier.py:71-75`). `pwm.py:on_cycle_end` enables PWM on a
  continuous OVERSHOOT and disables it on a PWM UNDERHEAT (`pwm.py:219-227`).
- Firmware: the tail ring stores flow only (`_tail_samples[SAT_TAIL_SAMPLE_SIZE]`,
  `SATcycles.ino:100`, filled at 1 Hz at `SATcycles.ino:882`). The control setpoint is one value,
  `_cycle_setpointAtStart = state.sat.fFinalSetpoint` at flame-on (`SATcycles.ino:725`), passed to
  `_cycleClassify` at flame-off (`SATcycles.ino:800-803`), which subtracts it from the tail
  percentiles (`SATcycles.ino:654-655`). The comment at `SATcycles.ino:634-638` records this as a
  deliberate per-cycle-constant approximation. The tail is 180 samples on the ESP32-S3
  (`src/libraries/Platform/src/boards.h:122`, `:194`), so a second float ring for the setpoint
  costs 180 x 4 = 720 bytes, as stated in Q2.
- Not in the question and not decided: the firmware's in-cycle overshoot seconds and shape metrics
  also use `_cycle_setpointAtStart` (`SATcycles.ino:896`, `:905`), while thermo-nova's shape
  metrics use the per-sample control setpoint with a fallback to the boiler setpoint
  (`cycles/tracker.py:191-197`).

**Changes an earlier entry:** none. It adds a prerequisite to entry 15 (port of `on_cycle_end`,
TASK-1331): the per-sample tail setpoint lands before or with the end-of-cycle switch. Entry 18
(the switch ignores the PWM auto switch setting) is unaffected.

**Task:** to be filed (prerequisite of TASK-1331).

**Open:** confirmation that "yes" means option A (our reading). Whether the in-cycle overshoot
seconds and shape metrics should also move to a per-sample setpoint was not asked.

## 37. Overshoot margin 2.0, and a cycle at exactly 2.0 counts as an overshoot (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 20:59 UTC (time decoded from the message ID). It answers our overview of all open SAT questions sent at 20:53 UTC (messages 1563656700118958262 and 1563656702086221987). This answers question Q3 in `docs/sat/open-questions.md`:
https://discord.com/channels/812969634638725140/1514720723980259460/1563658161821327361

**Quoted:**

> Q3: 2 or more

**Interpretation (ours):** "2 or more" repeats the wording of Q3 option C ("Keep 2.0 and '2.0 or more'"). We read it as option C. The margin stays 2.0, the comparison is `>=`, and he accepts that a continuous cycle whose tail sits on the flow-offset clamp is classified OVERSHOOT and switches to PWM. This is our reading of a short reply. He did not repeat the consequence in his own words.

**Settled by this statement:**

- The end-of-cycle overshoot test compares the tail p90 of the flow-minus-control-setpoint error with `>=` against a margin of 2.0. thermo-nova uses the same comparison, `p90 >= OVERSHOOT_MARGIN_CELSIUS` (`cycles/classifier.py:74`), but with a margin of 3.0 (`cycles/const.py:10`). Only the margin differs from thermo-nova, and it differs on purpose (entry 15).
- Options A (count only "more than 2.0") and B (use thermo-nova's 3.0) are rejected.
- He accepts the consequence named in Q3. In continuous mode, with the flame on and a known boiler temperature, thermo-nova raises a requested setpoint that lies below the boiler temperature minus the flow offset up to that minimum (`heating_control.py:485-489`, offset default 2.0 at `const.py:154`). The exceptions are: no clamp when the boiler temperature is missing (`:458-465`) or the flame is off (`:476-483`), and on a falling request the setpoint stays at the previous value when that was already lower (`:489`). `boiler_temperature` is the boiler flow temperature (`coordinator/__init__.py:184-185`). A sample held on the clamp therefore has an error of about 2.0. With margin 2.0 and `>=`, a cycle whose tail sits on the clamp counts as OVERSHOOT. `pwm.py:219-223` (`on_cycle_end`) then enables PWM.

**Compared with thermo-nova and the firmware:**

- thermo-nova measures each sample against that sample's own control setpoint: `flow_temperature - sample.control_setpoint` (`cycles/tracker.py:267-271`). The overshoot test is in `_classify_continuous` only. That function returns UNCERTAIN when p90 or p50 is missing (`cycles/classifier.py:71-72`) and tests overshoot first (`:74`, `:77-78`).
- Firmware margin: `fOvershootMargin = 2.0f` (`src/OTGW-firmware/SATtypes.h:476`). Firmware flow offset: `fFlowOffset = 2.0f` (`SATtypes.h:489`). Firmware clamp: `minAllowed = boilerTemp - flowOffset` (`SATcontrol.ino:1377-1381`).
- The p90 comparison in the firmware classifier already uses `>=`: `ctrlErrP90 >= settings.sat.fOvershootMargin && overshootSec > 10.0f` (`SATcycles.ino:681`). The shape metric does the same: `ctrlErr >= settings.sat.fOvershootMargin` (`SATcycles.ino:909`).
- The firmware does NOT yet behave as this answer requires. Its classifier adds a second condition: the overshoot timer must exceed 10 s (`overshootSec > 10.0f`, `SATcycles.ino:681`), which is not in `cycles/classifier.py:74`. That timer only grows while `flowTemp > _cycle_setpointAtStart + settings.sat.fOvershootMargin` (`SATcycles.ino:896`), a strict `>`. A tail held at an error of exactly 2.0 therefore never fills the timer and is not classified OVERSHOOT. Through the timer the firmware acts as option A, which he rejected. A code change is needed.
- The firmware measures against a different setpoint. It does not use the per-sample control setpoint. It uses the setpoint taken at cycle start: `_cycle_setpointAtStart = state.sat.fFinalSetpoint` (`SATcycles.ino:725`), passed as `controlSp` to `_cycleClassify` (`SATcycles.ino:800-802`), with `ctrlErrP90 = tailP90 - controlSp` (`SATcycles.ino:654`). Suppose the clamp raises the control setpoint during the cycle. The firmware's error can then exceed 2.0 while thermo-nova's per-sample error stays at about 2.0. This is a firmware divergence, not something he decided.
- The 60 s sustained switch (entry 22) also uses a strict `>`: `flowTemp > setpoint + settings.sat.fOvershootMargin` (`SATcycles.ino:979`). thermo-nova uses `>=` there (`heating_control.py:289`, reset on `<`). Entry 22 already recorded this, but at the stale line `:1018`. His answer covers the end-of-cycle classifier. We read it as not deciding the 60 s path.
- The firmware does not yet have the end-of-cycle switch (`pwm.py:219-227` port, TASK-1331, entries 15 and 18). No firmware code switches to PWM on a continuous OVERSHOOT classification at cycle end (a grep of `SAT_CYCLE_OVERSHOOT` finds only statistics, health and display uses).

**Changes an earlier entry:** none changed. This completes entry 15. Entry 15 set the margin to 2.0 (1b) but left open whether a cycle exactly at 2.0 counts. Entry 22 lists as Open "whether the 2.0 margin also applies to the cycle classifier OVERSHOOT test (`cycles/classifier.py:74`) or only to the switches". Entry 15 (1b, the margin for the on_cycle_end trigger, which acts on the classifier result) and this answer together settle that question for the classifier: margin 2.0 with `>=`. This is our reading. Entry 22's own 60 s rule (margin 2.0 for 60 s) is unchanged. Entry 22 also refers to "section 3, C2" for earlier, disagreeing margin statements. We did not find that section, and it is not checked here.

**Task:** to be filed.

**Open:**

- The firmware classifier still has the extra 10 s timer condition with a strict `>` (`SATcycles.ino:681`, `:896`). It still measures against the setpoint at cycle start instead of the per-sample setpoint (`SATcycles.ino:725`, `:800-802`, `:654`). Both points stop the firmware from following this answer, and neither was put to him. Aligning them with `cycles/tracker.py:267-271` and `cycles/classifier.py:74` is open for the task that ports `on_cycle_end` (TASK-1331).
- The order of the firmware tests is Q4's subject, not this one: short before overshoot, and overshoot tested in both modes (`SATcycles.ino:675-683`), against continuous-only and overshoot-first in `cycles/classifier.py:69-78`.

## 38. Room temperature lost or missing: keep heating on the heating curve, no P term (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 21:04 UTC, answer to Q6 of our overview of 20:58 UTC
(1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1563659520889331794

**Quoted:**

> Q6: Keep heating on heat curve. Not proportional because it is related to error ( kP*error ). Since we don’t have a valid room temperature we cannot use Proportional

**Interpretation (ours):** "Keep heating" means SAT does not trip and does not hand the boiler back
to the thermostat when the room temperature is lost or missing. It keeps sending a control setpoint
taken from the heating curve alone. His reason ("related to error") applies to every error-based
term, so we read it as curve only: no P, and no I or D either. He names only P, so that last part is
our reading. He did not answer the other half of Q6 (which stale time), and he did not say what
happens when a valid reading comes back. We assume normal PID control resumes then, but that is not
stated.

**Settled by this statement:**

- A lost or missing room temperature does not stop SAT. SAT keeps heating.
- While there is no valid room temperature, the setpoint is the heating curve value without the
  proportional term (kP * error), because there is no valid error.

**Compared with thermo-nova and the firmware:**

- thermo-nova, stale sensor (older than `sensor_max_value_age`, default `"06:00:00"`,
  `const.py:170`; the check runs only when that age is above 0, `climate.py:470`): `control_pid`
  calls `self.pid.reset()` and returns before the PID update (`climate.py:470-478`). `reset()` sets
  only the integral and the raw derivative to 0 (`pid.py:134-137`); `_last_error` stays. The output
  is curve + proportional + integral + derivative (`pid.py:127-132`), with proportional = kp *
  `_last_error` (`pid.py:96-101`). So a stale sensor gives curve + a frozen P from the last error,
  as the audit said.
- thermo-nova, temperature not available (`temperature_state` is None, `climate.py:295-306`):
  `control_pid` returns without a reset (`climate.py:480-482`). Integral and derivative stay too, so
  the output stays curve + P + I + D, all frozen at their last values.
- thermo-nova, no error ever seen: proportional is 0 when `_last_error` is None (`pid.py:98-99`), so
  only then is the output the curve alone. `_last_error` is also restored from storage at start-up
  (`pid.py:150`), so even after a restart P can be non-zero.
- In both cases thermo-nova keeps writing: `requested_setpoint` uses `self.pid.output`
  (`climate.py:368-375`), and `async_control_heating_loop` returns early only when the outside
  temperature is None or the HVAC mode is OFF (`climate.py:544-545`). Further suppression inside
  `heating_control` was not checked in this run.
- His answer therefore differs from thermo-nova: thermo-nova keeps curve + frozen P (and, on the
  None path, frozen I and D too); he wants curve only. A "curve only on a missing room temperature"
  path is **not found in the code**.
- Firmware: a NaN or out-of-range room temperature (below -10 or above 50) makes the control tick
  return before it computes a new setpoint (`SATcontrol.ino:4900-4915`). On PIC boards the last
  CS is still re-sent by `satPicCSKeepalive()`, which runs earlier in the loop
  (`SATcontrol.ino:4839`), so the last-good setpoint is held. After `SAT_MAX_SKIP_COUNT` = 10 such
  ticks (`SATcontrol.ino:143`) it sets `bSafetyTripped` and calls `satDisable()`. Every later pass
  stops at `if (state.sat.bSafetyTripped) return;` (`SATcontrol.ino:4812`). Only an explicit enable
  clears the trip (`satClearSafetyTrip()`, `SATcontrol.ino:2139-2146`, reached from
  `SATcontrol.ino:2154` and `settingStuff.ino:994`), or a reboot (`SATcontrol.ino:3915`). The stale
  limits are 5 min for BLE and the thermostat Tr (`SATcontrol.ino:140-141`) and 5 min for areas
  (`SATcontrol.ino:162`). The audit cited `SATcontrol.ino:4836-4850` and `:4747`; the code has
  since moved to the lines above. His answer replaces the trip with curve-only heating.
- The curve itself needs the outside temperature. What SAT does when that is missing as well is
  Q7, not this answer.

**Changes an earlier entry:** entry 27. It settles the open point "should a stale room temperature
trip SAT (firmware) or only reset the PID and hold (thermo-nova)": neither. SAT keeps heating on the
curve without P. The stale-timeout choice of entry 27 (5 min, his 20 min, or 6 h) stays open.
It also narrows entry 26 ("when SAT is off or faulted, the thermostat controls the boiler again"):
a lost room temperature is not a fault that hands control back to the thermostat. Entry 26 is not
contradicted, because SAT stays on.
It adds an exception to entry 29 ("Python always wins"): thermo-nova keeps curve + frozen P, and he
chose curve only. This is his explicit decision and belongs in entry 29's list of exceptions.
Related, not changed: entry 28 (how I and D behave in the SAT PID); the open point below on I and D
must be read against it.

**Task:** to be filed.

**Open:**

- The stale time for the room temperature (second half of Q6; entry 27, conflict C8).
- Whether I and D are also left out while the room temperature is missing (our reading: yes), and
  whether the integral is kept or reset (see entry 28).
- What happens when a valid room temperature returns: resume PID at once, and with a PID reset or
  not.
- Audit #63 (thermal-estimate fallback and 2 h safe setpoint) becomes moot or needs a decision once
  the trip is gone; not re-opened in this run.

## 39. No valid OpenTherm: SAT stops, sends CH=0 and CS=0 once, then sends nothing to the boiler (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 21:07:18 and 21:08:32 UTC, written under Q7 in his reply to
our overview of the open questions of 20:53 UTC (messages 1563656700118958262 and
1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1563660179655098410
https://discord.com/channels/812969634638725140/1514720723980259460/1563660492306780372

**Quoted:**

> Q7: if the software doesn’t have any valid OT, then SAT should stop working

> CH=0, CS=0 once and then stop sending messages to the boiler

**Interpretation (ours):** Q7 asks what SAT should use when the outside temperature is missing or
old. His answer is about a different case: the firmware has no valid OpenTherm communication with
the boiler. We record it as a decision for a lost or invalid OpenTherm link and do not read it as
the answer to Q7. We read "valid OT" as valid boiler replies on the OpenTherm bus. He did not say
how long the link must be gone, whether a Data-Invalid reply counts as "not valid", or whether SAT
starts again by itself when the link comes back. We read "stop sending messages to the boiler" as
SAT's own commands only. On a PIC gateway the thermostat's own traffic still goes through the PIC.

**Settled by this statement:**

- When the firmware has no valid OpenTherm link, SAT stops regulating.
- On that stop SAT sends `CH=0` and `CS=0`, once each.
- After that SAT sends no further commands to the boiler.

**Compared with thermo-nova and the firmware:**

- thermo-nova: we did **not find in the code** any check for a lost link to the gateway or boiler that
  stops the control loop. The MQTT coordinator subscribes to the gateway topics and stores what
  arrives (`coordinator/mqtt/__init__.py:52-63`, `:100-120`). It has no availability or timeout
  check. `MAX_BOILER_TEMPERATURE_AGE = 60` is defined (`const.py:40`) but used nowhere in
  `custom_components/sat` (grep, this run). `HeatingControl.update()` sends `CS=`, `MM=` and `CH=`
  on every pass that reaches `update()` (`heating_control.py:236-238`; OTGW MQTT commands at
  `coordinator/mqtt/opentherm.py:195-222`), whatever the state of the link. A pass is skipped
  only when the outside temperature is unknown or HVAC is off (`climate.py:544-545`). When heating
  is off it sends CS=10 (`MINIMUM_SETPOINT`, `heating_control.py:233`, `const.py:35`) and CH=0
  (`heating_control.py:238`, `COLD_SETPOINT` 22 at `const.py:34`), never CS=0. The only CS=0 in
  thermo-nova is the serial coordinator's clean-up, `set_control_setpoint(0)` and
  `set_max_relative_mod("-")` (`coordinator/serial/__init__.py:228-231`, called from `:189`). It runs
  when the climate entity is removed and on Home Assistant shutdown (`climate.py:150`
  `EVENT_HOMEASSISTANT_STOP`, `:152`, `:162`). That path sends no CH=0.
- Firmware, link state: `state.otBus.bOnline` is true while the boiler or the thermostat was heard
  within 30 s (`OTGW-Core.ino:5150-5175`). On OT-Direct it also drops at once when a MsgID 0 request
  gets no reply (`OTDirect.ino:1514-1523`).
- Firmware, SAT: the control loop does not stop when the bus goes offline. It reads `bOnline` only to
  reset the write-on-change cache on the offline-to-online edge (`SATcontrol.ino:4785-4792`, TASK-565),
  so the commands are sent again when the link returns. CS, MM and CH keep being written on change,
  plus a 5-minute refresh (`SATcontrol.ino:427`, `:5219-5225`). On OT-Direct, queued commands are sent
  before the poll schedule's offline check (`OTDirect.ino:1549-1557` against `:1571-1572`), so SAT's
  commands still go out while the bus is offline.
- Firmware, nearest existing trip: SAT trips when no OT command interface is present for 5 control
  passes (`SAT_MAX_PIC_FAILS`, `SATcontrol.ino:144`; trip at `SATcontrol.ino:5240-5247`).
  `hasOTCommandInterface()` (`OTGW-firmware.h:560-562`) means PIC available or OT-Direct mode. It does
  not check bus liveness. The trip calls `satDisable()` (`:5246`), so it already enqueues CS=0, CH=1
  and MM=T once, but at a moment when no command interface is there to send them. The SAT setting stays
  on (`satIsEnabled()`, `SATcontrol.ino:2350-2357`). Every later pass returns at `SATcontrol.ino:4812`
  while `bSafetyTripped` is set. The trip is cleared only by an explicit enable or at boot
  (`SATcontrol.ino:3915`), not when the link returns. The enable path is `satClearSafetyTrip()`
  (`SATcontrol.ino:2139-2155`), called from `satHandleEnabled()` (`SATcontrol.ino:2154`) and
  `settingStuff.ino:994`.
- Firmware, the stop itself: `satDisable()` sends `CS=0`, then `CH=1` (TASK-1346) and `MM=T`
  (TASK-1344) (`SATcontrol.ino:2391-2401`). His rule asks for `CH=0`. The code comment at
  `SATcontrol.ino:2392-2395` says CH= only counts while a CS override is active (citing
  `gateway.asm:53` and `pyotgw.py:602-603`). The flag itself stays set, so on OT-Direct the heating
  curve or a later CS= would find SAT's last `CH=0` and switch central heating off again. This
  difference is open (see below).

**Changes an earlier entry:** none replaced. It adds the link-lost case to entry 26 ("When SAT is off
or faulted, the thermostat controls the boiler again"). There is a possible tension with entry 26:
on OT-Direct, a final `CH=0` can keep central heating off after a later CS override, the reason
`satDisable()` sends `CH=1` today. This goes to him with the open point below. It agrees with entry 32,
which has CH=0 when the heater goes idle. It is a different case from entry 32 (all valves closed:
CS=10, idle, not CS=0). It is also different from entry 27 (lost room temperature). Q6 covers entry 27
and he answered it at 21:04:41 UTC (message 1563659520889331794, recorded in its own entry): keep
heating on the heat curve, without the proportional term. A lost room temperature therefore keeps
heating, while a lost OpenTherm link stops SAT.

**Task:** to be filed.

**Open:**

- **Q7 itself is not settled.** What SAT should use when the outside temperature is missing or old is
  still open. We posted a clarifying question on 2026-10-24:
  https://discord.com/channels/812969634638725140/1514720723980259460/1563666350818463826 For
  reference only: thermo-nova skips the whole control pass and sends nothing while the outside
  temperature is unknown (`climate.py:544-545`).
- `CH=0` (his words) against the `CH=1` and `MM=T` hand-back that `satDisable()` sends today
  (TASK-1346, TASK-1344). To put to him with the OT-Direct reason above and entry 26. The order matters
  too: he wrote "CH=0, CS=0", `satDisable()` sends CS first, and the comment at `SATcontrol.ino:2393`
  says CH= only counts while a CS override is active. Our inference from its `gateway.asm` citation is
  that this applies to the PIC.
- What counts as "no valid OT": the 30 s liveness window of `bOnline`, the immediate MsgID 0 failure
  on OT-Direct, Data-Invalid replies, or something else; and how long before SAT stops.
- Whether SAT starts again by itself when the link returns, or stays stopped until the user enables
  it again (as the current trip does).
- "Once": whether the PIC CS keepalive (`satPicCSKeepalive()`, `SATcontrol.ino:487-489`) and the
  5-minute refresh must stay silent after the stop. Today both are skipped once SAT is switched off
  (`SATcontrol.ino:4799-4803`) or tripped (the return at `SATcontrol.ino:4812` comes before the
  keepalive at `:4839`). The keepalive is also skipped for a CS below 8.

## 40. Heat pumps in PWM: minimum on time 180 s; 1800 s and 2400 s are cycle lengths (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 21:13 UTC. This answers Q8 of the overview of all open
questions sent at 20:53 UTC (messages 1563656700118958262 and 1563656702086221987). In that
message Q8 read only "Heat pumps in PWM: minimum on-time and cycles per hour?". The full Q8 entry in
`docs/sat/open-questions.md` ("Heat pumps in PWM: minimum on-time and cycles per hour (audit #9,
conflicts C3/C4)") was linked but not quoted in the message:
https://discord.com/channels/812969634638725140/1514720723980259460/1563661665155813528

**Quoted:**

> Q8: Minimum time ON is 180 seconds. 2 options for the PWM cycles of a heat pump
> a) 30 minutes or 1800 seconds
> b) 40 minutes or 2400 seconds

**Interpretation (ours):** the 180 s minimum on time applies to heat pumps as well, the same value
as for gas boilers (entry 31). The 1800 s and 2400 s are the PWM cycle length (the period), not a
minimum on time. That matches his entry 30 wording "2 cycles/hour ( 1800sec)" and "1.5 cycles/hour
( 2400sec )", with period = 3600 / cycles per hour. We read "cycle length" as thermo-nova's
`_on_time_upper_threshold` (`pwm.py:54`). He did not name a default here; entry 30 says the default
is 2 cycles per hour (1800 s). The reply is short and answers a one-line question, so this reading
should be confirmed with him (see Open).

**Settled by this statement (under the interpretation above):**

- Heat pump in PWM: minimum on time 180 s.
- Heat pump PWM cycle length: 1800 s (2 per hour) or 2400 s (1.5 per hour). That the user picks
  between them comes from entry 30 ("There are just 2 options. The user will decide.").
- With the thermo-nova formulas (`pwm.py:53-61`) this gives, at 1800 s: lower duty 0.10, upper duty
  0.90, minimum duty 0.05, maximum duty 0.95, maximum cycle 3600 s. At 2400 s: lower 0.075, upper
  0.925, minimum 0.0375, maximum 0.9625, maximum cycle 4800 s. All five duty ranges stay reachable.
  (Our arithmetic, not a quote from him.)

**Compared with thermo-nova and the firmware:**

- thermo-nova: minimum on time is a fixed 180 for every heating system (`pwm.py:53`); period
  `3600 / max(1, cycles_per_hour)` (`pwm.py:54`); maximum `2 * period` (`pwm.py:55`); duty thresholds
  from those (`pwm.py:58-61`). So thermo-nova agrees with him on the 180 s.
- thermo-nova heat pump cycle options are 2 ("Normal") or 3 ("High") per hour
  (`config_flow.py:686-691`); a migration sets heat pumps to 2 (`__init__.py:171-173`); the general
  default is 4 (`const.py:119`). A 1.5 per hour (2400 s) option: **not found in the code**.
  `cycles_per_hour` is offered as an integer string ("2", "3"), so 2400 s cannot be expressed as an
  integer cycles per hour. His answer repeats entry 30's 2 / 1.5 per hour, not thermo-nova's 2 / 3
  (conflict C4 in the audit). The question he answered did not show him thermo-nova's 2 / 3, so this
  reply does not show that he chose 1.5 over thermo-nova's 3 with that conflict in view. C4 stays
  open.
- Firmware: `satGetMinOnTimeSec()` returns `settings.sat.iHpCycleSeconds` for a heat pump
  (`SATcontrol.ino:363`), so it uses 1800 or 2400 s as the minimum on time, and
  `satGetMaxCyclesPerHour()` returns a fixed 2 for a heat pump (`SATcontrol.ino:350`). The setting is
  `iHpCycleSeconds`, default 1800, commented "Heat-pump min-on / cycle rate" (`SATtypes.h:463`),
  range 1800-2400 (`settingStuff.ino:999`, `restAPI.ino:3879`). In `satApplyPWM` the minimum on time
  and the period come from those two functions (`SATcontrol.ino:867-870`); at 1800 s that gives
  dutyLower = 1800/1800 = 1.0 and dutyMin = dutyMax = 0.5 (`SATcontrol.ino:897-900`), so ranges 2-4
  are never reached: duty 0.5 or more is continuous ON (`SATcontrol.ino:905-910`), below 0.5 is the
  ultra-low branch with 1800 s on or none at all (`SATcontrol.ino:911-921`). At 2400 s dutyLower is
  1.33 and dutyMax 0.33. This is audit finding #9. Under our interpretation (not his words: he did
  not mention the firmware), the firmware should use `iHpCycleSeconds` as the period instead of the
  minimum on time, and use 180 s as the minimum on time for heat pumps too.

**Changes an earlier entry:** entry 30, not rewritten. Our interpretation there read "A is the
answer" as heat pumps keeping a "long minimum on time". Under the reading above, that part no longer
holds: the minimum on time is the common 180 s, and the heat-pump-specific value is the cycle length.
The entry confirms entry 30's heat pump options (2 or 1.5 per hour, as 1800 / 2400 s). It extends
entry 31 (180 s minimum flame time) to heat pumps.

**Task:** to be filed.

**Open:**

- Confirm the reading with him in one follow-up question (one question per message, with options
  and the thermo-nova citation): "1800 / 2400 s is the PWM period and the minimum on time is 180 s"
  (A), or something else. In the same question, put C4 to him with thermo-nova's options shown: heat
  pump cycles 2 / 1.5 per hour (his entries 30 and 40) or 2 / 3 per hour (`config_flow.py:686-691`).
- C3 (heat pump "ONLY max modulation", entry 21 / 30, against "A is the answer" keeping heat-pump
  timing) is only partly settled: heat pumps keep their own cycle length choice, but the minimum on
  time is the common 180 s. The other heat-pump specifics in the firmware are not covered by this
  answer: the 40 °C setpoint cap (`SATcontrol.ino:308`) and the skipped sustained-overshoot switch
  (`satCycleCheckAutoSwitch` only runs when `!satAlwaysMaxModulation()`, `SATcontrol.ino:5190-5192`;
  `satAlwaysMaxModulation()` is true for a heat pump, `SATcontrol.ino:371`).
- Whether the gas-boiler / radiator cycles setting (`iCyclesPerHour`, default 3, range 2-6,
  `SATtypes.h:547`) should also be limited to his entry 30 options (3 or 4 per hour) is not part of
  this answer.

## 41. Continuous to PWM after 60 s of sustained overshoot, not 3 minutes (2026-10-24)

**Source:** #alpha-testing, 2026-10-24, answer to Q9 of the overview of open questions sent at
20:53 UTC (messages 1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1563661990759633008

**Quoted:**

> Q9: 60 seconds of overshoot

**Interpretation (ours):** Q9 in the overview asked "Continuous to PWM: 60 s or 3 minutes of
overshoot?" (`open-questions.md`, audit #12, conflict C1). We read his answer as: SAT switches from
continuous to PWM once the overshoot has lasted 60 s, as in thermo-nova and as he said on 2026-10-20
(entry 22). The 3 minutes he asked for on 2026-04-07 no longer apply. He did not say how the 60 s
should be counted; we take "60 seconds of overshoot" to mean 60 s from the first sample that is in
overshoot, which is what thermo-nova does.

**Settled by this statement:**

- The continuous-to-PWM sustain time is 60 s. Conflict C1 (3 minutes on 2026-04-07 against 60 s on
  2026-10-20) is closed in favour of 60 s.
- The doubt in entry 22 ("I am not sure for the interval") is resolved: 60 s is his choice, not only
  what the code happens to have.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `_maybe_enable_pwm_on_sustained_overshoot` (`heating_control.py:262-305`). The flow
  must be at or above `requested_setpoint + OVERSHOOT_MARGIN_CELSIUS` (`:288-291`). The timer comes
  from `sustained_runtime` (`:293-294`), which starts at 0 s on the first qualifying sample
  (`helpers.py:51-56`, `initialized=True`). The switch waits while
  `runtime.initialized or runtime.elapsed_seconds < OVERSHOOT_SUSTAIN_SECONDS` (`heating_control.py:296`),
  with `OVERSHOOT_SUSTAIN_SECONDS = 60.0` (`const.py:21`), then calls `self._pwm.enable()` (`:304`).
  So the overshoot must really last 60 s.
- Firmware: the constant matches, `SAT_OVERSHOOT_SUSTAIN_SEC = 60.0f` (`SATcycles.ino:29`). The
  counting does not. `satCycleCheckAutoSwitch` (`SATcycles.ino:952`) computes `dt` as the time since
  the previous call, whether or not that call saw an overshoot (`:955-956`), and adds it on every
  qualifying call (`:979-980`). The first sample in overshoot therefore already counts one full
  control interval, and the switch at `>= 60` (`:984-986`) comes after about one interval less than
  60 s of real overshoot. With the 30 s control tick named in audit #12, that is about 30 s. The tick
  interval itself was not reopened in this run. A gap above 60 s returns early (`:957`), so a control
  interval above 60 s disables this switch and the two PWM-to-continuous switches in the same
  function (`:993-1027`).
- His answer does not change the code's 60 s. On our reading of it (interpretation above), the
  firmware's counting becomes a fix to make, not a question: count from the first qualifying sample,
  as `helpers.py:51-56` does. Audit #12 already lists this as its action.

**Changes an earlier entry:** entry 22 (confirms its 60 s and removes his "not sure for the
interval"). It also replaces his 3-minute statement of 2026-04-07, which is recorded only as
conflict C1 in `sat-port-audit-2026-10-24.md` (finding #12 and section 4), not as a numbered entry
here. Its message link is not in our records.

**Task:** to be filed.

**Open:**

- Nothing from him on the 60 s.
- Not asked: whether the underheat (180 s, `const.py:22`, `SATcycles.ino:31`) and saturation
  (300 s, `const.py:23`, `SATcycles.ino:32`) timers follow the same counting rule. In the code we
  opened, thermo-nova uses the same `sustained_runtime` pattern only for the overshoot switch; the
  other two call sites were not opened in this run.
- Firmware uses `flowTemp > setpoint + margin` (`SATcycles.ino:979`) where thermo-nova resets only
  on `flow < threshold` (`heating_control.py:289`), so thermo-nova counts "equal" as overshoot. Already
  noted in entry 22. Q3 ("Overshoot margin 2.0 equals the flow offset 2.0", TASK-1331) is still queued.

## 42. PID cadence: a fixed 60 s PID update, as thermo-nova, not the control tick (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 21:15 UTC, answer to Q10 in our overview of all open SAT
questions (messages 1563656700118958262 and 1563656702086221987, 20:53 UTC; Q10: "PID update rate:
fixed 60 s as thermo-nova, or the control tick?", audit finding 14):
https://discord.com/channels/812969634638725140/1514720723980259460/1563662152831602801

**Quoted:**

> Q10: follow thermo-nova

**Interpretation (ours):** "follow thermo-nova" means option A of Q10. The PID runs on its own fixed
60 s timer, independent of the SAT control tick. Each run adds at most one 60 s integral step: only
inside the deadband, and only when the integral is not frozen. thermo-nova also runs the PID once at
start-up, so we read "follow thermo-nova" as covering that call too. We also read it as covering the
per-area (zone) PIDs, because thermo-nova runs those on the same 60 s timer. He did not mention zones,
so this part is our reading.

**Settled by this statement:**

- The main PID runs every 60 s, as thermo-nova does. thermo-nova registers `control_pid` with
  `async_track_time_interval(..., timedelta(seconds=PID_UPDATE_INTERVAL))` (`climate.py:712-716`),
  where `PID_UPDATE_INTERVAL = 60` (`pid.py:20`). It also calls `control_pid()` once when the entity is
  added (`climate.py:137` if HA is running, otherwise `climate.py:143` on `EVENT_HOMEASSISTANT_STARTED`).
  thermo-nova calls `control_pid` from nowhere else (a grep of `custom_components/sat` for
  `control_pid(` finds only these sites and the area ones below). A run that finds a stale room
  sensor resets the PID, and a run without a room temperature skips the update
  (`climate.py:470-482`).
- The PID cadence no longer follows the SAT control tick. In thermo-nova, the 5 s heating-control loop
  (`climate.py:706-710`) is a separate timer from the PID timer.
- The integral step stays `ki * error * PID_UPDATE_INTERVAL`, at most once per 60 s run: only inside
  the deadband (outside it the integral is 0, `pid.py:187-189`) and only when the integral is not
  frozen (`pid.py:167-168`). The integral therefore builds up at the same rate as in thermo-nova
  (`pid.py:191`).
- Zones (our reading, see above): each area PID also runs on a 60 s timer (`area.py:188-190`), with
  one call at start-up (`area.py:183-186`).

**Compared with thermo-nova and the firmware:**

- Firmware main PID: `satPidUpdate()` runs inside the control loop
  (`src/OTGW-firmware/SATcontrol.ino:5072`). The control loop runs on `timerSATControl`, whose period
  is `settings.sat.iControlInterval` (`SATcontrol.ino:582`). That setting defaults to 30 s
  (`SATtypes.h:467`), and users can set it from 10 to 300 s (`settingStuff.ino:1004`,
  `restAPI.ino:3892`). Within that loop, `satPidUpdate()` skips an update in two cases:
  - less than 10 s has passed (`SATpid.ino:279-283`, `SAT_PID_SAMPLE_TIME_LIMIT`, `:21`);
  - the error changed by less than 0.001 and less than 60 s has passed (`SATpid.ino:286-288`).

  In all other cases it updates. Inside the deadband, and when Ki is not about 0, each update adds the
  full 60 s step (`SATpid.ino:166-169`, `SAT_PID_UPDATE_INTERVAL = 60`, `:22`). When the error changes
  by at least 0.001 between ticks, the integral can therefore build up about twice as fast as in
  thermo-nova at the default 30 s tick, and up to six times as fast at 10 s. With an unchanged error,
  the second gate already holds updates to one per 60 s. These factors are upper bounds (audit
  finding 14, an estimate).
- Firmware zone PID: `satZonePidStep()` (`SATcontrol.ino:2004`) is called on every control tick
  (`SATcontrol.ino:5087`) and has no time gate. Inside the deadband each call adds
  `ki * error * 60.0f` (`SATcontrol.ino:2044`). The zone integral therefore builds up faster in the
  same way, and here without the error-change gate.
- The derivative time gate already matches thermo-nova: the firmware skips the derivative when
  `deltaTime <= SAT_PID_UPDATE_INTERVAL` (`SATpid.ino:222`), as `pid.py:217` does. This answer does not
  change that.
- A firmware control tick that is not 60 s therefore has to drive the PID from a separate 60 s timer
  (or an elapsed-time check of exactly 60 s). How to build this is a design choice for the task, not
  part of his answer.

**Changes an earlier entry:** entry 28. That entry says the PID must be identical to `pid.py`. Its
2026-10-03 quote ("No, don't change anything. Everything looks good now") could be read as also
accepting the firmware's control-tick cadence (ADR-197 open question; audit finding 14). This answer
settles that reading: the cadence is not kept, and the PID follows thermo-nova's 60 s timer. The rest
of entry 28 stands. Entry 28 also left open the 10 s minimum between updates (`SATpid.ino:21`,
`:279-283`). That question is not settled here; it stays with Q25. Related, not changed: entry 35
(multiple areas, upstream PR #172), whose area code (`area.py`) is the basis for our zone reading.

**Task:** to be filed. This answer also closes the "PID cadence" open question in ADR-197
(`docs/adr/ADR-197-sat-pid-v3-controller-supersedes-adr-069.md:164`) and Q10 in
`docs/sat/open-questions.md`.

**Open:**

- Whether the per-area/zone PIDs also move to the 60 s timer. We read "follow thermo-nova" as yes;
  he did not say so explicitly.
- Q25 (the firmware-only ±20 integral cap and the 10 s minimum between PID updates). With a fixed
  60 s cadence the 10 s gate can no longer trigger, but whether to remove it is his decision under Q25.
- Whether the user setting "control interval" (10-300 s) should stay for the rest of the control
  loop. This answer does not cover it. Q13 (5 s CS/MM/CH refresh) is related but separate.

## 43. The first PWM phase is the ON phase (2026-10-24)

(Number: use the next free number in the file; it currently ends at 35.)

**Source:** #alpha-testing, 2026-10-24, answer to Q11 of our overview of 20:58 UTC (messages
1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1563662623117938851

**Quoted:**

> Q11: First PWM phase is the ON phase.

**Interpretation (ours):** the answer is short. We read it as answer A of Q11: when PWM starts, SAT
starts the ON phase at once. It does not first wait an off time measured from the last flame-off.
He did not mention any conditions on that first ON phase. thermo-nova has conditions (on time
>= 180 s, the hourly cycle check, see below), but those are covered by other questions (audit #19,
entry 31, Q12) and are not settled by this answer. This reading is ours.

**Settled by this statement:**

- When PWM starts, the first phase is ON, with no initial OFF wait. This applies after a switch from
  continuous mode, both the sustained-overshoot switch and the end-of-cycle switch into PWM.
- The firmware's wait in the OFF phase, timed from the last flame-off, is not the design. Audit #17
  noted that nothing in the earlier entries approved this wait. That is now decided against it.

**Compared with thermo-nova and the firmware:**

- thermo-nova: `reset()` puts PWM in `PWMStatus.IDLE` (`pwm.py:87-100`, status at `:92`).
  `disable()` calls `reset()` (`pwm.py:106-109`). `enable()` only sets `_enabled` and keeps the IDLE
  status (`pwm.py:102-104`). It is called from the sustained-overshoot switch
  (`heating_control.py:304`) and from `on_cycle_end` after a continuous cycle classified OVERSHOOT
  (`pwm.py:219-223`). PWM is disabled when the heater goes off (`heating_control.py:230`) and when the
  requested setpoint reaches the maximum (`heating_control.py:204`). In `update()`, OFF or IDLE moves
  to ON when `on_time_seconds >= HEATER_STARTUP_TIMEFRAME` and
  `(status == IDLE or elapsed >= off_time_seconds)` (`pwm.py:154-155`); IDLE skips the off-time wait.
  `HEATER_STARTUP_TIMEFRAME` is 180 (`const.py:18`). The hourly cycle check comes first
  (`pwm.py:156-158`) and returns with the status still IDLE. The ON start counts the cycle, sets ON,
  and either sets the flame-on anchor or waits for ignition (`pwm.py:160-167`).
- thermo-nova's skip of the wait is limited to the IDLE status. If the first tick does not start ON
  (for example on time below 180 s), IDLE becomes OFF on that tick (`pwm.py:179-180`). From then on,
  an ON start waits until `elapsed >= off_time_seconds`, where `elapsed` counts from `_last_update`,
  last set at `reset()` (`pwm.py:91`) or at `:124`, not from flame-off. A missing flow temperature
  sets IDLE again (`pwm.py:123`).
- Firmware: the firmware has no IDLE state. `satPwmReset()` clears `bPwmFlameRequested`, so PWM
  starts in the OFF phase (`src/OTGW-firmware/SATcontrol.ino:1071-1080`). A change of control mode
  calls it (`SATcontrol.ino:1083-1087`). The OFF branch starts ON only when
  `millis() - satCycleGetFlameOffStartMs() >= offTimeMs` (`SATcontrol.ino:964-968`), which is a wait
  timed from the last flame-off. The ON start is at `SATcontrol.ino:982-994`. There is one exception:
  in duty range 5 (`duty >= dutyMax`), the firmware sets `bPwmFlameRequested` at once
  (`SATcontrol.ino:905-910`), so the initial OFF wait affects ranges 1-4. The comment at
  `SATcontrol.ino:1068` says "Start every PWM stint idle (thermo-nova pwm.enable())", but the code
  starts in OFF with a wait, so that comment describes the intent, not the behaviour.
- The firmware line numbers in the audit (#17: `SATcontrol.ino:957-961`, `1064-1080`) have drifted.
  The locations above are the ones opened in this session.
- The firmware's first-ON conditions differ from thermo-nova's, and this entry does not settle them.
  The firmware has no "on time >= 180 s" condition on an ON start (audit #19, related to entry 31).
  Its hourly limit (`satCycleIsHourLimitReached()`, `SATcontrol.ino:971`) counts every flame-on,
  hot-water ignitions included, while thermo-nova's check (`pwm.py:156`) never takes effect. That is
  Q12 (audit #18).

**Changes an earlier entry:** none. It is consistent with entry 8 (the duty cycle follows pwm.py),
entries 15 and 18 (end-of-cycle switch into PWM, thermo-nova's on_cycle_end), entry 22 ("SAT enters
in the PWM mode. ( CS = BT - 1 )", meaning PWM is entered with the flame kept on) and entry 31
(180 s minimum flame time, low-duty case). Entries 5, 9 and 17 (OFF phase CS=10 with CH=0) describe
what the dropped initial OFF wait would have sent. Those entries describe when PWM starts and what
each phase sends. This entry settles which phase comes first.

**Task:** to be filed.

**Open:**

- Whether the 180 s minimum on time gates the first ON phase (and every ON start) in the port. In
  thermo-nova it does (`pwm.py:155`). He did not say; see audit #19 and entry 31.
- What happens when the first tick does not start ON. thermo-nova falls to OFF and then waits
  `off_time_seconds` from `_last_update` (`pwm.py:179-180`). Not asked.
- Audit #17 notes that the change adds burner starts compared with the firmware today. The TASK-1331
  work (uncommitted, per the audit) would make the OFF wait the normal path after every overshoot
  cycle, so it must follow this entry.
- The hourly start cap (Q12, audit #18) is still open.

## 44. CS, MM and CH go out on every control update, as thermo-nova (2026-10-24)

**Source:** #alpha-testing, 2026-10-24, his answer to our overview of all open SAT questions (20:53 UTC, messages 1563656700118958262 and 1563656702086221987), item Q13:
https://discord.com/channels/812969634638725140/1514720723980259460/1563663056087556187

**Quoted:**

> Q13: Follow thermo-nova.

**Interpretation (ours):** Q13 (`docs/sat/open-questions.md`, row Q13) asked: "CS/MM/CH refresh: every 5 s as thermo-nova, or on change + 300 s?" (audit #24). We read "Follow thermo-nova" as the first option. CS=, MM= and CH= are all sent on every run of the heating control loop, including unchanged values. The firmware's write-on-change with a 300 s refresh is dropped for these three commands. His answer covers CS/MM/CH only. TC= is not part of it (see "Changes an earlier entry").

**Settled by this statement:**

- **What thermo-nova does, in time order:**
  - `_register_event_listeners` starts a timer that calls `schedule_heating_control_loop` every 5 s (`climate.py:706-710`, `timedelta(seconds=5)`).
  - `schedule_heating_control_loop` is debounced. If no run is pending, it schedules `async_control_heating_loop` 1 s later (`climate.py:526-530`).
  - The same scheduler also runs at start-up (`climate.py:138`, `:144`), after an HVAC mode change (`climate.py:601`) and after a target temperature change (`climate.py:664`).
  - `async_control_heating_loop` returns without sending anything when the outside temperature is unknown or HVAC mode is OFF (`climate.py:544-545`). Otherwise it calls `HeatingControl.update` (`climate.py:547-555`).
  - At the end of `update`, thermo-nova always sends the control setpoint, the max relative modulation and the heater state, whichever branch ran (`heating_control.py:236-238`). This includes the heater-OFF branch, which sets `MINIMUM_SETPOINT` and the configured maximum modulation (`heating_control.py:229-234`).
  - None of these is checked for a change first. The MQTT OTGW coordinator publishes `CS={value}` (followed by `PM=25`), `MM={value}` and `CH={1|0}` on every call (`coordinator/mqtt/opentherm.py:195-197`, `:216-220`, `:211-212`). `_publish_command` has no deduplication (`coordinator/mqtt/__init__.py:134-147`).
  - The PID runs on its own 60 s timer (`climate.py:712-716`, `PID_UPDATE_INTERVAL = 60` in `pid.py:20`). So the commands repeat about every 5 s, while the PID output they are based on changes at most once a minute.
- The firmware moves to this cadence for CS, MM and CH. ADR-199 (still Proposed) must be updated: its decision 4, "CH cadence (kept)", still records CH on change plus every 300 s, and its consequences and checklist list Q13 as open.

**Compared with thermo-nova and the firmware:**

- **Firmware today (write-on-change):**
  - `satEnqueueIfChangedCS`, `satEnqueueIfChangedMM` and `satEnqueueIfChangedCH` send only when the value changed, or when `SAT_CMD_REFRESH_MS` = 300000 ms (5 min) has passed since the last send (`src/OTGW-firmware/SATcontrol.ino:421-427`, `:465-478`, `:498-509`, `:512-521`; TASK-565).
  - They are called from the control tick `timerSATControl` (`SATcontrol.ino:582`, `:4845`). Its interval is `settings.sat.iControlInterval`: default 30 s (`SATtypes.h:467`), range 10-300 s (`settingStuff.ino:1004`).
  - Call sites in the tick: the main block (`SATcontrol.ino:5219-5225`), the thermal safe fallback (`:4969-4974`), summer simmer (`:4997-5004`) and valves closed (`:5010-5018`).
- **Two firmware refresh paths between control ticks:**
  - `satPicCSKeepalive` resends the last CS every 30 s on PIC boards, because the PIC drops a CS= after about 63-126 s without a new one (`SATcontrol.ino:480-496`, TASK-1236). It exists because SAT's own cadence is slow (`:482`). With CS every 5 s it may become redundant. That is our call.
  - `satPwmSuppressionRefresh` updates the suppression CS every 3 s during PWM ON (`SATcontrol.ino:1043-1066`). It does not exist only because of the slow cadence. It implements OpenTherm application text 2.1.1.2.5, "updated every 2-3 s" (`:1043-1045`), and Sergeant D approved "CS refresh every ~3 s" for TASK-1268 (entry 23). A 5 s cadence is slower than 3 s, so this answer does not make the refresh redundant. Removing it would go against entry 23 and must go to him.
- **Not decided by this answer: does "every 5 s" mean a separate 5 s command timer, or the firmware's control tick?**
  - In thermo-nova the 5 s loop recomputes the control setpoint from the latest PID output, and the PID runs every 60 s.
  - In the firmware, PID and command output share one tick of 10-300 s.
  - This is a design point for ADR-199 (or its own ADR). It is not settled here.
- **Out of scope for this question:**
  - thermo-nova also sends `PM=25` after every `CS=` (`coordinator/mqtt/opentherm.py:197`). The question did not cover it.
  - thermo-nova sends `TP=11:12={min(value, 80)}` before every `MM=` for Immergas, with the modulation value (`coordinator/mqtt/opentherm.py:217-218`). The firmware sends it on every control tick with the setpoint as its value (`SATcontrol.ino:5226-5232`).
  - Neither is settled by this answer.

**Changes an earlier entry:**

- **Entry 7:** its last bullet recorded that "follow the thermo-nova exactly" also covers sending CH after every control step (`heating_control.py:238`). The firmware kept CH on change plus every 5 minutes "without an explicit OK". This answer settles that point the thermo-nova way. CH now goes out on every update, as CS and MM do.
- **Entries 14 and 16 are unchanged.** TC= stays event-driven: pushed on a target change, with no periodic re-push (`satEnqueueIfChangedTC`, TASK-1272). This answer is about CS/MM/CH only.
- **Entry 23 is unchanged.** The 3 s CS refresh during PWM ON that he approved for TASK-1268 stays. This answer sets a 5 s floor for every command and does not replace that faster refresh.

**Task:** to be filed.

**Open:**

- Whether the 5 s cadence becomes its own timer or replaces the control tick. This is a design choice for ADR-199. If it changes SAT control behaviour, put it to him.
- Whether `PM=25` with every CS should be ported. Not asked.
- Whether the PIC CS keepalive (TASK-1236) is still needed once CS goes out every 5 s. That is our call. The 3 s suppression refresh stays, per entry 23, unless he says otherwise.
- The effect on the OT command queue and PIC traffic: about 3 commands every 5 s, against roughly 20x less today (the TASK-565 rationale, `SATcontrol.ino:421-425`). This needs a bench check before the change ships.

## 45. PWM ignition without a return temperature: skip the flame-off hold, flood CS and PM=25 for 20 s, MI=500 (2026-10-24)

**Source:** #alpha-testing, 2026-10-24 21:34 UTC, answer to Q14 of our overview of all open SAT
questions (20:53 UTC, messages 1563656700118958262 and 1563656702086221987; Q14: "PWM ignition hold
when the boiler has no return temperature sensor?", audit finding #27):
https://discord.com/channels/812969634638725140/1514720723980259460/1563667007692603523
With an attached screenshot of the OTGW firmware page, section `MI=milliseconds`
(https://otgw.tclcode.com/firmware.html#configuration).

**Quoted:**

> Q14: Great question. While return water temperature gives us stability. Boiler flow water
> temperature gives us responsiveness. That means we can skip the start up phase ( 20 seconds of
> Return temperature +20 ) and go directly to suppression phase. In order to achieve a light off and
> not a stalled ignition we need to flood the boiler with frequent CS updates during the ignition. We
> need to repeat PM=25 commands and CS commands. We need a new pattern of the commands for the first
> 20 seconds.
> Every 2 seconds we are sending the new CS commands and every 2 seconds + 1 second we ask for the
> boiler temperature ( PM=25). So we alternate the CS and PM=25 commands every second for the first 20
> seconds ( ignition ). Also we need to send the MI=500 command to the PIC for boilers that do not
> report return water temperature. I attach a screenshot from OTGW website where describes how the MI
> command works.

**Interpretation (ours):** this is a new design for boilers without a return water temperature, not
the thermo-nova behaviour. In the PWM ON phase, when no return temperature is available:

1. Skip the flame-off hold (the "start up phase": the return temperature plus the flame-off offset,
   held for the suppression delay) and go straight to the suppression phase: CS = boiler flow
   temperature minus the suppression offset.
2. During the first 20 s of the ignition, alternate one command per second: a new CS at t = 0, 2, 4 …
   and `PM=25` (ask the boiler flow temperature, MsgID 25) at t = 1, 3, 5 …, so the CS follows the
   rising flow temperature closely and the boiler lights off instead of stalling.
3. Send `MI=500` to the PIC for boilers that do not report a return water temperature.

We read "Return temperature +20" as his short form of the hold; thermo-nova's default flame-off
offset is 18.0 °C (`const.py:150`) and its default suppression delay is 20 s (`const.py:151`).

**Settled by this statement:**

- Without a return temperature, the PWM ON phase uses flow-based suppression from the start of the
  ignition; no flame-off hold.
- During the first 20 s of ignition the firmware sends CS and `PM=25` alternately, one per second
  (CS every 2 s, `PM=25` every 2 s offset by 1 s).
- `MI=500` is sent to the PIC for boilers that do not report a return temperature.

**Compared with thermo-nova and the firmware:**

- thermo-nova (`heating_control.py:392-395`): with the flame off and no return temperature it logs a
  warning and returns, so the previous control setpoint stays; there is no flow-based start and no
  command flood. With a return temperature it holds `return + 18.0` (clamped,
  `heating_control.py:397-405`) for 20 s after flame-on (`:414-433`), then suppresses to
  `flow - 1.0` (`:439-448`). So this entry is a deliberate deviation from thermo-nova, decided by
  Sergeant D.
- Firmware today: `satPwmFlameOffHold()` (`SATcontrol.ino:808-814`) uses `satGetReturnTemp()` plus
  `fFlameOffOffset`, floored at the cold setpoint, also when there is no return temperature, so a
  boiler without Tret gets the 28.2 / 21 floor with CH=1 (audit finding #27). `MI=500` is sent only
  at `initSAT()` as a manufacturer boot quirk (`SATcontrol.ino:3950-3953`), and audit finding #45
  found it is effectively never sent (manufacturer AUTO, detection not yet run). No `PM=25` is sent
  by SAT (audit finding #53).
- The OTGW documentation in his screenshot says `MI` sets the message interval "in stand-alone
  mode" (the PIC talking to the boiler without a thermostat), valid 100-1275 ms. Whether `MI=500`
  has any effect with a thermostat connected (gateway mode) is not stated there and not checked.

**Changes an earlier entry:** none. It answers Q14 and gives the reason for `MI=500` that audit
finding #45 asked for.

**Task:** TASK-1371.

**Open:**

- How "no return temperature" is detected (never reported, Data-Invalid, or stale) is not stated.
- Whether `MI=500` matters in gateway mode with a thermostat (the documentation describes
  stand-alone mode) and whether it should be reset (`MI=1000`) later.
- On OT-Direct boards there is no PIC: `PM=25` and `MI` map to the board's own polling; how the
  flood is done there is our design choice, to be shown to him in the plan.
- Whether the flood also applies to boilers that do report a return temperature: his text ties it
  to the case without one.

## 46. Room temperature lost or missing: reset P, I and D to 0 (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 03:56 UTC, answer to Q29 (2026-10-24 22:01 UTC, message
1563673817824628837: "while the room temperature is missing, what happens to I and D? A. Reset I and
D to 0 [...] B. Freeze I and D and leave them out of the setpoint [...]"):
https://discord.com/channels/812969634638725140/1514720723980259460/1563763199051038791

**Quoted:**

> @BlueFlame Bot yes reset P, I and D terms to 0

**Interpretation (ours):** option A, and P is named too: while the room temperature is lost or
missing, the P, I and D terms are 0, so the control setpoint is the heating curve only (entry 38).
"Reset" means the stored integral and derivative are set to 0, not frozen and brought back after
recovery. After recovery the PID starts from 0 and builds up again from the new error.

**Settled by this statement:**

- During a lost or missing room temperature: P = 0, I = 0, D = 0; requested setpoint = heating curve
  (plus the solar offset and the usual clamps, as in the TASK-1364 plan).
- The integral and the derivative are reset, not frozen.

**Compared with thermo-nova and the firmware:**

- thermo-nova on a stale inside sensor: `pid.reset()` (`climate.py:470-478`), which zeroes only
  `_integral` and `_raw_derivative` (`pid.py:134-137`); P keeps `kp * _last_error`
  (`pid.py:96-101`), so its output is curve + frozen P. On an unavailable sensor it returns without a
  reset (`climate.py:480-482`). His answer goes further than thermo-nova on P (entry 38 already
  said "not proportional"), and follows thermo-nova's stale path for I and D.
- Firmware today: no curve-only path; after about 10 invalid control ticks SAT trips and
  `satDisable()` resets the PID (`SATcontrol.ino:4900-4914`, `:2373-2386`).

**Changes an earlier entry:** completes entry 38 (it named only P) and answers its open point on I
and D.

**Task:** TASK-1364 (plan already stored; this answer is his OK on the open point).

**Open:** the stale time per source (entry 27, conflict C8) and whether the error statistics and the
published error should read 0 or "unknown" during the loss.

## 47. Lost OpenTherm: hand the boiler back like a user switch-off (CS=0, CH=1, MM=T), then send nothing (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 04:33 UTC, answer to Q30 (04:31 UTC, message
1563771986558193705: which final message should SAT send when OpenTherm is lost? A) CH=0 then CS=0;
B) CS=0, CH=1, MM=T (the same hand-back as a user switch-off); C) CH=0, CS=0, MM=T; our advice B):
https://discord.com/channels/812969634638725140/1514720723980259460/1563772498586378281

**Quoted:**

> @BlueFlame Bot I agree option B is better

**Settled by this statement:**

- When SAT finds no valid OpenTherm, it stops and sends, once, the same hand-back as a user
  switch-off: `CS=0`, `CH=1`, `MM=T` (`satDisable()`, TASK-1346 and TASK-1344), and then sends
  nothing more to the boiler.
- This replaces the literal `CH=0, CS=0` of entry 39 for the final message, because the gateway
  applies `CH=` only while a CS override is active and a stored `CH=0` would block an on/off
  thermostat on a PIC gateway (`gateway.asm:2719-2723`); the thermostat stays the backup (entry 26).

**Compared with thermo-nova and the firmware:** thermo-nova has no stop on a lost link; on shutdown it
sends `CS=0` and clears MM without CH (`coordinator/serial/__init__.py:228-231`). Firmware today: see
the TASK-1365 plan (no dedicated OT-loss stop).

**Changes an earlier entry:** entry 39 (the final message only; "SAT stops and then sends nothing"
stays).

**Task:** TASK-1365.

**Open:** what counts as "no valid OpenTherm" (the plan proposes no boiler reply for 30 s) and
whether SAT resumes by itself when OpenTherm returns (the plan recommends automatic resume); both
are in the TASK-1365 plan and not yet confirmed by him.

## 48. A user MaxTSet slider, bounded by MsgID 57, sent to the boiler (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 05:56 and 06:10 UTC, a new development he asked for (it also
answers Q15, "Maximum flow setpoint: a user setting? Which defaults?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1563793351961223221
https://discord.com/channels/812969634638725140/1514720723980259460/1563796961771913227

**Quoted:**

> @BlueFlame Bot NEW DEVELOPMENT:
> Create a MaxTset slider. The limit values should be  the MsgID 57 values ( Lower/Higher ). Then pass this value to the boiler. If the boiler doesn’t report the MsgID 57, then use as default lower/upper limit values the 20/80

> This development should be identical to the DHW Setpoint slider.

**Interpretation (ours):** a user setting "maximum CH setpoint" with a slider whose range is the
boiler's MsgID 57 bounds (MaxTSet lower/upper), or 20..80 °C when the boiler does not report them;
the chosen value is written to the boiler (MsgID 57; `SH=` on a PIC gateway) and SAT uses it as its
ceiling. "Identical to the DHW Setpoint slider" means the same pattern as the DHW setpoint (bounds
from MsgID 48, value sent to the boiler; entry 33).

**Settled by this statement:**

- Q15: the maximum flow setpoint becomes a user setting (a slider), bounded by MsgID 57 or 20..80.
- The value is passed to the boiler, not only used inside SAT.

**Compared with thermo-nova and the firmware:** thermo-nova has a user `maximum_setpoint` (range
10..100, `config_flow.py:612-613`) used as SAT's ceiling. Firmware today: a fixed cap per system
(62 / 45 / 40, `SATcontrol.ino` around 293-327, audit #29) combined with the boiler's MaxTSet
(`SATcontrol.ino:316`); no user setting and nothing written to the boiler.

**Changes an earlier entry:** none.

**Task:** TASK-1373.

**Open:** the default value when nothing is chosen (keep 62/45/40 per system, or the boiler's own
MaxTSet); whether writing MsgID 57 is safe on boilers that reject the write.

## 49. Fireplace mode: a switch that makes SAT heat on the heating curve only (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 06:18 UTC, a new development he asked for:
https://discord.com/channels/812969634638725140/1514720723980259460/1563799007157817405

**Quoted:**

> @BlueFlame Bot New development II:
> Create a switch called “Fireplace mode”
>
> When this switch is enabled SAT will send only the heating curve value to the control loop and not the PID. The behavior of SAT will be identical to the missing indoor temperature senso we discussed earlier.
>
> Some homes have a stove or an AC heating their primary room which most often the indoor temperature sensor is placed. When they are heating this specific room, SAT stops the heating completely because the setpoint is reached while the other rooms are still cold. This new development will let SAT continue heating the home regardless of the main room reached the setpoint or not.

**Settled by this statement:**

- A user switch "Fireplace mode" (default off). While on, SAT behaves exactly as with a missing room
  temperature (entries 38 and 46): the setpoint is the heating curve only, with P, I and D at 0.
- Purpose: a stove or air conditioner heats the room with the sensor; the rest of the house must
  keep getting heat.

**Compared with thermo-nova and the firmware:** not found in thermo-nova (grep for fireplace/stove in
`custom_components/sat`: nothing). Firmware: the curve-only path exists since TASK-1364
(`satPidCurveOnly`, alpha.500); the switch reuses it.

**Changes an earlier entry:** none (it reuses entries 38 and 46).

**Task:** TASK-1374.

**Open:** whether the room temperature is still shown and recorded while the switch is on (we will
keep showing it), and whether the switch should end by itself (for example at the next morning).

## 50. Heat pumps in PWM: minimum ON 180 s, cycle length 1800 s (default) or 2400 s (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 06:36 UTC, answer to Q31 (05:31 UTC, message
1563787079249297559: A) minimum ON 180 s, cycle length 1800 s (2 per hour, default) or 2400 s (1.5
per hour); B) 2 or 3 cycles per hour as thermo-nova; C) something else; our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563803408262234155

**Quoted:**

> Question 31: Yes the correct option is A

**Settled by this statement:**

- Heat pumps in PWM use the same minimum ON time as gas boilers, 180 s (thermo-nova `pwm.py:53`).
- The heat pump PWM cycle length is 1800 s (2 per hour, default) or 2400 s (1.5 per hour), a user
  choice; the duty logic is thermo-nova's (`pwm.py:54`, cycle = 3600 / cycles per hour).
- It confirms our reading of entry 40 and settles conflict C4 for heat pumps: his 2 / 1.5 per hour,
  not thermo-nova's 2 / 3 (`config_flow.py:686-693`).

**Compared with the firmware:** today 1800 / 2400 s are used as the heat pump minimum ON time
(audit #9), which collapses the duty mapping; see the TASK-1366 plan.

**Changes an earlier entry:** confirms entry 40 (no longer an interpretation).

**Task:** TASK-1366.

**Open:** whether hybrids get the heat pump cycle lengths (the TASK-1366 plan lists it).

## 51. SAT caps the CS it sends at the user's MaxTSet from the SAT settings (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 05:49 UTC, his reply to gruijter's report (05:35 UTC, message
1563788093775941692: CS shows 50.2 at start-up while his Intergas caps the flow at 48 and does not
report MsgID 57):
https://discord.com/channels/812969634638725140/1514720723980259460/1563791707135287388

**Quoted:**

> @BlueFlame Bot SAT clamps the higher CS value that sends to the boiler with the MaxTset that is defined by the user in the SAT settings.

**Settled by this statement:**

- The ceiling of every CS= that SAT sends is the maximum setpoint the user sets in the SAT settings.
- Read together with entry 48 (seven minutes later): that user value is the MaxTSet slider, bounded
  by MsgID 57 or 20..80 °C, and it is also written to the boiler. A boiler without MsgID 57 support
  (gruijter's Intergas) is still capped, because SAT itself clamps CS.

**Compared with thermo-nova and the firmware:** thermo-nova clamps the setpoint at the user's
`maximum_setpoint` (config option, range 10..100, `config_flow.py:612-613`). Firmware at HEAD:
`satEffectiveMaxSetpoint()` (`SATcontrol.ino:314-327`) takes the boiler's MsgID 57 value (or
`SAT_MAX_SETPOINT_DEFAULT` below 30 °C), then the per-system cap (62 / 45 / 40,
`SATcontrol.ino:302-309`), the hard ceiling and `settings.sat.fMaxSetpoint` (default 65,
`SATtypes.h:537`). `fMaxSetpoint` has no settings key in `settingStuff.ino` (grep), so the user
cannot set it today.

**Changes an earlier entry:** none; it adds the clamp rule to entry 48.

**Task:** TASK-1373.

**Open:** none beyond entry 48.

## 52. PWM cycles per hour default: 3 for gas boilers, 2 for heat pumps (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 07:10 UTC, answer to Q16 (07:08 UTC, message
1563811397534613505: A) default 4, choices 3 or 4 (thermo-nova); B) default 3, choices 3 or 4;
C) keep default 3 and the 2..6 range; our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563811907708649533

**Quoted:**

> Keep  3x hour for gas boilers and 2x hour for heat pumps as default values

**Settled by this statement:**

- Gas boilers: default 3 cycles per hour (a 1200 s cycle). With entry 30 the choices are 3 or 4.
  This is option B, not our advice A.
- Heat pumps: default 2 per hour (1800 s), which confirms entry 50.

**Compared with thermo-nova and the firmware:** thermo-nova's default is 4 (`const.py:119`), with
3 or 4 for radiators and 2 or 3 for heat pumps (`config_flow.py:686-701`). Firmware at HEAD: default 3
(`SATtypes.h:548`), accepted range 2..6 (`settingStuff.ino:1107`).

**Changes an earlier entry:** none; it completes entry 30 (choices) with the default.

**Task:** TASK-1366 (heat pump default) and a new task for the gas boiler range (3 or 4 only).

**Interpretation (ours):** he did not say whether the 2..6 range must shrink to 3 or 4. Entry 30
says gas boilers use 3 or 4, so we read the range as 3 or 4 for gas boilers.

**Open:** what hybrids default to.

## 53. Hot water setpoint: send SW= at once on every change (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 07:28 UTC, answer to Q17 (07:23 UTC, message
1563815326800552037: A) send SW= at once on every change and once after a restart, while DHW control
is on; B) keep sending it only when a hot water draw starts; C) something else; our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563816490799472681

**Quoted:**

> @BlueFlame Bot Exactly, option A is the correct. Normally users do not change the DHW setpoint or the MaxTset. They set it probably once and then forget it.

**Settled by this statement:**

- A DHW setpoint change sends `SW={value}` at once, and once after a restart, while DHW control is on.
  This confirms entry 33 ("When a DHW setpoint temperature change occurs you need to send the SW={value}
  command").
- Context from him: users set the DHW setpoint and the MaxTSet once and then leave them.

**Compared with thermo-nova and the firmware:** thermo-nova sends the setpoint at once on change
(`number.py:72-74`). Firmware at HEAD `31933adaf`: `SW=` only on the off-to-on edge of a hot water draw
and only with `bDhwEnabled` (`SATcontrol.ino:4885-4891` before TASK-1365 moved the lines).

**Changes an earlier entry:** none; it confirms entry 33.

**Task:** TASK-1376.

**Open:** whether the draw-edge `SW=` stays as well (not asked; the plan decides and names it).

## 54. SAT knows the user's MaxTSet and never sends a higher CS (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 07:31 UTC, his reply after our answer to gruijter (message
1563816425838219335, which said SAT has no user MaxTSet setting yet and asked for an OT log):
https://discord.com/channels/812969634638725140/1514720723980259460/1563817156884430930

**Quoted:**

> @BlueFlame Bot SAT should know this actually. So when the MaxTset slider is set to 48, then SAT knows that the highest value can send to the boiler is 48. It is also in the thermo-nova

**Settled by this statement:** the same rule as entry 51, restated: with the MaxTSet slider at 48, SAT
never sends a CS above 48. It applies to every CS, the ignition hold included.

**Compared with thermo-nova and the firmware:** thermo-nova clamps the PID output and the flame-off
hold at the user `maximum_setpoint` (`climate.py:379`, `:394`; `heating_control.py:397`). Firmware at
HEAD: no user setting (entry 51); the ignition hold is clamped with the system cap only
(`satGetMaxSetpoint()`, noted on TASK-1373).

**Changes an earlier entry:** none; it confirms entry 51.

**Task:** TASK-1373 (the clamp must also cover the ignition hold).

**Open:** none.

## 55. End-of-cycle classifier: match thermo-nova's classifier.py exactly (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 10:39 UTC, answer to Q4 (asked again 10:35 UTC, message
1563863552094240852; detail 2026-10-24 21:31 UTC, message 1563666347832119430: A) match classifier.py
exactly, B) keep "short" only as a display label, C) keep ours; our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563864684971696211

**Quoted:**

> @BlueFlame Bot Q4: Choose A

**Settled by this statement:**

- The firmware classifier follows `cycles/classifier.py` exactly: no SHORT label for a continuous
  cycle, no rule that a cycle under 120 s is always UNCERTAIN.
- Add classifier.py's UNCERTAIN results: kind UNKNOWN (`classifier.py:22-23`), missing PWM tail
  setpoint-error percentiles (`:42-43`), missing continuous tail control-setpoint-error percentiles
  (`:71-72`).

**Compared with the firmware:** the extra SHORT and under-120-s rules and the missing UNCERTAIN cases
are listed in the Q4 detail message; the label now switches PWM/continuous at cycle end (Q2 answer),
so it must follow classifier.py.

**Changes an earlier entry:** none.

**Task:** TASK-1387 (prerequisite for TASK-1331).

**Open:** none.

## 56. Outside temperature missing or old: stop SAT at once (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 12:37 UTC, answer to Q7 (asked again 12:34 UTC, message
1563893593339338803: A) keep the last value for a few hours, then stop SAT as for lost OT; B) stop
SAT at once; C) something else; our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563894284694982677

**Quoted:**

> Q7: I prefer option B.

**Settled by this statement:**

- When the outside temperature is missing or old, SAT stops at once; it does not keep the last
  value. This also closes the Q7 clarification of 2026-10-24 (his entry 39 rule was about lost
  OpenTherm; this is the outside-temperature rule).

**Interpretation (ours, asked as Q7b):** "stop" = the same hand-back as lost OpenTherm (entry 47:
CS=0, CH=1, MM=T once, then silent) and SAT resumes when a fresh value returns. What counts as "old"
is not settled.

**Compared with thermo-nova and the firmware:** thermo-nova returns early from the control loop when
the outside temperature is None (climate.py:544-545), so it stops sending, without a hand-back.
Firmware: `satGetOutsideTemp()` (`SATcontrol.ino:1495-1515`) falls back external (10 min stale,
`:142`, `:1503`) -> weather (`bValid` is never reset, so a failed fetch keeps an old value) -> OT
MsgID 27 (0.0 without a sensor): with no source SAT silently heats on a curve for 0 °C.

**Changes an earlier entry:** none.

**Task:** TASK-1390.

**Open:** the age threshold and the hand-back form (Q7b).

## 57. Outside temperature counts as old after one hour (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 13:19 UTC, answer to Q7b (13:18 UTC, message 1563904467403800678:
A) old = no fresh value from any source for 1 hour, then the lost-OpenTherm hand-back (CS=0, CH=1, MM=T
once) and resume by itself when a fresh value arrives; B) the same with 3 hours; C) something else;
our advice A):
https://discord.com/channels/812969634638725140/1514720723980259460/1563904963237642251

**Quoted:**

> Q7b: one hour is fine

**Settled by this statement:**

- The outside temperature is "old" when no source (external sensor, weather service, boiler MsgID 27)
  has delivered a fresh value for one hour.

**Interpretation (ours):** he chose the 1-hour option A, so the rest of option A applies too: on
expiry SAT does the lost-OpenTherm hand-back once (entry 47) and resumes by itself when a fresh value
arrives. He did not comment on the hand-back form separately.

**Compared with thermo-nova and the firmware:** thermo-nova stops sending when the outside
temperature is None (`climate.py:544-545`), without an age rule. Firmware: see entry 56.

**Changes an earlier entry:** completes entry 56.

**Task:** TASK-1390.

**Open:** none.

## 58. thermo-nova "pauses PWM counting" while hot water runs (statement, 2026-10-25)

**Source:** #alpha-testing, 2026-10-25 18:46 UTC, unprompted, in the Q12 thread (hot-water starts and the
hourly start limit; Q12 asked again 17:23 UTC, message 1563966057784418397):
https://discord.com/channels/812969634638725140/1514720723980259460/1563987251912904705

**Quoted:**

> @BlueFlame Bot thermo-nova pauses  PWM counting when the DHW is enabled. When DHW enable is off resumes PWM counting. Check thoroughly the thermo-nova branch about how it works

**What the thermo-nova code shows (opened this session, pin 7beaaf5):**

- The hourly counter counts only PWM's own ON-phase starts: `_current_cycle += 1` when an ON phase
  starts (`pwm.py:159-160`); boiler flame starts are never counted, so hot-water ignitions never count.
- There is no explicit pause: `pwm.update()` keeps running while hot water is active
  (`heating_control.py:211`; no hot-water gate in `pwm.py:117-140`). What hot water stops is the PWM
  setpoint override (`heating_control.py:385`) and the PWM disable guards (`:313-315`).
- The limit check compares the counter with `duty_cycle_seconds` (`pwm.py:156`), so in practice it
  never blocks.

**Interpretation (ours, confirmation asked):** his intent matches Q12 option A: hot-water starts must
not count against the hourly limit, only PWM's own ON phases do. The literal "pause" is not in the
code; the effect (hot water never counts) is. Reported to him with the citations.

**Compared with the firmware:** the firmware counts every flame-on edge in the rolling hour, hot water
included, and blocks a new PWM ON phase at the cycles-per-hour limit (SATcycles.ino:279-299; gruijter's
field data in Q12).

**Changes an earlier entry:** none.

**Task:** after his confirmation of Q12 (a task for "count only PWM ON phases").

**Open:** his confirmation that Q12 = A.

## 59. Hourly start limit counts only PWM's own ON phases (2026-10-25)

**Source:** #alpha-testing, 2026-10-25 19:52 UTC, answer to Q12 (confirmation asked 19:50 UTC, message
1564003236094935102: "Q12 = A (count only PWM's own ON phases against cycles per hour)?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564003846978670623

**Quoted:**

> Q12: yes count only PWM’s own ON phases against cycles per hour

**Settled by this statement:**

- The hourly start limit in PWM counts only PWM's own ON-phase starts, against the cycles-per-hour
  setting. Hot-water and relight flame starts do not count. Confirms the intent of entry 58.

**Compared with thermo-nova and the firmware:** thermo-nova counts ON-phase starts only
(`pwm.py:159-160`) in a rolling hour (`:138-141`), but its check compares with `duty_cycle_seconds`
(`:156`) and so never blocks; entry 59 asks for the cycles-per-hour limit. Firmware counts every
flame-on edge (SATcycles.ino:279-299), hot water included (gruijter's field data in Q12).

**Changes an earlier entry:** settles entry 58 (Open: confirmation).

**Task:** TASK-1401.

**Open:** none.

## 60. "CH=1 stays during the PWM OFF phase" (report, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 13:29 UTC, reacting to gruijter's alpha.513 graphs:
https://discord.com/channels/812969634638725140/1514720723980259460/1564269695257935976

**Quoted:**

> @BlueFlame Bot I noticed that the CH=1 stays during the PWM OFF phase. check again your documentation and please resolve this bug.

**Checked against the code and the capture (not yet settled):** thermo-nova sends CS=10 in the PWM OFF phase
(`heating_control.py:385`, `:388-389`, `const.py:35`) and then the heater state, ON only when the control setpoint is
above 22 (`heating_control.py:236-238`, `const.py:34`), so CH=0. The firmware does the same: `satApplyPWM` returns
SAT_MIN_SETPOINT in OFF (SATcontrol.ino:1042-1047) and `satChEnableDecide` returns 0 in the OFF phase
(SATchEnable.h:46-51), both queued in one tick (SATcontrol.ino:5381-5385). Entries 5, 9 and 17 say the same (entry 17:
"When we are in PWM OFF m, CS=10 CH=0"). gruijter's capture (telnet.log, local time) shows CS=10.0 and CH=0 queued and
sent at 12:06:55 and every later MsgID 0 request to the boiler with CH enable = 0 (R80000200), the thermostat's own CH=1
rewritten to CH=0 at 12:10:58. What stays 1 is the boiler's own slave status bit 1 "CH mode" (OT spec v4.2 line 1444,
master CH enable is line 1432): BC0000202 from 12:07:03 to 12:16:59 with the flame off, likely pump overrun (not proven).
The OTGW graph's "CH mode" lane is that boiler bit (v2.js:480).

**Interpretation (ours):** he read the boiler's CH-mode bit in the graph as SAT's CH enable. Not acted on: put back to
him with the frames (2026-10-26) before any change.

**Task:** none until he answers.

**Open:** whether he means SAT's CH enable (then which frame) or wants the boiler's CH-mode bit handled.

## 61. Add PWM ON and OFF times in seconds to the SAT log (request, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 13:35 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564271354390257867

**Quoted:**

> @BlueFlame Bot Also in the SAT log add PWM ON and PWM OFF times in seconds

**Settled:** logging only, no control change. **Task:** TASK-1407. **Open:** none.

## 62. Coefficient 2.2 for gruijter, why, and the morning rise time (statements, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 12:54-13:31 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564260826364248125 (disable solar gain), https://discord.com/channels/812969634638725140/1514720723980259460/1564261109333098577 (coefficient 2.2),
https://discord.com/channels/812969634638725140/1514720723980259460/1564269127013503097 (why 2.2), https://discord.com/channels/812969634638725140/1514720723980259460/1564270140957130834 (solar gain hypothesis)

**Quoted:**

> Can you disable solar gain?

> @gruijter also can you raise your heating curve coefficient to 2.2?

> Actually it is not that big. SAT’s each increment equals to 1/4 of a normal heating curve. In reality it is like raising the heating curve by 0.15 of a conventional heating curve. We made smaller increments for finer tuning. Another reason for setting it to 2.2 is that a lot of sat systems are tuned to that exact value. Moreover the morning heating schedule needs about 2 hours and 20 minutes to raise the room temperature to the new setpoint to most SAT users

> In the morning when sat started heating your home maybe the raw derivative was higher than 0.6 °C/h. it might triggered the solar gain.

**Settled:** tuning advice to one tester and field experience, not a change to defaults: the firmware default
coefficient stays 2.0 (entry 24, ADR-198). The 2 h 20 min figure is his experience, not in the code. His solar-gain
hypothesis is confirmed for the morning only: active 10:11:55-10:45:25 in gruijter's capture, without a sun position
(the firmware then judges on the rise rate alone, SATcontrol.ino:4616-4643; thermo-nova stays inactive without a sun
elevation, solar_gain.py:56-58) -> TASK-1408 (needs his OK before any change).

**Open:** whether the default coefficient should become 2.2 (not asked; queue if needed).

## 63. "Follow Alex's words" (process statement, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 19:20 UTC, after Alexw (alexw47) wrote at 19:17 that SAT uses the raw flow
temperature after the start-up filter and that our codebase may be outdated:
https://discord.com/channels/812969634638725140/1514720723980259460/1564358085181444097

**Quoted:**

> @BlueFlame Bot, @alexw47 is the developer of SAT. He owns the technical knowledge and he is responsible for the thermo-nova. You must follow his words.

**Not acted on:** this is about who the bot takes technical direction from, which is the maintainer's call. The
maintainer's rule (2026-10-25) makes Alexw a read-only member: nothing he writes leads to a code change, task or SAT
decision record. Put to Robert on 2026-10-26.

**Related technical point (checked, for Robert):** at the pinned thermo-nova commit 7beaaf5 the PWM duty uses the
learned ON temperature, which is only cleared at init and reset (pwm.py:49, :100) and otherwise held while the flame
is off (pwm.py:144-149, :232). Alex says newer code uses the raw flow after the start-up filter; if so the pin is behind
upstream. Moving the pin is a deliberate maintainer decision (CLAUDE.md SAT ground truth block).

**Open:** Robert's decision on (a) whether Alexw's technical statements may guide the bot, (b) whether to compare
upstream thermo-nova with the pin.

## 64. Alexw: "SAT uses the flow temperature while the flame is off" (technical statement, 2026-10-26)

**Status of the source:** Robert, 2026-10-26 (in session): "Geloof AlexW, hij is de ontwerper van de python variant."
Alexw (alexw47) wrote the thermo-nova Python code; his statements about thermo-nova behaviour outrank our reading of the
pinned copy (supersedes the read-only rule of 2026-10-25 for technical statements; entry 63).

**Source:** #alpha-testing, 2026-10-26 19:17 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564357301341655121

**Quoted:**

> That is incorrect. SAT does utilize the flow temperature while the flame is off. We apply a filter during startup, but we use the raw value thereafter. We use this to calculate the duty cycle. It is possible that you have an outdated codebase.

**Checked (2026-10-26, upstream GitHub API):** releases/thermo-nova HEAD is 7beaaf5 (2026-03-18), identical to the
local copy (pwm.py, heating_control.py, solar_gain.py byte-identical). develop is an ancestor of it (294 commits
behind, 0 ahead). At 7beaaf5 the duty uses _effective_on_temperature (pwm.py:232), updated only in the first 180 s after
ignition (pwm.py:144-149) and held after that. Alex's description matches develop: _last_boiler_temperature takes the
EMA during start-up and the raw flow after it while PWM is ON (develop pwm.py:123-131), used by _calculate_duty_cycle.

**Not acted on yet:** asked Alex (2026-10-26 19:23 UTC,
https://discord.com/channels/812969634638725140/1514720723980259460/1564358771931742271) which behaviour the port
should follow, develop's or a newer thermo-nova commit than 7beaaf5. Our 18:43 answer to Sergeant D (duty uses the held
learned ON temperature) is correct for 7beaaf5 and will be corrected if Alex says otherwise.

**Open:** Alex's answer; possibly moving the pin (CLAUDE.md SAT ground truth block).

**Maintainer decision (Robert, 2026-10-26, in session, after the doubt was put to him):** keep 7beaaf5. The firmware
follows the published thermo-nova code (learned ON temperature held after 180 s). Alex's statement stays an open point;
it is revisited if he names a newer thermo-nova commit.

## 65. Alexw: "when the flame is extinguished, we attempt to reignite it by increasing the flow by a factor of 10" (technical statement, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 19:30 UTC (Alexw, SAT Python developer; Robert's rule 2026-10-26: listen to him,
doubt goes to Robert):
https://discord.com/channels/812969634638725140/1514720723980259460/1564360503793287252

**Quoted:**

> Please note that when the flame is extinguished, we attempt to reignite it by increasing the flow by a factor of 10

**Checked (second reader):** this is in the develop branch: pwm.py:139-142 forces the setpoint to flow temperature + 10
when the flame is off and the average flame-on time is unknown or under 60 s ("Forcing setpoint up due to short flame
cycles"); read "factor of 10" as +10 C on the flow. At the pin 7beaaf5 (= upstream releases/thermo-nova HEAD) that code
is not there: with the flame off in the PWM ON phase heating_control.py:392-408 holds the setpoint at return
temperature + flame_off_setpoint_offset (default 18 C), then after ignition flow minus the suppression offset. The
firmware follows 7beaaf5 (no flow + 10 boost).

**Doubt put to Robert (2026-10-26):** same pattern as entry 64 (Alex describes develop, the pin differs).

**Maintainer decision (Robert, 2026-10-26, in session):** keep 7beaaf5 (no flow + 10 boost). Open point only.

## 66. Alexw: "updated in the newer version of SAT, in the branch you mentioned" (technical statement, 2026-10-26)

**Source:** #alpha-testing, 2026-10-26 19:26 UTC (Alexw, SAT Python developer), answering our question of 19:23 (which
behaviour the port should follow, develop's raw flow or a newer commit):
https://discord.com/channels/812969634638725140/1514720723980259460/1564359654958563339

**Quoted:**

> Correct, this has been updated in the newer version of SAT. In the branch you mentioned. I was not aware that you were using a beta version.

**Settles:** confirms that the PWM duty behaviour he described (raw flow after the start-up filter, entry 64) and the
reignition boost (entry 65) are in the **develop** branch, a newer version than the published thermo-nova release
(releases/thermo-nova HEAD 7beaaf5, 2026-03-18). The two entries describe a newer upstream, not a defect in our copy of
the pinned release.

**Interpretation (ours):** "the newer version" = Alexwijn/SAT develop (HEAD 7d4356034, 294 commits ahead of the pin;
the branch history shows the pin is an ancestor of develop was reported by the GitHub compare API as ahead 0 / behind
294 for the pin against develop).

**Relation to earlier entries:** entries 64 and 65 recorded Robert's decision "keep 7beaaf5". That was taken before this
confirmation; the facts it rested on are unchanged (the pin is the latest published release), but whether the port should
follow develop is a new, larger question for the maintainer.

**Open:** Robert's decision on (a) following develop for the PWM duty temperature and reignition, (b) moving the pinned
commit (CLAUDE.md SAT ground truth block: one commit, spec re-diff).

## 67. MaxTSet slider defaults: 62 °C radiators, 55 °C underfloor (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:59 UTC. This answers Q15 ("Maximum flow setpoint: a user setting, and which defaults?") from the overview of 2026-10-24 (messages 1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1564564109092192347

**Quoted:**

> Q15: yes a user setting. 55 underfloor, 62 radiators / Q16: please expand / Q17: yes / Q19: leave them as is

**Settled by this statement:**

- He confirms again that the maximum flow setpoint is a user setting (entries 48 and 51).
- The default when the user has not chosen a value is 62 °C for radiators and 55 °C for underfloor. This settles the first Open point of entry 48 ("the default value when nothing is chosen").

**Compared with thermo-nova and the firmware (opened this session):**

- thermo-nova's default is 50 for underfloor and 55 for every other system, heat pumps included (`helpers.py:97-99`, `calculate_default_maximum_setpoint`). The user can choose 10..100 (`config_flow.py:612-613`). Both of his values are higher than thermo-nova's: radiators +7, underfloor +5.
- thermo-nova has a 62 / 45 / 40 table (radiators / underfloor / heat pump), but it is `OVERSHOOT_PROTECTION_SETPOINT` (`const.py:189-193`), not a maximum setpoint. The firmware's fixed caps have the same values. The firmware comment that 40 °C "matches Python's heat pump cap" (`SATcontrol.ino:301`) is not borne out: no heat-pump maximum setpoint was found in thermo-nova.
- The firmware has a heating-system setting: `iHeatingSystem` auto / radiators / underfloor (`SATtypes.h:40-44`, `:463`, settings key `SATsystem` at `settingStuff.ino:997`). AUTO resolves to radiators (`SATcontrol.ino:272-273`), so a per-system default can be applied today.
- The firmware has no user MaxTSet today. The fixed system cap is 62 radiators / 45 underfloor (`SATcontrol.ino:305`), and 40 for a heat-pump source (`:307`). `settings.sat.fMaxSetpoint` (65, `SATtypes.h:538`) has no settings key: a grep of `settingStuff.ino` for maxtset/maxsetpoint finds nothing.
- **Conflict with the firmware ceilings:** `satEffectiveMaxSetpoint()` takes the boiler's MaxTSet, then the system cap (`:318`, `:320`), then `SAT_HARD_MAX_FLOOR = 50` for underfloor (`SATcontrol.ino:137`, applied at `:319`, `:321`).
  - As the code stands, an underfloor CS is capped at 45 by the system cap.
  - If TASK-1373 replaces the system cap with the user value, the 50 hard ceiling still turns a 55 default into 50.
  - The hard ceiling is a firmware addition. thermo-nova has no separate underfloor ceiling, only the global `MAXIMUM_SETPOINT = 65.0` (`const.py:36`).
  - We put this to him (clarification Q15b) and do not resolve it ourselves.

**Changes an earlier entry:** it completes entry 48 (Open: default value). It agrees with entries 51 and 54 (the user value caps every CS).

**Interpretation (ours):**

- He gave no value for heat pumps. Until he says otherwise we keep the firmware's 40 °C heat-pump cap (`SATcontrol.ino:307`). This differs from thermo-nova, whose default is 55 for a heat pump (`helpers.py:99`). The 40 °C in entry 30 appears only in our own interpretation text, not in his words. The heat-pump default has to be asked separately.
- "Default" means the value used until the user moves the slider. It is clamped to the boiler's bounds when the boiler reports them. Q41, on which he asked us to "please expand", is still open on whether those bounds come from MsgID 49 or MsgID 57.

**Task:** TASK-1373. Add the per-system defaults to its AC once the underfloor ceiling is settled.

**Open:**

- Underfloor 55 against the 50 hard ceiling (Q15b asked).
- The heat-pump default (not asked yet).
- Whether SAT writes the default to the boiler (`SH=` / MsgID 57) when the user never moved the slider. That write would overwrite a MaxTSet the installer set in the boiler. Entry 48 says the chosen value is passed to the boiler, but not whether that also applies to an unchosen default. Not asked yet; it belongs with Q41.

## 68. Cycles per hour: "please expand" (request, 2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:59 UTC. This replies to Q16 ("Cycles per hour default and range") in the overview of 2026-10-24 (messages 1563656700118958262 and 1563656702086221987):
https://discord.com/channels/812969634638725140/1514720723980259460/1564564109092192347

**Quoted:**

> Q16: please expand

**Settled by this statement:** nothing. He is replying to the one-line overview item. The default was already answered on 2026-10-25 (entry 52: 3 per hour for gas boilers, 2 per hour for heat pumps, message 1563811907708649533).

**What is still open:** the range of values a gas-boiler user may pick.

- Our 2026-10-25 question (message 1563811397534613505) offered A) default 4, choices 3 or 4; B) default 3, choices 3 or 4; C) default 3, keep 2..6. His reply (entry 52) set only the defaults and did not pick one of these for the range.
- Entry 52 recorded our reading "3 or 4 only". It rests on entry 30, where he said "We have the options 3 cycles/hour and 4 cycles/hour". TASK-1375 ("gas boiler PWM cycles per hour 3 or 4 only, default 3") is the "new task" that entry 52 names, and it is built on that reading.
- Firmware: default 3 (`SATtypes.h:549`). The setting accepts 2..6 (`settingStuff.ino:1107`). `satGetMaxCyclesPerHour()` returns 2 for a heat-pump source (`SATcontrol.ino:349`), otherwise the stored value if it is 2..6 (`:351-352`), and otherwise 3 for underfloor / 4 for radiators (`:354`).
- thermo-nova offers 3 or 4 for radiators only (`config_flow.py:695-702`) and 2 or 3 for heat pumps (`:686-693`). The options form has no cycles choice for underfloor (`:684-706`). The default is 4 (`const.py:119`).

**Not re-asked:**

- Hybrids: Q37 was answered "A" (gas timing) on 2026-10-28, message 1564866308317773845.
- The heat-pump hourly cap: Q36 was answered "A" in the same message.
- Heat-pump cycle length: entries 40 and 50.

**Changes an earlier entry:** none. Entry 52's interpretation (range 3 or 4) stays provisional until he answers.

**Task:** TASK-1375 waits for the answer.

**Open:** the gas-boiler range (expanded question below), and what happens to stored values outside that range.

## 69. Hot water setpoint: SW= on change, slider bounded by MsgID 48, re-confirmed (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:59 UTC. This answers Q17 ("DHW setpoint: SW= on change, slider bounded by MsgID 48") in the overview of 2026-10-24:
https://discord.com/channels/812969634638725140/1514720723980259460/1564564109092192347

**Quoted:**

> Q17: yes

**Settled by this statement:** the same as entry 53 option A and entry 33:

- Send `SW={value}` at once on every change, and once after a restart, while DHW control is on (entry 53).
- The slider is bounded by the boiler's MsgID 48 values (entry 33).

There is no new decision. It agrees with entries 33 and 53 and changes nothing.

**Compared with thermo-nova and the firmware:** see entry 53. thermo-nova sends the setpoint as soon as the value is set: `async_set_native_value` calls `async_set_control_hot_water_setpoint(value)` (`number.py:72-74`, opened this session).

**Changes an earlier entry:** none; it confirms entries 33 and 53.

**Task:** TASK-1376 (no change to its scope).

**Open:** entry 53's Open point remains: whether the `SW=` sent when a hot-water draw starts also stays. "Yes" does not answer it.

## 70. Coefficients set by the old wizard (1.4 / 0.6 / 0.8) stay as they are (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:59 UTC. This answers Q19 ("Units whose coefficient was set by the old wizard (1.4 / 0.6 / 0.8, not chosen by the user): leave them (entry 24) or reset them once to 2.0?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564564109092192347

**Quoted:**

> Q19: leave them as is

**Settled by this statement:**

- A stored heating-curve coefficient stays, including one the old setup wizard wrote (1.4 radiators, 0.6 underfloor, 0.8 heat pump).
- There is no one-time reset to 2.0 and no migration.
- This agrees with entry 24 ("Units that already store a coefficient keep it; no migration").

**Compared with the firmware (opened this session):**

- TASK-1347 (Done) already does this. The default is 2.0 (`SATtypes.h:467`), and in Automatic mode the wizard no longer writes `satcoefficient` (`data/v2.js:5474`).
- TASK-1347's bench note: a stored 0.60, written by the old wizard's underfloor preset, stayed 0.60 across the upgrade.
- The only firmware code that writes the coefficient on its own is the hourly auto-tune (`SATcontrol.ino:4917`; a grep for assignments to `fHeatingCurveCoeff` finds only this line and the settings clamp at `settingStuff.ino:1001`). Q22 removes the auto-tune (entry NN).

**Changes an earlier entry:** none; it confirms entry 24.

**Task:** none. TASK-1347 already implements it.

**Open:** none.

## 71. Cold cutoff: 28.2 / 21 restated, the boundary still open (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 07:32 UTC. This answers Q20 ("is a requested setpoint exactly equal to the cold setpoint (28.2 / 21) cold (`<=`, as thermo-nova heating_control.py:72 and :385) or not (`<`)?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564542304834945126

**Quoted:**

> Q20: cold cutoff should be 28.2/21 / Q21: 5.0 is fine / Q22: Remove / Q23: follow thermo-nova

**Settled by this statement:** the cold setpoint stays 28.2 °C for radiators and 21 °C for underfloor. This repeats entries 7, 11 and 24, and agrees with them.

**Not settled:** whether a requested setpoint exactly equal to 28.2 (or 21) counts as cold. He gave the values but did not choose between `<=` and `<`. We do not infer the boundary from his words; we ask again (clarification Q20b).

**Compared with thermo-nova and the firmware (opened this session):**

- thermo-nova's control path treats "equal" as cold:
  - The heater is ON only when `requested_setpoint > COLD_SETPOINT` (`heating_control.py:72`).
  - CH follows `_control_setpoint > COLD_SETPOINT` (`:238`).
  - The PWM override returns when `requested_setpoint <= COLD_SETPOINT` (`:385`).
  - Its COLD_SETPOINT is 22 (`const.py:34`). The per-system 28.2 / 21 is Sergeant D's choice (entries 7 and 11).
- thermo-nova's cycle classifier and its overshoot-at-flame-off check use strictly below (`cycles/classifier.py:51`, `device/status.py:121`).
- The firmware's control path also uses "at or below", as thermo-nova:
  - `satColdCutoff()` returns `requestedSetpoint <= coldSetpoint` (`SATchEnable.h:26-29`).
  - It drives the cutoff at `SATcontrol.ino:5442`.
  - For CH, `satChEnableDecide()` (`SATchEnable.h:46`, `:49`) calls it, and `SATcontrol.ino:5456` sends the result.
  - The `>` compares at `SATcontrol.ino:1096` and `:5419` are consistent with it.
- The firmware classifier uses strict `<`, as thermo-nova's classifier does (`SATcycles.ino:695`, `:717`).

**Conflict with an earlier entry:** the "Settled" bullet of entry 12 reads "When the requested setpoint drops to the cold setpoint or below". That records `<=` as settled, but it is our summary, not his words ("yes keep the thermo-nova solution"). Entries 7 ("below 28.2") and 11 ("lower than the thresholds") keep the boundary open. ADR-199 (`docs/adr/ADR-199-...md:69`, `:180`) requires the entry that records the Q20 answer to name entry 12 and correct that bullet. This entry does so: until Q20b is answered, the boundary is open, and entry 12's bullet must not be read as his decision. ADR-195 marked the boundary as settled by entry 12 (`ADR-195-...md:50`); ADR-199 replaces that.

**Task:** TASK-1355 / ADR-199 wait for Q20b.

- If he answers `<=`, no code changes.
- If he answers `<`, a new task changes `satColdCutoff` and the `>` compares at `SATcontrol.ino:1096` and `:5419`.

**Open:** the `<=` / `<` boundary (Q20b).

## 72. Heating curve: the coefficient upper bound stays 5.0; the hourly auto-tune is removed (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 07:32 UTC. Both are ADR-198 open questions:

- Q21: "is 5.0 enough, or should it go to 12 as thermo-nova config_flow.py:616-618?"
- Q22: "the firmware auto-tunes the coefficient hourly, thermo-nova does not: keep or remove?"

https://discord.com/channels/812969634638725140/1514720723980259460/1564542304834945126

**Quoted:**

> Q20: cold cutoff should be 28.2/21 / Q21: 5.0 is fine / Q22: Remove / Q23: follow thermo-nova

**Settled by this statement:**

- Q21: the heating-curve coefficient range stays 0.1..5.0.
  - This is a deliberate deviation from thermo-nova, which allows 0.1..12 in 0.1 steps (`config_flow.py:616-618`).
  - The default stays 2.0 (`const.py:182`, entry 24).
  - It confirms Robert's ADR-198 decision of 2026-10-24 (keep 0.1-5.0), so no follow-up ADR is needed.
- Q22: the firmware-only hourly auto-tune of the coefficient is removed.
  - thermo-nova only reads the coefficient from the options (`entry_data.py:213`, `pid.py:71`, `heating_curve.py:25`).
  - A grep for `heating_curve_coefficient` in `custom_components/sat` finds no code that writes it back.

**Compared with the firmware (opened this session):**

- Range: `SATcoefficient` is clamped to 0.1..5.0 (`settingStuff.ino:1001`). TASK-1359 aligns the REST metadata and the wizard slider to that clamp.
- Auto-tune: the block starts at `SATcontrol.ino:4798-4800`. Gain constants are at `:4806-4811`, statics at `:4814-4823`, and `satAutoTuneUpdate()` at `:4825-4934`.
  - It is off by default (`bAutoTune = false`, `SATtypes.h:535`).
  - It counts completed cycles (`state.sat.iCycleCount`, `:4836-4838`).
  - It runs once an hour, and only after at least 6 cycles (`:4822-4823`, `:4866-4867`).
  - It changes only the coefficient (`:4881-4882`), even though the setting is described as "automatic PID gains tuning" (`SATtypes.h:535`). The gains follow through the coefficient (ADR-198 point 6).
  - It scales the coefficient by the overshoot / undershoot / oscillation counts and clamps it to 0.3..5.0 (`:4913-4914`).
  - It writes the result to `settings.sat.fHeatingCurveCoeff` (`:4917`) and persists it with `writeSettings` (`:4923`).
  - It is called on each control tick while the room temperature is valid (`:5251-5252`).
  - The line numbers in the Q22 row of open-questions.md and in ADR-198 (4664-4705), and in audit #39 (4552-4660), are stale.
- What the removal touches:
  - Settings: `bAutoTune` and `fAutoTuneRate` (`SATtypes.h:535-536`), keys `SATautotune` / `SATautotunerate` (`settingStuff.ino:382-383`, `:1100-1101`).
  - State: `SATtypes.h:389-391`.
  - REST: `SATcontrol.ino:2944-2948`, `restAPI.ino:1060-1061`, `:1689-1692`.
  - MQTT: `SATcontrol.ino:3403-3407`, `:3599`; command table `MQTTstuff.ino:708-709`.
  - HA discovery: the switch at `MQTTHaDiscovery.cpp:3394-3397`.
  - Web UI: `data/v2.js:2136-2137`, `:2172`.
  - Debug: `handleDebug.ino:111`, `:265-267`.
  - Removing the HA entity and the MQTT topics is an API change: the retained discovery config must be cleared and `docs/api/MQTT.md` updated.

**Out of scope (not part of Q22):**

- `satautogains` (`settingStuff.ino:1105`), the automatic PID gains value (thermo-nova `CONF_AUTOMATIC_GAINS`, `const.py:120`).
- The heating-curve recommendation `satUpdateCurveRecommendation()` (`SATcontrol.ino:3934`, called at `:5253`). It does not write the coefficient (grep).

**Interpretation (ours):** a coefficient the auto-tune has already changed and stored stays as stored after the removal. This is the same rule as entry 24 and the Q19 answer (entry NN). He did not say this separately.

**Changes an earlier entry:** none. It closes ADR-198's open questions on the upper bound (Q21) and the auto-tune (Q22). The latter was ADR-198's last unchecked open question.

**Task:**

- TASK-1359 (range 0.1..5.0 everywhere) no longer waits on Q21.
- A new task removes the auto-tune.
- TASK-1355 AC2 still covers all three ADRs. ADR-198 now waits only on Robert's explicit OK. ADR-199 still waits on Q20b, and ADR-197 on its own questions (Q23-Q28 cluster).

**Open:** none for this entry.

## 73. PID resets: follow thermo-nova (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 07:32 UTC, answer to Q23 (overview of 2026-10-24, messages 1563656700118958262 and 1563656702086221987; Q23: "PID resets: reset integral and derivative on every target, HVAC-mode and window change as thermo-nova, or keep the firmware's narrower resets?", ADR-197, audit #15):
https://discord.com/channels/812969634638725140/1514720723980259460/1564542304834945126

**Quoted:**

> Q20: cold cutoff should be 28.2/21 / Q21: 5.0 is fine / Q22: Remove / Q23: follow thermo-nova

(Q20-Q22 are recorded in their own entries.)

**Settled by this statement:**

- The PID resets where thermo-nova resets it, with what thermo-nova resets. All citations opened this session, thermo-nova at 7beaaf5, `custom_components/sat`:
  - What a reset clears: `PID.reset()` sets `_integral = 0.0` and `_raw_derivative = 0.0` (`pid.py:134-137`). It does NOT clear `_last_error`, `_last_temperature` or `_last_derivative_updated`.
  - `reset_control_state()` resets the main PID and all area PIDs (`climate.py:461-464`; area reset `area.py:335-338`).
  - Target change: `async_set_target_temperature` returns early when the target is unchanged (`climate.py:643-644`), else resets (`:660`).
  - Preset change and preset clear reach that reset through `async_set_target_temperature` (`climate.py:616`, `:624`); setting the same preset again returns early (`:610-611`); a manual target equal to a preset goes through the preset (`:563-565`).
  - Window: open for the minimum time switches to the ACTIVITY preset (`climate.py:819`), close restores the pre-activity target (`:832`); both reach the target-change reset when the target really changes.
  - HVAC mode HEAT or OFF: reset on every call (`climate.py:570-583`).
  - reset_integral service: `pid.reset()` + `areas.pids.reset()` (`climate.py:217-220`, `services.py:31-39`).
  - Stale room sensor: `pid.reset()` (`climate.py:470-478`); that path belongs to the room-temperature entries 38 and 46, not here.

**Interpretation (ours):** "follow thermo-nova" means: (1) the same trigger set, including the zone PIDs; (2) a reset zeros the integral AND the raw derivative, and nothing else. No existing firmware function does exactly that: `satResetIntegral()` is too narrow (integral only, `SATpid.ino:86-91`), `satPidReset()` is too broad (it also clears last error, timers and `_pid_initialized`, `SATpid.ino:94-114`, which is the TASK-1270 start-up failure). The fix needs a new integral + raw-derivative reset.

**Compared with the firmware (opened this session):**

- Target set directly (`satHandleTargetTemp`, `SATcontrol.ino:2216-2246`, and the `SATtargettemp` setter, `settingStuff.ino:1000`): no PID reset. The only writes of `settings.sat.fTargetTemp` are `SATcontrol.ino:1275`, `:1284`, `:1420`, `:1443` and `settingStuff.ino:1000` (grep), so an adopted thermostat change (entry 25) also lands on a no-reset path; which caller entry 25 uses was not opened.
- Preset change and preset clear set only the published copy `state.sat.fPidI = 0.0f` (`SATcontrol.ino:1277`, `:1285`); `_pid_integral` is untouched, so the next update republishes the old integral. The raw derivative stays.
- Window open (activity switch) and close: `satResetIntegral()` (`SATcontrol.ino:1444`, `:1424`), integral only.
- SAT disable (HVAC OFF): `satPidReset()` + save (`SATcontrol.ino:2506-2507`). SAT enable (OFF to HEAT): `settingStuff.ino:982-996` calls only `satClearSafetyTrip()`; the callers of `satPidReset()` are `SATcontrol.ino:2506`, `:2621`, `:4122` only (grep), so no reset on enable.
- reset_integral over MQTT/REST: `satResetIntegral()` (`MQTTstuff.ino:620`, `restAPI.ino:1349`, `:1600`), integral only, zones untouched.
- Zone PIDs: the integral is zeroed only when a zone is excluded or outside its deadband (`SATcontrol.ino:2105`, `:2165`); no zone reset on a main target, preset, window or mode change was found in the code.

**Changes an earlier entry:** none. Related: entry 46 (lost room temperature resets P, I, D; `satPidCurveOnly`, `SATpid.ino:123-135`) is a separate path and stays as is. Entry 25 (thermostat as user interface): an adopted thermostat change is a target change, so under this answer it resets the PID, as thermo-nova would; no conflict, but the implementation must cover that path.

**Task:** to be filed ("PID resets as thermo-nova reset_control_state"); touches TASK-1270 (first-tick output) and TASK-1412.10. ADR-197 Decision 8 and its "PID resets" Open Question to be updated.

**Open:** none for Sergeant D. Implementation check: the reset must not reintroduce the TASK-1270 first-tick 0 output.

## 74. PID persistence: follow thermo-nova (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:56 UTC, answer to Q24 (overview of 2026-10-24, messages 1563656700118958262 and 1563656702086221987; Q24: "PID persistence: save and restore integral, last error, raw derivative, last temperature and the derivative time as thermo-nova, or only integral and derivative after a 30 min age check?", ADR-197, audit #42):
https://discord.com/channels/812969634638725140/1514720723980259460/1564563332449435649

**Quoted:**

> Q24: follow thermo-nova / Q25: follow thermo-nova / Q26: fixed 0.1 / Q27: match thermo-nova / Q28: Round PID values to 3 decimals

(Q25-Q28 are recorded in their own entries.)

**Settled by this statement:**

- thermo-nova persists five values per PID (`pid.py:243-249`, keys `pid.py:23-27`): integral, last error, raw derivative, last temperature, and the derivative timestamp (`last_derivative_updated`).
- It restores all five when the entity is added (`pid.py:141-157`; last error, last temperature, timestamp at `:150-152`; integral and raw derivative at `:154-155`). There is no age check: only "no stored data" skips the restore (`pid.py:147-148`).
- It saves after every PID update (`pid.py:179-181`), so every 60 s (`pid.py:20`; entry 42).
- A reset (`pid.py:134-137`) does not save by itself; the zeros reach storage with the next update.

**Interpretation (ours):** "follow thermo-nova" means: save and restore all five values, drop the 30-minute age check and the NTP gate on the restore. Two points are implementation questions for Robert, not for Sergeant D: (1) the firmware's derivative timestamp is `millis()` (`SATpid.ino:39`, `:47`), which does not survive a reboot, so restoring it needs a wall-clock (epoch) timestamp, and then NTP must be synced before the restored timestamp can be used; (2) writing LittleFS every 60 s (thermo-nova cadence) versus flash wear. A long-stale restored timestamp gives a large `delta_time`, so alpha in `pid.py:228` goes close to 1 on the first derivative step; that is thermo-nova behaviour, not a firmware choice.

**Compared with the firmware (opened this session):**

- Save (`SATcontrol.ino:2309-2320`): i, d, rd, err, ts (from the published copies `state.sat.*`, `:2315-2316`); no last temperature, no derivative timestamp. Written every 5 min (`SATcontrol.ino:5484-5486`), on disable (`:2507`, zeros), on flush (`:2622`, zeros) and in `satPersistAllToFs()` (`:2445`).
- Load (`SATcontrol.ino:2322-2359`): skipped while NTP is not synced (`:2341-2345`), discarded when older than `SAT_PID_STALE_SEC = 1800` s (`:2307`, `:2347`); retried once after NTP sync (`:4989-4993`). Only the integral and raw derivative go back into the controller (`satPidRestoreState`, `SATpid.ino:75-83`); `d` and `err` go only into the published copies (`SATcontrol.ino:2352-2353`).
- During a lost room temperature the firmware sets `_pid_lastError = NAN` (`SATpid.ino:127`, entry 46); a persisted last error must handle that value.

**Changes an earlier entry:** none.

**Task:** to be filed ("PID persistence as thermo-nova"), after TASK-1368 (60 s cadence). ADR-197 Decision 7 and Open Question "PID persistence" to be updated.

**Open:** none for Sergeant D. Robert: write cadence (60 s vs wear) and the zero-save on disable/flush (thermo-nova has no explicit zero-save).

## 75. Remove the firmware-only integral cap of plus/minus 20 and the 10 s minimum between PID updates (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:56 UTC, answer to Q25 ("Firmware-only integral cap of plus/minus 20 and 10 s minimum between PID updates: keep or remove?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564563332449435649

**Quoted:**

> Q24: follow thermo-nova / Q25: follow thermo-nova / Q26: fixed 0.1 / Q27: match thermo-nova / Q28: Round PID values to 3 decimals

(The Q25 part is "Q25: follow thermo-nova"; the others are recorded in their own entries.)

**Settled by this statement:**

- thermo-nova has neither: the integral is clamped only to plus/minus the heating curve value (`pid.py:191-192`, `clamp_to_range` in `helpers.py:148-150`), and `update()` has no minimum interval (`pid.py:159-171`); it runs on the 60 s timer (entry 42).
- So both firmware extras go:
  - the absolute cap `SAT_PID_INTEGRAL_ABS_MAX = 20` (`SATpid.ino:199-202`);
  - the 10 s minimum `SAT_PID_SAMPLE_TIME_LIMIT` (`SATpid.ino:21`) and its early return (`SATpid.ino:299-304`).

**Interpretation (ours):** it also covers the zone PID's own plus/minus 20 cap (`SATcontrol.ino:2162-2163`), because thermo-nova areas run the same PID update (`area.py:208`). He did not name zones; entry 42 made the same zone reading and left it open. The "error unchanged and less than 60 s" skip (`SATpid.ino:306-309`) is not named in Q25; it goes with the fixed 60 s cadence of entry 42 (TASK-1368), where thermo-nova has no such skip (`pid.py:159-171`).

**Changes an earlier entry:** closes the Open line of entry 28 (the two firmware-only extras never answered since 2026-06-21) and the Q25 Open line of entry 42. (Entry 42 cites the gates at the older lines `SATpid.ino:279-283` and `:286-288`; they are now `:299-304` and `:306-309`.)

**Task:** TASK-1368 (60 s cadence) or a sibling task. ADR-197 Open Question "Absolute integral cap ±20 and 10 s minimum between updates" closed; Context item 2 and Decision 3 to be updated.

**Open:** none for this question; whether zones follow remains our reading (as in entry 42).

## 76. PID deadband: a fixed 0.1, no user setting, no widening (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:56 UTC, answer to Q26 ("Deadband as a user setting (0.05-2.0) and its widening during room-temperature estimation: keep, or a constant 0.1 as thermo-nova const.py:16?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564563332449435649

**Quoted:**

> Q24: follow thermo-nova / Q25: follow thermo-nova / Q26: fixed 0.1 / Q27: match thermo-nova / Q28: Round PID values to 3 decimals

(The Q26 part is "Q26: fixed 0.1".)

**Settled by this statement:**

- The deadband is a constant 0.1 °C, as `DEADBAND = 0.1` (`const.py:16`), used by the integral (`pid.py:187`) and the derivative (`pid.py:205`).
- The user setting goes: `fDeadband` (`SATtypes.h:468`), parsed at `settingStuff.ino:1002` (range 0.05-2.0), saved at `settingStuff.ino:324`. The PID reads it at `SATpid.ino:172` and `:213`; the unused constant `SAT_PID_DEADBAND_DEFAULT = 0.1f` (`SATpid.ino:20`) can become the one source.
- The widening goes: during room-temperature estimation the firmware adds 0.5 °C per hour to `settings.sat.fDeadband` (`SATcontrol.ino:5202-5206`, saved at `:5180`) and restores it later (`:5360-5363`).

**Side effects (from the code):**

- The curve recommendation threshold `2 * deadband` (`SATcontrol.ino:3348`, `:3976`) becomes a fixed 0.2, which matches thermo-nova `_ERROR_THRESHOLD = DEADBAND * 2` (`sensor.py:301`). (thermo-nova's in-band count, `temperature/history.py:107`, uses `DEADBAND` itself, 0.1.)
- The zone PID reads the same setting (`SATcontrol.ino:2153`); it becomes 0.1 too.
- Removing the setting changes the API: the settings setters (MQTT `deadband` command, `MQTTstuff.ino:711`; REST, `restAPI.ino:1697-1698`), the REST read-outs (`restAPI.ino:2275`, `:3889-3890`, settings list `:4195`), MQTT `sat/deadband` (`SATcontrol.ino:3566`), the SAT JSON field (`SATcontrol.ino:2791`), the settings help (`settingsHelp.h:74`), the web UI (`v2.js:2020`, `:2170`, `:4647`) and the debug dump (`handleDebug.ino:94`).
- Not part of this: the 2-hour thermal-safe setpoint of the same estimation block (`SATcontrol.ino:5184-5201`).

**Changes an earlier entry:** none (entry 28 already uses thermo-nova's 0.1).

**Task:** to be filed. ADR-197 Open Question "Deadband as a setting and its widening" closed; Context item 1 and Decision 3 (which describe the setting) to be updated.

**Open:** none.

## 77. PID derivative details: match thermo-nova (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:56 UTC, answer to Q27 ("Derivative details: 'no change' threshold 0.001 vs exactly 0, and the reference temperature that moves only with the derivative time (thermo-nova pid.py:171, :211): match thermo-nova?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564563332449435649

**Quoted:**

> Q24: follow thermo-nova / Q25: follow thermo-nova / Q26: fixed 0.1 / Q27: match thermo-nova / Q28: Round PID values to 3 decimals

(The Q27 part is "Q27: match thermo-nova".)

**Settled by this statement:**

- "No change" is `temperature_delta == 0.0` (`pid.py:209-213`), not the firmware's `fabsf(tempDelta) < 0.001f` (`SATpid.ino:232-237`).
- The previous temperature is overwritten on every PID update (`pid.py:171`), also when the derivative step was skipped because 60 s or less passed (`pid.py:215-218`). The firmware's separate reference `_pid_derivRefTemp`, which moves only together with the derivative timestamp (`SATpid.ino:49-56`, set at `:226`, `:235`, `:255`, `:270`; TASK-1195), goes.

**Consequence to verify on the bench (not a question):** the firmware filters the room temperature before the PID (TASK-894, kept by entry 28). The TASK-1195 comment (`SATpid.ino:49-55`) says the EMA spreads one sensor step over about 8 control ticks; with the per-update overwrite, a change that arrives within 60 s of the last derivative step is dropped from the delta while the time keeps running. With the 60 s cadence (entry 42) this may matter less. The exact-zero test on a float EMA output also rarely sees exactly 0. The derivative may therefore fire with smaller values than today. Measure old vs new before release; if D becomes nearly inactive, report it to Sergeant D with the data.

**Changes an earlier entry:** none. Entry 28 names TASK-1195 (derivative timer) as its task, but no entry quotes him on the separate reference temperature; this answer undoes that part of TASK-1195. Entry 28's "the PID must be identical to pid.py" agrees with this answer, and its 2026-10-03 point (the filter chain stays) is not changed: the room EMA and the derivative's own low-pass stay.

**Task:** to be filed, after TASK-1368. ADR-197 Decision 5 to be updated; Open Questions "Derivative 'no change' test" and "Derivative reference" closed.

**Open:** none for now; bench result may lead to a follow-up.

## 78. PID rounding: "Round PID values to 3 decimals" (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 08:56 UTC, answer to Q28 ("Integral with Ki near 0 skips the clamp (SATpid.ino:166) and no rounding of gains/terms/output (pid.py:71-132): match thermo-nova?"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564563332449435649

**Quoted:**

> Q24: follow thermo-nova / Q25: follow thermo-nova / Q26: fixed 0.1 / Q27: match thermo-nova / Q28: Round PID values to 3 decimals

(The Q28 part is "Q28: Round PID values to 3 decimals".)

**What thermo-nova does (pid.py, opened this session):**

- Automatic gains are rounded to 6 decimals: Kp (`pid.py:71`), Ki (`:82`), Kd (`:93`). Manual gains are not rounded (`:65`, `:77`, `:88`).
- P (`:101`), I (`:106`), D (`:114`) and the raw-derivative property (`:119`) are rounded to 3 decimals.
- The output is rounded to 1 decimal (`:132`): curve + rounded P + rounded I + rounded D.
- Rounding is on read only: the stored `_integral` and `_raw_derivative` are not rounded (`:191-192`, `:229-230`). The integral step uses the rounded `self.ki` (`:191`).
- The error is already rounded to 3 decimals in the firmware (`satPidError`, `SATpid.ino:63-66`). The firmware rounds nothing else in `satPidUpdate` (`SATpid.ino:321-334`) or `_pidCalculateGains` (`:156-162`).

**Interpretation (ours, to be confirmed):** for the P, I and D terms his rule equals thermo-nova. For the gains (thermo-nova 6 decimals) and the output (thermo-nova 1 decimal), a literal "everything to 3 decimals" is his deviation from thermo-nova. It has a real effect on Ki: Ki = Kp / 8400 (`pid.py:82`), Kp = coefficient * curve value / 3 or 4 (`pid.py:70-71`), and the curve value is 27.2 (radiators) or 20 (underfloor) plus coefficient/4 times the curve term (`heating_curve.py:39`, `types.py:117-118`). At coefficient 2 and a curve value near 40, Kp is about 27 and Ki about 0.0032, which 3 decimals turn into 0.003 (about -6 %). The rounding step is up to 0.0005, so the relative change of Ki is up to 0.0005/Ki: up to 12.5 % at Ki 0.004, 25 % at 0.002, 50 % at 0.001. For Kp below 4.2, Ki rounds to 0 and the integral stops (this needs a low coefficient, below about 0.6). Not acted on until he confirms the scope.

**Not answered:** the Ki-near-0 half of Q28. The firmware returns before both the integral step and the clamp when Ki < 1e-9 (`SATpid.ino:187`); thermo-nova always adds and clamps (`pid.py:191-192`). A Ki rounded to 0 (above) would make that branch reachable in automatic mode, so the two halves are linked.

**Changes an earlier entry:** none. The ADR-197 Open Question and Q28 cite `SATpid.ino:166`; the line is now `:187`.

**Task:** none until the follow-ups are answered.

**Open:** (1) rounding scope (follow-up question Q28a); (2) the Ki-near-0 clamp (follow-up question Q28b, ask after Q28a).

## 79. OpenTherm back after a loss: SAT resumes by itself on the first boiler reply (2026-10-27)

**Source:** #alpha-testing, 2026-10-27 10:06 UTC. Answer to Q34, asked 2026-10-26 19:19 UTC (message 1564357717089587211). The options were: A) SAT resumes by itself on the first boiler reply; B) SAT stays stopped until the user switches it off and on; C) SAT resumes by itself only after the link has been back for a set time. Our advice was A.
https://discord.com/channels/812969634638725140/1514720723980259460/1564581168630861865

**Quoted:**

> Q34: A

**Settled by this statement:**

- When OpenTherm comes back after a loss, SAT resumes by itself on the first new boiler reply. The user does nothing. A loss here is entries 39 and 47; 'lost' means no boiler reply for 30 s, Q33 A, answered by the maintainer on 2026-10-26.
- SAT takes MsgID 1 back from the thermostat.
- The PID restarts from 0. Option A said so in so many words: "PID integral restarts from 0".
- This confirms the provisional TASK-1365 behaviour. No code change.

**Compared with thermo-nova and the firmware:**

- thermo-nova has no stop on a lost link, so it has nothing to resume. Its control loop skips a pass only when the outside temperature is None or HVAC is OFF (`climate.py:544-545`). We opened nothing else for a loss or resume path; none was found in the code we opened.
- Firmware, how it resumes: `satOtLinkDecide()` returns RESUME once the boiler-reply stamp has changed since the hand-back (`SATcontrol.ino:2570-2576`, the test is at `:2572`). `satOtLinkStep()` then clears `bOtLinkLost` and resets the command cache, so the next tick sends CS, MM and CH again (`:2604-2609`).
- Firmware, what the loss does: the hand-back is `satDisable()` (`:2598-2603`, defined at `:2485`). It:
  - resets PWM (`:2500`);
  - resets the PID and writes the zeros to flash (`:2506-2507`);
  - also saves the energy totals (`:2508-2509`);
  - resets the curve recommendation in RAM only (`:2510`; `satHCRReset()` at `SATcycles.ino:1259-1267` writes no file);
  - sends `CS=0`, `CH=1` and `MM=T` (`:2520`, `:2525`, `:2530`).
- After a resume the PID has been reset. With a valid room temperature, the first control tick therefore sends CS=10/CH=0 until the next full PID update (`SATpid.ino:109`, `:287-309`; floor and cold cutoff at `SATcontrol.ino:5378`, `:5442-5447`). That is the Q39 path; see the Q39 entry. Q39 A fixes the first tick after a resume as well. This is derived from the code: the TASK-1365 bench note of 2026-10-25 logged CS=23.0/MM=100/CH=1 re-sent on resume, with the room-temperature state not recorded (without a room temperature the curve-only path applies, `SATcontrol.ino:5297-5298`).

**Changes an earlier entry:**

- Entry 39, Open: settles "Whether SAT starts again by itself when the link returns".
- Entry 47, Open: settles the resume half.
- Entry 39's literal "stop" now applies only while the loss lasts.
- No conflict with entry 26: the thermostat is the backup during the loss.

**Task:** TASK-1365. AC#2 says "answered by Sergeant D (Q33, Q34)", but Q33 was answered by Robert, not Sergeant D. Amend AC#2 or accept the maintainer's answer explicitly.

**Open:**

- Option A does not settle the firmware side effects that the Q34 text named:
  - each loss writes three files: the zeroed PID (`:2507`) and the two energy totals (`:2508-2509`);
  - a flapping link repeats those flash writes;
  - a loss at boot, before the NTP-gated `satLoadPidState()`, overwrites the saved PID with zeros (stated in the Q34 text; not re-traced in this run).
- These are firmware/maintainer points connected to entry 28 (Open). They are not SAT design.

## 80. PWM ON phase: time it from the phase start and wait for ignition as thermo-nova; start ON only when the ON time is at least 180 s (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC. Answers to Q35 and Q38, two items of the queued questions in `docs/sat/open-questions.md`:
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted** (excerpt; the same message also answers Q36, Q37 and Q40-Q46):

> Q35: A
> Q38: A

**Settled by this statement:**

- **Q35 A:** a PWM ON phase is timed as thermo-nova does it, in this order (code opened this session):
  1. **ON start** (`pwm.py:160-167`). `_last_update` is set to the start (`:162`). `_waiting_for_flame_on` is set when there is no flame (`:163`). `_flame_on_at` is set to the start only when the flame is already burning (`:164`).
  2. **Waiting, flame appears.** The phase re-anchors at the ignition: `_last_update` and `_flame_on_at` become the ignition time, and the boiler temperature is reseeded (`:186-193`).
  3. **Waiting, no flame yet.** While `elapsed` (counted from the ON start) is below 180 s, the phase stays ON (`:194-196`).
  4. **Still no flame after 180 s.** Only the wait flag is cleared (`:197-199`). `_last_update` is **not** reset. "starting timer anyway" is the log text at `:199`, not a new anchor.
  5. **Minimum flame-on hold.** For 180 s after `_flame_on_at` the phase stays ON (`:201-203`). A flame that first appears after the wait has timed out sets `_flame_on_at` at `:144-146`, so the hold applies from then.
  6. **Expiry.** When `elapsed >= on_time`, the phase goes to OFF, with `_last_update` set to now (`:205-209`).
  - Result: a phase that ignites at t_i (while waiting) ends at t_i + max(on_time, 180 s).
  - A phase that never ignites ends at on_time after its start. If on_time is about 180 s or less, that can be the same tick on which the wait times out.
  - thermo-nova has no ignition abort that ends the ON phase.
- **Q35, our wording.** The Q35 text said "then start the on timer anyway". That came from the log string. Since he answered "as thermo-nova", we implement what the code does (steps 4 and 6), not that wording.
- **Q38 A:** an OFF or IDLE phase moves to ON only when the mapped on_time is at least `HEATER_STARTUP_TIMEFRAME` = 180 (`pwm.py:155`, `const.py:18`). This applies to every ON start, not only the first.
  - In thermo-nova the gate only bites in the ultra-low range without a flame. That range gives on_time 0 (`pwm.py:259-260`); with a flame it gives the 180 s lower threshold (`:247-257`, `:53`), and the other ranges give 180 s or more (`pwm.py:263-301`).

**Compared with thermo-nova and the firmware:**

- Firmware, ON start with the flame already burning: anchored at the phase start (`_pwm_onStartMs`, `SATcontrol.ino:872-877`, used at `:1024`). Q50 A confirms this.
- Firmware, ON start without a flame: `_pwm_onStartMs = 0` (`:883`). The anchor then falls back to the cycle tracker's last flame-on (`:1024`, set at `SATcycles.ino:741`), which is an older flame-on. So the expiry test (`SATcontrol.ino:1028`) is true on the next tick, and `satPwmEndOn()` ends the phase (`:1030`).
- The firmware also has a 180 s ignition abort (`:1035-1042`). thermo-nova has none.
- In OFF, the wait is timed from the last flame-off (`:1046`). That flame-off is usually long past, so ON restarts at once (`:1058`, `:1076`). CS and CH toggle every tick.
- Host evidence: `tests/test_sat_hp_pwm.py:248-249`, `test_r3_on_phase_waits_for_flame`, still marked `@unittest.expectedFailure`.
- Firmware, Q38: the 180 s gate exists only on the PWM-entry path (`entryOn = entry && onTimeMs >= minOnMs`, `:1057`). The normal start (`sinceFlameOff >= offTimeMs`, `:1058`) has no gate. Range 1 without a flame sets on 0 and off maxMs = 2P (`:998-1000`, `:946`), so ON is requested 2P after the flame-off.
- Line check (question cite to current HEAD):
  - ON timing: `SATcontrol.ino:949-958` is now `:1021-1032`.
  - Abort: `:959-970` is now `:1035-1042`.
  - Cycle-tracker flame-on: `SATcycles.ino:722` is now `:741`.
  - OFF branch: `SATcontrol.ino:971-1002` is now `:1043-1078`.

**Changes an earlier entry:**

- Settles entry 43, Open bullet 1: whether the 180 s minimum on time gates every ON start. It does (audit #19).
- Consistent with entry 31 (180 s minimum flame time) and with entry 43 (the first phase is ON).
- Entry 45 / TASK-1371 (the 20 s ignition flood) assumes the ON phase lasts through ignition. Q35 A makes that true.

**Task:** TASK-1382 (Q35), scoped together with TASK-1380 per its description. TASK-1383 (Q38). Both currently depend on TASK-1412.3.

**Open:**

- Duty range 5 starts ON directly (`SATcontrol.ino:982-990`, `satPwmBeginOn` at `:987`). That skips the Q38 gate and the hourly limit (comment at `:985-986`).
  - In thermo-nova, range 5 goes through `:155-160` with on_time = 2P (`pwm.py:297`, `:55`), so it passes the gate but meets the hourly check at `:156`. That check never blocks at the defaults.
  - Not asked.
- After Q35 lands, the provisional Q48-Q51 decisions must be revisited (TASK-1412.9 AC#2). Q49 B depends on Q35 directly; see the Q49 entry.
- Interaction with Q40 B (same message, another entry), the detection of a missing return temperature, for the TASK-1371 flood: not checked here.

## 81. Heat pumps: no hourly start limit in PWM, as thermo-nova (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC. This answers Q36, queued after the TASK-1366 review of 2026-10-25. It is part of a multi-answer message:
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted:**

> Q35: A / Q36: A / Q37: A / Q38: A / Q40: B for now, we may need to reconsider later / Q41: Please expand / Q42: Please expand / Q43: A / Q44: A / Q45: A / Q46: B

**Settled by this statement (his literal choice):**

- Q36 option A: heat pumps get no hourly cap on PWM ON starts, as in thermo-nova.
- In thermo-nova (opened this session; the local snapshot is commit c5211af, recorded as a mirror of 7beaaf5, with no local changes):
  - the check is `if self._current_cycle >= self._config.duty_cycle_seconds` (`pwm.py:156`);
  - `duty_cycle_seconds` comes from CONF_DUTY_CYCLE (`entry_data.py:220`), and its default is "00:13:00", which is 780 (`const.py:167`);
  - so the cap never takes effect at defaults, for any boiler type. There is no heat-pump case.
- The Q36 row said "Related to Q12", so the link to entry 59 was in front of him. Q36 says nothing about gas boilers, so for them entry 59 stands.

**Firmware at HEAD:**

- A heat pump is capped at 2 PWM starts per rolling hour (`SATcontrol.ino:345-349`).
- The cap is checked before every new ON phase (`SATcontrol.ino:1058-1074`), with two exceptions:
  - the ON phase at PWM entry while the flame burns (`:1063`);
  - duty range 5 (`:982-987`).
- The heat-pump period is 1800 s or 2400 s (`SATcontrol.ino:367-373`).

**The premise of Q36 has moved (we must tell him):**

- Q36 was written on 2026-10-25. Back then the firmware counted every flame-on edge, hot water and relights included, and the question cited `SATcycles.ino:279-299` for that counter.
- Since TASK-1401 the rolling-hour ring holds only PWM ON-phase starts:
  - the only writer is `satCycleRecordPwmStart()` (`SATcycles.ino:309-317`);
  - it is called through `satPwmCountStart()` (`SATcontrol.ino:851-861`).
- So the reason given in Q36 no longer holds ("one extra flame-on in the hour (hot water, relight) holds the next PWM start").

**How often a cap of 2 per hour would bind** (derived from the code, not measured):

- A heat-pump cycle in duty ranges 1-4 normally lasts at least one period (`SATcontrol.ino:991-1017`, upperMs = 1800/2400 s).
- It can be shorter in one case. The OFF wait counts from the last flame-off (`:1046`), and `:1050-1053` corrects that only while the flame burns. So a flame-out early in the ON phase shortens the cycle.
- Range 5 starts (`:982-987`) and PWM re-entry starts are extra starts.
- With Q49 = B (same message), every ON start counts, failed ignitions included.
- So the cap can bind, but rarely. Choosing A removes that.

**Relation to earlier entries:**

- **Entry 59:** "count only PWM's own ON phases against cycles per hour". It has no exemption by heating source and keeps the cycles-per-hour limit real, unlike thermo-nova (`pwm.py:156`). Q36 A narrows it, which is a later and more specific answer rather than a contradiction:
  - gas boilers and hybrids keep the limit (entry 59; for hybrids see the Q37 entry);
  - heat pumps have none.
- **Entry 52:** "Keep 3x hour for gas boilers and 2x hour for heat pumps as default values." Not a conflict. For heat pumps, "2x hour" is the cycle length (1800 s, entry 50). Q36 A means that number is no longer also a start cap.
- **Entry 50:** no conflict. It sets only the minimum ON time (180 s) and the cycle length (1800/2400 s).
- **Entry 30:** our interpretation there gave heat pumps "at most 2 cycles per hour". His words were only the options 2 and 1.5 per hour. Q36 A replaces our interpretation, not a statement of his.

**Changes an earlier entry:** narrows entry 59 (heat pumps exempt). Replaces our interpretation in entry 30.

**Task:** a TASK-1366 follow-up: skip the limit for heat pumps (`SATcontrol.ino:1063` / `:349`) and keep counting for `iCyclesThisHour`.

**Open:** none blocking. A heads-up (Q36b) tells him the counter changed since he read the question.

## 82. Hybrid in PWM: gas boiler timing (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC, answer to Q37 (queued after the TASK-1366 review of 2026-10-25):
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted:**

> Q37: A

**Settled by this statement:**

- A hybrid source in PWM uses the gas boiler timing:
  - a minimum ON time of 180 s;
  - a cycle length of 3600 / cycles per hour;
  - the gas choices and default: 3 or 4 per hour, default 3 (entry 52, with our range interpretation there).
- It does not get the heat-pump cycle length of 1800/2400 s (entries 40 and 50).

**Compared with thermo-nova and the firmware:**

- thermo-nova's PWM timing has no case per source: a 180 s minimum and a period of 3600 / cycles_per_hour (`pwm.py:53-55`). A grep of its `.py` files for "hybrid" finds nothing.
- The firmware at HEAD already does this. Only `SAT_SRC_HEAT_PUMP` is a special case, for:
  - the hourly cap (`SATcontrol.ino:349`);
  - the period (`:369-370`).
- A hybrid (`SAT_SRC_HYBRID`, `SATtypes.h:51`) therefore gets:
  - the user's cycles per hour (`SATcontrol.ino:351-353`), and outside the range 2..6 the fallback 3 for underfloor or 4 otherwise (`:354`);
  - a period of 3600 / cycles per hour (`:372`);
  - a minimum ON time of 180 s (`:359-362`).
- Under entry 59, the hybrid also keeps the real hourly limit, as a gas boiler does. That is our reading of "gas timing"; he did not mention the limit. Q36 (heat pumps exempt) does not cover hybrids.
- Out of scope here and unchanged: MM for hybrids is never 0 (entry 21; `SATcontrol.ino:384-385`, `:5434`).

**Changes an earlier entry:** it closes two open items:

- entry 50 Open ("whether hybrids get the heat pump cycle lengths"): no;
- entry 52 Open ("what hybrids default to"): the gas default, 3 per hour.

**Task:** no timing change. The gas-boiler range task from entry 52 (3 or 4 per hour) applies to hybrids as well.

**Open:** none.

## 83. First control tick after boot or SAT enable: send curve + P + restored I, not CS=10/CH=0 (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:14 UTC. Answer to Q39 (TASK-1270). Our options were: A) send curve + P + restored I with D=0 on the first tick (as thermo-nova); B) send the curve only on the first tick; C) keep today's CS=10/CH=0. Our advice was A.
https://discord.com/channels/812969634638725140/1514720723980259460/1564869959920783400

**Quoted:**

> @BlueFlame Bot Q39: A

**Settled by this statement:**

- On the first control tick after boot or SAT enable, SAT sends a real setpoint: heating curve + P + restored I. It no longer sends CS=10 with CH=0 until the first full PID update.
- The same holds after an OT-loss resume (Q34). The PID was reset there, so I = 0.

**Compared with thermo-nova and the firmware:**

- Firmware, the first `satPidUpdate()` call (with a valid room temperature):
  1. it initialises and sets the update time to now (`SATpid.ino:287-297`);
  2. with `elapsed` = 0, below the 10 s limit, it returns the stored `state.sat.fPidOutput` (`:299-304`, limit at `:21`);
  3. that stored value is 0 at boot and after `satPidReset()` (`:109`);
  4. later calls with an unchanged error keep returning it until 60 s have passed (`:306-309`, `:22`).
- The PID output 0 is floored to `SAT_MIN_SETPOINT` (`SATcontrol.ino:5378`). The cold cutoff then sends CS=10 (`:5442-5447`), and the CH rule sends CH=0 (`:5454-5456`).
- `satPidRestoreState()` pre-loads the integral and the raw derivative from flash (`SATpid.ino:75-80`).
- Line check: question cite `SATpid.ino ~264-285` is now `:287-309`.
- thermo-nova: `output` = round(curve + P + I + D, 1) (`pid.py:126-132`), and 0.0 only when the curve is None (`:129-130`).
  - P = kp × `_last_error` (`:96-101`); I = `_integral` (`:104-106`); D = kd × `_raw_derivative` (`:108-114`).
  - `_integral` and `_raw_derivative` are restored from storage (`:154-155`).
- **Conflict in our own option text.** Option A said "D=0 ... (as thermo-nova)", but thermo-nova's first output includes D from the restored derivative. Both cannot hold. Re-ask (see clarifications).

**Changes an earlier entry:** none.

- Entry 42 (60 s PID cadence, TASK-1368, which TASK-1270 depends on) is unchanged.
- Entries 38 and 46 (no room temperature: curve only, `satPidCurveOnly`, `SATcontrol.ino:5297-5298`) are not affected.
- Q28 (round the PID values to 3 decimals, another entry) matches thermo-nova's rounding of P, I and D (`pid.py:101`, `:106`, `:114`). thermo-nova rounds the output to 1 decimal (`:132`).

**Task:** TASK-1270.

**Open:** the D term on the first tick: D=0, or kd × the restored derivative as thermo-nova.

## 84. "No return temperature" means Unknown-DataID on MsgID 28, for now (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC, answer to Q40 (boiler-capability research, 2026-10-25):
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted:**

> Q40: B for now, we may need to reconsider later

**Settled by this statement (provisional, in his own words "for now"):**

- Entry 45's "boiler reports no return temperature" is detected this way: the boiler answered Unknown-DataID to a read of MsgID 28 (Tret).
- Not chosen:
  - A, never reported (None, as in thermo-nova);
  - C, Data-Invalid or stale;
  - D, present only through the board sensor.
- He expects to revisit it.

**Compared with thermo-nova:**

- thermo-nova has no Unknown-DataID test. The return temperature is `None` when the coordinator has no value:
  - MQTT: `coordinator/mqtt/opentherm.py:114-118`;
  - serial: `coordinator/serial/__init__.py:99-103`;
  - the base class returns None: `coordinator/__init__.py:189-191`.
- With None, the flame-off override returns without a hold setpoint (`heating_control.py:392-395`).
- B is therefore a further deviation. It comes on top of entry 45, which already deviates on purpose from `heating_control.py:392-395`.

**Compared with the firmware at HEAD (how Unknown-DataID on MsgID 28 is tracked):**

- The gateway already keeps a per-MsgID bitmap of what the boiler does not implement:
  - `boilerUnsupportedRead[32]` (`OTGW-Core.ino:5064`);
  - its accessor `isBoilerMsgIdUnsupportedRead(id)` (`:5074-5076`).
- Setting a bit (`OTGW-Core.ino:5438-5450`):
  - it is set when the boiler answers Unknown-DataID in a read context, with boiler evidence;
  - answers the gateway made itself are skipped (OT-Direct master mode, its UI= table, loopback; `:5439-5443`).
- Clearing a bit (`OTGW-Core.ino:5407-5426`): only a genuine boiler B frame with a real answer clears it.
- Persistence:
  - the bitmap is written to `/ot-boiler.json` and reloaded at boot (`:5059-5062`, `:5407-5409`);
  - so one verdict survives a restart until the boiler answers 28 for real.
- SAT does not use the bitmap today:
  - the only callers are MQTT (`MQTTstuff.ino:1935`) and REST (`restAPI.ino:827`, `:876`);
  - `satGetReturnTemp()` returns the raw `OTcurrentSystemState.Tret` without a validity check (`SATcontrol.ino:1643-1647`);
  - without a value, the flame-off hold floors at the cold setpoint (`SATcontrol.ino:827-833`).
- So the TASK-1371 detection reads `isBoilerMsgIdUnsupportedRead(28)`.

**Changes an earlier entry:** provisionally settles the open item of entry 45 (how "no return temperature" is detected).

**Task:** TASK-1371, with TASK-1412.5 (measurement validity).

**Open:**

1. A verdict exists only after something reads MsgID 28 and the boiler answers.
   - OT-Direct polls MsgID 28 itself every 10 s (`OTDirect.ino:147`, `R_ENTRY(28, OT_TEMP_INTERVAL_MS)`).
   - **Not traced in this session:** whether those boiler answers pass the `boilerEvidence` test, so that they set the bit.
   - PIC boards: whether the PIC reads MsgID 28 when the thermostat does not was **not checked**. A firmware comment says the thermostat polls MsgID 28 only every few minutes (`OTGW-Core.ino:1107-1108`).
2. Overlap with option D (entry 6, gruijter's Intergas):
   - a board sensor set to 'R' feeds Tret without any OpenTherm frame (`OTGW-Core.ino:1107-1112`, `:1312-1323`);
   - the boiler may still answer Unknown-DataID on 28;
   - which wins: the sensor value (a return temperature exists) or the Unknown-DataID verdict? Re-ask him.
3. Data-Invalid or a stale Tret (option C) stays unhandled. That is his "reconsider later".

## 85. Q41 and Q42: he asked us to expand (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted:**

> Q41: Please expand / Q42: Please expand

**Settled by this statement:** nothing. Both questions were too short to answer. Expanded versions are drafted, to be posted one at a time.

**Material for Q41** (bounds of the MaxTSet slider, TASK-1373; entry 48 said "the MsgID 57 values ( Lower/Higher )"):

- Spec v4.2:
  - Each remote boiler parameter has an upper and a lower bound. The master should read them; without them the master may apply its own defaults (lines 2016-2019).
  - MsgID 6 carries the transfer-enable and read/write flags. Bit 1 is "max CHsetpoint" (lines 2029-2040).
  - MsgID 49 carries the upper and lower bound of maxCHsetp (lines 2059-2060, 2382).
  - MsgID 57 is the value itself, read/write (line 2062).
  - So "Lower/Higher" belongs to MsgID 49, not to MsgID 57.
  - "Identical to the DHW Setpoint slider" (entry 48) matches the DHW pair: bounds in MsgID 48, value in MsgID 56 (lines 2057-2058, 2381).
- thermo-nova:
  - The device number "Maximum Setpoint {name} (Device)" (`number.py:84`) has a fixed range of 30..80 (`number.py:116-123`).
  - It is written with `SH=` (`number.py:125-127`, `coordinator/mqtt/opentherm.py:224-225`).
  - It reads only MaxTSet (`coordinator/mqtt/opentherm.py:18`, `:79-83`).
  - Neither MsgID 49 nor MsgID 6 was found in that path.
  - Entry 48's fallback of 20/80 differs from thermo-nova's 30/80.
- Firmware:
  - SAT reads only `OTcurrentSystemState.MaxTSet` (`SATcontrol.ino:316`).
  - MsgID 49 is stored in `MaxTSetUBMaxTSetLB` on the PS=1 summary route (`OTGW-Core.ino:4627`). The frame path was not traced.
  - OT-Direct already has a periodic write of MsgID 57 for `SH=` (`OTDirect.ino:143`).
  - No SAT code reads MsgID 49 or MsgID 6.
- Related, same day: his Q15 answer of 2026-10-27 08:59 UTC (msg 1564564109092192347), "yes a user setting. 55 underfloor, 62 radiators". It gives slider defaults per heating system. It is recorded in the cluster that handles Q15; it does not change Q41.

**Material for Q42 (on/off boiler):**

- Spec v4.2: MsgID 3 HB bit 1 = "Control type [ modulating, on/off ]" (line 1605).
- thermo-nova only shows the bit as a binary sensor, "Device Control Type" (`coordinator/serial/binary_sensor.py:51`). A grep finds no other use.
- Firmware: SAT does not read the bit. It appears only in:
  - an OT-Direct debug print of the MsgID 3 handshake (`OTDirect.ino:943-959`);
  - a comment (`OTGW-Core.ino:3113`).
- Entry 13 removed the user Force PWM switch. The question is whether that also rules out a PWM forced by capability.
- Precedent: in entry 30 he himself proposed deriving the device type from a MsgID 3 bit (cooling supported). Capability-driven behaviour from MsgID 3 is therefore not foreign to his design.

**Changes an earlier entry:** none.

**Task:** TASK-1373 waits on Q41. Q42 has no task.

**Open:** both questions, to be re-asked in expanded form.

## 86. Sustained switches: overshoot counts at equality, underheat and saturation timed as thermo-nova, the 60 s switch timed on the tick's scheduled time (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC. Answers to Q43, Q44 and Q46:
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted** (excerpt; Q43 and Q44 appear twice in the message, with the same answer):

> Q43: A
> Q44: A
> Q43: A
> Q44: A
> Q46: B

**Settled by this statement:**

- **Q43 A:** the 60 s overshoot switch counts a flow exactly on requested + margin as overshoot (`>=`). It resets only when the flow is below that threshold, as thermo-nova does (`heating_control.py:288-291`).
  - The margin stays 2.0 (entries 15 and 37).
  - thermo-nova's own margin is `OVERSHOOT_MARGIN_CELSIUS` = 3.0 (`cycles/const.py:10`, imported at `heating_control.py:21`). "As thermo-nova" therefore covers the comparison only.
- **Q44 A:** the underheat and saturation exits (PWM to continuous) are timed as thermo-nova times them:
  - the first qualifying sample starts the timer at 0 s (`sustained_runtime`, `helpers.py:51-56`);
  - there is no bail-out on a long gap;
  - underheat counts while flow <= requested + `UNDERSHOOT_MARGIN_CELSIUS` (-3.0, `cycles/const.py:11`) and resets on `>` (`heating_control.py:337-340`); its timer is at `:342-345`, 180 s (`const.py:22`);
  - saturation runs while off_time == 0 (`:359-362`); its timer is at `:364-367`, 300 s (`const.py:23`).
- **Q46 B:** the 60 s switch fires on the first control tick whose scheduled time is at least 60 s after the overshoot started. The few milliseconds by which the tick runs late no longer matter.

**Compared with thermo-nova and the firmware:**

- Firmware Q43: strict `flowTemp > setpoint + fOvershootMargin` (`SATcycles.ino:1025`, comment at `:1018-1019`).
- Firmware Q44:
  - `dt` is the time since the previous call (`:1046-1047`);
  - a gap of 0 or more than 60 s returns early (`:1054`);
  - underheat adds `dt` (`:1065`) and uses a strict `<` (`:1064`);
  - saturation adds `dt` (`:1081`);
  - the constants are at `:30` and `:31`; entry 41 cites `:31`/`:32`, which have drifted.
  - Both exits run only in the PWM ON phase (`:1059-1063`), as thermo-nova (`heating_control.py:321-323`).
- Firmware Q46:
  - the overshoot start and the 60 s test read `millis()` inside the tick (`:1001`, `:1028-1029`);
  - control ticks run on a catch-up grid (`SATcontrol.ino:590`; `safeTimers.h:192-193`, `timer_due += timer_interval`);
  - `safeTimers.h` has no accessor for a timer's scheduled time. `DUE()` expands to `__Due__(timerName##_due, ...)` (`:161`). So after DUE fires, the scheduled time is `timerSATControl_due` minus the interval, and B needs a small helper;
  - after more than 10 missed intervals the grid restarts from now (`safeTimers.h:186-187`).
- thermo-nova runs its loop every 5 s (`climate.py:706-709`) and times the switch from `demand.timestamp` (`heating_control.py:293`).
- Not measured: the Q46 claim that the switch comes one tick late about half the time is derived from the code.
- Line check (question cite to current HEAD): `SATcycles.ino:993` is now `:1025`; `:1014-1022` is now `:1045-1054`; `:1032` is now `:1064`.

**Changes an earlier entry:**

- Entry 41, Open: settles bullet 2 (how underheat and saturation are counted) and bullet 3 (the strict `>`).
- Entry 22: settles the strict `>` point recorded there ("Firmware uses `>` where thermo-nova uses `>=`"). Consistent with his own wording quoted in entry 22, "BT - CS >= 2".
- Entry 37: settles the point it left open, that its `>=` answer did not decide the 60 s path. The classifier decision itself (`>=`, 2.0) is unchanged.

**Task:** a new follow-up task to TASK-1367 (Done), with Q47 (see that entry). Records the decisions TASK-1412.8 AC#1 asks for, except the Q46 scope below.

**Interpretation (ours):** Q46 asked about the 60 s switch only. Q44 A turns underheat and saturation into the same kind of timer, but whether B also applies to them is not his answer. Re-ask (see clarifications).

**Open:**

- The scope of Q46 B for the underheat and saturation timers.
- thermo-nova resets the exit timers during hot water and for 300 s after it (`heating_control.py:313-315`). The firmware's exits have no hot-water condition: in `satCycleCheckAutoSwitch()` `bDhwActive` is read only for the overshoot guard (`SATcycles.ino:1009`, `:1023`), not in the exits (`:1056-1090`). Not asked; Q44 covered the timing only.

## 87. Heat pumps enter PWM through the 60 s overshoot switch, as thermo-nova (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:00 UTC. Answer to Q45:
https://discord.com/channels/812969634638725140/1514720723980259460/1564866308317773845

**Quoted** (excerpt):

> Q45: A

**Settled by this statement:**

- The heat-pump exclusion from the continuous-to-PWM overshoot switch is removed. A heat pump in sustained overshoot enters PWM by itself, as thermo-nova does. thermo-nova's `_maybe_enable_pwm_on_sustained_overshoot` has no heat-pump case (`heating_control.py:262-305`).
- The PWM-to-continuous exits then apply to heat pumps too. `_maybe_disable_pwm_runtime` has no source case either (`heating_control.py:307-329`).
- Heat-pump PWM timing follows entries 40 and 50.

**Compared with thermo-nova and the firmware:**

- Firmware gate: `if (!satAlwaysMaxModulation() && pidOutput > satGetColdSetpoint())` (`SATcontrol.ino:5419`). The comment at `:5416-5418` names Q45.
- `satAlwaysMaxModulation()` is true only for `SAT_SRC_HEAT_PUMP` (`:376-380`). Hybrids already pass the gate.
- Heat pumps otherwise fall into `satCycleSkipAutoSwitch()` (`:5422`), whose comment lists the exclusion as open question Q45 (`SATcycles.ino:164-166`).
- Heat-pump PWM timing is already in place: 180 s minimum ON (`SATcontrol.ino:359-362`), period 1800 or 2400 s (`:367-373`).
- MM is never 0 for a heat pump or hybrid in PWM (comment at `:383-385`).
- Line check: question cite `:5313` is now `:5419`. `:376-380` is unchanged.

**Ordering (ours):** our advice was "A, after Q35", and he answered A.

- A heat pump that has not started within one control tick hits today's ON-phase flap (the Q35 entry).
- So this lands with or after the Q35 fix (TASK-1382).

**Changes an earlier entry:**

- Consistent with entries 40 and 50, which run heat pumps in PWM.
- Entry 13 (Force PWM removed) is not affected: this is the automatic switch.
- Same message: Q36 A (no hourly start cap for heat pumps; today 2 per hour at `SATcontrol.ino:349`) is recorded in its own entry and belongs in the same change.

**Task:** a new task. It depends on TASK-1382 and TASK-1412.3, and goes together with the Q36 change.

**Open:** none from him.

## 88. The 60 s overshoot switch runs before the CS of the same tick, as thermo-nova (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:07 UTC. Answer to Q47:
https://discord.com/channels/812969634638725140/1514720723980259460/1564868134287048788

**Quoted** (excerpt; the same message answers Q48-Q51 and asks for a new table column):

> @BlueFlame Bot Q47: A

**Settled by this statement:**

- The continuous-to-PWM overshoot switch runs before the control setpoint of the same tick is computed. The first PWM CS therefore goes out on the switch tick, not one tick (30 s) later.

**Compared with thermo-nova and the firmware:**

- thermo-nova's order in `update()`:
  1. disable PWM at the maximum setpoint (`heating_control.py:203-204`);
  2. the overshoot enable (`:206-209`);
  3. `pwm.update` (`:211-215`);
  4. the underheat and saturation exits (`:217-220`);
  5. modulation (`:222`);
  6. the PWM or continuous setpoint (`:224-228`);
  7. send (`:236-238`).
- Firmware order:
  1. the maximum-setpoint check (`SATcontrol.ino:5385-5388`);
  2. `satApplyPWM()` / `satApplyContinuous()` (`:5396`, `:5403`). `satApplyPWM` runs the schedule and the setpoint together (`:1102`);
  3. then `satCycleCheckAutoSwitch()` (`:5419-5420`), which holds both the overshoot enable and the PWM exits.
- Line check: question cites `:5297` / `:5314` are now `:5396`-`:5403` / `:5420`.

**Interpretation (ours):** the question named only the 60 s switch. Moving the whole `satCycleCheckAutoSwitch()` ahead would be wrong: the saturation exit reads the off time recorded by this tick's `satApplyPWM()` (`SATcycles.ino:1079-1080`), so it would read the previous tick's value.

- We read A as: only the continuous-to-PWM part moves before the CS.
- thermo-nova also runs the exits before the setpoint (`heating_control.py:217-228`). Matching that as well needs the schedule step split from the setpoint step. That is beyond the question, so it is open.
- These code facts change when the switch moves into the same tick: the CH-rule capture of the PWM phase (`SATcontrol.ino:5398-5401`) and the note that PWM's first schedule step comes one tick later (`:1179-1184`).

**Changes an earlier entry:** none. Consistent with entry 44 (CS, MM and CH on every update) and entry 41 (60 s).

**Task:** the same follow-up task as Q43/Q44/Q46. TASK-1412.8 AC#1.

**Open:** whether the PWM-to-continuous exits must also come before the CS, as in thermo-nova (`heating_control.py:217-228`). Not asked. The plan states the choice.

## 89. PWM-entry ON counts but is never cut; a hot-water flame is not counted (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:07 UTC, answer to Q48 (TASK-1369/1401, 2026-10-26). A provisional maintainer decision was in force:
https://discord.com/channels/812969634638725140/1514720723980259460/1564868134287048788

**Quoted:**

> Q47: A / Q48: A but do not count flame when DHW enable is triggered / Q49: B / Q50: A / Q51: A / Create a new column in the open questions table with a mark for the answered questions for better traceability

He also wrote about hot water on 2026-10-27 at 18:37 UTC (msg 1564709639944151071); that report is recorded as its own entry below (after hot water the next PWM ON comes a full off time late).

**Settled by this statement:**

- Q48 A: when the PWM-entry ON phase is the 4th start of the hour, it counts, but the limit never cuts the burner.
  - This confirms the provisional maintainer decision of 2026-10-26.
  - It is implemented at `SATcontrol.ino:1059-1063`, where `!(entryOn && flame)` skips the limit.
  - thermo-nova counts that start at `pwm.py:160`.
- Addition: "do not count flame when DHW enable is triggered". A flame that belongs to a hot-water draw never counts as a PWM start.

**Interpretation (ours; to confirm, see Q48b):** "DHW enable" means the boiler's DHW-active bit (MsgID 0 slave LB bit 2, spec v4.2 line 1445). It does not mean the thermostat's DHW-enable bit (master HB bit 1, line 1433). Reasons:

- thermo-nova's own name for hot water is "DHW enable". In its MQTT coordinator, `DATA_DHW_ENABLE = "domestichotwater"` (`coordinator/mqtt/opentherm.py:22`) drives `hot_water_active` (`:68-69`). This firmware publishes `domestichotwater` from slave LB bit 2 (`OTGW-Core.ino:2514`). The serial coordinator uses `DATA_SLAVE_DHW_ACTIVE` (`coordinator/serial/__init__.py:58-59`).
- The master bit is published separately, as `dhw_enable` (`OTGW-Core.ino:2469`).
- In entry 58 he used the same words, "when the DHW is enabled ... When DHW enable is off", for hot water running.
- The firmware's `bDhwActive` is the slave bit (`SATcontrol.ino:5089`).

**Where the firmware can count a hot-water flame today:**

- During the draw nothing is counted. While the slave DHW bit is set, the control loop returns early (`SATcontrol.ino:5096-5113`), before `satApplyPWM()` (`:5396`) and so before `satPwmSchedule()` (`:1102`).
- After the draw ends with the flame still lit, there are two ways it gets counted:
  - (a) an ON phase that was already requested is counted at its first flame (`SATcontrol.ino:971`);
  - (b) an OFF wait that ends while the flame burns starts an ON phase and counts it at once (`:1076`, then `:872-873`).
- The comment at `SATcontrol.ino:852-855` says outright that "a flame lit by hot water during a requested ON phase counts the same way, as in thermo-nova, pwm.py:186-193". Q48 reverses that.
- In thermo-nova an ON start counts at `pwm.py:160`, whatever lit the flame. The exclusion is therefore a deliberate deviation from thermo-nova.

**Interaction with Q49 = B (same message):**

- Counting moves from the first flame to the ON start, as in thermo-nova (`pwm.py:160`).
- The DHW exclusion must attach to that new counting point (`satPwmBeginOn`, `SATcontrol.ino:867-885`), not to `:971`.
- Under B, an ON start right after a draw, with the hot-water flame still lit, is exactly the case to exclude.

**Code path behind the OFF wait after hot water** (confirmed independently by the capture analysis of gruijter's alpha.514 run, see the hot-water report entry below):

- The flame-edge handler runs on every loop, before the DHW return (`SATcontrol.ino:5051-5056` vs `:5096-5113`).
- So a hot-water flame-on sets `_cycle_flameOnStartMs` (`SATcycles.ino:741`), and its flame-off sets `_cycle_flameOffStartMs` (`SATcycles.ino:786-789`).
- The PWM OFF wait counts from that flame-off (`SATcontrol.ino:1046`). After a draw, the next ON phase therefore waits a full off time from the end of the draw. At low duty that is up to 2 x period - 180 s (`:946`, `:996`, `:1006`).
- thermo-nova times the OFF wait from `_last_update` (`pwm.py:134`, `:155`, `:208`). A hot-water flame does not move that value.
- Entry 58 says `pwm.update()` keeps running while hot water is active (`heating_control.py:211`). Whether `demand.heater_state == HeaterState.ON` at `:202` holds during hot water was not traced.

**Changes an earlier entry:**

- Entry 59 settled that hot-water flame starts do not count. This entry extends that to a hot-water flame that carries into a PWM ON phase.
- It reverses the thermo-nova-parity reading in the TASK-1401 code comment (`SATcontrol.ino:852-855`). That is a firmware comment, not an entry.
- The PWM-entry rule (entries 22/43 with entry 59) is no longer provisional.

**Task:** a TASK-1401 follow-up for the DHW exclusion, planned together with the Q49 = B change. The Q48 code stays.

**Open:**

- Q48b: which bit he means, and whether a flame lit for hot water that continues into a PWM ON phase counts. Our reading above is acted on meanwhile.
- The OFF-wait restart after hot water: confirmed, see the hot-water report entry below (TASK-1380, TASK-1382).
- His table request (a column marking answered questions in `docs/sat/open-questions.md`) belongs to the cluster that edits that file.

## 90. A PWM ON phase counts against cycles per hour at its ON start, as thermo-nova (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:07 UTC. Answer to Q49 (queued after the TASK-1401 review; provisional A was in force). Our advice was "A until Q35 is fixed, then B".
https://discord.com/channels/812969634638725140/1514720723980259460/1564868134287048788

**Quoted** (excerpt):

> Q49: B

**Settled by this statement:**

- Every PWM ON start counts against cycles per hour at the moment it starts, also an ON phase that never lights. This is thermo-nova's behaviour: `self._current_cycle += 1` at the OFF/IDLE-to-ON start (`pwm.py:159-162`, the increment at `:160`).
- It replaces the provisional A of TASK-1401, under which a phase counted only at its first flame.

**Compared with thermo-nova and the firmware:**

- Firmware today:
  - `satPwmCountStart()` counts once per phase at its first flame (`SATcontrol.ino:856-861`, comment at `:851-855`, called at `:971`);
  - a phase that starts with the flame already burning is counted at once (`:872-873`);
  - an unlit phase is not counted (the comment at `:854-855`);
  - the ring and the limit are at `SATcycles.ino:285-326`;
  - the limit is applied at `SATcontrol.ino:1063`.
- thermo-nova's own cap compares the count with `duty_cycle_seconds` (`pwm.py:156`). That value is `"00:13:00"` = 780 by default (`entry_data.py:220`, `const.py:167`), so the cap never blocks. Entry 59 keeps the real cycles-per-hour limit.

**Ordering dependency (ours, stated in our own question):** B is safe only after Q35 is fixed.

- With today's anchor, an ON phase that has not lit within one tick ends on the next tick (`SATcontrol.ino:883`, `:1024`, `:1028`, `:1030`) and the following tick starts ON again (`:1058`, `:1076`). So an unlit start comes every second tick.
- Under B, each of those starts counts. At a 30 s tick and the default of 3 per hour (entry 52; `SATtypes.h:549`), the starts at about 0, 60 and 120 s reach the limit (`:1063`). PWM starts are then blocked for up to an hour, until the oldest start leaves the rolling window (`SATcycles.ino:297-306`). Derived from the code, not measured; our question said "about 90 s", the code walk gives about 2 minutes.
- So B ships in the same build as the Q35 fix (TASK-1382), or after it. Provisional A stays until then.
- This orders the work. It does not change his answer: the end state is B.

**Changes an earlier entry:** consistent with entries 58 and 59: only PWM's own ON phases count. B decides when they count. It also changes the TASK-1401 behaviour that was provisional.

**Task:** a new follow-up task to TASK-1401. It depends on TASK-1382. See TASK-1412.9 AC#2.

**Open:** the interaction with Q48, answered in the same message: "A but do not count flame when DHW enable is triggered" (recorded in its own entry).

- The firmware comment at `SATcontrol.ino:852-854` counts a flame lit by hot water during a requested ON phase.
- Under B the count moves to the ON start, so a hot-water flame no longer decides it.
- How his qualifier maps onto counting at the ON start belongs to the Q48 entry. Not resolved here.

## 91. PWM phase anchors when the flame is already burning: ON counts from its start, OFF from the ON-to-OFF switch (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:07 UTC. Answers to Q50 and Q51 (queued after the TASK-1401 review; provisional A was in force for both):
https://discord.com/channels/812969634638725140/1514720723980259460/1564868134287048788

**Quoted** (excerpt):

> Q50: A
> Q51: A

**Settled by this statement:**

- **Q50 A:** an ON phase that starts with the flame already burning counts its ON time from that start. This covers duty range 5 and PWM entry. Source: `pwm.py:162-167`; expiry at `:205-209`.
- **Q51 A:** if the flame is still lit on the first tick after ON to OFF, the OFF wait counts from the ON-to-OFF switch. In thermo-nova, `_last_update` is set at `pwm.py:208`, and OFF uses `elapsed` from it (`:134`, `:155`).

**Compared with the firmware:** both answers confirm what TASK-1401 already implements.

- Q50: `_pwm_onStartMs` (`SATcontrol.ino:815`), set at `:872-877`, used at `:1024`.
- Q51: `_pwm_offStartMs` (`:817`), set at `:896`, used at `:1050-1053`.
- After the Q35 fix (TASK-1382), Q50's rule becomes part of the general ON anchor: time from the phase start, re-anchor at ignition.

**Changes an earlier entry:** none. Consistent with entries 43 and 59.

**Task:** TASK-1401 (confirmation, no change). The "provisional" marks go.

**Open:**

- Q51 covers only the case where the flame is still lit at the switch. thermo-nova always times OFF from the switch (`pwm.py:208`). The firmware otherwise times it from the last flame-off (`SATcontrol.ino:1046`; the switch time is used only while the flame is lit, `:1050-1053`), so a later flame-off from hot water restarts the wait.
- That general case is TASK-1380 (audit #59), and it was not asked.
- Related reports, recorded nowhere yet:
  - gruijter, 2026-10-27 12:18 UTC, on alpha.514: "after 12:30, when DHW is used multple times in short bursts, it does take longer for CH to come back again" (https://discord.com/channels/812969634638725140/1514720723980259460/1564614286351794217).
  - Sergeant D, 2026-10-27 18:37 UTC: "It looks like when DHW enable is triggered the next PWM cycle resets. It has a pattern of 20 minutes idle after DHW enable goes to 0. PWM cycle = 20 minutes" (https://discord.com/channels/812969634638725140/1514720723980259460/1564709639944151071). gruijter replied at 18:44 asking us to verify and fix (message 1564711480912388097).
  - This fits an OFF wait timed from the hot-water flame-off. That is our reading; we have not checked it against gruijter's capture.
  - See the clarification question.

## 92. After hot water the next PWM ON comes a full off time late (report, 2026-10-27)

**Source:** #alpha-testing, 2026-10-27. Sergeant D, 18:37 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564709639944151071
(with a graph at 18:30 UTC, msg 1564707985186488490). gruijter (tester, Intergas, PIC board, alpha.514), 12:18 UTC with
capture `20261027-093222.zip` and `otgw-graph-20261027-131309.csv`:
https://discord.com/channels/812969634638725140/1514720723980259460/1564614286351794217
and 18:44 UTC: https://discord.com/channels/812969634638725140/1514720723980259460/1564711480912388097

**Quoted:**

> It looks like when DHW enable is triggered the next PWM cycle resets. It has a pattern of 20 minutes idle after DHW enable goes to 0. PWM cycle = 20 minutes

gruijter:

> DHW usage does not seem to interfere anymore with CH cycles. Although after 12:30, when DHW is used multple times in short bursts, it does take longer for CH to come back again, even when the room temperature drops a bit.

> That would indeed explain what happened when DHW was used multiple times after 12:30 CET @BlueFlame Bot can you verify and fix if needed?

**Checked (capture analysis, 2026-10-28; times CET = UTC+1):**

- Settings: cycles per hour 3, so the period is 1200 s (his "20 minutes"). Duty 0.21-0.24 at the time.
- PWM went OFF at 12:18:24, CH flame off 12:18:32. Five hot-water draws followed: 12:26:35-12:26:54, 12:29:05-12:29:32,
  12:30:02-12:30:47, 12:38:49-12:39:39, 12:48:19-12:48:28. PWM ON came at 12:57:54 (CH flame 12:58:34), room 19.75 for
  setpoint 20.
- Without the draws the OFF wait (912-948 s at that duty) would have ended about 12:34:24: CH came back about 23.5 min late.
  Morning case: draws 09:56:55-09:58:41, ON 10:14:24 instead of about 10:11:54.
- The wait after the last draw was 9 min 26 s, not 20 min: it is the off time, restarted at each hot-water flame-off. His
  "20 minutes" is the period; the mechanism he describes is confirmed.
- The hourly start limit was not involved: no "cycle limit" lines, and the count dropped 3 -> 2 -> 1 in that window.

**Cause in the firmware (two readers: the capture analysis and the Q48 verification, independently):**

- The OFF wait counts from the last flame-off of any kind (`SATcontrol.ino:1046`, `satCycleGetFlameOffStartMs()`); the
  flame-off time moves on every flame-off, hot water included (`SATcycles.ino:786-789`). `_pwm_offStartMs`
  (`SATcontrol.ino:1050-1053`) is used only when the flame still burns at the ON->OFF switch.
- A second path: an ON phase that starts with the flame off has no start anchor (`:883`) and falls back to the last flame-on
  (`:1024`), here the hot-water ignition, so it ended on its first tick (CS=48 at 10:14:24, CS=10 at 10:14:54, CS=48 at
  10:15:24, flame 10:15:35). This is the Q35 path.
- thermo-nova times the OFF wait from `_last_update` (`pwm.py:134`, `:155`), set only at an ON start (`:162`), at ignition
  (`:188`) and at ON->OFF (`:208`); no flame-off moves it.

**Settles:** his observation is a firmware defect against thermo-nova and against entry 43 (the OFF wait timed from flame-off
is not the design) and decided by Q35 A; Q51 A (above) covers the flame-still-lit case, and whether the OFF wait always
counts from the ON->OFF switch is the Q51 scope question. **Tasks:** TASK-1380 (OFF from PWM's own ON->OFF
switch) and TASK-1382 (ON from its own start, Q35), with this capture as the reproduction; TASK-1412.7 (post-DHW recovery).

**Open:** none for him; the fix is decided. The proof is an old-vs-fix replay of these draws (host test, then bench).

## 93. PWM ON and OFF seconds in the SAT log (request repeated, 2026-10-28)

**Source:** #alpha-testing, 2026-10-28 04:41 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564861520494461061

**Quoted:**

> @BlueFlame Bot In the next build, add pwm_on (seconds) and pwm_off (seconds) to the SAT log.

**Settles:** the same request as entry 61, now with the field names pwm_on and pwm_off. Logging only, no control change.
**Task:** TASK-1407 (field names added). **Open:** none.

## 94. Mark answered questions in the open-questions table (process request, 2026-10-28)

**Source:** #alpha-testing, 2026-10-28 05:07 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564868134287048788

**Quoted:**

> Create a new column in the open questions table with a mark for the answered questions for better traceability

**Settles:** `docs/sat/open-questions.md` gets an "Answered" column: a mark plus the entry number for every answered
question, empty for open ones. Done in the same commit as this entry. **Open:** none.

**Tasks filed 2026-10-28 for entries 67-94:** TASK-1413 (entry 72, auto-tune removal), TASK-1414 (entry 73), TASK-1415 (entry 74), TASK-1416 (entry 75), TASK-1417 (entry 76), TASK-1418 (entry 77), TASK-1419 (entries 81, 87), TASK-1420 (entries 86, 88), TASK-1421 (entries 89, 90). Existing tasks got notes: TASK-1270, 1355, 1365, 1371, 1373, 1375, 1380, 1382, 1383, 1407, 1412.7-1412.10.

## 95. PID rounding, Ki clamp, first-tick D, tick timing scope, and hot water during a PWM ON phase (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 07:22 UTC, answers to the follow-ups Q28a, Q28b, Q39b, Q46b and Q48b (entries 78, 83, 86, 89):
https://discord.com/channels/812969634638725140/1514720723980259460/1564902129896792156

**Quoted:**

> Q28a: Round all the values to 3 decimals not only the Output.
>
> Q28b: Yes, match thermo-nova
>
> Q39b: D = 0
>
> Q46b: Yes
>
> Q48b: Boiler DHW active.
>
> Do not count the flame when the Boiler DHW is active.
>
> So when we are in PWM ON phase and the Boiler DHW active is triggered, pause the PWM ON seconds counter and resume when the DHW active trigger goes to OFF. During this time you don't need to send the CS=10, CH=0 commands to the boiler. Keep sending the PWM ON values to the boiler. We need to just pause the PWN ON timing. So when CH heating is resumed and CH flame is on resume the PWM ON timing.
> Note: When the DHW active goes to off and we are resume the PWM ON cycle probably the flame will be off. So you need to perform the start-up sequence to lit the fire

**Settled:**

- **Q28a:** every PID value is rounded to 3 decimals, the output included. thermo-nova rounds the gains to 6 decimals (`pid.py:71`, `:82`, `:93`), P, I, D and the raw derivative to 3 (`pid.py:101`, `:106`, `:114`, `:119`) and the output to 1 (`pid.py:132`). His rule is a deliberate deviation for the gains (3 instead of 6) and the output (3 instead of 1).
- **Q28b:** the Ki-near-0 integral clamp matches thermo-nova (entry 78).
- **Q39b:** on the first tick after boot or SAT enable, D = 0 (entry 83).
- **Q46b:** timing on the scheduled tick time applies to the other sustained timers too (underheat, saturation; entry 86).
- **Q48b, the bit:** "DHW" in entry 89 is the boiler's DHW-active bit (MsgID 0 slave status bit 2), as we read it. A flame while that bit is set is never counted as a PWM start.
- **Q48b, the ON phase:** while the boiler's DHW-active bit is set during a PWM ON phase, the ON-time counter pauses. SAT keeps sending the PWM ON values (no CS=10, CH=0). When DHW active drops, the ON phase resumes; the flame is then probably off, so the start-up sequence runs again to light it (the ignition wait and, without a return temperature, the entry 45 flood), and the ON timing resumes once the CH flame is on.

**Compared with thermo-nova:** thermo-nova has no hot-water pause: `pwm.update()` runs whenever the heater state is ON (`heating_control.py:202-215`), and the heater state has no hot-water term (`heating_control.py:64-72`), so its ON timer runs on during a draw. Pausing the ON timer is his deviation, recorded here.

**Compared with the firmware (alpha.515, TASK-1382):** the control loop returns early while the DHW bit is set (`SATcontrol.ino` ~5096-5113): no CS=10 is sent, but the ON timer's anchor keeps aging, so an ON phase can end at the first tick after a draw. That is the TASK-1422 case; its scope changes from "keep the clock running as thermo-nova" to "pause the ON timer during the draw, then restart the start-up sequence".

**Interpretation (ours):** "resume the PWM ON timing when the CH flame is on" means the paused ON phase becomes a waiting ON phase again after the draw (the 180 s ignition wait of entry 80 applies), and the remaining ON time continues from the ignition. He said nothing about an OFF phase during a draw; Q51b (entry 97) keeps the OFF wait timed from the ON->OFF switch.

**Tasks:** Q28a/Q28b -> TASK-1423 (new); Q39b -> TASK-1270; Q46b -> TASK-1420; Q48b count -> TASK-1421; Q48b ON pause -> TASK-1422 (scope rewritten). **Open:** none.

## 96. Cold cutoff boundary: SAT idles when the requested setpoint is below the cold setpoint (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 07:29 UTC, answer to Q20b (entry 71):
https://discord.com/channels/812969634638725140/1514720723980259460/1564904039643357215

**Quoted:**

> Q20: Cold cut-off boundary is the value that keeps SAT at idle without taking any action when the requested setpoint is below that value.

**Interpretation (ours, acted on):** idle when the requested setpoint is strictly below the cold setpoint (28.2 radiators / 21 underfloor); a request exactly equal to it still heats (`<`).

**Compared with thermo-nova and the firmware:** thermo-nova treats equality as cold: the heater is ON only when `requested_setpoint > COLD_SETPOINT` (`heating_control.py:72`), and the PWM override returns on `<=` (`:385`). The firmware does the same today: `satColdCutoff()` returns `requestedSetpoint <= coldSetpoint` (`SATchEnable.h:26-29`, used at `SATchEnable.h:49` and `SATcontrol.ino:5431`). His rule is a deviation at exactly the boundary.

**Changes an earlier entry:** entry 12's "at the cold setpoint or below" bullet; this entry replaces it with "below".

**Task:** TASK-1355 / ADR-199 (boundary), and a code change of `satColdCutoff` and the matching compares (entry 71 lists them). **Open:** confirmation of our reading is offered in the reply; acted on meanwhile.

## 97. Q51b: follow thermo-nova, the OFF wait always counts from the ON->OFF switch (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 07:33 UTC:
https://discord.com/channels/812969634638725140/1514720723980259460/1564904986037715015

**Quoted:**

> Q51b: Follow thermo-nova

**Settled:** the PWM OFF wait always counts from the ON->OFF switch, also when the flame is already off (`pwm.py:134`, `:155`, `:208`); hot-water and other flame-offs never move it. Confirms entries 43, 91 and 92. **Task:** TASK-1380 (implemented in alpha.515, commit de23e3ea1). **Open:** none.

## 98. Cold cutoff boundary: at or below the cold setpoint (2026-10-28, corrects entry 96)

**Source:** #alpha-testing, 2026-10-28 07:53 UTC, reply to our reading of Q20 (msg 1564909748946731029: "SAT idles when the requested setpoint is below 28.2 / 21, so exactly 28.2 still heats ... Tell me if you meant 'at or below'"):
https://discord.com/channels/812969634638725140/1514720723980259460/1564909937161805966

**Quoted:**

> @BlueFlame Bot at or below

**Settled:** SAT idles when the requested setpoint is at or below the cold setpoint (28.2 radiators / 21 underfloor): equality is idle (`<=`). This corrects our interpretation in entry 96 and agrees with thermo-nova (`heating_control.py:72` heater ON only when `requested_setpoint > COLD_SETPOINT`; `:385` returns on `<=`), with entry 12's "at the cold setpoint or below", and with the firmware today (`satColdCutoff()` returns `requestedSetpoint <= coldSetpoint`, `SATchEnable.h:26-29`).

**Changes an earlier entry:** entry 96's interpretation (strict `<`) is withdrawn; entry 12 stands. No code change for the boundary.

**Task:** TASK-1355 / ADR-199 (record the boundary as `<=`). **Open:** none.

## 99. Gas boiler cycles per hour: 3 or 4 only, default 3 (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 07:54 UTC, answer to the expanded Q16 (msg 1564909750683045899):
https://discord.com/channels/812969634638725140/1514720723980259460/1564910294709571667

**Quoted:**

> @BlueFlame Bot Q16: A

**Settled (option A as asked):** for gas boilers the user may choose only 3 or 4 PWM cycles per hour, default 3, for radiators and underfloor alike. A stored 2 becomes 3; a stored 5 or 6 becomes 4. Hybrids follow (entry 82, gas timing). Heat pumps are not covered (entries 50, 52, 81).

**Compared with thermo-nova and the firmware:** thermo-nova offers 3 or 4 only for radiators (`config_flow.py:695-702`), default 4 (`const.py:119`). The firmware accepts 2 to 6 (`settingStuff.ino:1107`), default 3 (`SATtypes.h:549`), and its out-of-range fallback returns 4 for radiators (`SATcontrol.ino:354`, entry 82). Default 3 and underfloor's 3/4 choice are his deviations from thermo-nova, consistent with entries 30 and 52.

**Changes an earlier entry:** confirms our provisional reading of entry 52 (range 3 or 4).

**Task:** TASK-1375. **Open:** none.

## 100. MaxTSet slider: bounds from MsgID 49, always send the value, SAT caps CS itself (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 08:28 UTC, answer to the expanded Q41 (msg 1564916590674452491):
https://discord.com/channels/812969634638725140/1514720723980259460/1564918647338700851

**Quoted:**

> Q41: A

**Settled (option A as asked):** the MaxTSet slider takes its bounds from MsgID 49 (max CH setpoint upper/lower bound), with 20/80 when the boiler does not report it; the value is always sent to the boiler (SH=, MsgID 57) without checking MsgID 6; SAT also caps the CS it sends itself (entries 51, 54). Same pattern as the hot-water slider with MsgID 48 (entry 53).

**Compared with thermo-nova and the firmware:** thermo-nova uses a fixed 30-80 range for the maximum setpoint entity (`number.py:116-123`) and reads only MsgID 57 (`coordinator/mqtt/opentherm.py:18`); MsgID 49 bounds are a deviation he chose. The firmware has no user MaxTSet yet (entry 67). OT-Direct already writes MsgID 57 for SH= (`OTDirect.ino:143`).

**Changes an earlier entry:** clarifies entry 48's "MsgID 57 values (Lower/Higher)": the bounds come from MsgID 49, the value is MsgID 57.

**Task:** TASK-1373. **Open:** Q15b (underfloor default 55 against the 50 C ceiling, heat-pump default), queued.

## 101. Interpretation: an ON start right after a hot-water draw counts as a normal start (2026-10-28, our reading, asked as Q52 D)

**Source:** no new statement. This records the reading TASK-1421 acts on, after review found that entry 89 (our interpretation text, "Interaction with Q49 = B") called an ON start right after a draw, with the hot-water flame still lit, "exactly the case to exclude", while entry 95 settled the rule differently.

**His words (entries 90, 95):** Q49 B "count at the ON start, every start" (`pwm.py:160`), and Q48b "Do not count the flame when the Boiler DHW is active."

**Reading acted on (ours):** the hourly limit counts ON starts, not flames. No ON start can happen while the boiler DHW-active bit is set, because the PWM schedule does not run during a draw (`SATcontrol.ino` DHW early return). So no flame is ever counted, and "do not count the flame when DHW is active" holds. An ON start that happens at the first tick after a draw, with the flame still burning from hot water, is a normal ON start and counts once. He wrote that the flame will probably be off by then (entry 95).

**Replaces:** the exclusion sentence in entry 89's "Interaction with Q49 = B" paragraph, which was our interpretation, not his words. Entry 89 itself is not rewritten.

**Compared with thermo-nova:** the same: `pwm.py:160` counts every ON start, whatever the flame.

**Task:** TASK-1421 (implemented this way). **Open:** asked as Q52 D together with the TASK-1422 details.

## 102. The boiler's on/off-control bit (MsgID 3) does not force PWM (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 10:30 UTC, answer to the expanded Q42 (msg 1564924112340652083):
https://discord.com/channels/812969634638725140/1514720723980259460/1564949482477457409

**Quoted:**

> Q42: A

**Settled (option A as asked):** SAT ignores the MsgID 3 high-byte bit 1 (modulating vs on/off boiler) for control and keeps choosing PWM or continuous itself; an on/off boiler that overshoots reaches PWM through the 60 s overshoot switch (entries 41, 86). Consistent with entry 13 (Force PWM removed).

**Compared with thermo-nova and the firmware:** thermo-nova only exposes the bit as a sensor (`coordinator/serial/binary_sensor.py:51`). The firmware does not act on it for SAT control today. No change.

**Task:** none (no capability-forced PWM task is created). **Open:** none.

## 103. Hot water during a PWM ON phase: resume details A, B and C confirmed; D to expand (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 10:45 UTC, answer to Q52 (msg 1564951907665977416):
https://discord.com/channels/812969634638725140/1514720723980259460/1564953189969829993

**Quoted:**

> @BlueFlame Bot Q52: A B and C yes. Expand on D

**Settled (as asked, refining entry 95):**

- **A:** after a hot-water draw that interrupted a PWM ON phase, SAT waits up to 180 s, counted from the end of the draw, for the CH flame; if none comes, the ON timer resumes anyway (the entry 80 rule).
- **B:** when the CH flame is back, the ON phase continues with the ON time that was still left: no new 180 s minimum burn and no new boiler-temperature measurement (the flow is still hot from the draw), the same as a relight in thermo-nova (`pwm.py:144-146`).
- **C:** if the flame is still burning at the first moment after the draw, the ON timer resumes at once.

**Compared with thermo-nova:** the pause itself is his deviation (entry 95; thermo-nova runs `pwm.update()` during hot water, `heating_control.py:202-215`); B follows thermo-nova's relight handling.

**Not settled:** D (an ON start right after a draw with the hot-water flame still lit counts as a normal start, our entry 101 reading); he asked us to expand. TASK-1421 counts it meanwhile (entry 101).

**Task:** TASK-1422 (A, B, C: implementation unblocked); D -> TASK-1421 follow-up depending on the answer. **Open:** D.

## 104. Interpretation: the hot-water resume rules apply to every interrupted ON phase, also one still waiting for its first ignition (2026-10-28, our reading)

**Source:** no new statement. Review of the TASK-1422 implementation (2026-10-28) found two cases entries 95 and 103 do not name: a draw during an ON phase that is still waiting for its first CH ignition, and a short draw inside the 180 s start-up window after an ignition.

**His words (entry 103 B):** "When the CH flame is back, the ON phase continues with the ON time that was still left. No new 180 s minimum burn and no new boiler-temperature measurement (the flow is still hot from the hot water)".

**Reading acted on (ours):** the reason he gave (the flow is hot after a draw) holds for every ON phase a draw interrupts. So after any draw:
- the phase resumes with the ON time it had used before the draw (A: up to 180 s from the draw end for the CH flame, then the timer runs; C: flame still lit, resume at once);
- the first CH flame after the draw is not treated as the phase's ignition: no new ON-time anchor, no new boiler-temperature seed, no new 180 s minimum burn;
- a start-up window (EMA of the boiler temperature, `pwm.py:148-149`) that the draw interrupted is closed; the value from before the draw is kept.

Without this the DHW flame or the first CH flame after the draw would anchor a full ON time and seed the boiler temperature from the hot flow (`pwm.py:186-193` first-ignition sync).

**Compared with thermo-nova:** thermo-nova has no hot-water pause at all (entry 95); its first-ignition sync (`pwm.py:186-193`) would fire on whatever flame comes first. This reading extends his deviation consistently.

**Task:** TASK-1422. **Open:** offered to him in the progress note; acted on meanwhile.

## 105. Q52 D: an ON start right after a draw with the hot-water flame still lit does not count (2026-10-28, corrects entry 101)

**Source:** #alpha-testing, 2026-10-28 10:51 and 10:58 UTC, answers to the expanded Q52 D (msg 1564954297135796305: A counts as thermo-nova, B not counted because the flame was lit for hot water, C no start while that flame burns, start later and count):
https://discord.com/channels/812969634638725140/1514720723980259460/1564954645489782858
https://discord.com/channels/812969634638725140/1514720723980259460/1564956502849093675

**Quoted:**

> @BlueFlame Bot Q52D: No

> @BlueFlame Bot Q52: D No

**Settled:** our reading in entry 101 (such a start counts, option A) is rejected: an ON start at the first tick after a draw, with the flame still burning from hot water, must not count against cycles per hour. This is his deviation from thermo-nova (`pwm.py:160` counts every ON start).

**Interpretation needed:** "No" rejects A but does not choose between B (the start happens but is not counted; SAT remembers that the burning flame was lit during DHW) and C (no ON start while that hot-water flame burns; the start comes after it goes out and is counted). Asked as Q52E.

**Changes an earlier entry:** entry 101 (our interpretation) is withdrawn.

**Conflict with shipped code:** TASK-1421 (alpha.517, alpha.518) counts this start. A follow-up task changes it once Q52E is answered.

**Task:** TASK-1424 (new, after Q52E). **Open:** Q52E (B or C).

## 106. Q52E: no PWM ON start while the hot-water flame still burns; start after it goes out, counted (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 11:23 UTC, answer to Q52E (msg 1564961956140421181):
https://discord.com/channels/812969634638725140/1514720723980259460/1564962902509752442

**Quoted:**

> @BlueFlame Bot Q52E:  C

**Settled (option C as asked):** when the PWM OFF wait ends while the flame is still burning from a hot-water draw (the boiler DHW-active bit already off), PWM does not start the ON phase yet; it waits until that flame has gone out, then starts normally, and that start counts against cycles per hour (entries 90, 105). Our advice was B.

**Compared with thermo-nova:** thermo-nova starts the ON phase at once and counts it (`pwm.py:155-167`); this hold is his deviation.

**Interpretation (ours, acted on):** "the hot-water flame" is a flame that was lit while the boiler DHW-active bit was set; a CH flame (e.g. the PWM-entry ON, entry 43) is not held. The hold ends when the flame goes out, or when the DHW bit returns (then the normal draw rules apply). Duty range 5 (continuous ON) follows the same hold.

**Task:** TASK-1424. **Open:** none.

## 107. Interpretation: the hot-water-flame hold applies only while PWM drives the setpoint (2026-10-28, our reading)

**Source:** no new statement. Review of the TASK-1424 implementation of entry 106 (2026-10-28).

**His words (entry 106, Q52E C):** "No PWM ON start while the hot-water flame still burns: PWM waits until the flame has gone out, then starts normally and that start counts."

**Reading acted on (ours):** the hold applies while PWM actually switches the burner, i.e. while the PWM setpoint override is active (overshoot protection on, no hot water, requested setpoint above the cold setpoint; `satPwmDrivesSetpoint`, as thermo-nova `heating_control.py:385`). Then SAT sends CS=10/CH=0 in the OFF phase, so the hot-water flame goes out and the held start follows. Without the override SAT keeps sending the previous setpoint with CH on, a combi boiler can carry the hot-water flame straight on as heating, and a hold would never end: PWM would stop counting starts while the burner runs. So without the override there is no hold.

Also (review, same task): a hot-water flame is marked at its ignition when the boiler DHW-active bit is set, not only on a control tick, so draws shorter than one tick are caught; a flame that entry 103 C resumes as the ON phase's CH flame is no longer marked.

**Compared with thermo-nova:** thermo-nova has no such hold (entry 106).

**Task:** TASK-1424. **Open:** offered to him in the progress note.

## 108. Interpretation: the hot-water guard covers the PWM exits too, during the draw and for 300 s after it, as thermo-nova (2026-10-28, our reading)

**Source:** no new statement. Plan review of TASK-1420 (entries 86, 88, 95), 2026-10-28.

**What his answers leave open:** Q44 A (entry 86) times the underheat and saturation exits as thermo-nova, from the first qualifying sample and without a bail-out on a long gap. Entry 86 Open names the thermo-nova hot-water reset of those exits (`heating_control.py:313-315`) as not asked.

**Why it cannot stay as it is:** the old firmware added the time since the previous call and skipped gaps above 60 s (`SATcycles.ino:1054`), so a draw, during which the control loop returns before the exits (`SATcontrol.ino` DHW branch), was never counted. With a start timestamp and no bail-out, an exit timer started before a draw would count the whole draw and could fire on the first tick after it.

**Reading acted on (ours):** port thermo-nova's guard as written:
- the exits reset their timers while the boiler DHW-active bit is set and for 300 s after it (`heating_control.py:313-315`, `DHW_OVERSHOOT_GUARD_SECONDS` = 300, `const.py:19`);
- the 300 s overshoot guard after a draw (`heating_control.py:272-274`) is made to work. The firmware refreshed its end time only inside a function that is never reached during a draw (`SATcycles.ino:1011`; audit 2026-10-24 #11), so it never armed.

**Consistent with:** entry 22, which already cites this guard as matching thermo-nova. The guard matched only on paper (audit #11).

**Task:** TASK-1420. **Open:** offered to him in the progress note.

## 109. Correction of our own premise in Q46 and entry 86: the control timer skips missed ticks, it does not catch up (2026-10-28, our finding)

**Source:** no new statement. Correctness review of TASK-1420, 2026-10-28.

**What we claimed:** Q46 and entry 86 (Q46 comparison bullets) said control ticks run on a catch-up grid (`SATcontrol.ino:590`; `safeTimers.h:192-193`), so the tick 60 s after the start often measures 59 99x ms and the switch comes one tick late. Q46c (queued, not posted) built on the same premise.

**What the code does:** `timerSATControl` is declared `CATCH_UP_MISSED_TICKS` (`SATcontrol.ino:582`), but `initSAT()` (called from `setup()`) and the SATinterval setting both re-set it with `CHANGE_INTERVAL_SEC(timerSATControl, settings.sat.iControlInterval)` without a type (`SATcontrol.ino:4277`, `settingStuff.ino:1005`), which makes it type 0, `SKIP_MISSED_TICKS` (`safeTimers.h:96`, `:131-135`). On each tick the next due time is now + interval (`safeTimers.h` SKIP branch). So two ticks are never closer than one interval: the tick two intervals after the first overshoot sample is always at least 60 s later, and the switch already comes on the first tick at least 60 s after the start. The "one tick late" case of Q46 did not occur in the firmware, and the catch-up burst of Q46c cannot occur. Measured on the host with the real `safeTimers.h` (review): after `initSAT` type=0; a tick 7 ms late gives due - interval = millis().

**Effect on his answers:** none. Q46 B (switch on the first tick at least 60 s after the start) and Q46b (the same for underheat and saturation) describe what the timer gives. TASK-1420 times all three timers on one tick time, taken once per control tick, as start timestamps (Q44 A). Q46c is withdrawn.

**Changes an earlier entry:** entry 86, the Q46 comparison bullets (catch-up grid, "about half the time one tick late"): wrong premise, corrected here.

**Task:** TASK-1420. **Open:** none; offered to him in the progress note.

## 110. Q15b B: keep the 50 °C underfloor ceiling; the underfloor default for max flow becomes 50 °C, as thermo-nova (2026-10-28, changes entry 67)

**Source:** #alpha-testing, 2026-10-28 19:06 UTC, answer to Q15b (msg 1565072816162873576):
https://discord.com/channels/812969634638725140/1514720723980259460/1565079331175268423

**Quoted:**

> Q15b: B

**The question (as posted):** the underfloor default of 55 °C (entry 67) against the firmware's fixed underfloor ceiling of 50 °C (`SATcontrol.ino:137`, applied at `:319`). A) drop the 50 ceiling, the user setting (default 55) is the limit, global 65 stays; B) keep the 50 ceiling, the underfloor default becomes 50, as in thermo-nova; C) default 55 and ceiling 50. Our advice was A.

**Settled:** the 50 °C underfloor ceiling (`SAT_HARD_MAX_FLOOR`) stays. The default maximum flow setpoint for underfloor is 50 °C, the value thermo-nova uses (`helpers.py:97-99`, `calculate_default_maximum_setpoint`: 50 underfloor, 55 otherwise). Radiators keep 62 °C (entry 67).

**Changes an earlier entry:** entry 67, the underfloor default (55 -> 50). The radiator default 62 is unchanged.

**Compared with the firmware:** the system cap for underfloor is 45 (`SATcontrol.ino:305`), below both the new default and the ceiling; how the user setting replaces that cap is TASK-1373's work (entries 51, 54, 100).

**Task:** TASK-1373. **Open:** the heat-pump default (entry 67 Open, not asked yet); whether SAT writes an unchosen default to the boiler (entry 67 Open, read with entry 100).

## 111. Q55: full thermo-nova for the 60 s PID timer: zones too, the PID keeps stepping when control is skipped; no room temperature keeps the heating curve (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 19:38 UTC, answer to Q55 (msg 1565082574374576272):
https://discord.com/channels/812969634638725140/1514720723980259460/1565087366996037693

**Quoted:**

> @BlueFlame Bot Q55: Full thermo-nova: zones get the 60 s timer too, and the PID keeps stepping in all of these cases. Only SAT off when OT loss. If no room temperature then continue heating with heating curve only as we previously agreed, so the home will stay warm.

**Settled (option A, with one change):**
- Zone (area) PIDs get their own 60 s timer, as thermo-nova `area.py:183-190`.
- The main PID steps on its own 60 s timer (entry 42) also while the control tick skips: hot water, boiler fault, thermal-safe fallback, summer simmer, valves closed.
- Without a room temperature the heating curve keeps heating, curve only (entries 38 and 46: no P term, P/I/D reset). This replaces "no room temperature stops it" in our option A.

**Interpretation (ours, acted on):** "Only SAT off when OT loss" reads as: the PID stops stepping only when SAT is off, and an OT loss turns SAT off (entry 79: SAT resumes by itself on the first boiler reply). Without a room temperature the PID step does not integrate (there is no error to integrate); the output is curve only as entries 38/46, so the home stays warm.

**Compared with thermo-nova:** zones and the independent timer as thermo-nova (`climate.py:712-716`, `area.py:183-190`); thermo-nova has no fault/fallback/simmer/valves paths, so stepping through them follows its timer model. Room temperature missing: thermo-nova resets the PID on a stale sensor (`climate.py:466-482`); his curve-only rule is entries 38/46.

**Task:** TASK-1368 (unblocked). **Open:** none.

## 112. Q53 A: continuous mode follows thermo-nova's previous-CS clamp (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 19:54 UTC and 20:04 UTC (repeated), answer to Q53 (msg 1565090124415639564):
https://discord.com/channels/812969634638725140/1514720723980259460/1565091338842669137
https://discord.com/channels/812969634638725140/1514720723980259460/1565093953013944431

**Quoted:**

> Q53: A

> Q53: A / Q54: Yes

**Settled:** in continuous mode, with the flame on and a valid flow, the CS follows thermo-nova `heating_control.py:485-489`: if the request rises above the previous CS, CS = max(request, flow - offset); otherwise CS = max(request, min(flow - offset, previous CS)). The previous CS is the last CS SAT sent: 10 after heater-off or the cold cutoff, 0 after SAT enable or boot (no restore after a reboot; option C not chosen). Firmware today: only max(request, flow - offset) (`SATcontrol.ino:1603-1610`).

**Interpretation (ours, acted on):** the firmware-only paths thermal-safe fallback and summer simmer write the CS they send as the previous CS, so the clamp never starts from a stale value (sub-point of Q53, our choice).

**Not settled:** "Q54: Yes" in the second message. Q54 had not been posted to him; it was only queued (open-questions.md). It asks which modulation value a heat pump gets, so "yes" does not answer it. Q54 is posted in full; this "yes" is not acted on.

**Task:** TASK-1381 (AC#2 now has his OK). **Open:** none for Q53.

## 113. Q54 B: a heat pump always gets the configured max modulation, in and outside PWM (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 20:23 UTC, answer to Q54 (msg 1565098261210079263):
https://discord.com/channels/812969634638725140/1514720723980259460/1565098699074576566

**Quoted:**

> @BlueFlame Bot Q54: B

**Settled:** for a heat pump the max relative modulation (MM) is always the configured "max modulation" setting, 100 when that is 0, both in and outside PWM. Never 0 (entry 21).

**Compared with thermo-nova and the firmware:** thermo-nova sends 0 in PWM for every boiler (`heating_control.py:139-140`, `:517-518`) and has no heat-pump case; his rule is a deviation, consistent with entry 21. Firmware today: 100 outside PWM, configured max inside PWM (`satComputeMM`, `SATcontrol.ino:379-388`); the outside-PWM branch changes. Hybrids are not part of this answer (they keep today's rule).

**Changes an earlier entry:** settles entry 21 Open ("100 % or the max modulation the user selects": the user's value).

**Task:** TASK-1426. **Open:** none.

## 114. Q32 C: remove the firmware-only comfort adjust; humidity is used only through Thermal comfort, as thermo-nova (2026-10-28)

**Source:** #alpha-testing, 2026-10-28 20:42 UTC, answer to Q32 (msg 1565102519879733351; raised by gruijter 2026-10-25):
https://discord.com/channels/812969634638725140/1514720723980259460/1565103411240902818

**Quoted:**

> Q32: C, follow thermo-nova

**Settled:** the firmware-only "comfort adjust" (a humidity offset on the target, `satUpdateComfort`, `SATcontrol.ino:3790-3800`, setting `bComfortAdjust`) is removed. Humidity is used only by Thermal comfort, where the PID uses the Summer Simmer Index instead of the room temperature, as thermo-nova `climate.py:276-277` (`SATcontrol.ino:1688-1700`). Our advice was A.

**Changes an earlier entry:** entry 32's "Humidity comes to play ONLY when summer simmer index is enabled" is now read as the Thermal comfort option.

**Task:** TASK-1282 (scope changes from "fix the gate" to "remove comfort adjust": the setting, its REST/MQTT/UI exposure and docs). **Open:** none; what happens to a stored `bComfortAdjust` value is an implementation detail (ignore it).

## 115. Q56 A: after a boot without a room reading yet, keep the restored PID state; a reading lost later still resets (2026-10-28, refines entry 46)

**Source:** #alpha-testing, 2026-10-28 21:39 UTC, answer to Q56 (msg 1565115722487832687):
https://discord.com/channels/812969634638725140/1514720723980259460/1565117854611939329

**Quoted:**

> Q56: A

**Settled:** as thermo-nova (`climate.py:480-482`: the PID update is skipped without a reset while no temperature is available yet). After a boot or SAT enable with a restored PID state, and no room temperature received yet since that boot, the PID waits: the restored integral and raw derivative are kept, nothing is saved over them, and the output is curve only until the first reading. A room temperature that was present and then goes missing still resets P, I and D (entry 46); a stale sensor resets as thermo-nova `climate.py:470-478`.

**Changes an earlier entry:** entry 46 (reset when the room temperature is lost or missing): "missing" no longer covers the time before the first reading after boot. Consistent with entry 83 (the restored I is used once a reading arrives).

**Task:** TASK-1368. **Open:** none.

## 116. Q18 A: zones follow thermo-nova's area.py aggregation as it is (2026-10-29, settles entry 35's open point)

**Source:** #alpha-testing, 2026-10-29 04:48 UTC, answer to Q18 (msg 1565223339390275625):
https://discord.com/channels/812969634638725140/1514720723980259460/1565225896418345072

**Quoted:**

> Q18: A

Option A as posted: "Port thermo-nova's zone logic as it is."

**Settled (our reading of a one-letter reply, against the posted option text):** the firmware's multi-zone aggregation is replaced by thermo-nova's. thermo-nova (`climate.py:372-392`): the primary PID output is kept and only raised, `setpoint = max(primary, areas output)` (`:385-386`); the areas output is the `int(0.75 * N)` percentile, so the highest output for N <= 4, capped at `baseline + 5.0` (`area.py:286-301`); a room more than 0.3 above its target sets a ceiling `22 + 10 - 4 x degrees over`, the minimum over rooms, floored at 22, applied as `min(setpoint, cap)` (`area.py:312-333`, `climate.py:388-389`); a room counts only with output above 22 or a valve at least 10 % open (`area.py:166-175`); ECO mode skips both steps (`climate.py:377-379`). The firmware (`SATcontrol.ino:5516-5566`) differs in eight ways: it replaces the primary output, uses `ceil(0.75N)-1`, adds the headroom on top, subtracts the largest overshoot with no 0.3 margin and floors at 10, applies no cap with one zone, counts zones above 10, and runs only with two or more zones, also in ECO. Two independent readers compared both sides (TASK-1340 audit, 2026-10-29); the citations were opened in that session.

**Changes an earlier entry:** entry 35 (port upstream PR #172 for multiple areas) left open whether PR #172 is in the pinned commit; this entry makes the pinned `area.py` the target regardless. Consistent with entry 35.

**Task:** TASK-1440. **Open:** the zone PID itself (no D term, output and integral clamps, `SATcontrol.ino:2270-2305`) differs from `pid.py` too; that is covered by the PID tasks (TASK-1414..1418, TASK-1423) where they apply to zone PIDs, and is not decided by this entry.

## 117. Q57 A: Thermal comfort humidity comes from indoor sources only (MQTT/REST or the BLE room sensor), no weather fallback (2026-10-29)

**Source:** #alpha-testing, 2026-10-29 05:14 UTC, answer to Q57 (msg 1565232005094973572):
https://discord.com/channels/812969634638725140/1514720723980259460/1565232358192455792

**Quoted:**

> Q57: A

Option A as posted: "Indoor only, as thermo-nova: humidity pushed over MQTT/REST, or the BLE room sensor's humidity; no weather fallback. Without indoor humidity Thermal comfort uses the room temperature."

**Settled (our reading of a one-letter reply, against the posted option text):** as thermo-nova, which takes humidity only from the configured humidity sensor and has none without one (`climate.py:282-287`). The firmware's outdoor-weather fallback (`SATcontrol.ino:5484-5488`) is removed. The BLE room sensor's humidity (`SATble.ino:892`, `state.sat.fBleHumidity`) becomes an indoor source. With no valid indoor humidity, Thermal comfort uses the room temperature, as with no humidity in thermo-nova.

**Changes an earlier entry:** none; consistent with entry 114 (humidity only through Thermal comfort) and entry 32.

**Task:** TASK-1441. **Open:** the priority between a pushed MQTT/REST value and the BLE sensor when both are present is not settled by this answer; our reading for the implementation: the most recent valid reading wins (to be confirmed if it matters).

## 118. Q58 A: in ECO mode no solar gain offset either, as thermo-nova (2026-10-29)

**Source:** #alpha-testing, 2026-10-29 06:47 UTC, answer to Q58 (msg 1565251228106096672):
https://discord.com/channels/812969634638725140/1514720723980259460/1565255839856726017

**Quoted:**

> Q58: A

Option A as posted: "As thermo-nova: in ECO no solar gain offset either."

**Settled:** in ECO mode the requested setpoint follows the main room's PID only: no other rooms (entry 116) and no solar gain offset, as thermo-nova `climate.py:376-379`, which returns before the solar offset at `:391-392`. The firmware subtracted the offset in ECO too (`SATcontrol.ino:5540`, no ECO check).

**Changes an earlier entry:** none; completes entry 116 for ECO. **Task:** TASK-1440 (same ECO branch). **Open:** thermo-nova's ECO return also rounds the primary to 0.1; rounding of PID values is covered by entry 95 / TASK-1423, not by this entry.

## 119. Field assessment of gruijter's alpha.527 run: regulation works as intended (2026-10-29)

**Source:** #alpha-testing, 2026-10-29 13:22 and 13:33 UTC, replies to gruijter's field capture (msg 1565344493475995819):
https://discord.com/channels/812969634638725140/1514720723980259460/1565355195939033089
https://discord.com/channels/812969634638725140/1514720723980259460/1565357952821624863

**Quoted:**

> I am very pleased with the regulation. It works as intended

> Also you boiler holds a nice ΔΤ. It works at 40/30 ( median 38.5/28.7 ) when CH is ON. Verdict: SAT passed the test to handle The most difficult boiler in control terms. But the real test is the WAF test 😂

**Settled:** this is an assessment, not a correction or a design decision. Sergeant D judges the control on gruijter's Intergas (field run on 2.0.0-alpha.527+fd491ff, capture from 08:58 local) as working as intended, with a supply/return of about 40/30 (median 38.5/28.7) while CH is on. Context from gruijter (msg 1565361639304339506): the boiler itself was tuned last year for a low return temperature (reduced power, minimum supply temperature 28, pump min/max and delay-off adapted). It changes no thermo-nova comparison and no firmware behaviour.

**Changes an earlier entry:** none. **Task:** TASK-1474 (analysis of the same capture); field evidence for Robert's "beta after field proof" decision (TASK-1469). **Open:** the run was on alpha.527, so the alpha.528-532 changes (zones/ECO, humidity source, previous-CS clamp, HA cleanups) are not covered by this assessment; the "WAF test" (household acceptance over a longer period) is still to come.

## 120. Restart: "this is SAT's bug", SAT stays quiet without valid OpenTherm; Q59 to be re-evaluated (2026-10-29)

**Source:** #alpha-testing, 2026-10-29 15:21 UTC, reply to Q59 (msg 1565378592890683554):
https://discord.com/channels/812969634638725140/1514720723980259460/1565385159803670560

**Quoted:**

> @BlueFlame Bot this is SAT's bug. We agreed in the open questions that when we don't have a valid OT response we keep sat quiet. Reevaluate Q59.

**Settled:** the restart behaviour seen in gruijter's alpha.527 capture (TASK-1474) is a SAT defect, not a design option. None of Q59's options A/B/C is chosen; he asks us to re-evaluate the question against the earlier decisions.

**Interpretation (ours, to be confirmed with the re-asked Q59):** the decisions he refers to are entry 39 (no valid OpenTherm: SAT stops and then sends nothing), entry 47 (that stop is the user switch-off hand-back CS=0, CH=1, MM=T, then silent), entry 79 (SAT resumes on the first new boiler reply) and entry 56 (outside temperature missing or old: SAT stops at once). Read together for a restart: SAT has no valid boiler reply and no outside temperature yet, so it should stay quiet (hand-back state, no CS/CH/MM of its own) and take over only once both are valid. In the capture it did the opposite: at 12:57:24 it sent CS=34.1, MM=100 and CH=1 with outside 0.0 °C (no weather fetched yet), until the weather arrived at 13:01:54.

**Compared with thermo-nova and the firmware:** thermo-nova skips a control pass while the outside temperature is None (`climate.py:544-545`) and restores "PWM enabled" and the last setpoint after a Home Assistant restart (`pwm.py:111-114`, `heating_control.py:181-191`, `climate.py:169-174`). Firmware: boot sends CS=0 (`SATcontrol.ino:4317-4325`), starts in continuous mode (`:4338`) and the outside-temperature fallback ends at MsgID 27 / 0.0 (entry 56 comparison, TASK-1390).

**Changes an earlier entry:** none; it applies entries 39, 47, 56 and 79 to the restart case. **Task:** TASK-1390 (outside temperature missing), restart hand-over task to be filed after the re-asked Q59. **Open:** whether "quiet" at a restart also waits for a valid outside temperature, and whether the PWM state is restored.
