---
id: TASK-1870
title: >-
  ADR-225: raise the ESP32-S3 IPC task stack (ipc0 and ipc1) to at least 1280 B
  via a custom_sdkconfig framework lib rebuild
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T18:15:00Z'
updated_date: '2026-11-12T18:22:00Z'
labels:
  - firmware
  - build
  - adr
dependencies: []
priority: high
ordinal: 946000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert chose A on ADR-225 (DM 1570489845019840593, 2026-11-12, relayed by the coordinator): accept it, IPC task stack >= 1280 B. Evidence: TASK-1763 (ipc1 overflow by 52 B, beta.162), TASK-1865 bench on beta.217 (ipc1 low point 80 B free in all 74 readings, ipc0 152-232 B), TASK-1774 (ipc0 panic in xPortEnterCriticalTimeout <- vTaskSwitchContext during the FS OTA on .63, beta.217+a7676dc). Work: accept ADR-225 through the adr-kit gates with Robert's decision as the User Decision, post the plan in #alpha-testing first (build tooling change), then add custom_sdkconfig CONFIG_ESP_IPC_TASK_STACK_SIZE to platformio.ini, rebuild, bench.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-225 Accepted through the adr-kit gates (Completeness, Evidence, Clarity, Consistency), Robert's DM 1570489845019840593 recorded as the User Decision
- [ ] #2 Plan posted in #alpha-testing before the build tooling change lands (message link in the notes)
- [ ] #3 platformio.ini sets CONFIG_ESP_IPC_TASK_STACK_SIZE >= 1280 through custom_sdkconfig for every S3 environment, with a gate that fails when it is removed (red on the old platformio.ini, green on the fix)
- [ ] #4 Build evidence: IDF version and Arduino core sha unchanged (5.5.1, 9bb7aa84fe), full sdkconfig diff rebuild vs prebuilt showing only the intended change(s), RAM line before/after, build time and disk use measured, build.bat reproducible
- [ ] #5 Bench by the bench-tester: hd_ipc0_stack_min / hd_ipc1_stack_min about 256 B higher than on beta.217 (ipc1 ~336 B), at least 12 /ReBoot cycles on .61 with a crashlog check per boot, and the filesystem OTA on .63 (Classic with PIC) repeated without an ipc0 panic
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Claimed by otgw-firmware-95 (2026-11-12, coordinator assignment after the beta.217 post). Order: plan to #alpha-testing first, then ADR-225 acceptance, then the platformio.ini change and build. Pushes only after the coordinator's OK (freeze rule).

Evidence from 09 on .63 (Classic with PIC, beta.217+a7676dc, evidence C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\b217-63\ 63-repro-*.log): ipc0 panic in 1 of 9 filesystem writes (217: 1 in 7; 203: 0 in 2, so not decided whether 217-specific). Right after an OTA boot hd_ipc0_stack_min = 88 B on .63 (ipc1 80), against >= 152 B on .61 in 74 readings. So on the Classic the thin margin is on ipc0, not only ipc1: ADR-225's single setting covers both (esp_ipc.c:26-30 per the ADR, to be re-checked in the rebuild's IDF source). 09 suggests logging ipc0_min per phase (boot, PIC detection, FS write) on .63 to find the path that uses the stack: taken into the plan as a diagnostic step before the rebuild, so the bench can show which path the +256 B protects.

Blocker found before any build (95, read-only, pioarduino 55.03.35 in D:\DevData\platformio\platforms/espressif32@src-17531e5e458e568c91320bf271c45c44/builder/frameworks): custom_sdkconfig (hybrid compile) writes into the SHARED package framework-arduinoespressif32-libs: espidf.py idf_lib_copy (~:2606-2651) copies the rebuilt .a, sdkconfig.h and memory.ld there and writes a root sdkconfig. After that, any build of an env without custom_sdkconfig (an unrebased lane, an older commit) takes arduino.py:645-648 check_reinstall_frwrk -> deletes both framework packages (:491-500) and downloads them again (platform.json:42, esp32-3.3.5-libs.tar.xz); the next custom build recompiles all IDF libs. Matching per project by sdkconfig.defaults line 1 '# TASMOTA__<md5(custom_sdkconfig+mcu)>' (arduino.py:616-642, espidf.py:695-711). Robert accepted the build cost, not this machine-wide effect, so the mechanism went to him (coordinator DM, options A custom_sdkconfig + lane rule, B separate PLATFORMIO_PACKAGES_DIR, C own esp_ipc.c copy linked ahead of the archive; recommendation C, fallback B).

Checked in the local IDF 5.5.1 (framework-espidf 5.5.1.251215) and the prebuilt libs: esp_ipc.c:26-30 IPC_STACK_SIZE = CONFIG_ESP_IPC_TASK_STACK_SIZE, one task per core created in the loop at :114, so one value sets ipc0 and ipc1; Kconfig:662-668 S3 default 1280; prebuilt qio_qspi/include/sdkconfig.h:1099 = 1024. Feasibility of C: the archive member esp_ipc.c.obj (libesp_system.a, qio_qspi) exports only esp_ipc_call, esp_ipc_call_blocking, esp_ipc_call_nonblocking (nm T); init is esp_ipc_init, static with __attribute__((constructor)) (esp_ipc.c:104-106, .ctors + section startup.esp_ipc_init), so it registers only if the object is linked: if our object defines the three exported symbols, the archive member is never pulled and only our constructor runs. ipc_task is placed in IRAM by ESP_SYSTEM_IRAM_ATTR (= IRAM_ATTR because CONFIG_ESP_SYSTEM_IN_IRAM=1, sdkconfig.h:1069), not by an archive pattern in sections.ld (no esp_ipc entry), so the attribute carries over to our compile. To prove on a build: .map shows our esp_ipc object and no libesp_system.a(esp_ipc.c.obj); boot reports hd_ipc*_stack_min about +256 B. Open: whether the .ctors of our object runs at the same point as the IDF startup constructor (both are global constructors in do_global_ctors); IPC must be up before the first esp_ipc_call, which is the same requirement the original has.
<!-- SECTION:NOTES:END -->
