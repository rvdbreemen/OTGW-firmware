# Spec T2: SAT page display states (TASK-2957, review F3 F7 F12)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. State semantics: answered by otgw-dev on 2026-11-20 (section 6).

**Scope: display only.** Nothing here changes what SAT sends to the boiler or when. The spec reads the fields that `/api/v2/sat/status` already sends and decides what the page says. Where a state cannot be told apart from today's fields, it says so and asks; it does not invent a field.

**Source of truth.** This spec describes what the firmware port *reports*, read from the code at origin/dev b3d14f76a. ADR-196 ranks Sergeant D's entries and thermo-nova above the firmware for SAT behaviour. The sub-lines below say what SAT does in a state, so they were checked against `docs/sat/sergeant-d-corrections.md`:

| Entry | Settles | Sub-line it supports | Later entries checked | Agrees |
|---|---|---|---|---|
| 47 | Lost OpenTherm: hand the boiler back (CS=0, CH=1, MM=T), then send nothing | PAUSED_OT "the boiler runs on its own control" | none supersedes | yes |
| 79 | OpenTherm back: SAT resumes by itself on the first boiler reply | PAUSED_OT "resumes by itself" | none supersedes | yes |
| 56 | Outside temperature missing or old: stop SAT at once | PAUSED_OUTSIDE "your thermostat controls the boiler" | 139, 330 refine the sources, not the stop | yes |
| 57 | Outside temperature counts as old after one hour | PAUSED_OUTSIDE "less than 1 hour old" | 126 (younger than 1 h, survives a restart), 139, 330 | yes |
| 121 | After a restart SAT stays quiet until a valid boiler reply and outside temperature | WAITING "waits until it can take over the boiler safely" | 126, 140 (NTP wait up to 60 s) | yes |
| 388 | After a reboot wait 60 s without sending a command | WAITING | 395 (start-up reads may run in the wait) | yes |

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
| 2 | STOPPED | enabled and `d.safety_tripped` | certain; one cause only (Q1: no OpenTherm command interface for 5 control ticks, `SATcontrol.ino:8532-8539`) |
| 3 | PAUSED_OT | enabled and `d.ot_link_lost` | certain (`SATcontrol.ino:4396`) |
| 4 | PAUSED_OUTSIDE | enabled and `d.outside_temp_missing` | certain (`SATcontrol.ino:4500`) |
| 5 | TAKEOVER | `!d.enabled && d.fallback_active` | certain (`SATcontrol.ino:7634-7637`) |
| 6 | WAITING | enabled and `!d.active` and none of 2 to 5 | certain on the condition; the reason is not in the status (Q2, Q3, Q7) |
| 7 | WINDOW | `d.window_open` | as today |
| 8 | SUMMER | `d.summer_active` | as today |
| 9 | HEATING | `satBurnerIsFiring(d)` | as today |
| 10 | IDLE | otherwise | split by room vs target, Q4 |

**TAKEOVER keeps its own headline**, as today. Dropping it was considered, so the headline could show whether the boiler heats. It was rejected: the user switch reads off during a takeover and `applySatLock()` locks the dial and presets (`v2.js:4785`, keyed on the user switch). A headline "Heating to 20°" next to a switch that says off would repeat the F3 contradiction. The takeover sub-line explains the switch instead, and the heating state stays visible in the pill.

**WAITING has no time limit, so its copy makes no time promise.** otgw-dev (Q2): `bActive` turns true only at the control tick (`SATcontrol.ino:8045`), and before that several gates return early. Some are short: boot silence 60 s (`:4441`, `:7968`), the NTP wait up to 60 s (`:4428`), up to one control interval (`:8038`). Others have no limit: waiting for the OpenTherm interface, no boiler reply since boot (`:4489`) and the MQTT-fallback gate (`:7928`). During a PIC flash `satDisable()` runs (`:7933-7935`), so a flash also shows WAITING (Q7).

