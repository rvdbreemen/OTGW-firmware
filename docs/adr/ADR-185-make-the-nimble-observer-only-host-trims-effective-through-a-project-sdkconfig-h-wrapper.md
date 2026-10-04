---
id: "ADR-185"
title: "Make the NimBLE observer-only host trims effective through a project sdkconfig.h wrapper"
status: "Accepted"
date: "2026-10-04"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "ble"
  - "nimble"
  - "memory"
  - "build-config"
  - "sdkconfig"
aliases:
  - "NimBLE observer trims"
  - "sdkconfig wrapper"
  - "include_next sdkconfig"
  - "NimBLE host pools"
components:
  - "sdkconfig.h wrapper header"
symbols:
  - "CONFIG_BT_NIMBLE_ROLE_PERIPHERAL"
  - "CONFIG_BT_NIMBLE_MAX_CONNECTIONS"
  - "CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT"
  - "CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT"
  - "CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-185 Make the NimBLE observer-only host trims effective through a project sdkconfig.h wrapper

## Status

Accepted, 2026-10-04.

**Decision Maker:** User: Robert van den Breemen. He chose option (a) of the TASK-1199 research on 2026-10-03, on the condition that it is measured before it is accepted.

## Status History

```yaml
status_history:
  - date: 2026-10-03
    status: Proposed
    changed_by: Agent (Claude, for Robert van den Breemen)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-04
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Maintainer accepted 2026-10-04 after the bench measurement (TASK-1199): +17.5 KB internal heap, +16-19 KB largest block, BLE scan intact"
    changed_via: adr-kit lifecycle
```

## Context

The firmware uses BLE (Bluetooth Low Energy) only as a passive scanner, on ESP32-S3 boards that often have no PSRAM (external pseudo-static memory). SATble reads room-temperature advertisements and never connects or advertises. Even so, the NimBLE host runs as a full stack, with all four roles and three connection slots.

TASK-988 and TASK-995 tried to trim it to an observer through `-D` flags in `platformio.ini`. Those flags never took effect. In NimBLE-Arduino 2.5.1, `src/nimconfig.h:3-4` includes the framework's prebuilt `sdkconfig.h` first on `ESP_PLATFORM`. Every `#ifndef` default after that, the role blocks at `nimconfig.h:180-200` included, sees the macro as already defined and keeps the framework value. TASK-1198 proved this by preprocessing the real compile commands of `NimBLEDevice.cpp` and `ble_hs.c`, then removed the flags. They had produced 660 of the 667 warnings in a full build.

The effective host values today:

| Setting | Value |
|---|---|
| MAX_CONNECTIONS | 3 |
| MAX_BONDS | 3 |
| MAX_CCCDS | 8 |
| ATT_PREFERRED_MTU | 256 |
| TRANSPORT_ACL_FROM_LL_COUNT | 24 |
| MSYS_1 block count | 12 |
| MSYS_2 | 24 blocks of 320 B |
| roles | peripheral, central, broadcaster and observer all on |

Where the memory goes (TASK-1199 research, re-checked where marked):

- **Host vs controller.** The NimBLE-Arduino host, its transport pools and its porting layer are compiled from source in this build. The controller comes prebuilt (`libbt.a(bt.c.obj)` and `libbtdm_app.a`). The link pulls only `bt.c.obj` from `libbt.a` (checked in `firmware.map`), so the host that runs is the NimBLE-Arduino one.
- **The pools are host memory.** They are allocated from internal heap at `NimBLEDevice::init` (`nimble/transport/src/transport.c:366-406`, `porting/nimble/src/os_msys_init.c:154-170`), and neither controller object references them.
- **Their size.** By arithmetic from the pool definitions, not measured, they take about 20.6 KB while BLE runs: ACL (connection data) 6816 B, MSYS_2 7680 B, MSYS_1 3072 B, EVT (event) 2736 B, CMD (command) 260 B.
- **The controller's own settings.** The `CONFIG_BT_CTRL_*` feature set is fixed inside the prebuilt `bt.c.obj`. `ble_max_act` is computed at runtime as MAX_CONNECTIONS + BROADCASTER + OBSERVER (`NimBLEDevice.cpp:927-928`), and IDF (Espressif IoT Development Framework) accepts 1..10 (`bt.c:1806`).
- **What happens when a pool runs dry.** The ESP32-S3 has no host flow control (`esp_nimble_cfg.h:776-780`), so the controller never learns the host's pool sizes. An empty adv pool drops advertisement reports (`esp_nimble_hci.c:254-261`). An empty high-priority EVT pool asserts (`:263-264`).

