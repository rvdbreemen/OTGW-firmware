---
id: "ADR-196"
title: "SAT behaviour ground truth: Sergeant D's recorded decisions, then pinned thermo-nova, then specs, then firmware"
status: "Proposed"
date: "2026-10-23"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat"
  - "governance"
  - "ground-truth"
  - "thermo-nova"
  - "documentation-process"
aliases:
  - "SAT ground truth"
  - "source precedence"
  - "Sergeant D"
  - "George"
  - "sergeantd"
  - "sergeant-d-corrections"
  - "Sergeant D corrections"
  - "thermo-nova"
  - "pinned thermo-nova commit"
  - "thermo-nova-spec"
  - "SAT anti-drift rules"
components:
  - "SAT source precedence rule"
  - "docs/sat/sergeant-d-corrections.md"
symbols: []
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-196 SAT behaviour ground truth: Sergeant D's recorded decisions, then pinned thermo-nova, then specs, then firmware

## Status

Proposed, 2026-10-23.

Guideline-level per ADR-080: no CI (continuous integration) gate checks
whether a SAT decision followed this rule. `tests/test_sat_ground_truth_block.py`
is a drift guard for the text of the rule only (see Verification).

## Status History

```yaml
status_history:
  - date: 2026-10-23
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) in this OTGW (OpenTherm Gateway) firmware is a port of the thermo-nova
Home Assistant component. SAT was designed on paper by Sergeant D and turned into
code by Alex (Alexwijn); there is no written design document (CLAUDE.md, SAT
ground-truth block, lines 465-486). Sergeant D is the person the maintainer calls
George: ADR-150 line 41 names "George (the SAT author)", and
`docs/sat/sergeant-d-corrections.md` line 3 says "Sergeant D designed SAT". He
is Discord user `sergeantd`, not the 1.x beta tester GeorgeZ83 (project memory,
Discord people reference).

Earlier AI (artificial intelligence) work described SAT behaviour from memory or from the firmware and got
it wrong. The corrections file records concrete cases:

- **Entry 1 (2026-10-22).** The firmware used a fixed `base + 20` (47.2 °C)
  BT (boiler temperature) in the PWM (pulse-width modulation) duty cycle while
  nothing was learned. Sergeant D said BT is the real flow temperature, which
  matches thermo-nova `pwm.py:235-238`. The same entry shows a conflict between
  him and the code: he said a 30 s filter window, thermo-nova has 180 s
  (`HEATER_STARTUP_TIMEFRAME = 180`, `const.py:18`, used at `pwm.py:148-149`).
  The conflict was put to him, not picked, and he chose thermo-nova ("Ok, keep
  the `thermo-nova` solution", message 1562861690322817025).
- **Entries 7 and 11 (2026-10-23).** His decision overrides the code: the CH
  (central heating enable) bit is cleared below the per-system cold setpoint
  28.2 °C / 21 °C instead of thermo-nova's `COLD_SETPOINT = 22` (`const.py:34`,
  used at `heating_control.py:72` and `:238`). The firmware implements his value
  in `SAT_COLD_SETPOINT_RAD` / `SAT_COLD_SETPOINT_FLOOR`
  (`src/OTGW-firmware/SATcontrol.ino:57-58`) and `satColdCutoff`
  (`src/OTGW-firmware/SATchEnable.h:26-29`).
- **Entry 5 (2026-10-23).** We told him the firmware sent CS (control setpoint)
  =10 without CH=0 in the PWM off phase. That claim about our own firmware was wrong and
  was retracted on Discord. Even statements about what the port does need the
  code opened, and the port is never evidence for what SAT should do.
- **Entry 11, answer 3.** "Follow thermo-nova" for the integral freeze, mirrored
  as `if (state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral) return;`
  (`src/OTGW-firmware/SATpid.ino:157`) against `climate.py:501` and
  `pid.py:167-168`.

The order of sources is written today only in the SAT ground-truth block of
CLAUDE.md (lines 465-486) and AGENTS.md (lines 237-258). Agents read that block;
ADR authors and the adr-kit context lookup (`/adr-kit:context`) do not. A grep of
`docs/adr` for "ground truth", "sergeant-d-corrections", "thermo-nova-spec" and
"source of truth" (2026-10-23) found no ADR that decides the precedence. ADR-150,
ADR-157, ADR-178 and ADR-179 cite single rulings by Sergeant D. The Proposed
ADR-195 restates the order in its Context, but ADR-195 decides the
cold-state rule, so nothing records the order itself as a decision.

The maintainer asked on 2026-10-23 that future decisions are checked against
George's recorded decisions ("Zorg voor dat toekomstige beslissingen ook getoetst
worden aan de hand van de vastgelegde besluiten van George"). The same day two
directives were committed: record every Sergeant D correction in the corrections
file (TASK-1329, commit 77a2f614c) and check every SAT decision against it
(TASK-1330, commit 11b3756da).

Sergeant D validated the three SAT documents on 2026-10-22 (entry 4, message
1562888583004688629): "I checked the files and everything looks good so far.
Good job." The process rule in that entry ("Later corrections land here as
numbered entries; the documents are not rewritten silently") is our wording, not
his; this ADR adopts it as the documentation process.

## Decision

For every statement or change about SAT behaviour the sources rank in this
order, and a higher source wins over a lower one:

1. **Sergeant D's recorded corrections and decisions** in
   `docs/sat/sergeant-d-corrections.md`: one numbered, dated entry each, with
   his words quoted and the Discord message link.
2. **thermo-nova at the pinned commit**
   `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (2026-03-18,
   `releases/thermo-nova`); the read-only local copy is
   `other-projects/SAT-releases-thermo-nova/` (marker file `UPSTREAM_COMMIT`).
