# Spec T2: SAT page display states (TASK-2957, review F3 F7 F12)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. State semantics to confirm: otgw-dev (questions Q1 to Q7 below).

**Scope: display only.** Nothing here changes what SAT sends to the boiler or when. The spec reads the fields that `/api/v2/sat/status` already sends and decides what the page says. Where a state cannot be told apart from today's fields, it says so and asks; it does not invent a field.

**Source of truth.** This spec describes what the firmware port *reports*, read from the code at origin/dev b3d14f76a. ADR-196 ranks Sergeant D's entries and thermo-nova above the firmware for SAT behaviour. The sub-lines below say what SAT does in a state, so they were checked against `docs/sat/sergeant-d-corrections.md`:

| Entry | Settles | Sub-line it supports | Later entries checked | Agrees |
|---|---|---|---|---|
| 47 | Lost OpenTherm: hand the boiler back (CS=0, CH=1, MM=T), then send nothing | PAUSED_OT "the boiler runs on its own control" | none supersedes | yes |
| 79 | OpenTherm back: SAT resumes by itself on the first boiler reply | PAUSED_OT "resumes by itself" | none supersedes | yes |
| 56 | Outside temperature missing or old: stop SAT at once | PAUSED_OUTSIDE "your thermostat controls the boiler" | 139, 330 refine the sources, not the stop | yes |
| 57 | Outside temperature counts as old after one hour | PAUSED_OUTSIDE "less than 1 hour old" | 126 (younger than 1 h, survives a restart), 139, 330 | yes |
| 121 | After a restart SAT stays quiet until a valid boiler reply and outside temperature | STARTING "waits for the boiler and its saved state" | 126, 140 (NTP wait up to 60 s) | yes |
| 388 | After a reboot wait 60 s without sending a command | STARTING | 395 (start-up reads may run in the wait) | yes |

The safety-trip and takeover sub-lines describe firmware rules (`SATcontrol.ino:8533-8538`, `:7625-7648`), not thermo-nova behaviour; Q1 asks otgw-dev to confirm them.

## 1. What the page does today

`renderSatPage()` in `v2.js:7365-7385` picks the headline in this order:

```
!enabled            -> "SAT is off"
safety_tripped      -> "Safety tripped!"
takeover            -> "Takeover (MQTT lost)"
window_open         -> "Window open"
summer_active       -> "Summer mode"
burner firing       -> "Heating to {{temp}}"
otherwise           -> "At target · idle"
```

`enabled` = the user switch (`d.enabled`) or an MQTT takeover (`!d.enabled && d.fallback_active`). "Burner firing" = `satBurnerIsFiring()` (`v2.js:6999-7002`): `boiler_status` not in `off, idle, anti_cycling, post_cycle, overshoot_cooling`.

**The defect (F3).** The chain never looks at `ot_link_lost` or `outside_temp_missing`. Both are set right after `satDisable()` hands the boiler back (`SATcontrol.ino:4389-4398` and `:4497-4504`). The user switch stays on, so `enabled` is true, `boiler_status` reads idle, and the headline falls through to "At target · idle". Only the hint lines in the messages panel (`v2.html:376-377`) say "paused". The same happens in the boot window before the first control tick: `active` is false until `SATcontrol.ino:8044`, but nothing on the page shows it.

**Correction to the review.** F3 said the red badge has no label. It has one: `aria-label="Messages: {{count}}"` (`v2.js:7293`, key `tools.sat_msgs_aria`). The visible badge shows only "!" and the count, so sighted users still get no word. The fix below adds visible text; the aria-label stays.

## 2. Display states

The page derives one **display state** per status poll, in this priority order. The first match wins.

