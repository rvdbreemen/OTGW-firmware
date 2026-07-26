---
id: TASK-1037
title: >-
  feat-2.0.0: port 1.7.2-beta.4 hardening (discovery heal, REST rate limit, UI
  pacing)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-26 22:06'
updated_date: '2026-07-26 22:29'
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
- [ ] #10 On 429 the client re-phases at a random offset inside its period so two dashboards cannot phase-lock; verified with two tabs for 10 minutes
- [x] #11 After >=3 consecutive refusals the affected UI region is marked data-stale with an explanatory title; the selector exists in components.css so check_design_system_drift passes
- [x] #12 ADR-170..173 written (Proposed); ADR-062 gets a supersession note for its automatic mechanism only
- [x] #13 evaluate.py gains gates for alias-budget coverage, poll/window coupling, non-OT single source, and auto-heal shape, each as a module-level fn with tests in tests/test_evaluate.py
- [x] #14 openapi.yaml documents the 429 on /v2/device/time, /v2/otgw/otmonitor and /v2/otgw/telegraf, and states in prose that the two otgw paths share one budget
- [ ] #15 ./build.sh green for esp32 target, python evaluate.py exit 0, python tests/test_evaluate.py green
- [ ] #16 Hardware: fresh boot with wiped broker announces SAT (252-255), diag (251), OTDirect (243) and S0 (245) discovery without a manual republish
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
<!-- SECTION:NOTES:END -->
