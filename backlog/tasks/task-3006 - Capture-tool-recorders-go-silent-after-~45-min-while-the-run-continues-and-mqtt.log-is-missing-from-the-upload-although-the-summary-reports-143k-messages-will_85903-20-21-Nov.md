---
id: TASK-3006
title: >-
  Capture tool: recorders go silent after ~45 min while the run continues, and
  mqtt.log is missing from the upload although the summary reports 143k messages
  (will_85903, 20-21 Nov)
status: To Do
assignee: []
created_date: '2026-11-21T14:02:00Z'
updated_date: '2026-11-21T15:24:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1128000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
will_85903's 31 h capture (beta.392 device, capture tool on Windows, Python 3.14.7, run folder on a NAS share \192.168.1.2\...). Upload: OTGW-field-data/will_85903/2026-11-21_upload/20261120-072201-upload.zip (posted #beta-testing msg 1573699374830190734). Facts: (1) device telnet output in the transcript and otframes.log both end at device time 08:07:33 (host 08:04:32) on 20 Nov; after that the transcript only has tool lines (reconnect notices 13:18 and 17:21, stop-time HTTP probes at 14:07 on 21 Nov). (2) crashlog.log has only 7 poll entries, the last at 07:56, although the poll interval is 30 s over 31 h. (3) The device did NOT reboot: bootcount 8 at start and stop, uptime 07:14 -> 1d 13:59 continuous. (4) summary.txt says 'MQTT capture: 143483 message(s) written to mqtt.log', but the zip has no mqtt.log. So the tool reported success for data it did not deliver, and its recorder threads stopped silently while the run went on. Possibly related: the run folder is on a network share.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce or explain the silent stop of the telnet/frame/crashlog recorders (e.g. a NAS run folder, a blocking write, Python 3.14), with a test on the real tool that fails on the old code
- [ ] #2 The tool notices a recorder that produced nothing for N minutes and reports it in summary.txt and gaps.log instead of continuing silently
- [ ] #3 The upload zip contains mqtt.log whenever the summary reports MQTT messages, or the summary says why it is missing
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (BlueFlame): likely cause from the tester, #beta-testing msg 1573720571928248551 (will_85903): 'Think that was me closing the lid of my laptop'. Laptop sleep stops the capture process, so the recorders going silent after ~45 min is probably host sleep, not a tool bug. Open for this task: the missing mqtt.log while the summary claimed 143k messages is not explained by sleep; and the tool could detect a host sleep (wall-clock jump with no frames) and say so in summary.txt / gaps.log, plus keep the machine awake (Windows SetThreadExecutionState) while recording.
<!-- SECTION:NOTES:END -->
