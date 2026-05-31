---
id: TASK-781
title: Fix capture MQTT debug Ctrl+C handling
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 13:50'
updated_date: '2026-05-31 13:58'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Keep the user's single-file capture-mqtt-debug.bat structure intact.
2. Avoid relying on Ctrl+C as the primary stop mechanism because cmd.exe can show the batch termination prompt after a child process handles the signal.
3. Add an in-console Q stop path inside the embedded PowerShell worker so capture cleanup, summary.txt, and transcript.txt are written without sending a console interrupt.
4. Route the PowerShell worker through start /b /wait and update help/README text to make the clean stop path discoverable while preserving Ctrl+C/Ctrl+Break as fallback interrupts.
5. Validate help/extraction and a timed dry run with temp output.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch metadata: dev tracking origin/dev, observed ahead 21. Coding agent: Codex.
Implementation notes: added a Q-key stop path in the embedded PowerShell payload, including a shared RequestStop method, a console-key poller, and stop-reason reporting. Updated runtime/help text and scripts/README.md so Q is the documented clean stop path; Ctrl+C/Ctrl+Break remain fallback interrupts because cmd.exe may still display its batch-job prompt for Ctrl+C. Validation: .\capture-mqtt-debug.bat --help succeeded; timed dry run with -DeviceHost 127.0.0.1 -BrokerHost 127.0.0.1 -DurationSeconds 1 -SkipToolInstall -OutputRoot $env:TEMP\otgw-mqtt-capture-validate succeeded and wrote summary.txt/transcript.txt.
Commit note: not finalized/committed yet because scripts/capture-mqtt-debug.bat and scripts/README.md already had substantial pre-existing unstaged user edits, scripts/capture-mqtt-debug.ps1 was already deleted, and the branch is ahead of origin/dev by 21 commits. A narrow commit cannot be produced from the current index without including user-owned script conversion work.
<!-- SECTION:NOTES:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Relevant script validation has been run or a blocker recorded.
<!-- DOD:END -->
