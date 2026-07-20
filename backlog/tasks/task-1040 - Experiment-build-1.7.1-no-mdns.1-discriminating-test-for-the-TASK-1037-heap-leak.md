---
id: TASK-1040
title: >-
  Experiment build 1.7.1-no-mdns.1: discriminating test for the TASK-1037 heap
  leak
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 15:23'
updated_date: '2026-07-20 19:52'
labels: []
dependencies: []
priority: high
ordinal: 159000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Purpose: settle whether mDNS is the source of the heap leak in TASK-1037, or only the site where the resulting OOM crashes. Waiting for more field captures does not answer this, and the capture script drives a headless browser at 365 REST requests/min, so it measures the fragmentation path rather than the leak.

The experiment: one build, identical to v1.7.1 in every respect except that mDNS is compiled out. Give it to martreides as an alternative to the 1.7.1 he runs now, and read the heap trend.

Both outcomes are informative, which is what makes it worth building:
- Heap stays flat over several hours: mDNS is both leak and crash site. TASK-1037 gets a root cause.
- Heap still decays but no more Exception (2) reboots: mDNS is only the crash site (the ungated unchecked new in _readRRAnswer). We keep a device that survives long enough to instrument, and the leak hunt continues elsewhere.

Critical constraint: base the build on the v1.7.1 tag, NOT on the current 1.7.2-beta.1 dev tip. The single-variable property is the entire value of the experiment; any other delta confounds the result.

Mechanics:
- Compile-time flag (for example OTGW_DISABLE_MDNS) guarding MDNS.begin() and MDNS.addService() in networkStuff.ino:398-406 and the three MDNS.update() call sites (OTGW-firmware.ino:379, OTGW-firmware.ino:427, OTGW-Core.ino:553-554). A build switch, not a code fork.
- Version tag no-mdns.1, giving 1.7.1-no-mdns.1. Valid semver (hyphens are allowed in prerelease identifiers) and self-labelling in the telnet banner, the web UI and the MQTT version topic, so no capture can later be mistaken for a normal 1.7.1 run.
- Set the tag with scripts/autoinc-semver.py --prerelease no-mdns.1. Do NOT use bin/bump-prerelease.sh: its regex is ^[a-zA-Z]+\.[0-9]+$ and rejects the hyphen.
- Experiment branch, never merged to dev, never released. No GitHub release, no Discord beta announcement.

Known side effects to communicate to the tester:
- otgw.local stops resolving. He must use the IP address (192.168.1.150 at last report) for the web UI, and check whether anything in his Home Assistant setup depends on the hostname.
- Because 1.7.1-no-mdns.1 sorts BEFORE 1.7.1 in semver, the firmware update check may offer 1.7.1 as an upgrade and silently end the experiment if he accepts. Verify this behaviour and warn him explicitly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Build produced from the v1.7.1 tag with mDNS as the only functional difference, verified by diff against the tag
- [ ] #2 Version reports as 1.7.1-no-mdns.1 in the telnet banner, the web UI and the MQTT version topic
- [ ] #3 Firmware and filesystem binaries built and verified to boot, with WiFi, MQTT and the web UI reachable by IP
- [ ] #4 Tester briefed on the otgw.local loss and the update-check downgrade risk before flashing
- [ ] #5 At least 4 hours of heap telemetry collected on the tester device, or a crash captured, whichever comes first
- [ ] #6 Result recorded in TASK-1037 as either 'mDNS is leak and crash site' or 'mDNS is crash site only', with the heap trend as evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Capture method for this experiment: scripts/capture-heap-leak.bat (TASK-1041, committed c7d48646). Do NOT use capture-mqtt-debug.bat directly, its headless browser and debug-toggle enabling are exactly the load that made the earlier captures unreadable for leak purposes.

Run it on both arms, same conditions, web UI closed:
- arm A: stock 1.7.1, several hours
- arm B: 1.7.1-no-mdns.1, several hours
Compare the free-heap and maxBlock trends.

Worktree: D:\Users\Robert\Documents\GitHub\RvdB\wt-no-mdns on branch exp-no-mdns-1.7.1, rooted at v1.7.1 (5475feee). Commit 3a56b39f.

Built and verified:
- build.bat completes, 0 errors
- artifacts OTGW-firmware-1.7.1-no-mdns.1+5475fee.ino.bin (743104 B) + .littlefs.bin (2072576 B) + .elf preserved
- xtensa-lx106-elf-nm on the .elf reports 0 MDNSResponder symbols, so the whole library including the stcMDNS_RRAnswer constructor at the decoded crash address is out of the link. Control: LLMNR, deliberately left enabled, still contributes 21 symbols.

Gotcha worth remembering: passing --prerelease to autoinc-semver.py is NOT enough when _VERSION_PRERELEASE is commented out in version.h. It writes the derived strings, but the build then regenerates version.h and the tag silently disappears. The first build produced a binary calling itself plain 1.7.1, indistinguishable from stock. Set the define active first.

