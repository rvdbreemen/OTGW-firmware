---
id: TASK-1935
title: File paths are normalised before the access check (1.x port of dev TASK-1934)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T13:16:00Z'
updated_date: '2026-11-14T16:15:00Z'
labels:
  - 1.x
  - security
  - bug
milestone: 1.x
dependencies: []
priority: high
ordinal: 263000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x port of dev TASK-1934. Web file requests are normalised (decoded path, dot segments resolved, paths above the root refused) before the access check and before the file is opened, aligned with the 2.x change. Details are kept locally until both lines have shipped. Robert's scope: trusted LAN; without an admin password the behaviour stays as it is.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real handleFile() and the real core 2.7.4 littlefs path lookup: red on the old code, green on the fix; plain and authorised requests keep working
- [ ] #2 Other path-taking routes on 1.x checked; findings filed
- [ ] #3 Bench by otgw-firmware-09 on .35 (1.x, password set): the probe set from the pre-fix run returns 0 protected bodies without credentials; web UI and authorised reads unchanged; without a password unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (commit 00499860e): fsCanonicalPath() before handleFile()'s access check, the same function as the 2.x change; the access check and the open use one normalised path; a directory request keeps its index.html. Host harness test/host/test_fileAccessPath.py (real handleFile() + the real littlefs of core 2.7.4 on a RAM block device, the decoded form of 09's pre-fix request set on .35): OLD (d0a5d290e) reproduces every case 09 saw on the device and passes the 8 controls; FIX 0 of 11, controls ok; RESULT PASS. Other path-taking routes (AC2): FSexplorer delete and the delete branch of handleFile() call checkHttpAuth() first, upload is gated by uploadAuthorized, the remaining LittleFS opens use fixed paths: nothing else to change. evaluate.py --quick 38/0. test/run_tests.bat: stops at test_mqttBeginPublishDesync (PubSubClient source not in the repo; build.py installs it), unrelated to this change. Details of the probe set are kept locally until both lines have shipped.

Status 2026-11-14 15:10 (parked): fix 00499860e on lane/ba-1x-1934 (local); 1.x lane build 1.8.0-beta.1+488ca5a and the v1.7.10 hotfix (branch hotfix/ba-1.7.10 in ../wt-ba-1x-1710 from the v1.7.9 tag: cherry-pick f33f22226 + version/notes 55be2ad48, build 1.7.10+55be2ad, archived) are both built, nothing pushed, no tag. Robert chose the v1.7.10 hotfix route (DM 1571159498054307881); the release itself is a separate hard stop. The test unit .35 did not come back after the app OTA of 488ca5a at 14:48 (HTTP 200; at 15:02 no ping, no 80, no 25238); needs a power cycle by Robert, who is away. Read-only check: same flash layout as 1.7.9 (eesz=4M2M, build.py unchanged), image size +208 B, OTA free space 1314816 B vs 781472 B needed, ?size= matched. Open: post-mortem on .35 after the power cycle (githash, reboot log top line, reset reason), then the bench of the 1.7.10 build on .35, then the otgw-1.x.x push via the coordinator's GO.

2026-11-14 (09) bench attempt on .35 OTGWCALENTA (Robert's choice A, live: boiler + thermostat connected). Before: 1.7.9+8a42071, bootcount 1083, uptime 1 d 07:26, lastreset 'External System'; settings saved (API 3751 B, raw ini 1732 B). 14:48:50 web OTA the XHR way: curl -H 'Expect:' --limit-rate 100k -F firmware=@fw.bin 'http://192.168.88.35/update?size=781472' (fw.bin = OTGW-firmware-1.8.0-beta.1+488ca5a.ino.bin, 781472 B, sha256 2d0cb944...42dc) -> HTTP 200 in 17.1 s, page text 'Flashing successful! Wait for the OTGW firmware to reboot ...'. After that the unit never came back: from 14:54 HTTP/telnet/25238 time out, ping 0/30 from 14:55 to 15:02. Not on USB, so no remote recovery; waits for a power cycle by Robert. Leading hypothesis (ba, unverified): .35 was serial-flashed on 2026-11-11 and its last boot was an external reset, not a power-on, so the ESP8266 kept the serial-download boot strap and the post-OTA software restart landed in the bootloader. The probe (TASK-1935 AC) has not run yet. After the power cycle: version/githash via / and /api/v1/devinfo, the reboot_log top line + reset reason, then the probe, and v1.7.10 only after ba's GO. Evidence: 09 scratchpad b1935/ (S0.json, S0.ini, info0.json, fw.bin, update-resp.txt).

Post-release step (Robert, DM 1571194518395560029, answer B, relayed by the coordinator 2026-11-14): this task and the 2.x task stay neutral until v1.7.10 is published. Right after the release, edit the GitHub release body of v1.7.10 and the beta.245 alpha note so they describe the issue openly (what it was, only with an admin password set, local network), and update the release notes accordingly. The draft text is kept locally until then.
<!-- SECTION:NOTES:END -->
