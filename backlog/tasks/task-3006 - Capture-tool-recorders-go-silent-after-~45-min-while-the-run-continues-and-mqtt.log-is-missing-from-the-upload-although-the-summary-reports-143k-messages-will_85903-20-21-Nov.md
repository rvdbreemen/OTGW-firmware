---
id: TASK-3006
title: >-
  Capture tool: recorders go silent after ~45 min while the run continues, and
  mqtt.log is missing from the upload although the summary reports 143k messages
  (will_85903, 20-21 Nov)
status: In Progress
assignee:
  - otgw-poc-dev
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

2026-11-21 (otgw-dev, handover to otgw-poc-dev on the coordinator's priority change; read-only analysis, no code changed): Evidence x/transcript TOOL ERRORS section: script.error.log has 'segment store: write failed (1 so far): OSError: [Errno 22] Invalid argument' from 2026-11-20T13:18:51 (right after the telnet+/ws reconnect at 13:18:51, ConnectionResetError on /ws), 10 / 100 at 13:18:52, 1000 at 13:19:09, 10000 at 17:33:12. Mechanism (code, scripts/otgw_capture.py): SegmentStore._handle_locked() caches one open handle per stream; write_many() catches OSError/ValueError and only counts it (_write_failed reports at 1/10/100/1000/10000 into script.error.log) but keeps the broken handle, so after one SMB hiccup on the NAS run folder every later write of that stream fails for the rest of the run. Fix direction: on a write error close and drop the handle from _handles so the next write reopens the file (and log 'reopened'). Counters lie: MqttCapture.handle() increments self.messages BEFORE the write and swallows exceptions (otgw_capture.py ~1377-1392); FrameCapture counts 208134 frames while otframes.log has 6864 lines; the summary says 'written' for both. Fix: count only successful writes (write_many returns ok) and report written vs received. Separate issue: from 08:04 (device time 08:07:33) to 13:18 both device streams (telnet and /ws) delivered nothing while the sockets stayed open and the device was up (bootcount 8 -> 8, uptime continuous); no write errors in that window, so that gap is data not arriving, not writes failing; TelnetIdleCheckSeconds 60 'health probe gated' kept waiting because the HTTP health answered. Needs a 'no data for N min although health OK -> reconnect + gap line' rule (AC2). mqtt.log in the zip: a short run never adds mqtt.log itself to the upload (build_upload_zip adds transcript, summary, crashlog, device-state, graph history, otframes/sim and csv/); MQTT is only in the transcript section (5825 lines here vs 143483 counted) and csv/mqtt.csv, which is missing because convert_mqtt_csv only accepts the long-run tab format (short-run lines are 'topic payload'), so 0 rows -> file deleted. crashlog.log: the poller logs only changes, so 7 entries is not evidence of a stop by itself (the #counter is the poll number: #69 at 07:56); a heartbeat line per N polls would make a stop visible.
<!-- SECTION:NOTES:END -->