**WAITING, boiler link down.** Q3: without an OpenTherm command interface (`:4375-4377`), in simulation or with PS=1 (`:4381-4383`), `ot_link_lost` never turns true, and SAT can sit in the no-boiler gate (`:4489`) indefinitely. So the SAT page also reads the boiler link the connection panel already uses (`CONN.boiler.s`, from `boilerOk` at `v2.js:3780`). When that link is down during WAITING, the page shows the "no boiler connection" variant. No new firmware field is needed.

**Follow-up proposal (for otgw-dev, not in this task):** a `wait_reason` field in `/v2/sat/status` (for example `boot_silence`, `ntp`, `no_interface`, `no_boiler`, `mqtt_fallback`, `flashing`) would let WAITING name its reason exactly. Without it the page stays generic, which is still honest.

**Both pause flags at once.** PAUSED_OT wins over PAUSED_OUTSIDE: without a boiler connection the outside temperature does not matter. The messages panel still lists both hint lines.

## 3. What each state shows

Pill = `#satPill`. Headline = `#satStatusBig`. Sub-line = a new line under the headline (`#satStateSub`, one sentence, normal weight, `--muted` colour, contrast per TASK-2956). "Live cards" = Control status (`#satControlStatus`), the control-loop chart card, PID, PWM & cycle tracking, Burner cycle mode.

| State | Pill text / style | Headline | Sub-line | Live cards |
|---|---|---|---|---|
| OFF | Off / `off` | SAT is off | (none) | values "—" as today; title unchanged |
| STOPPED | Stopped / `warn` | SAT stopped: no OpenTherm interface | SAT lost contact with the OpenTherm interface and stopped itself. Check the connection, then switch SAT off and on again to restart it. | muted, title + " (stopped)" |
| PAUSED_OT | Paused / `warn` | Paused: no boiler connection | SAT gets no reply from the boiler, so the boiler runs on its own control. SAT resumes by itself when the boiler answers again. | muted, title + " (paused)" |
| PAUSED_OUTSIDE | Paused / `warn` | Paused: no outside temperature | SAT needs an outside temperature less than 1 hour old; until then your thermostat controls the boiler. SAT resumes by itself when one arrives. [Set up a source] | muted, title + " (paused)" |
| TAKEOVER | Heating or Idle (by `satBurnerIsFiring`) / `warn` | Takeover (MQTT lost) | Home Assistant has been unreachable over MQTT for more than 5 minutes, so SAT controls the boiler. The switch stays off: SAT stops again when MQTT is back. | live |
| WAITING | Waiting / `idle` | Waiting to take over | SAT waits until it can take over the boiler safely: a boiler reply, the OpenTherm interface and a valid start state. | muted, title + " (waiting)" |
| WAITING, boiler link down | Waiting / `warn` | Waiting: no boiler connection | SAT waits for a reply from the boiler before it takes over. [Check connection ›] | muted, title + " (waiting)" |
| WINDOW | as today | Window open | (none) | live |
| SUMMER | as today | Summer mode | (none) | live |
| HEATING | Heating / default | Heating to {{temp}} | (none) | live |
| IDLE | Idle / `idle` | see 3.1 | (none) | live |

During a takeover the switch label (`#satEnableLbl`) reads "Off · takeover active" instead of "Disabled", so the switch and the headline tell the same story.

[Set up a source] is a link that opens Settings › SAT › Weather (the same target as the weather card link in spec T4, F11).

### 3.1 IDLE headline

"At target · idle" is only true when the room is at target. On .63 it showed with the room at 25.5 °C and the target at 19.8 °C.

The headline uses the firmware's own rule (Q4, otgw-dev): the tolerance band stops CH at room >= target + 0.1 °C (`SATchEnable.h:27-28`, ADR-250 partly superseded by ADR-253) and restarts below that after a 600 s minimum. `SAT_PID_DEADBAND` is 0.1 too (`SATtypes.h:479`).

