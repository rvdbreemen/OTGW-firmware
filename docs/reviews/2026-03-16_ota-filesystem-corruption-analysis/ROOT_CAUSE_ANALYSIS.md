---
# METADATA
Document Title: OTA Filesystem Corruption Root-Cause Analysis
Review Date: 2026-03-16 06:12:00 UTC
Commits Fixed: ab692e14c673c935572ad6f810882b3390716833
Target Version: v1.3.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: Root Cause Analysis
PR Branch: copilot/fix-usb-crash-dump-issue
Status: COMPLETE
---

# OTA Filesystem Corruption — Root Cause Analysis

## Why Was This Bug So Hard to Find?

The v1.3.0-beta LittleFS corruption bugs (resolved in commit `ab692e1`) are a good case study in the perfect storm of factors that make a bug extraordinarily hard to isolate. This document speculates on all the reasons they evaded detection.

---

## The Two Bugs

### Bug 1 — WiFi Reconnect Tearing Down the OTA HTTP Connection (Primary Regression)

**What happened:**  
`loopWifi()` was called unconditionally from `doBackgroundTasks()` — even while `state.flash.bESPactive` was `true`. During `Update.write()` the ESP8266 must suspend SPI flash reads, which starves the WiFi hardware. After the write completes, `WiFi.status()` may transiently return `WL_DISCONNECTED`. `loopWifi()` detects this and transitions to `WIFI_DISCONNECTED`, eventually calling `WiFi.begin()`. If that reconnect finishes before the HTTP upload stream ends, the `WIFI_RECONNECTED` branch calls `startWebSocket()` and `startMQTT()`, which reconfigure and restart network services — effectively tearing down the HTTP server connection that is still carrying OTA data. Result: the LittleFS partition is left partially written.

**The fix (one line):**
```cpp
// Before (broken):
loopWifi();

// After (fixed):
if (!isFlashing()) loopWifi();
```

---

### Bug 2 — Partial Partition Erase Leaving Stale Inodes (Latent Bug)

**What happened:**  
`Update.begin(uploadTotal, U_FS)` only erased sectors covering the upload image size (~125 blocks out of 256). LittleFS always mounts with `block_count` derived from the full partition size (256 blocks), so it scans all 256 blocks. The upper half (blocks 125–255) retained stale inode metadata from the previous filesystem, creating filesystem inconsistency.

**The fix (one argument):**
```cpp
// Before (broken):
Update.begin(uploadTotal > 0 ? uploadTotal : fsSize, U_FS)

// After (fixed):
Update.begin(fsSize, U_FS)  // always erase full partition
```

---

## Why Each Factor Made This Hard to Find

### 1. The Bug Only Triggered During *Filesystem* OTA — Not Firmware OTA

This is the most important discriminating factor.

- **Firmware OTA** (`U_FLASH`) writes to the sketch partition. The LittleFS partition is untouched. Rebooting after firmware OTA works fine — settings, labels, and all filesystem content survive intact.
- **Filesystem OTA** (`U_FS`) erases and rewrites the LittleFS partition. This is a far rarer operation — typically done only when deploying a new Web UI or when initial setup requires it.

Because filesystem OTA is infrequent and firmware OTA is the normal update path, most testing never exercises the broken code path. During development, it is natural to test firmware updates repeatedly while only occasionally updating the filesystem.

### 2. The Bug Is a Race Condition — Non-Deterministic

Bug 1 depends on timing. The WiFi stack starves during `Update.write()`, but whether `WiFi.status()` actually returns `WL_DISCONNECTED` by the time `loopWifi()` is next called depends on:
- How long the flash write takes (varies with image content)
- The WiFi connection quality (weak signal → drops faster)
- The router's behavior when the station goes quiet briefly

On a strong local network, the WiFi association survives the brief blackout and the bug never fires. On a marginal connection — or under stress — the transient disconnect triggers the reconnect cascade and the upload is silently corrupted. This non-determinism means:

