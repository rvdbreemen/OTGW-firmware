---
id: TASK-3118
title: >-
  English settings help: sathpcycle/satcyclesperhour help gives seconds and
  cycles per hour but the dropdowns show minutes; ethdns help says gateway
  address but the field is the router
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T07:56:00Z'
updated_date: '2026-11-27T08:00:00Z'
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
<!-- SECTION:NOTES:END -->
