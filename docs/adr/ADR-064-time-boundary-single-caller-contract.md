# ADR-064 — Time-boundary consume-on-read helpers MUST have exactly one call site

## Status

Accepted, 2026-04-20

## Context

`helperStuff.ino` provides four time-boundary helpers:

- `minuteChanged()` at `helperStuff.ino:493`
- `hourChanged()` at `helperStuff.ino:505`
- `dayChanged()` at `helperStuff.ino:480`
- `yearChanged()` at `helperStuff.ino:467`

Each works identically: compare current wall-clock field to a static `lastX`, return `true` iff changed, then update `lastX`. This is **consume-on-read** semantics: only the caller that triggers the change sees `true`. A second caller in the same tick gets `false` — the event has been consumed.

### Current call-site census (verified during plan research)

| Helper | Call site | Downstream consumer of the returned flag |
|---|---|---|
| `minuteChanged()` | `OTGW-firmware.ino:366` (main loop) | `doTaskMinuteChanged()` dispatch |
| `hourChanged()` | `OTGW-firmware.ino:272` (inside `doTaskEvery60s`, from TASK-345) | Nightly restart check + `sendMQTTheapdiag` publish (from TASK-346) |
| `dayChanged()` | `networkStuff.ino:494` (inside `sendtimecommand`) | PIC `SR=21` command (month, day) |
| `yearChanged()` | `networkStuff.ino:500` (inside `sendtimecommand`) | PIC `SR=22` command (year) |

These four call sites live in FOUR DIFFERENT functions. The distribution is historical accident; there is no single place a maintainer can look to see "what happens on every hour" or "what happens every day". Worse, adding a second consumer of any helper silently steals the event:

- Adding a second `dayChanged()` call anywhere (e.g., for a daily log rotate, a daily counter flush, a daily verification ping) would either always win (and break `SR=21`) or always lose (and itself never fire), depending on call order within the main loop iteration.
- The failure mode is silent. No compile error, no runtime assert. The only signal is "one of the features stopped working" in the field, hours to days after the change landed.

### Why this shows up now

The discovery-verification plan (see `docs/adr/ADR-062`) adds a new daily consumer: a call to `startDiscoveryVerification()` when a new day flips. Without a contract, the implementer either:

1. Re-calls `dayChanged()` — which steals `SR=21` from `sendtimecommand`, breaking the PIC date-sync.
2. Maintains a parallel local-static `lastVerifyDay` — which duplicates the bookkeeping, obscures intent, and becomes a pattern that proliferates across every future daily feature.

Both paths create a maintenance tax. The rule below formalises a third way and enforces it via a CI gate in `evaluate.py`.

## Decision

Each of the four helpers has **exactly one call site** in the entire firmware. Every downstream consumer reads a pre-computed `bool` flag, never re-calls the helper.

After the TASK-350 refactor, the canonical structure is:

```cpp
// OTGW-firmware.ino — doTaskMinuteChanged (single dispatcher for all sub-minute boundaries)
void doTaskMinuteChanged() {
  // ADR-064: single caller for each of hour/day/year; captured into flags for downstream gates.
  const bool hourFlag = hourChanged();
  const bool dayFlag  = dayChanged();
  const bool yearFlag = yearChanged();

  // Per-minute work
  sendtimecommand(dayFlag, yearFlag);   // refactored signature: flags passed in, no internal xChanged calls

  // Hourly consumers (extend here, never with a second hourChanged() call)
  if (hourFlag) {
    runNightlyRestartCheck();
    sendMQTTheapdiag();
  }

  // Daily consumers (extend here)
  if (dayFlag) {
    if (settings.mqtt.bDiscoveryAutoVerify) startDiscoveryVerification();
  }

  // Yearly consumers (extend here)
  // none currently beyond sendtimecommand's SR=22
}
```

`minuteChanged()` retains its single call site at `OTGW-firmware.ino:366` because it is the gate that triggers `doTaskMinuteChanged` in the first place; re-calling it inside the dispatcher would always return `false`.

### Rule (non-negotiable)

> For each of the four helpers `minuteChanged()`, `hourChanged()`, `dayChanged()`, `yearChanged()`, the codebase contains **exactly one** call site. Downstream consumers capture the returned bool once per tick into a local flag and read that flag.

### Enforcement

A new `evaluate.py` check `check_time_boundary_single_caller` scans all `src/OTGW-firmware/**/*.{ino,cpp,h}` files and counts occurrences of each helper name as a call (`hourChanged()`, `dayChanged()`, etc., excluding the definition itself in `helperStuff.ino`). If any helper has more than one call site, the check fails, blocking merge.

The permitted call site is pinned by a comment anchor at the canonical location:

```cpp
  // ADR-064: single caller
  const bool hourFlag = hourChanged();
```

The check implementation should be resilient to:
- Comments containing the helper name (skip lines starting with `//` or `*`)
- Function definitions (skip the lines in `helperStuff.ino` where the helper is declared)
- The `// ADR-064` anchor line itself (do count it, so the anchor and the call form one permitted pair)

### Guidance for future features

| Scenario | Correct approach |
|---|---|
| New hourly task | Add a statement inside `if (hourFlag) { ... }` in `doTaskMinuteChanged`. Never write `if (hourChanged())`. |
| New daily task | Add a statement inside `if (dayFlag) { ... }`. Never introduce a local `static int8_t lastDay`. |
| New sub-minute granularity (e.g., every 10s) | Use `DECLARE_TIMER_SEC` + `DUE()` — these are not consume-on-read and can have multiple consumers safely. |
| Moving a consumer between functions | Ensure the dispatcher still captures the flag exactly once; do not reintroduce the helper elsewhere. |

