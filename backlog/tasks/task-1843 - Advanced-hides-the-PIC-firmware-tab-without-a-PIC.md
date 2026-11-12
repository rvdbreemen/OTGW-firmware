---
id: TASK-1843
title: Advanced hides the PIC firmware tab without a PIC
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:55:00Z'
updated_date: '2026-11-11T23:59:00Z'
labels:
  - webui
dependencies: []
priority: medium
ordinal: 918000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12 screenshot: on a PIC-less OT-Direct unit Advanced opens on PIC firmware showing only No PIC detected. Hide that sub-tab and open Advanced on Debug Information when no PIC is present.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Without a PIC the PIC firmware sub-tab is hidden and Advanced opens on Debug Information; with a PIC nothing changes
- [x] #2 Test red on the old code; screenshots with and without PIC
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: applyPicTabGate(isPic) hides #advTabs PIC firmware sub-tab without a PIC and moves to Debug Information when it was open; called from fetchPic() once device/info has a known otcommandinterface, and initPicTabGate() when Advanced opens (covers a remembered Debug tab). A failed device/info answer decides nothing (existing checking state stays). Test adv-pic-tab-hidden-without-pic.test.mjs (real page, headless Chrome): old code 2 fail / 1 pass (PIC case), fix 3 pass; v2-undefined-names 2 pass. Screenshots C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1843-OTDirect.png (opens on Debug, no PIC tab), C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1843-PIC.png.
<!-- SECTION:NOTES:END -->
