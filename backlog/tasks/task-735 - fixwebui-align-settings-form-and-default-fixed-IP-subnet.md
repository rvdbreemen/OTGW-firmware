---
id: TASK-735
title: 'fix(webui): align settings form and default fixed-IP subnet'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-27 21:59'
updated_date: '2026-05-27 22:35'
labels:
  - webui settings
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Settings tab labels and inputs need consistent left alignment, and fixed-IP prefill should populate subnet mask from the current WiFi configuration or fall back to 255.255.255.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All settings labels in the affected settings tab are left aligned like the IP Configuration label.
- [ ] #2 Input fields in the settings tab start on a consistent left edge and remain visually aligned with each other.
- [ ] #3 Fixed-IP prefill populates subnet mask from current WiFi settings when available.
- [ ] #4 When no current subnet mask can be inferred, fixed-IP prefill defaults subnet mask to 255.255.255.0.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the settings page markup, CSS, and fixed-IP prefill code paths.
2. Update styles/markup so labels and controls use a consistent left-aligned form layout.
3. Update fixed-IP prefill so subnet mask is filled from current WiFi config or defaults to 255.255.255.0.
4. Run focused checks/build commands that cover the Web UI asset path.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev (ahead 8 during validation).
Coding agent: @codex.
Implementation: moved the settings form row alignment into index_common.css so light and dark use the same left-aligned label/input grid; set .settingDiv text alignment to left in both index.css and index_dark.css; brought dark fixed-IP row/label alignment in line with light; updated prefillFromDHCP() to accept subnet masks with 255 octets and default empty/invalid subnet data to 255.255.255.0 while ignoring all-zero dotted values.
<!-- SECTION:NOTES:END -->
