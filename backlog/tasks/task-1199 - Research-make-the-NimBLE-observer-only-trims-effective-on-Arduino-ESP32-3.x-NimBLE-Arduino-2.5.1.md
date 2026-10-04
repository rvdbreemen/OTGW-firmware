---
id: TASK-1199
title: >-
  Research: make the NimBLE observer-only trims effective on Arduino-ESP32 3.x
  (NimBLE-Arduino 2.5.1)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 08:58'
updated_date: '2026-10-04 06:59'
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
- [x] #3 If an option is chosen, the S3 bench with BLE on and no PSRAM shows internal free heap and maxfreeblock before and after in the same scenario, and a BLE scan still reports an advertiser
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

AC#3 bench measurement, 2026-10-04. OTGW32 (no PSRAM, combo image, app-only flash). Both builds use the same procedure (ble_heap_probe2.py, kept out of the repo):
1. satbleriskack=true with satbleenable already on, then a reboot.
2. Wait until the device has gone and come back.
3. Take 10 samples of /api/v2/device/info at boot+60..150 s.
4. Read /api/v2/sat/ble/discovery.
5. Restore and reboot.
OLD = 2.0.0-alpha.405+8fdc3b3, framework NimBLE config; it differs from NEW's base only by the TASK-1052 shim guard. NEW = 2.0.0-alpha.404+b6e900a with the include/sdkconfig.h wrapper.
- internal_free: OLD 14896-17296 B; NEW 33524-33848 B. About +17.5 KB.
- maxfreeblock: OLD 9716-12788 B; NEW 28660 B (stable). +16-19 KB.
- BLE: active on both, and 4 valid sensors on both (age 2.9-25.5 s), so the passive scan still reports advertisers.
- hd_min_free_heap since boot: OLD 7148 B; NEW 2308 B, set before the first sample and flat afterwards. Not explained: a one-off init-time dip on NEW. Watch it in the soak.
Earlier attempts in this session: an MQTT-stats observer gave no data, because stats are published less often than every 6 min. A REST bench run on OLD hit 503 'low heap' for about 20 minutes after a runtime BLE start (consent set without a reboot) and MQTT keepalive timeouts. Those runs are kept for the record.
Evidence: %LOCALAPPDATA%/OTGW-capture/task1199/bench-2026-10-04/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The NimBLE observer-only host trims now take effect (ADR-185, accepted 2026-10-04). A project header include/sdkconfig.h, found before the framework's sdkconfig dir through -Iinclude in the shared [env] build_flags, does #include_next and then trims only host values:
- peripheral and broadcaster roles off (central stays on, because the host does not link without its connection code);
- 1 connection, 1 bond, 0 CCCDs, ATT MTU 23;
- ACL_FROM_LL 2 and MSYS 6/6.
The EVT pools and every CONFIG_BT_CTRL_* stay at the framework values. NimBLE-Arduino is pinned to 2.5.1, the version the trims were verified against.

Evidence:
- AC#1 and AC#2: the research notes.
- Preprocessor: 67/67 values in 8 TUs x 3 envs.
- scripts/check_nimble_overrides.py: PASS on alpha.406. It checks the -I order (it fails on a missing -I and on -iquote) and that the link map takes only libbt.a(bt.c.obj).
- Two adversarial verifiers found no blocker.
- AC#3 bench: OTGW32 without PSRAM, BLE active, 4 valid sensors on both builds. internal_free goes from 14.9-17.3 KB to 33.5-33.8 KB (about +17.5 KB), and maxfreeblock from 9.7-12.8 KB to 28.7 KB.
- Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.406). evaluate.py --quick: 0 failures.
- Open observation for the soak: a one-off boot-time dip of hd_min_free_heap to 2308 B on the new build.
<!-- SECTION:FINAL_SUMMARY:END -->