Why it matters now: under request overload the device's heap low-water mark fell to 504 bytes, and the device went network-deaf for minutes (TASK-1162). Internal heap held by idle BLE pools is headroom the web stack does not have.

## Decision

**User Decision:** a project-owned header named `sdkconfig.h` sits in `include/`, and `-Iinclude` in the shared `[env]` `build_flags` puts that directory ahead of the framework's sdkconfig directory. GCC (the C compiler) therefore finds the wrapper first in every translation unit of all three envs. The wrapper includes the framework file with `#include_next <sdkconfig.h>` and then redefines only host-side NimBLE values for the passive scan: the peripheral and broadcaster roles off, one connection, one bond, no CCCDs (Client Characteristic Configuration Descriptors), a 23-byte ATT (Attribute Protocol) MTU (Maximum Transmission Unit), and smaller ACL and MSYS (system mbuf) pools. The EVT counts and every `CONFIG_BT_CTRL_*` value stay untouched.

| Setting | Framework | Wrapper | Why this value |
|---|---|---|---|
| ROLE_PERIPHERAL, ROLE_BROADCASTER | 1 | 0 | the firmware never advertises or accepts a connection |
| ROLE_CENTRAL | 1 | 1, unchanged | the host does not link without it (below) |
| MAX_CONNECTIONS | 3 | 1 | the minimum |
| MAX_BONDS | 3 | 1 | the floor: 0 does not link (below) |
| MAX_CCCDS | 8 | 0 | every user is guarded by `#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)` |
| ATT_PREFERRED_MTU | 256 | 23 | the ATT default and minimum; it saves no memory |
| TRANSPORT_ACL_FROM_LL_COUNT | 24 | 2 | carries received connection data only |
| MSYS_1 and MSYS_2 block counts | 12 and 24 | 6 and 6 | carry connection traffic only (below) |

Four findings from the implementation shaped these values:

- **The central role stays on.** NimBLE-Arduino 2.5.1 compiles its connection code (`ble_att*.c`, `ble_gattc.c`, `ble_sm*.c`, `ble_l2cap*.c`) only while the central or the peripheral role is on (`#if NIMBLE_BLE_CONNECT`). `ble_gatts.c`, `ble_gap.c`, `ble_hs_conn.c` and `NimBLEDevice.cpp` call into that code either way. With both roles off, the linker resolves 19 of those calls from the IDF host that is prebuilt into `libbt.a`. That object set brings a second copy of the porting layer, and the esp32-combo link fails on multiple definitions of `npl_freertos_*`. With the central role on, NimBLE-Arduino's own objects resolve every host symbol.
- **A disabled role also loses its deprecated alias.** The framework file defines `CONFIG_NIMBLE_ROLE_*` aliases (`sdkconfig.h:1960-1963`). While the alias of a role exists and the role is 0, `nimconfig_rename.h:15-25` redefines the role as empty, and every `#if CONFIG_BT_NIMBLE_ROLE_*` stops compiling. The wrapper removes the aliases of the two roles it turns off.
- **One bond, not zero.** With zero, `ble_store_config.c` no longer defines `ble_store_config_rpa_recs` and `ble_store_config_our_secs`, while `ble_store_nvs.c` still references both (`:635` for the first). Both objects were compiled with that value and their symbol tables compared.
- **The advertising path does not use MSYS.** Advertising reports arrive in the EVT discard pool (`esp_nimble_hci.c:254-261`), and the host hands the report to the scan callback without copying it (`ble_gap.c:1329-1349`, `:1923-1932`). MSYS feeds only connection traffic. Both MSYS pools keep 6 blocks, so no pool disappears from the pool list.

How the wrapper works (an excerpt of `include/sdkconfig.h`):

```c
// sdkconfig.h (project wrapper; see ADR-185)
#pragma once
#include_next <sdkconfig.h>                 // the framework's prebuilt sdkconfig.h
#undef  CONFIG_NIMBLE_ROLE_PERIPHERAL        // deprecated alias: nimconfig_rename.h would empty the role
#undef  CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL 0   // 0, not #undef: nimconfig.h would set an undefined role back to 1
#undef  CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1   // >= 1; ble_max_act follows it at runtime
// ... the broadcaster role, bonds, CCCDs, ATT MTU, ACL and MSYS counts in the same pattern
```

## Decision Contract

### Must

- `include/sdkconfig.h` does `#include_next <sdkconfig.h>` before anything else. After that it only `#undef`s and `#define`s host-side `CONFIG_BT_NIMBLE_*` macros, and removes the deprecated `CONFIG_NIMBLE_ROLE_*` alias of each role it turns off.
- A role that is off is defined as `0`, never left undefined.
- At least one of `CONFIG_BT_NIMBLE_ROLE_CENTRAL` and `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL` stays `1`, so NimBLE-Arduino's own host links without objects from `libbt.a`.
- `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` and `CONFIG_BT_NIMBLE_MAX_BONDS` stay at least 1.
- `-Iinclude` stays in the shared `[env]` `build_flags`, which esp32, esp32-classic and esp32-combo all inherit, so the wrapper's directory precedes the framework's sdkconfig directory.

