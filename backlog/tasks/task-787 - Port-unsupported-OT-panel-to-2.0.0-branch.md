---
id: TASK-787
title: Port unsupported OT panel to 2.0.0 branch
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 16:47'
updated_date: '2026-05-31 16:55'
labels:
  - webui ui port
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Apply the TASK-785 Web UI fix to the feature-dev-2.0.0 OTGW32/ESP32 SAT support branch so long boiler unsupported OpenTherm messages are shown in a readable panel instead of overflowing the Statistics footer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Feature 2.0.0 branch renders boiler unsupported OT messages outside the Statistics footer/table so long lists do not overflow.
- [x] #2 Unsupported messages are shown as understandable per-message rows with id, label, and read/write mode.
- [x] #3 Light and dark theme styles are present on the feature branch.
- [x] #4 The port is validated with focused local checks.
- [x] #5 Relevant previous dev UI-fix commits are identified and either ported or recorded as already present on the feature branch.
- [x] #6 MQTT publish-on-change settings UI from dev TASK-782 is present on the feature branch.
- [x] #7 Mobile settings field wrapping from dev TASK-783 is represented in the feature branch components.css layout.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Confirm the feature 2.0.0 worktree state and compare its current unsupported OT UI against the TASK-785 dev commit. 2. Port the TASK-785 Web UI changes into the feature branch, resolving any conflicts with existing 2.0.0 UI code. 3. Run focused syntax/rendering checks, update TASK-787 ACs/final summary, commit, and push the feature branch.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Started on branch feature-dev-2.0.0-otgw32-esp32-sat-support as @codex. Scope is to port the already validated TASK-785 unsupported OT panel UI from dev into the 2.0.0 feature branch.

Scope expanded per user request: also port previous UI-fix commits made on dev into the feature 2.0.0 branch, not only the TASK-785 panel follow-up. I will identify recent dev UI/webui commits, compare them against the feature branch, and port only missing changes.

Identified dev UI commits to port: TASK-782 (MQTT publish-on-change checkbox), TASK-783 (mobile settings field wrapping), TASK-784 (boiler unsupported banner overflow), TASK-785 (separate unsupported OT panel). The 2.0.0 branch already has TASK-692 tooltip behavior, so TASK-784/TASK-785 will be adapted into components.css and will replace the tooltip/badge with the separate panel.

Ported dev UI commits into the 2.0.0 design-system structure: TASK-782 publish-on-change checkbox in refreshSettings(); TASK-783 mobile settings wrapping as a components.css media rule; TASK-784/TASK-785 boiler unsupported overflow fix as a separate token-based panel below the Statistics table/footer.

Validation passed: node --check src/OTGW-firmware/data/index.js; git diff --check for index.html/index.js/components.css; inline Node mock confirmed refreshBoilerSupport() renders read/write unsupported message rows and clears/hides on empty state; static check confirmed mqttpublishonchange labels and tooltip text are present.

Non-blocking observation: untracked tests/test_webui_asset_versioning.py currently fails because FSexplorer.ino does not version CSS links such as ds-tokens.css/components.css. That cache-busting gap was already present and is outside this UI-port patch.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented on branch feature-dev-2.0.0-otgw32-esp32-sat-support by @codex. Ported recent dev UI fixes TASK-782 through TASK-785 into the 2.0.0 Web UI structure. Added the MQTT publish-on-change synthetic checkbox for mqttinterval, added mobile settings wrapping in components.css, and replaced the boiler unsupported footer badge/tooltip with a separate responsive panel below the Statistics table that lists each unsupported OpenTherm message with id, label, and read/write mode. Adapted the styling to 2.0.0 theme tokens instead of reintroducing dev's split index.css/index_dark.css files. Validation passed: node --check src/OTGW-firmware/data/index.js; git diff --check; inline Node mock for refreshBoilerSupport rendering/empty state; static check for publish-on-change labels. Residual unrelated note: untracked tests/test_webui_asset_versioning.py fails on pre-existing CSS cache-busting coverage in FSexplorer.ino.
<!-- SECTION:FINAL_SUMMARY:END -->
