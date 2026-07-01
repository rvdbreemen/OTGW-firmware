---
id: TASK-981
title: 'feat(v2-webui): command bar echo-into-log + PIC/OT prompt styling'
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-01 22:17'
labels: []
dependencies: []
ordinal: 193000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area monitor-cmdbar (findings 1-5). Echo sent command + PIC acknowledgement into the log console: render raw WS event lines (prefix > < ! * S) that rawFromLine currently drops, with .cmd/.rsp/.err colouring via per-line DOM elements (never innerHTML). Unpause log on send. Restyle to .cmdbar with prompt label bound to otcommandinterface (PIC vs OT prompt; v2 supports OT-Direct so a hardcoded PIC label would be wrong). Input attrs maxlength=14, autocomplete=off, spellcheck=false, mockup placeholder wording. Error chip shows the API error body message instead of the bare status code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sent command and PIC/OT acknowledgement (incl. NG) appear in the log console with distinct colouring
- [ ] #2 Log unpauses on command send
- [ ] #3 Prompt label follows otcommandinterface
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->
