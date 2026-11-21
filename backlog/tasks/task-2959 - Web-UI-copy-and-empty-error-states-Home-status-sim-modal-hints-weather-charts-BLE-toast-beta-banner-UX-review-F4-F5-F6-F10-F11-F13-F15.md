---
id: TASK-2959
title: >-
  Web UI copy and empty/error states: Home status, sim modal, hints, weather,
  charts, BLE toast, beta banner (UX review F4 F5 F6 F10 F11 F13 F15)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:26:00Z'
updated_date: '2026-11-21T14:28:00Z'
labels:
  - ux
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1093000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F4, F5, F6, F10, F11, F13, F15). F4: Home says 'everything quiet' with no boiler (and 'all good' with no data). F6: sim modal closes on a failed POST with no feedback (v2.js:8519-8535). F10: Appliance-type hint 13 lines in a narrow column. F11: weather card says location detection needs HTTPS (v2.js:1621), which the firmware does not have. F13: charts without an empty state; 'live' label while SAT is off. F15: BLE toast body in mono font, covers cards. F5: SAT beta banner is a bold wall of text with script paths (v2.html:355-357); copy by BlueFlame; the banner disappears at 2.0.0 stable, so drop F5 if stable lands first. Weather wording paired with otgw-docs. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: home-1400-dark.png, home-1400-dark-simmodal.png, mock-home-390-light.png, sat-control-1400-dark.png, mock-sat-control-1400-light.png, settings-1400-dark.png, sat-simple-1400-*.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 F4: Home shows 'Waiting for data from the gateway…' until the first successful /api/v2/otgw/otmonitor answer, 'No boiler connected' with a working 'Check connection ›' while boilerOk is false, and 'everything quiet' only with the boiler link up; the header reads 'checking…' until the first health answer; a browser test covers the four cases
- [x] #2 F6: a browser test answers the sim enable POST with HTTP 500 (modal stays open, error line names 'HTTP 500', button 'Try again'), with a network error (reason 'no answer from the gateway') and with HTTP 200 (modal closes, toast shows); pending disables both buttons
- [x] #3 F10: no settings hint renders longer than 2 lines at 1400 px; Appliance type shows the short line and its (i) holds the full text from SET_META.satsource.help; settingsHelp.h unchanged
- [ ] #4 F11: on an HTTP page 'Detect location' is hidden and the hint has no 'HTTPS'; the link opens SAT settings › Weather with Latitude focused (the group heading while SAT is off); otgw-docs uses the same wording in manual §5.9 EN and NL
- [x] #5 New EN keys exist; NL and DE filled by the i18n sync, tests/webui/i18n-*.test.mjs pass; keys that change meaning (sat_geo_hint, sat_beta_head/body if built, the satsource hint if translated) get re-translated NL and DE values, visible in the sync diff
- [x] #6 F15: the discovery card body is in the UI font and spans the width at 390 px; no auto-collapse (coordinator decision 2026-11-20, KISS)
- [x] #7 F13: the control-loop chart shows the overlay and label per spec T4 in each state (paused, stopped and waiting rows after TASK-2957); the heating curve card shows its one-line empty state at 120 px
- [ ] #8 F5: the SAT beta banner shows BlueFlame's approved line 'SAT is still being tuned during the beta. A capture of a day with SAT on helps us most: [How to share a capture]' with an absolute link to the public manual note 'Reporting SAT field results' (https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/manuals/en/ch05-sat-thermostat.md#overview; NL pack: docs/manuals/nl/h05-sat-thermostaat.md#wat-doet-sat), target=_blank rel=noopener; never the internal field-run recipe; or the item is closed as dropped because stable removed the banner
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T4-copy-and-states.md (exact copy, states and edge cases per finding). ACs replaced by the spec's acceptance list. Dependency: the F13 paused/stopped/starting rows need TASK-2957's display states; the other rows can land first. F15 collapse chip is an open option for the coordinator. F5 copy drafted, sent to BlueFlame.

2026-11-20: coordinator decided F15 = base only (no auto-collapse). BlueFlame approved the F5 copy (absolute GitHub link, since the device does not serve docs/). Spec T4 updated.

2026-11-20: F5 link moved from the internal field-run recipe to the public manual note (coordinator decision); BlueFlame informed.

