---
id: TASK-1037
title: >-
  feat-2.0.0: port 1.7.2-beta.4 hardening (discovery heal, REST rate limit, UI
  pacing)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-26 22:06'
updated_date: '2026-10-01 17:07'
labels: []
dependencies: []
ordinal: 246000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the four 1.7.2-beta.4 changes from the otgw-1.x.x line to 2.0.0 (dev), adapted to this line's async/FreeRTOS/ESP32-S3 architecture, plus two defects found by adversarial review of that beta.

Origin: 1.x TASK-1043 (ADR-086 rate limit), 1.x TASK-1044 (UI poll reduction + local clock), 1.x TASK-1048 (ADR-087 daily drip republish), 1.x TASK-1035 (connstatus discovery).

NOT a cherry-pick. Verified divergences: 1.x TASK-1035 is already satisfied here (pseudo-id 244 = OTGWpiccontrolsid already carries gateway_mode + otgw_connected and is already queued); porting its renumbering would collide with OTGWdiag200id at 251. ADR-086 is already taken on this line (time-boundary single-caller, CI-gated), so new ADRs start at 170. v2.js is out of scope (WebSocket-driven, already ticks a local clock); only classic index.js is a port target.

Real bug found while mapping: publishNonOTDiscoveryConfigs() omits 7 non-bus-seen ids (243, 245, 251, 252, 253, 254, 255) that markAllMQTTConfigPending() reaches via its 0..255 LUT walk. None is ever bus-seen so JIT publish never reaches them - on a clean boot SAT never announces itself to Home Assistant until a settings save or manual republish.

Review defects being fixed on this line (sibling 1.x task needed, own worktree): (a) /api/v2/otgw/telegraf serves the identical payload as otmonitor from the same branch but was not rate-limited, bypassing the cap; (b) two dashboards phase-lock against a global per-endpoint budget so the same client is refused every cycle and freezes silently.

Full design: docs/adr/ADR-170..173 (to be written) and the session plan.

Granularity: maintainer chose one task / one prerelease tag. Tradeoff accepted: a field regression bisects to the whole port, not to one change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 publishNonOTDiscoveryConfigs() and markAllMQTTConfigPending() both queue the non-OT id set through a single shared helper; the 7-id boot gap (243/245/251/252/253/254/255) is closed
- [x] #2 Daily discovery auto-heal is an unconditional heap-gated markAllMQTTConfigPending() drip republish; startDiscoveryVerification() has zero automatic callers and remains reachable only from REST and telnet
- [x] #3 Daily heal heap precondition delegates to the drip's own restore predicate (no ported 8000 literal, no ESP.getXxx() direct call)
- [x] #4 /api/v2/otgw/otmonitor and /api/v2/otgw/telegraf share ONE rate-limit budget; exhausting one returns 429 on the other
- [x] #5 Rate-limit 429 carries RFC 9457 application/problem+json with retry_after in the BODY as well as a Retry-After header (header is not CORS-safelisted)
- [x] #6 Rate limiter uses burst>=2 so a Telegraf scrape alongside one open dashboard is not starved; sustained rate stays 1 per window
- [x] #7 503 (device-wide) and 429 (endpoint quota) remain semantically distinct; existing POST-cooldown 429s keep their ADR-035 envelope
- [x] #8 index.js polls otmonitor at 2000ms and device/time at 5000ms via named constants; GATEWAY_MODE_REFRESH_INTERVAL rescaled to 12 ticks to preserve the 60s wall-clock cadence
- [x] #9 index.js ticks the device clock locally from epoch+dateTime, with a fallback that still renders dateTime when the offset cannot be learned (no stuck 00:00:00 placeholder)
- [x] #10 On 429 the client re-phases at a random offset inside its period so two dashboards cannot phase-lock; verified with two tabs for 10 minutes
- [x] #11 After >=3 consecutive refusals the affected UI region is marked data-stale with an explanatory title; the selector exists in components.css so check_design_system_drift passes
- [x] #12 ADR-170..173 written (Proposed); ADR-062 gets a supersession note for its automatic mechanism only
- [x] #13 evaluate.py gains gates for alias-budget coverage, poll/window coupling, non-OT single source, and auto-heal shape, each as a module-level fn with tests in tests/test_evaluate.py
- [x] #14 openapi.yaml documents the 429 on /v2/device/time, /v2/otgw/otmonitor and /v2/otgw/telegraf, and states in prose that the two otgw paths share one budget
- [x] #15 ./build.sh green for esp32 target, python evaluate.py exit 0, python tests/test_evaluate.py green
- [ ] #16 Hardware: fresh boot with wiped broker announces SAT (252-255), diag (251), OTDirect (243) and S0 (245) discovery without a manual republish
- [x] #17 D1: rateLimitTryAdmit() admits the first GET after any idle gap, except one that ends within burst x window of a multiple of 2^32 ms, which is refused for at most one window; proven old-vs-fix with test/host/rate_limit_gcra.ps1 (idle 1, 24.8, 24.9, 30, 49 days and the wrap band)
- [x] #18 D2: the web client honours a 429 Retry-After for at most 4 poll periods (the backoffPeriod() ceiling) plus jitter, so a huge Retry-After cannot park a poller for days; tested on otmonitor (device/time uses the same makePacedPoller)
- [x] #19 D3: stop() is sticky: after a hidden-tab or teardown stop, a request that was in flight does not re-arm polling, and stop()+start() inside one in-flight window never has two requests of the same poller in flight
- [x] #20 D4: after a data-stale episode an element gets its own title attribute back, or none if it had none before
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete; static gates green. BUILD NOT VERIFIED in this container - see blocker below.

