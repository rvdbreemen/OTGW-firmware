---
id: TASK-1763
title: Boot panic in ipc1 during GPIO ISR install (beta.162, .61, 1 of 10 reboots)
status: In Progress
assignee:
  - '@system32-030509'
created_date: '2026-11-09T19:21:00Z'
updated_date: '2026-11-10T15:25:00Z'
labels:
  - bug
  - crash
  - boot
dependencies: []
priority: high
ordinal: 862000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench OTGW32 .61, 2.0.0-beta.162+5786928, 2026-11-09, TASK-1528 step b run (10 reboots via GET /ReBoot, run-20261109). 1 of 10 reboots ended in "Exception/panic" (crashlog 12:02:44, bootcount 189 -> 190); the other 9 were "Software reset". Robert waived it for the beta.162 public beta (2026-11-09).

Timeline (host log and telnet, local time): boot6 (two-tab trace boot, trace ended 11:53:16; uptime ~11 min, hd_min_free_heap 3592, idle except device/info every 5 s and a read-only telnet). 12:02:20 GET /ReBoot; deferred reboot "Reboot after upgrade" (37 ms); pre-doRestart heap=63152 minHeap=3592 exccause=0; doRestart: flushSettings 1 ms, mqtt disconnect 1 ms, ws close 0 ms, "stopping telnet+otgwstream" 12:02:20.463; one more device/info served at 12:02:20.644 (also in the 9 good reboots); telnet at 12:02:22 still got the old banner; telnet timeouts 12:02:28 and 12:02:34, connection refused 12:02:39 and 12:02:43; back at 12:02:45.8 with "Boot : Exception/panic", boot line exccause=1. That reboot took 25.5 s; the other 9 took 15.2-16.0 s.

Decode (by otgw-firmware-4c, ELF 88650453 from OTGW-build-archive): panic in ipc1, backtrace heap_caps_malloc <- esp_intr_alloc <- gpio_isr_register_on_core_static <- ipc_task, i.e. during GPIO ISR install.

