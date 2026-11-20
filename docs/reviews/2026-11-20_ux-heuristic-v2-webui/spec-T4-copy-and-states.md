# Spec T4: copy, empty and error states (TASK-2959, review F4 F5 F6 F10 F11 F13 F15)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. Copy for F5: BlueFlame (draft below, marked). Web UI only; EN strings, NL and DE through the existing i18n sync.

Code read at origin/dev b3d14f76a. Line numbers are from that commit.

## F4. Home status: no healthy words without a healthy bus

**Today.** `statusSentence()` (`v2.js:2985-2995`) falls through to `home.status_standby` "Standby · everything quiet" whenever there is no fault, no DHW, no flame and no CH demand. It does not look at the bus. The header summary shows `graph.all_ok` "All systems connected" before the first health poll has answered, and also when the mock answers `{}`.

**New order** for the Home status line (first match wins):

| # | Condition | Text (EN) | Key |
|---|---|---|---|
| 1 | no successful `/api/v2/otgw/otmonitor` answer yet since the page opened | Waiting for data from the gateway… | `home.status_waiting` (new) |
| 2 | `model.fault` | Fault reported — check the appliance display. | `home.status_fault` (as today) |
| 3 | boiler link down: the same `boilerOk` the connection panel uses (`v2.js:3780`: PIC `boilerconnected`, OT-Direct `otgwconnected`) is false | No boiler connected · [Check connection ›] | `home.status_no_boiler` (new), `home.status_check_conn` (new) |
| 4 to 7 | DHW, flame, CH demand | as today | as today |
| 8 | otherwise, with the boiler link up | Standby · everything quiet | `home.status_standby` (as today) |

- [Check connection ›] opens the connection panel, the same action as clicking the header summary (`button.connsum`).
- Row 3 needs the health poll; until its first answer, row 1 holds.
- **Header summary:** before the first health answer it reads `graph.conn_checking` "checking…" (an existing key) instead of "All systems connected".

## F6. Simulation modal: wait for the answer, show failures

**Today.** `showSimModal()` (`v2.js:8515-8537`): "Enable simulation" fires the POST, swallows errors (`.catch(function () { })`) and closes the modal at once.

**States:**

| State | Primary button | "Not now" | Body |
|---|---|---|---|
| idle | Enable simulation | enabled | `sat.sim_body` (as today) |
| pending | Enabling… (disabled, `aria-busy="true"`) | disabled | as today |
| success | the modal closes, toast `sat.sim_enabled` "Simulation enabled" | | |
| error | Try again | enabled | as today, plus a line under it in the error colour: "Could not enable simulation ({{reason}}). Try again, or switch it on under Settings › SAT › Simulation." |

- `{{reason}}`: "HTTP {{status}}" for a non-2xx answer, "no answer from the gateway" for a network error or timeout.
- The error line gets `role="alert"` so a screen reader hears it.
- The backdrop click keeps setting `otgw-sim-prompted` (unchanged); during pending it does nothing.
- **Optional (recommended):** offer the modal only while the user is on Home, so it does not cover work on Settings or Monitor. When the user is elsewhere, show it on the next visit to Home.

Copy keys: `sat.sim_enabling` "Enabling…", `sat.sim_retry` "Try again", `sat.sim_failed` (the error line), `sat.sim_reason_http` "HTTP {{status}}", `sat.sim_reason_net` "no answer from the gateway".

## F10. Long setting hints move into the (i) popover

**Today.** The settings row prints the whole `hint` under the label. For `satsource` (Appliance type, `v2.js:4233`) that is 13 lines in a narrow column.

**Rule:** a hint under a label renders as at most 2 lines at 1400 px (measured in the browser, not counted in characters, because the label column is narrow). Longer help goes into the existing (i) popover (`.sinfo`), and the inline line points to it.