Done:
- ADR-171: queueNonOTDiscoveryIds() helper in MQTTstuff.ino; both publishNonOTDiscoveryConfigs() and markAllMQTTConfigPending() delegate. Closes the 7-id boot gap (243/245/251/252/253/254/255) that meant SAT never announced to HA on a clean boot.
- ADR-170: daily block in OTGW-firmware.ino now does a guarded markAllMQTTConfigPending() drip; startDiscoveryVerification() has no automatic caller left. Heap gate delegates to discoveryDripHeapHealthy() -> discoveryDripIsHeapHealthyForRestore() rather than porting 1.x's ESP8266-shaped >= 8000 literal. New state.discovery.iLastDailyHealEpoch published as otgw-firmware/stats/disc_last_daily_heal_epoch.
- ADR-172: GCRA rate limiter in restAPI.ino, const PROGMEM route table + RAM budget array so otmonitor and telegraf share ONE budget id (fixes the 1.x bypass). burst 2 so telegraf is limited without the ~75% starvation a burst-1 window would inflict. retry_after repeated in the RFC 9457 body because Retry-After is not CORS-safelisted. 503 stays ordered first and semantically distinct.
- ADR-173: index.js paced setTimeout poller replacing both setIntervals, re-phasing at a random offset inside the period on 429 (a phase lock is a phase problem; rate backoff alone cannot break it). Local device clock from epoch+dateTime with a fallback so a non-NTP device does not sit on [00:00:00]. GATEWAY_MODE_REFRESH_INTERVAL 60->12 ticks. data-stale after >=3 consecutive refusals + selector in components.css.
- 4 evaluate.py gates as module-level fns + check_* wrappers; 13 unit tests in tests/test_evaluate.py whose NEGATIVE cases reproduce the actual 1.x defects (telegraf bypass, alias with its own budget, reintroduced auto-verify, ported 8000 literal, window creep).
- openapi.yaml: RateLimited response component; 429 on /v2/device/time, /v2/otgw/otmonitor, /v2/otgw/telegraf; telegraf prose states the shared budget and a safe scrape interval.
- ADR-170..173 written (Proposed); ADR-062 Status carries a supersession note scoped to its automatic mechanism only.

Verified: python evaluate.py --quick -> 80 checks, 0 FAIL (1 pre-existing WARN, stale boards.h path in an unrelated gate). python tests/test_evaluate.py -> 61 tests OK. node --check on index.js OK. openapi.yaml parses and all three paths carry 429.

BLOCKER - firmware build could not run here. ./build.sh --target esp32 fails in toolchain provisioning, not compilation. Two separate environment faults: (1) ~/.platformio/penv had no pip - repaired with ensurepip; (2) the remaining failure is the agent proxy returning 403 for github.com archive/release downloads, so 'uv pip install' cannot fetch the pioarduino platformio-core zip, and ~/.platformio/packages contains only tool-esp_install (the entire ESP32-S3 toolchain and framework are absent). Nothing in this repo can fix that. The C++ in this commit is therefore UNCOMPILED. Verified by hand instead: webPushHeader/webSend overloads match the calls, WEB_MAX_PENDING_HEADERS reaches restAPI.ino via OTGW-firmware.h:930 -> networkStuff.h:32, and the const char[][API_WORD_LEN] parameter conversion is the same one kV2Routes handlers already rely on.

