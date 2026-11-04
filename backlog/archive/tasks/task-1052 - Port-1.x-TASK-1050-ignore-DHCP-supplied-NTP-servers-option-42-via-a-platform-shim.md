---
id: TASK-1052
title: >-
  Port 1.x TASK-1050: ignore DHCP-supplied NTP servers (option 42) via a
  platform shim
status: Done
assignee:
  - '@claude'
created_date: '2026-07-31 19:51'
updated_date: '2026-10-03 18:02'
labels: []
dependencies: []
ordinal: 247000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit 6eceed8f7 (v1.7.2). VERIFIED APPLICABLE ON ESP32-S3: D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/sdkconfig line 2819 has CONFIG_LWIP_DHCP_GET_NTP_SRV=y, so every DHCP renewal pushes the router NTP server (option 42) into the SNTP module, same leak class as the 1.x lwIP2 path. Field signature on 1.x was an uptime-locked heap onset at T1 lease renewal followed by reboot. The firmware runs its own NTP via startNTP(), so DHCP-supplied servers are unwanted regardless. Per the CLAUDE.md ESP platform abstraction rule this must NOT be a raw esp_sntp call in the .ino: add a platformIgnoreDhcpNtp() shim to platform_esp32.h (esp_sntp_servermode_dhcp(0)) and platform_esp8266.h (sntp_servermode_dhcp(0)) and call it unguarded from setup() before WiFi auto-connect can complete DHCP.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 platformIgnoreDhcpNtp() shim exists in BOTH platform_esp32.h and platform_esp8266.h
- [x] #2 setup() calls platformIgnoreDhcpNtp() before any WiFi/DHCP path, with a comment citing the sdkconfig evidence
- [x] #3 no raw esp_sntp/sntp symbol appears outside the platform headers
- [x] #4 build.bat green for esp32 target
- [x] #5 python evaluate.py --quick shows no new abstraction-boundary violations
- [x] #6 Maintainer decision 2026-10-03: the binary single-writer proof is accepted as the evidence that no DHCP-supplied NTP server reaches SNTP on ESP32. In both ELFs the reject flag lives in .bss (zero at boot), its only writer is called from startWiFi with 0, and esp_netif_sntp_init (the only IDF code that writes 1) is not linked. The bench option-42 field test is dropped.
- [x] #7 The shim is kept as insurance against a framework that flips the default, and it skips the call while the TCP/IP stack is not initialised (sys_thread_tcpip(LWIP_CORE_IS_TCPIP_INITIALIZED)). The 'leaks heap on every renewal' premise is corrected in platform_esp32.h and networkStuff.ino. build.bat is green for esp32, esp32-classic and esp32-combo (alpha.405), and evaluate.py --quick shows 0 failures.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 amended in flight: platform_esp8266.h does NOT exist on this branch. dev is ESP32-S3-only (ESP8266 dropped 2026-06-20), and src/libraries/Platform/src/ contains only boards.h, platform.h and platform_esp32.h. The shim was added to platform_esp32.h alone; there is no second platform header to add it to. The AC was written from the CLAUDE.md abstraction rule, which describes a two-platform layout that no longer applies here.

platformIgnoreDhcpNtp() wraps esp_sntp_servermode_dhcp(0), with #include <esp_sntp.h> added alongside the other esp_* includes. Called as the FIRST statement of setup(), ahead of the runtime hardware detect and anything that can bring up WiFi, so no DHCP handshake can complete before it runs. No raw sntp/esp_sntp symbol appears outside the platform header (verified by grep). evaluate.py 94 checks / 0 failures, no new abstraction-boundary violations; build.bat --target esp32 SUCCESS for firmware and filesystem.

Evidence for applicability, since my first read of this was wrong: D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/sdkconfig:2819 has CONFIG_LWIP_DHCP_GET_NTP_SRV=y. I had initially assumed ESP-IDF defaulted this off and that the port was unnecessary.

STATUS CORRECTED 2026-07-31: moved back from Done to In Progress. It was closed on a green compile, which is the wrong bar for this change. The entire value of esp_sntp_servermode_dhcp(0) is runtime behaviour across DHCP lease renewals; a build proves only that the symbol resolves.

Specific unverified risk: the shim is called as the FIRST statement of setup(), i.e. before esp_netif/lwIP bring-up. The 1.x original did the same on ESP8266, but that is a different SDK, and an lwIP-side servermode flag set before tcpip_init() may simply be re-initialised by it. If so the call is a silent no-op and the leak path stays open.