- **The bug passes on a developer's desk** (strong WiFi, local network)
- **The bug fires in a user's home** (weaker signal, busier network, more interference)

### 3. It Was a Silent Regression from v1.2.0

In v1.2.0, WiFi reconnection was handled by the blocking `restartWifi()` function. This function was only ever called from `doTaskMinuteChanged()` — which was already guarded by `!isFlashing()`. The flashing guard was present *indirectly*.

When v1.3.0 refactored to the non-blocking `loopWifi()` state machine (ADR-047) and moved it to `doBackgroundTasks()`, the guard was inadvertently dropped. The two code paths look superficially similar:

```cpp
// v1.2.0 (safe — implicit guard via call site):
void doTaskMinuteChanged() {
    if (isFlashing()) return;  // <-- guard lives here, not in restartWifi()
    restartWifi();
    ...
}

// v1.3.0-beta (unsafe — guard not carried over):
void doBackgroundTasks() {
    loopWifi();  // <-- guard was never added when refactoring
    ...
}
```

A code reviewer would need to trace the v1.2.0 call chain back to `doTaskMinuteChanged()` and notice the implicit guard to realize it was missing in the new implementation. This kind of implicit-protection loss is notoriously easy to overlook in refactors.

### 4. The Crash Happens *After* the OTA Upload "Succeeds"

The HTTP upload returns 200 OK. The Web UI shows "Upload complete" and starts the health-check polling. The device reboots. Everything looks normal from the outside.

Only on the first boot *after* the OTA does the crash happen — when the firmware tries to mount the corrupted LittleFS partition and read settings. There is a **temporal gap** between cause (corrupted write) and effect (boot crash), and the OTA tooling has already declared success. This breaks the natural assumption that "it worked because the upload succeeded."

### 5. WiFi Goes Down Before the Crash, Eliminating Telnet and MQTT Visibility

This is why **USB crash dumps were the only way to see the problem**.

When LittleFS mounts on boot, it tries to read `settings.ini` to load WiFi credentials. If the mount fails or `settings.ini` is corrupted, the device cannot retrieve its SSID and password. Without WiFi, there is no:
- Telnet debug stream (requires WiFi)
- MQTT publish (requires WiFi + broker)
- Web UI (requires WiFi)
- WebSocket log stream (requires WiFi)

The only output channel still alive is **USB serial** (HardwareSerial / UART0). The ESP8266 panic handler writes the crash dump to UART0 before halting. Without a USB cable connected at the moment of the crash, the exception goes unseen entirely — the device just appears to reboot in a loop.

This explains the phrase: *"It only became clear after OTA of the filesystem. It showed itself thru the USB crash dumps."*

### 6. The Crash Dump Points to LittleFS — Not to OTA Code

The exception decoder output would show a stack trace inside LittleFS mount/read functions — not inside `loopWifi()` or `Update.write()`. A developer reading the crash dump cold would see:

```
Exception (29):        ← load/store alignment exception, or
Exception (2):         ← illegal instruction reading garbage flash data
epc1=0x402xxxxx        ← address inside LittleFS lfs_mount() or lfs_file_open()
```

Nothing in the crash dump points to `loopWifi()` or `Update.begin()`. The developer must reason *backwards* from "LittleFS is corrupt on this boot" to "what could have corrupted it?" and then to "the last thing that touched the filesystem was the OTA upload."

### 7. Bug 2 Was Latent — Only Visible Under Specific Conditions

The partial-erase bug (Bug 2) does not always cause visible corruption:

- If the device was **freshly flashed** (blocks 125–255 never written), those blocks are erased from manufacturing and contain `0xFF`. LittleFS handles `0xFF` blocks correctly — they look like unused/erased blocks. No corruption.
- If the device has been used for a while and **files existed in the upper partition blocks**, those blocks now contain stale metadata. LittleFS sees what looks like valid (but stale) inodes, potentially pointing to blocks that now contain new data. Result: either immediate mount failure or silent file corruption.

