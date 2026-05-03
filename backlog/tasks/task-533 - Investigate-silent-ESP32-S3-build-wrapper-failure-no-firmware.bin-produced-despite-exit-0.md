---
id: TASK-533
title: >-
  Investigate silent ESP32-S3 build wrapper failure (no firmware.bin produced
  despite exit 0)
status: To Do
assignee: []
created_date: '2026-05-03 19:53'
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
- [ ] #1 Reproduceer het issue op een schone werkboom om vast te stellen of het een runtime-omgeving probleem is of code-gerelateerd
- [ ] #2 Bepaal of pio run -e esp32 daadwerkelijk faalt of dat het succesvol completeert maar de wrapper-output gaat verloren door buffering / pipe-handling
- [ ] #3 Run pio direct (niet via build.py) om de werkelijke ESP32-S3 build-staat te zien — firmware.bin, firmware.elf, partitions.bin moeten alle drie verschijnen bij succes
- [ ] #4 Identificeer de hoofdoorzaak: wrapper buffering, wrapper exit-code-misinterpretatie, of toolchain failure
- [ ] #5 Fix de wrapper zodat (a) pio's volledige output naar de console gaat (line-buffered of streamed), en (b) een non-zero exit code van pio het wrapper-script ook non-zero maakt
- [ ] #6 Verifieer: na de fix, een ESP32-S3 build produceert `.pio/build/esp32/firmware.bin` en `.elf` en logt de PlatformIO compile/link steps zichtbaar
- [ ] #7 Geen regressie op ESP8266: `build.py --firmware` levert nog steeds correcte ESP8266 artefacten en distribution zip
- [ ] #8 Documenteer in een commit-message of een korte ADR-amendment indien er een onderliggende toolchain configuratie-keuze gewijzigd moet worden
<!-- AC:END -->
