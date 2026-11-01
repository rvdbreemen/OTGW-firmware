# TASK-1527 field test plan: longer heat-pump PWM periods against today's 30 minutes

Status: proposal, 2026-11-01. Scope set by Robert (entry 193): **no new control mechanism, only more time.** SAT keeps
its current heat-pump PWM (ON time fixed at the start of each period); the field test compares today's period with
periods of 1.5, 2 and 3 hours. Degree minutes and the time-proportional variant stay in the model for now
(rounds 12-17) and are not field-tested. The firmware options are TASK-1542 (dev session); nothing here is built.

## Why a field test

Rounds 12-17 (this folder) compare the options in a model with known gaps:

| Gap | Effect on the conclusion |
|---|---|
| COP depends only on flow and outdoor temperature; no start losses, no defrost | Long runs get no efficiency credit. Literature: cycling degradation coefficient about 0.05-0.10 on modern units (NIST TN 1848, US air-to-air context), so a single-stage unit would lose roughly 3-7 % at 30 % load (part-load factor 1 - Cd x (1 - 0.3)). The model is biased against long periods on on/off machines. |
| No buffer tank | On/off heat pumps on radiators look worse than they are in practice. |
| The heat pump's own control is assumed | The heat pump may modulate or stop by itself inside a long ON phase. |
| Radiator start counts come from the round 4 flow loop, not validated | Absolute starts per day on radiators are uncertain; the paired differences less so. |
| Houses are synthetic, calibrated on one winter | No house-specific behaviour (sun, internal gains, occupants). |

Sergeant D's UK charts (entry 192) cannot answer it either: two different homes, heat computed as electricity x COP,
and an unknown heat-pump control. Only one design separates the control setting from the house and the weather:
**the same house, both settings, alternating.**

## What the model predicts (round 17, SAT's PWM as it is)

| House type | 30 min (today) | 90 min | 120 min | 180 min |
|---|---|---|---|---|
| Underfloor, on/off, heavy floor: starts/day / mean abs room error (C) | 33.7 / 0.325 | | 8.2 / 0.374 | 4.7 / 0.395 |
| Radiators, modulating 30 %: starts/day / RMS (C) | 8.0 / 0.204 | | 3.6 / 0.365 | |
| Radiators, modulating 60 %: starts/day / RMS (C) | 28.5 / 0.231 | | 17.5 / 0.466 | |
| Radiators, on/off: starts/day / RMS (C) | 35.8 / 0.273 | | 29.8 / 0.511 | |

Source: round17.md. The model says: long periods cut starts strongly on underfloor at a modest comfort cost, and
cost comfort quickly on radiators. maarten's on/off ground-source heat pump on underfloor runs a 3-hour period in
practice and is reported to work well (round 12); the field test checks whether that holds when measured.

## Hypotheses (fixed before the test starts)

Against today's 1800 s period, in the same house:

- **H1 starts:** the long period cuts compressor starts per day by at least 50 %.
- **H2 comfort:** the room error (RMS against the setpoint, heating hours only) rises by no more than 0.05 C on
  underfloor and 0.10 C on radiators.
- **H3 energy:** electricity per heating degree day does not rise by more than 3 %; where a heat meter exists, the
  COP per degree day does not fall.

A field result outside the model's range is a finding about the model, not only about the setting.

## Design: switchback (crossover) within one house

- **Arms:** A = 1800 s (today). B = the long period for that house type: **10800 s (3 h) on underfloor**,
  **5400 s (1.5 h) on radiators** (round 17: comfort cost grows quickly beyond about 60 min there). A third arm
  of 7200 s only if a house has the time; three arms need about 50 % more days.
- **Block length 4 days, the first day discarded** as wash-out. Sergeant D (entry 168) puts a house's response at
  60-72 h; a screed floor carries heat over between blocks.
- **Order:** ABBA BAAB repeated, so a weather trend does not line up with one arm.
- **Switch at a fixed time** (03:00 local, low load, no hot water). By hand is acceptable for a first test if the
  tester logs the moment; a firmware scheduler is better (see below).
- **Length:** at least 8 blocks per arm (64 days) in the heating season, November to March. Checked after the first
  4 blocks per arm against the observed day-to-day spread (internal pilot).