Candidates (not proven): attachInterrupt / attachInterruptArg in src/libraries/OpenTherm/src/OpenTherm.cpp:95 and :100; attachInterrupt in src/OTGW-firmware/s0PulseCount.ino:50. Cause unknown; whether the panic happened in the restart path of the old boot or early in the new boot's setup is not settled by the logs (no serial capture).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Determine from the decode and code where the GPIO ISR install ran (restart path of the old boot vs setup of the new one) and why heap_caps_malloc in esp_intr_alloc panicked, with evidence
- [ ] #2 Reproduce or bound it on the bench: repeated /ReBoot cycles (including after a long uptime and a low-heap boot) with serial capture; count panics per N reboots
- [ ] #3 Fix, or a decision with Robert, with before/after panic counts
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Robert 2026-11-10 06:40 (AskUserQuestion in the coordinator's session, relayed): '(c) Both': (b) now as a quick mitigation in the next beta, (a) afterwards via an ADR.
(b) Early GPIO ISR install (this lane, lane-1763b on origin/dev 956f7748a):
1. platform_esp32.h: inline platformGpioIsrServiceInstallEarly() = gpio_install_isr_service((int)ARDUINO_ISR_FLAG), the same flags as Arduino's own install (esp32-hal-gpio.c:220), so the later attachInterrupt() gets ESP_ERR_INVALID_STATE, which Arduino treats as installed (:221).
2. OTGW-firmware.ino setup(): call it right after bootHeapPhase('setup-early'), before startWiFi() and before the first attachInterrupt() (initOTDirect -> OpenTherm::begin -> OpenTherm.cpp:95; initS0Count -> s0PulseCount.ino:50). An error other than INVALID_STATE is logged, and Arduino retries the install at the first attach.
3. Host test tests/test_gpio_isr_early_install.py: source order in setup(), the helper's flags, and a host build of the REAL Arduino __attachInterruptFunctionalArg with the REAL helper (IDF calls stubbed): with the early call the one install runs in the early call; in the old order it runs inside the first attach. Red on the old code, green on the new.
4. Prerelease bump (number from the coordinator), build only after the coordinator's go, bench check (one boot, no regressions: OT-Direct frames, S0 if configured), then /ReBoot cycles (AC2) to compare the panic rate with beta.162 (1/10).
Limit: (b) lowers the chance (fewer interrupts before WiFi); the install still runs on ipc1's 1024 B stack. (a) removes the cause.
(a) IPC stack >= 1280 B (IDF S3 default) via custom_sdkconfig / framework libs rebuild: an ADR (Proposed) via /adr-kit:adr first, then a separate change.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 ~04:25, coredump decode (session 4c6667da, slot from the coordinator): one read-only GET /api/v2/device/coredump on .61 (no reboot): 10,596 B, header X-Coredump-App-Sha256 886504532 = prefix of sha256 of the beta.162+5786928 ELF (886504532221f56a44fd289c6bc07eeb1f162c34382e7d09f60e545ab7e86cd1), so the ELF matches. esp-coredump 1.18.1 (PIO penv) with tool-xtensa-esp-elf-gdb: exccause 0x41 DebugException, EPC6 0x40383bfa, crashed task ipc1 (TCB 0x3fcbc338), not in interrupt context. Backtrace: _frxt_int_enter portasm.S:127 <- _xt_lowint1 xtensa_vectors.S:1225 <- vPortExitCriticalSafe (portmacro.h:596) / multi_heap_internal_unlock multi_heap.c:176 <- multi_heap_malloc(size=8) multi_heap_poisoning.c:265 <- heap_caps_malloc(8, caps 0x804) heap_caps.c:84 <- esp_intr_alloc_intrstatus_bind(source=16, handler gpio_intr_service) intr_alloc.c:574 <- esp_intr_alloc intr_alloc.c:728 <- gpio_isr_register_on_core_static gpio.c:608 <- ipc_task esp_ipc.c:84 <- vPortTaskWrapper. gdb on the saved core: ipc1 pxStack 0x3fcbbef4, pxEndOfStack 0x3fcbc2f0 (1,020 B), current SP 0x3fcbbec0 = 52 B below pxStack. Words at pxStack: 0xa5a5a5a5 fill except 0x00060c23 at pxStack+8 (a saved PS-like value, cf. EPS6 0x60c30): the level-1 interrupt frame was written past the bottom of ipc1's stack and the end-of-stack watchpoint (CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y) fired. VERDICT: ipc1 stack overflow (CONFIG_ESP_IPC_TASK_STACK_SIZE=1024 in the precompiled arduino-libs 5.5.0+sha.9bb7aa84fe; IDF default on S3 is 1280); not heap corruption, not OOM. Files (local, not in git; coredump.bin may hold RAM content): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-build-archive\coredumps\task-1763-beta162-61\ (decode.txt, cmds.gdb, coredump.bin, core.elf).

DECISION FOR ROBERT (not implemented): (a) raise the IPC task stack to 1280 B or more by rebuilding the Arduino libs with custom_sdkconfig (a build tooling change, so an ADR first): fixes the cause, costs a libs rebuild in the build chain and RAM for two IPC stacks (+256 B each at 1280). (b) install the GPIO ISR service early, before startWiFi, so the ISR allocation runs while few interrupts are pending: smaller chance, the cause stays (any later cross-core ISR install can still overflow). Recommendation from the evidence: (a), because only (a) removes the overflow; (b) at most as a stopgap. The project sdkconfig.h wrapper of ADR-185 does not help: the stack size is compiled into the precompiled esp_ipc.c.

2026-11-10 16:23, second case DECODED (session 4c6667da): .63 (OTGW Classic, S3 Mini Pro with PSRAM) crashlog '2026-11-10 06:43:01 - reboot cause: Exception/panic (4)', 'Core dump: ipc0 pc=0x4037caec elf=a4485854 bt=...!' (corrupted). elf a4485854 = 2.0.0-beta.167+ddf8498 (the published public beta; ELF from OTGW-build-archive, sha256 a44858545960759d..., coredump header X-Coredump-App-Sha256 a44858545: match). One read-only GET /api/v2/device/coredump (30,756 B). esp-coredump 1.18.1: exccause 0x41 DebugException, EPC6 0x4037caef, crashed task ipc0 (TCB 0x3fcbbd80), not in interrupt context. Backtrace: _xt_lowint1 xtensa_vectors.S:1207 <- (frame lost, 0x40060030) <- heap_caps_malloc_base(8, caps 0x804) heap_caps_base.c:202 <- heap_caps_malloc heap_caps.c:84 <- esp_intr_alloc_intrstatus_bind(source=8, flags=1032, handler r_rwbtdm_isr_wrapper_hack) <- esp_intr_alloc <- btdm_intr_alloc (BLE controller init) <- ipc_task esp_ipc.c:84. gdb: ipc0 pxStack 0x3fcbb93c, pxEndOfStack 0x3fcbbd30 (1,012 B), SP 0x3fcbb950 = only 20 B above the stack bottom; bottom words 0xa5a5a5a5, 0x80376df8 (a heap_caps_malloc_base return address), 0x3fcbba30, 0x3fcbaf9c, 0x00000008 written into the end-of-stack watchpoint area. VERDICT: the same class as the ipc1 case (IPC task stack too small for esp_intr_alloc's malloc path plus a level-1 interrupt frame), here on ipc0 during the BLE controller's interrupt allocation (btdm_intr_alloc), not the GPIO ISR install. Consequence: mitigation (b) (early GPIO ISR install, beta.168) does NOT cover this path; only (a) (larger IPC stacks, ADR-225) covers both. Timing: 06:43:01 is the moment of the TASK-1774 rollback during the coordinator's flash of .63, so this panic is the likely cause of that rollback (inference from the timing, see TASK-1774). Files (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-build-archive\coredumps\task-1763-beta167-63-ipc0\.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-09T22:18:00Z

---

Code analysis, read-only (030509, 2026-11-09; firmware origin/dev 3c1353581; Arduino core + IDF 5.5.1 sources under D:\DevData\platformio\packages; sdkconfig framework-arduinoespressif32-libs/esp32s3/sdkconfig). F = fact from code/logs, G = guess.
(A) Which call can produce this backtrace.
F: esp32-hal-gpio.c:211-240: __attachInterruptFunctionalArg() calls gpio_install_isr_service() only once per boot (static interrupt_initialized, :212/:219-221); later attach/detach only use gpio_isr_handler_add/_remove (:240, :258), which do no esp_intr_alloc and no IPC.
F: IDF esp_driver_gpio/src/gpio.c:528-553 gpio_install_isr_service -> gpio_isr_register :614-641 -> isr_core_id = current core (:624-626) -> esp_ipc_call_blocking(isr_core_id, gpio_isr_register_on_core_static) (:633) -> esp_intr_alloc (:608). So 'heap_caps_malloc <- esp_intr_alloc <- gpio_isr_register_on_core_static <- ipc_task' is the FIRST attachInterrupt of a boot, on core 1 (CONFIG_ARDUINO_RUNNING_CORE=1, sdkconfig:612; ipc1 = the IPC task of core 1).
F: esp_intr_alloc mallocs small internal blocks (intr_alloc.c:172 vector_desc_t, :574 intr_handle_data_t, :611/:634); a NULL return is handled (ESP_ERR_NO_MEM), so a panic INSIDE heap_caps_malloc is not 'out of memory'.
F: sdkconfig: CONFIG_HEAP_POISONING_LIGHT=y (header/tail canaries checked by the allocator), CONFIG_ESP_IPC_TASK_STACK_SIZE=1024, CONFIG_ESP_IPC_USES_CALLERS_PRIORITY=y, CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y, CONFIG_COMPILER_STACK_CHECK_MODE_NORM=y.
G (most likely): a heap-integrity assert in multi_heap (corrupted block met by the malloc), i.e. the corruption happened earlier in that boot, not in the GPIO code. Less likely: ipc1's 1024 B stack (that usually reports 'Stack canary watchpoint triggered (ipc1)' instead). 4c's panic message text (e.g. 'CORRUPT HEAP' / 'assert failed: multi_heap...' / 'Stack protection fault') decides between the two; please add it.
(1) Order at boot, and which boot.
F: setup() order (OTGW-firmware.ino): LittleFS + readSettings, startWiFi (:108 within setup), initOTDirect (:150/:198/:230 by board path) -> OTDirect.ino:962 otMaster.begin(masterISR) -> OpenTherm.cpp:40-79 gptimer, :91-96 pinMode + attachInterrupt(CHANGE) = the first GPIO attach of the boot; then otSlave.begin :973 (OpenTherm.cpp:95/100); later startNTP/startTelnet/startWebserver/startMQTT, then updateRebootCount (OTGW-firmware.ino:540), startWebhookTask, initS0Count -> s0PulseCount.ino:50 (only if S0 is used, and never the first attach: OTDirect comes first). No other attachInterrupt/gpio ISR install in src/ or the bundled libraries (grep).
F: the restart path does no detach/attach: doRestart (helperStuff.ino) -> flushSettings, prepareForReboot (mqtt disconnect, ws close, OTGWstream.stop; debugTelnet.stop() is commented out), delay(2000), platformRestart(). That is why telnet still showed the OLD banner at 12:02:22 (inside the 2 s delay).
G (inference from the timeline, not proven): the panic happened in the NEW boot's setup, at initOTDirect's first attach, before updateRebootCount: the restart at ~12:02:22.5; a first boot 'A' that never answered telnet (timeouts 12:02:28/34); a second boot 'B' (connection refused at 39/43 = WiFi up, telnet not yet started; up 12:02:45.8 with lastreset Exception/panic). bootcount went only 189 -> 190, which fits a boot A that died before :540 and so never counted itself. The extra ~10 s over a normal reboot (25.5 vs 15.2-16.0 s) also fits one aborted boot.
(2) Can an ISR fire before its state is ready?
F: otMaster/otSlave are globals (state set by the constructor, OpenTherm.cpp:12 rxStatus NOT_INITIALIZED); attach happens before rxStatus = IDLE (:109). handleInterrupt (:324-366) with NOT_INITIALIZED takes the default branch and only writes responseTimestamp: benign. masterISR/slaveISR are IRAM (OTDirect.ino:39-40). S0: count zeroed before attach (s0PulseCount.ino:49-50). G: an early edge does not explain a panic inside heap_caps_malloc in ipc1.
(3) Can the restart path call detach/attach while an IPC call runs?
F: no: neither doRestart nor prepareForReboot touches GPIO interrupts, OTDirect or S0 (code above). Runtime re-attaches (OTDirect.ino:2600/2610/2621 mode switch, :3250-3254 GW=R) never reach the IPC path (install-once, (A)).
Suggested next steps (no code before a decision):
- 4c: the panic reason text and the PC/epc in multi_heap.
- A heap integrity check (heap_caps_check_integrity_all) logged right before initOTDirect on a debug build, to see whether the heap is already corrupt at that point.
- Repeat /ReBoot cycles with a serial console (TASK-1763 AC2).

---

created: 2026-11-09T22:22:00Z

---

Update with 4c's decode (relayed by the coordinator, 2026-11-09) and a check of it (030509):
4c: the full backtrace has no abort path (no panic_abort / __assert_func / multi_heap_assert / stack-overflow hook); it ends in _xt_lowint1 (a level-1 interrupt frame on ipc1's stack) -> _frxt_setup_switch. Hypothesis: the end-of-stack watchpoint (CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y, a debug exception) fires when an interrupt frame lands on ipc1's 1024 B stack during heap_caps_malloc in esp_intr_alloc. This replaces my heap-assert guess above (no assert in the trace), so the stack hypothesis is now the leading one.
Checked (F):
- sdkconfig (framework-arduinoespressif32-libs/esp32s3/sdkconfig): CONFIG_ESP_IPC_TASK_STACK_SIZE=1024 and :2548 CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y. The IDF Kconfig default on S3 is 1280 (esp_system/Kconfig:662-668: 'default 1280 if !APPTRACE_ENABLE && IDF_TARGET_ESP32S3').
- Which libs are linked: the package is framework-arduinoespressif32-libs 5.5.0+sha.9bb7aa84fe (package.json), Arduino core 3.3.5. The device's own boot line in the run logs reads 'core=v5.5.1-931-g9bb7aa84fe sdk=v5.5.1-931-g9bb7aa84fe' (boot7C-telnet.txt:113): same sha 9bb7aa84fe. So the linked libs are this package (4c's open point), as far as the sha identifies them.
- Correction to the fix direction 'attachInterrupt from a task on core 1, so gpio_isr_register runs locally without IPC': in IDF 5.5.1 that does not avoid ipc1. gpio.c:629-634 calls esp_ipc_call_blocking() on every multicore build (only CONFIG_FREERTOS_UNICORE runs it inline), and esp_ipc.c:121-164 runs the function inline only when the CALLER IS the ipc task (:134-139). A call from loopTask on core 1 to core 1 still hands it to ipc1 and runs on ipc1's stack. Setup already runs on core 1 (CONFIG_ARDUINO_RUNNING_CORE=1).
Fix directions (opinion, to weigh; no code before a decision):
(a) a larger IPC stack (1280 or more) through a custom sdkconfig, which needs a framework libs rebuild (pioarduino custom_sdkconfig); this changes the build;
(b) install the GPIO ISR service once early in setup, before startWiFi (e.g. a first attach or gpio_install_isr_service at setup-early), when fewer interrupts are active, so an interrupt frame during esp_intr_alloc is less likely. This lowers the probability, it does not remove the cause;
(c) both.
Proof: coredump GET (ADR-210) after the release + esp-coredump with the beta.162 ELF (4c), and repeated /ReBoot cycles to measure the rate before/after.

---

created: 2026-11-10T03:25:00Z

---

PROVEN by coredump (decode by otgw-firmware-4c, 2026-11-10; relayed to 030509): ipc1 stack overflow, not heap corruption, not OOM.
- Coredump from .61, header app sha 886504532 = sha256 of the beta.162+5786928 ELF. exccause 0x41 DebugException, EPC6 0x40383bfa, task ipc1 (not an ISR).
- Backtrace: _frxt_int_enter portasm.S:127 <- _xt_lowint1 xtensa_vectors.S:1225 <- vPortExitCriticalSafe / multi_heap_internal_unlock multi_heap.c:176 <- multi_heap_malloc(size 8) poisoning.c:265 <- heap_caps_malloc(8, 0x804) <- esp_intr_alloc_intrstatus_bind(source 16, gpio_intr_service) intr_alloc.c:574 <- esp_intr_alloc intr_alloc.c:728 <- gpio_isr_register_on_core_static gpio.c:608 <- ipc_task esp_ipc.c:84.
- gdb on the saved core: ipc1 pxStack 0x3fcbbef4, pxEndOfStack 0x3fcbc2f0 (1,020 B), SP 0x3fcbbec0 = 52 B below the stack bottom (checked: 0x3fcbbef4 - 0x3fcbbec0 = 52; 0x3fcbc2f0 - 0x3fcbbef4 = 1020). The bottom words are a5a5a5a5 except 0x00060c23 at pxStack+8 (an interrupt-frame write).
- So the level-1 interrupt frame, taken while malloc unlocks inside esp_intr_alloc during the first GPIO ISR install (the IPC path of my analysis above), landed past ipc1's end. The FreeRTOS end-of-stack watchpoint (CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y) fired.
- This confirms 4c's stack hypothesis; my heap-assert guess is withdrawn.
Files, local only, not committed (device data): 4c's scratchpad cd1763\ (coredump.bin, decode.txt, core.elf, cmds.gdb).
Fix options now rest on proof (decision for Robert, no code yet):
(a) a larger IPC stack (>= the IDF S3 default 1280 B) through custom_sdkconfig / a framework libs rebuild; this removes the cause;
(b) install the GPIO ISR service earlier in setup, before startWiFi; this lowers the chance, but does not remove the cause;
(c) both.
Note: attaching from core 1 does not avoid ipc1 (esp_ipc.c:134-139).

---

created: 2026-11-10T03:27:00Z

---

Location fix for the comment above: the coredump files (coredump.bin, decode.txt, core.elf, cmds.gdb) are kept in D:\Users\Robert\Documents\GitHub\RvdB\OTGW-build-archive\coredumps\task-1763-beta162-61\ (local, not in git; checked present 2026-11-10). 4c's session scratchpad cd1763\ was only temporary.

---

created: 2026-11-10T06:39:00Z

---

Implementation (b), commit f40e1bf73 on lane-1763b (local, beta.168; push only after the bench):
- platformGpioIsrServiceInstallEarly() (platform_esp32.h) called in setup() right after bootHeapPhase('setup-early'), before startWiFi()/initOTDirect()/initS0Count().
- Evidence line at boot: 'boot: ipc stack free min ipc0=.. ipc1=.. B' (platformIpcStackHighWater, uxTaskGetStackHighWaterMark via xTaskGetHandle).
Tests (host, Windows, 2026-11-10):
- tests/test_gpio_isr_early_install.py: 5/5 on the new code; on the old code (OTGW_SETUP_INO/OTGW_PLATFORM_H = HEAD copies) 4 of 5 fail (the 1 that passes is the old-order control case).
- evaluate.py --quick: 92 checks, 0 failed.
- Full scripts/run_tests.py -j 4 (host, no webui): 197 files, 190 passed, 6 failed, 1 skipped. NOT green as a whole. Per file:
  - also red on origin/dev without this change: tests/test_adr_governance.py (test_live_index_complete), tests/test_build_staging.py (3 failures), tests/test_sat_dhw_range.py ('off' not found);
  - green when run alone (failed only under -j 4 load): tests/test_discord_post_payload.py, tests/test_flash_migrate_settings_compare.py, test/host/test_override_reply.py.
Build: build.bat started 07:37 (commit charge 79.3%). Bench .61 pending the coordinator's slot: 12 boots on 168 and 12 boots with a temporary old-order build (same log line, labelled OLD ORDER, not committed), each with the crashlog and the ipc line. Caveat: the high-water mark of a non-crashing boot shows the margin, not proof that (b) prevents the rare nesting; the coredump (-52 B) is the only direct crash measurement.

---

created: 2026-11-10T07:54:00Z

---

Bench .61, 2026-11-10 07:5x-08:53 (slot from the coordinator; OLD order first, beta.168 last). Settings saved before and checked after each OTA: ota_flash.py 'settings: 164 keys, changed: none' twice; final settings diff against the pre-slot dump: none. Start beta.167+e79e495 bootcount 232; end beta.168+283698a bootcount 272, Software reset, MQTT connected.
Builds: beta.168+283698a = commit f40e1bf73 + backlog-only 283698a14 (07:56). OLD-ORDER measurement build = the same tree with the early-install call removed and the log label '(OLD ORDER)', uncommitted, built 08:08; the string 'OLD ORDER' is in that ino.bin only (grep). Not pushed, not posted; the tree was restored (git restore) right after the build.
Results (boot line 'boot: ipc stack free min ... ipc0=.. ipc1=.. B', uxTaskGetStackHighWaterMark at the end of setup()):
- OLD order, script v2, 12 boots (bootcount 247-258): 12x Software reset, 0 panics; ipc1 free minimum 88 B in all 12; ipc0 208-224 B.
- beta.168 (early install), 12 boots (261-272): 12x Software reset, 0 panics; ipc1 free minimum 88 B in all 12; ipc0 144-224 B.
- OLD order, script v1, 12 boots (235-246): no panics in device/info; no ipc lines (v1 telnet bug: it stayed on the old instance's dead socket); 3 boots read bootcount 0 / empty lastreset from an early device/info answer, so for those 3 the reset reason is unknown.
Reading:
- ipc1's deepest point is the same, 88 B free of 1024 B, in both orders. The IPC call chain of the GPIO ISR install (esp_intr_alloc -> heap_caps_malloc) uses ~936 B every time, deterministically.
- A level-1 interrupt frame taken at that depth needs more than 88 B. That is the coredump case: SP 52 B below the bottom.
- So (b) does not change the margin. It can only lower the chance that an interrupt lands at that moment, and that chance is not visible in a high-water mark of a boot without a crash.
- 0 panics in 24 boots (OLD 12 + 168 12) proves nothing about (b): the rate on beta.162 was 1 in 10, and beta.167 already had 0 in 12.
- The measured 88 B margin supports (a) (ADR-225): +256 B (1280) would give ~344 B free at that point.
Evidence files (local, not committed): scratchpad t1763/bench/ (old-*, old2-*, new168-* events/summary/telnet, settings-0/1/2 json).

---
<!-- COMMENTS:END -->
