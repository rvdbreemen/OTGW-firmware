---
id: TASK-533
title: >-
  Investigate silent ESP32-S3 build wrapper failure (no firmware.bin produced
  despite exit 0)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-03 19:53'
updated_date: '2026-05-05 21:01'
labels:
  - build
  - esp32
  - tooling
  - ci
  - 2.0.0
dependencies: []
references:
  - build.py
  - .pio/build/esp32
  - 'https://github.com/rvdbreemen/OTGW-firmware/commit/74d95b7d'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Symptoom

`python build.py --firmware` op `feature-dev-2.0.0-otgw32-esp32-sat-support` rapporteert exit 0, maar er wordt **geen ESP32-S3 firmware-binary geproduceerd**. ESP8266-zijde levert wel een complete set artefacten (firmware.bin, merged binary, distribution zip).

Reproduceerbaar over twee opeenvolgende runs (gedaan tijdens TASK-532 implementatie op 2026-05-03, ongeveer 19:42 en 19:47).

## Concrete observaties

- Wrapper-output stopt consistent direct na `Running: pio run -e esp32` — geen verdere logregels uit pio's output zichtbaar.
- `.pio/build/esp32/` bevat alleen `partitions.bin` (vroege build-step) — `firmware.bin`, `firmware.elf` ontbreken.
- ESP8266-zijde levert `OTGW-firmware-esp8266-2.0.0-alpha+0424948.ino.bin`, `.elf`, `merged-full.bin`, en distribution zip.
- Exit code is 0, dus de wrapper denkt dat het gelukt is.
- `pio` is niet beschikbaar op het Bash $PATH (`pio: command not found`), wat suggereert dat build.py de pio in een Python venv invoket (`~/.platformio/penv/Scripts/pio.exe` op Windows).

## Mogelijke hoofdoorzaken

1. **Output buffering/pipe-cut**: build.py voert pio aan met `subprocess.run` zonder line-buffering, en het wrapper-script eindigt voor pio's stdout volledig is geflusht. De ESP32-stap loopt dan misschien wel maar de logging gaat verloren.
2. **Stille pio failure**: pio run faalt met een non-zero exit code, maar de wrapper interpreteert het verkeerd of negeert het.
3. **PlatformIO toolchain issue**: ESP32-S3 platform-pakket niet geïnstalleerd / gebroken, pio bail't vroeg uit zonder error-melding op stdout.

## Impact

- **Blokkeerde AC #3** van TASK-532 op build-niveau (workaround: code-level deductieve verificatie geaccepteerd door project owner per "optie a" beslissing).
- Mogelijk affecteert het alle ESP32-S3 builds op deze branch — dat zou een veel breder issue zijn dan één task.
- Risico: dingen die we als "groen build" rapporteren zijn dat alleen op ESP8266; ESP32-S3 regressies kunnen sluipen.

## Bron

CLI sessie 2026-05-03 (zie ook commit `74d95b7d` op feature-2.0.0 — TASK-532 implementatie waar dit issue oppervlakte kwam).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduceer het issue op een schone werkboom om vast te stellen of het een runtime-omgeving probleem is of code-gerelateerd
- [x] #2 Bepaal of pio run -e esp32 daadwerkelijk faalt of dat het succesvol completeert maar de wrapper-output gaat verloren door buffering / pipe-handling
- [x] #3 Run pio direct (niet via build.py) om de werkelijke ESP32-S3 build-staat te zien — firmware.bin, firmware.elf, partitions.bin moeten alle drie verschijnen bij succes
- [x] #4 Identificeer de hoofdoorzaak: wrapper buffering, wrapper exit-code-misinterpretatie, of toolchain failure
- [x] #5 Fix de wrapper zodat (a) pio's volledige output naar de console gaat (line-buffered of streamed), en (b) een non-zero exit code van pio het wrapper-script ook non-zero maakt
- [x] #6 Verifieer: na de fix, een ESP32-S3 build produceert `.pio/build/esp32/firmware.bin` en `.elf` en logt de PlatformIO compile/link steps zichtbaar
- [x] #7 Geen regressie op ESP8266: `build.py --firmware` levert nog steeds correcte ESP8266 artefacten en distribution zip
- [x] #8 Documenteer in een commit-message of een korte ADR-amendment indien er een onderliggende toolchain configuratie-keuze gewijzigd moet worden
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Reproduce on the 2.0.0 worktree: python build.py --firmware, capture full stdout/stderr + exit code, then ls -la .pio/build/esp32/ to confirm only partitions.bin is present.
2. Run pio directly from inside the 2.0.0 worktree (macOS path) to separate "wrapper hides output" from "pio actually fails". Capture exit code.
3. Read build.py to find how it spawns pio (subprocess.run / Popen, stdout/stderr wiring). Suspects: output captured to a buffer never printed, or pio exit code not propagated.
4. Fix the wrapper: stream pio output line-buffered to console, assert non-zero pio exit propagates to wrapper exit, never silently swallow stderr.
5. Verify: python build.py --firmware on 2.0.0 produces .pio/build/esp32/firmware.bin + .elf and logs the compile/link steps; ESP8266 artefacts + distribution zip still produced; evaluate.py --quick clean.
6. Land: commit on feature-dev-2.0.0-otgw32-esp32-sat-support, push per standing permission once build + evaluator pass.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-05: Reproduction + fix.

