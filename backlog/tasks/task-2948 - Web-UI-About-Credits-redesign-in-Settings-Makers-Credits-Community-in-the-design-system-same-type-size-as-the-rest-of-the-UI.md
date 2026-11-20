---
id: TASK-2948
title: >-
  Web UI: About/Credits redesign in Settings: Makers, Credits, Community in the
  design system, same type size as the rest of the UI
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T10:44:00Z'
updated_date: '2026-11-20T11:58:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1078000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-20 via the coordinator (screenshot): the About and Contributors cards at the end of Settings have inconsistent typography. Cause (measured 2026-11-20, computed style in headless Chrome on origin/dev): the card paragraphs inherit the body's 16 px while the card heading is .95rem (15.2 px) and the settings rows use smaller text; the 'Credits' sub-heading is a bold paragraph, not a heading. Wish: neat screens with attribution in three groups: (1) Makers, Robert first; (2) Credits for upstream work (Schelte Bron PIC/OTGW, George Ntellas SAT algorithm, Alex SAT HA component, Nodo Shop; candidates from the code in TASK-2947); (3) Community. Names come from TASK-2947 (otgw-docs research, credits-research.md); who is named and how is Robert's decision. Constraints: existing design-system classes only (ADR-091 drift gate), headings translated (ADR-219 t()/data-i18n, nl/de/el), names not translated, before/after screenshots.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Typography: every text in the About/Credits cards uses the same font family and size as the other Settings cards (computed style test red on origin/dev, green on the fix)
- [x] #2 Three groups Makers (Robert first), Credits, Community with translated headings (I18N_EN + nl/de/el), names untranslated; only existing design-system classes (evaluate.py design-system drift gate no new WARN/FAIL)
- [x] #3 Name list as Robert decides on TASK-2947's open points (AI agents, real names from GitHub profiles only, short list plus CONTRIBUTORS file)
- [x] #4 Before/after screenshots at 1400 px and 390 px; full tests/webui sweep green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-webui implementation. Cause (measured, computed style, origin/dev): the TASK-1893 cards put text in bare <p> at the 16 px body size next to a .95rem h4 and .84rem settings rows. Redesign (v2.html only, no CSS change): three .adv-card cards Makers (#setAbout), Credits (#setCredits), Community (#setContrib), each entry a settings row: .srow > .slbl (name, bold, .84rem) + .shint (role, .7rem muted). Existing keys reused where the meaning held (about_credits, about_schelte*, about_george, about_alex*, contrib_*); new keys about_makers, about_maker_role, about_ai, about_ai_role, about_nodo, about_willem, about_ihor, about_bert, about_jens, about_robr, about_libs, contrib_full in I18N_EN and nl/de/el; about_title, about_by, about_kit dropped (unused); contrib_title now 'Community'. Footer test (TASK-1893) updated: Robert first via the role line, DaveDavenport.

Robert's decisions (via the coordinator and otgw-docs, TASK-2947): (1) 'B, and you may mention the products used over time': one neutral line under Makers naming GitHub Copilot, Claude Code and OpenAI Codex; (2) A, public names only: Code and Community lists are GitHub handles or Discord display names, from CONTRIBUTORS.md on origin/dev 53f9f94ee (Code: all 17; Community: top 20; confirmed pairs one entry, Roos-AID (RobR)); (3) 'A, rest in CONTRIBUTORS.md': a 'Full list of contributors' link to CONTRIBUTORS.md on dev. Credits keep the names as published in the code headers, library.properties and README credits (Schelte Bron, George Ntellas, Alex (Alexwijn), Nodo Shop, Willem Aandewiel, Ihor Melnyk, Bert Melis, Jens Fleischer, RobR), the same set CONTRIBUTORS.md credits. The maker role has no year: the research says 'since 2020' but the copyright line reads 2021-2026, so the role line stays out of that.