3. **`docs/sat/thermo-nova-spec.md`**, the cited specification written from that
   code (TASK-1292, commit 0b7329769).
4. **The firmware and its docs** (`src/OTGW-firmware/SAT*.ino`, `SATchEnable.h`,
   `SATtypes.h`, `docs/sat/firmware-sat-spec.md`, `docs/sat/sat-comparison.md`).
   They describe what the port does today and are never the authority for what
   SAT should do.

Later corrections are added as new numbered entries. A later entry that changes
an earlier one names it; old entries and the spec documents are not rewritten
silently.

Every new or superseding SAT ADR, plan, review and SAT-control change reads the
corrections file first. It cites the entries it relies on, or states that no
entry covers the topic. It cites thermo-nova as `file:line` relative to
`custom_components/sat` at the pinned commit, opened in the same session. A
conflict between Sergeant D and the code is reported to him with both
citations; it is never resolved by picking one silently.

The pinned commit moves only in a dedicated commit that updates the
SAT ground-truth block and re-diffs `docs/sat/thermo-nova-spec.md` against the
new upstream.

Scope: SAT behaviour only. The OpenTherm specification and the other reference
ladders in CLAUDE.md ("Debugging User-Reported Issues") are unchanged. The
CLAUDE.md/AGENTS.md block stays the operational text; this ADR records it as an
architectural decision so that ADR authors and `/adr-kit:context` find it.

## Decision Contract

### Must

- Read `docs/sat/sergeant-d-corrections.md` before writing a SAT ADR, plan,
  review or SAT-control change, and cite the matching entry numbers or state
  "no entry covers this topic".
