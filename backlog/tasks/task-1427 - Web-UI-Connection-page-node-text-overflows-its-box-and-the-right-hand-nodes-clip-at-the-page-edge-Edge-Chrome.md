---
id: TASK-1427
title: >-
  Web UI Connection page: node text overflows its box and the right-hand nodes
  clip at the page edge (Edge/Chrome)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T15:19:00Z'
updated_date: '2026-11-03T06:14:00Z'
labels:
  - web-ui
  - tester-feedback
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 591000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester report, maarten_41762 (<@1544684325847703602>), #alpha-testing 2026-10-28 15:16Z, msg 1565021558026543135 (https://discord.com/channels/812969634638725140/1514720723980259460/1565021558026543135), screenshot saved locally at %TEMP%\discord-attach\maarten-1565021558\image.png. Monitor > Connection diagram: 'master · seen 1s ago' / 'slave · seen 1s ago' run past the Thermostat and Boiler boxes; on the right the Broker and This browser nodes are cut off at the card edge ('Broker — H', 'This brows'). Edge and Chrome. Cosmetic, reporter calls it very low priority.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Screenshot before/after at the reporter's width and at 1280 px: node text stays inside its box and the right-hand nodes are fully visible
- [x] #2 Phone width (375 px) still works, no horizontal page scroll
- [ ] #3 build (filesystem) green; no new FAIL in evaluate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 maintainer decision (Robert, in session): released ('vrijgeven'); label needs-maintainer-ok removed.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: connRecency layout v2.js:1844 unchanged. Doable.

Released by Robert 2026-11-03 (interactive, maintainer) (was already In Progress).

2026-11-03 (agent, verified by lead incl. screenshots in scratchpad shots/1427-old|new): browser test tests/webui/connection-map-text-fits.test.mjs on the real page, red on the old v2 files, green on the fix; integrated in wt-next.
<!-- SECTION:NOTES:END -->