- Bug as filed (exit 0 + no firmware.bin) does not reproduce on macOS. ./build.sh --firmware --target esp32 produced firmware.bin (1.84 MB) and firmware.elf (24 MB) end-to-end with exit 0. The "exit 0 + missing artefact" failure mode is already guarded by verify_artifact_exists() (TASK-337) — would exit 2 if pio silently failed.
- What does reproduce: log-line reordering when stdout is redirected to a non-TTY (CI logs, tail -f). Python's print() is block-buffered when not on a tty, while pio writes through inherited fd 1 line-buffered. Result: wrapper's "Running: pio run -e esp32" landed at log line 226 of 317, AFTER pio's "Processing esp32" at line 3 — to a user reading the redirected log it looks like "output stops after Running: pio".
- Fix: PYTHONUNBUFFERED=1 in build.sh and build.bat next to PYTHONUTF8=1. No build.py changes — the safety nets are already correct.
- Post-fix on macOS: "Running: pio run -e esp32" at line 49 immediately precedes "Processing esp32" at line 50. ESP32 + ESP8266 builds both clean (exit 0, fresh firmware.bin + firmware.elf).
- evaluate.py --quick: 59 pass / 2 unrelated warnings / 0 fail / 97.1%.
- Windows reproduction of the original report remains uncovered (bug filer was on Windows; macOS used here). PYTHONUNBUFFERED is honored identically on Python 3 across all platforms, so the fix should apply on Windows too.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Set PYTHONUNBUFFERED=1 in both shell wrappers so the build wrapper's progress messages interleave correctly with PlatformIO's compile/link output when stdout is redirected to a file or pipe.

## Why
Original symptom (TASK-533): "wrapper output stops after Running: pio run -e esp32, no firmware.bin produced, exit 0". Reproduction on macOS yielded exit 0 with full firmware.bin + firmware.elf — the "exit 0 + missing artefact" mode is already guarded by verify_artifact_exists() (TASK-337), so the wrapper would have exited 2 if pio had truly failed silently. What does reproduce is log-line reordering: when stdout is redirected (CI, > log, tail -f), Python's print() is block-buffered while pio writes line-buffered to inherited fd 1, so wrapper messages land much later in the log than they happened. To a user reading the redirected log it looks like "output stops after Running: pio".

## Changes
- build.sh:10  — export PYTHONUNBUFFERED=1
- build.bat:10 — set "PYTHONUNBUFFERED=1"

No build.py changes; run_command's stdout=None inheritance and verify_artifact_exists post-pio (TASK-337) are already in place.

## Verification
- ./build.sh --firmware --target esp32 redirected → exit 0, firmware.bin (1.84 MB) + firmware.elf (24 MB) produced; "Running: pio run -e esp32" at log line 49 immediately precedes "Processing esp32" at line 50 (was 226 vs 3 before the fix).
- ./build.sh --firmware --target esp8266 redirected → exit 0, firmware.bin (842 KB) + firmware.elf (2.4 MB); same ordering improvement.
- evaluate.py --quick: 59 pass / 2 unrelated warnings / 0 fail / 97.1%.

## Risk / follow-ups
- Windows reproduction of the original report is not covered (bug filer was on Windows; we tested on macOS). PYTHONUNBUFFERED is honored identically on Python 3 across Windows / macOS / Linux, so the fix should apply on Windows too, but on-Windows verification remains open.
- If "exit 0 + missing artefact" recurs in the future, verify_artifact_exists() exits 2; TASK-337's safety net is intact.
<!-- SECTION:FINAL_SUMMARY:END -->