2026-11-21 otgw-webui (lane/ba-2959, stacked on TASK-2957 lane/ba-2957; push after 2957, after the 20:00 compose). Evidence OTGW-build-artifacts-safe/evidence-TASK-2959/ (red/green per item). F4: Home 'Waiting for data from the gateway…' until the first otmonitor answer, 'No boiler connected' + 'Check connection ›' (opens Monitor › Connection) while CONN.boiler is st-down, header 'checking…' until the first /v2/health answer; test home-status-states RED 1/2 fail -> GREEN 3/3. F6: sim modal pending (both buttons disabled, Enabling…), error line role=alert with 'HTTP 500' / 'no answer from the gateway' and 'Try again', 200 closes + toast; sim-modal-states RED 0/3 -> GREEN 3/3. F10: Manufacturer (5 lines) and Boiler rated input (3) get a short hint, the full text in the (i) via SET_META.help, translated through settings.help.<k> (existing NL/DE/EL text moved); Appliance type: option B (coordinator 2026-11-21) per entry 204, sergeant-d-corrections.md:5470 'the experimental type always shows: ...': the warning stays inline while the experimental type is chosen, otherwise 'The (i) explains each type.'; setting-hints-two-lines measures all 70 hints at 1400 px: RED (satsource 11, satmanufacturer 5, satboilerratedkw 3) -> GREEN (max 2); v2-settings-sat-followups updated (short hint for another type, warning inline after choosing the experimental type). F11: hint without HTTPS, 'SAT settings › Weather' is a link to the SAT page Control view with Latitude focused (Detect location was already hidden on HTTP, TASK-1257); sat-weather-hint-link RED -> GREEN; sat-detect-location stubs extended and sat-sun-source-hint copy updated (7/0, 4/0). Manual §5.9 copy goes to otgw-docs after the push. F13: control-loop note per state (off, paused/stopped, waiting, collecting; corner when points exist) and heating-curve empty line (card 365 -> under 160 px); sat-chart-empty-states RED 0/2 -> GREEN 2/2. F15: discovery hint in the UI font, identifiers as <code>, full width minus 16 px at 390 px; discover-card-font-width RED (mono, left 34) -> GREEN. AC5: NL/DE/EL for the new and changed keys (sat_geo_hint re-translated), i18n-lang 6/0, lang-menu 7/0, i18n-no-hardcoded 3/0, i18n-followups 4/0; written by hand, no review workflow run. F5 (beta banner) waits (coordinator). Open: AC4 docs part (otgw-docs), AC8 F5, full sweep + build + bench.

2026-11-21 fixes after the otgw-ux-designer critique (coordinator decision): P1 Home 'Waiting for data…' could stay forever when the first otmonitor seed failed while the socket was open (the 20 s fallback only re-seeds a closed socket): now a first OpenTherm frame on the socket also counts as data, and the seed is retried every 10 s until the first answer; the waiting line also holds until the first /v2/health answer (spec row 1; before, st-unknown fell through to 'everything quiet'). home-status-states: 2 new cases red on 9d3056538 (stuck waiting after 1 request with an open fake socket; 'everything quiet' without a health answer), file 5/0 now. P2 sim modal per corrected spec T4 (ux 69d87480c): error 'Could not enable simulation ({{reason}}).' + link 'Open the simulation setting' (SAT page, Technical view, satsimulation), a 409 with a server reason shows that reason and the button reads Close (no Try again); role=dialog, aria-modal, initial focus on the primary button, focus kept in the dialog while pending and back on the button after a failure, Escape closes when not pending. sim-modal-states: 3 cases red on 9d3056538, file 5/0. P2: aria-live=polite on #satStateSub; Appliance type (i) now starts with the settingsHelp.h:75 all-types sentence, then the experimental paragraph (spec T4 corrected). satOpenSetting(key, view) generalises the weather link. home-heatpump-wording fixture gets a /v2/health answer (Home waits for it now). NL pill/card 'Wacht' -> 'Wachten' (ux P3). NAS sweep on 9d3056538: 91/101 first pass; the remaining reds are CDP evaluate/timing timeouts on the NAS, green locally. Not done (P3, follow-up): 'Idle · Anti-cycling' firmware label vs 'anti-cycling pause', badge aria-label vs visible text (WCAG 2.5.3), takeover+STOPPED sub-line, WAITING/IDLE hysteresis, STOPPED vs PAUSED colour.
<!-- SECTION:NOTES:END -->
