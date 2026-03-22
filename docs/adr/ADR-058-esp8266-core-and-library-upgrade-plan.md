# ADR-058: ESP8266 Core 3.1.2 and Library Upgrade Plan

## Status: Implemented — Pending Hardware Validation

## Date: 2026-03-22

## Context

The firmware has been pinned to ESP8266 Arduino Core **2.7.4** since development began. A previous attempt to
upgrade to Core 3.0.2 was abandoned due to a regression in the HTTP stream layer (noted in the Makefile
comment: `# bug in http stream, fallback to 2.7.4`). That specific bug was fixed in Core 3.1.x.

As of **dev HEAD at 2739fdb** (PR #516, merged 2026-03-22), the build system version mismatches between
`build.py` and `Makefile` have been resolved. The baseline is now:

| Component             | Pinned version |
|-----------------------|----------------|
| ESP8266 Arduino Core  | **2.7.4**      |
| WiFiManager           | 2.0.17         |
| PubSubClient          | 2.8.0          |
| TelnetStream          | 1.2.4          |
| AceCommon             | 1.6.2          |
| AceSorting            | 1.0.0          |
| AceTime               | 2.0.1          |
| OneWire               | 2.3.8          |
| DallasTemperature     | 4.0.6          |
| WebSockets            | 2.3.6          |

### Why upgrade now

1. **+16 KB heap** — Core 3.0.0 introduced a PoC cache-configuration change that reclaims ~16 KB of instruction
   cache as heap RAM. On a device with only ~40 KB usable RAM today, gaining ~40 % more heap is the single most
   impactful reliability improvement available without hardware changes.

2. **GCC 10.3** — Better dead-code elimination, improved `-Os` optimisation, stricter diagnostics that catch
   latent bugs at compile time.

3. **lwIP 2.1.3** (Core 3.1.0) — DHCP improvements, better TCP stack stability under sustained load.

4. **Stale TZDB** — AceTime 2.0.1 contains timezone data from ~2022. Regions with DST rule changes since then
   produce incorrect local-time calculations. Upgrading to 4.1.0 brings TZDB 2025b.

5. **WebSocket stability** — WebSockets 2.3.6 → 2.7.2 includes four minor versions of bug fixes relevant to
   the firmware's real-time dashboard streaming.

6. **TelnetStream ESP8266 Core 3.x compatibility** — TelnetStream 1.2.4 was written before Core 3.x;
   versions 1.2.3–1.2.4 added explicit Core 3.1 compatibility fixes. Version 1.3.0 is the current stable
   release.

### Pre-upgrade code audit (2026-03-22)

Before any files are changed, the current source was audited against every known Core 3.x breaking change:

| Breaking change in Core 3.x | Firmware impact | Status |
|-----------------------------|-----------------|--------|
| WiFi disabled at boot by default | `networkStuff.ino:51` already calls `WiFi.mode(WIFI_STA)` before `WiFi.begin()` | ✅ Already handled |
| `ICACHE_RAM_ATTR` → `IRAM_ATTR` | `s0PulseCount.ino:32` already uses `IRAM_ATTR`; custom OTGWSerial library contains no ISR attributes | ✅ Already handled |
| `String` returns `bool` not `unsigned char` | `String` is prohibited in hot paths (ADR-049); the few remaining uses are in init code not affected by this change | ✅ No impact |
| `analogWriteRange()` default changed to 8-bit | No `analogWrite` or `analogWriteRange` calls exist in the firmware source | ✅ No impact |
| `i2s.h` removed (Core 3.0.1) | Not used | ✅ No impact |
| Removed lwip-v1.4 / axTLS | Firmware uses plain HTTP (ADR-003); no TLS dependency | ✅ No impact |
| `EEPROM.end()` returns `bool` | Firmware uses LittleFS (ADR-008); EEPROM not used | ✅ No impact |

**Conclusion:** The firmware source requires **zero changes** to compile against Core 3.1.2. All breaking
changes were already handled by previous refactors or are simply not applicable.

### AceTime 4.x API audit

The firmware uses the following AceTime types and methods:

```
ace_time namespace
ExtendedZoneProcessor
ExtendedZoneProcessorCache<N>
ExtendedZoneManager
zonedbx::kZoneAndLinkRegistrySize
zonedbx::kZoneAndLinkRegistry
TimeZone
timezoneManager.createForZoneName()
ZonedDateTime::forUnixSeconds64()
ZonedDateTime: .year() .month() .day() .hour() .minute() .second()
TimeZone::forTimeOffset() / TimeOffset::forMinutes()
```

All of the above remain present and functionally unchanged in AceTime 4.1.0. The notable 4.x rename
(`LocalDate` → `PlainDate`, `LocalDateTime` → `PlainDateTime`) does not affect this firmware because it uses
`ZonedDateTime` exclusively, never `LocalDate` or `LocalDateTime` directly.

## Decision

Upgrade the firmware in three sequential phases. Each phase is independent and releasable. Do not collapse
phases — each must build and pass `evaluate.py` before the next begins.

### Phase 1 — Library-only upgrades (low risk)

Update `build.py` and `Makefile` to the following versions. The core stays at 2.7.4 throughout this phase.

| Library       | From   | To     | Notes |
|---------------|--------|--------|-------|
| WebSockets    | 2.3.6  | **2.7.2** | Four minor versions of bug fixes; no API changes for server usage |
| AceTime       | 2.0.1  | **4.1.0** | TZDB 2025b; no source changes required (see API audit above) |
| TelnetStream  | 1.2.4  | **1.3.0** | New dependency: `NetApiHelpers` must be added to the install chain |

**Required build-file changes for TelnetStream 1.3.0:**

`NetApiHelpers` must be installed *before* TelnetStream in the dependency chain. In both `build.py` and
`Makefile` add `NetApiHelpers@1.0.0` (or latest) immediately before the TelnetStream install entry.

**Libraries confirmed already at latest stable — no changes needed:**

| Library           | Version | Latest  |
|-------------------|---------|---------|
| WiFiManager       | 2.0.17  | 2.0.17  |
| PubSubClient      | 2.8.0   | 2.8.0   |
| AceCommon         | 1.6.2   | 1.6.2   |
| AceSorting        | 1.0.0   | 1.0.0   |
| OneWire           | 2.3.8   | 2.3.8   |
| DallasTemperature | 4.0.6   | 4.0.5*  |

*DallasTemperature is pinned at 4.0.6 which is one patch ahead of the latest published tag (4.0.5). This is
consistent — no downgrade needed.

**Phase 1 acceptance criteria:**

- [ ] Both `build.py` and `Makefile` updated consistently
- [ ] `python build.py` compiles without errors
- [ ] `python evaluate.py --quick` passes with zero critical issues
- [ ] Telnet debug port 23 responds after flash
- [ ] Local time displayed correctly for a DST-observing timezone (AceTime validation)
- [ ] WebSocket dashboard receives live OT updates

### Phase 2 — ESP8266 Core 2.7.4 → 3.1.2 (medium risk, transformative gain)

**No source-code changes are required** (see pre-upgrade audit above). The only change is the board-manager
URL in `build.py` and `Makefile`.

Change from:
```
https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json
```

To the stable index with an explicit version pin:
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

And pin the core install to version `3.1.2`:
```
arduino-cli core install esp8266:esp8266@3.1.2
```

> **Why 3.1.2 and not latest?**  Core 3.1.2 (March 2024) is the most recent tagged stable release.
> Pinning to an explicit version prevents silent regressions from future core updates.

> **Why not 3.0.2?**  The HTTP stream regression that forced the original fallback to 2.7.4 was present in
> 3.0.2 and fixed in 3.1.x. 3.1.2 is the safe target.

**Phase 2 acceptance criteria:**

- [ ] Both `build.py` and `Makefile` updated consistently
- [ ] `python build.py` compiles with GCC 10.3 without errors or new warnings
- [ ] `python evaluate.py --quick` passes
- [ ] Binary size delta documented (expected: similar size, better optimisation)
- [ ] Full hardware validation checklist below passes

### Phase 3 — Hardware validation (mandatory before merge)

This firmware cannot be fully validated without physical hardware. The following checklist must be completed
on a real D1 mini or NodeMCU device **for both Phase 1 and Phase 2** before merging to dev.

**Boot and connectivity:**
- [ ] Boot completes; watchdog does not trigger within 60 s
- [ ] WiFi connects; DHCP lease obtained
- [ ] WiFiManager captive portal appears when no credentials are stored
- [ ] OTA firmware update succeeds and device reboots correctly

**Protocol and API:**
- [ ] OpenTherm messages are received and decoded correctly from the PIC
- [ ] REST API `/api/v2/` responds with correct JSON
- [ ] MQTT connects to broker and publishes OT data within one cycle
- [ ] WebSocket dashboard in browser receives live frame updates
- [ ] Telnet debug (port 23) connects and shows output

**Time and sensors:**
- [ ] NTP sync completes; local time correct for configured timezone
- [ ] DST transition test: set timezone to `Europe/Amsterdam` and verify offset is correct
- [ ] DS18B20 temperature sensor reads correctly (if wired)

**Stability:**
- [ ] Heap remains stable (non-decreasing) under 30-min sustained load
- [ ] Free heap after boot is measurably higher under Core 3.1.2 than 2.7.4
  (expected: +14–16 KB; log `ESP.getFreeHeap()` at startup for both builds)
- [ ] No exception stack traces in serial/telnet output during test period

### Phase 4 — ADR status update

Once hardware validation passes, update this ADR's status to **Accepted** and record the measured heap delta
(expected: +14–16 KB vs Core 2.7.4 baseline).

## Implementation record

**Branch:** `claude/arduino-upgrade-analysis-PwiZg`
**Date:** 2026-03-22

### Changes made

`build.py`:
- `board_manager.additional_urls` → `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
- `core install esp8266:esp8266` → `esp8266:esp8266@3.1.2`
- Added `NetApiHelpers@1.0.2` before TelnetStream in library list
- `TelnetStream@1.2.4` → `1.3.0`
- `AceTime@2.0.1` → `4.1.0`
- `WebSockets@2.3.6` → `2.7.2`

`Makefile`:
- `ESP8266URL` → `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
- `core install $(PLATFORM)` → `$(PLATFORM)@3.1.2`
- Added `libraries/NetApiHelpers` to `LIBRARIES` list and install chain
- `NetApiHelpers@1.0.2` install rule added (serialised before TelnetStream)
- `TelnetStream@1.2.4` → `1.3.0`; dependency updated to `libraries/NetApiHelpers`
- `AceTime@2.0.1` → `4.1.0`
- `WebSockets@2.3.6` → `2.7.2`

### Pre-commit verification

`python3 evaluate.py --quick` result: **22/22 checks passed, health score 100%**

### Still required before merging to dev

All items in the Phase 3 hardware validation checklist above must be completed on physical hardware.

## Consequences

### Positive

1. **+14–16 KB usable heap** — the single largest reliability improvement achievable in software.
2. **Current TZDB** — users in DST-observing regions get correct local time.
3. **GCC 10.3** — better optimisation and stricter compile-time diagnostics.
4. **lwIP 2.1.3** — more robust TCP/DHCP behaviour under sustained load.
5. **WebSocket stability** — four minor-version bug-fix series applied.
6. **TelnetStream Core 3.x compatibility** — debug telnet no longer relies on pre-3.x workarounds.

### Negative

1. **Hardware test gate** — Phase 2 cannot be merged without physical device validation.
2. **Heap gain is a runtime measurement** — compile-time metrics do not confirm the +16 KB claim; it must be
   measured on device.
3. **Future core updates** — pinning to 3.1.2 means manually tracking future security releases.

### Risks and mitigations

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| HTTP stream regression reappears in 3.1.2 | Low — confirmed fixed in 3.1.x | Test full HTTP file-serve path on hardware |
| AceTime 4.x introduces runtime behaviour change | Low — API audit shows no-change path | Validate DST offset on hardware |
| TelnetStream 1.3.0 + NetApiHelpers incompatible with Core 3.1.2 | Low — 1.3.0 explicitly targets Core 3.x | Test telnet port on hardware |
| Core 3.1.2 changes heap layout, breaking static buffer sizes | Very low — static sizes are compile-time | Run evaluate.py; monitor heap at runtime |
| WiFi regression under Core 3.x on specific D1 mini revision | Low | Test WiFiManager portal + reconnect |

## Related

- ADR-001: ESP8266 Platform Selection
- ADR-003: HTTP-Only Network Architecture
- ADR-004: Static Buffer Allocation
- ADR-008: LittleFS Filesystem
- ADR-009: PROGMEM String Literals
- ADR-013: Arduino Framework (not ESP-IDF)
- ADR-015: NTP + AceTime timezone library
- ADR-024: TelnetStream for debug output
- ADR-030: Heap Monitoring
- ADR-049: String Prohibition in Protocol Paths

## References

- `build.py` — primary build script (board manager URL, library install list)
- `Makefile` — CI/secondary build (must stay in sync with build.py)
- `src/OTGW-firmware/networkStuff.ino` — WiFi initialisation (`WiFi.mode(WIFI_STA)`)
- `src/OTGW-firmware/s0PulseCount.ino` — `IRAM_ATTR` ISR (already Core 3.x compatible)
- `src/OTGW-firmware/OTGW-firmware.h` — AceTime type declarations
- `src/OTGW-firmware/helperStuff.ino` — AceTime runtime usage
- https://github.com/esp8266/Arduino/releases/tag/3.1.2
- https://github.com/bxparks/AceTime/blob/develop/CHANGELOG.md
- https://github.com/Links2004/arduinoWebSockets/releases/tag/2.7.2
- https://github.com/JAndrassy/TelnetStream/releases/tag/1.3.0
