# GPIO / OTA Post-Mortem

**Review Date**: 2026-03-16  
**Commit**: 56f19aaea7592ae3b35f264f483b4c25d826ef66  
**Branch**: `dev-branch-1.3-improved-setting-and-state-structures`

## Overview

This archive documents a post-mortem of a regression introduced in the settings/state refactor work and the follow-up fixes applied around OTA observability and UI polish.

The primary production issue was an ESP8266 crash during boot after flashing the filesystem. The immediate root cause was unsafe use of PROGMEM string comparison in internal control flow. During the investigation and fix, two related operator-facing improvements were also completed:

- XHR OTA uploads now emit clear block-by-block telnet progress for both firmware and filesystem uploads
- OTA lifecycle logs now use timestamped debug macros consistently
- The footer heap readout now has fixed spacing before the copyright text

## Files In This Review

- **POSTMORTEM.md** - Full incident analysis, fix assessment, and prevention guidance

## Quick Summary

**Incident**:
- Device crashed after filesystem flashing and reboot
- Crash occurred while parsing settings during boot
- Failure path was in GPIO conflict validation logic

**Root Cause**:
- Internal branch selection used a `PGM_P` discriminator and then applied `strcasecmp_P()` to it
- The code treated a flash pointer as though it were a RAM string, which is unsafe on ESP8266

**Fix**:
- Replaced string-based internal discrimination with a typed enum declared in the shared header
- Restored a type-safe `checkGPIOConflict()` API and removed the unsafe PROGMEM comparison pattern

**Follow-up Improvements**:
- Added XHR upload start/progress/complete/abort telnet logging in the shared OTA handler
- Switched OTA operator-visible logs to `DebugT*` macros so timestamps are consistent
- Added structural spacing in the footer between heap info and copyright text

## Main Lessons

1. Internal control flow should use enums or IDs, not sentinel strings.
2. On ESP8266, RAM-vs-flash pointer mistakes are crash-class bugs, not style issues.
3. Temporary fixes are acceptable for diagnosis, but production fixes should restore strong typing.
4. Operator-visible lifecycle logs should be timestamped by default.
5. Verification needs both code checks and workspace hygiene checks.

## Recommended Reading Order

1. Read `POSTMORTEM.md` for the full technical analysis.
2. Review the GPIO conflict helper in `settingStuff.ino` and its declaration in `OTGW-firmware.h`.
3. Review the OTA upload handler in `OTGW-ModUpdateServer-impl.h`.
