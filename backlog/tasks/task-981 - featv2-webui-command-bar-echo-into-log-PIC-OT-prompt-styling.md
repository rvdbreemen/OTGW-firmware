---
id: TASK-981
title: 'feat(v2-webui): command bar echo-into-log + PIC/OT prompt styling'
status: In Review
assignee:
  - '@claude'
created_date: '2026-07-01 22:17'
updated_date: '2026-07-02 05:26'
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
- [x] #1 Sent command and PIC/OT acknowledgement (incl. NG) appear in the log console with distinct colouring
- [x] #2 Log unpauses on command send
- [x] #3 Prompt label follows otcommandinterface
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by subagent impl-981: eventLineClass() classifier + onWsMessage else-branch pushes real firmware WS event lines (> < ! * S) into logBuf; renderLog rebuilt as per-line DOM (DocumentFragment, textContent-only) with .cmd/.rsp/.err colouring; no client-side ack simulation (real PIC replies, avoids double echo). Prompt bound to otcommandinterface in fetchConn (PIC vs OT). Unpause-before-POST with Pause-label restore. Error body message surfaced from sendApiError JSON. Uppercase leading 2-letter code only. Placeholder em dash replaced per house style. evaluate.py --quick 0 FAIL, drift gate clean, JS parse OK. On-device echo verification pending.
<!-- SECTION:NOTES:END -->
