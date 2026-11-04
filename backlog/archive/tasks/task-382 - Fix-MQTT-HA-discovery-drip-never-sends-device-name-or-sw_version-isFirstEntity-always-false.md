---
id: TASK-382
title: >-
  Fix: MQTT HA discovery drip never sends device name or sw_version
  (isFirstEntity always false)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-22 06:34'
updated_date: '2026-04-22 06:47'
labels:
  - bug
  - mqtt
  - discovery
dependencies: []
references:
  - 'Discord #beta-testing, andrebrait, 2026-04-22'
  - 'MQTTstuff.ino:1766 (doAutoConfigureMsgid buildDiscoveryContext call)'
  - 'MQTTstuff.ino:1689 (doAutoConfigure buildDiscoveryContext(true))'
  - 'MQTTHaDiscovery.cpp:1743 (writeDeviceBlock isFirstEntity guard)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Reported by **andrebrait** in Discord `#beta-testing` on 2026-04-22, upgrading from 1.3.10 → 1.4.1:

> "MQTT auto-discovery has discovered the OTGW as an 'Unnamed device' with no firmware version"

Home Assistant shows the OTGW device as "Unnamed device" with no `sw_version` after a fresh MQTT connect or after settings reset.

## Root Cause

The drip discovery mechanism in `MQTTstuff.ino` always calls `buildDiscoveryContext()` **without** the `isFirst = true` argument, so `ctx.isFirstEntity` is always `false` for every entity published via the drip.

In `MQTTHaDiscovery.cpp`, `writeDeviceBlock()` only writes `name`, `manufacturer`, `model`, and `sw_version` when `ctx.isFirstEntity == true`:

```cpp
// MQTTHaDiscovery.cpp:1743
if (ctx.isFirstEntity) {
    // writes manufacturer, model, name ("OpenTherm Gateway (<hostname>)"), sw_version
}
```

With `isFirstEntity = false`, only `"identifiers"` is written. HA never learns the device name or firmware version from the drip.

`doAutoConfigure()` (full republish) correctly calls `buildDiscoveryContext(true)`, but this function is only triggered by:
- Telnet key `F`
- REST endpoint `POST /api/v2/discovery/republish`
- Daily auto-verify (1.4.1 new feature)

It is **not** called automatically on MQTT connect. The drip path (`loopMQTTDiscovery()` → `doAutoConfigureMsgid()`) is the only normal post-connect path and it never sends device info.

### Key code locations

- `MQTTstuff.ino:1766` — `doAutoConfigureMsgid()` calls `buildDiscoveryContext()` without `isFirst`
- `MQTTstuff.ino:1689` — `doAutoConfigure()` calls `buildDiscoveryContext(true)` correctly
- `MQTTstuff.ino:627-631` — `startMQTT()` calls `markAllMQTTConfigPending()` then drip takes over
- `MQTTHaDiscovery.cpp:1743-1755` — `writeDeviceBlock()` conditionally includes device info

## Proposed Fix

Add a `dripDeviceInfoPending` static flag to `MQTTstuff.ino`.

1. Set it to `true` in `markAllMQTTConfigPending()`
2. Pass it to `doAutoConfigureMsgid()` as an `isFirst` boolean parameter
3. `doAutoConfigureMsgid()` passes it to `buildDiscoveryContext(isFirst)` and clears `ctx.isFirstEntity` after the first entity is published (as it already does in `doAutoConfigure()`)
4. In `loopMQTTDiscovery()`: after a successful `doAutoConfigureMsgid()` call with `isFirst = true`, clear `dripDeviceInfoPending`

This ensures the very first entity in each new drip cycle carries the full device block, exactly as `doAutoConfigure()` does.

## Why this regressed vs 1.3.10

In 1.3.x (pre-ADR-077), `doAutoConfigure()` was called directly on MQTT connect, sending all entities including the device block in a single burst. ADR-077 introduced the drip to avoid a blocking burst, but the "first entity carries device info" invariant was not preserved in the drip path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After MQTT connect on a device with default settings, HA shows 'OpenTherm Gateway (<hostname>)' as device name
- [x] #2 After MQTT connect, HA shows the correct firmware version in sw_version
- [x] #3 The fix applies only to the drip path; doAutoConfigure() behavior is unchanged
- [x] #4 Build passes: python build.py --firmware exits 0
- [x] #5 evaluate.py --quick exits 0 with no new violations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added `dripDeviceInfoPending` static flag to `MQTTstuff.ino`. Set to `true` in `markAllMQTTConfigPending()`, passed as `isFirst` parameter to `doAutoConfigureMsgid()`, cleared after first successful drip publish. `doAutoConfigureMsgid()` signature changed from `(byte OTid)` to `(byte OTid, bool isFirst)` and passes `isFirst` to `buildDiscoveryContext()`. The first entity in each new drip cycle now carries the full device block (name, manufacturer, model, sw_version), matching the behaviour of `doAutoConfigure()`. Build: 97.1% health, no new violations. Branch: fix-issue-mqtt-discovery-device-name."
<!-- SECTION:FINAL_SUMMARY:END -->