**Where the popover text comes from.** `helpFor()` (`v2.js`) returns `SET_META[k].help` first, and only falls back to `settings-help.json`, which the firmware serves from `settingsHelp.h`. So the long text moves into `SET_META[k].help` in `v2.js`, and the change stays inside `src/OTGW-firmware/data/`. `settingsHelp.h` is not touched. For `satsource` it holds a shorter text ("Heat source: auto (gas boiler), gas boiler, heat pump or hybrid…", line 75), which the new `help` then shadows in the v2 UI.

For `satsource`:
- Inline hint (EN): "What produces the heat. The (i) explains each type."
- Popover (`SET_META.satsource.help`): the current hint text, unchanged.

otgw-webui lists the other settings whose hint renders longer than 2 lines at 1400 px and applies the same rule. Where a hint has no short form yet, the inline line is "See (i) for details." and the coordinator assigns the short copy to me.

## F11. Weather card: no HTTPS dead end

**Today.** `tools.sat_geo_hint` (`v2.js:1621`): "Location detection needs HTTPS. Enter Latitude and Longitude under SAT settings > Weather, or let Home Assistant publish sat/sun_elevation." The "Detect location" button uses browser geolocation, which browsers allow only in a secure context. The gateway serves HTTP only, so the button can never work from the gateway's own address.

**New:**
- Show "Detect location" only when `window.isSecureContext` is true (for example behind a Home Assistant ingress with HTTPS). Otherwise hide it.
- Hint (EN), key `tools.sat_geo_hint` changed: "Enter your Latitude and Longitude under SAT settings › Weather, or let Home Assistant publish sat/sun_elevation." The words "SAT settings › Weather" are a link that opens that group and focuses the Latitude field. While SAT is off the SAT settings are locked; the link then opens the group and focuses its heading instead, where the existing off-note (`settings.sat_off_note`) explains the lock.
- `tools.sat_geo_needs_https` is no longer shown on an HTTP page.
- The manual says the same: `docs/manuals/en/ch05-sat-thermostat.md` §5.9 and the NL chapter, owner otgw-docs.

## F13. Empty and paused charts

**SAT control loop** (`#satHistChart`, `satDrawHistory()`, `v2.js:7518`):

| Condition | Overlay text (centred, `--muted`) | "live" label |
|---|---|---|
| SAT off | No control data: SAT is off. | "off" |
| paused or stopped (spec T2) | No new control data: SAT is paused. | "paused" / "stopped" |
| waiting (spec T2) | No control data yet: SAT is waiting to take over. | "waiting" |
| SAT on, fewer than 2 points | Collecting data… | hidden |
| SAT on, 2 or more points | (no overlay) | "live" |

**Dependency:** the paused, stopped and waiting rows need the display-state derivation of spec T2 (TASK-2957); otgw-dev answered its questions on 2026-11-20. The off, collecting and live rows and the heating-curve empty state can land first.

Points collected before a pause stay drawn; the overlay then sits in the top-left corner instead of the centre, so it does not hide them.

**Heating curve** (`#satCurveBig`, `satDrawCurve()`): today the SVG is emptied when `coefficient` or `target_temp` is null, which leaves a blank card of about 400 px. New: the card shows one line, "The heating curve appears once SAT knows its coefficient and target.", and keeps its height at 120 px instead of 400.

Copy keys: `tools.sat_loop_empty_off`, `tools.sat_loop_empty_paused`, `tools.sat_loop_empty_waiting`, `tools.sat_loop_collecting`, `tools.sat_curve_empty`.

## F15. BLE discovery card

**Today.** `.discover-bd .s` sets `font-family: var(--font-mono)` (`v2.css:876`), so the hint "View them in Settings › Sensors." is in mono. The stack is fixed at the bottom right (`v2.css:865`) and covers whatever card is under it until it is dismissed.

**New (decided by the coordinator, 2026-11-20: the base only, no auto-collapse, KISS):**
- `.discover-bd .s` uses the UI font. Mono stays only for real identifiers (a MAC or sensor address) inside the text, as a `<code>` span.
- The stack keeps `max-width: 340px` and the × to dismiss (unchanged). At 390 px it spans the full width minus 16 px margins.

*Considered and rejected:* auto-collapse into a "N new sensors ›" chip after 15 s. It would fix "covers content", but adds a timer and a second component; the × already dismisses the card.

