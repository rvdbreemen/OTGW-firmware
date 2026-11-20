# UX heuristic review: v2 web UI, Home, SAT and Settings (2026-11-20)

Reviewer: otgw-ux-designer. Task: TASK-NNNN. Assigned by the coordinator. Triage: the coordinator turns findings into otgw-webui tasks; nothing here is a decision.

## Method and scope

- Heuristics: Nielsen's ten usability heuristics (H1 to H10). Accessibility: WCAG 2.2, level AA unless marked otherwise.
- Severity 0 to 4 (Nielsen): 4 blocks the task, 3 major, 2 minor, 1 cosmetic. Effort: S (copy or one CSS rule), M (one component), L (several screens or a design decision).
- State: **U** = seen in every state. **NB** = seen only on a unit with no boiler and SAT paused. That was the only state available, so NB findings may not reach a normal user.
- Contrast: the WCAG relative-luminance formula on rendered pixels, cross-checked against the CSS token values.

### Evidence

Screenshots are local evidence, **not committed**. They are in `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\`.

- Source unit: .63, firmware 2.0.0-beta.402+b885f3d, Classic with an S3 module, PIC 6.8, no boiler on the bus. Captured 12:34 to 12:45, read-only, with the bench-tester's OK.
- Safety: GET only. A page guard blocked every non-GET fetch or XHR and every WebSocket send, and logged 0 attempts.
- Masking: IP, hostname, SSID, MAC, BSSID and chip ID were replaced in the DOM before each shot. A leak check after each shot found 0.
- Captured: Home, SAT Thermostat and Settings at 1400 px dark and light and at 390 px dark. SAT Control, SAT Technical and Monitor at 1400 px dark. Plus `home-1400-dark-simmodal.png`, the first visit with the simulation modal.
- **Not captured:** 390 px light, SAT Control and Technical in light, a unit with a boiler connected, and keyboard and screen-reader walkthroughs.

## Summary

| ID | Screen | Heuristic / SC | Sev | Effort | State | Owner | Evidence |
|---|---|---|---|---|---|---|---|
| F1 | SAT (all views) | WCAG 1.4.3 | 3 | S | U (beta builds) | webui | 1.77:1 measured |
| F2 | Global (links, light) | WCAG 1.4.3 | 3 | S | U | webui | 4.17:1 measured |
| F3 | SAT Thermostat | H1, H4 | 3 | M | NB | webui + dev | sat-simple-1400-*.png |
| F4 | Home | H1 | 2 | S | NB | webui | home-1400-dark.png |
| F5 | SAT banner | H8 | 2 | S | U (beta builds) | webui + BlueFlame | sat-simple-1400-*.png |
| F6 | Global modal | H1, H3, H9 | 2 | S | NB | webui | v2.js:8519-8535 |
| F7 | SAT Control | H1 | 2 | M | NB | webui + dev | sat-control-1400-dark.png |
| F8 | Mobile nav, rail, Home chips | H6 | 2 | S | U | webui | *-390-dark.png |
| F9 | Settings rail | H4, H8 | 2 | S | U | webui | settings-1400-dark.png |
| F10 | SAT Control, Appliance type | H8 | 2 | S | U | webui | sat-control-1400-dark.png |
| F11 | SAT weather card | H9, H2 | 2 | S | U (no lat/long set) | webui + docs | v2.js:1621 |
| F12 | SAT burner cycle card | H2 | 1 | S | U | webui | v2.js:1606-1608 |
| F13 | SAT control-loop chart | H1 | 1 | M | NB | webui | sat-simple-1400-dark.png |
| F14 | Global type scale | readability (not a WCAG fail) | 2 | L | U | webui + architect | v2.css, 66 sizes below .75rem |
| F15 | BLE toast | H4 | 1 | S | U | webui | settings-1400-dark.png |

Recommended order: F1 and F2 first, because they are measured AA failures and the fix is small. Then F5, F8, F9, F10, F11 and F12, which are S effort. F3, F6 and F7 need a short check with otgw-dev on what the fields mean. F14 is a design-system question for the architect, not a sprint item.

## Findings

### F1. The warning banner text fails contrast in the light theme (WCAG 1.4.3, sev 3)

- **What:** the SAT beta notice (`#satBetaNotice`, `.pic-banner.warn`) uses `color: var(--zone-warn)`. In light that is #ff9800, on its own 16 % tint of the same colour.
- **Measured:** about 1.77:1 in light, from rendered pixels (fg #fd9800 on bg #f3e7d0); AA requires 4.5:1. Dark measures 6.58:1 and passes.
- **Evidence:** `sat-simple-1400-light.png`, `v2.css:669-670`, `ds-tokens.css:100` (`--status-warn: #ff9800`).
- **Scope:** `v2.css` has 6 rules with `color:var(--zone-warn)` as text colour. Every warn text in light is likely affected, the PIC update banner among them; I measured only the SAT banner.
- **Proposal:** keep #ff9800 for borders, icons and fills, and add a darker warn text token for light. On #f3e7d0, #8a4b00 measures 5.56:1 and #7a4100 6.64:1. Dark stays as it is. This is a token change, so a design-system decision for webui and the ADR-091 gate owner.

### F2. Text links in light fail contrast (WCAG 1.4.3, sev 3)

- **What:** `--accent` in light is `--accent-primary` #0288a0.
- **Measured:** #0288a0 on #fff is 4.17:1, below 4.5:1. The links sit in small text (.7rem hints) in several places. Dark uses #00bffe on #2c2c2e and passes.
- **Evidence:** `ds-tokens.css:62`, `v2.css:99` (`.fs-banner a`), and the new About links (TASK-2948, `about2-1400-light.png`).
- **Proposal:** a light-only text-link colour: #027a8f (5.02:1) or the existing `--accent-hover` #015a70 (7.78:1). Buttons and fills keep #0288a0. Pair with F1 in one token task.

### F3. The SAT headline contradicts the status line (H1, H4, sev 3, NB)

- **What:** the big status reads "At target · idle" (`v2.js:1577`). Under it: "SAT paused: no OpenTherm connection, the boiler is back under normal control" (`v2.js:572`). The header shows an "Idle" chip, the toggle shows "Enabled", and a red "1" badge carries no label. That is four signals telling three stories. The room is 25.5 °C against a 19.8 °C target, so "At target" is also visibly false.
- **Evidence:** `sat-simple-1400-dark.png`, `sat-simple-1400-light.png`.
- **Proposal:** one status, with priority order *fault > paused > heating > at target > idle*. When paused, the headline becomes "Paused: no boiler connection" and the sub-line explains it. Give the red badge text or an accessible name ("1 problem: see Connection"). otgw-dev should confirm which states exist; I have not traced the SAT state machine, and that is outside my role.

### F4. Home says "everything quiet" while there is no boiler (H1, sev 2, NB)

- **What:** the Home status line reads "Standby · everything quiet". At the same time the header says "2 connection issues" and the gateway sees no boiler.
- **Evidence:** `home-1400-dark.png`.
- **Proposal:** when no boiler is detected, the Home status reads "No boiler connected" and links to Connection. "Everything quiet" only shows when the bus is healthy.

### F5. The SAT beta banner is a wall of bold warning text (H8, sev 2)

- **What:** a bold paragraph of three lines with two script paths, a Discord invite and a GitHub URL, in warning colour. "Dismiss for this session" brings it back in every new session.
- **Evidence:** `sat-simple-1400-*.png`, `v2.html:355-357`, `v2.js:6943-6950`. It only shows on prerelease versions (version check `v2.js:6940`), so it ends with 2.0.0 stable.
- **Proposal:** one plain-weight sentence plus a link: "SAT is new in this beta. Your field data helps: [how to share a capture]". The link goes to the manual section, which holds the script names (pair with otgw-docs). Copy by BlueFlame. Low priority if stable lands on 29 Nov.

### F6. Simulation modal: a failed enable is silent, and it covers every page (H1, H9, sev 2, NB)

- **What:** on a unit with no boiler, "No boiler detected, run in simulation mode?" opens over whichever page is open. "Not now" and a backdrop click both set `otgw-sim-prompted`, so it shows once per browser profile. "Enable simulation" sends a POST with an empty `.catch`, and the modal closes whether the POST worked or not. On an error the user gets no feedback.
- **Evidence:** `v2.js:8519-8535`, `home-1400-dark-simmodal.png`.
- **Proposal:** keep the modal open until the POST answers. On failure show "Could not enable simulation" plus a retry. On success show the existing toast. Optional: offer the modal on Home only, not over the Settings or Monitor work in progress.

### F7. Values shown without a paused or stale marker (H1, sev 2, NB)

- **What:** with SAT paused and the flame off, Control status shows Modulation 100 %, Max modulation 100 %, Error 0.00 °C and PID output 0.0 °C, all in the same style as live values. The control-loop chart draws a filled 100 % modulation band. Boiler setpoint shows "—".
- **Evidence:** `sat-control-1400-dark.png`.
- **I do not claim these values are wrong.** What they mean is SAT and firmware semantics; that needs otgw-dev, and Sergeant D for SAT behaviour. The UX point is only that frozen values look live.
- **Proposal:** while SAT is paused, grey out the control-status values and add "(paused)" to the card title, as the tiles already do with "updated 10 min ago".

### F8. Cut-off strips on mobile have no scroll hint (H6, sev 2)

- **What:** at 390 px the main nav ends in "Advan…", the Settings rail in "MQT…", and the Home chips (Flow, Return, DHW, Pressure) are cut off at the right edge. All of them scroll horizontally (`overflow-x:auto`, `v2.css:108`, `v2.css:535`), but nothing shows that more is there.
- **Evidence:** `home-390-dark.png`, `settings-390-dark.png`.
- **Proposal:** a right-edge fade mask on scrollable strips, plus scroll-into-view of the active item. That is CSS on the existing classes.

### F9. Settings rail at 1400 px: wrapped labels and a jumping icon (H4, H8, sev 2)

- **What:** the "Network" item puts its icon on its own line above the label, because the REBOOT badge takes the width. "OpenTherm Gateway" and "SAT — Smart Thermostat" wrap to two lines. The System panel to the right is about 370 px wide in a 1000 px column, leaving empty space.
- **Evidence:** `settings-1400-dark.png`.
- **Proposal:** keep icon and label on one line (`flex-wrap:nowrap`) and move the badge to a small dot or a second line. Shorten the rail labels to "OpenTherm" and "SAT". Let the panel fill the column.
- **Open question for webui:** the SAT group has 90 settings. Is it the same set as "SAT settings" at the bottom of SAT > Control (`v2.html:444-445`)? I have not verified whether it is one source rendered twice or two separate sets. If two, that is a consistency risk worth a task.

### F10. The Appliance-type help text is squeezed into the label column (H8, sev 2)

- **What:** under SAT settings > System, the Appliance-type hint is 13 lines in a column about 100 px wide, next to a single select.
- **Evidence:** `sat-control-1400-dark.png`.
- **Proposal:** move long help into the existing (i) popover (`.sinfo`) and keep at most one hint line under the label.

### F11. The weather card asks for HTTPS, which the project rules out (H9, H2, sev 2)

- **What:** "Location detection needs HTTPS. Enter Latitude and Longitude under SAT settings > Weather…" (`v2.js:1621`). The firmware deliberately has no HTTPS, so the first sentence points at a dead end.
- **Proposal:** "Set your location under SAT settings > Weather (latitude and longitude), or let Home Assistant publish sat/sun_elevation." Better still, link straight to the field. Pair with otgw-docs so the manual says the same.

### F12. The burner-cycle card contradicts itself (H2, sev 1)

- **What:** "Off — SAT is not actively controlling. SAT chooses the mode itself." (`v2.js:1607`). The second sentence is appended to every mode (`v2.js:1606-1608`), and next to "not controlling" it reads as a contradiction.
- **Proposal:** drop "SAT chooses the mode itself" from the Off text. Keep it for PWM and continuous.

### F13. The control-loop chart has no empty state (H1, sev 1, NB)

- **What:** with no data, five empty plot bands and a legend of eight series are shown, with no message.
- **Evidence:** `sat-simple-1400-dark.png`.
- **Proposal:** an overlay "No control data yet: SAT is paused (no boiler connection)" until the first point arrives.

### F14. Type scale: many sizes below 12 px (readability, sev 2, not a WCAG failure)

- **What:** `v2.css` has 66 font-size declarations below .75rem (12 px), from .55rem (8.8 px) up to .74rem. Examples: `.mc-cell .lbl` .64rem in mono uppercase (`v2.css:282`), `.sat-tile .tag` .56rem (`v2.css:825`), `.hdr-id small` .62rem (`v2.css:86`).
- **Not a WCAG failure:** WCAG sets no minimum font size. This is a readability heuristic. The cost is legibility on phones and for older users, the typical owners of a boiler.
- **Proposal:** a design-system decision rather than a fix. Define a type scale with at most 4 small steps and a 12 px floor for text a user must read; decorative tags can stay smaller. Owner: architect with webui, against the ADR-091 drift gate.

### F15. The BLE toast uses the mono font and covers content (H4, sev 1)

- **What:** the body of "4 new BLE sensors discovered" is in the mono font, unlike other toasts and hints. It overlaps the About card at 1400 px.
- **Evidence:** `settings-1400-dark.png`, `sat-control-1400-dark.png`.
- **Proposal:** body text in the UI font; anchor the toast bottom-right with a max width.

## What works well

- Clear top-level structure (Home, SAT, Monitor, Settings, Advanced) with sub-labels on desktop.
- Dark theme contrast is solid in the places measured: hints 5.79:1, links 5.83:1, warn banner 6.58:1.
- Settings search is prominent and has example queries.
- Tiles show data age ("updated 10 min ago"), a pattern F7 can reuse.
- Only two `outline:none` rules (`v2.css:349`, `v2.css:813`); everything else keeps a focus ring.

## Not verified

- No keyboard-only or screen-reader walkthrough. Focus visibility is checked only by reading the code.
- No unit with a boiler connected. Every NB finding needs a recheck on a live system.
- Light theme only at 1400 px, for three screens.
- `prefers-reduced-motion` is absent from `v2.css` and `ds-tokens.css` (35 transition/animation rules). That is WCAG 2.3.3, level AAA, so it is listed here and not as a finding.