Remaining ACs (10, 15, 16) need a machine that can build and a bench device.

Pushed to claude/otgw-adversarial-review-4wg4uc; draft PR #673 opened against dev (https://github.com/rvdbreemen/OTGW-firmware/pull/673).

CI note: the 'ADR lint + index-check' job failed on the first push with 'ADR-166 (Proposed) is MISSING from docs/adr/README.md'. Confirmed PRE-EXISTING: origin/dev's README has zero ADR-166 references, so index-check was already red on the base branch before this work. Fixed in a follow-up docs-only commit (b4db0ba2) because it is a two-line index entry and it was blocking this PR's checks. Both adr_governance.py lint --strict and index-check now report 0 fail over 173 ADRs locally.

AC #15 (build) is expected to be closed by CI rather than locally: the PR runs 'pio run -e esp32', 'esp32-classic' and 'esp32-combo' on runners with network access, which is the first real compile of this C++. Self check-in scheduled to re-verify.

AC #15 CLOSED via CI. All 11 checks green on e39fc65e: pio run -e esp32, -e esp32-classic and -e esp32-combo all SUCCESS, plus evaluate.py --quick, ADR lint + index-check, claude-review, CodeQL and all four CodeQL Analyze jobs. This is the first successful compile of this port's C++ — it could not be built locally (sandbox proxy 403s GitHub archive/release downloads, so no ESP32 toolchain in the container).

One real defect was found by that first compile and fixed in e39fc65e: the Arduino .ino prototype generator hoists a declaration of every .ino function to the top of the combined TU, ahead of restAPI.ino's type definitions, so rateLimitTryAdmit(ApiRateLimitBudget&, uint32_t) produced a prototype naming an undeclared type and the definition then collided with it ('redeclared as different kind of entity'). All three targets failed on it. Fixed by passing the budget INDEX (uint8_t) and resolving the reference internally; no behavioural change. Hand-verification could not have caught this - the fault is in generated code, not in the source as written, which is exactly why the unbuilt state was flagged rather than glossed.

Remaining open: AC #10 (two-tab starvation, needs two browsers against a bench device) and AC #16 (fresh boot + wiped broker confirming SAT/diag/OTDirect/S0 discovery). Both need hardware. Separately, a sibling otgw-1.x.x task is still needed for the two review defects (telegraf bypass, 429 phase-lock starvation) on that line - it requires its own worktree.

