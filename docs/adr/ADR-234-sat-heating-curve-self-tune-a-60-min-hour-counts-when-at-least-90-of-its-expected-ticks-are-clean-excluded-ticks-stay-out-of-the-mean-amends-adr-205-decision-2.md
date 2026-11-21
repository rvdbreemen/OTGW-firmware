---
id: "ADR-234"
title: "SAT heating curve self-tune: a 60 min hour counts when at least 90 % of its expected ticks are clean; excluded ticks stay out of the mean (amends ADR-205 decision 2)"
status: "Accepted"
date: "2026-11-17"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-205"
  - "ADR-228"
  - "ADR-231"
topics:
  - "heating curve self-tune"
  - "clean hour"
  - "clean tick share"
  - "clean hour exclusions"
aliases:
  - "90 percent clean hour"
  - "clean share of an hour"
  - "per-tick exclusions"
components:
  - "self-tune clean-hour rule"
symbols:
  - "satTuneAddTick"
  - "satTuneCloseHour"
  - "satTuneMarkReason"
  - "satTuneNoteReason"
  - "SAT_CT_MIN_CLEAN_PCT"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-234 SAT heating curve self-tune: a 60 min hour counts when at least 90 % of its expected ticks are clean; excluded ticks stay out of the mean (amends ADR-205 decision 2)

## Status

Accepted, 2026-11-17.

Amends ADR-205 decision 2 (the clean hour) and entry 174's clean-hour rule. Every other decision of ADR-205, ADR-228 and
ADR-231 stays in force. Implemented by TASK-1932.

