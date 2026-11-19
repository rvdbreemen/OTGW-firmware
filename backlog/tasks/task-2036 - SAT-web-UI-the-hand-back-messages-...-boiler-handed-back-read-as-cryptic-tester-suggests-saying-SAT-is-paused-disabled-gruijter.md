---
id: TASK-2036
title: >-
  SAT web UI: the hand-back messages ('... boiler handed back') read as cryptic;
  tester suggests saying SAT is paused/disabled (gruijter)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T21:24:00Z'
updated_date: '2026-11-19T02:01:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1044000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-11-18 21:06 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572719799446802463), on beta.375 after 'No outside temperature for 1 hour: boiler handed back': 'Boiler handed back is cryptic. SAT disabled would make more sense to me.' The same phrase is used for the OpenTherm loss (v2.html satOtLost, satOutsideMissing; v2.js sat_ot_lost, sat_outside_missing; narration in SATcontrol.ino:4484). Note: SAT is not disabled in that state, it stays enabled and resumes by itself (entries 56, 57; SATcontrol.ino:4476-4492), so 'disabled' would be inaccurate; candidate wording e.g. 'SAT paused: no outside temperature for 1 hour; your thermostat controls the boiler'. Tester feedback, needs Robert's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides the wording (recorded in the task notes)
- [ ] #2 The web UI texts sat_ot_lost and sat_outside_missing (and the NL/DE packs) use the chosen wording; web UI tests green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 22:42 UTC Robert chose option A (DM 1572743956645552149, reply to the coordinator's question; relayed by the coordinator): sat_outside_missing EN 'SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler' / NL 'SAT gepauzeerd: al 1 uur geen buitentemperatuur, je thermostaat regelt de ketel'; sat_ot_lost EN 'SAT paused: no OpenTherm connection, the boiler is back under normal control' / NL 'SAT gepauzeerd: geen OpenTherm-verbinding, de ketel regelt weer normaal'. Released by Robert: needs-maintainer-ok removed. Implementation: ba (web UI strings + NL/DE packs), after TASK-1889. Check the DE pack wording with the translate skill. AC1 evidence: this note.

2026-11-19 ba: implemented in lane/ba-2036 (beta.380 local). EN/NL verbatim from Robert's option A; DE (neutral register) and EL (polite plural) per i18n/glossary.md: de 'SAT pausiert: seit 1 Stunde keine Außentemperatur, das Thermostat regelt den Kessel' / 'SAT pausiert: keine OpenTherm-Verbindung, der Kessel regelt wieder normal'; el 'SAT σε παύση: ...'. Also v2.html fallback text. Tests: new tests/test_sat_handback_wording.py + updated test_sat_ot_loss.py: red on origin strings (5 failed), green on the fix (28 passed). Web UI: i18n-lang 6/6, i18n-followups 4/4, i18n-no-hardcoded 3/3, lang-menu 7/7 (first run 1 load stall at 69 s, rerun green). evaluate --quick 88 passed, 0 failed. DE/EL not reviewed by the translate skill's adversarial pass (2 strings); a native check is welcome.
<!-- SECTION:NOTES:END -->