Remaining: AC3 (boot + reachability by IP), AC4 (brief the tester), AC5 (4h telemetry), AC6 (record the verdict in TASK-1037). Capture with scripts/capture-heap-leak.bat from TASK-1041, web UI closed.

Worktree: RvdB/wt-no-mdns on branch exp-no-mdns-1.7.1, rooted at v1.7.1 (5475feee). Commit 3a56b39f.

Built and verified:
- build.bat completes, 0 errors.
- Artifacts: OTGW-firmware-1.7.1-no-mdns.1+5475fee.ino.bin (743104 B), .littlefs.bin (2072576 B), .elf preserved for addr2line.
- xtensa-lx106-elf-nm on the .elf reports 0 MDNSResponder symbols: the whole library is out of the link, including the stcMDNS_RRAnswer constructor at the decoded crash address. Control: LLMNR, deliberately left enabled, still contributes 21 symbols. This is stronger evidence than a size diff, which was misleading here because the first build already had mDNS off.
- Functional diff vs v1.7.1 is exactly the OTGW_DISABLE_MDNS flag plus the four guards. Everything else in the 26-file diff is version-banner churn and the githash in data/version.hash.

Gotcha worth remembering: passing --prerelease to autoinc-semver.py is NOT enough when _VERSION_PRERELEASE sits commented out in version.h. The script writes the derived strings, but the build then regenerates version.h from the (still commented) define and the tag silently disappears. The first build produced a binary calling itself plain 1.7.1, indistinguishable from stock, which is exactly the confusion this task set out to avoid. Set the define active first.

AC2 deliberately left unchecked: version.h and the artifact names are right, but "reports as" needs an actual boot. Verify on the telnet banner, the web UI and the MQTT version topic once flashed.

Remaining: AC2, AC3 (boot plus reachability by IP), AC4 (brief the tester), AC5 (4h telemetry), AC6 (verdict into TASK-1037). Capture with scripts/capture-heap-leak.bat from TASK-1041, web UI closed for the duration.

UITSLAG DISCRIMINEREND EXPERIMENT (TASK-1040), capture transcript-20260720-191135, build 1.7.1-no-mdns.1+5475fee, device 48E72958B013, boot 19:11, dood 20:34:47 (83 min uptime).

VERDICT: mDNS is NIET het lek. Het verval treedt zonder mDNS gewoon op. Maar het beeld is genuanceerder dan een simpel nee.

1. NIEUW EN OPVALLEND: 59 minuten lang stond de heap byte-identiek stil op 20992 vrij / 14416 grootste blok, sample na sample. Dat is in geen enkele eerdere capture voorgekomen; stock-builds schommelden altijd honderden bytes. Het weghalen van mDNS elimineerde alle idle-churn volledig.

2. Vanaf 20:12 alsnog een monotoon en versnellend verval: 20992 -> 4528 in 22 minuten, van ~100 B/min oplopend naar ~1500 B/min.

3. Het toestel is gestorven om 20:34:47. Bewijs: laatste telnet-regel 20:34:47, capture pas gestopt 20:40:14, en het toggle-herstel meldde "partial (1 of 2) - An existing connection was forcibly closed by the remote host". De verbinding was dus al hard weg.

4. GEEN crash-signatuur beschikbaar voor deze dood. De reboot_log is alleen bij boot en bij poll #2 opgehaald, beide voor 20:34. Wat er in reboot_log staat is historie van voor deze build (Software/System restart, External Watchdog, en een oudere Exception (2)). Om te weten hoe deze build stierf is de reboot_log van de VOLGENDE boot nodig.

5. Onset valt samen met twee gebeurtenissen in dezelfde minuut: NTP-resync om 20:11:31 (met timezone-lookup) en crashlog-poll #2 om ~20:11:37 (de capture-samenvatting bevestigt "2 polls"). Allebei hadden echter een onschuldige eerste keer: NTP om 19:41:30 zonder enig effect (heap bleef exact 20992), crashlog om 19:11:37 idem. Dat pleit tegen een simpel lek-per-gebeurtenis.

6. NIET veroorzaakt door MQTT- of OT-belasting. OTGW-publicaties liggen vlak op 195-272 per 5 minuten over de hele run, ook dwars door de onset heen. OT-frames idem.

7. De discovery-republish (119 configs) zit NA uptime 4847s, dus na 20:32, twintig minuten NA de onset. Gevolg, geen oorzaak; vermoedelijk een MQTT-herverbinding onder heap-druk.

BLINDE VLEK, en dit is nu het belangrijkste punt: met -QuietDebugToggles staat REST-logging uit, dus browser- en REST-verkeer is in deze capture ONZICHTBAAR. Uit de vorige run weten we dat deze gebruiker twee tabbladen open had. Een browser die om 20:12 opengaat zou exact dit patroon geven en zou hier niet te zien zijn.

CONSEQUENTIE VOOR DE TOOLING: het stille script loste instrument-perturbatie op en creeerde een nieuw gat. De volgende meting moet OTmsg, MQTT, MQTTGate en Sensors stil houden maar REST API AAN laten. Dat is precies de verdachte belasting, en die kost weinig logvolume.
<!-- SECTION:NOTES:END -->
