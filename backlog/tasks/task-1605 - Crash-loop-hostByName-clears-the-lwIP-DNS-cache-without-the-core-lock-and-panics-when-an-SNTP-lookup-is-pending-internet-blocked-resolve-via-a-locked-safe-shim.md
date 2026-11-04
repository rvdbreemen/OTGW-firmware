---
id: TASK-1605
title: >-
  Crash loop: hostByName() clears the lwIP DNS cache without the core lock and
  panics when an SNTP lookup is pending (internet blocked): resolve via a
  locked-safe shim
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T16:24:00Z'
updated_date: '2026-11-04T18:10:00Z'
labels:
  - network
  - crash
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 746000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .63 beta.74, 2026-11-04 16:56-17:21 (TASK-1471 T1: MikroTik forward drop for .63 to the internet, DNS still answered by the router): Exception/panic every 5 min (bootcount 116 -> 121, reboot_log 'Exception/panic (4)' at 17:01:12, 17:06:13, 17:11:14, 17:16:15, 17:21:16). Core dump backtrace (addr2line, beta.74 elf 2967e703): panic_abort <- __assert_func <- sys_untimeout (lwip timeouts.c:334) <- sntp_try_next_server (sntp.c:439) <- sntp_dns_found (sntp.c:616) <- dns_call_found (dns.c:1071) <- dns_clear_cache (dns.c:414) <- NetworkManager::hostByName (NetworkManager.cpp:82) <- NetworkClient::connect <- HTTPClient::GET <- weatherFetchOpenMeteo (SATweather.ino:375) <- weatherLoop <- loop. arduino-esp32 NetworkManager::hostByName calls dns_clear_cache() without LOCK_TCPIP_CORE on the first call after an IP-state change (always the first call after boot); a pending SNTP DNS callback then runs on loopTask and trips the lwIP core-lock assert. Upstream master is still unlocked (checked 2026-11-04). Affects every hostname lookup through hostByName: SAT weather (SATweather.ino), MQTT hostname broker (MQTTstuff.ino ~1319), PIC update check/download (OTGW-Core.ino ~6915-6962). Moving the fetch to another task (TASK-1604) does not fix it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A Platform shim (platform_esp32.h) resolves a hostname with lwip_getaddrinfo (thread-safe, no dns_clear_cache) with a bounded wait; no application code calls Network/WiFi.hostByName or connects by hostname any more (source gate test)
- [x] #2 Weather, PIC update check/download and MQTT hostname broker connect by the resolved IP while the HTTP Host header keeps the hostname (HTTPClient reuses the pre-connected client)
- [ ] #3 Bench .63 with the TASK-1471 T1 drop rule for 25 min: no panic, bootcount unchanged, no loop stall from these paths (red: beta.74 five panics in 25 min)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 (opus agent; lead reran in wt-int4): platformResolveHost() (platform_esp32.h:406-447, lwip_getaddrinfo AF_INET, never dns_clear_cache; DNS bound derived 7 s per server, 21 s worst case with 3 servers); networkStuff.ino netResolveUrlHost/netConnectIp/netConnectUrl; weather, PIC HEAD/GET, webhook (also fixes its double lookup) do http.begin() THEN connect by IP so HTTPClient reuses the client and the Host header stays the hostname (HTTPClient.cpp:283-288, 1075-1085, 1145); MQTT broker lookup via the shim (setServer(IPAddress) path, ClientSync connect(host) unreachable). tests/test_no_hostbyname.py 12 OK (HEAD: 4 FAIL incl. connect-order C2), weather handoff 10, stream_tail 4 OK, evaluate 0 failed. Known: test_sim_fixture_builtin ser2net case already fails on HEAD (not touched). Tag beta.76.

2026-11-04 bench beta.76 on .61 (T1: MikroTik forward drop .61 -> internet, 18:14-18:39, rule auto-removed, 'left: 0'): 5 weather attempts (18:19, 18:20, 18:22, 18:26, 18:34), each 'Net: DNS lookup failed' after ~14 s inside the worker task (stack 31732 vs loop 32756); bootcount 42 the whole run (no restart), hd_max_loop_gap_ms 456 constant; reboot_log only the flash lines. Same rule on .63 with beta.74 (16:56-17:21): 5 Exception/panic restarts. Same-board (.63) green run with beta.77 follows after the 20:00 publish.
<!-- SECTION:NOTES:END -->
