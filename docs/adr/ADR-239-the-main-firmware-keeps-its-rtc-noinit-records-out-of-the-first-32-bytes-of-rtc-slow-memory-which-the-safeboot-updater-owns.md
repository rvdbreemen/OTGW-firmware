---
id: "ADR-239"
title: "The main firmware keeps its RTC_NOINIT records out of the first 32 bytes of RTC slow memory, which the safeboot updater owns"
status: "Proposed"
date: "2026-11-14"
binding: true
gate: "scripts/check_rtc_layout.py (run by build.py on every ESP32 ELF); tests/test_rtc_layout_check.py"
documents_shipped: false
verified_in:
  - "src/OTGW-firmware/OTGW-firmware.h:g_rtcSafebootGuard"
  - "scripts/check_rtc_layout.py"
  - "tests/test_rtc_layout_check.py"
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "RTC memory"
  - "safeboot"
  - "reboot reason"
  - "memory layout"
aliases:
  - "RTC_NOINIT placement"
  - "safeboot RTC overlap"
components:
  - "RTC slow memory layout of the main firmware"
symbols:
  - "g_rtcSafebootGuard"
  - "check_rtc_layout"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-239 The main firmware keeps its RTC_NOINIT records out of the first 32 bytes of RTC slow memory, which the safeboot updater owns

## Status

Proposed, 2026-11-14.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

The main firmware keeps state over a restart in RTC slow memory with `RTC_NOINIT_ATTR` records: the doRestart()
reason (`g_rtcRebootReason`, TASK-1447), the WiFi dead-restart count and skip-portal mark (TASK-1450, TASK-1455) and
the filesystem-OTA snapshot (`g_fsOtaRtc`, ADR-233). The ESP-IDF bootloader loads the initialised RTC segments of
whichever app it starts.

On the 4MB layout an app OTA passes through the safeboot updater (ADR-191: `POST /safeboot`, the updater writes the
image, then restarts into the new app). The safeboot ELF has an initialised `.rtc.force_slow` segment of 0x20 B at
0x50000200 (the IDF variable `s_sleep_sub_mode_ref_cnt`; `readelf -l` of the safeboot build: LOAD VirtAddr
0x50000200 FileSiz 0x20). The main firmware placed `g_rtcRebootReason` (0x34 B) exactly at 0x50000200 (`nm -n -S`
of beta.243+c6dc184). Every pass through safeboot overwrote its magic and the first 28 reason bytes.

Device evidence on that code: the .63 reboot log of the beta.241 bench shows, after each app OTA, the boot after
safeboot without a reason ("11:59:31 Software reset (3) [rst 0x0c/0x0c]") where `[OTA] handover to safeboot` was
stored, while the filesystem OTA right after it, which does not pass safeboot, keeps its reason (TASK-1918).

The linker decides the order of `.rtc_noinit`; with LTO no source order is guaranteed.

## Decision

The first 32 bytes of RTC slow memory (0x50000200-0x5000021F) belong to the safeboot updater: the main firmware
holds them with a 32 B guard array, `g_rtcSafebootGuard` (OTGW-firmware.h, `RTC_DATA_ATTR`: initialised RTC data,
which the IDF linker script places in `.rtc.data` before `.rtc.bss` and `.rtc_noinit`), so `.rtc_noinit` starts at
exactly 0x50000220 in every version, and it checks the real ELF on every build.

## Decision Contract

### Must

- Keep `g_rtcSafebootGuard` (32 B, `RTC_DATA_ATTR`, `used`, referenced once in `setup()` so `--gc-sections`
  keeps it) at 0x50000200, so `_rtc_noinit_start` is exactly 0x50000220.
- Run `scripts/check_rtc_layout.py` on the ELF of every ESP32 build (build.py, `collect_pio_artifacts()`) and fail
  the build when the guard is missing or not at 0x50000200, when `_rtc_noinit_start` is not exactly 0x50000220
  (the records keep their address across versions, so a reason the old firmware stored is found after an OTA), or
  when a record starts below 0x50000220.
- Validate every RTC record with a magic word (and the snapshot with its CRC32), so the bytes another layout leaves
  behind are rejected.