Evidence 2026-11-20. New test tests/webui/about-cards-typography.test.mjs (real v2.html/js/css, 1400 and 390 px): red on origin/dev data, 2/2 fail on 'every text uses the heading, row label or row hint size (15.2px, 13.44px, 11.2px)' with 56 texts at 16px; green on the fix 2/2. footer-copyright (TASK-1893, updated) green. Full tests/webui sweep one file at a time, timeout 180: 91 files rc 0, 423 pass, 0 fail. evaluate.py --quick: 88 passed, 0 warnings, 0 failed (design-system class drift gate clean: only existing classes adv-card, srow, slbl, shint). i18n audit nl/de/el: 0 missing, 0 orphan. Screenshots (synthetic page, no device data): OTGW-field-data/webui-screens/2026-11-20_task2948-about/ before-1400.png, before-390.png, about-1400.png, about-1400-end.png, about-390.png, about-390-end.png; no horizontal scroll at 390 px. Not yet on hardware: needs a build and a look on a unit (v2.html/v2.js live in LittleFS).

2026-11-20 UX review by otgw-ux-designer, round 1: fix before commit (1) name lists at .shint 11.2 px muted, too small to scan; (2) links in the browser default colour (#9e9eff dark, #0000EE light); (3) Nodo Shop linked on the name, the others in the role text; (4) AI line as a bold peer row under Makers reads as a second maker. Nits: (5) role hints in sentence case; (6) 'Community (Discord)' row under the Community card; (7) George Ntellas (Sergeant D) as in CONTRIBUTORS.md; (8) ESP32Async is an org, name AsyncTCP and ESPAsyncWebServer; (9) © row placement (kept). Coordinator decided 4 and 7 (AI wording 'Built with help from AI coding assistants (GitHub Copilot, Claude Code, OpenAI Codex) under Robert's direction and review.'). All of 1-8 applied: name lists in a plain block span at row size (.84rem, --text, weight 400); v2.css #page-settings .adv-card a{color:var(--accent)} (existing selectors, no new class); Nodo Shop plain, link 'OpenTherm Gateway kit' in the hint (new key about_nodo_link); AI line a hint-only row (about_ai dropped); roles sentence case in en/nl/de/el; rows 'Code on GitHub' / 'Discord'; libs line names AsyncTCP, ESPAsyncWebServer.

Round 2 (otgw-ux-designer): OK to commit. Dark: hint #a0a8b0 on #2c2c2e 5.79:1, links 5.83:1 (AA). Light (its own mock shot): hint #5e6266 on #fff 6.15:1; links --accent #0288a0 on #fff 4.17:1, under AA 4.5:1: a design-system token finding, filed in its heuristic review, not changed here. Focus ring on the links: by code reading only (v2.css has no outline:none on a; only .cmdbar-in and .satonb-slope remove it), not proven in a browser. Tests after the fixes: about-cards-typography 2/2, footer-copyright 2/2, home-room-follows-sat 7/7; evaluate.py --quick 88 passed, 0 warnings, 0 failed; i18n nl/de/el 0 missing, 0 orphan. Screenshots: OTGW-field-data/webui-screens/2026-11-20_task2948-about/v2/.

2026-11-20 Robert's decision (via the coordinator, verbatim): "Okay, ik wil dat je Willem Aandewiel NIET noemt. Verwijder hem uit de credits." Removed from the About/Credits screen: the Credits row in v2.html and the key about_willem in I18N_EN and nl/de/el. Copyright headers in the source files stay (licence); otgw-docs removes him from CONTRIBUTORS.md. Guards: footer-copyright.test.mjs asserts that neither the Settings markup nor v2.js contains 'Aandewiel' or about_willem (red on the previously staged v2.html/v2.js: 'Willem Aandewiel is not named on the About/Credits screen', green now 2/2); about-cards-typography.test.mjs asserts no card text names him.
<!-- SECTION:NOTES:END -->
