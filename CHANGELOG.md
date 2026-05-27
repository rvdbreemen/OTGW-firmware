# Changelog

All notable changes to OTGW-firmware (the ESP8266 firmware for the NodoShop OpenTherm Gateway) are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

For full release notes per version, see the matching `RELEASE_NOTES_<version>.md` file. Current release notes live at the repository root; previous release notes are archived in [`docs/releases/`](docs/releases/).

## [Unreleased]

Tracking the `1.6.0-beta.N` line on `dev`. Promotion target: `1.6.0`.

### Added
- Static IP address settings: `wifistaticip`, `wifisubnet`, `wifigateway`, `wifidns1`, `wifidns2` are now persisted in settings and applied before WiFiManager connect, enabling DHCP-bypass for environments where the router does not assign predictable addresses (TASK-548)
- Statistics table columns are now drag-to-resize: a grab handle on each column header lets the user adjust column widths, persisted in localStorage under `otStatsColWidths` so preferences survive page reloads (TASK-703)
- Fixed IP address settings UI redesigned: a "Use DHCP" toggle hides the IP fields by default; each IP address uses four segmented number inputs (0-255 per octet) with auto-advance, backspace navigation, and full-address paste support; unchecking "Use DHCP" auto-prefills all fields from the current DHCP lease so switching to a fixed IP requires no manual lookup; the device info API now also exposes the current subnet, gateway, and DNS servers for the prefill (TASK-709)
- Bilateral OT-bus support map: bitmaps tracking which MsgIDs are seen from thermostat side and boiler side, with direction-aware "T/B/T+B" labels in the telnet diagnostic view and a new `GET /api/v2/otgw/support-map` REST endpoint; Web UI shows which data points the gateway has actually observed (TASK-683, TASK-684, TASK-685, TASK-686, TASK-688, #640)
- HA discovery: PIC-control entities exposed as `button` and `select` under pseudo-ID 251 (TASK-PR#576, #596)
- Standalone HA discovery topic wiper for cleaning stale retained discovery topics out of the broker (TASK-611, #587)
- `/beta-prerelease` skill plus `.github/workflows/beta-prerelease.yml` GitHub Action for tag-driven beta publishing; draft-first release creation with all assets attached in one atomic call to satisfy GitHub's immutable-releases policy (#607)
- `beta-prerelease.yml` `workflow_dispatch` now accepts a `ref` input and creates the tag at that ref if missing, enabling end-to-end beta publishing from the GitHub Actions UI without a local `git push` (#609)
- `beta-prerelease.yml` release body now inlines a "What's new since the last public release" digest sourced from `RELEASE_NOTES_<base>-beta.md` above a `<!-- digest:end -->` sentinel; the `/beta-prerelease` skill restructured so README + CHANGELOG staleness check runs as Phase 1 (pre-bump) instead of Phase 2.5 (post-bump), preventing stale narrative from locking onto an immutable tagged release; new `RELEASE_NOTES_1.6.0-beta.md` at repo root carries the per-line narrative (TASK-639, #612)
- Markdown link-validation guardrails for repository documentation (#573); link-check scope extended to `docs/guides/` and `docs/process/` in `.github/workflows/evaluate.yml` (#581) so the `../` link-path rot caught manually during the documentation review is enforced in CI

### Changed
- Pure JIT MQTT discovery: only non-OT pseudo-IDs (climate, number, Dallas, heap stats, firmware/PIC) are queued at boot; OT MsgID discovery configs publish on first MsgID reception, not on connect (ADR-073, supersedes ADR-041)
- Dev version line bumped to `1.6.0-beta.N` (was `1.5.x-beta.N`) (#601)
- Mainloop responsiveness audit: `delay()` / `delayMs()` usages on the cooperative path replaced with non-blocking timer checks so `doBackgroundTasks()` keeps running at full cadence under load (TASK-651, TASK-652, #617)
- MQTT `resetgateway` command now requires payload `"1"` (matching the HA-discovery `payload_press` value already in use) and is rate-limited to one PIC reset per 5 seconds. Non-matching payloads are logged and ignored; rapid retries inside the cooldown window are silently dropped with a log line. Closes the unauthenticated-LAN reset-storm path raised by the dev review (TASK-661)
- Mainloop Tier-1 follow-up: `handleOTGW()` PIC drain loops bounded at 4 lines per call, dead `executeCommand` path deleted, and the last stray `delay(1)` on the cooperative path replaced with `yield()` (TASK-671, #626)
- Mainloop Tier-1 follow-up #2: `String` usage removed from `helperStuff.ino` / `webhook.ino` hot paths; `emergencyHeapRecovery()` reworked to actually free RAM (drops the OTGWstream client and skips one discovery-drip tick when heap is critical, per ADR-079); always-on `BGTRACE` instrumentation dropped from production builds (TASK-673, #633)
- Mainloop Tier-2 dispositions: webhook HTTP timeout tightened from 1000 ms to 500 ms; the per-sensor OneWire read in `pollSensors()` left as bus-physics-bound; the 15 s MQTT connect socket timeout accepted as a known sync-blocker bounded by the 42 s retry gate (TASK-674, ADR-080, #635)
- Version-bump policy: per-commit `_VERSION_PRERELEASE` enforcement removed from `.githooks/pre-commit` on `dev`; the bump is now performed once per beta cut by `bin/bump-prerelease.sh` inside the `/beta-prerelease` skill (TASK-669, #624)

### Fixed
- Fixed IP UI octet inputs switched from `type="number"` to `type="text"` with `inputMode="numeric"` for correct mobile keyboards; ARIA `role="group"` and per-octet `aria-label` added for screen-reader accessibility; paste handler now validates all four octets before applying; `ArrowLeft` navigation added; per-field range validation runs before save and blocks the save button from hiding on invalid input; octet initialisation moved after DOM append so values render correctly on first load; dark-theme and common-theme CSS added for the fixed-IP section (TASK-709)
- LittleFS filesystem size was reported as 1 MB instead of 2 MB in the device-info API and Web UI; the partition size is now read directly from the LittleFS partition descriptor (TASK-701)
- Auto-scroll in the OT log was reset when switching tabs and when navigating back to the main page; scroll position is now preserved across tab switches and page revisits (TASK-701)
- `GET /api/v2/device/info` triggered multiple TCP yield points and excessive heap churn on each call; buffer allocations reduced and yield points consolidated (TASK-701)
- `/api/v2/device/info` no longer refuses requests under moderate heap fragmentation because its contiguous-block precheck was reduced from an over-conservative 8192-byte gate to the existing pbuf-sized safety threshold (TASK-723)
- MQTT discovery verify now runs an hourly first-run trigger in addition to the existing force-path, so any entities missed by the JIT pass are recovered automatically without user intervention (TASK-704)
- Statistics table column widths and the "boiler unsupported" badge were visually unbalanced after the support-map feature landed; column proportions corrected and badge styling refined (TASK-705, TASK-706)
- `logHeapStats` in `helperStuff.ino` was printing the window drop counters (`webSocketDropCount` / `mqttDropCount`) which reset to 0 after each throttle warning, making the per-minute heap line show ephemeral snapshots instead of monotonic lifetime totals; now prints the correct `state.heapdiag.iWsDropsTotal` / `iMqttDropsTotal` as every other consumer already does (TASK-697, #642)
- Beta.20 telnet diagnostic noise cleaned up: `onNotFound` handler now emits accurate `200 (file)` / `404` lines; `apifirmwarefilelist` no longer mirrors JSON to telnet; `checklittlefshash` suppressed on match; PROGMEM fixes for `strcmp_P` chains in `OTGW-Core.ino` and FSexplorer path handling (#637)
- HA capability-flag binary sensors for bits 2-5 (cooling, OTC active, CH2 active, summer/winter) stuck at `unknown` in Home Assistant: the global MQTT status fanout rate gate suppressed per-bit publishes on subsequent MsgID 5 frames; the rate gate is dropped and the per-bit publish is scoped to all three pending types so every bit reaches its retained topic on every status change (ADR-076, TASK-649, #614)
- HA `DHW Control`, `Thermostat`, and all sensor entities flapping `unavailable` (regression since 1.5.0/TASK-538): HA entity availability (`avty_t`) now reflects only the ESP↔MQTT link (birth/LWT) instead of OpenTherm-bus liveness. OT-bus liveness remains on the dedicated `otgw_connected` sensor. **Contract change:** consumers that read the base `<toptopic>/value/<nodeid>` topic as OT-bus liveness must migrate to the `otgw_connected` sensor (ADR-074, TASK-607)
- MQTT proxy-answer (no-B) routing: MsgIDs without a boiler response now route to the correct worldview topic instead of going silent; root cause behind PR #565 (ADR-075, #599)
- MsgID 0 Status canonical publish gated on boiler-side worldview so the canonical topic stops flapping on thermostat-only frames (TASK-633, #604)
- Silently-dropped MQTT set-commands now surface in the default debug stream instead of being swallowed (#602)
- JIT MQTT discovery could stall: the just-in-time trigger enqueued any OT MsgID with a valid value, including IDs with no HA sensor/binsensor config; `doAutoConfigureMsgid()` fails for those and the drip loop retains the pending bit, so the per-tick scan re-picked the same phantom ID forever and never published the real entities until the operator pressed `F`. The JIT trigger now applies the same `hasConfig` filter as the force path so both enqueue an identical ID set (ADR-073, TASK-601)
- FSexplorer **Update Firmware** button hidden on touch-capable desktops: the touch-class CSS media query no longer suppresses the upload control (GitHub #575, #598)
- `flash_otgw.sh` / `flash_otgw.bat` hardened: spec parity between the two scripts, SHA256 integrity verification, version-aware binary selection (#570)
- `flash_otgw.bat` COM port detection via registry; PS1 generation; auto-download of binaries when not found locally
- `build.py` auto-initialises missing git submodules so a fresh clone or stale checkout builds without manual `git submodule update` (#594)
- `evaluate.py` false-positive and stale-check fixes; CI gate is now meaningful again (#592)

### Documentation
- `docs/guides/MQTT_STALE_TOPICS_CLEANUP.md`: added a "Recovering missing HA entities" section distinguishing the just-in-time progressive-appearance behaviour and PIC-only-reset semantics from the upgrade stale-topic cleanup, with escalating recovery steps (wait, force re-announce, clear broker + reboot)
- New integration guides for openHAB and Domoticz (#590)
- New Dutch beginner guide for cleaning up stale MQTT topics in MQTT Explorer
- PIC and ESP firmware guides split into EN/NL language variants (#578); PIC guide scope restored and ESP-flash docs routed to `FLASH_GUIDE.md` (#579)
- Schelte firmware detail links added and PIC summaries aligned (#580)
- Repository documentation link paths normalised (#573)
- `CLAUDE.md`: documented `npx -y backlog.md` fallback when both the backlog MCP and the backlog CLI are unavailable (#571)
- API and ADR documentation refreshed mid-cycle (TASK-596): `docs/api/MQTT.md` documents the boot vs. JIT split per ADR-073; `docs/api/README.md` corrects the `/discovery/verify` REST endpoint description; `docs/adr/README.md` gains the ADR-041 (Superseded) and ADR-073 (Accepted) index entries
- Release-notes housekeeping (TASK-596): `RELEASE_NOTES_1.5.0.md` and `RELEASE_GITHUB_1.5.0.md` moved from the repo root into `docs/releases/`; the older `1.3.3` and `1.3.4` notes (both `RELEASE_NOTES_*` and `RELEASE_GITHUB_*`) archived under `docs/releases/archive/`
- Documentation-review findings 1-5 fixed (#581): stale `../` link paths corrected across `docs/guides/BUILD.md`, `docs/guides/FLASH_GUIDE_NL.md`, `docs/guides/PIC_FIRMWARE_EN.md`, `docs/guides/browser-debug-console.md`, and `docs/process/DOCUMENTATION_LINKS_POLICY.md`. The dev README banner was also restored to its dev-line styling in the same PR after a brief main-branch-styling slip introduced upstream in #574
- ADR-076 accepted: drops the global MQTT status fanout rate gate so all 13 capability-flag bits reach their retained topics on every status change
- ADR-077 proposed and then superseded by ADR-078: HA-core-style capability-flag aliases (37 opt-in topics) were drafted, implemented behind a feature flag, then reverted from `dev` and deferred to the 2.0.0 line; ADR-078 captures the deferral rationale
- ADR-079 accepted: `emergencyHeapRecovery()` defined as real recovery (drop OTGWstream client, skip one discovery-drip tick) instead of the previous "yield + log" no-op (TASK-673)
- ADR-080 accepted: the 15 s `MQTTclient.setSocketTimeout()` documented as a known main-loop sync-blocker bounded by the 42 s retry gate; replacing PubSubClient with an async client is explicitly out of scope for the 1.6.0 line (TASK-674)

### Removed
- Dead and orphaned code paths cleaned out of `dev` (#586, #589): inactive subsystem code and the matching scaffolding in `OTGW-firmware.h` removed, since neither is reachable on the 1.5.x / 1.6.x line.
- Accidentally committed root files removed; `.gitignore` tightened so they cannot return (TASK-635, #606)

## [1.5.0] - 2026-05-08

First stable release of the `1.5.x` LTS line on Arduino Core 2.7.4. Promotes 29 beta builds of fixes, MQTT improvements, and HA discovery refinements to stable.

### Added
- MQTT worldview semantics for `/thermostat` and `/boiler` source subtopics (ADR-069, TASK-549)
- Sibling-suffix MQTT source topic shape: `<msgid>_thermostat` / `<msgid>_boiler` (ADR-070, TASK-552)
- Sibling-suffix HA discovery topic shape replacing nested children (ADR-071, TASK-556)
- Drip mode threshold-hysteresis: deadband and K-tick dampening for stable source topics (TASK-553)
- HA auto-discovery for PIC and firmware diagnostic topics (TASK-540)
- Compact telnet welcome banner with log-triage snapshot and inline toggle list (TASK-545)
- `GET /api/v2/debug` REST endpoint for one-call diagnostic dump (TASK-536)
- HA discovery friendly names in human-readable Title Case with MDI icons (ADR-072, TASK-572, TASK-573)
- No-Python flash scripts: `flash_otgw.sh` / `flash_otgw.bat` and `build.sh` / `build.bat`
- ADR-066 documenting source-aware MQTT publish gating decision
- `docs/api/MQTT-message-id-echo-audit.md` spec-audit reference per OpenTherm v4.2
- `bSlaveEchoesValue` field on `OTlookup_t` populated for every MsgID
- Smart MQTT republish: `POST /api/v2/mqtt/republish` endpoint; republish on reconnect gated at 5-minute offline threshold

### Changed
- `/gateway` sub-topic removed; canonical base topic replaces it (TASK-538)
- ADR-066 MQTT base topic gating extended to OT-log WebSocket and REST state (TASK-483)
- Force-discovery routed through drip publisher with `maxBlock` throttle to prevent log flooding
- MQTT publish gating tightened: 250 ms minimum spacing between gated fan-out publishes

### Fixed
- Master MQTT topic flapping for `Tr`, `TrSet`, `MaxRelModLevelSetting`, and analogous write-only MsgIDs (ADR-066, TASK-478): base topic uses Read-Ack and Write-Data only; per-MsgID `bSlaveEchoesValue` flag gates the boiler echo path
- ADR-066 Write-Ack gate enum-family bug that silenced valid Write-Ack publications (TASK-561)
- MsgID 1 `TSet` `bSlaveEchoesValue` flip to `false` for heat-pump boiler stability (TASK-571)
- WiFi: DHCP lease not acquired after first reboot post-flash (TASK-432); `wifi_station_dhcpc_start()` removed, SDK manages DHCP autonomously
- WiFi: TCP listeners re-bound on reconnect causing port-already-in-use errors
- GW=R PIC reset command stuck in queue causing infinite PIC reset loop (TASK-538 queue fix); GW=R is now fire-and-forget
- WebSocket reload-storm churn: 250 ms reconnect debounce and `pagehide` shutdown handler added
- Non-monotonic debug timestamps in `_debugBOL` across a second-tick boundary

## [1.5.0-beta] - 2026-04-26

LTS line on Arduino Core 2.7.4. Carries forward the v1.4.x feature set on the proven, conservative Core version. Status: beta, in active development on `dev`.

### Added
- LTS line `1.5.x` on Arduino Core 2.7.4 as a parallel track to the v1.4.x Core 3.1.2 line
- Deferred-reboot machinery with lifecycle heap snapshots at four points around an OTA-triggered reboot
- `logBootSignature()` boot telemetry (reset reason, SDK version, sketch size, free heap at earliest `setup()`)
- `BGTRACE` per-phase timing instrumentation in `doBackgroundTasks` and the main loop (off by default in stability builds)
- `processOT` sub-trace with per-phase heap and time deltas (off by default in stability builds)
- HA auto-discovery for `otgw-firmware/stats/*` metrics so heap and discovery state appear as proper HA sensors
- `sendMQTTDataPic()` helper centralising `otgw-pic/` publish sites
- Self-hosted Inter and JetBrains Mono fonts in the WebUI (no external CDN dependency)
- Design system tokens for centralised colours, spacing, and typography across light and dark themes

### Changed
- Arduino Core baseline reverted to 2.7.4 from 3.1.2 for field-tested stability
- Partition layout retained at `eesz=4M2M` (4 MB flash, 2 MB LittleFS) from v1.4.x; `v1.4.1 → 1.5.x` upgrade does not require a filesystem partition reformat
- lwIP returns to the version shipped with Core 2.7.4 (the 2.2.0 update was Core 3.1.2-specific)
- MQTT msgId 0 Status fan-out gate decoupled from `iInterval` with independent 60 s heartbeat
- MQTT msgId 5/6/100 bit-and-byte fan-out gating with 60 s heartbeat (Scope C-min)
- MQTT publish gating tightened to 1 s minimum spacing between gated publishes (250 ms in latest beta)
- Nightly restart routes through the unified `doRestart()` path so it benefits from the same cleanup and snapshot machinery
- `BGTRACE` and `OTTRACE` instrumentation disabled by default in stability builds
- SimpleTelnet submodule bumped to `25a0250` (printf stack raised to 256 bytes)

### Removed
- Legacy `mqttha.cfg` template archive pipeline (streaming HA discovery from v1.4.x supersedes it)
- `WiFi.disconnect()` call from the reboot path (it wiped NVRAM credentials on Core 3.1.x)

### Fixed
- HA discovery for pseudo-ID 247 stats and related publish gates hardened
- `IS_PIC_ENTRY` flag honoured in HA discovery `stat_t` generators
- OTA reboot reliability: explicit service cleanup before `ESP.restart()` (WebSocket, telnet, HTTP, MQTT torn down in defined order)
- OTA reboot reliability: `ESP.reset()` fallback path when `ESP.restart()` returns to caller (Core 3.1.x failure mode)
- OTA safety-tail delay restored after `ESP.restart()` so auto-reset window fires reliably
- WebUI dark theme `.input-changed` was unreadable (black text on dark grey)
- WebUI dark theme `color-scheme` declaration, placeholder colour, scrollbar styling
- WebUI light theme input contrast and mobile header toggle overlap
- WebUI cross-browser dark/light theme rendering (Chrome, Firefox, Safari)
- WebUI log render hotpath stalls; restore buffer capped at 10 000 entries
- Per-message WebSocket console logs silenced behind the `otgwDebug.verbose` gate

## [1.4.1] - 2026-04-21

First public release in the 1.4.x series on Arduino Core 3.1.2. v1.4.0 was tracked internally but not published as a standalone release; v1.4.1 ships the complete 1.4.x body of work.

### Added
- SimpleTelnet debug console with formatted welcome banner and structured debug-key dispatch
- Retained MQTT discovery verification self-heal mechanism with daily auto-verify (ADR-062)
- Hourly heap diagnostic MQTT topic with 17-field JSON payload covering memory and discovery stats
- Nightly restart with configurable hour setting (default 4:00 AM)
- Configurable device manufacturer and model in MQTT device announcements (credit: Schelte Bron)
- NTP telemetry and debug toggle on telnet key 6
- WiFi SSID display and Reset WiFi button on the Settings page
- REST API endpoints for sensor status, discovery state, on-demand verification, and republish
- OTGW simulation mode for testing without physical hardware
- Unified time-boundary dispatcher for periodic tasks (ADR-064)

### Changed
- Arduino Core upgraded from 2.7.4 to 3.1.2 with improved WiFi driver stability
- LittleFS partition size increased from 1 MB to 2 MB (Core 3.1.2 layout)
- MQTT HA discovery rewritten with streaming bitmap-driven drip API (309 configs / 80+ msgIds, no static staging buffer)
- WiFi reconnect hardening: erroneous DHCP calls during active association removed
- OpenTherm v4.2 alignment: IDs 58 to 69 treated as reserved in v4.x mode
- Nightly restart timing now wall-clock aligned through the unified dispatcher
- Heap pressure reduction during HA discovery with configurable drip intervals (2 s normal / 10 s slow-mode)

### Fixed
- WiFi reconnect regression causing repeated cancelled associations before completion (#525)
- `MaxTSet` and `TdhwSet` showing 0 °C in Home Assistant (WRITE_ACK now accepted for OT_WRITE)
- PROGMEM-as-RAM `Exception (3)` crashes after Core 3.1.2 alignment enforcement (byte-safe helpers added)
- Retained MQTT discovery state can now be verified against broker; missing configs re-announced
- OpenTherm Answer Thermostat messages published to boiler MQTT source topic
- NTP last-sync field no longer poisoned by SDK boot value `0xFFFFFFFF`

## [1.3.5] - 2026-04-05

Stability follow-up to v1.3.4.

### Added
- MQTT uptime and firmware version publishing on connect

### Fixed
- WiFi reconnection regression introduced in v1.3.0 with too-aggressive 5-second per-attempt timeout

## [1.3.4] - 2026-04-01

### Added
- Thermostat-only MQTT support (OTGW stays online without boiler connected)

### Changed
- Renamed "OTGW Connected" to "OpenTherm Active" for clarity on the Device Info page

### Fixed
- MQTT throttle slot permanent suppression of stable sensor values after transient publish failures
- Debug Information page tooltips not wired up to device info labels

## [1.3.3] - 2026-03-31

### Added
- PIC-less OTGW support with automatic PIC availability detection and re-detection
- Central `isPICEnabled()` guard protecting all PIC-dependent code paths

### Fixed
- Dashboard no longer shows unsupported OpenTherm message IDs with empty or zero values
- Gateway mode detection for non-gateway PIC firmware returns "N/A" correctly

## [1.3.2] - 2026-03-29

### Fixed
- File deletion failures caused by global buffer conflicts during LittleFS operations
- File explorer "Error loading file list" by switching to streaming implementation

## [1.3.1] - 2026-03-28

### Changed
- Ser2net awareness in command queue to avoid conflicting PIC commands (ADR-059)
- All commands now route through the unified command queue

### Fixed
- Command queue matching by full register letter instead of just 2-character prefix
- `PR=A` banner response never dequeued from command queue
- CS override interference from PIC settings readout triggers
- Time-sync `SC=` command bypassed queue; now routes through proper queue
- Startup queue pause lasting 2 seconds without ser2net activity
- WebUI footer overlapping log window in Firefox / LibreWolf

## [1.3.0] - 2026-03-26

Major feature release: PIC settings visibility, safer upgrades, optional admin protection, fuller `PS=1` integration, lower RAM pressure.

### Added
- PIC Gateway Settings panel exposing all 15 configuration registers via REST API and MQTT
- Single-click GitHub release OTA with version comparison and rollback support
- Optional HTTP Basic Authentication for admin endpoints (disabled by default)
- Configurable MQTT publish gating for OpenTherm and `PS=1` summary data
- Full `PS=1` summary translation with MQTT publishing and HA discovery
- Monitor-page command bar for one-shot OTGW PIC commands
- Light/dark theme toggle button with persistent preference
- Triple-reset WiFi recovery to reopen captive portal
- OTGW simulation mode for testing
- Crash log endpoint for ESP8266 diagnostics
- OTGW event reporting (PIC restart, serial errors) via MQTT and WebSocket
- Heap memory info in device status and Web UI footer
- Gateway mode and WebSocket connection status indicators with tooltips

### Changed
- Global variables reorganised into `OTGWSettings` and `OTGWState` structs
- ArduinoJson dependency completely removed in favour of bounded manual JSON handling
- MQTT autodiscovery memory reduced via streaming template rendering
- Non-blocking WiFi reconnect state machine replaces the blocking 30-second loop
- REST API migration completed with dispatch table routing
- WiFiManager upgraded to stable 2.0.17
- Adaptive throttling based on a 4-level heap health system (ADR-030)

### Fixed
- ESP hostname reverting to `ESP-XXXXXX` after settings save
- Settings page blank on iOS Safari
- Boot-time spurious service restarts
- Hostname normalisation writing to wrong buffer
- File Explorer delete handling
- Webhook payload truncation after reboot
- Unsafe LittleFS OTA flashing without WiFi suppression
- IP validation incorrectly rejecting valid addresses with `255` octet
- NTP hostname not applied in all code paths
- Numeric settings accepting out-of-range values
- MQTT subscription topic truncation
- WiFi portal triggered by stale RTC data after USB flash
- PIC settings buffer truncation for longer-than-expected text responses

### Security
- Centralised auth enforcement in API dispatcher prevents individual handler oversights
- CORS wildcard removed; dynamic origin echoing instead
- CSRF validation hardened using static buffers instead of Arduino `String` class
- Webhook SSRF prevention with DNS resolution and RFC1918 validation
- XSS fix in statistics table with HTML entity escaping
- Boot command validation with alphabetic prefix check
- MQTT payload truncation guard rejects oversized payloads

## [1.2.0] - 2026-03-03

Protocol-alignment and discovery release.

### Added
- Comprehensive Home Assistant MQTT auto-discovery for 309 OpenTherm configs across 80+ message IDs
- Configurable source-separated MQTT publishing with nested topic paths (disabled by default)
- Webhook feature with configurable URL, payload, and content type for OpenTherm status bit changes
- OpenTherm v4.2 alignment with new message IDs 39, 93 to 97

### Changed
- OpenTherm direction flags corrected for IDs 4, 27, 37, 38, 98, 99, 109, 110, 112, 124, 126
- OpenTherm type / byte semantics updated for IDs 38, 71, 77, 78, 87, 98, 99
- `FanSpeed` handling as Hz instead of RPM
- `RelativeHumidity` handling as f8.8 instead of split-byte legacy format
- Legacy pre-v4.2 IDs 50 to 55 and 58 to 63 suppressed in AUTO mode (v4.x systems)
- Gateway mode parsing handles actual `PR=M` response format
- Serial read line buffer increased from 256 to 512 bytes for `PS=1` summary support
- Improved mobile responsiveness with stacked layouts and better touch targets

### Removed
- v0 and v1 REST API endpoints (return 410 Gone)

### Fixed
- MQTT topic spelling: `eletric` to `electric`, `incidator` to `indicator`, `ventlation` to `ventilation`
- MQTT HA discovery mismatches for `FanSpeed`, `Hcratio`, and `vh_configuration`
- `MQTTseparatesources` setting not persisted across reboots
- Gateway mode detection now properly tracks known / unknown state
- Serial robustness for overflow handling and line corruption

## [1.1.0] - 2026-02-25

Dallas sensors, RESTful API v2, and a 20-bug codebase overhaul.

### Added
- Dallas sensor custom labels with inline Web UI editor and LittleFS storage
- Dallas sensor graph visualisation with 16-colour palette and theme support
- Dallas sensor REST API endpoints for bulk label management
- WebUI data persistence to `localStorage` with auto-restoration and capture mode
- Browser debug console (`otgwDebug`) with diagnostic toolkit
- Non-blocking modal dialogs replacing blocking `prompt` / `alert` calls
- `PS=1` mode auto-detection with UI handling and WebSocket events
- Gateway mode display improvements and one-minute polling limit
- RESTful API v2 with 13 new endpoints, consistent JSON errors, and CORS support
- Full OpenAPI 3.0 specification documentation
- Architecture Decision Records ADR-030 through ADR-035

### Changed
- Frontend API migration from v0 / v1 to v2 endpoints
- OTmonitor refresh interval improved from 5 s to 1 s

### Fixed
- MQTT whitespace authentication issue with automatic trimming on boot and change
- Streaming file serving reducing RAM usage by 95 % (fixes slow Web UI)
- Settings persistence with synchronous flush before HTTP confirmation
- Serial buffer expansion to 512 bytes with proper overflow handling
- Dark mode PIC firmware icon visibility with CSS invert filter
- Out-of-bounds array write on OT message ID 255
- Wrong MQTT hour bitmask corrupting night setpoint schedules
- `is_value_valid()` using wrong data parameter
- PIC version string one-byte off-by-one comparison error
- Stack buffer overflow in hex parser
- ISR race conditions in S0 pulse counter (missing `volatile`, `uint16_t` counter)
- GPIO outputs feature gated by debug flag (non-functional in production)
- Null pointer crash from missing `strtok` checks in MQTT callback
- File descriptor leak in settings path
- Year overflow in date handling (`int8_t` to `int16_t`)
- Blocking 750 ms DS18B20 sensor read replaced with async non-blocking mode
- HTTP client resource leak with unconditional `end()`
- Settings flash wear reduced from 20 writes to 1 with 2-second debounce
- Disconnected sensor (-127 °C) published to MQTT suppressed
- GPIO conflict detection

### Security
- CSRF protection added to settings and admin endpoints
- Reflected XSS in error page fixed with HTML entity escaping
- Input sanitisation improvements across the API surface

## [1.0.0] - 2026-02-04

Major milestone: improved stability, modern UI, robust integration.

### Added
- Real-time graphs with ECharts for boiler temperatures, setpoints, pressure, and modulation
- Statistics dashboard with session and long-term heating system data
- Dark mode fully integrated with system preference detection
- Live log viewer using WebSockets for real-time streaming
- File System Explorer redesigned with better upload / download / delete
- WebSocket architecture for live data, reducing network overhead and latency
- MQTT auto-discovery with Home Assistant integration and stable reconnections
- Stream logging for OpenTherm logs to filesystem
- Interactive firmware flashing tool (`flash_esp.py`)
- PIC firmware upgrade from Web UI with binary validation
- Live update progress via WebSocket
- Settings preservation during firmware upgrades
- Memory safety via PROGMEM string optimisation
- Heap protection with active memory monitoring and adaptive throttling
- Watchdog improvements for recovery from hangs

### Changed
- Build pipeline migrated from `make` to fully integrated `arduino-cli`
- Log viewer switched to WebSocket transport
- Aggressive string-literal optimisation using `F()` and `PSTR()`
- Log line formatting and decoding improvements

### Removed
- HTTP polling for logs in favour of WebSockets
- Legacy commented-out code and unused libraries

### Fixed
- PIC firmware update crashes from binary data handling (`strncmp_P` to `memcmp_P`)
- MQTT buffer fragmentation and reconnection logic
- Timezone initialisation issues
- Multiple `Exception (2)` and `Exception (28)` causes related to memory access

## Pre-1.0 history

The pre-1.0 versions predate the Keep a Changelog format adopted in this project. Brief summaries are preserved here for completeness; full detail lives in the [GitHub releases page](https://github.com/rvdbreemen/OTGW-firmware/releases) and in commit history.

### [0.10.3]
- Changed: MQTT password masking on settings page
- Changed: HA discovery template improvements
- Fixed: status function regressions

### [0.10.2]
- Fixed: PIC firmware update path
- Changed: filesystem image bundles latest PIC firmware

### [0.10.1]
- Changed: build process improvements
- Fixed: VH status parsing
- Added: WiFi quality indicator

### [0.10.0]
- Added: PIC16F1847 (6.x firmware) support
- Added: DHCP NTP override
- Added: S0 pulse counter
- Added: Dallas sensor auto-configure

### [0.9.x]
- Added: JIT Home Assistant auto-discovery
- Added: climate entity
- Added: MQTT `set` commands
- Added: time setup and NTP improvements

### [0.8.x]
- Changed: MQTT topic convention
- Added: HA device grouping
- Added: climate entity (early form)
- Added: PIC firmware integration
- Added: Dallas temperature sensors
- Added: command queue

### [0.7.x]
- Changed: filesystem migrated to LittleFS
- Added: ser2net on TCP port 25238
- Added: ventilation / heat-recovery message IDs
- Added: PIC reset on boot

### [0.6.x]
- Added: standalone Web UI
- Added: OTA support

### [0.5.x]
- Added: REST API v1
- Added: settings UI

### [0.4.x]
- Added: ser2net
- Added: REST API v0

### [0.2.x and 0.3.x]
- Added: MQTT integration
- Added: serial stream output

### [0.0.1]
- Added: initial OpenTherm protocol parsing