### Must Not

- Place an RTC_NOINIT record of the main firmware in 0x50000200-0x5000021F, or add initialised or zeroed RTC data
  (`RTC_DATA_ATTR`, `RTC_BSS`) that moves `_rtc_noinit_start` away from 0x50000220.
- Change safeboot's RTC usage without re-checking this range.

### Exceptions

- None.

### Verification

- `scripts/check_rtc_layout.py <elf>`: red on the beta.243 ELF (`g_rtcRebootReason` at 0x50000200), green on the
  fix build; `tests/test_rtc_layout_check.py` (synthetic layouts plus the real old ELF).
- Upgrade transition, real code: `tests/test_reboot_reason.py` (case 8), `tests/test_wifi_skip_portal.py` and
  `tests/test_wifi_deadlink.py` (old-layout subtests), `test/host/test_fs_ota_rtc_snapshot.py` T13 (no format, no
  restore from an old snapshot read 0x20 B higher; the unshifted control is acted on).
- Bench (TASK-1918 AC3): after an app OTA through safeboot the boot shows `- [OTA] handover to safeboot`.

## Alternatives Considered

- **A guard in `.rtc_noinit`, defined first in the translation unit.** Tried first and rejected by the gate: in the
  real ELF the linker placed the global guard after the three static records (guard at 0x50000244,
  `g_rtcRebootReason` still at 0x50000200). Source order is no mechanism under LTO.
- **Pin each record to an address (linker script or a custom section).** Rejected: it needs our own linker
  fragment for the Arduino IDF build, more machinery than one guard array plus a check, for the same guarantee.
- **Move the records in safeboot instead (or drop safeboot's `.rtc.force_slow` data).** Rejected: the variable
  belongs to the IDF sleep code in the safeboot build; changing safeboot also means a safeboot build and a USB
  flash on every unit, while the main firmware goes out over WiFi.
- **Only a host test on an archived ELF.** Rejected: it does not go red on the build that introduces the problem.
- **Do nothing.** Rejected: every planned restart through safeboot loses its reason, and a future record placed
  there (for example a larger snapshot) would lose data.

## Consequences

**Positive:**

- The reason of a restart through safeboot (an app OTA) reaches the reboot log again.
- A build that would place a record in safeboot's range fails, whatever the linker does with the order.

**Negative:**

- RTC slow memory left after the main `.rtc.force_slow`: 0x50002000 - 0x50001F94 = 108 B (was 140 B). The next
  RTC user (TASK-1931) has to fit in that or shrink the FS-OTA snapshot.
- The first boot after the update reads the old firmware's records 0x20 B shifted. All records reject those bytes
  (magic, CRC; tests above), so that boot logs no reason and keeps no WiFi mark: one boot without a planned
  reason, accepted.
- build.py gets one post-build step on the ESP32 targets (nm on the ELF, about a second).
- The bootloader reloads the guard's 32 B on every boot of the main firmware; harmless, it is padding.
- A future `RTC_DATA_ATTR` or `RTC_BSS` variable fails the build; it needs its own decision (shrink the guard is not
  an option; place it elsewhere or supersede this ADR).

## Open Questions

- None.

## Related Decisions

- **ADR-191 (Update firmware over WiFi on 4MB boards through a safeboot factory partition)**: the updater whose
  bootloader load writes the 32 B.
- **ADR-233 (RTC snapshot during a filesystem OTA)**: the largest RTC record; its runtime guard
  `FS_OTA_RTC_MIN_ADDR` already refuses an address below 0x50000220.
- **ADR-080 (binding ADR rules need a CI gate)**: the gate is `scripts/check_rtc_layout.py` in build.py.

## References

- TASK-1918; TASK-1447, TASK-1450, TASK-1455, TASK-1774.
- `src/OTGW-firmware/OTGW-firmware.h:g_rtcSafebootGuard`, `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:440`
  (`[OTA] handover to safeboot`).
- `scripts/check_rtc_layout.py`, `build.py` (`collect_pio_artifacts()`), `tests/test_rtc_layout_check.py`.