| # | State | Condition (existing fields only) | Confidence |
|---|---|---|---|
| 1 | OFF | `!d.enabled && !d.fallback_active` | certain (same as today) |
| 2 | STOPPED | enabled and `d.safety_tripped` | certain on the flag; cause text: Q1 |
| 3 | PAUSED_OT | enabled and `d.ot_link_lost` | certain (`SATcontrol.ino:4396`) |
| 4 | PAUSED_OUTSIDE | enabled and `d.outside_temp_missing` | certain (`SATcontrol.ino:4500`) |
| 5 | TAKEOVER | `!d.enabled && d.fallback_active` | certain (`SATcontrol.ino:7634-7637`) |
| 6 | STARTING | enabled and `!d.active` and none of 2 to 5 | likely; Q2, Q3, Q7 |
| 7 | WINDOW | `d.window_open` | as today |
| 8 | SUMMER | `d.summer_active` | as today |
| 9 | HEATING | `satBurnerIsFiring(d)` | as today |
| 10 | IDLE | otherwise | split by room vs target, Q4 |

**TAKEOVER keeps its own headline**, as today. Dropping it was considered, so the headline could show whether the boiler heats. It was rejected: the user switch reads off during a takeover and `applySatLock()` locks the dial and presets (`v2.js:4785`, keyed on the user switch). A headline "Heating to 20°" next to a switch that says off would repeat the F3 contradiction. The takeover sub-line explains the switch instead, and the heating state stays visible in the pill.

**Both pause flags at once.** PAUSED_OT wins over PAUSED_OUTSIDE: without a boiler connection the outside temperature does not matter. The messages panel still lists both hint lines.

## 3. What each state shows

Pill = `#satPill`. Headline = `#satStatusBig`. Sub-line = a new line under the headline (`#satStateSub`, one sentence, normal weight, `--muted` colour, contrast per TASK-2956). "Live cards" = Control status (`#satControlStatus`), the control-loop chart card, PID, PWM & cycle tracking, Burner cycle mode.

| State | Pill text / style | Headline | Sub-line | Live cards |
|---|---|---|---|---|
| OFF | Off / `off` | SAT is off | (none) | values "—" as today; title unchanged |
| STOPPED | Stopped / `warn` | SAT stopped: safety check | SAT stopped itself after a safety check failed. Switch SAT off and on again to restart it. | muted, title + " (stopped)" |
| PAUSED_OT | Paused / `warn` | Paused: no boiler connection | SAT gets no reply from the boiler, so the boiler runs on its own control. SAT resumes by itself when the boiler answers again. | muted, title + " (paused)" |
| PAUSED_OUTSIDE | Paused / `warn` | Paused: no outside temperature | SAT needs an outside temperature less than 1 hour old; until then your thermostat controls the boiler. SAT resumes by itself when one arrives. [Set up a source] | muted, title + " (paused)" |
| TAKEOVER | Heating or Idle (by `satBurnerIsFiring`) / `warn` | Takeover (MQTT lost) | Home Assistant has been unreachable over MQTT for more than 5 minutes, so SAT controls the boiler. The switch stays off: SAT stops again when MQTT is back. | live |
| STARTING | Starting / `idle` | Starting… | SAT waits for the boiler and its saved state before it takes over. | muted, title + " (starting)" |
| WINDOW | as today | Window open | (none) | live |
| SUMMER | as today | Summer mode | (none) | live |
| HEATING | Heating / default | Heating to {{temp}} | (none) | live |
| IDLE | Idle / `idle` | see 3.1 | (none) | live |

During a takeover the switch label (`#satEnableLbl`) reads "Off · takeover active" instead of "Disabled", so the switch and the headline tell the same story.

[Set up a source] is a link that opens Settings › SAT › Weather (the same target as the weather card link in spec T4, F11).

### 3.1 IDLE headline

"At target · idle" is only true when the room is near the target. On .63 it showed with the room at 25.5 °C and the target at 19.8 °C.

| Condition | Headline |
|---|---|
| `room_temp` null | Idle |
| room within the display band of target (Q4) | Idle · room at target |
| room above target + band | Idle · room above target |
| room below target − band, `boiler_status` = `anti_cycling` | Idle · anti-cycling pause |
| room below target − band, any other idle status | Idle · {{boiler label}} (existing `tools.sat_boiler_*` labels, for example "Post-cycle") |

Proposed band: 0.3 °C, display only. Q4 asks whether the firmware has a deadband value the page should reuse instead.

### 3.2 Muted style for live cards

