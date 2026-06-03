---
id: TASK-808
title: >-
  fix(webui): active tab not restored on page refresh — 2.0.0 WebUI lands on
  Home
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 05:25'
updated_date: '2026-06-02 16:06'
labels:
  - webui
  - field-report
  - needs-repro
  - 2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report @sergeantd (alpha.99, OTGW32, 2026-05-30): "when I hit refresh and I am NOT in the Home page, it reloads and lands in the home page." The 2.0.0 WebUI does not restore the active tab across a browser refresh; it always returns to Home. Code: data/index.js logs window.location.hash (index.js:139) but has no evident restore-active-tab-from-hash logic; localStorage is used for PIC settings + log/data persistence, not for the active tab. Verify still present on current alpha.139 (finding is alpha.99-era, ~40 alphas old), then persist + restore the active tab via URL hash or localStorage. Frontend-only (data/index.js); rebuild filesystem with python build.py. 2.0.0-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Active tab is restored after a browser refresh (you land back on the tab you were viewing, not Home)
- [ ] #2 Home remains the default on first load / when no tab state is present; no console errors
- [x] #3 python build.py green (firmware + filesystem); evaluate.py --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-02 (loop) — implemented. Root cause confirmed in data/index.js: startMainPage() (init) only special-cased window.location.hash=='#tabPICflash'; every other load fell through to showMainPage() (Home). No active-page persistence existed (hash was only read for the PIC-flash deep-link).

Fix (frontend-only, data/index.js):
1. setActivePageSection() now persists the active page to the URL hash via history.replaceState (PAGE_SECTION_HASH map: ''=Home, #sat, #settings, #deviceinfo, #satsettings, #tabPICflash). replaceState = no history pollution; no hashchange handler exists so it does not re-navigate. Wrapped in try/catch (best-effort).
2. startMainPage() restore: on load, dispatches the hash to the matching page fn (satPage/settingsPage/deviceinfoPage/satSettingsPage), keeps the existing #tabPICflash board-class branch, defaults Home on empty/unknown hash. try/catch falls back to Home.

Cold-routing to non-Home pages is proven safe by the pre-existing #tabPICflash->firmwarePage() path; all page fns are parameterless + self-contained.

NOT persisted: webhookPage() (it hand-toggles classes, bypasses setActivePageSection) — niche Advanced sub-page, left as-is (refresh there still lands Home, unchanged).

Verified: node --check index.js clean (build.py does not lint JS). Bumped alpha.139->140. Full build in progress. AC#1-3 self-verifiable; AC#4 (field-confirm by @sergeantd on OTGW32) pending — ship alpha.140 for him to test.

Shipped in alpha.140 (commit d6195940, pushed). AC#3 verified (build both targets SUCCESS, evaluate.py --quick 61/0). AC#1/#2 (active-tab-restored / Home-default behavior) implemented + node-syntax-clean but need a browser to confirm; AC#4 = field-confirm by @sergeantd. Per maintainer 2026-06-02: do NOT announce the build to @sergeantd — builds are published tonight together with the user (zip upload). Task stays In Progress pending browser/field confirmation.
<!-- SECTION:NOTES:END -->
