---
id: TASK-1128
title: 'feat-2.0.0: port the PIC diagnose screen from 1.x into the v2 web interface'
status: To Do
assignee: []
created_date: '2026-09-05 15:31'
labels:
  - feature
  - webui
  - pic
dependencies: []
priority: medium
ordinal: 276000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the PIC diagnose screen shipped on otgw-1.x.x (TASK-1127, v1.7.6-beta.2) to the 2.0.0 line, v2 shell only. The PIC can run Schelte Bron's diagnostic firmware, which replaces OpenTherm with an interactive text test menu on the serial line; today that needs a separate terminal tool. Not a mode: the page exists when device/info.picfwtype is 'diagnose', which a PIC-less board never reports at all. Transport is an adaptation, not a copy: 2.0.0 already has the byte-transparent raw stream (TASK-1111) with a single splice point in drainOTRawQueue(), but its producer is gated off by default, widening it forces a consumer gate, and the write path must go through enqueuePICTx() because direct UART writes are barred by both the linter and the threading model. Design reuses the existing v2 .console and .cmdbar components, so the whole port needs one new CSS rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A diagnose page exists in the v2 shell and is reachable only while device/info reports picfwtype 'diagnose'; it is absent on a gateway PIC and on any board without a PIC
- [ ] #2 The PIC menu appears when the page is opened, without the user having to send anything first
- [ ] #3 Typed input reaches the PIC and its output appears on the page, verified live against a diagnose PIC on real hardware
- [ ] #4 picfwtype rides on the 15s health poll so a PIC reflash is noticed without a page reload
- [ ] #5 The page is built from existing v2 components and adds no more than one new CSS rule
- [ ] #6 esp32-classic and esp32-combo build green, evaluate.py --quick shows no new failures, node --check passes on v2.js
<!-- AC:END -->