- Class `is-stale` on the card: values in `--muted`, no change in layout. The card title gets the suffix in the same element (" (paused)", " (starting)", " (stopped)"), so a screen reader hears it.
- The measured tiles (Outside, Room, Target) stay live in every state: they are measurements, not control output. Boiler setpoint already shows "—" while `active` is false (`SATcontrol.ino:4836` sends NaN).
- The control-loop chart: the "live" label (`v2.html:386`, `tools.sat_loop_live`) is replaced by the state word ("paused", "starting", "stopped", "off"). The empty-data overlay is in spec T4, F13.

### 3.3 Burner cycle mode (F12)

| `control_mode` | Big | Sub-line |
|---|---|---|
| 0 | Off | SAT is not actively controlling. |
| 1 | Continuous | PID drives the burner continuously. SAT chooses the mode itself. |
| 2 | PWM | PID modulates the burner in PWM bursts. SAT chooses the mode itself. |

Only `tools.sat_cycle_sub_off` changes. `satDisable()` calls `satSetControlMode(SAT_MODE_OFF)` (`SATcontrol.ino:4285`) on every hand-back (OpenTherm loss, outside temperature missing, safety trip), so the Off text also covers the pauses.

### 3.4 Messages badge

The badge (`#satMsgBtn`) gets visible text next to the icon: "1 message" / "{{count}} messages" (plural keys), so it no longer reads as a bare red number. The existing aria-label stays. With a warning it keeps the `warn` style.

## 4. Copy (EN)

Existing keys keep their names; only their values change, so the NL and DE entries stay attached. NL and DE follow through the existing i18n flow (the `translate_webui_otgw-firmware` skill, mode sync); no other language is written here.

**Keys whose meaning changes** (the i18n completeness tests stay green while NL and DE still say the old thing, so these need a re-translation, not just a fill):

| Key | Old EN | New EN |
|---|---|---|
| `tools.sat_big_safety` | Safety tripped! | SAT stopped: safety check |
| `tools.sat_big_at_target` | At target · idle | Idle · room at target |
| `tools.sat_cycle_sub_off` | SAT is not actively controlling. SAT chooses the mode itself. | SAT is not actively controlling. |

**New keys:**

| Key | EN |
|---|---|
| `tools.sat_pill_paused` | Paused |
| `tools.sat_pill_starting` | Starting |
| `tools.sat_pill_stopped` | Stopped |
| `tools.sat_big_paused_ot` | Paused: no boiler connection |
| `tools.sat_big_paused_outside` | Paused: no outside temperature |
| `tools.sat_big_starting` | Starting… |
| `tools.sat_big_idle` | Idle |
| `tools.sat_big_idle_above` | Idle · room above target |
| `tools.sat_big_idle_status` | Idle · {{status}} |
| `tools.sat_sub_takeover` | Home Assistant has been unreachable over MQTT for more than 5 minutes, so SAT controls the boiler. The switch stays off: SAT stops again when MQTT is back. |
| `tools.sat_enable_takeover` | Off · takeover active |
| `tools.sat_sub_stopped` | SAT stopped itself after a safety check failed. Switch SAT off and on again to restart it. |
| `tools.sat_sub_paused_ot` | SAT gets no reply from the boiler, so the boiler runs on its own control. SAT resumes by itself when the boiler answers again. |
| `tools.sat_sub_paused_outside` | SAT needs an outside temperature less than 1 hour old; until then your thermostat controls the boiler. SAT resumes by itself when one arrives. |
| `tools.sat_sub_source_link` | Set up a source |
| `tools.sat_sub_starting` | SAT waits for the boiler and its saved state before it takes over. |
| `tools.sat_card_paused` | (paused) |
| `tools.sat_card_starting` | (starting) |
| `tools.sat_card_stopped` | (stopped) |
| `tools.sat_loop_state_paused` / `_starting` / `_stopped` / `_off` | paused / starting / stopped / off |
| `tools.sat_msgs_one` / `tools.sat_msgs_other` | 1 message / {{count}} messages |

The hint lines `h_mon.sat_ot_lost` and `h_mon.sat_outside_missing` stay in the messages panel unchanged.

## 5. Edge cases