### Must Not

- Override any `CONFIG_BT_CTRL_*` value, `CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT`, or the EVT pool counts and size.
- Put `CONFIG_BT_NIMBLE_*` overrides back into `platformio.ini` `build_flags`; the framework sdkconfig.h overrides them.
- Edit the vendored NimBLE-Arduino library (`nimconfig.h`) to get there.

### Exceptions

- None.

### Verification

- `python scripts/check_nimble_overrides.py --env <env>` passes for esp32, esp32-classic and esp32-combo, each on a `compile_commands.json` made for that env. It reruns the recorded compile commands of `NimBLEDevice.cpp` and `ble_hs.c` with `-E -dM` and checks three things: the wrapper's values, the `MYNEWT_VAL_*` settings derived from them, and that the guarded values still equal the framework file.
- The link map of each env lists `bt.c.obj` as the only `libbt.a` member, so no second NimBLE host is linked. The same script checks this whenever `.pio/build/<env>/firmware.map` exists.
- On the no-PSRAM OTGW32 bench with BLE on, `scripts/tests/ble_heap_bench.py` measures free internal heap and largest free block from uptime 60 to 180 s, old build against new. It also confirms that a passive scan still reports advertisers, through the any-advertiser count on the telnet debug stream. A pair of runs counts only when both report `ble_running_proven`: a stack that failed to start would otherwise look like a large saving.

## Alternatives Considered

- **(b) Rebuild the Arduino libs with a custom sdkconfig** (pioarduino `custom_sdkconfig`, hybrid compile, `arduino.py:551-553, 912-913`). This keeps host and controller consistent and could trim the controller too. Rejected for now:
  - it compiles IDF v5.5.1.251215 instead of the shipped libs' `release/v5.5@9bb7aa84fe`;
  - it overwrites the shared `framework-arduinoespressif32-libs` package;
  - it forces framework reinstalls across envs and worktrees.
  That cost and drift are not justified while the host-only saving is unmeasured.
- **(c) Runtime knobs only.** `NimBLEDevice::setScanDuplicateCacheSize()` (controller duplicate cache, default 100) and a weak `btInUse()` override, which frees controller memory at boot when BLE is off. Both complement this decision, and neither shrinks the host pools that run while BLE is on.
- **Edit `nimconfig.h` in the vendored library.** Rejected: it breaks the no-library-changes rule and is silently lost on a library update.
- **(d) No change.** Keep about 20.6 KB of host pools while BLE runs. ADR-169 already defaults BLE off on boards without PSRAM, which limits exposure. Rejected because users who accept the consent gate still pay the full pool cost, and the overload stalls (TASK-1162) are a heap-headroom problem.

## Consequences

**Positive:**

- Smaller host pools while BLE runs: ACL from 24 to 2 buffers, MSYS_1 from 12 to 6 blocks and MSYS_2 from 24 to 6 blocks. By the same arithmetic as the Context that is 13544 B of the 20.6 KB (6248 + 1536 + 5760 B). The real saving is measured before acceptance (Open Questions).
- Less static memory and less code: one connection, one bond and no CCCDs shrink the host's static tables, and the peripheral and broadcaster code is compiled out.
- More internal-heap headroom for the web stack under load (TASK-1162).
- One documented place for NimBLE host settings, replacing `-D` flags that only produced warnings, with a repeatable check.

**Negative:**