**Decision Maker:** Robert (maintainer) and Sergeant D (SAT design owner) for the rule: entry 340 of
`docs/sat/decisions/sergeant-d-corrections.md` (Q-TUNE2 B, 2026-11-14), and entry 343 (Q-340a changed to B, 2026-11-14,
supersedes entry 341's A) for the share basis.
Robert accepts or rejects this record.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-205
    changed_via: adr-kit lifecycle
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-228
    changed_via: adr-kit lifecycle
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-231
    changed_via: adr-kit lifecycle
  - date: 2026-11-17
    status: Accepted
    changed_by: "Robert van den Breemen (decision, in session 2026-11-16 23:03, relayed by the coordinator session), recorded by Agent (d3)"
    reason: "Accepted: decision Robert van den Breemen (2026-11-16 23:03, coordinator session), recorded by Agent (d3) after the four verification gates passed"
    changed_via: adr-kit lifecycle
```

## Context

The heating curve self-tune of SAT (Smart Autotune Thermostat; ADR-205, ADR-228, ADR-231) learns from clean hours only.
It runs one control tick every 30 s, so a full tuner hour has 120 ticks. Until now ADR-205 decision 2 dropped the whole
hour on the first excluded tick: `satTuneAddTick()` noted the first reason from `satTuneTickReason()`
(`src/OTGW-firmware/SATcontrol.ino:6291`), and `satTuneCloseHour()` counted the hour only when no reason was set. One
minute of hot water, or one tick at the minimum modulation, cost the tuner the whole hour.

Evidence (TASK-1927; field inputs, substitute code variants): 5 of gruijter's field runs (betas 68, 167, 171, 214, 218),
67 tuner hours. The real tuner code, cut from each run's own commit, reproduces the reason and the counted flag in 67 of
67 hours. Today 8 hours count. With a 60 min hour that counts at 90 % clean ticks, 10 hours count, with the same votes on
every run and no hour moved more than 0.5 C by its excluded ticks. Shorter windows or a lower share flip votes through the
morning warm-up after the night setback (the comms scratchpad `tune-replay/out/followup2_summary.txt`, Table A).

Robert asked Sergeant D: "Drop it to 90%?"; Sergeant D: "I agree", and on Q-TUNE2 "The answer s B" (entry 340). The open
point, whether the 90 % is measured against the ticks the hour had or against the expected 120, was asked as Q-340a;
Sergeant D first answered "The answer is A" (entry 341), then "Change my answer to B" (entry 343, which supersedes 341).
On the field data both readings give the same 10 hours, because all 59 sixty-minute replay windows have 120 ticks; they
differ only for an hour with missed ticks.

thermo-nova has no coefficient self-tune (entry 72, quoted in entry 329), so this rule is firmware-only.

## Decision

A tuner hour stays 60 min and counts when no whole-hour marker was set and its clean ticks are at least 90 % of the
ticks a full hour should have; its excluded ticks stay out of the hour mean and out of the warm-up and floating counts.

1. **Per-tick exclusions** (the ones `satTuneTickReason()` returns, unchanged: SAT not active on the tick, hot water,
   sun unknown or at least 12 degrees, the PWM rule of ADR-231, flame off while CH is on, modulation at or below the
   minimum, room temperature lost) only lower the clean share of the hour. `satTuneAddTick()` counts them in
   `exclTicks` and adds the signal of a tick to the clean sums only when the tick is clean.
2. **Clean share** (entry 343): clean ticks against the expected ticks of the hour (hour length / control interval,
   120 at the 30 s default; `SATcontrol.ino:6752`), so a missed tick counts against the hour like an excluded one: at
   least 108 clean ticks, 54 min of clean data. In integers, `(ticks - exclTicks) * 100 >= SAT_CT_MIN_CLEAN_PCT * expected`
   with `SAT_CT_MIN_CLEAN_PCT` 90 (`src/OTGW-firmware/SATcurveTune.h`, `satTuneCloseHour()` in `SATcontrol.ino`). The
   basis is always the expected number, also when the hour logged more ticks (a tick late across the boundary): 108 clean
   ticks count whatever the hour logged (entry 369, Q-343a B). A counted hour therefore always meets the minimum-ticks
   rule (entry 343).
3. **Whole-hour markers** stay whole-hour, through `satTuneMarkReason()` (`SATcontrol.ino:6354`): SAT not active or
   tripped at a service pass (`not_sat`, `SATcontrol.ino:6800`), a tick path that returns early (`not_controlling`,
   `SATcontrol.ino:6674`), a coefficient change in the hour (`coeff_changed`), too few ticks (`few_ticks`, below
   `SAT_CT_MIN_TICK_FRACTION` 0.75 of the expected ticks, 90 at 30 s) and no ticks (`no_ticks`). An hour that nothing
   excluded but that is short of the 108 because of a gap reports `few_ticks` too.
4. **Hour record.** A counted hour reports the mean and the phase counts of its clean ticks, the values the decision
   uses. A not-counted hour reports all its signal ticks as before, so the PWM hour of a heat pump still shows its
   signal (TASK-1551, entry 205). `reason=` stays the first exclusion in time; a counted hour reports `none`.

## Decision Contract

### Must

- Count an hour only when no whole-hour marker was set, its clean ticks are at least 90 % of the expected ticks of the
  hour, and a clean tick carried a signal.
- Measure the share against the expected ticks of the hour only, whatever the hour logged (entries 343, 369).
- Keep excluded ticks out of the mean and the phase sums that reach the day and the decision.
- Keep `not_sat` from the service pass, `not_controlling`, `coeff_changed`, `few_ticks` and `no_ticks` as whole-hour
  markers.

### Must Not

- Change the per-tick exclusions themselves, their order, or the phase rule of ADR-228 and ADR-231.
- Shorten the hour or lower the share below 90 % (entry 340, option C rejected).
- Let a tick a per-tick exclusion applies to enter the clean sums.

### Exceptions

- None.

### Verification

- `tests/test_sat_curve_selftune.py`, class `CleanShare` (compiles the real tuner blocks): an hour of 120 ticks with 1
  excluded tick counts and its mean is the clean-tick mean; 108 of 120 clean counts, 107 does not; 100 logged
  ticks with 10 excluded do not count, 108 clean of 120 expected with 12 missed do, 107 do not (`few_ticks`); 121 logged
  ticks with 108 clean count and with 107 clean do not (entry 369); the whole-hour markers still drop an hour; an excluded tick reaches no phase; three days with
  one excluded tick an hour decide on the clean mean. On the tuner from before TASK-1932 (e6bcfc2da) 6 of these 7 tests
  fail; the whole-hour marker test passes there too, as it should.
- The replay of the 5 gruijter runs of TASK-1927 on the fix gives 10 counted hours (base 8) with the same votes
  and pooled clean means as the `share10` replay variant: r1 +0.97 band, g167 +3.19 raise, g68 +0.23 band; every counted
  hour has 120 ticks, so the share basis (entries 341 vs 343) does not change this result.

## Alternatives Considered

- **Keep the whole-hour rule (do nothing).** Rejected by Robert and Sergeant D (entry 340): 8 of 67 field hours count,
  and a single hot-water minute drops an hour.
- **90 % against the ticks the hour logged** (Q-340a A). Sergeant D chose it first (entry 341), then changed to B
  (entry 343): a gap must count against the hour, so an hour is never credited for data it did not see.
- **Shorter windows (15 or 30 min) or a share below 90 %** (Q-TUNE2 option C). Rejected (entry 340): on the field data
  they flip votes through the morning warm-up after the night setback, a one-sided selection entry 174 rejected.
- **Count the share but keep the excluded ticks in the mean.** Rejected: entry 340 says the excluded ticks are left out
  of the mean; a min_mod tick near the minimum carries a signal that does not describe the curve.

## Consequences

**Positive:**

- More hours count: 10 instead of 8 on the 5 field runs (+25 %), with the same votes.
- A counted hour's mean is never moved by its excluded ticks; on the field data the excluded ticks would have moved no
  counted hour by more than 0.5 C, and now they cannot.

**Negative:**

- An hour with up to 10 % excluded ticks now counts, so its clean ticks can lean toward the conditions that were not
  excluded (for example the part of the hour without hot water). Mitigation: the share stays high (90 %), the window
  stays 60 min, and the 3-day mean with the 0.1 step limits the effect of one hour.
- A short stall now costs the hour: 13 missed ticks (6.5 min) at 30 s drop an otherwise clean hour as `few_ticks`.
  Accepted by Sergeant D (entry 343). The old 75 % minimum no longer decides alone; it only names the reason for very
  short hours.
- The rule rests on one house in November; no run reached a full tuner day, so the daily decision itself is not
  field-tested (entry 340, open point 3). Mitigation: Robert asked for more field data.

## Open Questions

- [x] Is the 90 % counted against all ticks of the hour or against the expected 120? — **Answered 2026-11-14 by
  Sergeant D (relayed by otgw-firmware-92):** first A (msg 1571110323295879212, entry 341), then "Change my answer to
  B" (msg 1571111795874209925, entry 343): the expected 120.
- [x] An hour that logged more ticks than expected (for example 121 at 30 s): 90 % of the ticks it logged (109 of 121)
  or 108 clean ticks whatever it logged? — **Answered 2026-11-15 by Sergeant D, "16: B" (Q-343a, entry 369,
  https://discord.com/channels/812969634638725140/1105556725714649128/1571637957230862506):** 108 clean ticks whatever
  it logged. Implemented.

## Related Decisions

- **ADR-205 (SAT heating curve self-tune: hourly evaluation, daily decision on the curve integral)**: amends decision 2,
  the clean hour.
- **ADR-228 (phase split)**: unchanged; the phase sums of a counted hour carry its clean ticks only.
- **ADR-231 (PWM ticks on a gas boiler)**: unchanged; its PWM rule is one of the per-tick exclusions this record counts.

## References

- TASK-1932 (implementation), TASK-1927 (evidence).
- `docs/sat/decisions/sergeant-d-corrections.md` entries 174, 340, 341, 343.
- `src/OTGW-firmware/SATcontrol.ino:6291` (`satTuneTickReason`), `:6348` (`satTuneNoteReason`), `:6354`
  (`satTuneMarkReason`), `:6362` (`satTuneAddTick`), `:6398` (`satTuneCloseHour`), `:6752` (expected ticks).
- `src/OTGW-firmware/SATcurveTune.h:19` (`SAT_CT_MIN_TICK_FRACTION`), `:25` (`SAT_CT_MIN_CLEAN_PCT`).
- `tests/test_sat_curve_selftune.py`, class `CleanShare`.

## Enforcement

No declarative rule: the decision is a numeric threshold inside the tuner, guarded by the host test class `CleanShare`
in `tests/test_sat_curve_selftune.py`, which compiles the real tuner blocks.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "#define SAT_CT_MIN_CLEAN_PCT\\s+90u",
      "path_glob": "src/OTGW-firmware/SATcurveTune.h",
      "message": "ADR-234: the clean share of a tuner hour is 90 % (entries 340, 343)."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "A numeric threshold guarded by the CleanShare host test on the real tuner code; no prose rule to judge."
}
```
