# TASK-1527 field test plan: degree minutes against today's heat-pump PWM

Status: proposal, 2026-11-01. Nothing here is built. The firmware work it needs is listed at the end and goes to the
dev session as a separate task once Robert approves the plan.

## Why a field test

Rounds 12-17 (this folder) compare the control options in a model. The model has known gaps that matter for exactly
the question Sergeant D raised on 2026-11-01 (entry 192: "degree minutes prioritise efficiency over comfort"):

| Gap | Effect on the conclusion |
|---|---|
| COP depends only on flow and outdoor temperature; no start losses, no defrost | Long runs get no efficiency credit. Literature: cycling degradation coefficient about 0.05-0.10 on modern units (NIST TN 1848, US air-to-air context), so a single-stage unit would lose roughly 3-7 % at 30 % load (part-load factor 1 - Cd x (1 - 0.3)). The model is biased against degree minutes on on/off machines. |
| No buffer tank | On/off heat pumps on radiators look worse than they are in practice. |
| The heat pump's own control is assumed | "Own control" results are a guess per brand. |
| Radiator start counts come from the round 4 flow loop, not validated | Absolute starts per day on radiators are uncertain; the paired differences less so. |
| Houses are synthetic, calibrated on one winter | No house-specific behaviour (sun, internal gains, occupants). |

The UK charts Sergeant D posted (system 473 vs a gas boiler with SAT) cannot answer it either: two different homes,
heat computed as electricity x COP, and the heat pump's control is unknown.

Only one design separates the control method from the house and the weather: **the same house, both methods,
alternating.**

## Hypotheses (fixed before the test starts)

Against today's heat-pump PWM (1800 s), in the same house:

- **H1 starts:** degree minutes cut compressor starts per day by at least 40 %.
- **H2 comfort:** the room error (RMS against the setpoint, heating hours only) rises by no more than 0.05 C.
- **H3 energy:** electricity per heating degree day rises by no more than 3 %; where a heat meter exists, the COP
  per degree day does not fall.

Model predictions to check (round 16): underfloor on/off -90 % starts, comfort better, -1.7 % electricity;
radiators modulating 30 % (DM -240) -35 % starts, +0.01 C; radiators on/off -49 % starts, +0.07 C, +2.5 %.
A field result outside the model's range is a finding about the model, not only about the control.

## Design: switchback (crossover) within one house

- **Arms:** A = today's SAT heat-pump PWM (1800 s); B = degree minutes (start at -S, stop at 0, min 20 min on and
  10 min off), S from the heating system: -120 underfloor, -240 radiators with a modulating heat pump.
  A third arm (long time-proportional PWM) only if a house has time for it; three arms need about 50 % more days.
- **Block length 4 days, the first day discarded** as wash-out. Sergeant D (entry 168) puts a house's response at
  60-72 h; a screed floor carries heat over between blocks, so the first day of a block still reflects the previous
  arm.
- **Order:** ABBA BAAB repeated, so a weather trend does not line up with one arm.
- **Switch at a fixed time** (03:00 local, low load, no hot water) by the firmware, not by hand.
- **Length:** at least 8 blocks per arm (64 days) in the heating season, November to March. The number is checked
  after the first 4 blocks per arm against the observed day-to-day spread (internal pilot) and against a simulated
  run of this same protocol (round 18).
- **Kept constant:** room setpoint and schedule, heating curve and PID settings, hot-water schedule. Every change is
  logged with a timestamp; a block with a change is marked and analysed separately.

## Houses

At least one per type that the model treats differently:

1. On/off heat pump on underfloor heating (candidate: maarten's ground-source heat pump, which already runs a
   3-hour PWM in practice).
2. Modulating heat pump on radiators.
3. On/off heat pump on radiators, with and without a buffer tank if available.

Choosing and asking the testers is Robert's decision. One house per type gives a within-house answer, not a
population answer; the report must say so.

## What is measured (per minute, kept locally: diagnostics stay local, TASK-1473)

| Signal | Source |
|---|---|
| Mode / arm, degree-minute accumulator, SAT wanted flow (CS) | firmware, new MQTT topics |
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
- Cost (the spark gap Sergeant D asks for): daily electricity x tariff, reported at the tester's tariff and at a
  fixed reference price ratio, so houses can be compared.
- Decision rule per house type: B replaces A if H1 holds and the upper bound of the interval for H2 and H3 stays
  within the margins above. Otherwise A stays, and the report states which hypothesis failed.

## Before the field: round 18 in the model

1. Add a start-loss term (cycling degradation coefficient 0.05 and 0.10 as sensitivity) and report electricity both
   with and without it.
2. Add cost at a chosen electricity / gas price ratio, and SAT on a gas boiler in the same house as reference.
3. Run this exact protocol (4-day blocks, ABBA, discard day 1) in the model over the three seasons, to estimate how
   many blocks are needed to detect the predicted comfort and energy differences.

## Firmware needed (not built here; a dev-session task after approval)

- Degree-minute mode for heat pumps, experimental opt-in, as specified above.
- A test scheduler that switches arm at 03:00 by a fixed block list, and publishes the active arm.
- Per-minute MQTT topics for the arm and the degree-minute accumulator.
- Before implementation: Q-1527a answered by Sergeant D, or an explicit decision that the test mode is
  experimental and does not settle Q-1527a.

## Sources

- NIST Technical Note 1848, p. 15: "Dougherty (2003) demonstrated that the typical value of CD is now in the range
  0.05 to 0.10 for most systems. So cyclic degradation and the part load efficiency losses may be of less consequence
  than was previously thought." (read 2026-11-01). Caveat: this is the US SEER context, air-to-air systems, not
  air-to-water or ground-source heat pumps; used here only as the order of magnitude for the sensitivity run.
  https://nvlpubs.nist.gov/nistpubs/TechnicalNotes/NIST.TN.1848.pdf
- Dongellini and Morini 2019, On-off cycling losses of reversible air-to-water heat pump systems as a function of the
  unit power modulation capacity, Energy Conversion and Management 196, 966-978, doi 10.1016/j.enconman.2019.06.022.
  Not read (paywall); known from search results only, which describe start-up losses as significant mainly for
  single-stage units. Not used for any number in this plan.
- UK government report on hot-water cylinders and buffer tanks for heat pumps (inverters modulating to about 30 %
  hardly need a buffer; fixed-speed units do):
  https://assets.publishing.service.gov.uk/media/5a7a479ee5274a319e779234/hot_water_cylinders_buffer_tanks_heat_pumps.pdf
- NIBE F1145 manual, degree minutes (menu 4.9.3): https://www.manualslib.com/manual/1146229/Nibe-F1145.html?page=49
- NIBE F370 manual, room sensor "factor system": https://www.manualslib.com/manual/1590978/Nibe-F370.html?page=44