2026-09-30 D1-D4 fixed (alpha.381), in-session evidence.
- D1 (AC#17) restAPI.ino rateLimitTryAdmit(): unsigned arithmetic plus one clamp (a TAT more than burst x window ahead can only lie in the past, so it restarts at now); signature, the four-field gApiRateLimitBudgets initializer, route table, headers and hook unchanged. test/host/rate_limit_gcra.ps1 slices the real function: OLD (-Rev HEAD 3e30c6e0e) 40 passed / 16 failed, all D1 (idle 2^31+1 ms refused 24.86 d, 24.9 d, 30 d, 49 d, wrap band after 2 GETs 2500/5000 ms); FIX 56 passed / 0 failed; the 600000-GET stream checksum a63e89bb22aa30b1 is identical on both, so normal operation is unchanged. A mutation of the clamp to >= makes the harness fail (workflow review).
- D2-D4 (AC#18-#20) index.js makePacedPoller()/setRegionStale(): Retry-After capped at 4 periods plus jitter; stop() is sticky through a generation token and no longer clears inFlight; a stale episode saves and restores the element's own title. tests/webui/paced-poller.test.mjs (headless Chrome, real index.js, mocked /api/v2): OLD (HEAD index.js) 6 of 12 FAIL (D2 0 re-polls in 24 s after Retry-After 1702968 s; D3a 2 requests after the hidden-tab stop; D3b 2 in flight; D4 #heap-info title lost); FIX 12/12 PASS (re-polls at +8.2 s/+16.8 s, 0 requests after stop, max 1 in flight, title restored).
- AC#15: build.bat --target all at alpha.381: esp32, esp32-classic, esp32-combo SUCCESS for firmware and filesystem (fresh 11:32-11:40, alpha.381+3e30c6e, images saved under %LOCALAPPDATA%/OTGW-capture/img-alpha381-3e30c6e); python evaluate.py (full) exit 0, 80 passed / 4 warnings / 0 failed; python tests/test_evaluate.py 66 tests OK.
Maintainer call (from the review): with stop() sticky, a fetch that never settles also blocks a stop()+start() restart because index.js has no fetch timeout; clearing inFlight in stop() instead brings the D3b overlap back. An AbortController timeout on the poller's fetch would remove the trade-off; not done here.
Follow-ups (not blocking): refreshDevTime()/refreshOTmonitor() direct calls that bypass the in-flight guard at index.js ~:5002 (applyPSmodeState) and ~:9352 (after a sensor-label save); ADR-172:113-115/:121-123 and ADR-173:93-94 text is stale after D1/D2 (adr-kit); restAPI.ino:2594-2595 and components.css:2117-2118 still claim two dashboards are both served.
OPEN: AC#10 (two tabs for 10 minutes) and AC#16 (fresh-boot discovery on a wiped broker) need the bench.

2026-09-30 docs aligned with the shipped behaviour (docs-only commit, no bump): docs/api/MQTT.md, docs/api/openapi.yaml, docs/c4/c4-code-mqtt.md, c4-component-integration-layer.md, c4-container.md and docs/manuals/nl/h10-bijlagen.md. An HA restart republishes STATE (ADR-174), not discovery; the reconnect republish only runs after >300 s offline; MQTTharebootdetection gates nothing; the daily heal (ADR-170) replaced the automatic verify; drip timing; the REST republish is queued for loop() (TASK-1176). Verification (workflow wf_fe6c4173-ec6, WP4 + review + fixup): a sentence inventory maps all 166 keyword lines of the six docs to code anchors (286 anchors, 0 failures) on both the base and current dev; 17 file:line citations checked for staleness on dev (0 stale); 39 regression patterns for the previously false sentences find nothing; openapi.yaml parses with the same 66 paths.

2026-10-01 AC#10 verified with a browser harness on the real code, not on the bench.

Setup:
- The shipped dev classic index.js (paced poller, ADR-173) runs in headless Chrome, one process per tab.
- A host server answers /api/v2/otgw/otmonitor and /api/v2/device/time with devoracle.exe: the real checkApiRateLimit(), rateLimitTryAdmit() and sendApiRateLimited(), sliced from restAPI.ino by anchor (slices checked verbatim and brace-balanced) and compiled with MSVC. The 429 status, Retry-After and the problem+json body with retry_after are the firmware's own.
- Tabs are told apart by a per-profile cookie. 10 minutes per run.
- Control: a one-line mutant of index.js whose 429 branch keeps the phase: penalise(opts.periodMs) instead of Retry-After + U[0,P).

Results:
- FIX, 2 tabs: otmonitor 191 / 208 grants, longest gaps 7.6 / 8.1 s; device/time 77 / 73, longest gaps 17.6 / 21.0 s. Balanced, no phase lock.
- FIX, 3 tabs: otmonitor 139 / 132 / 130, longest gaps 19.1 / 23.6 / 36.5 s; device/time 42 / 56 / 52, gaps 30.2-37.8 s. Balanced.
- MUTANT, 2 tabs: otmonitor 104 / 294 and device/time 120 / 29, a locked lopsided split.
- MUTANT, 3 tabs: otmonitor 120 / 31 / 249 (longest gap 76.5 s); device/time 109 / 22 / 19 (longest gaps 100.7 and 255.6 s).

So the re-phase is what keeps service balanced. Burst 2 plus browser timing jitter kept every mutant tab above zero grants. An in-phase simulation of three clients without re-phase gives the third zero grants (300/300 runs; see dev TASK-1057 / 1.x TASK-1188).

Why the harness counts: the claim is about the client logic against the limiter's decisions, and both are the real code. A bench run would add real network timing; it can be repeated on the OTGW32 once it is back on the network.

v2.js does not take part: it fetches otgw/otmonitor only every 20 s and only while its WebSocket is down (v2.js:4327), and does not poll device/time.

Still open: AC#16 (fresh boot with a wiped broker, hardware). It needs a broker that may be wiped; the test rig at 192.168.1.234:1883 was not reachable on 2026-10-01.

2026-10-01 maintainer decision for AC#16: use a throwaway Mosquitto broker on the development PC (C:\Program Files\mosquitto), point the OTGW32's MQTT settings at it for the test, then restore them. The Home Assistant broker is not touched. Runs once the OTGW32 is back on the network.
<!-- SECTION:NOTES:END -->
