# OTGW-firmware v1.3.1 Release Notes

**Release date:** 2026-03-28
**Branch:** `main` (from `dev`)
**Compare:** [v1.3.0...v1.3.1](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.0...v1.3.1)

---

v1.3.1 is a stability and correctness release that fixes command queue reliability, CS override interference, and serial coordination issues reported by the community after v1.3.0.

## Overview

- **Command queue overhaul:** PR command responses are now correctly matched by register letter, preventing wrong queue entries from being removed or silently retried until dropped.
- **Ser2net awareness:** The command queue now detects traffic from port 25238 (OTmonitor, ser2net clients) and temporarily pauses queue processing to avoid conflicting commands on the PIC serial bus.
- **CS override fix:** External setpoint commands (CS=) sent via Home Assistant / MQTT are no longer intermittently overridden by the thermostat's lower setpoint.
- **All commands via queue:** The SC= time-sync command previously bypassed the command queue; it now goes through the same queued path as all other PIC commands.
- **UI footer overlap fix:** The log window no longer overlaps the status bar when stretched to the bottom of the screen (Firefox/LibreWolf).

## Bug fixes

- **PR response queue matching:** `checkOTGWcmdqueue` previously matched only the 2-character command prefix (`PR`), causing a response for `PR=S` to incorrectly remove `PR=O` from the queue. Now matches the full register letter, with leading-space handling for the PIC response format `"PR: X=value"`.
- **PR=A (banner) never dequeued:** The PIC responds to `PR=A` with a banner string (`"OpenTherm Gateway x.x"`) that has no `:` at position 2, so `checkOTGWcmdqueue` was never reached. PR=A is now removed from the queue directly when the banner is detected.
- **CS override interference (#523):** PIC settings readout triggers are now whitelisted to specific commands (GW, SB, VR, TS, IT, OH, GA, GB, LA-LF) so that incoming CS=/CH= commands from Home Assistant no longer trigger a full PIC settings re-read cycle that could temporarily reset the setpoint.
- **`strstr_P` crash:** `strstr_P(kSettingsCmds, cmd)` had inverted arguments — PROGMEM pointer as haystack, RAM pointer as needle — causing an Exception (2) crash on ESP8266. Fixed by removing `PROGMEM` and using plain `strstr`.
- **SC= time-sync bypassed queue:** `sendtimecommand()` sent the SC= command directly via `sendOTGW()`, bypassing the command queue. Now routes through `addOTWGcmdtoqueue()` with deduplication.
- **SR= forced immediate send:** `handleOTGWqueue()` was called immediately after queueing SR= date/year commands, bypassing normal queue scheduling. Removed.
- **Queue boot delay:** `lastSer2netCmdMs` was initialized to `0`, causing the command queue to be paused for 2 seconds at boot even without ser2net activity. Fixed with unsigned wraparound initialization.
- **UI footer overlap:** Added `padding-bottom` to body in both light and dark themes so the fixed-position status bar no longer overlaps the log window content.

## Internal improvements

- **`removeFromCmdQueue()` helper:** The queue entry shift-down logic (previously duplicated in 3 places) is now a single static function.
- **Non-blocking PIC command/response handling:** PR= settings queries and PR=M gateway mode queries are now fully asynchronous via the command queue, replacing the old blocking `executeCommand()` pattern that starved the HTTP server for 2+ seconds.
- **Consistent char matching:** All queue prefix comparisons use direct character matching instead of `strncmp`.

## Upgrade notes

- **No breaking changes vs v1.3.0:** No MQTT topic renames, REST API changes, or settings-format migrations.
- **Flash both firmware and filesystem:** The CSS fix requires a filesystem update.
- **Hard-refresh the browser after flashing** (Ctrl+F5).
