---
id: TASK-1951
title: >-
  OT-Direct PR=M answers S/P/L, which handlePRresponse logs as unexpected and
  ignores
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T22:57:00Z'
updated_date: '2026-11-15T07:55:00Z'
labels:
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 996000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on .61 (beta.264, OT-Direct, no boiler) 2026-11-14: MQTT command PR=M gives telnet 'PR: M=S' and handlePRresponse logs 'PR=M unexpected value [S]'. Cause (95, from code): OT-Direct reports its own modes in PR: M= (OTDirect.ino:3857-3865: G, M, P=bypass, S=master/standalone, L=loopback), while handlePRresponse (OTGW-Core.ino:1367-1379) accepts only G/M (the PIC only knows G/M), logs 'unexpected value' for anything else and returns without updating the gateway-mode state. Not caused by TASK-1848. Decision needed: the consumer accepts the OT-Direct extensions S/P/L, or OT-Direct reports G/M like the PIC (pyotgw compatibility).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decide (coordinator/Robert): the consumer accepts S/P/L, or OT-Direct answers PR=M with G/M like the PIC
- [x] #2 Host test on the real handlePRresponse / OT-Direct PR=M path: red on the current code, green on the fix; no 'unexpected value' for a valid OT-Direct mode
- [ ] #3 Bench: PR=M on an OT-Direct unit updates the gateway-mode state without the warning
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 ba research (read-only, no code). Facts: (1) OT-Direct PR=M prints G/M/P/S/L (OTDirect.ino:3857-3865) and passes the line through otDirectBridgeProcessPRResponse() (OTDirect.ino:2701-2706) to the 25238 bridge and processOT(), so handlePRresponse() sees it. (2) handlePRresponse() 'M' branch (OTGW-Core.ino:1368-1389) accepts only G/g and M/m; anything else logs 'unexpected value' and returns: state.otBus.bGatewayMode/bGatewayModeKnown stay as they were and otgw-pic/gateway_mode is not published. (3) The automatic PR=M query queryOTGWgatewaymode() (OTGW-Core.ino:1103-1120) returns when !state.pic.bAvailable, so on OT-Direct only a manual PR=M (MQTT, REST, telnet, 25238 client such as OTmonitor) triggers it. (4) Consumers of bGatewayMode: MQTT connect re-publish (MQTTstuff.ino:2351), SAT PM=25 gate (SATcontrol.ino:2070/2111, also requires isPICEnabled()), debug dump, REST device/info otgwmode (restAPI.ino:3351/3563, 'N/A' when !isGatewayFirmware(), which is always false without a PIC: OTGW-firmware.h:786). So on OT-Direct nothing functional depends on it today. (5) The full OT-Direct mode is already exposed: REST device/info otdmode gateway/monitor/bypass/master/loopback (restAPI.ino:3229-3235). (6) GW= on OT-Direct accepts the same extensions (OTDirect.ino:3784-3822: M, 2/S, P, L; TASK-1946), so PR=M mirrors what GW= sets. (7) PIC 6.8 prints only G or M (gateway.asm:2229-2232 PrintGateway). (8) Third parties: OTmonitor matches only 'PR: M=[GM]' (otmonitor.tcl:635-637) and ignores other letters; pyotgw stores the raw text after 'M=' without validation (pyotgw.py:192/199, vars.py:359) and HA shows it as the plain text sensor operating_mode (core-dev opentherm_gw/sensor.py:341-344).

Options for Robert. A = the consumer accepts the OT-Direct letters: handlePRresponse() maps G, S (master) and L (loopback) to gateway_mode ON (the ESP generates or rewrites the frames) and M and P (bypass relay, thermostat wired to the boiler) to OFF; no warning for a valid OT-Direct letter; PIC behaviour unchanged. Consequence: GW=S then PR=M reads back S (round trip kept), HA's operating_mode keeps showing the real letter, OTmonitor stays as today (it ignores S/P/L). B = OT-Direct answers PR=M with G or M like the PIC (S, L -> G; P -> M); handlePRresponse unchanged. Consequence: full compatibility for OTmonitor and pyotgw, but GW=S then PR=M reads back G, and the real mode is only in REST otdmode. Recommendation A: the PIC command surface of OT-Direct already carries these extensions in GW=, PR=M is its read-back, no third-party client breaks on it (pyotgw/HA pass the text through, OTmonitor ignores unknown letters), and the fix is in one function with a host test on the real handler. Main risk of A: a future client that validates [GM] strictly; low, none found in pyotgw, HA or OTmonitor.

2026-11-15 decision (Robert, Discord DM 1571428391457005750, relayed by the coordinator): "A". handlePRresponse() accepts the OT-Direct letters: G, S, L = gateway mode ON; M, P = OFF; no 'unexpected value' warning for a valid OT-Direct letter; PIC behaviour unchanged.

2026-11-15 fix (option A): handlePRresponse() 'M' branch (OTGW-Core.ino) maps G/S/L (any case) to gateway ON and M/P to OFF; any other letter still logs 'unexpected value' and leaves the state alone. Host test test/host/test_pr_m_otd_letters.py (real handlePRresponse + PicSettingsSection sliced by anchor): --old-rev origin/dev -> OLD fails exactly D1 (S), D2 (L), D3 (P) (known=0, warned=1, nothing published), passes controls C1 G, C2/C2b M, C3 X; FIX 7/7 pass (published gateway_mode ON/OFF, no warning). RESULT PASS. Neighbours on the same handler: test_pic_settings_value, test_boiler_unsupported_origin, test_local_frame_consumers PASS; tests/test_pic_temp_sensor_port OK. Commit + bump after the beta slot, together with 1947-A in one build.
<!-- SECTION:NOTES:END -->
