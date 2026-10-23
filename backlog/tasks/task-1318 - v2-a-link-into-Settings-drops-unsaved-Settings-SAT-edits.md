---
id: TASK-1318
title: 'v2: a link into Settings drops unsaved Settings/SAT edits'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T12:39:00Z'
updated_date: '2026-10-23T21:51:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 441000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1312 review (pre-existing, not caused by it): showPage('settings') calls fetchSettings(), which resets setDirty = {}. A BLE 'Assign' or sensor-roster link while the user has unsaved edits on Settings or the SAT page silently discards them. Expected: keep the dirty map across a refetch (merge), or ask before discarding.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction in a webui test: edit a field, follow the BLE Assign link, the edit is still dirty (red on current code)
- [x] #2 Fix keeps unsaved edits across the refetch or asks first; the test goes green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Bug in v2.js: showPage('settings') -> fetchSettings() resets setDirty = {} and re-renders, so unsaved edits on Settings/SAT vanish when a BLE Assign or sensor-roster link jumps into Settings. Fix (KISS): fetchSettings keeps the existing setDirty map; after rendering the fetched values it re-applies each dirty value to its input (and keeps the dirty marking / save-bar state), so server values fill only untouched fields. A real save still clears setDirty as today. No asking dialog (a merge loses nothing and needs no new UI). Repro first: webui test (existing harness style, e.g. tests/webui/v2-settings-sat-followups.test.mjs) edits a field, follows the BLE Assign link (or calls the same showPage('settings') path), asserts the field value and dirty state survive: red on HEAD, green on the fix. Also check other callers of fetchSettings (save success path must still reset). Not a SAT behaviour change (UI only). Cross-line: no.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: new tests/webui/v2-settings-dirty-survives-refetch.test.mjs (fake device, real v2.html/v2.js, headless Chrome; edits two Settings fields, leaves and re-enters Settings via showPage('settings'), the same function BLE Assign/roster links reach through openSettingsGroup): HEAD v2.js 4 FAIL (values/dirty marks lost, save bar 0, save posts 0), fix 0 failures of 10. Review (opus): core fix holds; fixed F1 CHANGELOG entry was under 2.0.0-beta (moved to Unreleased/Fixed) and F2 a kept unsaved edit could override a setup wizard's saved answer (wizard Go now deletes its written keys from setDirty before the refetch; code-read, not covered by a test). Accepted F3 (local unsaved edit wins over an outside change; visible via dirty mark/save bar). Regression: v2-navigation 0/34, v2-settings-sat-followups 0/26 (peak in flight 2), settings-help fail 0, sat-page-order 0 FAIL; evaluate 0 fail; build.bat combo SUCCESS.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Following a link into Settings (BLE Assign, sensor roster) no longer throws away unsaved Settings/SAT edits: fetchSettings keeps the dirty map and re-applies it after the refetch, dropping an entry only when the key vanished or the device already holds that value; a save or Discard still clears, and the setup wizards' saved answers replace older unsaved edits of the same keys. Evidence: AC1 webui test red on HEAD (4 fails) and AC2 green on the fix (10/10), regression suites green. Not verified: Firefox/Safari and a real device; the wizard-key clearing is code-read only.
<!-- SECTION:FINAL_SUMMARY:END -->
