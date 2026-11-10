---
id: "ADR-225"
title: "Raise the ESP32-S3 IPC task stack to at least 1280 B by rebuilding the Arduino framework libs with a custom sdkconfig"
status: "Proposed"
date: "2026-11-10"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "build"
  - "sdkconfig"
  - "esp-idf"
  - "ipc"
  - "boot-stability"
  - "freertos"
aliases:
  - "ipc1 stack overflow"
  - "CONFIG_ESP_IPC_TASK_STACK_SIZE"
  - "custom_sdkconfig"
  - "TASK-1763 (a)"
components:
  - "Build configuration of the ESP32-S3 framework libraries (IPC task stack size)"
symbols:
  - "CONFIG_ESP_IPC_TASK_STACK_SIZE"
  - "ipc_task"
  - "esp_ipc_call_blocking"
  - "gpio_install_isr_service"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-225 Raise the ESP32-S3 IPC task stack to at least 1280 B by rebuilding the Arduino framework libs with a custom sdkconfig

Terms used below: IPC (inter-processor call), GPIO (general-purpose input/output), ISR (interrupt service routine), IDF (ESP-IDF, the Espressif IoT Development Framework), RAM (random-access memory), DRAM (data RAM, the internal RAM for data), PSRAM (pseudo-static RAM, external).

## Status

Proposed, 2026-11-10.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-11-10 06:40. Via AskUserQuestion in the coordinator's session (relayed to agent otgw-firmware-030509, who wrote this ADR) he chose "(c) Both" for TASK-1763:

- (b): install the GPIO ISR service early, as a quick mitigation. This is in beta.168 (commit 0d7aecf34, lane-1763b).
- (a): a larger IPC stack, "afterwards, with an ADR". This record proposes how; it is not accepted.

## Status History

