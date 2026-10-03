---
id: TASK-1199
title: >-
  Research: make the NimBLE observer-only trims effective on Arduino-ESP32 3.x
  (NimBLE-Arduino 2.5.1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-03 08:58'
updated_date: '2026-10-03 12:52'
labels:
  - ble
  - research
dependencies: []
priority: medium
ordinal: 321000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-988 and TASK-995 meant to trim NimBLE to a passive-scan observer: one connection, no bonds or CCCDs, MTU 23, 2 ACL buffers, 6 MSYS1 blocks, and the peripheral, broadcaster and central roles off. The goal was to free internal DRAM, about 16 KB by TASK-995's own estimate, for the static-file serve gate with BLE on (TASK-978).
The trims never took effect: the framework sdkconfig.h wins (see the cleanup task, preprocessor proof). TASK-995 was closed Done with its ACs 'trims applied, saving measured' unchecked.
The prebuilt BT controller (libbt) is compiled with the sdkconfig values. Any override has to keep the host and the controller consistent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Where the host-side CONFIG_BT_NIMBLE_* values come from for NimBLE-Arduino 2.x on Arduino-ESP32 3.x is documented, from upstream docs or issues plus the nimconfig.h code. So is which of them the prebuilt controller depends on
- [x] #2 Options compared, each with a named controller/host mismatch risk: an sdkconfig wrapper via #include_next, a custom framework sdkconfig rebuild, a library or upstream option, or no change
- [ ] #3 If an option is chosen, the S3 bench with BLE on and no PSRAM shows internal free heap and maxfreeblock before and after in the same scenario, and a BLE scan still reports an advertiser
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
RESEARCH, 2026-10-03. Done by a background research agent, read-only; the key claims were re-checked in the main session (marked [checked]).
Q1, what is compiled and what is prebuilt:
- Compiled from NimBLE-Arduino 2.5.1 sources: the host (nimble/nimble/host/src), the transport pools (nimble/transport/src/transport.c), the porting/NPL layer, and the VHCI glue (esp_port/esp-hci/src/esp_nimble_hci.c).
- Prebuilt: libbt.a(bt.c.obj), the controller glue, and ld/libbtdm_app.a, the controller.
- framework libbt.a also contains a full IDF NimBLE host, but the link pulls ONLY libbt.a(bt.c.obj) [checked: firmware.map lists only libbt.a(bt.c.obj)]. So the NimBLE-Arduino host is the one that links.
Q2, coupling between host and controller:
- The ACL, EVT and MSYS pools belong to the host and are allocated from internal heap at NimBLEDevice::init (transport.c:366-406, os_msys_init.c:154-170). Neither bt.c.obj nor libbtdm_app references them.
- S3 has no host flow control (esp_nimble_cfg.h:776-780), so the controller never learns the host's counts. When a pool runs dry:
  - adv reports are dropped (esp_nimble_hci.c:254-261);
  - an empty high-priority EVT pool asserts (:263-264);
  - ACL exhaustion retries (:166-174).
  So keep the EVT counts. ACL_FROM_LL can shrink for an observer.
- At runtime, ble_max_act = MAX_CONNECTIONS + BROADCASTER + OBSERVER (NimBLEDevice.cpp:927-928); IDF bt.c:1806 requires 1..10. It follows the host values.
- At compile time, CONFIG_BT_CTRL_* and CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT feed BT_CONTROLLER_INIT_CONFIG_DEFAULT() (esp_bt.h:196-202, 333-380), but the controller's feature #ifs are fixed inside the prebuilt bt.c.obj. Leave them alone.
- MSYS_2 (24 x 320 B) is also active (sdkconfig.h:821-822).
Q3, upstream (NimBLE-Arduino 2.5.1 docs):
- README.md:55 says to configure by editing src/nimconfig.h; README.md:16 sends IDF users to esp-nimble-cpp.
- CHANGELOG:108 '2.3.4: Cleanup redefinition warnings for arduino core 3.3+' [checked].
- The older PlatformIO -D route no longer works because nimconfig.h:3-4 includes sdkconfig.h first [checked]. On ESP there is no ext_nimble_config.h hook.
- GitHub issues were not searched (unverified).
Q4, options:
- (a) A project sdkconfig.h wrapper that does #include_next <sdkconfig.h> and then redefines the host values. Not compile-verified; it applies project-wide. Roles go to 0 (an #undef lets nimconfig.h set them back to 1), MAX_CONNECTIONS stays >= 1, and CTRL_* stay untouched.
- (b) pioarduino custom_sdkconfig hybrid compile (arduino.py:551-553, 912-913). It keeps host and controller consistent, but compiles IDF v5.5.1.251215 instead of the shipped libs' release/v5.5@9bb7aa84fe, overwrites the shared framework-arduinoespressif32-libs package, and triggers framework reinstalls across envs and worktrees. Build time not measured.
- (c) Runtime knobs: NimBLEDevice::setScanDuplicateCacheSize() (default 100, NimBLEDevice.cpp:259-272), and a weak btInUse() override that frees controller memory at boot when BLE is off.
- (d) No change.
Footprint, arithmetic from the pool definitions and not measured: host pools about 20.6 KB internal heap while BLE runs (ACL 6816, MSYS_2 7680, MSYS_1 3072, EVT 2736, CMD 260); controller statics 896 B; host statics about 5.4 KB. Savings per option: not measured.
Agent recommendation: (a), host-only (observer role, MAX_CONNECTIONS 1, small ACL and MSYS pools, EVT counts unchanged), plus setScanDuplicateCacheSize. Verify with -E that the wrapper wins, then measure heap and maxblk after NimBLEDevice::init, old against new, on a no-PSRAM S3. Hold (b).
Note: .pio/build/esp32-combo/firmware.* carry the timestamp 12:25 although the main session's last build was at 11:08. The research agent probably ran pio despite being told not to. Sizes are identical, build/ artifacts untouched.

OLD baseline observation, 2026-10-03, bench OTGW32 (no PSRAM), alpha.404+220fca7, BLE consent satbleriskack set true at runtime through POST /api/v2/settings:
- Internal heap: internal_free fell from 78668 to 15212 B within about a minute, and maxfreeblock from 40948 to 10228 B. No reboot. NimBLE with the framework host config costs about 63 KB of internal heap on this board.
- The device reset a telnet client twice: once while BLE started, once right after a settings POST with BLE running.
- REST: 13 consecutive POST attempts (10 s timeout each, 5 s apart) got no HTTP answer for about 3 minutes; then the 14th answered 200. hd_min_free_heap read 396 B afterwards. Ping answered again after the episode.
- After riskack false and a reboot: internal_free 79376 B, maxfreeblock 42996 B.
Consequences for AC#3: the bench script must tolerate minutes of REST silence on OLD with BLE on. A scan observable read over telnet is unreliable on OLD, because the session gets reset. Observables in the code: SATble.ino onResult() counts every advertisement (_bleAdCount); satBLELoop() prints 'SAT BLE: <n>s window: <ads> ads, <accepted> accepted, ...' once per iBleInterval when telnet debug key 7 is on (a toggle). GET /api/v2/sat/ble/discovery lists only parsed ATC/pvvx, BTHome v2 or MiBeacon sensors.
<!-- SECTION:NOTES:END -->
