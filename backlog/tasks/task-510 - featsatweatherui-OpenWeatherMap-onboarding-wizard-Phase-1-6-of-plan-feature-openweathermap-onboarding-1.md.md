---
id: TASK-510
title: >-
  feat(satweather+ui): OpenWeatherMap onboarding wizard (Phase 1-6 of
  plan/feature-openweathermap-onboarding-1.md)
status: To Do
assignee: []
created_date: '2026-05-02 11:04'
updated_date: '2026-05-02 11:26'
labels:
  - feature
  - satweather
  - ui
  - esp32
  - owm
  - follow-up
dependencies: []
references:
  - >-
    plan/feature-openweathermap-onboarding-1.md (lives on PR #559 branch; will
    land on feature-dev-2.0.0-otgw32-esp32-sat-support after Phase 0 merge)
  - 'PR #559 (Phase 0 prerequisite)'
  - ADR-056 (admin endpoint security and secret-handling)
  - ADR-091 (design-system class drift gate)
  - ADR-077 (streaming MQTT HA discovery; informs the modal/dialog cadence)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to PR #559 (Phase 0 — Open-Meteo refactor + PROGMEM fix + 5-min startup gate). PR #559 ships 3 of the 49 tasks in `plan/feature-openweathermap-onboarding-1.md`. This task captures the remaining 46 tasks (Phase 1 through Phase 6): the actual OpenWeatherMap onboarding wizard.

## Why a separate task

