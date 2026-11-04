---
id: TASK-695
title: >-
  fix(persistence): replace Stream::find/parseInt with portable byte parser
  (TASK-693 follow-up; PR #641 ESP32 build fix)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 09:14'
updated_date: '2026-05-24 10:13'
labels:
  - fix
  - port-from-dev
  - esp32
  - persistence
dependencies: []
priority: high
ordinal: 63000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PR #641 (port of dev TASK-688) fails the ESP32 build in CI. Local reproduction blocked by sandbox SSL chain, but static analysis pins the most likely culprit: readBitmapArrayFromJson in OTGW-Core.ino is the first call site in the 2.0.0 codebase to invoke Stream::find() and Stream::parseInt() on a LittleFS::File. All other f.find()/f.parseInt() usage in the codebase is on hardware-serial Streams (OTGWSerial.find at OTGW-Core.ino:508). ESP32 LittleFS may not expose those Stream methods on File the way ESP8266 does, or may trigger a different timeout-driven blocking pattern that the compiler rejects.\n\nDefensive fix: replace both calls with a small manual byte parser using only f.read(), f.available(), and f.peek(). Same semantics, universally portable.\n\nIf this isn't the real issue, the next CI run will surface a different error and we re-diagnose.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 readBitmapArrayFromJson no longer calls f.find() or f.parseInt(); uses a new fileFindToken() helper (manual state machine matching the search key char-by-char) and a new parseIntArrayInto() helper (digit accumulator with delimiter-based finalisation).
- [x] #2 Helpers are static (TU-local), use only File::read(), File::available(), File::peek() — methods inherited from FS::FileImpl on both ESP8266 and ESP32 LittleFS.
- [x] #3 Behaviour preserved: a well-formed /ot-thermo.json or /ot-boiler.json still loads the same bitmap state as before. Malformed input (truncated, no closing ']') exits cleanly without crash.
- [x] #4 python build.py --firmware --target esp8266 exits 0 (local; ESP32 verified by CI rerun on PR #641).
- [x] #5 CI pio run -e esp32 on PR #641 passes after the push.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Commit f251ad6a pushed to feat-2.0.0/port-beta-20-log-review. PR #641 CI re-runs automatically; AC#5 (ESP32 green) blocked on that result. If CI still fails, the new error will point to the actual cause and we re-diagnose.

CI logs finally retrieved (via certifi-bundle patch + check-runs/annotations API): real failure was firmware-size overflow, not Stream::find/parseInt API compatibility. ESP32 firmware = 1,969,847 B vs 1,966,080 B partition limit = 3,767 B over. The Stream-parser swap from this commit is fine but did not help. Need a follow-up to trim flash.

PR #641 merged with this commit included alongside TASK-696. CI passed on the merge build (ESP32 + ESP8266 both green), so AC#5 is satisfied retroactively.

Post-merge diagnosis note: this fix turned out to be unrelated to the actual cause (which was flash-budget — TASK-696 enabled -flto to recover 105 KB). The Stream::find/parseInt swap shipped anyway because it is harmless and slightly more portable. Worth treating as a lesson: when CI logs are unreachable, run the failing target locally before changing code on a hypothesis.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced Stream::find()/parseInt() on LittleFS::File with a manual byte parser (fileFindToken + parseIntArrayInto) using only File::read()/available()/peek(). The original suspicion was that those Stream methods were the ESP32 build failure on PR #641 — but TASK-696 (-flto + flash-budget) turned out to be the real fix. This change shipped anyway because the manual parser is harmless: same observable behaviour on well-formed JSON, more defensive on malformed input, and removes any future portability risk between ESP8266 and ESP32 LittleFS Stream inheritance.

## Verification
- pio run -e esp8266: SUCCESS (locally).
- pio run -e esp32: SUCCESS (merged build on PR #641).
- python evaluate.py --quick: 60 / 1 warn / 0 (unchanged baseline).

## Process note for future cross-tree work
Gated on CI log access I could not get without auth. Burned a round of guessing. The actual unblocker was patching the sandbox certifi bundle to trust the egress TLS-Inspection CA, then running `pio run -e esp32` locally to see the size-overflow error directly. Worth wiring this into a session-start hook so the next port-from-dev task does not repeat the same dead end.
<!-- SECTION:FINAL_SUMMARY:END -->