- **Kept constant:** room setpoint and schedule, heating curve and PID settings, hot-water schedule. Every change is
  logged with a timestamp; a block with a change is marked and analysed separately.

## Houses

At least one per type the model treats differently:

1. On/off heat pump on underfloor heating (candidate: maarten's ground-source heat pump).
2. Modulating heat pump on radiators.
3. On/off heat pump on radiators, with and without a buffer tank if available.

Approved by Robert 2026-11-01 to ask testers. One house per type gives a within-house answer, not a population
answer; the report must say so.

## What is measured (per minute, kept locally: diagnostics stay local, TASK-1473)

| Signal | Source |
|---|---|
| PWM period setting and PWM phase | firmware (existing SAT state) |
| SAT wanted flow (CS) | MsgID 1 |
| Compressor or flame on/off, modulation level | OpenTherm MsgID 0 bit 3, MsgID 17 |
| Flow and return temperature | MsgID 25, 28 |
| Room temperature and setpoint | SAT inputs |
| Outdoor temperature | MsgID 27 or the weather source SAT uses |
| Electricity of the heat pump | sub-meter (required) |
| Delivered heat | heat meter (optional; without it COP is not measured) |
| Hot-water activity | MsgID 0 bit 2 (hours with hot water are excluded from comfort) |

## Analysis (fixed before the test starts)

- Unit of analysis: one analysed day. Per day: starts, median and distribution of run length, room RMS and minutes
  outside +-0.3 C during heating hours, electricity, heat (if metered), heating degree days (base 15.5 C), solar.
- Energy: regression of daily electricity on degree days, solar and arm; the arm coefficient with a block bootstrap
  95 % interval (resampling whole blocks, because days within a block are correlated).
- Starts and comfort: paired difference between neighbouring A and B blocks, block bootstrap interval.
- Cost (the spark gap Sergeant D asks for): daily electricity x tariff, at the tester's tariff and at a fixed
  reference price ratio, so houses can be compared.
- Decision rule per house type: the long period becomes the recommended setting for that type if H1 holds and the
  upper bounds of the intervals for H2 and H3 stay within the margins. Otherwise 1800 s stays, and the report states
  which hypothesis failed.

## Deferred: round 18 in the model (TASK-1543, label simulatie)

Start-loss term, spark-gap cost with SAT on gas as reference, and this protocol run in the model to size the number
of blocks. Deferred by Robert on 2026-11-01.

## Firmware needed (TASK-1542, dev session)

- Heat-pump PWM period options 5400, 7200 and 10800 s next to 1800 and 2400 s (setter, REST limits, UI, host test
  against the real PWM mapper, bench run).
- Optional: a test scheduler that switches the period at 03:00 by a fixed block list and publishes the active arm.

## Sources

- NIST Technical Note 1848, p. 15: "Dougherty (2003) demonstrated that the typical value of CD is now in the range
  0.05 to 0.10 for most systems. So cyclic degradation and the part load efficiency losses may be of less consequence
  than was previously thought." (read 2026-11-01). Caveat: US SEER context, air-to-air systems, not air-to-water or
  ground-source heat pumps; used only as the order of magnitude for the deferred sensitivity run.
  https://nvlpubs.nist.gov/nistpubs/TechnicalNotes/NIST.TN.1848.pdf
- Dongellini and Morini 2019, On-off cycling losses of reversible air-to-water heat pump systems as a function of the
  unit power modulation capacity, Energy Conversion and Management 196, 966-978, doi 10.1016/j.enconman.2019.06.022.
  Not read (paywall); known from search results only. Not used for any number in this plan.
- UK government report on hot-water cylinders and buffer tanks for heat pumps (inverters modulating to about 30 %
  hardly need a buffer; fixed-speed units do):
  https://assets.publishing.service.gov.uk/media/5a7a479ee5274a319e779234/hot_water_cylinders_buffer_tanks_heat_pumps.pdf
- thermo-nova at the pinned commit: `pwm.py:54` (period 3600 / cycles per hour), `config_flow.py:688-691`
  (heat pumps: 2 or 3 cycles per hour).
