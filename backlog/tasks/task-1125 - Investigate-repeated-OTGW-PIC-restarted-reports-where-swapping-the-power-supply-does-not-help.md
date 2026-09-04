---
id: TASK-1125
title: >-
  Investigate repeated 'OTGW PIC restarted' reports where swapping the power
  supply does not help
status: To Do
assignee: []
created_date: '2026-09-04 20:58'
labels:
  - bug
  - needs-info
  - support
dependencies: []
priority: medium
ordinal: 213000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on Tweakers by Compizfox, 2026-09-04 20:03 UTC: recurring "OTGW PIC restarted [5.8]" events, several different power supplies tried, no difference, asking what else could cause it.\n\nThe premise worth testing before accepting the power hypothesis: that message is emitted from the PIC BANNER branch of processOT() (OTGW-Core.ino:4585), so it fires on every PIC boot, whatever caused it. The ESP resets the PIC deliberately, twice per ESP boot: detectPIC() at OTGW-firmware.ino:138 and resetOTGW() at :199, both calling OTGWSerial::resetPic(), which pulses PICRST and writes GW=R. So a rebooting ESP produces a stream of 'PIC restarted' events while the PIC power rail is perfectly healthy, and swapping the PSU that feeds the PIC would change nothing. That matches the reporter's observation exactly.\n\nOther deliberate reset paths to rule in or out: the MQTT reset topic (MQTTstuff.ino:746), a GW=R line arriving over the ser2net bridge (OTGW-Core.ino:4788), and telnet 'p' (handleDebug.ino:150).\n\nPrior art with the same signature: TASK-538 (archived), GW=R stuck in the command queue caused an infinite PIC reset loop. If the reporter runs a firmware old enough to predate that fix, this is likely a rediscovery rather than something new. Their PIC is 5.8, which is well behind the current 6.8, so their ESP build may be old too.\n\nWhat to ask before writing any code: the ESP firmware version, and whether runtime.reboots and uptime move in step with the 'PIC restarted' events. If the ESP reboot count tracks them, the PIC is a symptom and the ESP is the subject.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The reporter's ESP firmware version and reboot/uptime counters are obtained, and it is established whether the ESP is restarting alongside the PIC
- [ ] #2 If the ESP is stable, the deliberate reset paths (MQTT reset topic, ser2net GW=R, telnet p, command-queue GW=R as in TASK-538) are ruled in or out against their setup
- [ ] #3 The outcome is reported back on the Tweakers thread, including the finding that a PSU swap cannot help when the ESP is the one restarting the PIC
<!-- AC:END -->
