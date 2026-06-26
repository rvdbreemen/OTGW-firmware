---
id: TASK-933
title: >-
  feat(v2-webui): align live v2 UI with the design mockup (ground truth) on real
  data
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-25 17:20'
updated_date: '2026-06-26 22:27'
labels: []
dependencies: []
ordinal: 147000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The shipped v2 Web UI (v2.html/v2.js/v2.css) diverges from the approved design mockup (docs/design/boiler-dashboard-concepts.html on branch claude/webui-gauge-widget-design-s5t9s, PR #649). Bring the live implementation to match the mockup's structure, labels, and interactions, but bound to REAL device data/REST keys (not the mockup's simulated data). Confirmed gaps: settings show raw REST keys instead of human-readable labels/hints/categories; connectivity collapses the OT bus into a single down state instead of the mockup's two-link (thermostat vs boiler) MODE-vs-HEALTH model. Full audit across Home (A/B/C), Monitor (Log/Stats/OT Support/Graph/Connection), Settings, Connectivity to follow as the issue list. Ground truth = the mockup; data source = the real OTGW32 at 192.168.88.39.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Comprehensive issue list produced: mockup vs impl, every page/dashboard/setting, with file:line and fix direction
- [x] #2 Settings render human-readable labels + hints + categories + REBOOT badges (mockup SET_CATS model) bound to real REST keys
- [x] #3 Connectivity models OT bus as two links (thermostat/boiler) with MODE-vs-HEALTH vocabulary per the mockup
- [ ] #4 Home A/B/C dashboards match the mockup layout/labels on real OT data
- [x] #5 Monitor sub-tabs (Log/Stats/OT Support/Graph/Connection) match the mockup
- [ ] #6 Final: live v2 UI visually matches the mockup at desktop+mobile, driven by real device data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
MASTER ISSUE LIST (4 parallel audits, mockup=ground-truth). Root pattern: v2.html+v2.css are faithful 1:1 mockup ports (all CSS classes exist); gaps are in v2.js wiring + ~20 lines firmware REST.

FIRMWARE (done): /health (sendHealth, restAPI.ino) now emits thermostatconnected, boilerconnected, otcommandinterface, otgwmode, ntpenable (mirror of device/info).

SETTINGS (P0/P1) — biggest, user's explicit complaint:
- Raw REST key shown as label (v2.js:701 lbl.textContent=k). Build lowercase-keyed catalog: label+hint+category+sub-group+reboot+enum. Mockup SET_CATS uses mixed-case keys -> won't bind; re-key to lowercase (GET is lowercase, restAPI.ino:3233).
- Missing: hints, REBOOT badges, sub-groups, masonry, enum selects.
- Enum fixes: boardmode i 0-2 Auto/PIC/OTDirect (mockup 4-opt WRONG), satsystem 0-2 (mockup 0-3), satsource 0-3, sathpcycle 1800-2400 SEC (mockup minutes), otdmode 0-4, gpiooutputstriggerbit/webhooktriggerbit 0=Flame/1=CH/2=DHW/3=Fault, satmanufacturer, satheatingmode 0/1.
- Password: GET returns sentinel 'password=N'; empty input = leave unchanged; skip POST unless typed.
- Drop mockup 'usedhcp' (no such key; DHCP=empty wifistaticip). ssid readonly. Omit ui_usev2.
- Phase 2 (firmware): ~50 OTD-PID/curve/bypass/vent + SAT DHW/pressure/zones/sensorarea/boilermodel keys persisted but NOT REST-exposed -> expose in sendDeviceSettings + knownSettings[] before wiring, else drop. Do NOT ship dead 200-echo inputs.

CONNECTIVITY (P0) — second complaint:
- Split OT bus -> thermostat+boiler (data now on /health; UI fallback to bOnline when otcommandinterface=OT-Direct).
- Mode chip: use otgwmode (NOT networkmode, current bug v2.js:605/628 shows WiFi in GATEWAY chip); blue st-mode in strip+map.
- Five-state: st-off (disabled-in-settings), st-unknown (pre-fetch); MQTT off -> grey not red.
- renderConnDetail: grouped 11-row chain (Heating bus/Network/Integrations/Browser) + fix hints + recency + PIC/HA/REST/Live rows (currently flat 5 rows v2.js:931-950).
- Strip: 6 pills (Live/Mode/MQTT/WiFi/Boiler/Thermostat); NTP pill hardcoded green -> wire or drop.

OT SUPPORT (P0):
- No OTmap spec table in v2.js -> port from firmware OTGW-Core.h / classic index.js. Detail panel: Decimal/Hex/Spec name/human name/Data type/Direction/Unit/support badge/conclusion (CSS sd-spec/sd-badge/sd-concl already exist, dead). Default-pin + placeholder; hover spec name for all 128.

HOME Concept A (P1):
- Heat-pump source-aware renderA: LCD HP not CH, tag COMPRESSOR, tile Compressor.
- statusSentence() restore (modulation %, 'tap open', fault sentence, standby).
- rad-bar tint writer (flow-temp). flame --fx width curve. multi-state LCD. populate #aStrip mobile. tile casing ON/OFF/FAULT/OK.
Concept B: TT= preview vs real write (keep real); dial arc=setpoint/tick=room (currently inverted). Concept C: grid 11 cells (+T room/outside/Status), pressure unit+severity; ticker color via DOM spans.

HEADER (P2): UI-switch as <button> distinct class (not span cloning theme-toggle), keyboard-operable; dark accent follow brand-cyan not accent-primary (blue/cyan split-brand).

DO NOT REGRESS: sortable stats, wired Clear/Download/Graph PNG/CSV, real settings API, textContent XSS-safe, live dBm, mobile tap-targets (TASK-932).

Picking up via fresh import of the claude.ai 'Mockups into design system' project (PR #649 source): OTGW Patterns and Tokens.dc.html + OTGW Mockup Screens.dc.html. Plan = (A) import design assets into docs/design/ [absent on dev], (B) re-diff imported design vs current v2.{html,js,css}+firmware REST -> updated file:line issue list (AC#1), (C) implement still-open self-verifiable slices; device AC#6 (.88.39) left for maintainer sign-off. Coordinating to avoid collision with open 924/925/932.

AC#1 done: consolidated file:line issue list at docs/audits/2026-06-26-v2-mockup-alignment-audit.md. Ground truth = interactive mockup docs/design/boiler-dashboard-concepts.html (maintainer directive 2026-06-26). Result: live v2 is a faithful, largely-complete port; original-audit OPENs (settings labels/cats, OT-Support spec table, two-link conn, sortable stats, graph CSV/PNG) now DONE. Residual: 7 trivial fidelity fixes, 3 medium (ticker colour, string-enum selects, SAT long-tail SET_META), 1 firmware (/health per-link age for conn recency/st-warn), 5 maintainer design calls (dark nav cyan vs #383838, dark off-LED brightness, tile radii, Concept-C Flame cell, pressure bands). Overlaps: 932 owns mobile tap-targets; 924 owns dhw token + a11y; 925's source-toggle/stats-sort/graph-export already landed.

Implemented all 11 self-verifiable alignment items (maintainer chose full incl. firmware). v2.js/html/css: (1) OT-Support sd-hint footnote + (7) sd-badge moved after name; (2) Concept-B footer 'Outside X · OTGW firmware'; (3) Concept-B headline reuses statusSentence (surfaces fault+modulation); (4) stats sort glyph ▲/▼ + aria-sort (updateSortIndicators + CSS); (5) theme-toggle aria-pressed/aria-label; (6) ui_graphtimewindow select; (8) Concept-C ticker coloured via DOM block-line spans (ts/t/b), no innerHTML; (9) enum-select path made string-safe + preserves out-of-list current value, webhookcontenttype select added (ntptimezone KEPT free-text: AceTime IANA zone-name, a fixed list risks offering a zone absent from the device registry -> silent UTC fallback); (10) SAT long-tail SET_META ~46 curated labels/hints + sub-groups (Presets/Weather/Solar/Summer/Comfort/Multi-area/PV-boost/Auto-tune/Simulation/BLE). Firmware (11) connectivity recency/ADR-155 degraded: promoted epoch*lastseen statics into state.otBus.t{Boiler,Thermostat}LastSeen (OTBustypes.h+OTGW-Core.ino), /health emits additive thermostat_age_s/boiler_age_s (-1 when never seen / OT-Direct), v2.js otLinkState() -> st-warn past 20s on PIC + CONN.*.seen populates the recency line. evaluate.py 0 FAIL; node --check v2.js OK; esp32 build running. AC#6 (on-device at .88.39) remains maintainer sign-off.

Self-verifiable ACs done (alpha.276, committed 62bf7252, pushed origin/dev; esp32 build green, evaluate.py 0 FAIL, node --check v2.js OK). AC#2 settings labels/hints/categories/REBOOT + SAT long-tail + enum selects: done. AC#3 connectivity two-link MODE-vs-HEALTH + recency/st-warn: done. AC#5 Monitor sub-tabs (ticker colour, stats sort glyph, OT-Support sd-hint): done. REMAINING (maintainer hardware sign-off, OTGW32 @192.168.88.39): AC#4 'on real OT data' value-binding and AC#6 final desktop+mobile visual match — code is correct + self-verified but live-data rendering and the st-warn stale band need the device. Moved to In Review pending that verification.

Playwright validation pass (mock OTGW device, REST-driven, light+dark): item1 sd-hint footnote ✓ visual; item3 Concept-B headline reuses statusSentence ✓ (HP-aware 'Heating · compressor on · 62%'); item4 stats sort glyph ✓ DOM (class 'sorted asc' + aria-sort ascending + ::after ' ▲'); item5 theme aria-pressed flips ✓; items6/9 enum selects render w/ correct values ✓; item7 sd-badge after name ✓; item8 ticker parse+colour mapping 5/5 Node (T/R→t, B/A→b); item10 ~46 SAT keys curated + sub-grouped (Presets/Weather/Solar/Summer/Comfort/Multi-area/PV-boost/Auto-tune/Simulation/BLE) ✓ visual; item11 connectivity 'Thermostat Degraded' amber from thermostat_age_s=25 while Boiler Connected from age=5 ✓ end-to-end + Mode GATEWAY from otgwmode ✓. Zero runtime exceptions on load. Self-verifiable ACs now browser-confirmed; AC#6 (live data/WS on real OTGW32) still maintainer sign-off.
<!-- SECTION:NOTES:END -->