| Condition | Headline |
|---|---|
| `room_temp` null | Idle |
| room >= target + 1.0 | Idle · room above target |
| room >= target + 0.1 (the band's stop condition) | Idle · room at target |
| room < target + 0.1, `boiler_status` = `anti_cycling` | Idle · anti-cycling pause |
| room < target + 0.1, any other idle status | Idle · {{boiler label}} (existing `tools.sat_boiler_*` labels, for example "Post-cycle"), or plain "Idle" without a label |

The 1.0 °C step for "above target" is a display rule with no firmware equivalent. It separates a normal overshoot after a band stop from a room that is clearly too warm (the .63 case, +5.7 °C). Below target + 0.1 while idle, the reason is usually the 600 s minimum off-time or anti-cycling. The status has no field for the minimum, so the page shows the boiler label instead of guessing.

### 3.2 Muted style for live cards

- Class `is-stale` on the card: values in `--muted`, no change in layout. The card title gets the suffix in the same element (" (paused)", " (waiting)", " (stopped)"), so a screen reader hears it.
- **Values that are not live while `active` is false show "—" instead of a muted number** (Q6, otgw-dev). P, I, D and `pid_output` are 0 after `satPidReset()` (`SATpid.ino:181-185`). `error` is not reset at all, so it is stale (`:217`, `:395`). A muted 0, or a stale error, still reads as a measurement.
- **Modulation label (Q6):** `current_modulation` is the MM that SAT sends (`SATcontrol.ino:8457`, `:8464`; 100 at the MM=T hand-back), not the boiler's own modulation (that is OpenTherm MsgID 17, RelModLevel). The Control status row is renamed "Modulation sent" and shows "—" while `active` is false. The boiler's own value belongs on Home and in Monitor, which already show MsgID 17.
- The measured tiles (Outside, Room, Target) stay live in every state: they are measurements, not control output. Boiler setpoint already shows "—" while `active` is false (`SATcontrol.ino:4836` sends NaN).
- The control-loop chart: the "live" label (`v2.html:386`, `tools.sat_loop_live`) is replaced by the state word ("paused", "waiting", "stopped", "off"). The empty-data overlay is in spec T4, F13.

### 3.3 Burner cycle mode (F12)

| `control_mode` | Big | Sub-line |
|---|---|---|
| 0 | Off | SAT is not actively controlling. |
| 1 | Continuous | PID drives the burner continuously. SAT chooses the mode itself. |
| 2 | PWM | PID modulates the burner in PWM bursts. SAT chooses the mode itself. |

Only `tools.sat_cycle_sub_off` changes. **Do not derive the card from `control_mode` alone** (Q5, otgw-dev). `satDisable()` sets OFF (`SATcontrol.ino:4285`), but the `control_mode` command needs only enabled and not tripped (`:4583-4597`), and the boot sets CONTINUOUS (`:4046`). So the mode can read Continuous while `active` is false. Rule: in STOPPED, PAUSED_OT, PAUSED_OUTSIDE and WAITING the card shows "—" with the Off sub-line, whatever `control_mode` says.

### 3.4 Messages badge

The badge (`#satMsgBtn`) gets visible text next to the icon: "1 message" / "{{count}} messages" (plural keys), so it no longer reads as a bare red number. The existing aria-label stays. With a warning it keeps the `warn` style.

## 4. Copy (EN)

Existing keys keep their names; only their values change, so the NL and DE entries stay attached. NL and DE follow through the existing i18n flow (the `translate_webui_otgw-firmware` skill, mode sync); no other language is written here.

**Keys whose meaning changes** (the i18n completeness tests stay green while NL and DE still say the old thing, so these need a re-translation, not just a fill):

| Key | Old EN | New EN |
|---|---|---|
| `tools.sat_big_safety` | Safety tripped! | SAT stopped: no OpenTherm interface |
| `tools.kv_modulation` | Modulation | Modulation sent |
| `tools.sat_big_at_target` | At target · idle | Idle · room at target |
| `tools.sat_cycle_sub_off` | SAT is not actively controlling. SAT chooses the mode itself. | SAT is not actively controlling. |

**New keys:**

| Key | EN |
|---|---|
| `tools.sat_pill_paused` | Paused |
| `tools.sat_pill_waiting` | Waiting |
| `tools.sat_pill_stopped` | Stopped |
| `tools.sat_big_paused_ot` | Paused: no boiler connection |
| `tools.sat_big_paused_outside` | Paused: no outside temperature |
| `tools.sat_big_waiting` | Waiting to take over |
| `tools.sat_big_waiting_boiler` | Waiting: no boiler connection |
| `tools.sat_big_idle` | Idle |
| `tools.sat_big_idle_above` | Idle · room above target |
| `tools.sat_big_idle_status` | Idle · {{status}} |
| `tools.sat_sub_takeover` | Home Assistant has been unreachable over MQTT for more than 5 minutes, so SAT controls the boiler. The switch stays off: SAT stops again when MQTT is back. |
| `tools.sat_enable_takeover` | Off · takeover active |
| `tools.sat_sub_stopped` | SAT lost contact with the OpenTherm interface and stopped itself. Check the connection, then switch SAT off and on again to restart it. |
| `tools.sat_sub_paused_ot` | SAT gets no reply from the boiler, so the boiler runs on its own control. SAT resumes by itself when the boiler answers again. |
| `tools.sat_sub_paused_outside` | SAT needs an outside temperature less than 1 hour old; until then your thermostat controls the boiler. SAT resumes by itself when one arrives. |
| `tools.sat_sub_source_link` | Set up a source |
| `tools.sat_sub_waiting` | SAT waits until it can take over the boiler safely: a boiler reply, the OpenTherm interface and a valid start state. |
| `tools.sat_sub_waiting_boiler` | SAT waits for a reply from the boiler before it takes over. |
| `tools.sat_card_paused` | (paused) |
| `tools.sat_card_waiting` | (waiting) |
| `tools.sat_card_stopped` | (stopped) |
| `tools.sat_loop_state_paused` / `_waiting` / `_stopped` / `_off` | paused / waiting / stopped / off |
| `tools.sat_msgs_one` / `tools.sat_msgs_other` | 1 message / {{count}} messages |

The hint lines `h_mon.sat_ot_lost` and `h_mon.sat_outside_missing` stay in the messages panel unchanged.

## 5. Edge cases

- **Resume.** After a pause ends, `active` turns true only at the next control tick (`SATcontrol.ino:8044`), so WAITING shows for up to one control interval. That is accurate ("SAT takes over again"), not a glitch.
- **Simulation.** `d.simulation` true: states are derived the same way; the existing SIMULATION badge stays. In simulation the OpenTherm-loss check is skipped (`SATcontrol.ino:4380-4384`), so PAUSED_OT does not show.
- **No status yet / fetch failed.** Keep today's behaviour (the `#satNoData` card, `v2.html:352`, key `h_mon.sat_nodata`); the state table only runs on a status object.
- **Settings lock.** `applySatLock()` keys on the user switch (`satIsOn()`, `v2.js:4770`), not on the display state: paused does not lock the settings. That is right, because the user can still change them.
- **Takeover ends while paused.** The pill drops back to Paused on the next poll; no transition text.

## 6. Answers from otgw-dev (2026-11-20, read from origin/dev b3d14f76a, relayed by the coordinator)

- **Q1.** The only writer of `bSafetyTripped` is `SATcontrol.ino:8536`: no OpenTherm command interface for 5 control ticks in a row (`SAT_MAX_PIC_FAILS`, `:148`; `:8532-8539`). The status has no reason field (`safety_tripped`, `:4953`; MQTT `sat/safety_tripped`, `:5151`). Applied: the STOPPED headline and sub-line name the cause.
- **Q2.** `bActive` is set true only at `:8045`. Gates before it: safety trip `:7960`, boot silence 60 s (`:4441`, `:7968`), OpenTherm link lost `:7969` (no limit, until a boiler reply), `satRestartStep()` `:7972` (NTP up to 60 s `:4428`; waiting for the interface, no limit; no boiler since boot `:4489`, no limit; outside temperature missing, no limit), MQTT fallback `:7928`, flashing `:7933`, plus up to one control interval `:8038`. Applied: STARTING became WAITING, with no time promise.
- **Q3.** Mostly yes (lost after 30 s with `iBoilerLastSeenMs == 0`, `:4361-4367`), but not without an OpenTherm command interface (`:4375-4377`) or in simulation or with PS=1 (`:4381-4383`). Then `ot_link_lost` never turns true, and SAT can stay in the `:4489` gate indefinitely. Applied: the "WAITING, boiler link down" variant, from the connection panel's boiler link.
- **Q4.** No 0.3: `SAT_PID_DEADBAND` is 0.1 (`SATtypes.h:479`), and the tolerance band stops CH at room >= target + 0.1 (`SATchEnable.h:27-28`, ADR-250/253), restarting below that after a 600 s minimum. Applied: section 3.1 uses the band.
- **Q5.** Confirmed, and the mode CAN be non-OFF while `active` is false: the `control_mode` command (`:4583-4597`) needs only enabled and not tripped, the boot sets CONTINUOUS (`:4046`), and MQTT `sat/mode` masks only disabled or tripped (`:5172`). Applied: section 3.3 does not derive the card from the mode in the not-active states.
- **Q6.** P, I, D and `pid_output` are 0 after `satPidReset()` (`SATpid.ino:181-185`); `error` is not reset and goes stale (`:217`, `:395`). `current_modulation` is the MM that SAT sends (`SATcontrol.ino:8457`, `:8464`; 100 at MM=T), not the boiler's value (MsgID 17, RelModLevel). Applied: "—" while not active, and the row renamed "Modulation sent".
- **Q7.** No: during a flash `satDisable()` runs (`:7933-7935`) and `bActive` turns false, so the page shows WAITING. Applied in section 2.

## 7. Acceptance (replaces the ACs of TASK-2957 where they differ)

1. The answers of section 6 are applied as written there (done in this spec; the builder only checks the code still matches the cited lines).
2. Each state in section 2 renders the pill, headline, sub-line and card treatment of section 3, checked by a browser test that feeds one status object per state (10 states, plus the WAITING variant with the boiler link down); during TAKEOVER the switch label reads "Off · takeover active".
3. With `enabled: true, ot_link_lost: true, boiler_status: "idle"` (the .63 case), the page never shows "At target" or "idle" in the headline.
4. The IDLE headline follows 3.1; with room 25.5 and target 19.8 it reads "Idle · room above target", with room 20.0 and target 19.8 "Idle · room at target".
5. The messages badge shows visible text and keeps its aria-label.
5a. While `active` is false, Control status shows "—" for P, I, D, PID output, Error and Modulation; the Modulation row is labelled "Modulation sent"; the Burner cycle card shows "—" in STOPPED, PAUSED_OT, PAUSED_OUTSIDE and WAITING even with `control_mode` 1 or 2.
6. The Burner cycle Off sub-line is "SAT is not actively controlling."
7. New strings exist as EN keys; NL and DE are filled through the i18n sync, and the i18n tests (`tests/webui/i18n-*.test.mjs`) pass.
8. The four keys whose meaning changed (`sat_big_safety`, `sat_big_at_target`, `sat_cycle_sub_off`, `kv_modulation`) have re-translated NL and DE values, visible in the sync diff.
9. No change to any file outside the web UI (`src/OTGW-firmware/data/`) and its tests.