FIELD VALIDATION NEEDED (new AC#6): on a bench S3, boot on a network whose DHCP server sends option 42, then confirm no DHCP-supplied server appears in the SNTP configuration and that only settings.ntp.sHostname is in use. Re-check after a forced lease renewal, since the renewal is the actual leak trigger. If the flag does not survive lwIP init, move the call to just after the netif is up and before WiFi.begin().

TASK-1051 (the resync interval) is unaffected and remains correctly Done: it is a compile-time constant with no runtime handshake to verify.

PLACEMENT CORRECTED (alpha.351, commit 3530b9b1). The unverified risk flagged on 2026-07-31 was real, and worse than 'silent no-op'.

Chain, all from the local toolchain sources:
- sntp.c:266 esp_sntp_servermode_dhcp() does not touch the flag; it dispatches through tcpip_callback(do_servermode_dhcp, ...).
- tcpip.c:318 tcpip_callback() opens with LWIP_ASSERT("Invalid mbox", sys_mbox_valid_val(tcpip_mbox)). sys_mbox_valid(mbox) is (*(mbox) != NULL) (port/freertos/include/arch/sys_arch.h:45); tcpip_mbox is a static, NULL until tcpip_init().
- The assert is LIVE in this build: CONFIG_LWIP_ESP_LWIP_ASSERT=y (esp32s3 sdkconfig:2837) skips the LWIP_NOASSERT define at cc.h:33-35, and CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y (sdkconfig:1117) leaves NDEBUG undefined, so cc.h:82 maps LWIP_PLATFORM_ASSERT to __assert_func. Not a no-op: an abort.
- initArduino() (esp32-hal-misc.c:269-318) never calls esp_netif_init(). The first one in our boot path is WiFi.mode() -> wifiLowLevelInit() (WiFiGeneric.cpp:251) -> Network.begin() -> esp_netif_init(). Grepped the firmware for esp_netif_init/tcpip_init: no call sites of our own.

Fix: call moved from the first statement of setup() to directly under WiFi.mode(WIFI_STA) in startWiFi() (networkStuff.ino:92). Still ahead of the leak trigger, which is the T1 renewal rather than the first lease. Ordering contract documented at both the shim and the call site.

REJECTED alternative: calling esp_netif_init() inside the shim to make the call site-independent. It is idempotent (esp_netif_lwip.c:536 guards on LWIP_CORE_IS_TCPIP_INITIALIZED), but it would move TCP/IP bring-up to boot t=0, ahead of probeProImu(), the 0x26 disarm, LittleFS and readSettings() - the same early-setup reordering that hung boot in TASK-949 - and hides a stack-init side effect behind a name that does not advertise it.

Also noted: SNTP_ERROR wraps tcpip_callback's return in LWIP_ERROR, which is a silent no-op when LWIP_DEBUG is unset (cc.h:93). Had the assert not been live, the failed dispatch would have left no trace at all.

Verification: python evaluate.py --quick 76 checks / 0 FAIL (1 pre-existing WARN). build.bat --target esp32 SUCCESS, firmware + littlefs artifacts fresh at alpha.351.

STILL OPEN and NOT claimed: no bench board is attached (no serial ports present, 192.168.1.143 unreachable), so nothing here is hardware-confirmed. The abort behaviour of the OLD placement is a source-derived prediction, not an observed boot. AC#6 unchanged.

AC bookkeeping: AC#1 checked as amended - platform_esp8266.h does not exist on this ESP32-S3-only branch, so platform_esp32.h is the only header there is to add the shim to. AC#5 checked; the evaluator has been green across both the original commit and this one.

HARDWARE A/B DONE 2026-08-03 on the bench OTGW32 (ESP32-S3, COM4, USB-CDC console). The source-derived prediction is now an observed fact.

Method: flash alpha.351 (fixed placement), capture 45s of boot console; then rebuild the SAME tree with only the call moved back to the top of setup(), flash app-only, capture 30s; then restore and re-verify. Reset triggered by an RTS/DTR pulse, output read straight off COM4.

OLD placement (call as first statement of setup()) - boot loop:
  assert failed: tcpip_callback /IDF/components/lwip/lwip/src/api/tcpip.c:318 (Invalid mbox)
  Backtrace: 0x403827ad:0x3fcebca0 0x40382775:0x3fcebcc0 0x403899ce:0x3fcebce0 0x420978a3:0x3fcebe20 0x42095221:0x3fcebe40 0x420319f5:0x3fcebe60 0x420706ff:0x3fcec0f0 0x4038372d:0x3fcec120
  Rebooting...
55 ROM banners in 30 seconds. Exactly the file, line and assertion text predicted from the toolchain sources.

NEW placement (under WiFi.mode(WIFI_STA) in startWiFi()): 1 ROM banner in 45s and again in 40s after the re-flash, zero assert/abort/panic hits, no re-enumeration of the USB-CDC port. Boots and stays up.

CONSEQUENCE WORTH FLAGGING: dev has been boot-looping on ESP32-S3 for every build from alpha.348 (57177d910, 2026-07-31) through alpha.350. Any tester who flashed one of those got a device that never reaches setup(). Fixed from alpha.351 (3530b9b1).

AC#6 STILL OPEN and unchanged. The bench board is provisioned for a 192.168.1.x network while this host sits on 192.168.88.x, so it is not reachable for a runtime SNTP inspection, and no DHCP server sending option 42 was involved in this test. What is proven is that the shim now RUNS instead of aborting; what is NOT proven is that a DHCP-offered NTP server is actually refused afterwards and stays refused across a forced lease renewal.

2026-08-25 backlog sweep: code is implemented and committed on this branch; verified by git rather than by the task file. TASK-1068 and TASK-1069 both landed in a7e06f8df; TASK-1052's shim is at platform_esp32.h:234 with the call at networkStuff.ino:92. Every AC except the on-device one is met.

Left In Progress deliberately. The remaining AC needs ESP32 hardware in the loop, which no amount of code reading can substitute for, and flipping the task to Done would claim a verification that never happened.

2026-10-03 read-only research plus an independent adversarial verification. Reports are kept out of the repo at %LOCALAPPDATA%/OTGW-capture/research-2026-10-03/task1052-*.md.

FINDING, confirmed twice in source and in both current ELFs (esp32 and esp32-combo, alpha.404). The lwIP flag that lets DHCP option 42 replace SNTP servers, sntp_set_servers_from_dhcp, is a zero-initialised .bss byte (IDF lwip/apps/sntp/sntp.c:246-248). It is read only in dhcp_set_ntp_servers (:834) and written only by sntp_servermode_dhcp (:784-792). In the linked image the only caller of esp_sntp_servermode_dhcp is startWiFi, and it passes 0. esp_netif_sntp_init, the only IDF code that can set the flag to 1, is not linked.
Consequences:
- On ESP32, DHCP-supplied NTP servers are rejected with or without the shim; the shim writes 0 over 0.
- CONFIG_LWIP_DHCP_GET_NTP_SRV=y only makes the client request option 42; it does not let option 42 in.
- The 'leaks heap on every lease renewal' premise does not hold on this lwIP (2.2.0 development snapshot). The path copies into a static table, every timer rearm cancels first, and Kiss-of-Death exclusion is present. This is from source only, with no bench data.
- The stale premise appears in networkStuff.ino:87-89, platform_esp32.h:212-214 and :232-233, and in this task's description.
- AC#6 as written passes with or without the shim, so there is no OLD failure to reproduce. A real verdict needs a positive control: force the flag to 1 on a test build and show option 42 taking over slot 0.
- No option-42 source exists today. The router's config is unknown (a zero-packet check is to read the router config). The laptop hotspot (ICS) cannot set DHCP options. A wired rig (laptop Ethernet to the bench W5500 plus a small Python DHCP responder) is possible but has unverified preconditions.
DECISION FOR THE MAINTAINER:
(a) accept the binary single-writer proof as AC#6 evidence and fix the stale comments; or
(b) build the positive-control bench (test-only keys N/R/K, sntp_restart, an option-42 source).
Separately: keep the shim as insurance against a future framework that defaults the flag to 1 (then guard it with sys_thread_tcpip(LWIP_CORE_IS_TCPIP_INITIALIZED), because it asserts if tcpip never initialised), or remove it as dead code. Its only observed runtime effect so far was the alpha.348-350 boot loop.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of 1.x TASK-1050 to 2.0.0. The platformIgnoreDhcpNtp() shim keeps DHCP-supplied NTP servers (option 42) out of SNTP, so only our own server from startNTP() is used. Research on 2026-10-03 (one pass plus an independent verification) showed that on this framework lwIP's reject flag already starts at 0 and nothing sets it to 1. The shim therefore pins the default instead of changing it, and the premise that the DHCP path leaks heap on every renewal does not hold on ESP32 (lwIP 2.2.0 development snapshot, from source).
alpha.405 keeps the shim as insurance against a framework that flips the default. It now skips the call while the TCP/IP stack is not initialised, which removes the tcpip_callback assert path, and it corrects the comments.

Evidence:
- AC#1-#5: as recorded at port time.
- AC#6: the maintainer accepted the binary proof on 2026-10-03. In both ELFs the flag is a .bss byte, the only call passes 0 (from startWiFi), and esp_netif_sntp_init is not linked.
- AC#7: build.bat green for esp32, esp32-classic and esp32-combo (alpha.405+8fdc3b3, SUCCESS lines and fresh .ino.bin and .littlefs.bin, 19:53-20:01), and evaluate.py --quick 78 checks with 0 failures and 0 warnings.

Research reports are kept out of the repo at %LOCALAPPDATA%/OTGW-capture/research-2026-10-03/task1052-*.md.
<!-- SECTION:FINAL_SUMMARY:END -->
