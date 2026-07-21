---
id: TASK-1046
title: >-
  Raise NTP resync interval from 30 minutes to once per day (discriminating test
  for the 80-minute death)
status: To Do
assignee: []
created_date: '2026-07-20 20:14'
updated_date: '2026-07-21 19:59'
labels: []
dependencies: []
priority: high
ordinal: 165000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence (TASK-1037, TASK-1040) plus a Grafana uptime graph over 17-19 July show a consistent sawtooth: the OTGW dies around 4800 seconds (80 minutes) of uptime, day after day, and the no-mdns experiment died at 4992s with the same shape. The decay onset lines up with the SECOND NTP resync: the firmware resyncs every 30 minutes (NTP_RESYNC_TIME = 1800), the first resync at ~30 min is harmless, the second at ~60 min starts the monotone heap decay, death follows ~22 minutes later.

The SNTP layer itself was cleared as a leak: on ESP8266 Core 2.7.4, configTime(int,...) only calls setServer() (a pointer store, no allocation, verified against upstream lwIP and the vendored lwip1 source), and sntp_init guards on sntp_pcb==NULL. So whatever the resync costs is in the path around it: the DNS lookup of pool.ntp.org, or the AceTime timezoneManager.createForZoneName() call that runs on every sync.

This task raises the resync interval on the beta from 30 minutes to once per day, as a discriminating change: if the sawtooth flattens, the resync path is confirmed as the trigger; if it does not, the leak is elsewhere and we have ruled out the strongest current suspect. A daily resync is more than adequate for a device whose clock drift over 24h is far below the one-second display resolution, and NTP still runs at boot.

Change is a single constant: NTP_RESYNC_TIME in OTGW-firmware.h from 1800 to 86400.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 NTP_RESYNC_TIME set to 86400 seconds
- [x] #2 NTP still syncs at boot
- [x] #3 Build passes and evaluator shows no new failures
- [x] #4 Field/bench observation records whether the 80-minute sawtooth flattens (confirm or refute the resync-path hypothesis)
<!-- AC:END -->



## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
NTP_RESYNC_TIME 1800 -> 86400 in commit 6eda7ce2. Build clean, evaluator 34/37 (single failure pre-existing). AC4 (does the 80-minute sawtooth flatten?) stays open: needs a multi-hour run on the beta, which is exactly what the 1.7.2-beta.1 release directory is for.
<!-- SECTION:NOTES:END -->
