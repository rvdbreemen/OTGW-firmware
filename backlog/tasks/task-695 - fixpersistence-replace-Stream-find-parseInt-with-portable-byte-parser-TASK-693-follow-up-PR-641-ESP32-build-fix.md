---
id: TASK-695
title: >-
  fix(persistence): replace Stream::find/parseInt with portable byte parser
  (TASK-693 follow-up; PR #641 ESP32 build fix)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 09:14'
updated_date: '2026-05-24 09:15'
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
- [ ] #1 readBitmapArrayFromJson no longer calls f.find() or f.parseInt(); uses a new fileFindToken() helper (manual state machine matching the search key char-by-char) and a new parseIntArrayInto() helper (digit accumulator with delimiter-based finalisation).
- [ ] #2 Helpers are static (TU-local), use only File::read(), File::available(), File::peek() — methods inherited from FS::FileImpl on both ESP8266 and ESP32 LittleFS.
- [ ] #3 Behaviour preserved: a well-formed /ot-thermo.json or /ot-boiler.json still loads the same bitmap state as before. Malformed input (truncated, no closing ']') exits cleanly without crash.
- [ ] #4 python build.py --firmware --target esp8266 exits 0 (local; ESP32 verified by CI rerun on PR #641).
- [ ] #5 CI pio run -e esp32 on PR #641 passes after the push.
<!-- AC:END -->
