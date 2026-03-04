# Release Notes — v1.3.0-beta

**Last updated:** 2026-03-02<br>
**Release branch:** `dev-branch-v1.3.0`<br>
**Comparison target:** `v1.2.0-beta` (`dev-1.2.0-stable-version-adding-webhook`)<br>

---

## Summary

v1.3.0-beta builds on v1.2.0-beta with focused reliability, usability, and memory improvements. There are no breaking changes: no MQTT topic renames, no API removals, and no settings format changes.

**New features:** triple-reset WiFi recovery, one-shot OTGW PIC commands from the web UI, full PS=1 summary parsing with MQTT/HA discovery, heap status in device info.

**Bug fixes:** boot-time spurious service restarts, hostname dot-stripping targeting wrong buffer, webhook payload truncation on settings load.

**Memory / performance:** String heap fragmentation eliminated from settings save path; ArduinoJson fully removed.

---

## New Features

### 🔁 Triple-Reset WiFi Recovery

**Problem solved**: when WiFi credentials become invalid (router replaced, SSID changed, password changed, device moved), the only previous recovery path was to physically re-flash the firmware. The device would boot, fail to connect, and sit in an unrecoverable loop.

**Solution**: triple-clicking the hardware reset button within 10 seconds now clears all stored WiFi credentials and restarts the WiFiManager captive portal for reconfiguration.

**How it works**:
1. The firmware counts resets that occur within 10 seconds of boot.
2. On the third reset, the saved WiFi configuration is erased.
3. The blue LED blinks to indicate recovery mode is active.
4. The device starts an access point on `192.168.4.1` with a captive portal.
5. Connect to the access point from any phone or laptop, configure the new WiFi network, and the device reconnects.

**Important**: normal single resets and double resets are unaffected. Only three resets within 10 seconds triggers recovery.

See [docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md](docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md) for detailed instructions.

**ADR**: [ADR-043](docs/adr/ADR-043-reset-pattern-wifi-recovery.md)

---

### 💻 One-Shot OTGW PIC Commands from the Web UI

A command bar has been added to the OpenTherm Monitor page. You can now type raw OTGW PIC commands directly in the browser and see the response in the log in real time — without needing a telnet or serial connection.

**Examples**:
- `TT=20.5` — set room temperature target
- `SH=60` — set hot water temperature setpoint
- `PR=A` — print firmware revision
- `GW=R` — reset the gateway PIC

The log shows sent commands, their responses, and any error responses (`NG`, `SE`, `BV`, `OR`, `NS`, `NF`, `OE`) with appropriate visual styling.

---

### 📊 PS=1 Summary Parsing with MQTT/HA Discovery

Previously, `PS=1` mode (Print Summary) was detected and the firmware would stop streaming raw OT frames — but the `PS=1` summary lines themselves were not parsed.

v1.3.0 fully parses all `PS=1` summary fields and:
- Publishes each field to MQTT (using the same topic structure as normal OT message publishing).
- Registers Home Assistant auto-discovery entries per field, so PS mode users now get proper HA sensor entities.

This makes the firmware fully functional in PS=1 mode for Home Assistant users who previously had no HA entities while in PS mode.

---

### 🧠 Heap Status in Device Info

Free heap, max contiguous block size, and heap health level are now included in `GET /api/v2/device/info`:

```json
{
  "device": {
    "freeheap": 12936,
    "maxfreeblock": 12440,
    "heaplevel": "HEALTHY"
  }
}
```

`heaplevel` values: `HEALTHY`, `LOW`, `WARNING`, `CRITICAL`.

These values are also surfaced on the firmware flash utility page to help diagnose low-memory conditions before flashing.

---

## Bug Fixes

### Boot-Time Spurious Service Restarts

**Root cause**: `readSettings()` called `updateSetting()` for every field. `updateSetting()` unconditionally set `settingsDirty = true` and queued pending side effects (MQTT reconnect, NTP restart, mDNS restart). This caused `flushSettings()` on the first `loop()` iteration to restart all network services and rewrite the settings file — dropping in-progress MQTT connections immediately after boot.

**Fix**: `settingsDirty` and `pendingSideEffects` are cleared immediately after `readSettings()` completes. Services start cleanly from the initial connection attempt rather than restarting redundantly.

### Hostname Dot-Stripping Targeting Wrong Buffer

**Root cause**: the code that strips dots from the hostname (`strchr(..., '.')`) was pointing at `settingMQTTtopTopic` instead of `settingHostname`. On devices where the MQTT top-topic contained a dot, the hostname could be incorrectly modified; on devices where it didn't, the strip was silently a no-op on the wrong buffer.

**Fix**: corrected to `strchr(settingHostname, '.')`.

### Webhook Payload Truncation on Settings Load

`valueBuf` in the settings reader was sized for typical short values and could truncate long webhook payload strings, causing the payload to load incorrectly after a reboot.

**Fix**: `valueBuf` widened to accommodate the maximum webhook payload length.

---

## Memory and Performance Improvements

### String Heap Fragmentation Eliminated from Settings Save

**Problem**: `writeSettings()` called `writeJsonStringKV()` 14 times per save. Each call created a temporary `String` object on the heap, immediately freed it, then allocated a new one of a different size. On the ESP8266's heap allocator, repeated alloc/free cycles of varying sizes cause fragmentation — the heap becomes "swiss cheese" over time, reducing the largest contiguous free block even when total free bytes look healthy.

**Fix**:
- Added `escapeJsonStringTo()` in `jsonStuff.ino`: a buffer-writing JSON string escaper with no heap allocation.
- `writeJsonStringKV()` now uses the global `cMsg` buffer as scratch (safe because `writeSettings()` has no `yield()` between calls).
- `writeJsonStringPair()` (dallas labels) now uses small fixed stack buffers sized for the known key (≤16 chars) and value (≤24 chars) limits.
- Removed dead `escapeJsonString()` (the `String`-returning variant) which had no remaining callers.
- `readSettings()` now reuses `cMsg` as line buffer instead of a local `lineBuf[256]`, saving 256 bytes of stack per invocation.

### REST API v2 Migration Complete

The OTA/firmware flash page (`/update`) was the last place in the firmware that called v1 endpoints for health polling and sensor label restore. These are now on v2. The REST API migration started in v1.1.0 is 100% complete.

### ArduinoJson Removed

All JSON serialisation paths in the firmware now use bounded manual helpers (`wStrF`, `wBoolF`, `wIntF`, `parseSettingsLine()`). This removes the ArduinoJson library entirely, reducing both flash usage and RAM overhead.

---

## No Breaking Changes

| Area | v1.2.0 → v1.3.0 |
|------|-----------------|
| MQTT topics | No changes |
| HA discovery | Additive only (PS=1 fields added) |
| REST API | No removals or renames |
| Settings keys | No changes |
| `settings.ini` format | No changes |

Upgrading from v1.2.0-beta requires no migration steps.

Upgrading from v1.1.x or earlier: see [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) for v1.2.0 migration guidance — MQTT topic renames (OpenTherm v4.2 alignment), REST API v0/v1 removal, and device-info key changes apply.

---

## Validation Basis

Compiled from the git delta between the `dev-1.2.0-stable-version-adding-webhook` merge point and the current `dev-branch-v1.3.0` head (`f4a12df`). Changes derived from commit history and file diffs across firmware, Web UI, settings handling, and documentation.