```yaml
status_history:
  - date: 2026-11-10
    status: Proposed
    changed_by: "Agent: otgw-firmware-030509"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

**The failure.** On bench .61 (OTGW32, an ESP32-S3 without PSRAM, beta.162), 1 of 10 reboots ended in a panic. The coredump was decoded by otgw-firmware-4c and is recorded in TASK-1763. It proves that the stack of `ipc1` overflowed. `ipc1` is the IPC (inter-processor call) task on core 1: the FreeRTOS task that ESP-IDF uses to run a function on that core for another task.

- The exception was exccause 0x41 (DebugException), raised by the FreeRTOS end-of-stack watchpoint.
- `ipc1`'s stack pointer was 52 B below the bottom of its 1020 B stack.
- A level-1 interrupt frame had been written past the end.

**Why it runs on `ipc1`.** Arduino installs the GPIO ISR service on the first `attachInterrupt()` of a boot (`esp32-hal-gpio.c:211-240`). IDF then registers the interrupt through `esp_ipc_call_blocking()`. It does this on every multicore build, even when the caller is already on that core (`esp_driver_gpio/src/gpio.c:624-634`; `esp_system/esp_ipc.c:121-164`). So `esp_intr_alloc()` and `heap_caps_malloc()` run on `ipc1`'s stack, and an interrupt taken there does not fit.

**Where the 1024 B comes from.**

- The Arduino framework libraries for the S3 are prebuilt with `CONFIG_ESP_IPC_TASK_STACK_SIZE 1024` (`framework-arduinoespressif32-libs/esp32s3/qio_qspi/include/sdkconfig.h:1099`). The package is 5.5.0+sha.9bb7aa84fe, the same sha as on the device's boot line.
- The IDF Kconfig default for the S3 is 1280 (`esp_system/Kconfig:662-668`).
- `esp_ipc.c:26-30` creates the task with `IPC_STACK_SIZE = CONFIG_ESP_IPC_TASK_STACK_SIZE`.

**Why the existing override does not help.** The project already overrides sdkconfig values through `include/sdkconfig.h` (ADR-185, `-Iinclude` in platformio.ini). That wrapper only changes what our own translation units see. The IPC task is created by precompiled IDF code inside the framework libraries, so the wrapper cannot change its stack size. This follows from how the libraries are built; it has not been tested with a build.

**What (b) does and does not do.** Mitigation (b) (TASK-1763, beta.168) moves the one install to before `startWiFi()`, which lowers the chance of an interrupt during it. The install still runs on the 1024 B stack.

## Decision

Rebuild the ESP32-S3 framework libraries with `CONFIG_ESP_IPC_TASK_STACK_SIZE` of at least 1280 (the IDF S3 default), using pioarduino's `custom_sdkconfig` option in platformio.ini. Then `ipc1` and `ipc0` both get the larger stack. Mitigation (b) stays in place. The cost is a longer, heavier build, accepted for this one boot-stability setting.

`custom_sdkconfig` is pioarduino's "hybrid compile": it rebuilds the framework libraries from ESP-IDF with the given sdkconfig lines. The project uses pioarduino platform 55.03.35 (platformio.ini:111).

Before acceptance the rebuild must show:

- the same IDF version and Arduino core sha as today (5.5.1, 9bb7aa84fe);
- the rest of the sdkconfig unchanged, with a full diff;
- reproducible builds with build.bat, CI and the build archive;
- measured build time and disk use;
- the RAM cost: 256 B more stack per core, so 512 B of internal DRAM, also on the OTGW32 without PSRAM. Show it with the RAM line of the build output, before and after.

## Decision Contract

### Must

- Set `CONFIG_ESP_IPC_TASK_STACK_SIZE` ≥ 1280 in the framework libraries linked into every S3 environment.
- Prove the larger stack on the device: log `ipc1`'s stack size or high-water mark at boot, for example `uxTaskGetStackHighWaterMark(xTaskGetHandle("ipc1"))`.
- Keep mitigation (b): the early GPIO ISR install in `setup()`, guarded by `tests/test_gpio_isr_early_install.py`.
- Include the full sdkconfig diff, rebuild against prebuilt, in the change that adopts it.

### Must Not

- Rely on `include/sdkconfig.h` for this value; it does not reach the precompiled IPC task.
- Let the rebuild change other sdkconfig values without saying so.

### Exceptions

- None.

### Verification

- Bench: at least 12 `/ReBoot` cycles on .61, with a crashlog check per boot. The old rate was 1 panic in 10 on beta.162. Read the `ipc1` high-water mark before and after.
- Build: the RAM line of the build output before and after, plus the sdkconfig diff.

## Alternatives Considered

- **Only (b), the early GPIO ISR install (beta.168).** Kept, but not enough on its own: it lowers the chance, while the install still runs on the 1024 B stack, so the cause stays (TASK-1763 coredump).
- **The `include/sdkconfig.h` wrapper (ADR-185).** Rejected: it only changes headers of our own translation units, not the precompiled `esp_ipc.c` that creates the task with `IPC_STACK_SIZE`.
- **Avoid IPC by calling `attachInterrupt()` from core 1.** Rejected: IDF 5.5.1 still hands the call to `ipc1` unless the caller is the IPC task itself (`esp_ipc.c:134-139`). `setup()` already runs on core 1.
- **Switch to the ESP-IDF framework with Arduino as a component.** Rejected for now: it changes the whole build, all libraries and build scripts, for one setting.
- **Do nothing beyond (b) and accept a rare boot panic.** Rejected by Robert's choice "(c) Both". A boot panic on a heating controller costs a reboot and leaves an "Exception/panic" reset in the field.

## Consequences

**Positive:**

- The cause of the TASK-1763 boot panic is removed: the IPC task gets 256 B more stack, which is the IDF default for the S3.
- It gives a controlled way to set other framework sdkconfig values later, each with its own ADR.

**Negative:**

- The build becomes heavier and slower: the framework libraries are rebuilt, and ESP-IDF must be available. This is not yet measured.
  - Risk: CI or a contributor's machine cannot build.
  - Mitigation: measure first, document the requirement, and keep build.bat as the single entry point.
- The rebuild may change more than the one value.
  - Mitigation: the full sdkconfig diff is required (see the Decision Contract).
- It costs about 512 B more internal DRAM (two IPC stacks, +256 B each), also on the OTGW32 without PSRAM (heap tiers, TASK-1736/1757). To be shown with the RAM line.

## Open Questions

- [ ] Does pioarduino 55.03.35 `custom_sdkconfig` rebuild the libs reproducibly with build.bat and CI, and how long does it take? (measurement)
- [ ] 1280 (the IDF default) or more (e.g. 1536)? Decide after reading the high-water mark on the bench.
- [ ] Acceptance of the build cost by Robert.

## Related Decisions

- **ADR-185 (the include/sdkconfig.h wrapper for the NimBLE host trims):** the existing way to override sdkconfig values for our own translation units. This ADR explains why that way cannot change the IPC stack, and adds a library rebuild for that one value.

## References

- TASK-1763 (coredump proof, mitigation (b), plan); TASK-1736 and TASK-1757 (heap budget on the OTGW32 without PSRAM).
- `D:\DevData\platformio\packages\framework-arduinoespressif32-libs\esp32s3\qio_qspi\include\sdkconfig.h:1099` (`CONFIG_ESP_IPC_TASK_STACK_SIZE 1024`); `...\esp32s3\sdkconfig` (`CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y`).
- ESP-IDF 5.5.1: `components/esp_system/esp_ipc.c:26-30`, `:121-164`; `components/esp_system/Kconfig:662-668`; `components/esp_driver_gpio/src/gpio.c:624-634`.
- Arduino core 3.3.5: `cores/esp32/esp32-hal-gpio.c:211-240`.
- `platformio.ini:111` (pioarduino 55.03.35); `include/sdkconfig.h` (ADR-185).
- Coredump files: `OTGW-build-archive\coredumps\task-1763-beta162-61\` (local, not in git).

## Enforcement

There is no machine-checkable surface until the build change exists. A check is added with the adopting change, for example a `require_pattern` for `CONFIG_ESP_IPC_TASK_STACK_SIZE` in the `custom_sdkconfig` of platformio.ini.
