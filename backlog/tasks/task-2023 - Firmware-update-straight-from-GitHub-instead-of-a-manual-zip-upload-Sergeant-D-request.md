---
id: TASK-2023
title: >-
  Web UI: show 'new version available' from the GitHub releases API, checked in
  the browser (option B, Robert 2026-11-18)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T11:54:00Z'
updated_date: '2026-11-19T08:57:00Z'
labels:
  - feature
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1038000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Asked by Sergeant D in #dev-sat-mqtt 2026-11-18 11:37 UTC (https://discord.com/channels/812969634638725140/1105556725714649128/1572576499674914927): 'can we work on a new update process, like downloading the new version directly from GitHub repository instead of the user manually upload the zip file to the FW?' Constraint: the firmware is HTTP only (ADR-003, CLAUDE.md core rule 'No HTTPS or WSS'); GitHub release assets are HTTPS only; the app slot is tight on flash (TASK-1955). Options put to him (reply 1572580626995875892): A) helper on a PC or Home Assistant that downloads the zip from GitHub and pushes it to /update (no firmware change); B) gateway shows 'new version available' with a link, from a version file on a plain HTTP mirror; C) HTTPS in the firmware (reverses ADR-003, needs flash). Maintainer decision needed before any work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert chose an option (or declined), recorded in this task
- [x] #2 No firmware HTTP change: the check runs client-side in the web UI against the GitHub releases API; the gateway makes no outbound request (code review + network trace in the browser)
- [x] #3 The UI compares the latest release with the running fwversion and shows a notice with a link to the release page when it is newer; prerelease (beta) vs stable handled: a 2.x beta user is told about newer betas and stable releases, a stable user only about stable unless they opt in; tests on the real web UI files
- [x] #4 A setting switches the check off; with it off, no request to GitHub is made (browser network trace)
- [x] #5 Privacy note in the manual (EN and NL): the browser contacts GitHub when the check is on, and how to switch it off
- [x] #6 The check respects the web UI's limit of at most 2 API calls in flight and fails quietly when GitHub is unreachable or rate-limited (screenshot of both states)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN (ba, 2026-11-19). Option B, data source 2: the browser asks GitHub; the gateway makes no outbound request (ADR-003).

Facts checked in this session:
- GitHub CORS: `curl -sI -H "Origin: http://otgw.local" .../releases?per_page=1` returns `Access-Control-Allow-Origin: *`.
- Unauthenticated rate limit is 60/h per IP (X-RateLimit-Limit 60).
- 2.x releases are tagged vX.Y.Z-beta.N and flagged prerelease. Every 2.x beta has latest=false, because main is the 1.x stable line, so /releases/latest would return a v1.7.x tag and must not be used.
- Alpha builds (e.g. beta.382) are newer than the latest published release (374), so a numeric compare is needed.
- The v2.js API queue (ADR-184/TASK-1210, isApi()) only queues same-origin /api/ requests; a request to api.github.com goes straight to the browser and takes no device slot.
- No Content-Security-Policy in the firmware sources or v2.html (grep).

Design:
1. Data:
   - Fetch `https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=30`.
   - Keep only tags matching `^v2\.\d+\.\d+(-beta\.\d+)?$` (the running major, 2), that are not drafts.
   - Take the highest by semver: X.Y.Z numeric, `beta.N` numeric, and stable X.Y.Z above every X.Y.Z-beta.N.
   - Read fwversion from the cached withDeviceInfo() snapshot (no new device request) and strip +githash.
2. Beta/stable rule (AC3, team decision):
   - A beta/alpha build (isPrereleaseFw) sees newer betas and newer stable releases.
   - A stable build sees only newer stable releases, unless the opt-in "include betas" is on.
3. When:
   - Once, about 30 s after page start (after the startup burst).
   - The result is cached in localStorage with a timestamp; a new GitHub request at most every 6 h per browser.
   - On HTTP 403/429, a network error or a CORS error: no notice, no retry loop, keep the last cached answer. The quiet failure of AC6.
