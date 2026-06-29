---
id: TASK-951
title: >-
  Add missing human labels in debug/device-info screen (eth_current_*,
  hardware_type, audit all keys)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-29 20:36'
updated_date: '2026-06-29 21:40'
labels: []
dependencies: []
ordinal: 164000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User found raw JSON keys showing instead of human labels on the device-info/debug screen: eth_current_subnet, eth_current_gateway, eth_current_dns, hardware_type. translateToHuman() (data/index.js:7014) returns the raw key when it is absent from translateFields[]. Audit ALL device/info keys AND settings-screen keys against the dict, add friendly labels for every gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every key rendered on the device-info/debug screen has a human label in translateFields[] (no raw key shown)
- [ ] #2 Settings-screen labels audited; any raw-key gaps filled
- [ ] #3 Live-verified on classic-S3: debug screen shows no raw snake_case keys
- [ ] #4 littlefs build green; evaluate --quick no new failures; prerelease bumped
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Both debug + settings screens use translateToHuman()/translateTooltip() over translateFields[]/translateTooltips[] (data/index.js:7115,7363). 79 keys missing labels (debug 20 incl 14 perf_*, settings 59: ui 8/sat 28/otd 18/wifi 5). Phase 1: add 79 translateFields[] labels matching existing style. Phase 2: add 79 translateTooltips[] entries with accurate explanations sourced from settings definitions (settingStuff.ino / *types.h min-max + OTDirect/SAT code + ADRs). One littlefs build + one prerelease bump + one commit (user flashes once). Units verified from settings struct before finalizing labels.
<!-- SECTION:PLAN:END -->
