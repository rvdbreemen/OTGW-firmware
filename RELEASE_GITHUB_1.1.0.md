The first major feature release since v1.0.0.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.1.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md)

## New features

- **Dallas sensor labels**: Click a sensor name in the Web UI to assign a friendly label. Labels persist in `/dallas_labels.ini` on the device, are backed up automatically before filesystem flash, and appear in the real-time temperature graph.
- **RESTful API v2**: 13 new `/api/v2/` endpoints with consistent JSON errors, proper HTTP status codes, CORS support, and RESTful resource naming. API compliance raised from 5.4 to 8.5/10. Full OpenAPI 3.0 specification included. Frontend fully migrated — zero legacy API calls remain.
- **WebUI data persistence**: Log data saved to browser localStorage, restored on page load, cleared automatically after firmware flash.
- **PS mode detection**: Automatic detection of `PS=1` from the PIC. When active, OT log and WebSocket streaming are suspended and time-sync commands are suppressed — improves compatibility with Domoticz.
- **Gateway mode overhaul**: Refactored detection and display logic. REST API field renamed from `mode` to `otgwmode`. Polling throttled to once per minute.
- **Enhanced diagnostic logging**: WebSocket events for OTGW command responses, PS mode transitions, and serial buffer overflows. Browser debug console (`otgwDebug`) available in the browser JavaScript console.

## Bug fixes

- **MQTT auth after upgrade from v0.10.x**: MQTT credentials are now automatically whitespace-trimmed on boot.
- **Slow Web UI**: File serving replaced with chunked streaming — 95% reduction in RAM used, resolves the sluggish UI reported on v1.0.0.
- **Settings reverting to defaults**: Settings now written to flash synchronously before HTTP confirmation; ArduinoJson replaces fragile string-split parsing.
- **Serial buffer**: `MAX_BUFFER_READ` increased to 512 bytes; overflow now discards the corrupt partial line.
- **20 additional bugs** from a full codebase review: out-of-bounds write on message ID 255, wrong MQTT hour bitmask (hours 16–23), ISR race conditions in S0 pulse counter, reflected XSS, GPIO outputs non-functional due to a debug flag, 750 ms blocking sensor read, disconnected sensor publishing -127°C to MQTT, file descriptor leak, null pointer crash on malformed MQTT topics, flash wear (20 writes per save reduced to 1), and more. See [CODEBASE_REVIEW.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md).

## Migration

**Flash both firmware and filesystem.** Hard browser refresh (Ctrl+F5) required after flashing.

No breaking changes — all MQTT topics and existing API endpoints remain functional. One exception: the gateway mode field in the REST API response was renamed from `mode` to `otgwmode`. Update any custom integrations that read this field directly.

v0 and unversioned API endpoints are deprecated and will be removed in v1.3.0.

## Thank you

This release would not have been possible without the community. Thank you to everyone who tested, reported issues, and pushed for improvements — your feedback is invaluable to the project.

Special thanks to: @hvxl, @sjorsjuhmaniac, @DaveDavenport, @DutchessNicole, @RobR, @GeorgeZ83, @tjfsteele, @vampywiz19, @Stemplar, @proditaki, and everyone in the Discord who keeps the conversation going.

If you want to support the project: [Buy me a coffee](https://www.buymeacoffee.com/rvdbreemen)
