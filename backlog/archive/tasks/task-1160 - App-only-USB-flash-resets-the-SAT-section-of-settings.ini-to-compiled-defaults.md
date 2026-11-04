---
id: TASK-1160
title: >-
  flash_otgw.bat --update --app silently flashes a LittleFS image too, wiping
  settings
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 18:15'
updated_date: '2026-09-23 21:38'
labels:
  - settings
  - flash
  - bug
dependencies: []
priority: high
ordinal: 292000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reproduced twice on the OTGW32 bench 2026-09-23: after flash_otgw.bat --update --app (alpha.372), every SAT key in /settings.ini is back at its compiled default (e.g. satsensormaxage 120 -> 21600, satenabled true -> false) while OTD and MQTT keys in the same file survive, and the file itself is rewritten. A plain RTS reset (esptool read_mac) does NOT reproduce it (marker survived). This is the sanctioned firmware update path on 4MB boards. Root cause unknown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause identified with on-device evidence that separates 'SAT keys not applied at parse' from 'applied, then reset and flushed'
- [x] #2 An app-only flash preserves every settings key, verified on the bench with a marker value
- [ ] #3 If testers are affected before the fix ships, the alpha channel gets a warning
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-23 investigation (bench OTGW32, 192.168.88.61). PREMISE CORRECTED: this is not 'the SAT section resets'. The only non-default keys on the bench were SAT test markers, and OTDmode=3 is re-derived at boot by thermostat auto-detect (OTDirect.ino ~957), so 'OTD survived' proved nothing. Stronger finding: after an app-only USB flash, RECENT LittleFS writes are rolled back to an older state, not just settings. Observed after the DIAG2 flash (22:1x): /otgw_simulation.log went back from the 4230 B coverage fixture uploaded at 21:29 to the older 12716 B log, and /bootparse.log (written by the previous diag boot) disappeared, while settings.ini, the fixture and the log were all present before the flash. Earlier: SAT markers (satsensormaxage 120/180) reverted to 21600 in 5 of 5 normal-build app flashes; a plain RTS reset (esptool read_mac) kept them. Two diag builds with extra LittleFS writes early in readSettings() did not reproduce (2/2), so timing/FS-state dependent. LittleFS is ~99% full (listfiles freeBytes 11674 after the 5% margin; 1561190 of 1572864). Side finding: static file serving of /settings.ini is unreliable (same file downloaded as 4172 B, 000, then 5752 B), so file snapshots over HTTP need a size check. Next: read the LittleFS region with esptool before and after an app write (device held in the bootloader) to separate 'flash tool touches the FS region' from 'firmware rolls back at boot'.

Decisive experiment 22:14-22:20: read the LittleFS region (0x270000, 1.5 MB) with the chip held in the ROM bootloader, wrote the app with esptool --before no_reset --after no_reset, read the region again: bit-identical. The flash tool does not touch the filesystem. After a hard reset from the bootloader the markers survived (satsensormaxage=240, otdsetbacktemp=14.5). So the loss is not caused by the write itself; it is tied to how the RUNNING firmware is reset by flash_otgw.bat (single esptool session: default_reset into the bootloader, write, hard_reset), and it is not deterministic. Working hypothesis, not yet proven: a reset that lands during a LittleFS metadata commit leaves the active metadata block invalid, and LittleFS falls back to the other block of the pair, which holds the state of the previous compaction (possibly tens of minutes old). This fits the loss of a fixture uploaded 26 min before the flash. A ~99% full filesystem makes compactions more frequent. Next steps: (1) identify periodic LittleFS writers during normal running; (2) try to reproduce without flashing (repeated RTS resets while writes are active); (3) read the FS region before and after a flash_otgw.bat run to capture the torn state; (4) check LittleFS free space and block_cycles on the combo partition.

22:25: flashing the clean alpha.376 with flash_otgw.bat --update --app kept both markers (satsensormaxage=240, otdsetbacktemp=14.5). Together with the direct-esptool run that is 2 clean runs after 5 failing ones earlier the same evening: not deterministic, consistent with the torn-commit hypothesis. Bench restored to defaults (SAT off, BLE on, riskack off, setback 16, maxage 21600, OTDmode 3 master).

ROOT CAUSE (proven 2026-09-23 23:4x): not firmware. flash_otgw.bat --update --app <bin> auto-picked a LittleFS image whenever --fs was not given (select_update_images: scriptdir, then build\OTGW-firmware-*.littlefs.bin, then .pio glob) and wrote it to 0x270000, replacing settings.ini, while printing 'NVS WiFi + settings kept'. It only bit after a FULL build (build\ then holds a littlefs image); --firmware builds clear build\, which is why the diag runs and the later runs were clean. Reproduced on demand: littlefs copied into build\, --update --app -> 'Filesys: ... -> 0x270000', marker 4242 -> 21600. Every earlier symptom fits (fixture back to the shipped 12716 B log, bootparse.log gone, OTDmode re-derived by auto-detect). Fix (host tooling, no firmware change): explicit --app/--fs means exactly those images, auto-pick only when neither is given; the summary and done messages state that a filesystem write resets settings; header and help corrected. Verified: same setup, --update --app writes only 0x10000, marker 7777 kept; bare --update still writes app + fs and now warns. Also found while fixing: sed -i had stripped the CRLF endings of the .bat, which broke cmd parsing; restored to CRLF.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The settings loss after a USB update was the flash tool, not the firmware: flash_otgw.bat --update --app also wrote any LittleFS image it found in build\, which replaces settings.ini, while claiming settings were kept. An explicit --app or --fs now writes exactly those images, and every filesystem write says it resets the settings. Reproduced and verified on the OTGW32 bench.
<!-- SECTION:FINAL_SUMMARY:END -->