- Cite thermo-nova as `file:line` at commit
  `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, opened in the current session.
- Record a new correction or decision by Sergeant D as a new numbered entry
  (date, verbatim quote, Discord link, thermo-nova and firmware comparison, task,
  what stays open), marking a short reply that we interpreted as an
  interpretation.
- Report a conflict between an entry and the thermo-nova code to Sergeant D
  with both citations.
- Move the pinned commit only in a dedicated commit that re-diffs
  `docs/sat/thermo-nova-spec.md`.

### Must Not

- Treat the firmware, `firmware-sat-spec.md` or `sat-comparison.md` as the
  authority for SAT behaviour.
- Rewrite an existing corrections entry or the spec documents silently.
- Fill a gap with plausible behaviour; write "not found in the code" and ask
  Sergeant D.
- Update the local thermo-nova copy without moving the pin as above.

### Exceptions

- Firmware-only features with no thermo-nova counterpart, where Sergeant D
  defers to the maintainer (entry 2, follow-thermostat with push setpoint), are
  decided by the maintainer and recorded in the same corrections entry.

### Verification

- `python tests/test_sat_ground_truth_block.py`: keeps the SAT ground-truth
  block identical in CLAUDE.md and AGENTS.md, pins the commit, and requires the
  corrections file to exist. It guards the rule's text, not compliance.
- Review: the reviewer reads the corrections file and treats an unmentioned
  conflict with an entry as a finding (CLAUDE.md block;
  `.claude/skills/implement-next-task/SKILL.md` lines 87, 102 and 158).
- `/adr-kit:context "SAT ground truth"` surfaces this ADR.

## Alternatives Considered

- **Keep the rule only in CLAUDE.md and AGENTS.md.** No new record to maintain.
  Rejected: ADR authors and the adr-kit context lookup do not read that block,
  and a superseding SAT ADR such as ADR-195 has no decision to cite.
- **thermo-nova code first, Sergeant D's corrections second.** The code is
  precise and versioned. Rejected: Sergeant D designed SAT and has overridden the
  code on purpose (entries 7 and 11, 28.2 / 21 instead of `COLD_SETPOINT = 22`,
  `const.py:34`). Ranking the code first would undo his decisions.
- **Firmware and its specs as the reference.** Cheapest, the code is local.
  Rejected: the firmware is the port under review; entry 1 (`base + 20`
  placeholder) and entry 5 (our wrong claim about CH=0) show it drifts from SAT.
- **Make this a binding ADR with a CI gate (ADR-080).** Rejected: whether a
  plan cited the right entry, or whether a conflict was put to Sergeant D, is a
  judgement that `evaluate.py` cannot check from a diff.

## Consequences

**Positive:**

- One recorded decision that every SAT ADR, plan and review can cite, found
  through `/adr-kit:context`.
- His decisions stop being lost between sessions: 12 entries were recorded on
  2026-10-22 and 2026-10-23, each with a Discord link.
- Conflicts between Sergeant D and thermo-nova are decided by him, as in entry 1
  (30 s against 180 s).

**Negative:**

- More work per SAT change: open the corrections file and the cited thermo-nova
  lines in every session. Mitigation: the corrections file is one document with
  numbered entries.
- SAT work can wait on Sergeant D's answer on Discord. Mitigation: the open item
  is recorded in the entry; work that does not depend on it continues.
- Two copies of the rule (the block and this ADR) can drift. Mitigation: the
  block is the operational text and is guarded by
  `tests/test_sat_ground_truth_block.py`; a change of order needs a superseding
  ADR.
- Short Discord replies need interpretation (entries 9, 11 and 12). Mitigation:
  each entry marks our reading as an interpretation.

## Open Questions

- [ ] Robert: should acceptance of this ADR wait until Sergeant D has confirmed
  that his recorded entries rank above the thermo-nova code for SAT, or is your
  directive of 2026-10-22 (the SAT ground-truth block) enough to accept it? The
  ranking follows from his role as designer and from entries 7 and 11; he has not
  been asked to confirm the ranking itself.

## Related Decisions

- **ADR-080 (Binding ADR rules must have a CI gate)**: meta-rule precedent; this
  ADR is guideline-level under it.
- **ADR-150 (SAT per-heating-system cold setpoint boiler-off cutoff)**: an
  earlier ruling by Sergeant D (named George there) that overrode thermo-nova's
  `COLD_SETPOINT`.
- **ADR-157 (SAT heating source manual config, auto-detect is a non-control
  hint)**, **ADR-178 (SAT area-sensor reference accepts a Bluetooth sensor
  address)** and **ADR-179 (owner
  of the OpenTherm control setpoint while SAT is enabled)**: SAT ADRs that cite
  single rulings; new SAT ADRs follow this precedence.
- **ADR-195 (SAT cold state and CH-enable rule, Proposed)**: the proposed SAT
  cold-state ADR that would replace ADR-150 on acceptance; it applies this order
  and cites entries 5, 7, 9, 11 and 12.
- **ADR-069 (SAT PID v3 controller implementation)**: the PID
  (proportional-integral-derivative) controller ported from thermo-nova; changes
  to it, such as the solar integral freeze in entry 11, follow this order.
- **ADR-071 (SAT heating curve algorithm)**: the heating curve the PID corrects;
  a change to the curve formula is a SAT behaviour change under this order.
- **ADR-085 (SAT Smart Autotune Thermostat integration)**: the umbrella decision
  that brought SAT into the firmware as a port; this ADR names what that port is
  checked against.

## References

- `docs/sat/sergeant-d-corrections.md`, entries 1 to 12 (2026-10-22 and
  2026-10-23), notably entry 1 (#alpha-testing,
  https://discord.com/channels/812969634638725140/1514720723980259460/1562827601834418319),
  entry 4 (message 1562888583004688629, channel not recorded in the entry),
  entry 7
  (https://discord.com/channels/812969634638725140/1514720723980259460/1563172561162862714)
  and entry 11
  (https://discord.com/channels/812969634638725140/1514720723980259460/1563232315969249376).
- CLAUDE.md lines 465-486 and AGENTS.md lines 237-258 (SAT ground-truth block).
- `tests/test_sat_ground_truth_block.py` (drift guard).
- `other-projects/SAT-releases-thermo-nova/UPSTREAM_COMMIT` (pinned commit,
  verified identical 2026-10-22).
- thermo-nova at 7beaaf5, relative to `custom_components/sat`: `const.py:18`,
  `const.py:34`, `heating_control.py:72`, `heating_control.py:238`,
  `pwm.py:148-149`, `pwm.py:235-238`, `climate.py:501`, `pid.py:167-168`.
- Firmware: `src/OTGW-firmware/SATchEnable.h:20-29`,
  `src/OTGW-firmware/SATcontrol.ino:57-58`,
  `src/OTGW-firmware/SATcontrol.ino:707`, `src/OTGW-firmware/SATpid.ino:157`.
- Commits: 0b7329769 (SAT specifications, TASK-1292), fa88f01b4 (validation
  recorded, TASK-1292), 77a2f614c (TASK-1329), 11b3756da (TASK-1330).
- Upstream: https://github.com/Alexwijn/SAT/tree/releases/thermo-nova

The decision governs how SAT decisions are documented and reviewed; a code diff
cannot show whether the right entry was consulted, so it has no Enforcement
block. It is guideline-level per ADR-080.