Users who only did a single filesystem flash (e.g., initial setup) never saw the bug. Only users who had used the filesystem long enough that data was spread across many blocks, and then re-flashed the filesystem, hit the corruption. This dramatically narrowed the reproduction population.

### 8. The Filesystem Image Itself Is Not Retained for Comparison

Unlike firmware where you can compare the flashed `.bin` with the original, the LittleFS binary content is only meaningful in the context of what was already in the partition. You cannot simply re-read the flash to verify the write succeeded — you need to mount it and check individual files. This makes post-mortem verification difficult without specialized tools.

---

## Summary Table

| Factor | Why It Made the Bug Hard to Find |
|--------|----------------------------------|
| Only triggered by filesystem OTA | Developers test firmware OTA routinely; filesystem OTA is rare |
| Race condition (Bug 1) | Non-deterministic; strong WiFi never triggers it |
| Silent regression from v1.2.0 | Implicit guard lost in refactor; not obvious from diff |
| Crash happens post-reboot, not during upload | Temporal gap between cause and effect; upload "succeeds" |
| WiFi down when crash occurs | No telnet/MQTT/WebSocket; USB crash dump is the only channel |
| Crash dump points to LittleFS, not OTA | Developer must reason backwards through two code paths |
| Bug 2 only visible on previously-used filesystems | Fresh devices never reproduce it |
| No preserved baseline for flash comparison | Hard to verify correctness post-write without tools |

---

## Lessons for Future Work

1. **Always carry flash guards explicitly.** When a function that must not run during OTA is moved to a new call site, explicitly add the `!isFlashing()` guard. Do not rely on the guard living at the call site elsewhere.

2. **Filesystem OTA must be a first-class test case.** Every release cycle should include a test that flashes the filesystem via OTA and verifies the device boots cleanly and reads all settings files correctly.

3. **Always erase the full partition.** When writing a filesystem image, always pass the full partition size to `Update.begin()`. Never rely on the image size to determine how much to erase — a smaller image leaves stale blocks that the filesystem will scan.

4. **OTA telnet logging is essential.** The addition of detailed telnet-debug OTA progress messages (commit `94a3ee8`) would have shown the `UPLOAD_FILE_ABORTED` event triggered by the WiFi reconnect, revealing the bug immediately if a USB connection were present.

5. **USB serial is a last-resort diagnostic.** Add a reset-reason and LittleFS mount success/failure message as early as possible in `setup()` before WiFi is initialized, so the boot state is visible even on a USB-only connection.

---

## Related Commits and Artifacts

| Commit | Description |
|--------|-------------|
| `ab692e1` | Fix: prevent OTA filesystem corruption (both bugs) |
| `94a3ee8` | Add OTA XHR upload progress tracking/logging |
| `dd45397` | Use DebugT (timestamp) for OTA upload logging |
| `a60f9ac` | Update OTA flash implementation and health endpoint |
| `dfa61a8` | Harden OTA/LittleFS flashing and update docs |
| `56f19aa` | Refactor OTA updater flow and helpers |

| ADR | Description |
|-----|-------------|
| ADR-029 | Simple XHR-Based OTA Flash (KISS Principle) |
| ADR-047 | Non-Blocking WiFi Reconnect State Machine |

---

## References

- ESP8266 Update library: `Update.begin(size, U_FS)` documentation — the `size` argument determines how many sectors are erased before writing
- LittleFS block scanning: LittleFS always mounts with `block_count = partition_size / block_size` regardless of image size
- `WIFI_RECONNECTED` state handler in `networkStuff.ino` — calls `startWebSocket()` and `startMQTT()`
- `isFlashing()` inline in `OTGW-firmware.h` — returns `state.flash.bESPactive || state.flash.bPICactive`