4. Display:
   - A small notice in the header next to the version chip: "New version available: v2.0.0-beta.390", with a link to `https://github.com/rvdbreemen/OTGW-firmware/releases/tag/<tag>`.
   - The link is built from the validated tag; html_url/body from the response are not used.
   - Rendered with textContent only.
   - A "dismiss until a newer one" button stores the dismissed tag in localStorage.
5. Off switch (AC4): a DEVICE setting, so it applies to every browser and the manual can say "switch it off in Settings":
   - `updatecheck` (bool), default per the open question below, in ONE constant (OTGW-firmware.h struct default).
   - `updatecheckbeta` (bool, stable users' opt-in for betas, default off).
   - Touchpoints (template bLEDblink):
     - OTGW-firmware.h struct
     - settingStuff.ino write + update
     - restAPI.ino GET addBool + POST whitelist
     - settingsHelp.h + regenerated settingsHelpGz.h (test_progmem_assets_deflated)
     - docs/api/openapi.yaml
   - The UI reads the flag from the settings snapshot it already loads; while the flag is off, no request to GitHub is made (not even with a cached answer older than 6 h).
   - AC2 read as "the gateway makes no outbound request": the device code only gets a setting, no network code.
6. i18n: new strings in en/nl/de/el per i18n/glossary.md.
7. Tests on the real data/v2.js with a fetch stub:
   - Red on origin: no notice, no setting.
   - Green:
     - Newer release: the notice appears with the right link.
     - Equal or older release: no notice.
     - A v1.x tag is ignored.
     - Stable user: betas are ignored unless opted in.
     - Beta user: sees a newer beta and a newer stable release.
     - Alpha newer than latest: no notice.
     - Setting off: 0 fetches to github.
     - 403/429/network error: no notice and no exception.
     - The queue's inFlight() unchanged during the GitHub fetch.
     - Cache: a second page start within 6 h makes no fetch.
   - Python: the setting is round-trippable (GET/POST whitelist) and the gz is in sync.
8. Semver compare: a few lines of project-specific code, so no library search (CLAUDE.md "trivial code").
9. ADR: a Proposed ADR (via adr-kit) "the web UI checks the GitHub releases API from the browser for a newer version; the device stays HTTP-only and makes no outbound request; device setting, cached at most every 6 h", after Robert's answer on the default. ADR context: no Accepted ADR governs this. ADR-213 ("Opt-in, local") is the nearest privacy precedent; ADR-003 HTTP-only stays intact.
10. AC5 manual note EN/NL: text supplied by ba, placed by otgw-docs via the coordinator (manual owner).
11. Build only after the 08:00 beta slot; bench by 09: the network trace with the setting on and off, both failure states (screenshots, AC6).

OPEN QUESTION for Robert (via the coordinator, after his TASK-2026 answer): the default of `updatecheck`.
- A) Default on. Users see new versions without doing anything. Every browser that opens the UI contacts api.github.com (the user's IP goes to GitHub, at most every 6 h per browser) without the user asking.
- B) Default off, with a one-time question in the UI: "Check GitHub for new versions? (your browser contacts github.com)" Yes/No; the answer becomes the device setting. Nothing goes to GitHub without consent; one extra dialog per device (the first browser to answer decides).
- C) Default off, silent. Fully local-first; only users who find the setting see updates, so beta testers on old builds stay unnoticed.
- Recommendation: B. It fits the project's local-first line (ADR-213 opt-in) and still gets most users to "on", at the cost of one dialog per device.
- Until he answers, I build B. The default sits in one constant, so A or C is a one-line change.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 Maintainer decision (Robert, DM 1572585865589428318, relayed by the coordinator), verbatim: "B en neem taak C op voor een latere release 2.1.0". Scope of this task is now option B only: the gateway shows 'new version available' with a link, from a version file on a plain HTTP mirror (no HTTPS on the device, ADR-003). Option C (HTTPS self-update) is filed as its own task for 2.1.0 (see the 2.1.0 task referencing ADR-218).

