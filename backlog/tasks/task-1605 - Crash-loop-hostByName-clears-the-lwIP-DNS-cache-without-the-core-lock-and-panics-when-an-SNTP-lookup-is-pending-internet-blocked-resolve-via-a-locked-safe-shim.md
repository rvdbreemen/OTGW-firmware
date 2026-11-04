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
updated_date: '2026-11-04T16:24:00Z'
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
- [ ] #1 A Platform shim (platform_esp32.h) resolves a hostname with lwip_getaddrinfo (thread-safe, no dns_clear_cache) with a bounded wait; no application code calls Network/WiFi.hostByName or connects by hostname any more (source gate test)
- [ ] #2 Weather, PIC update check/download and MQTT hostname broker connect by the resolved IP while the HTTP Host header keeps the hostname (HTTPClient reuses the pre-connected client)
- [ ] #3 Bench .63 with the TASK-1471 T1 drop rule for 25 min: no panic, bootcount unchanged, no loop stall from these paths (red: beta.74 five panics in 25 min)
<!-- AC:END -->
