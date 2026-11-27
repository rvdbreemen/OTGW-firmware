---
id: TASK-3118
title: >-
  English settings help: sathpcycle/satcyclesperhour help gives seconds and
  cycles per hour but the dropdowns show minutes; ethdns help says gateway
  address but the field is the router
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T07:56:00Z'
updated_date: '2026-11-27T08:06:00Z'
labels:
  - webui
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1173000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-webui's translation reviewers (TASK-2927 review), relayed by the coordinator 2026-11-27. Two defects in the ENGLISH settings help (src/OTGW-firmware/settingsHelp.h): (1) the help text of sathpcycle and satcyclesperhour gives seconds and cycles per hour while the dropdowns in the web UI show minutes; (2) the help text of ethdns says 'gateway address' but the field is the router address (Ethernet.ino:70). Owner: otgw-webui. The fix changes firmware assets, so it rides a web UI beta and needs the prerelease bump (bin/bump-prerelease.sh); settingsHelp.h is deflated into settingsHelpGz.h (ADR-247), which must be regenerated.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The help texts of sathpcycle, satcyclesperhour and ethdns are corrected and checked against the UI labels and the field meaning in the code (cite file:line)
- [ ] #2 settingsHelpGz.h regenerated and python tests/test_progmem_assets_deflated.py green; one prerelease bump
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 Note (coordinator): the fix changes firmware assets (settingsHelp.h and the regenerated settingsHelpGz.h), so it rides a web UI beta and needs the prerelease bump with bin/bump-prerelease.sh. Owner: otgw-webui (took it 2026-11-27).

2026-11-27 (otgw-webui): fixed in settingsHelp.h: ethdns 'router address' (Ethernet.ino:70 uses the router when DNS is 0.0.0.0; the field settings.eth.sGateway is the router address); sathpcycle and satcyclesperhour now in MINUTES like the dropdowns (30/40/60 min and 20/15 min; v2.js enum_min_* and pwm_help_*), cycles-per-hour wording kept. FACT: helpFor() returns satPwmHelp() (the pwm_help_* strings, already in minutes) for these two keys, so the settingsHelp.h text was not shown in the UI today; corrected so the file, the settings-help test and the translations agree. settingsHelpGz.h regenerated; settings-help 29 and test_progmem_assets_deflated 4 green; one prerelease bump. Not verified: rendering on a device.
<!-- SECTION:NOTES:END -->