## Alternatives Considered

### Alternative A: Per-consumer local-static (each new feature reimplements its own day/hour tracking)

Let every new periodic feature carry its own `static int8_t lastDay` (or `lastHour`, `lastYear`) and roll its own `if (currentDay != lastDay) { ... lastDay = currentDay; }` block. The four `helperStuff.ino` helpers are left alone; they remain optional utilities that some callers happen to use.

**Rejected** because it duplicates the wall-clock bookkeeping at every call site, hides the intent ("this work runs daily" disappears into a static-variable comparison), and makes feature removal error-prone — a contributor deleting a daily task can easily leave an orphan `lastDay` static behind. It also loses the central registry property: a maintainer who wants to know "what does this firmware do on every day boundary?" must grep the entire codebase for date comparisons rather than read one block of `if (dayFlag)` statements. The pattern proliferates with every new feature, multiplying the bookkeeping surface for no functional benefit.

### Alternative B: Multi-subscriber event bus with callback registration

Introduce a small in-firmware pub/sub: `registerOnDay(callback)` / `registerOnHour(callback)` etc., and have the four helpers fan out to all registered subscribers when a boundary fires. Each new daily feature would call `registerOnDay(myDailyTask)` from its `setup()`.

**Rejected** because it is a substantial structural addition (a callback list, registration API, dispatch loop) for exactly four events on a microcontroller that has no other dynamic-callback patterns. It introduces ordering questions (which callback runs first?), error-handling questions (what if one callback yields and re-enters the dispatch?), and lifetime questions (no `unregister` story), all to solve a problem that a pre-computed local `bool` already solves in zero new code. Overkill given the actual scale.

### Alternative C: Convert the helpers to non-consuming (return flag without updating `lastX`; callers update separately)

Change the semantics so `dayChanged()` returns `true` whenever a day flip is observed and only updates `lastDay` when the caller explicitly calls a separate `ackDayChanged()` (or similar). Multiple callers can then safely ask "did the day flip?" and at least one of them, by convention, acknowledges.

**Rejected** because it breaks every existing call site, requires every read to become a two-step pattern (read + ack), and pushes the "who acks?" responsibility onto contributors with no compile-time enforcement. Forgetting to ack means the helper returns `true` forever; double-acking has no effect but adds noise. The transition is error-prone and the resulting API is harder to reason about than the consume-on-read original.

### Alternative D (chosen): Single call site per helper, downstream consumers read a captured `bool` flag

Pin each of the four helpers to exactly one call site (the `doTaskMinuteChanged` dispatcher), capture the return into a local `bool` once per tick, and have all downstream daily/hourly/yearly work read that flag. Enforce the rule mechanically with a new `evaluate.py` check that fails the build if any helper has more than one call site.

**Trade-off accepted**: requires a one-time refactor (TASK-350) that touches `OTGW-firmware.ino`, `networkStuff.ino` (signature change to `sendtimecommand`), and `evaluate.py`. After that, adding a new daily/hourly task is a one-line addition inside the appropriate `if (dayFlag)` / `if (hourFlag)` block, and the rule is machine-checked so future contributors cannot silently regress it.

## Consequences

### Benefits

- One canonical place to read "what happens on every hour / day / year".
- Adding a new consumer is a one-line inline statement, not a new helper function with its own state.
- Wall-clock alignment improves: post-refactor, hourly work fires at `HH:MM:00` rather than at a boot-relative offset that drifts over long uptimes.
- The rule is machine-checked, not just documented — future PRs cannot silently violate it.

### Costs

- The refactor (TASK-350 in the current plan) is a multi-file change: `OTGW-firmware.ino` (move blocks), `networkStuff.ino` (change signature), `evaluate.py` (add check). Landed as one atomic commit.
- `sendtimecommand` gains two new parameters. Only one caller (`doTaskMinuteChanged`) so migration is contained.
- All three flags (`hourFlag`/`dayFlag`/`yearFlag`) fire `true` on the first post-NTP-sync dispatcher tick because the helpers' `lastX = -1` sentinels mismatch any real wall-clock value. Downstream consumers must defend against boot-time first-minute fires: `runNightlyRestartCheck` already does via `uptime > 3600`, and `sendMQTTheapdiag` publishes an acceptable near-zero snapshot (overwritten on the next real hour). New consumers added to `if (hourFlag) { ... }` must consider this.

### Alternatives considered and rejected

See `## Alternatives Considered` above for the full discussion of the per-consumer local-static, multi-subscriber event bus, and non-consuming-helpers alternatives.

## Related Decisions

- ADR-062 — retained discovery verification (introduces the new daily consumer that motivates this rule)
- TASK-345 — already established single-caller dispatch for `hourChanged()` in `doTaskEvery60s`; this refactor moves that same pattern into `doTaskMinuteChanged` for wall-clock alignment
- TASK-350 — implements this ADR
- TASK-351 — adds the new daily consumer inside the unified dispatcher
- `helperStuff.ino:467-515` — the four helper definitions (unchanged by this ADR)
- `networkStuff.ino:494-504` — current `sendtimecommand` (signature changes in TASK-350)

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->

## Enforcement

```json
{
  "llm_judge": true
}
```
