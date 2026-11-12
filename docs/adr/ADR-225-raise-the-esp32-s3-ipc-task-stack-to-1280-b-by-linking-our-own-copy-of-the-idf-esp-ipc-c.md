---
id: "ADR-225"
title: "Raise the ESP32-S3 IPC task stack to 1280 B by linking our own copy of the IDF esp_ipc.c"
status: "Accepted"
date: "2026-11-12"
binding: false
gate: "tests/test_esp_ipc_override_gate.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "build"
  - "sdkconfig"
  - "esp-idf"
  - "ipc"
  - "boot-stability"
  - "freertos"
aliases:
  - "ipc1 stack overflow"
  - "ipc0 panic during filesystem OTA"
  - "CONFIG_ESP_IPC_TASK_STACK_SIZE"
  - "esp_ipc_stack.c"
  - "TASK-1763 (a)"
components:
  - "IPC task creation (src/OTGW-firmware/esp_ipc_stack.c, a copy of IDF esp_ipc.c)"
symbols:
  - "CONFIG_ESP_IPC_TASK_STACK_SIZE"
  - "OTGW_IPC_TASK_STACK_SIZE"
  - "ipc_task"
  - "esp_ipc_init"
  - "esp_ipc_call_blocking"
  - "gpio_install_isr_service"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-225 Raise the ESP32-S3 IPC task stack to 1280 B by linking our own copy of the IDF esp_ipc.c

