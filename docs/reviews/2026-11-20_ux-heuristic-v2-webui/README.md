# UX heuristic review: v2 web UI, Home, SAT, Settings and Monitor (2026-11-20)

Reviewer: otgw-ux-designer. Task: TASK-2952. Assigned by the coordinator. Triage: the coordinator turns findings into otgw-webui tasks; nothing here is a decision.

## Method and scope

- Heuristics: Nielsen's ten usability heuristics (H1 to H10). Accessibility: WCAG 2.2, level AA unless marked otherwise.
- Severity 0 to 4 (Nielsen): 4 blocks the task, 3 major, 2 minor, 1 cosmetic. Effort: S (copy or one CSS rule), M (one component), L (several screens or a design decision).
- State: **U** = seen in every state. **NB** = seen only on a unit with no boiler and SAT paused, or in the mock with empty data. NB findings may not reach a normal user and need a recheck on a live system.
- Contrast: the WCAG relative-luminance formula. Two independent sources per number: computed styles in the browser (`mock-contrast.mjs`, composites translucent layers up to the first opaque background) and a Python recomputation from the hex values. On .63, rendered pixels cross-checked against the CSS tokens.
- Every finding ends with a **Proposed AC** that otgw-webui can test.

### Evidence

Screenshots and logs are local evidence, **not committed**. They are in `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\`, scripts in its `tools\` folder.

**Live unit (.63).** Firmware 2.0.0-beta.402+b885f3d, Classic with an S3 module, PIC 6.8, no boiler on the bus. Captured 12:34 to 12:45, read-only, with the bench-tester's OK.
- Safety: GET only. A page guard blocked every non-GET fetch or XHR and every WebSocket send, and logged 0 attempts.
- Masking: IP, hostname, SSID, MAC, BSSID and chip ID were replaced in the DOM before each shot. A leak check after each shot found 0.
- Captured: Home, SAT Thermostat and Settings at 1400 px dark and light and at 390 px dark. SAT Control, SAT Technical and Monitor at 1400 px dark. Plus `home-1400-dark-simmodal.png`, the first visit with the simulation modal.

**Mock (synthetic).** The real `v2.html`/`v2.js`/`v2.css` from origin/dev 431d6b50a, served by `tests/webui/lib/page-harness.mjs`; every `/api/` call answers `{}`, so no device data is shown. 0 JavaScript exceptions in every run.
- `mock-gaps.mjs`: Home, SAT Thermostat, SAT Control and Settings at 390 px light (`mock-*-390-light.png`). SAT Control, SAT Technical and Monitor at 1400 px light (`mock-*-1400-light.png`).
- `mock-contrast.mjs`: every visible text style on Home, SAT (three views) and Settings, light and dark (`mock-contrast-light.txt`, `mock-contrast-dark.txt`; 27 to 149 text styles per view).
- `mock-gaps.mjs` kbd mode and `mock-kbd-sat.mjs`: Tab walks on Home and SAT Control (`mock-kbd-home-1400-light.txt`, `mock-kbd-sat-control-1400-light.txt`).
- `mock-a11y-sat.mjs`: names, roles and states of the SAT Control controls (`mock-a11y-sat-control.txt`).
- `mock-focus.mjs`: the focus ring on the header nav, light and dark (`mock-focus-nav-light.png`, `mock-focus-nav-dark.png`).
- `mock-disabled.mjs`: disabled state of the SAT Control buttons with SAT off (`mock-disabled-sat-control.txt`).

## Summary

| ID | Screen | Heuristic / SC | Sev | Effort | State | Owner | Evidence |
|---|---|---|---|---|---|---|---|
| F1 | SAT (all views) | WCAG 1.4.3 | 3 | S | U (beta builds) | webui | 1.77:1 measured |
| F2 | Global accent (light) | WCAG 1.4.3 | 3 | M | U | webui (+ ADR-091 gate) | 2.81 / 3.86 / 4.17:1 measured |
| F16 | Theme toggle (dark) | WCAG 1.4.3 | 3 | S | U | webui | 2.12:1 measured |
| F3 | SAT Thermostat | H1, H4 | 3 | M | NB | webui + dev | sat-simple-1400-*.png |
| F19 | Every page | WCAG 2.4.1 (A), 1.3.1 | 2 | S | U | webui | mock-kbd-*.txt |
| F18 | Main nav, SAT views, SAT switch | WCAG 4.1.2 | 2 | S | U | webui | mock-a11y-sat-control.txt |
| F20 | SAT Control (SAT off) | H1, H5 | 2 | S | U (SAT off) | webui | mock-disabled-sat-control.txt |
| F17 | SAT status pill, source tag | WCAG 1.4.3 | 2 | S | U | webui | 2.68 / 2.13:1 measured |
| F4 | Home | H1 | 2 | S | NB | webui | home-1400-dark.png |
| F5 | SAT banner | H8 | 2 | S | U (beta builds) | webui + BlueFlame | sat-simple-1400-*.png |
| F6 | Global modal | H1, H3, H9 | 2 | S | NB | webui | v2.js:8519-8535 |
| F7 | SAT Control | H1 | 2 | M | NB | webui + dev | sat-control-1400-dark.png |
| F8 | Mobile nav, rail, Home chips | H6 | 2 | S | U | webui | *-390-*.png |
| F9 | Settings rail | H4, H8 | 2 | S | U | webui | settings-1400-dark.png |
| F10 | SAT Control, Appliance type | H8 | 2 | S | U | webui | sat-control-1400-dark.png |
| F11 | SAT weather card | H9, H2 | 2 | S | U (no lat/long set) | webui + docs | v2.js:1621 |
| F14 | Global type scale | readability (not a WCAG fail) | 2 | L | U | webui + architect | v2.css, 66 sizes below .75rem |
| F12 | SAT burner cycle card | H2 | 1 | S | U | webui | v2.js:1606-1608 |
| F13 | SAT charts | H1 | 1 | M | NB | webui | sat-*-1400-*.png |
| F15 | BLE toast | H4 | 1 | S | U | webui | settings-1400-dark.png |
| F21 | Focus ring | H4 | 1 | S | U | webui | mock-focus-nav-*.png, v2.css:54,66,449,496 |

Recommended order:
1. The measured AA failures with small fixes: F1, F2, F16, F17. F1, F2 and F16 are one token task, because they all come from how the accent and warn tokens are used as text and as fills.
2. The cheap structural accessibility fixes: F19, F18, F20.
3. The S-effort copy and layout items: F5, F8, F9, F10, F11, F12.
4. F3, F6 and F7 need a short check with otgw-dev on what the fields mean.
5. F14 is a design-system question for the architect, not a sprint item.

## Findings

### F1. The warning banner text fails contrast in the light theme (WCAG 1.4.3, sev 3)

- **What:** the SAT beta notice (`#satBetaNotice`, `.pic-banner.warn`) uses `color: var(--zone-warn)`. In light that is #ff9800, on its own 16 % tint of the same colour.
- **Measured:** about 1.77:1 in light, from rendered pixels (fg #fd9800 on bg #f3e7d0); AA requires 4.5:1. Dark measures 6.58:1 and passes.
- **Evidence:** `sat-simple-1400-light.png`, `v2.css:669-670`, `ds-tokens.css:100` (`--status-warn: #ff9800`).
- **Scope:** `v2.css` has 6 rules with `color:var(--zone-warn)` as text colour. Every warn text in light is likely affected, the PIC update banner among them; I measured only the SAT banner.
- **Proposal:** keep #ff9800 for borders, icons and fills, and add a darker warn text token for light. On #f3e7d0, #8a4b00 measures 5.56:1 and #7a4100 6.64:1. Dark stays as it is.
- **Proposed AC:** every text that uses the warn colour measures >= 4.5:1 against its rendered background in light and in dark (all 6 `color:var(--zone-warn)` rules), measured with `mock-contrast.mjs` or a pixel check; the ADR-091 drift gate stays green.

### F2. The accent colour fails as text in three light contexts (WCAG 1.4.3, sev 3)

- **What:** light uses `--accent-primary` #0288a0 (`ds-tokens.css:62`) for three different jobs, and it fails in each:

  | Context | Measured | Where |
  |---|---|---|
  | (a) Text on a light surface: links, the active SAT view tab and its sub-label | 4.17:1 on #fff, 3.86:1 on #f1f7f8 | "Monitor › Graph", "Raw data (JSON)", About links (TASK-2948), active "Control / OPERATIONAL" tab (9.9 px sub-label) |
  | (b) Text on the dark header: the active main-nav item | 2.81:1 on #383838 | "SAT", "Home", "Settings" when active, `mock-focus-nav-light.png` |
  | (c) White text on an accent fill: the active Light theme button | 4.17:1 (#fff on #0288a0) | theme toggle in the header |

- **Why one token cannot fix it:** the header stays #383838 in both themes. Text on #383838 needs a light colour (luminance about 0.35 or more for 4.5:1); text on #f1f7f8 needs a dark one (about 0.17 or less). No single colour passes both. A darker accent fixes (a) and (c) but makes (b) worse.
- **Proposal, per context:**
  - (a) A light-only text-link token: #027a8f (5.02:1 on #fff, 4.64:1 on #f1f7f8, narrow) or the existing `--accent-hover` #015a70 (7.19:1 on #f1f7f8).
  - (b) The header uses the dark-theme accent #00bffe for its active item in both themes: 5.52:1 on #383838. The header is dark in both themes, so it can take the dark token.
  - (c) The fill behind white text moves to #027a8f (5.02:1) or #015a70 (7.78:1).
  - Decorative fills and borders without text may keep #0288a0.
- **Evidence:** `mock-contrast-light.txt` (all three contexts), `ds-tokens.css:62`, `v2.css:99` (`.fs-banner a`), `about2-1400-light.png`, `sat-simple-1400-light.png`. Python recomputation matches the browser numbers.
- **Proposed AC** (the coordinator's AC, widened from one token to three contexts):
  1. Text on a light surface in the accent colour >= 4.5:1 on #fff and on #f1f7f8.
  2. The active nav item on the dark header >= 4.5:1, in light and in dark.
  3. Text on an accent fill >= 4.5:1, in light and in dark.
  4. All three measured with `mock-contrast.mjs` in light and dark, with 0 accent FAIL lines, and the ADR-091 drift gate green.

### F16. The active Dark theme button fails contrast (WCAG 1.4.3, sev 3)

- **What:** in dark, the active theme button shows white text on the dark accent: #fff on #00bffe = 2.12:1. The dark accent is fine as text on dark surfaces (5.52:1 on #383838) but too light as a fill behind white text.
- **Evidence:** `mock-contrast-dark.txt` (the only FAIL in dark, on every page), theme toggle in the header.
- **Proposal:** dark text on the fill: #1c1c1e on #00bffe = 8.01:1. This is context (c) of F2 in the dark theme; fix both in one token task.
- **Proposed AC:** the active theme button measures >= 4.5:1 in both themes; `mock-contrast.mjs` dark reports 0 FAIL lines.

### F3. The SAT headline contradicts the status line (H1, H4, sev 3, NB)

- **What:** the big status reads "At target · idle" (`v2.js:1577`). Under it: "SAT paused: no OpenTherm connection, the boiler is back under normal control" (`v2.js:572`). The header shows an "Idle" chip, the toggle shows "Enabled", and a red "1" badge carries no label. That is four signals telling three stories. The room is 25.5 °C against a 19.8 °C target, so "At target" is also visibly false.
- **Evidence:** `sat-simple-1400-dark.png`, `sat-simple-1400-light.png`.
- **Proposal:** one status, with priority order *fault > paused > heating > at target > idle*. When paused, the headline becomes "Paused: no boiler connection" and the sub-line explains it. Give the red badge text or an accessible name ("1 problem: see Connection"). otgw-dev should confirm which states exist; I have not traced the SAT state machine, and that is outside my role.
- **Proposed AC:** with SAT enabled and no OpenTherm connection, the headline, header chip and sub-line all say paused (no "At target" or "idle"); the red badge has an accessible name; a browser test drives the paused state and checks the three texts.

### F4. Home says "everything quiet" while there is no boiler (H1, sev 2, NB)

- **What:** the Home status line reads "Standby · everything quiet". At the same time the header says "2 connection issues" and the gateway sees no boiler.
- **Evidence:** `home-1400-dark.png`. The mock with empty data also shows "All systems connected" and "everything quiet" (`mock-home-390-light.png`): the UI defaults to "all good" when it has no data. The mock is not a real device state, so this only supports the point.
- **Proposal:** when no boiler is detected, the Home status reads "No boiler connected" and links to Connection. "Everything quiet" only shows when the bus is healthy.
- **Proposed AC:** with no boiler detected, Home shows "No boiler connected" with a link to Connection, and never "everything quiet"; with no status data yet, Home shows a neutral "Waiting for data" instead of a healthy state.

### F5. The SAT beta banner is a wall of bold warning text (H8, sev 2)

- **What:** a bold paragraph of three lines with two script paths, a Discord invite and a GitHub URL, in warning colour. "Dismiss for this session" brings it back in every new session.
- **Evidence:** `sat-simple-1400-*.png`, `v2.html:355-357`, `v2.js:6943-6950`. It only shows on prerelease versions (version check `v2.js:6940`), so it ends with 2.0.0 stable.
- **Proposal:** one plain-weight sentence plus a link: "SAT is new in this beta. Your field data helps: [how to share a capture]". The link goes to the manual section, which holds the script names (pair with otgw-docs). Copy by BlueFlame. Low priority if stable lands on 29 Nov.
- **Proposed AC:** the banner is one sentence in normal weight with one link to the manual; no script paths in the UI; text contrast per F1.

### F6. Simulation modal: a failed enable is silent, and it covers every page (H1, H9, sev 2, NB)

- **What:** on a unit with no boiler, "No boiler detected, run in simulation mode?" opens over whichever page is open. "Not now" and a backdrop click both set `otgw-sim-prompted`, so it shows once per browser profile. "Enable simulation" sends a POST with an empty `.catch`, and the modal closes whether the POST worked or not. On an error the user gets no feedback.
- **Evidence:** `v2.js:8519-8535`, `home-1400-dark-simmodal.png`.
- **Proposal:** keep the modal open until the POST answers. On failure show "Could not enable simulation" plus a retry. On success show the existing toast. Optional: offer the modal on Home only, not over the Settings or Monitor work in progress.
- **Proposed AC:** a browser test answers the enable POST with HTTP 500: the modal stays open and shows an error with a retry; with HTTP 200 it closes and shows the toast.

### F7. Values shown without a paused or stale marker (H1, sev 2, NB)

- **What:** with SAT paused and the flame off, Control status shows Modulation 100 %, Max modulation 100 %, Error 0.00 °C and PID output 0.0 °C, all in the same style as live values. The control-loop chart draws a filled 100 % modulation band. Boiler setpoint shows "—".
- **Evidence:** `sat-control-1400-dark.png`.
- **I do not claim these values are wrong.** What they mean is SAT and firmware semantics; that needs otgw-dev, and Sergeant D for SAT behaviour. The UX point is only that frozen values look live.
- **Proposal:** while SAT is paused, grey out the control-status values and add "(paused)" to the card title, as the tiles already do with "updated 10 min ago".
- **Proposed AC:** while SAT is paused, the Control status card title carries "(paused)" and its values use the muted style; a browser test checks both in the paused state.

### F8. Cut-off strips on mobile have no scroll hint (H6, sev 2)

- **What:** at 390 px the main nav ends in "Advan…", the Settings rail in "MQT…", and the Home chips (Flow, Return, DHW, Pressure) are cut off at the right edge. All of them scroll horizontally (`overflow-x:auto`, `v2.css:108`, `v2.css:535`), but nothing shows that more is there. Same in light (`mock-home-390-light.png`, `mock-settings-390-light.png`).
- **Evidence:** `home-390-dark.png`, `settings-390-dark.png`, `mock-home-390-light.png`.
- **Proposal:** a right-edge fade mask on scrollable strips, plus scroll-into-view of the active item. That is CSS on the existing classes.
- **Proposed AC:** at 390 px each horizontally scrollable strip (main nav, Settings rail, Home chips) shows an edge fade while more content is off-screen, and the active item is scrolled into view; screenshots at 390 px in both themes.

### F9. Settings rail at 1400 px: wrapped labels and a jumping icon (H4, H8, sev 2)

- **What:** the "Network" item puts its icon on its own line above the label, because the REBOOT badge takes the width. "OpenTherm Gateway" and "SAT — Smart Thermostat" wrap to two lines. The System panel to the right is about 370 px wide in a 1000 px column, leaving empty space.
- **Evidence:** `settings-1400-dark.png`.
- **Proposal:** keep icon and label on one line (`flex-wrap:nowrap`) and move the badge to a small dot or a second line. Shorten the rail labels to "OpenTherm" and "SAT". Let the panel fill the column.
- **Open question for webui:** the SAT group has 90 settings. Is it the same set as "SAT settings" at the bottom of SAT > Control (`v2.html:444-445`)? I have not verified whether it is one source rendered twice or two separate sets. If two, that is a consistency risk worth a task.
- **Proposed AC:** at 1400 px every rail item keeps icon and label on one line, with or without a badge; webui answers the open question in the task notes.

### F10. The Appliance-type help text is squeezed into the label column (H8, sev 2)

- **What:** under SAT settings > System, the Appliance-type hint is 13 lines in a column about 100 px wide, next to a single select.
- **Evidence:** `sat-control-1400-dark.png`.
- **Proposal:** move long help into the existing (i) popover (`.sinfo`) and keep at most one hint line under the label.
- **Proposed AC:** no settings hint is longer than one line under its label at 1400 px; longer help sits in the (i) popover.

### F11. The weather card asks for HTTPS, which the project rules out (H9, H2, sev 2)

- **What:** "Location detection needs HTTPS. Enter Latitude and Longitude under SAT settings > Weather…" (`v2.js:1621`). The firmware deliberately has no HTTPS, so the first sentence points at a dead end.
- **Proposal:** "Set your location under SAT settings > Weather (latitude and longitude), or let Home Assistant publish sat/sun_elevation." Better still, link straight to the field. Pair with otgw-docs so the manual says the same.
- **Proposed AC:** the weather card text no longer mentions HTTPS and links to the latitude and longitude fields; the manual uses the same wording.

### F12. The burner-cycle card contradicts itself (H2, sev 1)

- **What:** "Off — SAT is not actively controlling. SAT chooses the mode itself." (`v2.js:1607`). The second sentence is appended to every mode (`v2.js:1606-1608`), and next to "not controlling" it reads as a contradiction. Seen again in the mock (`mock-sat-control-1400-light.png`).
- **Proposal:** drop "SAT chooses the mode itself" from the Off text. Keep it for PWM and continuous.
- **Proposed AC:** the Off state shows only "SAT is not actively controlling."; PWM and continuous keep the second sentence.

### F13. The SAT charts have no empty or paused state (H1, sev 1, NB)

- **What:** with no data, the control-loop chart shows five empty plot bands and a legend of eight series with no message, and still carries the "live" label while SAT is off. The Heating curve card is an empty white area of about 400 px with only "No calibration markers yet." in the corner.
- **Evidence:** `sat-simple-1400-dark.png`, `mock-sat-control-1400-light.png`.
- **Proposal:** an overlay "No control data yet: SAT is paused (no boiler connection)" until the first point arrives, and the same pattern for the heating curve. Show "live" only while points arrive.
- **Proposed AC:** with no data points, both charts show a one-line empty-state message and no "live" label; the label returns with the first point.

### F14. Type scale: many sizes below 12 px (readability, sev 2, not a WCAG failure)

- **What:** `v2.css` has 66 font-size declarations below .75rem (12 px), from .55rem (8.8 px) up to .74rem. Examples: `.mc-cell .lbl` .64rem in mono uppercase (`v2.css:282`), `.sat-tile .tag` .56rem (`v2.css:825`), `.hdr-id small` .62rem (`v2.css:86`). The source tag "SAT → BOILER" renders at 9 px (`mock-contrast-light.txt`).
- **Not a WCAG failure:** WCAG sets no minimum font size. This is a readability heuristic. The cost is legibility on phones and for older users, the typical owners of a boiler.
- **Proposal:** a design-system decision rather than a fix. Define a type scale with at most 4 small steps and a 12 px floor for text a user must read; decorative tags can stay smaller. Owner: architect with webui, against the ADR-091 drift gate.
- **Proposed AC:** (for the architect's decision task) a written type scale in the design system with its floor; a later webui task brings `v2.css` onto it.

### F15. The BLE toast uses the mono font and covers content (H4, sev 1)

- **What:** the body of "4 new BLE sensors discovered" is in the mono font, unlike other toasts and hints. It overlaps the About card at 1400 px.
- **Evidence:** `settings-1400-dark.png`, `sat-control-1400-dark.png`.
- **Proposal:** body text in the UI font; anchor the toast bottom-right with a max width.
- **Proposed AC:** the BLE toast body uses the UI font and does not cover card content at 1400 px or 390 px.

### F17. The SAT status pill and the source tag fail contrast (WCAG 1.4.3, sev 2)

- **What:** two small labels in light:
  - The "Off" pill next to the SAT title (`span.sat-pill`): #fff on #9e9e9e = 2.68:1, 11.2 px bold.
  - The "SAT → BOILER" tag on the Boiler setpoint tile (`span.tag`): #ff5a3c on #d6d6d6 = 2.13:1, 9 px bold.
- **Evidence:** `mock-contrast-light.txt` (SAT Control and Technical), `mock-sat-control-1400-light.png`.
- **Caveat:** the tag background was computed by compositing translucent layers. The screenshot shows a pinkish tint, so the real background may differ from #d6d6d6. A pixel check is needed before fixing the tag.
- **Proposal:** the Off pill on #616161 (6.19:1) or #757575 (4.61:1). The tag uses a dark text on a light tint, for example #a12a12 on #ffe5df (6.14:1).
- **Proposed AC:** both labels measure >= 4.5:1 in light and dark against their rendered background, checked by pixel sampling on a screenshot.

### F18. Active states and names are not exposed to assistive technology (WCAG 4.1.2, sev 2)

- **What:**
  - The main nav buttons (`.seg`) carry no `aria-current`. The active page shows only by colour and underline.
  - The SAT view tabs (Thermostat, Control, Technical) have no `role=tab` and no `aria-selected`, so a screen reader does not hear which view is open.
  - The theme buttons do use `aria-pressed`, so the UI is inconsistent: the pattern exists but is not applied to navigation.
  - The SAT switch (`input[type=checkbox]`) takes its accessible name from the state text next to it. Unchecked, it is announced as "Disabled, checkbox, not checked", a double negative. Its name should be "SAT"; the checked state carries on or off.
- **Evidence:** `mock-a11y-sat-control.txt`, `mock-kbd-sat-control-1400-light.txt`.
- **Proposal:** `aria-current="page"` on the active `.seg`; `role=tablist`/`tab`/`tabpanel` with `aria-selected` on the SAT views; `aria-label="SAT"` on the switch, with the visible "Enabled"/"Disabled" text kept as a hint.
- **Proposed AC:** the active nav button has `aria-current="page"`; the active SAT view tab has `aria-selected="true"` and the others `false`; the SAT switch's accessible name is "SAT" in both states; a browser test reads the three attributes after a page and view change.

### F19. No way to skip the header: no main landmark, no h1, no skip link (WCAG 2.4.1 level A, 1.3.1, sev 2)

- **What:** every page has one `<nav>`, but no `<main>` (or `role=main`), no `<h1>` and no skip link. A keyboard user tabs through 10 header stops (connection, language, two theme buttons, five nav items, view picker) on every page before reaching content; a screen-reader user has no landmark or heading to jump to.
- **Evidence:** `mock-kbd-home-1400-light.txt` (`landmarks: {"main":0,"nav":1,"h1":0}`, skip link: none), `mock-kbd-sat-control-1400-light.txt` (same header stops before the SAT controls).
- **Note on the Tab walk:** on Home the walk loops after the header. That is not a defect: with empty mock data Home has no interactive content beyond the view picker.
- **Proposal:** a "Skip to content" link as the first focusable element, visible on focus; the page container becomes `<main>`; one `<h1>` per page (it may be visually hidden where the nav already shows the page name).
- **Proposed AC:** the first Tab on any page focuses a visible "Skip to content" link that moves focus into `<main>`; each page has exactly one `<main>` and one `<h1>`; checked with the Tab-walk script.

### F20. With SAT off, disabled controls look enabled (H1, H5, sev 2)

- **What:** with SAT off, the target buttons (lower, raise), the six presets and "Detect location" are `disabled`, and Tab skips them. They still render at full opacity with `cursor: pointer`, so they look clickable and do nothing. Only `.fs-btn` has a disabled style in `v2.css` (`v2.css:751`).
- **Evidence:** `mock-disabled-sat-control.txt` (9 buttons: `disabled=true`, `opacity=1`, `cursor=pointer`), `mock-sat-control-1400-light.png`, `mock-kbd-sat-control-1400-light.txt` (Tab goes from the view tabs straight to "Monitor › Graph").
- **Proposal:** a shared disabled style for buttons (the `.fs-btn:disabled` rule generalised: reduced opacity, `cursor: not-allowed`) plus a one-line hint near the dial: "SAT is off: switch it on (top right) to change the target." The SAT settings block already has such a line.
- **Proposed AC:** every disabled button in the v2 UI renders with the shared disabled style and `cursor: not-allowed`; with SAT off, the dial card shows the hint; screenshot in both themes.

### F21. Two focus-ring styles (H4, sev 1)

- **What:** most controls use the browser's default focus ring (`outline-style: auto`), which Chrome draws as a dark-and-white double ring. It is clearly visible in both themes, so WCAG 2.4.7 passes. Four places have a designed 2 px accent ring instead: the language menu (`v2.css:54`, `v2.css:66`), group summaries (`v2.css:449`) and the info button (`v2.css:496`).
- **Evidence:** `mock-focus-nav-light.png`, `mock-focus-nav-dark.png`, `mock-kbd-sat-control-1400-light.txt` (`outline=auto` versus `outline=solid`).
- **Proposal:** one focus style for every control, defined once in the design system. A 2 px ring in a colour that passes 3:1 against both the header and the page (see F2 (b)) is the natural choice.
- **Proposed AC:** a single `:focus-visible` rule covers buttons, links, summaries and inputs; the Tab walk shows the same ring style on every stop.

## What works well

- Clear top-level structure (Home, SAT, Monitor, Settings, Advanced) with sub-labels on desktop.
- Dark theme text contrast is solid on surfaces: hints 5.79:1, links 5.83:1, warn banner 6.58:1, and the mock scan found no dark text failure except the active theme button (F16).
- Settings search is prominent and has example queries.
- Tiles show data age ("updated 10 min ago"), a pattern F7 can reuse.
- Focus stays visible everywhere (browser default or designed ring); only two `outline:none` rules (`v2.css:349`, `v2.css:813`).
- Good accessible names where they exist: "lower target" and "raise target" on the dial buttons, labelled range inputs (DHW setpoint, Max flow setpoint, Max relative modulation), `aria-pressed` on the theme buttons, language links with `hreflang`.

## Not verified

- **Screen reader:** no NVDA or VoiceOver walkthrough. F18 and F19 come from the DOM (attributes and landmarks), not from listening.
- **Live data:** no unit with a boiler connected. Every NB finding needs a recheck on a live system. The mock answers `{}`, so states with data (an active preset, a running PID, a filled chart) were not seen in light.
- **Preset state:** whether the active preset carries `aria-pressed` could not be checked: the mock has no active preset.
- **Contrast scan limits:** one element per text and colour pair; rest state only (no hover, pressed or focus colours); text over gradients or images is not measured; the source tag in F17 needs a pixel check.
- **Light at 390 px from the live unit:** 390 px light exists only from the mock (`mock-*-390-light.png`), not from .63.
- **Monitor in light:** captured from the mock only (`mock-monitor-1400-light.png`), with no data.
- **Advanced page:** not reviewed.
- `prefers-reduced-motion` is absent from `v2.css` and `ds-tokens.css` (35 transition/animation rules). That is WCAG 2.3.3, level AAA, so it is listed here and not as a finding.