OPEN QUESTION for Robert (infra choice, coordinator 2026-11-18: put to him when someone picks up B, not before): which host serves the version file over plain HTTP, and who keeps it in sync with each release (by hand, or a step in /beta-release-2x and /release-2x).

Robert in #dev-sat-mqtt 2026-11-18 12:06 UTC (https://discord.com/channels/812969634638725140/1105556725714649128/1572583679895797861), verbatim: "For 2.0.0 we do B and for 2.1.0 a POC for updating from GitHub directly." So option B targets 2.0.0.

2026-11-18 PENDING with Robert (coordinator put it as 1/2): how the version check gets its data. (1) A plain-HTTP host we run that serves the version file; needs hosting and a sync step per release. (2) No host: the web UI in the user's browser asks the GitHub releases API itself (HTTPS is fine in the browser, the gateway stays HTTP-only per ADR-003), with a privacy note (the browser contacts GitHub) and an off-switch. Coordinator recommends 2. Not decided yet.

2026-11-18 Robert chose hosting option 2 (DM 1572594516429381716, a reply to 1572588660904296520, relayed by the coordinator), verbatim: "2". Design: no host and no firmware HTTP change; the web UI in the user's browser asks the GitHub releases API (HTTPS in the browser; the gateway itself stays HTTP-only per ADR-003), compares with the running fwversion and shows 'new version available' with a link to the release page; a privacy note in the docs (the browser contacts GitHub) and a setting to switch the check off. Owner: otgw-webui, after their BLE bug (coordinator). ACs rewritten for this design.

2026-11-18 The beta/stable rule in AC3 (beta users are told about newer betas and stable releases; stable users only about stable releases unless they opt in) is decided by the coordinator as a team decision; Robert may overrule (the coordinator has told him).