## F5. SAT beta banner (copy by BlueFlame)

**Today.** `#satBetaNotice` (`v2.html:355-357`, keys `tools.sat_beta_head` and `tools.sat_beta_body`): a bold paragraph with two script paths, a Discord invite and a GitHub URL. It shows on prerelease versions only (`v2.js:6940`), so 2.0.0 stable removes it.

**Copy approved by BlueFlame, 2026-11-20** (one line, normal weight, warn style per TASK-2956):

> SAT is still being tuned during the beta. A capture of a day with SAT on helps us most: [How to share a capture]

- The link goes to the public "Reporting SAT field results" note in the manual, not to the internal field-run recipe (coordinator decision 2026-11-20: an internal document never sits behind a user link). Absolute, because the device does not serve `docs/` and a relative link would 404: EN `https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/manuals/en/ch05-sat-thermostat.md#overview`; the NL pack links the same note in `docs/manuals/nl/h05-sat-thermostaat.md` (anchor `#wat-doet-sat`). Opened with `target="_blank" rel="noopener"`. The link text is capitalised and no period follows it. Script names and the Discord or GitHub addresses stay in the docs, not in the UI.
- BlueFlame's reasons: "new in this beta" is no longer true (SAT is in every 2.0.0 beta); "a capture of a day with SAT on" names what the field-run recipe asks for; "experimental" is avoided on purpose (Robert, 2026-11-18: SAT drops that word at 2.0.0 stable).
- Keys: `tools.sat_beta_head` gets the sentence and a new `tools.sat_beta_link` gets the link label; `tools.sat_beta_body` is no longer shown. NL suggestion from BlueFlame for the i18n sync: "SAT wordt tijdens de beta nog bijgesteld. Een capture van een dag met SAT aan helpt ons het meest: [Zo deel je een capture]".
- Keep "Dismiss for this session".
- **Drop this item** if 2.0.0 stable ships before TASK-2959 is built: the banner disappears with the prerelease check.

## Acceptance (replaces the ACs of TASK-2959 where they differ)

1. **F4:** Home shows "Waiting for data from the gateway…" until the first OpenTherm data, "No boiler connected" with a working [Check connection ›] while `boilerOk` is false, and "everything quiet" only with the boiler link up. The header reads "checking…" until the first health answer. A browser test covers the four cases.
2. **F6:** a browser test answers the enable POST with HTTP 500 (the modal stays open, the error line names "HTTP 500", the button reads "Try again"), with a network error (the reason reads "no answer from the gateway") and with HTTP 200 (the modal closes and the toast shows). Pending disables both buttons.
3. **F10:** no settings hint under a label renders longer than 2 lines at 1400 px; Appliance type shows the short line, and its (i) holds the full text from `SET_META.satsource.help`; `settingsHelp.h` unchanged.
4. **F11:** on an HTTP page the "Detect location" button is hidden and the hint has no "HTTPS"; the link opens SAT settings › Weather with Latitude focused; otgw-docs has the same wording in §5.9 (EN and NL).
5. **F13:** the control-loop chart shows the overlay and label per the table in each state (the paused, stopped and waiting rows after TASK-2957); the heating curve card shows its one-line empty state at 120 px.
6. **F15:** the discovery card body is in the UI font and spans the width at 390 px; no auto-collapse.
7. **F5:** the banner shows the BlueFlame-approved line with the absolute link to the manual note "Reporting SAT field results" (EN ch05 `#overview`, NL h05 `#wat-doet-sat`) (`target="_blank" rel="noopener"`), or the item is closed as dropped because stable removed the banner.
8. New strings exist as EN keys; NL and DE are filled through the i18n sync, and the i18n tests (`tests/webui/i18n-*.test.mjs`) pass.
9. Keys that keep their name but change meaning have re-translated NL and DE values, visible in the sync diff: `tools.sat_geo_hint`, `tools.sat_beta_head` and `tools.sat_beta_body` (if F5 is built), and the `satsource` hint if it is translated.
