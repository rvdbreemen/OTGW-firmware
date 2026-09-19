---
id: TASK-1139
title: 'Investigate: gateway reports no thermostat and no boiler after 1.7.5 (GH #684)'
status: To Do
assignee: []
created_date: '2026-09-19 05:09'
updated_date: '2026-09-19 15:18'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/684'
priority: medium
ordinal: 220000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Appiejs on GH #684, 2026-09-18. After upgrading to 1.7.5 the gateway shows an empty home screen and reports Thermostat Connected false, Boiler Connected false, OpenTherm Active false, Gateway Mode Monitor. PIC is gateway v6.6 on pic16f1847.

Two things in the report argue AGAINST a 1.7.5 regression, and they should be checked before anyone looks at firmware:

1. He states he also tried 1.7.1 with PIC 6.6 and got the same result. If that holds, whatever changed is not in the firmware.

2. The telnet banner he pasted reads FW 1.7.1+c50cbcc with Up: 0(d)-00:00(H:m), so it was taken within a minute of boot. Boiler OFF and Thermostat OFF that early is expected: no OpenTherm frames have arrived yet, and the 30s liveness window has not even elapsed. The dump may simply be too early to mean anything, which would make the whole report rest on an artefact.

Worth noting what is NOT the problem: the false flags are the firmware correctly reporting no traffic. This is not TASK-1135, where the flags were stale in the other direction.

Gateway Mode Monitor with GW-mode detecting in the banner is the detail most worth chasing. In monitor mode the gateway does not drive the bus, and detecting means the firmware had not yet established the mode at the time of the dump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A telnet banner taken at least five minutes after boot is obtained, so the liveness window has elapsed and Boiler/Thermostat OFF means something
- [ ] #2 It is established whether the same symptom really reproduces on 1.7.1, including whether the filesystem was flashed alongside the firmware
- [ ] #3 Either a firmware cause is identified with evidence, or the issue is closed as hardware or wiring with the reasoning recorded for the next reader
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-19: asked Appiejs on GH #684 for (a) a telnet banner taken 5+ minutes after boot, since the one he posted reads Up: 0(d)-00:00 and Boiler/Thermostat OFF is expected that early, (b) whether the boiler still runs normally on his thermostat, which separates 'the OTGW sees nothing' from 'there is nothing on the bus', and (c) whether the 1.7.1 test included the filesystem, because firmware-without-filesystem keeps serving the previous web interface and would make 'same result' mean something else. Also flagged Gateway Mode Monitor as worth confirming as deliberate. Pointed at Schelte's hardware checklist as the next step if it still reads empty after five minutes.

2026-09-19: Appiejs re-ran the capture after the baud fix. usb-serial.log is now 99.5 percent printable (was framing garbage), plus telnet.txt of 21 KB and an empty crash-frames.log.

New facts from him: he is now on firmware 1.7.5 and PIC gateway v6.8 (was 1.7.1 / 6.6), and the appliance is a heat PUMP, not a boiler. His own observation is the most useful thing in the report: the thermostat is recognised at first, then the pump loses it and the thermostat alternates between 'OT' and the measured temperature. So this is not 'never worked', it is 'works then degrades'.

Two conclusions that are code-backed:
1. GW-mode stays 'detecting'. bGatewayModeKnown is set only inside handlePRresponse when a valid PR:M reply is parsed (OTGW-Core.ino:759), and the firmware re-sent PR=M at 16:56:37 and 16:57:37. So no gateway-mode answer was ever processed.
2. Zero processOT lines in 21 KB while his debug toggle 1 OTmsg was [1]. OTGWDebugTf is gated on state.debug.bOTmsg, so those lines would have been present if frames were arriving. No OpenTherm frames reach the ESP.

One thing deliberately NOT concluded: that the PIC answers nothing at all. handlePRresponse logs nothing, so absent PR: lines are not evidence of absent replies. Stated as such in the reply rather than glossed over.

Also recorded for future readers: a USB capture on this hardware shows only the ESP transmit side, because the adapter sits on the same line as the ESP. PIC-to-ESP traffic is invisible there; telnet is where received data shows up. Worth adding to the capture docs.

Next steps given to him, in order: (1) does the heat pump work normally with the OTGW removed from between thermostat and pump, which is the cheapest split; (2) Schelte's debugging checklist, since 'works then drops out' fits a marginal supply or reference voltage; (3) the diagnose PIC firmware with the beta.2 diagnose screen if the first two find nothing. Also flagged Reboots: 6 to watch and MQTT disconnected as unrelated.
<!-- SECTION:NOTES:END -->
