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
  (thermo-nova `pwm.py:237`, firmware `SATcontrol.ino:734`). Both match.
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
