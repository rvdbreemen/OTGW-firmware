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