2026-11-19 05:33:49 UTC Robert chose option B for the default (DM 1572847348177047675, his answer right after the coordinator's reposted question 1572846887172706357; relayed by the coordinator): default off + a one-time 'Check GitHub for new versions?' question. That is UI_UPDATE_CHECK_DEFAULT_MODE 1 in UItypes.h, as built; the open question in the plan is decided.

2026-11-19 ADR-262 Proposed (adr-kit; number 262 because 261 is taken in d3's lane-2026, confirmed by the coordinator): browser-side check, no outbound request from the gateway, default B in one constant, Enforcement forbids api.github.com in firmware .ino. adr-lint: strict pass; tests/test_adr_governance.py OK. ADR-003 not machine-related: adr relate refuses it (legacy ADR without Status History); the link is in Related Decisions. AC5 manual text placed by otgw-docs in docs-2023 b75dfbe90 (not pushed; waits for the firmware hash).

2026-11-19 beta.383 build 629e1a049 (wt-ba-1889 warm tree, JOBS=1): exit 0, app 2150752 B (93.8 %, +560 B vs 379); ino sha256 E869775C..., littlefs 1578A088...; archive OTGW-build-archive\2.0.0-beta.383+629e1a0.zip. Bench request with 09 (queue #4). Browser web UI tests on 629e1a049: theme-toggle-labels 4/4, settings-reboot-badge 1/1, v2-settings-dirty-survives-refetch first 0/1 then a >5 min stall (reaped) while two llama-servers held 9 GB commit (91 %); after the coordinator killed them (commit 75 %) the same test passed 1/1 within a 3 min timeout: the red was machine load, not TASK-2023 (its fake device has no updatecheck key, so maybeUpdateCheck() returns on its first line). settings-fetch-retry, lang-menu, v2-navigation pending (guard: commit charge < 80 %, no build).

2026-11-19 bench 09 on .63, 383+629e1a0 (screenshots 09 scratchpad b383\ui\): (AC4) the question appears after ~30 s with 0 GitHub requests before it; 'No' gives POST updatecheck=false then updatecheckasked=true and no question after a reload; (AC2/AC3) switched on: exactly 1 GET https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=30 without Cookie and Referer, all from the browser; a planted cache shows the notice with the release link, the x keeps it hidden after a reload; (AC6) GitHub blocked: no notice, no JS error, failure cached 6 h ({rel:[]}), no retry; 0x 503 from the device; updatecheckasked not shown as a row. AC2 code side: tests/test_update_check_settings.py test_gateway_never_calls_github (no firmware source names api.github.com). Not measured: an outbound capture at the router, and the 2-in-flight limit under load (covered by the queue assertion in update-check-github.test.mjs). '2 connection issues' in the header on the bench = the bench state: the count covers only ws/mqtt/wifi/boiler/therm (v2.js header status), .63 has no boiler and no thermostat; GitHub is not one of them.

2026-11-19 after the bench (09's note d): the one-time question now names api.github.com like the help text, the manual and ADR-262 (upd_ask_body en/nl/de/el); tests/test_update_check_settings.py asserts it in all 4 languages (red on the 629e1a049 texts, green now). A text-only change, not benched; it rides on the renumbered build. Browser web UI tests on 629e1a049: theme-toggle-labels 4/4, settings-reboot-badge 1/1, v2-settings-dirty-survives-refetch 1/1 (after a load stall at 91 % commit); settings-fetch-retry 0/2 as two Chrome stalls at 79 % commit (CDP no reply, 0 GETs), lang-menu and v2-navigation not run yet (guard commit < 80 %).

2026-11-19 AC5: the privacy note is on origin 163bb8fbb (otgw-docs): EN docs/manuals/en/ch03-web-interface.md '#### Check GitHub for new versions', NL docs/manuals/nl/h03-webinterface.md '#### GitHub controleren op nieuwe versies', both 'From firmware 2.0.0-beta.387', with who contacts GitHub, what is sent (no referrer, no cookies), the one-time question, how to switch it off (Settings > Behavior / Instellingen > Gedrag), the beta rule and the x; checked in this session (grep, no TBD-2023 left). Firmware pushed as beta.387 (origin 63a773a4b..850f555bc), zip msg 1572895492637991013. Browser web UI tests on 850f555bc: settings-fetch-retry 2/2, lang-menu 7/7, v2-navigation 1/1 (commit 76 %), with theme-toggle-labels 4/4, settings-reboot-badge 1/1, v2-settings-dirty-survives-refetch 1/1 earlier.

2026-11-19 ADR-262 Accepted by Robert (DM 1572896690426486836, 08:49 UTC, 'A' to the coordinator's question naming ADR-262; relayed by the coordinator). adr accept --confirm signed 'Agent: Claude (ba), for User: Robert van den Breemen'; the accept clarity gate first flagged GET, POST and PSRAM (adr-lint gate_clarity), fixed by code spans and 'an external memory chip (PSRAM)'; adr-lint strict pass, test_adr_governance OK.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The web UI shows 'New version available' from the GitHub releases API, checked in the user's browser; the gateway makes no outbound request (ADR-003 kept; ADR-262 Proposed). Robert: option B, browser as data source, default off with a one-time question (DMs 1572585865589428318, 1572594516429381716, 1572847348177047675). Shipped in beta.387 (850f555bc). AC1: Robert's choices (notes). AC2: no firmware network code (tests/test_update_check_settings.py test_gateway_never_calls_github; ADR-262 Enforcement) and bench 09 network trace: all GitHub traffic from the browser. AC3: releases filtered to the running major, semver compare, beta/stable rule; tests/webui/update-check-github.test.mjs 13/13 on the real v2.js (red 0/13 on origin, 3 mutants red); bench: 1 GET without Cookie/Referer, notice + link + dismiss. AC4: device setting updatecheck (+ hidden updatecheckasked, updatecheckbeta), off by default: bench 0 GitHub requests before consent and after 'No'. AC5: manual EN/NL on origin 163bb8fbb. AC6: GitHub request bypasses the ADR-184 queue (test: inFlight 0), bench 0x 503; quiet failure with a 6 h failure cache (test 403/429/network/non-list; bench blocked). Not verified: an outbound capture at the router; 2-in-flight under heavy load; the dialog text api.github.com after the bench (text only, asserted by test).
<!-- SECTION:FINAL_SUMMARY:END -->