- **Resume.** After a pause ends, `active` turns true only at the next control tick (`SATcontrol.ino:8044`), so STARTING shows for up to one control interval. That is accurate ("SAT takes over again"), not a glitch.
- **Simulation.** `d.simulation` true: states are derived the same way; the existing SIMULATION badge stays. In simulation the OpenTherm-loss check is skipped (`SATcontrol.ino:4380-4384`), so PAUSED_OT does not show.
- **No status yet / fetch failed.** Keep today's behaviour (the `#satNoData` card, `v2.html:352`, key `h_mon.sat_nodata`); the state table only runs on a status object.
- **Settings lock.** `applySatLock()` keys on the user switch (`satIsOn()`, `v2.js:4770`), not on the display state: paused does not lock the settings. That is right, because the user can still change them.
- **Takeover ends while paused.** The pill drops back to Paused on the next poll; no transition text.

## 6. Questions for otgw-dev (collected by the coordinator)

- **Q1.** Is the PIC-unavailable path (`SATcontrol.ino:8533-8538`, `SAT_MAX_PIC_FAILS`) the only place that sets `bSafetyTripped`? If there are more causes, should the status carry a reason so the sub-line can name it? Until answered, the sub-line stays generic.
- **Q2.** STARTING = enabled, `!active`, no pause or trip flag. Which gates can hold SAT there: the 60 s boot silence (`:4438-4448`), the boot decide wait (up to `SAT_BOOT_DECIDE_MS` = 60 s), the wait for a first boiler reply (`:4489`)? Any other, and can one of them last longer than about 2 minutes?
- **Q3.** With no boiler on the bus since boot, does `ot_link_lost` always turn true after 30 s (so the page shows PAUSED_OT, as seen on .63), or can SAT sit in the `iBoilerLastSeenMs == 0` gate without that flag?
- **Q4.** Is there a SAT deadband or "at target" tolerance in the firmware that the IDLE headline should use instead of a 0.3 °C display band? Display only; no behaviour change asked.
- **Q5.** (answered in the code, confirm only) `satDisable()` sets the mode to OFF (`:4285`), `bActive` false and `fFinalSetpoint` 0 (`:4291-4292`), and resets the PID (`satPidReset()`, `:4294`). Can anything set the mode back to non-OFF while `active` stays false?
- **Q6.** After `satPidReset()`, are `pid_output` and `error` in `/v2/sat/status` 0 while paused, and is `current_modulation` the boiler's reported value (so still live)? The muted style is right either way; the answer decides whether "—" would be more honest than a muted 0 for the PID fields.
- **Q7.** `satControlLoopRun()` returns early for several gates before the tick (the comment at `_sat_tuneGatesPassed`, `:7973`, names disabled, flashing, MQTT fallback, safety, OT link, restart). During a PIC flash the loop returns early: does `bActive` keep its last value of true, so the page would show HEATING or IDLE while SAT sends nothing? If so, should the status carry a flag for it, or is a flash short enough to ignore?

## 7. Acceptance (replaces the ACs of TASK-2957 where they differ)

1. otgw-dev's answers to Q1 to Q4, Q6 and Q7 (and the Q5 confirmation) are in the task notes; where an answer changes a condition in section 2, this spec is updated first.
2. Each state in section 2 renders the pill, headline, sub-line and card treatment of section 3, checked by a browser test that feeds one status object per state (10 states); during TAKEOVER the switch label reads "Off · takeover active".
3. With `enabled: true, ot_link_lost: true, boiler_status: "idle"` (the .63 case), the page never shows "At target" or "idle" in the headline.
4. The IDLE headline follows 3.1; with room 25.5 and target 19.8 it reads "Idle · room above target".
5. The messages badge shows visible text and keeps its aria-label.
6. The Burner cycle Off sub-line is "SAT is not actively controlling."
7. New strings exist as EN keys; NL and DE are filled through the i18n sync, and the i18n tests (`tests/webui/i18n-*.test.mjs`) pass.
8. The three keys whose meaning changed (`sat_big_safety`, `sat_big_at_target`, `sat_cycle_sub_off`) have re-translated NL and DE values, visible in the sync diff.
9. No change to any file outside the web UI (`src/OTGW-firmware/data/`) and its tests.
