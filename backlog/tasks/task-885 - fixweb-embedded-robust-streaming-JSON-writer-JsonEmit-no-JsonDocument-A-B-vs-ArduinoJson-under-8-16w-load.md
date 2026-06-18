---
id: TASK-885
title: >-
  fix(web): embedded-robust streaming JSON writer (JsonEmit, no JsonDocument) +
  A/B vs ArduinoJson under 8-16w load
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-18 18:22'
updated_date: '2026-06-18 19:37'
labels: []
dependencies: []
ordinal: 101000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Senior-embedded experiment requested by maintainer (2026-06-18): the pre-migration hand-rolled JSON (no JsonDocument, non-throwing, medium-chunk direct writes into the network buffer) SURVIVED a 16-worker/2min flood with 0 reboots, where the ArduinoJson migration rebooted at 8w until the gate+backstop. Build a header-only non-throwing streaming JSON writer (JsonEmit, src/OTGW-firmware/jsonEmit.h) that emits live firmware state directly into AsyncResponseStream (single-pass, no doc; chunked-re-run rejected because a no-doc producer re-reading volatile fields shifts byte width between passes -> corruption). Convert the heavy endpoints (device/info, sat/status, settings, debug, the 3 0..255 arrays). Then run an A/B (streaming vs ArduinoJson, gate ON/OFF, 8/12/16 workers) to decide on HARD NUMBERS whether to keep ArduinoJson (ADR-141), go hybrid (streaming heavy-only), or fully revert. Decision spine = the B-raw panic decode (heap/bad_alloc -> approach on trial; bare TWDT/core-1 -> TASK-879, approach-exonerated) + A/B maxblock-floor headroom. Full plan: docs/plans/PLAN-streaming-json-jsonEmit.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 JsonEmit header: non-throwing, no-alloc, type-correct (real bools, native floats NaN->null, escaped strings matching the old \u escaping with unsigned-char fix); host/device unit checks pass (escaping, structure balance, depth-overflow, float 3dp/Dallas 1dp)
- [ ] #2 Heavy endpoints converted; json_golden 0-FAIL + explicit float/bool byte-checks (golden's strip_volatile is blind to float-int and string-bool flips)
- [ ] #3 A/B run: A-raw vs B-raw at 8/12/16w with panic decode + maxblock floor; A-raw survives 16w gate-OFF where B-raw reboots (or recorded if not)
- [ ] #4 Decision recorded on hard numbers (keep ArduinoJson / hybrid / full revert) in a superseding ADR if ADR-141 is reversed
- [ ] #5 Shipped build survives 8-16w flood (0 reboots) AND improves real-200 throughput vs alpha.214 under load; evaluate.py green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
JsonEmit BUILT + VALIDATED. Header (src/OTGW-firmware/jsonEmit.h): non-throwing, no-alloc, type-correct (real bools, NaN/Inf->null, escaped strings + unsigned-char fix, F()/const-char* keys+values, narrow-int overloads guarded by PLATFORM_INT_DISTINCT_FROM_INT32 from platform.h). 5 heavy handlers converted (device/info, settings, debug, otmonitor in restAPI.ino; sat/status+ble in SATcontrol/SATble) via 2 parallel sub-agents. Compiles clean (binary ~8KB SMALLER than ArduinoJson). json_golden: 27 PASS, 4 FAIL all ENVIRONMENTAL (stale-golden build version, volatile heap, filesystem file-count, live-state flap) - ZERO converter bugs; settings BYTE-IDENTICAL. Byte-checks PASS: bools real true/false (no quoted-bools), floats have decimals + precision (ki_manual 0.000500 after fixing PID gains to 6 decimals; default 3dp lost sub-0.001 precision).
A/B HARD NUMBERS (heap axis = the discriminator): B-raw (ArduinoJson, gate-off, alpha.213) @8w = REBOOT on bad_alloc, heap fragmented, maxblock CRATERED <12KB, ADR-089 tier breaches. A-raw (streaming, gate-off) @8w = heap PRISTINE (floor 92KB, maxblock 31KB, 0 tier entries, no fragmentation collapse) - the OOM/bad_alloc failure CLASS is GONE; it rebooted once but NON-heap (no panic text, no wdt label, reset 'Unknown' = lost-USB watchdog or brownout under WiFi flood, approach-independent + hardware). A-ship (streaming+gate) @16w = 0-1 stochastic reboot (watchdog = TASK-879 core-1 starvation, approach-independent), heap floor 48-91KB, 0 tiers. B-ship (ArduinoJson+gate, alpha.214) @16w = 0 reboots, heap floor ~86KB.
CONCLUSION (per plan decision criteria): streaming MATERIALLY more robust on the HEAP axis (removes the bad_alloc/fragmentation failure class; maxblock stays high where B craters) - confirms the maintainer's intuition that the pre-migration no-doc approach handled heap pressure far better. The residual reboots under extreme 8-16w flood are NON-heap (watchdog/TASK-879/brownout), approach-INDEPENDENT, and need the orthogonal fix (gate + core-affinity/yield), NOT a JSON change. Decision pending maintainer review: hybrid (streaming heavy + ArduinoJson light, the built state) vs full ADR-141 revert vs keep. platformio.ini gate-off A/B flag removed.

CORRECTIONS (advisor, before presenting): (1) The A-raw @8w 'reset Unknown' reboot: serial capture was EMPTY (no panic/wdt/backtrace text). Cause NOT established. Heap was pristine at the 3s-sampled points so a heap/bad_alloc cause is unlikely-but-not-excluded; do NOT assert 'non-heap/brownout/approach-independent' - that is unverified inference. (2) The A/B is NOT a faithful controlled experiment: B-raw = alpha.213 (a DIFFERENT commit + the chunked variant, not a same-HEAD ArduinoJson+gate-off+backstop-off build). The cross-commit comparison is SUGGESTIVE, not controlled. The heap-axis difference is structural (pooled JsonDocument vs no doc) and proven per-request, but the reboot comparison is not controlled. (3) AXES: streaming wins the HEAP axis (maxblock ~31KB vs <12KB cratered; 0 tier entries; settings byte-identical; ~8KB smaller binary). It does NOT win the REBOOT axis: on the gate-ON shippable, A-ship rebooted once @16w (watchdog) where B-ship (alpha.214) rebooted zero. 'Robust under 8-16w' was ALREADY met by alpha.214; streaming adds heap HEADROOM, not reboot robustness, and does not fix the watchdog. Possible small real cost: streaming's faster per-request path may raise sustained core-1 occupancy -> marginally more loopTask starvation (unconfirmed). HONEST VERDICT: streaming is a genuine improvement on heap pressure + simplicity + removing the bad_alloc/OOM failure class (plausibly helps long-uptime field devices + the TASK-879 web-dead under MQTT+WS heap pressure). It is NOT a forced revert. Maintainer picks: hybrid / full-revert / keep.
<!-- SECTION:NOTES:END -->
