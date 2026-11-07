---
id: TASK-1690
title: >-
  Capture tool: find the OTGW gateways on the LAN (mDNS) and let the user pick
  one
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T11:35:00Z'
updated_date: '2026-11-07T13:22:00Z'
labels:
  - capture
  - tooling
dependencies: []
priority: medium
ordinal: 806000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw_capture.py asks for the device host by hand. The firmware already announces _http._tcp on port 80 over mDNS (networkStuff.ino startMDNS, both lines) and answers /api/v2/device/info. Discover the gateways with a standard-library mDNS query on every IPv4 interface, confirm each by its device/info fingerprint, and when no --device is given offer a numbered list (last-used host preselected, manual entry still possible). Dev only: Robert chose 2026-11-07 to leave 1.x (old .bat/.sh scripts) as it is. Hand-rolled instead of python-zeroconf: 0.154.0 needs Python >=3.11 (tool supports 3.9), has no pure-Python wheel (71 Cython wheels) and adds ifaddr; LGPL-2.1.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Discovery finds every OTGW on the bench LAN that answers mDNS (live run lists 4 of 4: OTGW1, OTGWELGA, marvin-otgw32, zaphod-classic-s3) and drops non-OTGW _http._tcp responders
- [x] #2 Without --device and with a console: more than one gateway shows a numbered list with hostname, IP, firmware and the last-used host as default; the chosen IP becomes args.device
- [x] #3 --discover prints the gateways and exits 0 without starting a capture; --no-prompt and an explicit --device never run discovery
- [x] #4 Host tests drive the real discovery and selection code against a fake mDNS responder and fake HTTP gateways; red before the change, green after
- [x] #5 One gateway found: it is used without a prompt; --device still picks another (Robert 2026-11-07)
- [x] #6 MQTT password: password=0 asks nothing; password=N>0 is read from the gateway's /settings.ini when the length matches, else a saved password of that length or the prompt; never printed or logged
- [x] #7 No gateway found: the message 'No OTGW answered on the network (mDNS)' and the manual host prompt with the saved host (live via 172.30.32.1, test_g)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07: scope dev only (Robert, AskUserQuestion: 'Alleen dev'). Robert added mid-session: nothing found must fall back to manual input; covered by test_g_nothing_found_falls_back_to_manual_entry and a live run on an interface without gateways.

The list shows hardware_type + hardwaremode instead of board: the 2.x combo image reports 'Nodoshop OTGW Combo (ESP32-S3)' on both marvin-otgw32 (otgw32 OT-Direct) and zaphod-classic-s3 (otgw-classic PIC); 1.x has neither and shows 'PIC <picdeviceid>'.

tests/test_capture_tool.py is timing-flaky on this machine at 100% CPU (Syncthing): runs gave 1 and 4 timing failures with this change and 6 on a HEAD copy (baseline), each time other tests. The cases that touch resolve_inputs and the arguments (GatewayBrokerCase, test_k, test_p) passed 3 of 3 in isolation.

2026-11-07 follow-up (Robert): AC "one gateway as the prompt default" is replaced by "one gateway is used without a prompt". Firmware audit of the MQTT password: GET /api/v2/settings gives only "password=N" (restAPI.ino, snprintf password=%u of strnlen(settings.mqtt.sPasswd)); the debug endpoints print *** / (not set); the only clear-text read path is GET /settings.ini, served by handleFile() in FSexplorer.ino behind checkHttpAuth() (ADR-056 section 1), so open when no admin password is set. The tool reads it there only when N > 0 and the length matches.
Evidence: tests/test_capture_discovery.py 30/30 green; on the previous script 6 failures + 1 error (test_f single gateway, five test_j password cases; test_i failed once under CPU load and passed 4 of 4 in isolation on old and new code). Live --no-prompt resolve: .16 (1.7.5-beta.6) broker homeassistant.local:1883, user set, a 10-character password read from /settings.ini and absent from the output; .61 and .63 anonymous, no settings.ini read. evaluate.py --quick: 0 failed. tests/test_capture_tool.py: 2 timing failures (test_m, test_t); the same set passed, then failed 1, in isolation; those runs use a fake telnet device and never reach the password path.
Not verified: an MQTT login with the read password (only its length was compared), a gateway with an admin password on real hardware (the 401 path is a host test).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: live 'python scripts/otgw_capture.py --discover' on 192.168.88.0/24 listed 4 of 4 OTGWs (OTGW1 1.7.5-beta.6, OTGWELGA 1.7.2-beta.3, marvin-otgw32 2.0.0-beta.120, zaphod-classic-s3 2.0.0-beta.120) out of 10 _http._tcp responders (Shelly, HP LaserJet, slimmelezer dropped), exit 0, 5.6 s, all interfaces. AC2: live resolve_inputs with stdin scripted (not a real TTY): 4-entry list, default [4] = saved 192.168.88.63, answer 3 -> args.device 192.168.88.61; tests test_e_* (list, saved IP and hostname as default, number, typed host, out-of-range asked again). AC3: test_f_single_gateway_is_the_prompt_default, test_g_nothing_found_falls_back_to_manual_entry, live fallback: 'No OTGW answered ... enter its host' then 'OTGW device host [192.168.88.63]:'. AC4: test_i_discover_lists_and_exits_without_capture (subprocess, no logs dir), test_h_explicit_device_skips_discovery, test_h_no_prompt_skips_discovery. AC5: tests/test_capture_discovery.py red on the old code (22 tests: 1 failure, 21 errors) and green after (24 tests OK, 3 consecutive runs). Gates: evaluate.py --quick 92 checks, 0 failed. Not verified: macOS/Linux, networks with client isolation, a real interactive console.
<!-- SECTION:FINAL_SUMMARY:END -->
