---
id: TASK-1943
title: >-
  Classic PIC: answers to PR= commands are not processed; /api/v2/pic/settings
  stays empty (.63, beta.250 and beta.254)
status: To Do
assignee: []
created_date: '2026-11-14T17:47:00Z'
updated_date: '2026-11-14T18:09:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 994000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on .63 (Classic + ESP32-S3, PIC pic16f1847 gateway 6.8, device/info picfwversion 6.8 picdeviceid pic16f1847, so the PIC is detected) on 2.0.0-beta.250+baf84d9 and 2.0.0-beta.254+f12af9f, 2026-11-14. GET /api/v2/pic/settings returns every field empty (setpoint_override, setback, dhw_override, gpio, gpio_states, led, tweaks, temp_sensor, smart_power, thermostat_detect, builddate, clock_mhz, reset_cause, standalone_interval, voltage_ref), also after triggering the settings cycle twice and waiting 90 s; no otgw-pic/settings/* topic on MQTT (only otgw-pic/version, deviceid, firmwaretype, designer at boot). Telnet shows the cycle sending PR=T, PR=D, PR=P, PR=R, PR=B, PR=C, PR=Q, PR=N, PR=V, PR=M about 3 s apart ('CmdQueue: Insert queue in slot[0]:cmd[PR=x]' then 'CmdQueue: Queue slot [0] due') but not a single 'PR: x=...' reply line. A manual POST /api/v2/otgw/commands {"command":"PR=A"} at 18:46:41 (beta.254): inserted, 'Queue slot [0] due' at 18:46:42.405, no reply line in 20 s, while the PIC's OT traffic (R80000200 every second) is logged normally. TASK-1940 PR=E polls on beta.254 likewise show no 'PR: E=' reply. Not known yet: whether the PIC answers at all on the serial line (no raw serial capture), whether this is specific to .63 or to SAT being enabled (SAT simulation was on), and since which build. Possibly visible to every beta user with a Classic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a Classic: PR=A (or the settings cycle) gives no processed reply and an empty /api/v2/pic/settings; capture the raw PIC serial answer if possible
- [ ] #2 Find the cause (parser, queue, SAT/simulation interaction or the bench PIC) with evidence
- [ ] #3 Fix if it is firmware: /api/v2/pic/settings filled and otgw-pic/settings/* published after the cycle; red-then-green on the bench
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 19:09 (09, coordinator hint): GET /api/v2/health on .63 (beta.250 again) reports otgwconnected:false, otgwmode:"detecting", boilerconnected:false, thermostatconnected:false, simulation_active:false. The R80000200 frames seen every second are the gateway's own R (request) frames towards the boiler, with no boiler answer: the PIC on .63 is not on an OT bus or a simulator. Open question for whoever picks this up: does the PIC answer PR= at all without an OT bus (it should; PR= is a local query), or does the firmware ignore PIC lines while otgwmode is 'detecting'? A raw serial capture or a test on a Classic with a boiler or the boiler simulator settles it.
<!-- SECTION:NOTES:END -->
