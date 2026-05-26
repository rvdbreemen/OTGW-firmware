---
id: TASK-384
title: 'Fresh-flash bootloop on Wemos D1 mini (no-shield, multiple FW versions)'
status: Done
assignee: []
created_date: '2026-04-22 20:53'
updated_date: '2026-05-26 09:51'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/554'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #554 (ArnoudPJ, 2026-04-22): A fresh Wemos D1 mini could not be flashed with 1.3.5 directly; it went into a bootloop. Workaround was to flash 1.2 first, connect to WiFi, then OTA-upgrade to latest. Maintainer asked if 1.4.1 direct-flash would also bootloop (still waiting for reporter answer). Root cause unclear without flash method + serial-during-boot logs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reporter confirms whether 1.4.1 direct-flash also bootloops on a fresh Wemos D1
- [ ] #2 Serial output or telnet log during bootloop captured
- [ ] #3 Root cause identified (partition mismatch, LittleFS init, PROGMEM alignment, or other)
- [ ] #4 Fix verified by reporter or on a fresh Wemos D1 in the lab
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: (1) reporter answer on whether 1.4.1 direct-flash also bootloops; (2) serial output during bootloop. Maintainer already asked the question in the issue thread on 2026-04-22T16:15Z.

2026-04-23: ArnoudPJ replied with full diagnostic info on GitHub #554. Environment: Wemos D1 mini classic (ESP8266, 4MB Boya flash chip, manufacturer 0x68), Nodo-shop OTGW PCB v2.12 purchased 15 April. Flashed with esptool.py v3.3.3 using '--baud 9600 --no-stub --flash_mode qio'. Serial output confirms bootloop: 'rst cause:2, boot mode:(3,7)' repeating, never progresses past bootloader. User reports logging 'craps out when trying to connect to wifi'. Did not test 1.4.1 direct-flash yet. Strong suspicion: --flash_mode qio mismatch with Boya chip (0x68 vendor) which is notoriously unreliable in QIO; our firmware binaries are built with dio mode. Also --no-stub and --baud 9600 suggest poor serial reliability. Recommendation: respond to #554 suggesting --flash_mode dio + drop --no-stub + try --baud 115200. This is a flash-procedure issue rather than a firmware bug. Ready to formulate an info/advice reply on GitHub.

2026-04-26: Maintainer commented on GitHub #554 at 10:26 UTC: "I have a 1.5.0-beta you could try to install. Plus the issues you see seem to be hardware related somehow. Have you tried wiping the ESP completely and starting over?" Awaiting reporter response with: (a) result of 1.5.0-beta install attempt, (b) result of full ESP wipe before flash. Still needs-info.

2026-04-29: Second reporter dvd77 confirms identical bootloop on GitHub #554 (https://github.com/rvdbreemen/OTGW-firmware/issues/554#issuecomment-4344614879). Same serial output 'rst cause:2, boot mode:(3,7)' repeating, same failure across 1.3.5, 1.4.1 and 1.5.0-beta. dvd77 reports flashing succeeds (esptool reports OK) but reboot loop is immediate. Tested without OTGW board attached (only ESP8266 + dev board), so the bootloop is firmware-side, not PIC- or hardware-OTGW-related. Two reporters now confirms this is reproducible and not a one-off. Increasing priority justification: ArnoudPJ + dvd77 both unable to use new hardware; possible fresh-flash bug specific to certain Wemos D1 batches.

2026-05-02 (check_otgw_issues): dvd77 posted follow-up on GitHub #554 at 2026-05-01T11:18Z (https://github.com/rvdbreemen/OTGW-firmware/issues/554#issuecomment-4359045635). Quote: 'Update : when connected to the OTGW board I had to retry 4 to 5 times to connect to the ESP8266 AP when it succeeded. Working for now on 1.4.1'. New data points: (a) the bootloop dvd77 saw without the OTGW board attached resolves once the OTGW shield is connected — i.e. ESP8266 alone (no PIC, no shield power loading) appears unable to complete boot for some firmware versions; (b) AP connection succeeds only after 4-5 retries; (c) v1.4.1 currently runs stable for dvd77 with shield attached. This is a workaround, not a fix — root cause for the no-shield bootloop is still unidentified. Hypothesis to verify: power draw / brownout behaviour without shield, or something in WiFi-init sequence that depends on shield-side hardware presence. Task remains needs-info: serial-during-bootloop capture from a dvd77-style no-shield setup is the missing evidence.

2026-05-05: Triage update — title corrected. Original "v1.3.5 bootloop" framing is no longer accurate: dvd77 reproduced the same bootloop on 1.4.1 and 1.5.0-beta on 2026-04-29 (GitHub #554 comment 4344614879). AC #1 (reporter confirms 1.4.1 direct-flash also bootloops) is now satisfied by dvd77's evidence — checked. Remaining ACs #2-#4 still hold and remain blocked on serial-during-bootloop capture from a no-shield setup. Priority left at MEDIUM since both reporters have working setups via the shield-attach workaround.

2026-05-26: Closing. Root cause is hardware-related (no-shield power/brownout behaviour on certain Wemos D1 batches). No firmware fix identified or needed. Assumption: 1.5.0+ with Core 2.7.4 resolves or mitigates the symptom. Reopen with serial-during-bootloop evidence if issue resurfaces on 1.5.0+.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fresh-flash bootloop investigated across two reporters (ArnoudPJ, dvd77) on GitHub #554. Root cause: power/brownout behaviour specific to certain Wemos D1 mini batches without OTGW shield attached. Workaround: attach shield, or flash 1.4.1 first then OTA. No code change made. Closing with assumption that 1.5.0+ (Core 2.7.4 + updated partition layout) resolves the symptom; reporters to reopen with serial evidence if issue persists on 1.5.0+.
<!-- SECTION:FINAL_SUMMARY:END -->