Terms used below: IPC (inter-processor call), IDF (ESP-IDF, the Espressif IoT Development Framework), ESP (Espressif's chip family prefix), OTGW (OpenTherm Gateway), DM (Discord direct message), GPIO (general-purpose input/output), ISR (interrupt service routine), RAM (random-access memory), DRAM (data RAM, internal), IRAM (instruction RAM, internal), PSRAM (pseudo-static RAM, external), OTA (over-the-air update), ELF (the firmware image with symbols), LTO (link-time optimization), PIC (the Microchip PIC microcontroller on the OTGW Classic).

## Status

Accepted, 2026-11-12.

**Decision Maker:** User: Robert van den Breemen (maintainer), in three steps:

- 2026-11-10 06:40, via AskUserQuestion in the coordinator's session: "(c) Both" for TASK-1763. (b), the early GPIO ISR install, shipped in beta.168 (commit 0d7aecf34). (a), a larger IPC stack, "afterwards, with an ADR": this record.
- 2026-11-12, Discord DM 1570489845019840593 (relayed by the coordinator): "A", accept this ADR, IPC task stack at least 1280 B.
- 2026-11-12, Discord DM 1570519798071300168 (relayed by the coordinator): "C", the mechanism: our own copy of IDF 5.5.1 `esp_ipc.c` with a 1280 B stack, linked in; fallback B (a separate PlatformIO packages directory for the library rebuild) if the link override fails.

The first version of this record (2026-11-10) proposed a library rebuild through pioarduino's `custom_sdkconfig`. It was amended on 2026-11-12, while still Proposed, after a source check showed that rebuild rewrites a package shared by every worktree on the build machine (see Alternatives Considered).

## Status History

```yaml
status_history:
  - date: 2026-11-10
    status: Proposed
    changed_by: "Agent: otgw-firmware-030509"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-12
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Accepted by Robert (DM 1570489845019840593: A, raise to >= 1280 B; DM 1570519798071300168: C, own esp_ipc.c copy, fallback B), relayed by the coordinator; amended mechanism written by otgw-firmware-95"
    changed_via: adr-kit lifecycle
```

## Context

**The failure on `ipc1`.** On bench .61 (OTGW32, an ESP32-S3 without PSRAM, beta.162), 1 of 10 reboots ended in a panic. The coredump, decoded by otgw-firmware-4c and recorded in TASK-1763, proves that the stack of `ipc1` overflowed. `ipc1` is the IPC task on core 1: the FreeRTOS task that IDF uses to run a function on that core for another task.

- The exception was exccause 0x41 (DebugException), raised by the FreeRTOS end-of-stack watchpoint.
- `ipc1`'s stack pointer was 52 B below the bottom of its 1020 B stack.
- A level-1 interrupt frame had been written past the end.

**Why it runs on `ipc1`.** Arduino installs the GPIO ISR service on the first `attachInterrupt()` of a boot (`esp32-hal-gpio.c:211-240`). IDF then registers the interrupt through `esp_ipc_call_blocking()`, on every multicore build, even when the caller is already on that core (`esp_driver_gpio/src/gpio.c:624-634`; `esp_system/esp_ipc.c:121-164`). So `esp_intr_alloc()` and `heap_caps_malloc()` run on `ipc1`'s stack, and an interrupt taken there does not fit.

**The thin margin on `ipc0`.** On the OTGW Classic with PIC (.63, beta.217+a7676dc, bench-tester otgw-firmware-09, TASK-1774), a filesystem OTA panicked on `ipc0` in 1 of 9 writes (11%): "Core dump: ipc0 pc=0x4038394f", decoded as `xPortEnterCriticalTimeout` (port.c:478) called from `vTaskSwitchContext` (tasks.c:3653). The web interface file was left at 4096 of 199243 B and the settings were reset. Right after an OTA boot `ipc0` had 88 B free at its low point, against at least 152 B in 74 readings on .61. The crashlog has no exception cause, so a stack overflow is the leading reading, not a proven one. On beta.217 .61 showed `ipc1` at 80 B free in all 74 readings (TASK-1865).

**Where the 1024 B comes from.**

- The Arduino framework libraries for the S3 are prebuilt with `CONFIG_ESP_IPC_TASK_STACK_SIZE 1024` (`framework-arduinoespressif32-libs/esp32s3/qio_qspi/include/sdkconfig.h:1099`).
- The IDF Kconfig default for the S3 is 1280 (`esp_system/Kconfig:662-668`).
- `esp_ipc.c:26-30` creates both tasks with `IPC_STACK_SIZE = CONFIG_ESP_IPC_TASK_STACK_SIZE`, one per core in the loop at `:110-118`. One value sizes `ipc0` and `ipc1`.

**Which IDF the libraries contain.** The libs' `versions.txt` names "esp-idf: release/v5.5 9bb7aa84fe" and their defines carry `IDF_VER "v5.5.1-931-g9bb7aa84fe"`. Their `package.json` says "5.5.0+sha.9bb7aa84fe" while the headers say 5.5.1. So the version number alone cannot tell two release/v5.5 snapshots apart; the commit can. `components/esp_system/esp_ipc.c` at commit 9bb7aa84fe (fetched from GitHub on 2026-11-12) is byte-identical to the local framework-espidf 5.5.1.251215 copy, sha256 `1429b684...414f`.

**How the prebuilt IPC code is linked.** In the prebuilt `libesp_system.a`, the member `esp_ipc.c.obj` defines three global functions: `esp_ipc_call`, `esp_ipc_call_blocking` and `esp_ipc_call_nonblocking` (nm). Everything else is static: `esp_ipc_init` is a constructor (`__attribute__((constructor))`, `esp_ipc.c:104-106`, a `.ctors` entry) and runs only when the member is linked. `ipc_task` is placed in IRAM by its own `ESP_SYSTEM_IRAM_ATTR` (`CONFIG_ESP_SYSTEM_IN_IRAM=1`), not by a pattern in the linker script. A linker takes an archive member only to resolve a symbol that is still undefined, so an object of our own that defines those three functions keeps the member out, and only our constructor creates the tasks.

**Why the existing override does not help.** The project overrides sdkconfig values through `include/sdkconfig.h` (ADR-185). That wrapper only changes what our own translation units see; the IPC tasks are created by prebuilt code.

## Decision

Add `src/OTGW-firmware/esp_ipc_stack.c`, a copy of IDF `components/esp_system/esp_ipc.c` at commit 9bb7aa84fe, the IDF inside the pinned libraries. It differs from upstream only in one block between two marker comments (the OTGW-PATCH markers), which:

- sizes the IPC tasks from `OTGW_IPC_TASK_STACK_SIZE` = 1280 B, the IDF S3 default, instead of the prebuilt `CONFIG_ESP_IPC_TASK_STACK_SIZE` of 1024;
- stops the build with `#error` when the libraries' IDF version is not 5.5.1 or their `CONFIG_ESP_IPC_TASK_STACK_SIZE` is not 1024, so a framework update forces a re-check.

Both `ipc0` and `ipc1` get 1280 B. Mitigation (b) stays in place. The file keeps its Apache-2.0 licence header.

1280 and not more: the worst overflow measured was 52 B (TASK-1763) and the lowest free values are 80 B on `ipc1` and 88 B on `ipc0` (beta.217). 1280 B adds 256 B to each stack, about five times the worst overflow, for 512 B of internal DRAM. A higher value is a new decision if the bench shows less than 256 B of headroom on 1280.

If the build check below fails (the linker still takes the prebuilt member, or LTO moves `ipc_task` code or literals out of IRAM), the fallback is Robert's B: rebuild the libraries with `custom_sdkconfig` in a PlatformIO packages directory of its own for the 2.x line. That fallback gets its own ADR that supersedes this one.

## Decision Contract

### Must

- Link `src/OTGW-firmware/esp_ipc_stack.c` into every ESP32-S3 environment, with a stack of at least 1280 B for both IPC tasks.
- Keep the file equal to IDF `esp_ipc.c` at the pinned commit outside the `OTGW-PATCH` block, and keep its `#error` guards.
- Copy the file again from the new IDF, and re-run the gates, whenever the pioarduino platform or the Arduino libraries move.
- Prove each firmware build that adopts or changes it with `scripts/check_esp_ipc_override.py` on the ELF before it is flashed.
- Keep mitigation (b): the early GPIO ISR install in `setup()`, guarded by `tests/test_gpio_isr_early_install.py`.

### Must Not

- Rely on `include/sdkconfig.h` for this value; it does not reach the prebuilt IPC task.
- Edit the copy outside the `OTGW-PATCH` block.
- Rebuild the shared framework packages with `custom_sdkconfig` in the common PlatformIO directory.

### Exceptions

- None.

### Verification

- Host gate `tests/test_esp_ipc_override_gate.py`: fails when the file is missing (the code from before this ADR), when it differs from upstream outside the block, when the stack is below 1280, when a guard is missing, when the platform pin 55.03.35 moves, or when the installed libs come from another IDF commit. Its mutants run on every test run.
- ELF check `scripts/check_esp_ipc_override.py --elf <elf> [--map <map>]`: the stack argument to `xTaskCreatePinnedToCore` in `esp_ipc_init` is 0x500, `esp_ipc_init` is in `__init_array`, `ipc_task` and all its literals are in IRAM, the IPC data is in internal DRAM, no IPC function is defined twice, and the map does not pull `esp_ipc.c.obj` from `libesp_system.a`. On the beta.217 ELF it fails with "IPC stack is 1024 B, want 1280 B".
- Bench by the bench-tester: `hd_ipc0_stack_min` and `hd_ipc1_stack_min` about 256 B higher than on beta.217 on .61 and .63; at least 12 `/ReBoot` cycles on .61 with a crashlog check per boot; the filesystem OTA on .63 repeated without an `ipc0` panic. The firmware logs the IPC low points after the PIC probe and per OTA chunk, so the log shows which phase uses the stack.
- Build: the RAM line of the build output before and after (512 B more internal DRAM expected).

## Alternatives Considered

- **A: `custom_sdkconfig` in platformio.ini (the first version of this record).** Rejected. In pioarduino 55.03.35 it rewrites the shared package `framework-arduinoespressif32-libs` (`builder/frameworks/espidf.py`, `idf_lib_copy`, about lines 2606-2651). Afterwards every build of a project without `custom_sdkconfig` (another worktree, an unrebased lane, an older commit) deletes both framework packages and downloads them again (`arduino.py:645-648` `check_reinstall_frwrk`, `:491-500`), and the next custom build recompiles all IDF libraries. On a machine where several agents build one at a time from separate worktrees, that costs every lane a reinstall, and the first library rebuild is unmeasured but likely longer than the build slot allows.
- **B: the same rebuild in a separate `PLATFORMIO_PACKAGES_DIR` for 2.x.** Kept as the fallback. It isolates the rebuild from other worktrees, but costs several gigabytes of duplicate toolchains, a change in build.py, and the unmeasured rebuild time.
- **Only (b), the early GPIO ISR install (beta.168).** Kept, but not enough on its own: the install still runs on the 1024 B stack, and the `ipc0` margin on .63 is not touched by it.
- **The `include/sdkconfig.h` wrapper (ADR-185).** Rejected: it only changes headers of our own translation units.
- **Avoid IPC by calling `attachInterrupt()` from core 1.** Rejected: IDF 5.5.1 still hands the call to `ipc1` unless the caller is the IPC task itself (`esp_ipc.c:134-139`).
- **Switch to the ESP-IDF framework with Arduino as a component.** Rejected: it changes the whole build for one setting.
- **Do nothing beyond (b).** Rejected by Robert's choices "(c) Both" and "A".

## Consequences

**Positive:**

- Both IPC tasks get the IDF default stack of 1280 B, 256 B more each, which removes the measured `ipc1` overflow and widens the `ipc0` margin seen on .63.
- A normal build: no library rebuild, no change to the shared PlatformIO packages, no extra build time.
- The change is one file plus its gates, easy to remove when the prebuilt libraries ship 1280 themselves (the `#error` on 1024 then fires and points here).

**Negative:**

- We carry a copy of IDF code. It must follow the pinned IDF exactly; the hash gate and the `#error` guards stop a silent drift, but every platform update needs a manual re-copy.
- The override depends on how the linker picks archive members. If a later IDF adds another global symbol to `esp_ipc.c.obj` that something references, the member is pulled again; its three functions then clash with ours and the link fails with a multiple-definition error rather than shipping two sets of IPC tasks. The `#error` guards make that later IDF a deliberate re-copy anyway, and the ELF check (one definition each, stack argument 0x500) checks each build.
- The file is compiled with the project's flags, including `-flto`, unlike the prebuilt object. A single-file compile with the libs' own flags gives the same sections as the prebuilt object (`.iram1.2`, `.ctors`, `.dram1.*`, 3 global symbols, stack 0x500); whether LTO keeps code and literals in IRAM is checked on each ELF.
- About 512 B (0.5 KB) more internal DRAM (+25% per IPC stack), also on the OTGW32 without PSRAM (heap budget, TASK-1736/1757), to be shown with the RAM line.

## Open Questions

- [x] Acceptance of the build cost by Robert. — **Answered 2026-11-10 by User: Robert van den Breemen (relayed by the `Coordinator OTGW` session):** Robert accepts the build cost, 2026-11-10 ~17:00, via AskUserQuestion in the coordinator's session: 'Accept, build it now'
- [x] 1280 (the IDF default) or more? — **Answered 2026-11-12:** 1280, see the Decision (worst overflow 52 B, lowest free 80 B and 88 B, 256 B added per stack).
- [x] Library rebuild or link override? — **Answered 2026-11-12 by User: Robert van den Breemen, DM 1570519798071300168 (relayed by the coordinator):** C, the link override, with B as fallback.

## Related Decisions

- **ADR-185 (the include/sdkconfig.h wrapper for the NimBLE host trims):** the existing way to override sdkconfig values for our own translation units. This ADR explains why that way cannot change the IPC stack.

## References

- TASK-1763 (coredump proof, mitigation (b)); TASK-1865 (beta.217 bench, `ipc1` 80 B); TASK-1774 (`ipc0` panic during the filesystem OTA on .63); TASK-1870 (this change); TASK-1736 and TASK-1757 (heap budget on the OTGW32 without PSRAM).
- `framework-arduinoespressif32-libs/versions.txt` (esp-idf release/v5.5 9bb7aa84fe), `esp32s3/flags/defines` (`IDF_VER "v5.5.1-931-g9bb7aa84fe"`), `esp32s3/qio_qspi/include/sdkconfig.h:1099-1101`.
- ESP-IDF at 9bb7aa84fe: https://github.com/espressif/esp-idf/blob/9bb7aa84fe/components/esp_system/esp_ipc.c (sha256 `1429b6843566356758c9e4c80ecfcdc4c01c1bf3ad20dc4bfe534d41ece7414f`), `:26-30`, `:104-118`, `:121-164`; `components/esp_system/Kconfig:662-668`; `components/esp_driver_gpio/src/gpio.c:624-634`.
- pioarduino platform 55.03.35: `builder/frameworks/espidf.py` (`idf_lib_copy`), `builder/frameworks/arduino.py:491-500`, `:616-648`.
- Arduino core 3.3.5: `cores/esp32/esp32-hal-gpio.c:211-240`.
- `platformio.ini:111` (pioarduino 55.03.35); `include/sdkconfig.h` (ADR-185).

## Enforcement

`tests/test_esp_ipc_override_gate.py` runs in `python scripts/run_tests.py` and fails on the code from before this ADR. `scripts/check_esp_ipc_override.py` runs on each built ELF before the bench request. The `#error` guards in `esp_ipc_stack.c` stop every build on a different IDF version or prebuilt IPC setting. The record stays guideline-level (`binding: false`): the gates check the mechanism, not a code pattern across the tree.
