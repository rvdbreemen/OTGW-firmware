---
id: TASK-1056
title: >-
  Remove dead code found by the post-ADR-167 sweep, keeping capability-flag
  extension points
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-31 22:26'
updated_date: '2026-07-31 23:07'
labels: []
dependencies: []
ordinal: 251000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
A 5-lens adversarially-verified sweep over src/ produced 43 findings (36 unique). Triaged into what is genuinely dead versus what only LOOKS dead.

DELETE - ADR-167 residue: HEAP_LOW_RESTORE_THRESHOLD (OTGW-firmware.h:200) and its only reader, the two #else arms of HAS_FRAGMENTATION_AWARE_HEAP_GATE in MQTTstuff.ino (discoveryDripHasHeapPressure ~2114, discoveryDripIsHeapHealthyForRestore ~2131). That flag is an ESP8266-vs-ESP32 discriminator, not a per-board capability, so on an ESP32-S3-only branch it is 1 in every environment and its off-arm can never compile. Removing both readers drops it to zero readers, so retire the flag from boards.h in the same commit or it becomes a new dead symbol.

DELETE - unreferenced C symbols (definition-only, verified repo-wide not just src/): strHTTPmethod, otCmdDequeue, satSaveCycleWindow, satLoadCycleWindow, satHandleZoneMode, print_flag8, isOTOverrideActive (+ its declaration at OTGW-Core.h:805), satBLEGetTemperature, satBLEGetHumidity, publishMQTTNumeric (both overloads), oledWake.

DELETE - unreferenced JS in data/: setBackGround, getBackGround, stopPersistenceTimer, setG, loadUISettings, safeGetElementById, setHTML, addClass, removeClass, fileWritableStream. Each verified against index.html/v2.html for inline onclick wiring, not just against the .js files.

KEEP DELIBERATELY - capability-flag off-branches. HAS_BYPASS_RELAY / PIN_BYPASS_RELAY (OTDirect.ino:783, boards.h:85), the #else of HAS_LEDC_LED (OTGW-firmware.ino:625), HAS_LLMNR (networkStuff.ino:641, platform_esp32.h:78), MDNS_NEEDS_UPDATE (OTGW-firmware.ino:903, platform_esp32.h:79) and the !HAS_WEATHER_FORECAST arms (SATweather.ino:63, :396) are all unreachable on today's boards, but the off-branch of a HAS_* flag IS the abstraction's extension point per the CLAUDE.md platform rule. Deleting it means the next board variant that sets the flag to 0 gets a compile error instead of a fallback. Recorded here so the next sweep does not re-find them and delete them.

KEEP - false positives the src/-only sweep produced: bleMatchesConfiguredMAC is exercised by tests/test_ble_parsers.cpp, and OTValueType is documented in docs/MANUAL.md, docs/c4/c4-code-otgw-core.md and docs/manuals/en/ch08-developer-guide.md. Both looked unreferenced only because the sweep scoped to src/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-167 residue removed: HEAP_LOW_RESTORE_THRESHOLD, both HAS_FRAGMENTATION_AWARE_HEAP_GATE #else arms, and the flag itself retired from boards.h in the same commit
- [x] #2 All 12 unreferenced C symbols removed together with any orphaned declarations
- [x] #3 All 10 unreferenced JS symbols removed from data/
- [x] #4 bleMatchesConfiguredMAC, OTValueType and every capability-flag off-branch are left in place, with the reason recorded in the task notes
- [x] #5 build.bat with no --target: all three envs green for BOTH firmware and filesystem (6 SUCCESS lines), because a branch dead under one board may be live under another
- [x] #6 python evaluate.py shows no new failures and python tests/test_evaluate.py still passes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed in two commits so testers get a clean A/B signal.

Commit 1 (ADR-167 residue): HEAP_LOW_RESTORE_THRESHOLD plus its 12-line rationale block, both HAS_FRAGMENTATION_AWARE_HEAP_GATE #else arms in MQTTstuff.ino, and the flag itself retired from both board branches of boards.h so it did not become a fresh dead symbol.

Commit 2 (unreferenced symbols): 13 C definitions + 6 orphaned declarations + 10 JS symbols, 265 lines of C and 65 of JS.

FOUND BEYOND THE SWEEP: print_flag8flag8. The sweep reported only print_flag8; counting the whole print_* family at once showed both have zero callers while every other member has at least one. Family-level counting catches what symbol-level auditing misses.

CORRECTED THE SWEEP: bleMatchesConfiguredMAC and OTValueType were both reported dead and are both alive. The workflow scoped its greps to src/, so it could not see tests/test_ble_parsers.cpp or the three docs files. Kept, and recorded here.

METHOD NOTE for whoever runs the next sweep: the workflow returned 43 candidates and 43 survivals, i.e. a zero refutation rate against verifiers explicitly told to default to not-dead. That is a signal the verifiers agreed with the finders rather than testing them independently. Hand-checking a sample found the two false positives above. Do not trust a 100% survival rate.

Also fixed a comment in restAPI.ino:103 that pointed at strHTTPmethod, which this commit deletes.

Verification: all three targets (esp32, esp32-classic, esp32-combo) SUCCESS for firmware AND filesystem, built serially from wiped .pio/build dirs; node --check clean on index.js/v2.js/sat.js; evaluate.py 94 checks 0 failures; tests/test_evaluate.py 61 tests OK.
<!-- SECTION:NOTES:END -->
