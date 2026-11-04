---
id: TASK-489
title: >-
  fix(satble): reset bDiscoveryPublished when BLE slot is recycled for a
  different MAC
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 05:42'
updated_date: '2026-04-30 05:47'
labels:
  - esp32
  - ble
  - sat
  - ha-discovery
  - bug
  - code-review
dependencies:
  - TASK-487
  - TASK-488
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Code-review finding #1 on the TASK-487/TASK-488 BLE feature pack
(commits `ace21a48..d8028b0a`).

`BLESensorData::bDiscoveryPublished` is set to `true` after the first
successful HA-discovery publish in `satBLEPublishMQTT()`. When that slot
later goes stale (`bValid` flips to `false` after `BLE_STALE_MS`) and is
subsequently re-allocated for a *different* MAC by `bleFindOrAllocSlot()`,
the new MAC inherits the stale `bDiscoveryPublished == true` flag.
Result: the new sensor never gets an HA discovery config published, so
it appears only as raw state topics with no HA device card.

The scan callback updates every other field on slot reuse but did not
touch `bDiscoveryPublished`:

```cpp
strlcpy(_bleSensors[slot].sMacAddress, macBuf, ...);
_bleSensors[slot].fTemperature = temp;
... // no reset of bDiscoveryPublished
```

## Hits in the practical case

- More than 4 distinct BLE sensors visible to the OTGW32 (slots churn).
- A sensor disappears for >5 minutes (`BLE_STALE_MS`) and another
  sensor takes its slot.
- A sensor's MAC changes (battery swap on some BTHome devices that
  randomize the address).

## Fix

In the scan callback, before `strlcpy`-ing the new MAC into the slot,
detect "MAC differs from what was stored" and reset
`bDiscoveryPublished` to `false`:

```cpp
if (strcmp(_bleSensors[slot].sMacAddress, macBuf) != 0) {
  _bleSensors[slot].bDiscoveryPublished = false;
}
```

For unchanged MAC the strcmp matches and the flag is preserved
(no discovery spam). For fresh slots `sMacAddress` is `""`, the
strcmp differs from the new MAC, the flag is reset to `false`
(was already `false` from default-init, so no-op). For recycled
slots the previous tenant's MAC is still in `sMacAddress`, the
strcmp differs from the new tenant's MAC, the flag is reset to
`false` and the new MAC gets a fresh discovery publish on the
next `satBLEPublishMQTT()` call.

## Validation

- ESP32 build clean.
- ESP8266 build unaffected (BLE code is `#if defined(ESP32)`).
- Behaviour regression test (manual): three BLE sensors active,
  remove sensor B for >5 minutes while sensor C newly enters range,
  observe sensor C's HA discovery configs being published when its
  first advertisement lands.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Scan callback resets `bDiscoveryPublished` to `false` when the MAC stored in the slot differs from the incoming advertisement's MAC
- [x] #2 Scan callback does NOT reset `bDiscoveryPublished` when the same MAC is seen again (no discovery spam on every scan)
- [x] #3 ESP32 build clean
- [x] #4 ESP8266 build unchanged (helpers stay inside `#if defined(ESP32)`)
- [x] #5 Comment in code explains the reset rationale so future readers understand the slot-recycling case
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-489 — bDiscoveryPublished slot-recycling reset

Fixes the code-review finding that `BLESensorData::bDiscoveryPublished`
was inherited across slot recycling: a stale slot reused for a new MAC
would skip HA-discovery publishing because the previous tenant's flag
remained `true`.

### Change
In `src/OTGW-firmware/SATble.ino` scan callback, before the `strlcpy`
of the new MAC into the slot, the slot's currently-stored MAC is
compared against the incoming advertisement's MAC; on mismatch
`bDiscoveryPublished` is reset to `false`. For the same-MAC case the
flag is preserved (no discovery spam on every scan).

### Verification
- ESP32 build SUCCESS (`-j 1`, 1m43s incremental).
- ESP8266 build unaffected (BLE code is `#if defined(ESP32)`).
- Inline comment explains the slot-recycling rationale so future
  readers see why the strcmp guard is there.

Pushed in commit `67ad53cf` on `feature-dev-2.0.0-otgw32-esp32-sat-support`.
<!-- SECTION:FINAL_SUMMARY:END -->
