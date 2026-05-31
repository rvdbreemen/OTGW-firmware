---
id: TASK-781
title: Fix capture MQTT debug Ctrl+C handling
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 13:50'
updated_date: '2026-05-31 13:50'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
capture-mqtt-debug.bat currently shows the Windows batch 'Terminate batch job (Y/N)?' prompt when stopping the diagnostic capture with Ctrl+C. Adjust the wrapper/stop flow so users can stop capture cleanly and still get the summary/transcript output.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Ctrl+C stop path avoids or clearly suppresses the interactive 'Terminate batch job (Y/N)?' prompt where feasible.
- [ ] #2 Stopping capture still leaves transcript and summary artifacts in the diagnostic folder.
- [ ] #3 Change is documented or discoverable through the script help/output when a different stop key is required.
<!-- AC:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Relevant script validation has been run or a blocker recorded.
<!-- DOD:END -->
