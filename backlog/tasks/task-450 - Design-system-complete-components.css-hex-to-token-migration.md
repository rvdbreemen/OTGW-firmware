---
id: TASK-450
title: 'Design system: complete components.css hex-to-token migration'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-27 18:38'
updated_date: '2026-04-27 18:47'
labels:
  - ui
  - design-system
  - refactor
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-435 follow-up A and B passes, components.css still contains 155 raw hex codes (100 unique). These fall into three categories: (1) context-ambiguous codes like #fff that map to fg-inverse / bg-surface / slider-thumb-bg depending on usage, (2) status palette soft variants (#ffcdd2, #c8e6c9, etc.) that need new tokens like --status-error-soft / --status-ok-soft added to ds-tokens.css first, (3) SAT-specific tile colors (#b2dfdb, #80cbc4) that may warrant SAT-scoped tokens.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 components.css contains zero raw hex codes (grep -E '#[0-9a-fA-F]{3,8}' returns no matches outside comments)
- [x] #2 ds-tokens.css extended with status-*-soft variants needed by the soft status backgrounds
- [ ] #3 Light + dark theme regression-tested via /design.html on real hardware
- [ ] #4 Build clean for both ESP8266 and ESP32-S3 targets
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Phase 1 — Audit: list every remaining unique hex code in components.css with line-context, group into 4 categories (status-soft, neutral-extras, sat-specific, context-ambiguous like #fff). Phase 2 — Token expansion: add the missing tokens to ds-tokens.css (--status-*-soft for soft-bg variants, --sat-tile-* for SAT panels, --fg-disabled for #6c757d). Phase 3 — Context-aware replacement: handle #fff/#ffffff by reading the surrounding rule and choosing fg-inverse, bg-surface, slider-thumb-bg, or sim-badge-text. Phase 4 — Bulk replace_all for the new tokens via Python script. Phase 5 — Verify zero hex remain (grep -E '#[0-9a-fA-F]{3,8}' returns no non-comment lines), build clean for both targets, post final commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-27: Phases 1-4 executed.

- Phase 1 audit: 100 unique remaining hex codes categorised into status-soft, sat-tile, neutral-extras, context-ambiguous, and status-palette-extras.
- Phase 2 ds-tokens.css extension: added 16 new tokens (status-{ok,warn,error}-soft-{bg,border,text}, status-info-soft-{bg,border}, status-error-darker, sat-tile-teal-{bg,border,accent}, fg-disabled, fg-mute) with light + dark mirrors.
- Phase 3 context-aware replacement: regex-based pattern replace for #fff/#ffffff (color: -> fg-inverse, background: -> bg-surface) — 15 hits.
- Phase 4 bulk replace: 138 + 1 = 139 additional hex-to-token swaps mapped through Python script.

Total this round: 154 replacements, bringing components.css from 155 hex -> 0.

Cumulative across all TASK-435 follow-up B + TASK-450:
  Original: 379 raw hex hits in components.css
  Final:    0 raw hex hits
  Reduction: 100%

Token health audit:
  117 tokens defined in ds-tokens.css
  114 unique tokens referenced in components.css
  9 legacy aliases (--bg-color, --border-color, --primary-color, etc.) defined locally in the FSexplorer-merged sections of components.css; all alias to a canonical ds-tokens.css token via var(--*). No raw hex in any alias chain.
  --slider-pct is a runtime variable set by sat-slider.js, no static def needed.

ACs 1+2 met. AC3 (hardware regression) requires flashing — pending user verification. AC4 build verification running.
<!-- SECTION:NOTES:END -->
