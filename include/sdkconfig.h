// sdkconfig.h - project wrapper around the framework's prebuilt sdkconfig.h (ADR-185).
//
// NimBLE-Arduino's nimconfig.h includes "sdkconfig.h" before its own #ifndef
// defaults, so -D CONFIG_BT_NIMBLE_* build flags never take effect (TASK-1198).
// The -Iinclude in platformio.ini [env] build_flags puts this directory ahead of
// the framework's sdkconfig directory, so every translation unit gets this file.
// It pulls in the framework file with #include_next, then trims the NimBLE host
// for the passive scan in SATble.ino. Host values only: no CONFIG_BT_CTRL_*, no
// CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT, no EVT pool counts.
// Check: python scripts/check_nimble_overrides.py --env <env>
#pragma once
#include_next <sdkconfig.h>

// Peripheral and broadcaster roles off. A disabled role is defined as 0, never left
// undefined (nimconfig.h would set it back to 1), and its deprecated
// CONFIG_NIMBLE_ROLE_* alias goes too (nimconfig_rename.h would redefine the role
// as empty). The central role stays on: with central and peripheral both off,
// NimBLE-Arduino leaves out code that its other host files still call, and the
// link pulls a second NimBLE host from the prebuilt libbt.a and fails.
#undef  CONFIG_NIMBLE_ROLE_PERIPHERAL
#undef  CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL 0
#undef  CONFIG_NIMBLE_ROLE_BROADCASTER
#undef  CONFIG_BT_NIMBLE_ROLE_BROADCASTER
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER 0

// One connection slot, the minimum. NimBLEDevice::init passes
// MAX_CONNECTIONS + BROADCASTER + OBSERVER (now 2) to the controller as ble_max_act.
#undef  CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1

// One bond is the floor: with 0, ble_store_nvs.c still references the bond
// arrays that ble_store_config.c then leaves out.
#undef  CONFIG_BT_NIMBLE_MAX_BONDS
#define CONFIG_BT_NIMBLE_MAX_BONDS 1
#undef  CONFIG_BT_NIMBLE_MAX_CCCDS
#define CONFIG_BT_NIMBLE_MAX_CCCDS 0

// 23 is the ATT default and the legal minimum (BLE_ATT_MTU_DFLT).
#undef  CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 23

// ACL and MSYS buffers carry connection traffic only. Advertising reports use the
// EVT pools, which keep their framework counts.
#undef  CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT
#define CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT 2
#undef  CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT
#define CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT 6
#undef  CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT
#define CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT 6
