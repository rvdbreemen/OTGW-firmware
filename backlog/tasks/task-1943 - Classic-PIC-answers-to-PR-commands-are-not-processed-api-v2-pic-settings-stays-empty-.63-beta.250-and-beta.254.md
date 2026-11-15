---
id: TASK-1943
title: >-
  Classic PIC: answers to PR= commands are not processed; /api/v2/pic/settings
  stays empty (.63, beta.250 and beta.254)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T17:47:00Z'
updated_date: '2026-11-15T03:36:00Z'
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

2026-11-15 picked up by ba (wt-ba-1916): first a code trace of the Classic PR: reply path (serial task -> processOT -> handlePRresponse -> state.pic settings / REST), then a bench brief for 09 on .63 (raw PIC serial capture of PR=A and the settings cycle).

2026-11-15 ba code trace (hypothesis, NOT yet confirmed on the bench). Path: picSerialDrainOnce() (OTGW-Core.ino:723-777) assembles every PIC line and enqueues it without a filter -> drainOTFrameQueue() -> processOT(); a 'PR: x=' line takes the buf[2]==':' branch (OTGW-Core.ino:6284-6291): checkCommandResponse(), handlePRresponse(), Debugln(buf). So any PR: reply from the PIC would print on telnet; 09 saw none, which points at the TX side. TX: the queue logs 'Queue slot [i] due' and calls sendPICSerial() (OTGW-Core.ino:4155-4166). sendPICSerial() (OTGW-Core.ino:4451-4475) returns false without sending when satSimulationBlocksBusTx() is true (TASK-1580: the queue then consumes the entry silently), and that gate (SATcontrol.ino:2976-2997) is true whenever settings.sat.bSimulation OR state.debug.bOTGWSimulation is on, for every command, PR= included. A sent command prints 'Sending to Serial [..]'; a blocked one prints 'SAT-SIM trace: PR=x (src=pic-serial)' (SATTraceTf). The description says SAT simulation was on; the 19:09 note's /api/v2/health simulation_active:false is state.debug.bOTGWSimulation only (restAPI.ino:3573), not the SAT simulation, so it does not rule this out. Also explains otgwmode 'detecting' (PR=M never sent) and the missing PR: E= replies. ADR-117 (Accepted) invariant 1 is 'no boiler-side bytes leave the device'; PR= is a PIC-local register read that puts nothing on the OT bus (gateway.asm 6.8 PrintSetting* only prints), but ADR-117 path (a) blocks all PIC serial. Bench check needed (09, .63 after 08:31): GET /api/v2/settings satsimulation; telnet for 'SAT-SIM trace: PR=' vs 'Sending to Serial [PR='; then with SAT simulation off: PR=A and the settings cycle fill /api/v2/pic/settings.
<!-- SECTION:NOTES:END -->
