---
id: TASK-1050
title: >-
  Ignore DHCP-provided NTP servers (sntp_servermode_dhcp(0)) to stop per-renewal
  SNTP leak
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-28 17:39'
updated_date: '2026-07-28 20:09'
labels:
  - bug
  - network
dependencies: []
priority: high
ordinal: 169000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence (beta.4 reporter with Pi-hole DHCP + static-IP fix; pi-hole discourse 81736; TASK-1037 uptime-locked onset at T1 renewal) converges on lwIP DHCP option 42 handling: our prebuilt lwIP2 has LWIP_DHCP_GET_NTP_SRV=1 (lwipopts.h:958), so every DHCP ACK/renewal pushes the DHCP-supplied NTP server into the SDK SNTP module, leaking heap on routers that send option 42 (Pi-hole, some D-Link). Firmware manages its own NTP (pool.ntp.org), so DHCP-supplied NTP is unwanted. Fix: call sntp_servermode_dhcp(0) at boot before WiFi/DHCP so option 42 is ignored.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sntp_servermode_dhcp(0) is called at boot before DHCP can deliver option 42
- [x] #2 Firmware NTP sync via pool.ntp.org still works (settings-driven loopNTP unaffected)
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
- [ ] #5 Field validation: reporter on DHCP (Pi-hole network) survives well past the ~90min renewal boundary on a build with this change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify LWIP_DHCP_GET_NTP_SRV=1 in prebuilt lwIP2 (done: lwipopts.h:958)\n2. Add #include <lwip/apps/sntp.h> to OTGW-firmware.h\n3. Call sntp_servermode_dhcp(0) at top of setup(), before persistent-WiFi autoconnect completes DHCP\n4. Build + evaluate gates\n5. Test build to reporter (Pi-hole network) for field validation past 90-min renewal boundary
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope per user: firmware-side one-liner ONLY, no additional SDK/core patches (the igmp_leavegroup + second UdpContext nullcheck proposals are explicitly out of scope). Include added at OTGW-firmware.h:35; call at top of setup() right after WatchDogEnabled(0). lwip/apps/sntp.h is extern-C-guarded; symbol confirmed present in liblwip2*.a; guarded decl is the real function in our build since SNTP_GET_SERVERS_FROM_DHCP=1.

Built clean: exit 0 + fresh bin (764752B). Committed as 6eceed8f (include OTGW-firmware.h:35, call top of setup()). AC2 rationale: loopNTP/startNTP use configTime with settings-driven hostname (pool.ntp.org) via sntp_setservername; sntp_servermode_dhcp(0) only stops DHCP option-42 injection, does not touch name-based slots. Evaluator: same single pre-existing ADR-087 reference failure, nothing new. Artifact rebuild with commit hash running. AC5 (field validation on Pi-hole network past 90-min boundary) remains open - reporter-gated.

ROOT CAUSE RESEARCH COMPLETE (multi-agent, source-verified, 2026-07-28). Chain: (1) core 2.7.4 lwIP2 glue force-enables sntp_servermode_dhcp(1)+OPMODE_POLL at lwip init (lwip-git.c:436, confirmed via disassembly of esp2glue_lwip_init in our liblwip2) - SNTP runs regardless of sketch; (2) every DHCP ACK incl. renewals runs dhcp_handle_ack->dhcp_set_ntp_servers->sntp_setserver: option-42 addr into slot 0, name=NULL, other slots wiped - silently replaces pool.ntp.org, no fallback; (3) lwIP 2.1.2 sntp.c lacks two upstream fixes: 5c2887a2 (bug #56431, four sys_untimeout-before-sys_timeout dedup lines) and 5666f305 (bug #55253, KoD server exclusion via kod_received flag). Unsynced option-42 server (stratum-0/KoD; Pi-hole thread proved leak toggles with server health, not option presence) -> duplicate self-perpetuating timer chains; MEMP_MEM_MALLOC=1 makes every pending timer a heap malloc with MEMP_NUM caps ignored -> accelerating referenced-alloc leak matching field signature. Both fixes are in lwIP 2.1.3 = core 3.1.0+; cores 2.7.4/3.0.x vulnerable. sntp_servermode_dhcp(0) verified correct: runs after glue init (SDK boot), flag checked at sntp.c:805, glue re-enables only at lwip_init. Open: onset clock T1-renewal vs second-SNTP-poll (+3600s) indistinguishable without packet capture; irrelevant for the fix. Beta.5 (3b42cf79) built+delivered; awaiting field test (AC5).
<!-- SECTION:NOTES:END -->