- *Risk:* the wrapper applies to every translation unit compiled in this build, framework headers included. *Mitigation:* only host-side macros are redefined. Of the sources compiled in this build, only NimBLE-Arduino uses the overridden names; the framework's own BLE library uses them too but is not compiled. The pools are allocated by host code compiled in this build.
- *Risk:* the controller runs with a different configuration although no `CONFIG_BT_CTRL_*` value changes. `NimBLEDevice::init` computes `ble_max_act` at runtime as MAX_CONNECTIONS + BROADCASTER + OBSERVER, so it drops from 5 to 2. *Mitigation:* IDF accepts 1 to 10 (`bt.c:1806`), and a passive scan needs one activity. The bench delta therefore also contains any change in the controller's own allocation, not only the host pools.
- *Risk:* a role combination with central and peripheral both off, or a NimBLE-Arduino update that moves code between its role guards, makes the linker take host objects from the prebuilt `libbt.a`. *Mitigation:* that ends in duplicate `npl_freertos_*` definitions and a failed link, so it cannot ship unnoticed. The link-map check under Verification catches a partial pull.
- *Risk:* the central role keeps code compiled that the firmware never calls. *Mitigation:* none needed for correctness; it is the price of a host that links on its own.
- *Risk:* smaller pools drop advertisement reports. *Mitigation:* the reports come from the EVT discard pool (`esp_nimble_hci.c:254-261`), which keeps its 8 buffers; the ACL and MSYS pools carry connection traffic only. The bench check confirms that a passive scan still reports advertisers.
- *Risk:* a NimBLE-Arduino or framework update changes the include mechanism, and the wrapper silently stops working, as the `-D` flags did. *Mitigation:* `scripts/check_nimble_overrides.py` reruns the preprocessor on the real compile commands and fails when a value no longer arrives. That includes the MSYS_1 count, which reaches the code only through the framework alias `CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT` (`nimconfig.h:253-257`).

## Open Questions

- [x] Measured internal free heap and largest free block after `NimBLEDevice::init`, old against new, on the no-PSRAM OTGW32 with BLE on. This is the acceptance gate. — **Answered 2026-10-04 by User: Robert van den Breemen:** Measured 2026-10-04 on the no-PSRAM OTGW32 bench with BLE active, same procedure for both builds (TASK-1199 notes): internal_free 14896-17296 B on the framework config against 33524-33848 B with the wrapper (about +17.5 KB); largest free block 9716-12788 B against 28660 B (+16-19 KB). A one-off boot-time dip of the since-boot minimum to 2308 B on the wrapper build is recorded for the soak.
- [x] Does a passive scan still report advertisers with the reduced MSYS pools? — **Answered 2026-10-04 by User: Robert van den Breemen:** Yes. Both builds reported 4 valid roster sensors (age 2.9-25.5 s) through /api/v2/sat/ble/discovery after a reboot with BLE active.
- [x] Final pool counts for ACL_FROM_LL, MSYS_1 and MSYS_2: the starting point is the TASK-995 intent (ACL 2, MSYS_1 6); MSYS_2 is to be decided. — **Answered 2026-10-04 by User: Robert van den Breemen:** ACL_FROM_LL_COUNT 2, MSYS_1 6 and MSYS_2 6. Advertising reports use the EVT discard pool, not MSYS, and the EVT counts stay at the framework values.
- [x] Does the wrapper win the include order on all three envs (esp32, esp32-classic, esp32-combo)? — **Answered 2026-10-04 by User: Robert van den Breemen:** Yes. -Iinclude precedes the framework sdkconfig dir in every TU on esp32 (361/361), esp32-classic (361/361) and esp32-combo (362/362); scripts/check_nimble_overrides.py checks this order and fails on a missing -I or on -iquote.

## Related Decisions

- **ADR-169 (PSRAM-Aware BLE Default with a No-PSRAM Instability-Consent Gate)**: decides when BLE runs on boards without PSRAM. This ADR shrinks what BLE costs when it does run.

## References

- TASK-1198: preprocessor proof that the `-D` flags were ignored; the flags removed.
- TASK-1199: research notes (what is compiled vs prebuilt, coupling, options, footprint).
- TASK-1162: heap low-water mark of 504 B under request overload.
- `.pio/libdeps/esp32-combo/NimBLE-Arduino/src/nimconfig.h:3-4` and `:180-200`; NimBLE-Arduino `CHANGELOG.md:108` (2.3.4: "Cleanup redefinition warnings for arduino core 3.3+").
- `nimconfig_rename.h:15-25` (role aliases), `ble_store_nvs.c:635` and `ble_store_config.c:30-72` (bond arrays), framework `sdkconfig.h:1960-1963` (deprecated role aliases).
- `include/sdkconfig.h` (the wrapper), `scripts/check_nimble_overrides.py` (the repeatable preprocessor check), `scripts/tests/ble_heap_bench.py` (the bench measurement).

## Enforcement

The one rule a pattern can check is that NimBLE host overrides do not come back as `-D` flags. `scripts/check_nimble_overrides.py` also fails when the wrapper defines a name outside `CONFIG_BT_NIMBLE_*` or one of the guarded names. Whether a value is truly host-side still needs a reviewer, so the model-based judge stays off and the review step under Verification covers it.

```json
{
  "forbid_pattern": [
    {
      "pattern": "-D\\s*CONFIG_BT_NIMBLE_",
      "path_glob": "platformio.ini",
      "message": "NimBLE host values belong in the sdkconfig.h wrapper (ADR-185); the framework sdkconfig.h overrides -D flags."
    }
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