PR #559's title promised a full OWM onboarding wizard but only landed Phase 0 (Open-Meteo bug-fixes and refactor). Phase 1-6 is multi-component work that deserves its own scope:
- New firmware setting `sWeatherApiKey[64]` + persistence path
- Plain-HTTP `weatherFetchOWM()` in `SATweather.ino` with 15-min hard floor
- New REST endpoint `GET /api/v2/sat/weather/needs-setup`
- 4 native HTML `<dialog>` elements + new `.owm-dialog` CSS using existing design-system tokens
- Self-contained `owmOnboarding` IIFE in `sat.js` with 5-step wizard (no-temp → API key → location → validate → save)
- Browser-side OWM key validation via HTTPS (firmware can't do TLS — ADR forbids BearSSL on ESP8266)
- Reverse-geocoding step to show resolved city name
- Silent auto-prefill of lat/lon via Geolocation API on SAT panel activation
- Settings panel API key field with `inputType: 'password'` and "***" masking contract
- Manual hardware tests + documentation updates

Scope is large enough that splitting into 3-4 sub-PRs (firmware backend, REST endpoint, browser modal infra, browser onboarding logic) is realistic. Should NOT be attempted as one mega-PR.

## Plan reference

The full task breakdown is in `plan/feature-openweathermap-onboarding-1.md` on the branch where PR #559 lives. Once PR #559 (Phase 0) merges into `feature-dev-2.0.0-otgw32-esp32-sat-support`, the plan-doc lands at the same path on the feature branch and remains the spec for this task.

Specific tasks from the plan that this backlog item covers:
- **Phase 1** (TASK-006..014): Firmware OWM setting + HTTP fetch
- **Phase 2** (TASK-015..016): REST `needs-setup` endpoint
- **Phase 3** (TASK-017..022): Browser modal infrastructure (4 `<dialog>` + CSS)
- **Phase 4** (TASK-023..033): Browser onboarding logic + auto-prefill
- **Phase 5** (TASK-034..036): Settings panel API key field
- **Phase 6** (TASK-037..048): Manual verification + documentation

## Architectural decisions already locked in (from plan v1.2)

- HTTPS validation in browser, plain HTTP for ongoing firmware fetches (ADR forbids TLS on ESP8266)
- 15-minute hard floor for OWM via `DECLARE_TIMER_SEC(timerOWMPoll, 900, SKIP_MISSED_TICKS)` matching the Open-Meteo pattern
- API key masked as `"***"` in GET responses (per TASK-011); never expose actual key
- `sessionStorage` (not localStorage) for dismiss state — user reconnecting from new browser still gets reminded
- `<dialog>` element with `<form method="dialog">` for native close semantics
- Reuse existing `.sat-grid`/`.sat-row`/`.sat-btn` classes per ADR-091 design-system drift gate; only add `.owm-dialog` (≤60 lines)
- Open-Meteo remains the zero-config fallback when no OWM key is configured

## Hard prerequisite

PR #559 (Phase 0) must be merged first AND its two compliance gaps (TASK-003b OT-bus guard, TASK-004 min-interval floor) must be closed before this task starts. Otherwise the OWM path inherits the same rate-limit and OT-bus-redundancy issues.

## Out of scope

- Multi-provider weather aggregation (averaging Open-Meteo + OWM). Plan calls for mutually exclusive use.
- Caching weather data across reboots. Each reboot starts fresh; first fetch waits the 5-min startup gate.
- ESP8266 TLS support. Firmware stays plain-HTTP; only browser does HTTPS.
- Per-zone weather overrides for multi-area SAT. Future task if/when needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PR #559 (Phase 0) merged AND its two compliance fixes (TASK-003b OT-bus guard, TASK-004 min-interval floor) are in place on `feature-dev-2.0.0-otgw32-esp32-sat-support` before any work in this task starts
- [ ] #2 Phase 1 firmware tasks (TASK-006..014) complete: `sWeatherApiKey[64]` field added to `SATSection`, persisted via `settingStuff.ino`, masked via `"***"` sentinel in GET responses, allowlisted in `kSettingsAllowList[]`, `weatherFetchOWM()` implemented with 15-min hard floor and plain-HTTP, `outside_temp_valid` field added to `satSendStatusJSON()`
- [ ] #3 Phase 2 REST endpoint complete (TASK-015..016): `GET /api/v2/sat/weather/needs-setup` returns `{needs_setup, uptime_seconds, outside_temp_valid, api_key_configured}` and is wired into `kV2Routes[]`
- [ ] #4 Phase 3 modal infrastructure complete (TASK-017..022): 4 `<dialog>` elements (`dlg-owm-no-temp`, `dlg-owm-apikey`, `dlg-owm-location`, `dlg-owm-validate`) added to `index.html` with `<form method="dialog">`, new `.owm-dialog` CSS in `components.css` using existing design-system tokens (≤60 lines)
- [ ] #5 Phase 4 onboarding logic complete (TASK-023..033): `owmOnboarding` IIFE in `sat.js` with 5-step state machine, silent geolocation auto-prefill on SAT panel activation, OWM key validation via HTTPS from browser, reverse-geocoding to resolve city name, sessionStorage-based dismiss persistence, all `fetch()` calls have `.catch()` per CLAUDE.md
- [ ] #6 Phase 5 settings panel field complete (TASK-034..036): `satweatherapikey` field renders as `<input type="password">`, tooltip + label-map entries added
- [ ] #7 Phase 6 verification complete (TASK-037..048): all manual hardware tests pass, README updated to describe Open-Meteo default, OWM option, 15-min rate limit, key registration link
- [ ] #8 `python evaluate.py --quick` exits 0; PROGMEM compliance + design-system class drift gate PASS; no new failed checks
- [ ] #9 `python build.py` clean on ESP8266 and ESP32-S3; LittleFS rebuild required and shipped
- [ ] #10 ADR-056 amendment authored OR explicit rationale why amendment is not needed (decision documented in task notes)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
[2026-05-02] Inventory after PR #559 squash-merge (commit `cea66459`) shows the squash actually contained more than the initial review (the 5 commits on origin/copilot/...) suggested. Copilot pushed additional work between my first fetch and the merge. Updated scope:

**Already in code (skip these in re-execution):**
- TASK-006 `sWeatherApiKey[65]` field added to `SATSection` (note: 65 not 64 — includes NUL).
- TASK-007 `SATweatherapikey` persistence in `settingStuff.ino`.
- TASK-015/016 `/api/v2/sat/weather/needs-setup` endpoint wired into `handleSAT` and `kV2Routes[]`.
- TASK-023 `maybeAutoPrefillWeatherLocation()` in `sat.js`.
- Phase 4 wizard helpers in `sat.js`: `checkWeatherNeedsSetup`, `showOwmWizard`, `validateOwmKey`, `persistOwmSettings`.
- TASK-034 settings panel field for `satweatherapikey` (rendered as `hidden: true` rather than the plan's `inputType: 'password'` — close enough, but worth deciding which is canonical).

**Still to do:**
- TASK-009 `weatherFetchOWM()` plain-HTTP fetch in `SATweather.ino`. **Currently the OWM key is stored but never read for an actual fetch** — functionally the OWM path does nothing.
- TASK-010 `weatherLoop()` mutex between Open-Meteo and OWM paths.
- TASK-011 REST mask of the API key (return `"***"` sentinel in GET responses).
- Phase 3 native HTML `<dialog>` elements (TASK-017..022). The wizard helpers in `sat.js` reference DOM elements that don't exist yet.
- Phase 3 `.owm-dialog` CSS in `components.css`.
- TASK-013 `outside_temp_valid` field in `satSendStatusJSON()`.
- Phase 6 verification tasks (TASK-037..048).

**Effective scope reduction**: ~70% of Phase 1-4 already in code. Remaining work is roughly: `weatherFetchOWM()` + REST mask + 4 HTML dialogs + CSS + Phase 6 verification. Probably 1-2 PRs instead of 3-4.
<!-- SECTION:NOTES:END -->
