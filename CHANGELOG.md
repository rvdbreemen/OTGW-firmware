# Changelog

All notable changes to OTGW-firmware (the ESP8266 firmware for the NodoShop OpenTherm Gateway) are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

For full release notes per version, see the matching `RELEASE_NOTES_<version>.md` file. Current release notes live at the repository root; previous release notes are archived in [`docs/releases/`](docs/releases/).

## [Unreleased]

### Added

- **A reboot command on the telnet console.** Everything served on `httpServer` runs from `handleClient()`, which `canServeHttp()` withholds under heap pressure, so a device whose gate had engaged could be neither flashed nor rebooted over HTTP. Telnet stays reachable by design and its loop runs before the gate, but the console had no way to act: there was no reboot command and no `ESP.restart` call anywhere in it. `R` now requests a deferred reboot, routed through the existing mechanism so a reboot asked for mid-flash waits instead of bricking the device, and allocating nothing, which is the property that matters in the state it exists for. Verified on a bench gateway: the reboot fired 3 ms after the keystroke and the device came back. (TASK-1089, ADR-092)

### Fixed

- **Two dashboards open at once left one of them permanently refused.** The gateway serves one request per rate-limit window per endpoint from a single budget shared by every client, and both web-interface pollers ran on a fixed-phase timer, so two browsers opened at the same moment polled at the same instants forever: one won every window and the other was refused every window. The refused client skipped the cycle quietly, which preserved the lock rather than breaking it. On a 429 the timer is now re-armed after a delay drawn from one full poll period, giving it a random phase. Simulated against the server's real windows (1500 ms for the OpenTherm poll, 4000 ms for the status poll, one budget shared by all clients): before, 300 of 300 runs left one client with zero successful requests over two minutes; after, none. Note this redistributes service rather than creating capacity. Both routes have a poll period shorter than twice their window, so two dashboards cannot both be served every cycle at any phase; what changes is that the refusal now rotates between them instead of pinning on one forever. (TASK-1090, ADR-086)
- **An upload that was interrupted leaked a file handle.** A client disconnecting mid-body left the file open for the lifetime of the sketch, one handle per interruption, and LittleFS allows only a bounded number. A rapid-refresh storm during an upload could exhaust them and leave the filesystem unable to open anything at all. Interrupted uploads are now closed, and a stale handle is closed defensively when the next upload starts. (TASK-793)
- **The main page kept streaming to browsers that had already gone away.** `index.html` is about 11 KB and was streamed without ever checking whether the client was still connected, and every write to a closed socket still walks the write path before failing, which costs loop time exactly when the device is under load. The stream now stops. The two low-memory refusals also carry `Retry-After`, so a refused client backs off instead of re-requesting immediately and holding the heap in the state that caused the refusal. Measured under a scripted storm: completed requests rose from 14 of 47 to 18 of 47 at six concurrent clients, with no crash and no reboot in any run. (TASK-793)

### Internal

- **The beta release page no longer claims a branch the tag did not come from.** The generated body stated that every prerelease came from a tag push on `dev`, which was false on every beta ever published from this line: they are all tagged on `otgw-1.x.x`. The branch is now derived from the tag, and named only when exactly one branch contains it. The same assumption sat in the manual re-publish guard, which accepted only `dev` and defaulted to it, so re-publishing a stuck 1.x beta would have tagged 2.0.0 code under a 1.7.x version. The body also now leads with what the build changes instead of a glossary of asset types. (TASK-1117)

## [1.7.5] - 2026-09-04

### Security

- **settings.ini was served without authentication, so setting an admin password published it.** The file holds httppasswd and MQTTpasswd in cleartext, and the file-serving path had no auth check while every sibling operation did. An unauthenticated client on the LAN could read both credentials from a password-protected gateway and replay them. The read is now gated with the same check as the delete and reboot operations, including a leading `//settings.ini` alias that resolves to the same file. Verified on a live gateway: with a password set the request returns 401 unauthenticated and 200 with credentials. (TASK-1099, ADR-056)
- **A CORS preflight could reach protected routes unauthenticated, and the CSRF check never saw its own headers.** checkHttpAuth() short-circuited OPTIONS, so a preflight reached the delete, reboot and reset routes without credentials; and collectHeaders() registered only If-None-Match, so the same-origin check could never read Origin or Referer and always passed. The short-circuit is removed (legitimate v2 preflights are still answered upstream) and Origin/Referer are now collected, which turns origin enforcement on for every route that calls checkHttpAuth(). Verified on hardware: an unauthenticated OPTIONS returns 401 and an authenticated cross-origin POST returns 403. (TASK-1100, ADR-054, ADR-056)
- **The /upload same-origin check ran only after the file had been written.** A cross-origin POST overwrote the target on disk and was refused only afterwards. The check now runs at the first chunk, before the file is opened, so a rejected upload writes nothing. (TASK-1103, TASK-1101)

### Added

- **The filesystem image now ships PIC gateway 6.8 and diagnose 2.2.** The bundled images lagged upstream at gateway 6.6 and diagnose 2.1, so every fresh flash started behind and had to fetch the difference over the web. Both are now byte-identical to the current files on Schelte Bron's site, with matching `.ver` files. That pairing matters more than it looks for diagnose: the firmware normally reads the version out of the hex itself and uses the `.ver` file only as a fallback, but the diagnose image carries no `OpenTherm Gateway ` banner for that parse to find, so its `.ver` file is the only version source it has. A `.ver` that disagreed with its `.hex` would make the device report a version it does not run and never fetch the real one. (TASK-1122)
- **The PIC firmware table now marks the cells a web download actually changed.** The refresh button rewrote the version cell in place and never touched the size cell, so a click that downloaded a new image looked exactly like a click on a file that was already current: both were silent. Both cells now go through one helper that compares before it writes, so a value that actually changed is highlighted for about fifteen seconds and an unchanged value stays visually quiet. Clicking again restarts the highlight rather than scheduling a second timer. (TASK-1110)
- **Cumulative DHW water total, published by the gateway itself.** The Energy dashboard needs a cumulative volume, not a rate, and Home Assistant MQTT discovery cannot create the integration and template helpers that would build one host-side, so every user had to wire that up by hand against their own entity id. The gateway now integrates MsgID 19 and publishes `dhw_water_total` as an auto-discovered sensor (`device_class: water`, `L`, `state_class: total_increasing`), selectable in the Energy dashboard water section with no configuration. The gateway does not poll MsgID 19, so an interval longer than 60 seconds between frames counts as a gap in the measurement and adds nothing: a bus that falls silent at 8 l/min must not keep booking water that never flowed. The total is not persisted, because `total_increasing` reads the drop after a reboot as a meter reset and keeps the long-run sum. Nothing is published, and no entity is created, until a MsgID 19 frame has actually decoded, so installations whose thermostat never requests that id do not gain a meter pinned at zero. Verified against a live Home Assistant. (GH #675, TASK-1091)

### Fixed

- **Configured boot commands were typed into the diagnose firmware's menu at every boot.** The diagnose PIC image is a menu that reads single keystrokes, so a boot command such as the default `GW=1` never arrives as a command: `G`, `W`, `=`, `1` and CR each select a menu entry and start a test. Reported by Schelte Bron, author of the PIC firmware. The commands are now skipped on a PIC identified as diagnose, and are sent as soon as the PIC has identified itself rather than blindly at the end of `setup()`, where no banner has been read yet and there is no firmware type to test. A gateway that never sends a banner still gets its boot commands after three seconds, so the gate can only ever spare a confirmed diagnose PIC. (TASK-1121)
- **The WebSocket server stopped listening for good after a single WiFi reconnect, which also disabled the PIC flash button.** `refreshServicesAfterWifiReconnect()` dropped stale clients with `doWebSocketClose()`, but `WebSocketsServer::close()` also closes the listening socket and clears the running flag, and `begin()` is only ever called from `setup()`. One reconnect therefore left port 81 refusing every connection until the next reboot, and the flash button gates on that port. Measured on a gateway with seven days of uptime: port 81 answered with a TCP reset while a freshly booted sibling accepted the handshake normally. The reconnect path now drops clients only, which is what the comment above it already claimed it did. (TASK-1107)
- **Clicking flash did nothing at all when the OT log WebSocket was unavailable.** `performFlash()` waited up to five seconds for the WebSocket and then gave up with "Connection timed out. Cannot track progress.", returning before the upgrade request was ever sent, so the firmware never saw the click even though the flash back end was healthy. ADR-025 puts flash progress on HTTP polling, and that poller is already armed by the time the gate runs, so a missing WebSocket costs live log lines and not the flash itself. The request now goes out either way and the status line says progress is tracked by polling instead of reporting a failure. Verified by driving the endpoint directly with port 81 closed: gateway.hex went from 6.6 to 6.8 in 27 seconds with 0 errors and 0 retries. (TASK-1108, ADR-025)
- **Output on the OTmonitor bridge only arrived once a complete line had been assembled.** The serial-to-network path on port 25238 buffered bytes until a CR or LF arrived, so a prompt that carries no newline never reached a connected client, and a CR, LF or NUL inside a payload was consumed as a terminator and replaced by a synthesised CRLF. Reported by Schelte Bron against the diagnose PIC firmware, whose prompt is declared as `da "Enter test number: \032"` where the sentinel is never transmitted, leaving the prompt stranded while the PIC blocks waiting for input. Transport and parsing are now separate concerns: every byte is forwarded verbatim as it is read, coalesced in a 64-byte buffer so a burst does not become one TCP segment per byte, while line assembly continues to feed the OT parser only. A line that overflows the parser buffer still reaches the client. Verified on hardware: 69 of 98 reads on port 25238 no longer end on a line terminator, which the previous code could not produce. (TASK-1109)
- **The upload free-space guard could be defeated by an integer underflow.** Once LittleFS passed 95.2% full, the reported free space wrapped from a small number to about 4 GB, so the UI kept the Upload button enabled and accepted a file the filesystem could not hold. The used-bytes figure is now clamped before the 32-bit subtraction. (TASK-1098)
- **The upload handler reported success even when nothing was written.** An upload onto a full filesystem, or onto a name colliding with a directory, still answered a 303 redirect; the handler now tracks the outcome and returns a 507 on failure, which the UI surfaces. A filename longer than 30 characters, which the server silently shortened to its last 30, is now refused in the UI up front. (TASK-1101)
- **Files with a reserved character in the name could not be opened, downloaded or reliably listed.** The open and download links assigned the raw path to href while the delete link on the same row encoded it, so a name containing `#`, `?` or `%` opened the wrong URL; the multipart upload filename was run through urlDecode, mangling a literal `+` or `%`; and the listing built its JSON with a raw format specifier, so one name containing a quote or backslash broke the whole listing. Links are now per-segment encoded, the filename is stored verbatim, and names are escaped in the JSON. Verified on hardware: `note#1.txt` and `a+b.txt` upload, list, open and delete correctly. (TASK-1102)
- **The delete handler could run off the end of its path buffer.** The leading-slash normalization could shift a non-NUL byte into the last buffer slot for a path of 63 or more characters; the terminator is now forced. (TASK-1103)
- **The bootstrap FSexplorer helper page kept showing after a successful upload.** The routes were bound at boot to a filesystem existence check, so a device that booted with an empty filesystem kept serving the helper page until a reboot even after FSexplorer.html had been uploaded through it. The routes now decide per request. (TASK-1104)

- **FSexplorer wrote every upload to the root, whatever directory you were browsing.** A user building his own PIC firmware could not get the hex file into `/pic16f1847`: the file landed in the root and there is no way to move it from there. The file list already put the current directory on the form action as `?path=`, and the server already read that parameter, but the submit handler built its own XMLHttpRequest and posted to a hardcoded `/upload`, so the path was discarded on every upload. The handler now posts to the form action. Cleaning up the test uploads exposed a second defect in the same screen: the delete handler copied the path into a 34-byte buffer, so `/pic16f1847/` plus a realistic firmware name was truncated and answered "File not found", leaving the file unremovable. That buffer is now 64 bytes, and the comment that claimed a 31-character limit on paths is corrected: the limit is on the filename. Both fixes were verified on a live gateway: upload into a subdirectory, upload into the root, and delete of a 41-character path. Note that the flash list reads the directory matching the detected PIC type, and a `.ver` file is not needed: the firmware reads the version out of the hex and writes the `.ver` itself. (TASK-1096)

- **The water total entity appeared on gateways that have no DHW flow data, and never came back after a broker restart.** Two defects in the entity shipped by beta.3, both found by an adversarial review of ADR-093 rather than in the field. The daily discovery auto-heal re-announces every row in the discovery table, and the water meter has one, so a gateway whose thermostat never requests MsgID 19 gained a retained config for an entity that receives no state and sits at `unknown`. The auto-heal now skips that row until a frame has actually decoded. Separately, the announce latch was a function-local static that the broker-restart path could not reach, so once a restarted broker dropped the retained configs the meter was never re-announced until the gateway rebooted; the latch now lives with the counter and is re-armed on that path. (TASK-1093, ADR-093)
- **The DHW flow rate sensor went unavailable in Home Assistant on beta.2.** Home Assistant validates `unit_of_measurement` against `device_class` and discards the whole discovery config on a mismatch. Its constant for litres per minute is `L/min` with a capital L, and the firmware emitted `l/min`. Until beta.2 this entity carried no device class, so nothing validated the unit; typing it as `volume_flow_rate` in the same build turned a working sensor into a rejected config. Reproduced and fixed against a live Home Assistant: the corrected config builds the entity, the old one builds nothing. (GH #675, TASK-1092)
- **The HTTP heap gate could hold itself shut.** `canServeHttp()` withholds `handleClient()` below the contiguous-block threshold, but `handleClient()` is also the only code that drains the web server's unclaimed-connection queue, and a pending connection releases its buffers only at refcount zero. While the gate was shut, every pending connection kept its pcb and buffers, so the block the gate waited for could never return. In field logs `maxBlock` then oscillated between 480 and 1872 and never crossed 2048 again for the rest of the run. Pending connections are now reaped directly, without running the handler or its multipart parser (whose unchecked 2100-byte allocation is larger than the gate threshold, so pumping the handler is never safe below it). (TASK-1039)
- **The telegraf endpoint bypassed the REST rate limit entirely.** `/api/v2/otgw/telegraf` and `/api/v2/otgw/otmonitor` route to the same handler and return the same payload, but only otmonitor was listed as rate limited, so a client on the telegraf path polled uncapped. Both paths now share one budget: a second table row would carry its own timestamp and let a client alternate between the two paths at twice the intended rate. (ADR-086, TASK-1090)
- **The "boiler does not implement" panel could report an id the boiler demonstrably answers.** The per-message-id capability bitmap was maintained from a frame's master/slave bit alone. A gateway override answer to the thermostat is a type-7 with that bit set, so it counted as boiler evidence and marked the id unsupported, while the boiler's own Write-Ack had already marked the same id as acked. The bit was published retained and persisted to `/ot-boiler.json`, so it survived reboots and nothing ever cleared it. Override answers are no longer boiler evidence, and a genuine boiler Ack now retracts an earlier verdict, so existing false entries clear themselves from live traffic without deleting any file. The retraction requires a real boiler frame: a gateway that answers the thermostat outright emits no boiler frame at all, and must not be able to clear a truthful verdict. (GH #677, TASK-1080)
- **Settings values longer than 149 characters were silently truncated and reported as saved.** A webhook payload template over that length was stored cut off mid-JSON while the API returned HTTP 200. Two independent causes: the settings POST handler read into an undersized buffer, and the JSON field extractor discarded what did not fit while still reporting success. The extractor now fails instead of truncating, and the buffer matches the largest writable setting. Verified with a host-compiled harness under `test/` that exercises the shipped parser and fails against the pre-fix code. (TASK-1082, follow-up TASK-1083)

### Changed

- **The OTmonitor bridge on port 25238 now accepts one client instead of two.** The port is a bidirectional serial bridge and the library underneath has no per-client stream identity: `read()` returns a bare byte and the read path serves whichever client happens to have data. Two clients writing at the same time therefore spliced their bytes into a single PIC command stream, so one client's partial line could concatenate with another's into a malformed command. Within one client the order always held; between clients it did not. The port now has a single slot, which removes that failure mode outright. A reconnect from the same address still takes the session over, so a client that crashed or was killed can reclaim the port without waiting for a timeout. The cost is stated plainly: the second slot existed for "Home Assistant plus one debug consumer", and a passive second consumer is now refused as well, because the library cannot tell a reader from a writer. Serving one writer alongside several readers would need a per-client read API in the library and a change to every caller. Found by an adversarial audit of byte exactness on this port. (TASK-1115)
- **The DHW flow rate sensor is now typed `device_class: volume_flow_rate`.** GitHub #675 asked for `device_class: water`, but Home Assistant restricts that class to cumulative volume units (`L`, `gal`, `m3`, `ft3`, `CCF`, `MCF`), and this sensor is `l/min` with `state_class: measurement`. A water-classed rate entity is rejected by HA. Note this does not place the sensor on the Energy dashboard: that needs a cumulative total, which now ships as its own entity in this same release (see Added, TASK-1091). (GH #675, TASK-1081)

### Documentation

- ADR-090 (Accepted 2026-08-25) decided to publish a firmware-integrated cumulative DHW water total for the Home Assistant Energy dashboard. That entity ships in this release (TASK-1091).
- ADR-093 accepted: keep the cumulative DHW water total in RAM and announce it on first data. It supersedes ADR-090, carrying its decision to ship a firmware-side total forward unchanged while replacing three requirements of its Decision Contract that building the feature showed to be wrong: persisting the counter to flash (whose unclean-power-loss restore corrupts the Home Assistant statistic, a risk ADR-090 recorded as unresolved), registering the entity in the boot-publish path (which gives every gateway a meter pinned at 0.0 L, including the majority whose bus carries no MsgID 19), and keeping the rate sensor on the `l/min` unit string that Home Assistant rejects. It also settles a question ADR-090 did not address: an interval longer than 60 seconds between frames is a gap in the measurement, not flow. ADR-090 is now Superseded.
- ADR-091 accepted: a heap refusal must not suppress the cleanup path it depends on. The HTTP heap gate withheld the only code that drains the pending-connection queue, so the memory it waited for could never be released (TASK-1039).
- ADR-092 accepted: keep a recovery route reachable when the HTTP heap gate refuses, so a gateway in a degraded heap state can still be reached and reset.
- ADR-094 accepted: it corrects the record of the DHW water total, tightening the announce gating and replacing estimated figures with measured ones and live citations. It supersedes ADR-093, which is now marked Superseded.
- ADR-095 accepted: the OpenTherm data flow pipeline now carries an enforced byte-transparent serial bridge. It supersedes ADR-038, keeping the pipeline description while making byte transparency on port 25238 a binding property rather than an implementation detail, with a declarative enforcement rule so a future edit cannot quietly reintroduce line buffering (TASK-1109).

## [1.7.4] - 2026-08-10

Home Assistant integration and OpenTherm decoding release for the 1.x (ESP8266) line: entities survive a Home Assistant Core restart, three Remeha vendor message IDs decode for the first time, and four ventilation topics honour their heartbeat again. No breaking changes versus v1.7.2. Full notes: [RELEASE_NOTES_1.7.4.md](RELEASE_NOTES_1.7.4.md).

### Fixed
- Home Assistant entities stayed on "unknown" after every Home Assistant Core restart, and only a gateway reboot brought them back. Discovery configs are retained, so Home Assistant rebuilt the entities from the broker, but state topics are not retained and most values publish only when they change: a freshly restarted Home Assistant subscribed and then waited for a value the firmware had no reason to send. The `homeassistant/status` transition from `offline` to `online` now resets the publish gates, so every tracked value re-publishes as first-seen. That covers the 128 OpenTherm message-id slots, the status and ventilation-status bit and byte fan-outs, the ASF, RBP and Remote Override fan-outs, and `hvac_mode` and `hvac_action`. The republish is paced by OpenTherm bus arrival rather than emitted in one burst, and discovery itself is untouched, so the just-in-time discovery behaviour is unchanged. Reported with a full diagnostic capture that recorded a real Home Assistant restart, which is what made the cause findable. (TASK-1058, ADR-088)
- `hvac_mode` and `hvac_action` could latch a value Home Assistant never received. Both publishers discarded the result of the MQTT send and updated their in-memory cache regardless, while the force flag that triggered the send was already cleared. A single dropped publish therefore stranded the topic until the thermostat mode genuinely changed. They now update the cache only after a confirmed send, and fall back to the unset state otherwise so the next OpenTherm frame retries. (TASK-1058)

- `hvac_mode` and `hvac_action` are now re-published at least every 5 minutes even when the value does not change. Both were the only on-change gated topics without an interval heartbeat: every neighbouring status topic already republishes once a minute, but these two were re-sent only on a real change or a Home Assistant restart, so a consumer that missed the last publish stayed stale indefinitely. Measured on a bench gateway before the fix, `hvac_mode` published exactly once across a ten minute capture. A genuine value change still publishes immediately, and the heartbeat timestamp only advances on a confirmed send, so a dropped publish retries on the next OpenTherm frame instead of restarting the five minute window. (TASK-1060)
- OpenTherm message IDs 131, 132 and 133 (the Remeha vendor messages: dF-/dU-codes, service message, and detection of connected SCUs) were never decoded. They arrived, and were logged as `Unknown message`, so they produced no labelled value and no Home Assistant entity. The internal message-id list placed those three entries three positions too low, at 128 to 130, which are unassigned OEM ids; the message table had them correctly at 131 to 133. Ids 128 to 130 meanwhile produced label-less output because they were being decoded as if they were the Remeha messages. Numbering now follows the OpenTherm data-id reference shipped in `docs/opentherm specification/`. Owners of a Remeha qSense or Tzerra gain three working sensors. (TASK-1064)
- Four ventilation and heat-recovery status topics (`vh_fault`, `vh_ventilation_mode`, `vh_bypass_status`, `vh_bypass_automatic_status`) only ever published once, at startup. They never refreshed on their one minute heartbeat, so if Home Assistant missed that first message the entity stayed empty until the gateway was rebooted. The slave-side status bits shared their publish timers with the master-side bits, and the master fan-out reset those timers microseconds before the slave fan-out read them, so the interval never elapsed. The two remaining bits were unaffected because their timers had no master counterpart, which is what made the pattern identifiable. (TASK-1066)

### Changed
- The "MQTT Home Assistant Reboot Detection" setting is deprecated and no longer shown in the web interface. Detecting a Home Assistant restart now always requires observing it go offline first, which is what that setting used to switch off. Requiring the full offline-to-online transition also means a retained Home Assistant birth message, which some setups publish and the broker then replays on every reconnect, cannot trigger a republish on an ordinary reconnect. The setting is still read from and written to `settings.ini`, so existing configurations load unchanged; it simply no longer affects behaviour. (TASK-1058, ADR-088)

### Added
- `SHA256SUMS` and the source bundle are attached to stable GitHub releases again. (TASK-936)
- An OpenTherm decode-coverage simulation fixture with a golden-baseline regression gate, runnable against a device in one command. It independently surfaced the ventilation heartbeat starvation fixed in this release. (TASK-1062, TASK-1063, TASK-1065)

## [1.7.2] - 2026-07-30

Long-run stability release for the 1.x (ESP8266) line: two genuine heap leaks fixed, both of which drained long-running devices to an out-of-memory reboot after roughly 1 to 1.5 hours. v1.7.0 fixed heap fragmentation; this release fixes the leaks that remained. No breaking changes versus v1.7.1. Full notes: [RELEASE_NOTES_1.7.2.md](RELEASE_NOTES_1.7.2.md).

### Fixed
- Heap leak on every DHCP lease renewal on networks whose DHCP server advertises an NTP server (option 42), for example Pi-hole and some D-Link routers. The prebuilt lwIP2 in core 2.7.4 is compiled with `LWIP_DHCP_GET_NTP_SRV=1`, so each renewal pushed the router's NTP server into the SDK SNTP module and leaked memory until the device ran out and rebooted. Field signature was an uptime-locked onset at the first lease renewal (typically around 90 minutes) followed by a reset, which looked like the discovery-republish leak but is a separate cause. The firmware runs its own NTP client, so DHCP-supplied servers were never wanted: `sntp_servermode_dhcp(0)` is now called at the top of `setup()`, before the persistent-WiFi auto-connect can complete DHCP. (TASK-1050)
- Crash when an mDNS answer arrived while the heap was exhausted (`Exception (2) epc1=0x40233cba excvaddr=0x8`). Plain `new` in ESP8266 core 2.7.4 returns NULL on failure but still runs the constructor, so the six `stcMDNS_RRAnswer` allocation sites in `_readRRAnswer()` dereferenced NULL. They now use `new (std::nothrow)` with a null guard; the callers already handled a NULL answer. The core tree is gitignored, so `build.py` re-applies this patch idempotently after every core install. (TASK-1049)
- Spontaneous heap death and ~1 to 1.5 hour reboot loop on long-running devices (the "true leak", distinct from the 1.7.0 fragmentation gating). Root cause: the automatic MQTT discovery-verify (ADR-062) subscribed to `homeassistant/+/<node>/#` to count retained discovery configs, but under the reduced PubSubClient buffer it read back only a fraction of them, falsely declared the rest missing, and triggered a full discovery republish that re-armed every hour. Verify to false-missing to republish leaked heap until the device ran out of memory and the external watchdog reset it. The verify readback is removed. The daily auto-heal is now an unconditional, heap-gated drip republish of the retained configs (guarded by MQTT connected, no drip already in progress, and a healthy largest-contiguous block), and the hourly first-run retry is deleted. No wildcard subscribe, no count, no false-missing, no retry storm. Confirmed on the bench: 5 hour soak with MQTT connected to a real broker holds the free heap flat where the previous build died at about 80 minutes. (TASK-1037, TASK-1048, ADR-087)
- Gateway-mode and OTGW-connected Home Assistant discovery entities are queued at boot so they publish once and self-heal, instead of only appearing after a mode change. (TASK-1035)

### Changed
- NTP now resyncs once per day instead of every 30 minutes, cutting periodic allocation churn on long-running devices. (TASK-1046)
- Web UI ticks the clock locally and polls the device less often; the two UI-polled endpoints (`/api/v2/otgw/otmonitor`, `/api/v2/device/time`) are rate-limited to 1 request per second server-side, reducing the HTTP load a couple of open dashboard tabs put on the device. (TASK-1043, TASK-1044)

### Added
- Per-second heap sampling in the onset window for leak diagnosis, off by default. (TASK-1037)

### Documentation
- `scripts/capture-heap-soak.bat`: browser-free, low-perturbation long-run capture preset for confirming heap stability over a 24 hour soak on a fixed build. Complements `capture-heap-onset.bat` (which hunts the leak onset on an unfixed build). (TASK-1037)

## [1.7.1] - 2026-07-09

Home Assistant integration release for the 1.x (ESP8266) line: cooling support in the climate entity plus new gateway and device-health auto-discovery sensors. No breaking changes versus v1.7.0. Full notes: [RELEASE_NOTES_1.7.1.md](RELEASE_NOTES_1.7.1.md).

### Added
- Unified off/heat/cool Home Assistant climate entity. The MQTT climate ("Thermostat") entity now represents cooling: `modes` are `off`/`heat`/`cool`, driven by new `hvac_mode` and `hvac_action` topics computed from the OpenTherm status bits. Cooling-capable systems (for example a Honeywell Round Modulation Heat/Cool on a heatpump) previously showed as heating-only, carrying the cooling setpoint with no way to reflect cooling. Mode is reflective (the thermostat owns heat/cool switching), and `hvac_action` reads the central-heating bit rather than flame, so DHW hot-water draws do not show as heating. Validated against real gas-boiler (idle, DHW, active heating) and heatpump (cooling) captures. (GH #665, ADR-085)
- `hvac_mode` and `hvac_action` exposed as standalone discoverable Home Assistant sensors (off/heat/cool and off/idle/heating/cooling). (TASK-941)
- Gateway mode and OTGW-connected state as Home Assistant binary-sensor auto-discovery entities, gated by the discovery index so they publish once and self-heal. Home Assistant can now see whether the gateway is in gateway or monitor mode and whether the OTGW link is up, without scraping the debug page.
- Uptime, unsupported-message-id count and per-path fragskip counters (`http_fragskips` / `mqtt_fragskips` / `ws_fragskips`) as Home Assistant auto-discovery sensors, so long-running-device health and the heap-fragmentation back-off activity are visible in Home Assistant.

### Fixed
- Build break on the uptime sensor: the discovery table referenced `HaUnit::s` (seconds) but that unit was never added to the `HaUnit` enum or its string mapper, so the firmware did not compile. Added the `s` unit (the uptime value is published in seconds). The climate, gateway-mode, OTGW-connected and uptime/fragskip entities therefore reach devices for the first time in this release.
- Corrected a stale Mosquitto winget package ID in the `capture-mqtt-debug` diagnostic helper, which broke its automatic MQTT-client install step. (GH #666, PR #671)

## [1.7.0] - 2026-06-25

Stability and headroom release for the 1.x (ESP8266) line. Headlined by heap-fragmentation crash-proofing (beta.1 to beta.6) and a RAM / heap-headroom optimization pass (beta.7 to beta.34) for long-running devices under load. Full notes: [RELEASE_NOTES_1.7.0.md](RELEASE_NOTES_1.7.0.md).

### Changed
- RAM and heap-headroom optimization pass (beta.7 to beta.33). The OpenTherm message-name table (`OTmap[]`) was moved into flash (PROGMEM) using inline table arrays, freeing about 4.96 KB of static RAM and lifting the idle free heap by about 6 KB. The largest-contiguous-block floor under sustained load rose from about 4.9 KB to about 11 KB, restoring the headroom the firmware had at 1.3.5. Roughly 25 smaller buffer and integer-width trims followed (OpenTherm log and topic buffers, MQTT namespace and client-id buffers, the JSON coalescing send buffer, the REST API URI tokenizer, the OT command queue, several diagnostic counters), plus two String-allocation removals (`dBmtoQuality()`, `upTime()`) that cut per-call heap churn. Net effect: about 6.6 KB of static RAM reclaimed with no functional change. Each step shipped under its own prerelease tag and was bench-verified (build, evaluator, on-device soak). (TASK-903)

### Fixed
- MQTT broker port above 32767 was stored as a negative value because the field was `int16_t`, so high non-standard ports could not be saved. The field is now `uint16_t` (full 1 to 65535 range). (beta.32, TASK-903)
- S0 pulse-counter pulses-per-kWh values above 65535 wrapped: the input validator allowed 1 to 100000 into a `uint16_t` field. The validator now constrains to the field's 1 to 65535 range. (beta.33, TASK-903)
- Debug Information tab showed raw key names for the runtime WiFi values (`wifi_current_subnet` / `_gateway` / `_dns1` / `_dns2`) and the simulation flag. These now render friendly labels, with a "(current)" qualifier distinguishing the runtime values from the configured static-IP fields. (beta.34, TASK-904)

### Fixed
- Random reboots on 1.6.x/1.7.x under sustained load: mainloop root cause (beta.6). A field bisect of the 1.6.0 cycle pinned a second fragmentation driver. TASK-651 had replaced the trailing `delay(1)` in `doBackgroundTasks()` with `yield()`, which uncapped the main loop. Under sustained decode + MQTT + WebSocket/HTTP load the unbounded loop fragments the heap (the largest contiguous block collapses while total free still looks fine) until an allocation fails and the device reboots. beta.6 restores `delay(1)` (the field-proven pre-TASK-651 behaviour, stable through 1.6.0-beta.13), which caps the loop at ~1 kHz and yields to the SDK. Reproduced and confirmed on a bench via a synthetic boiler-traffic replay plus MQTT load. (TASK-901)
- HTTP static-file serving crash, fixed at the source (beta.4). The firmware-side heap gates in beta.2/beta.3 fired but could not stop the crash, because the faulting allocation is a per-TCP-segment `new uint8_t[~1460]` *inside* the ESP8266 core's `streamFile` loop: serving a large asset (e.g. the ~250 KB `index.js`) fragments the heap mid-transfer, a later chunk's unchecked `new[]` returns NULL, and the core writes to it (the `0x4000df64` ROM `memcpy` fault). A firmware entry-gate cannot see between the core's per-chunk allocations. beta.4 patches the core at build time (ADR-084): `BufferedStreamDataSource::get_buffer()` now returns NULL instead of copying into a failed allocation, and `ClientContext::_write_some()` treats that as "retry later", so a mid-transfer low-memory moment drops/retries the connection instead of crashing. The core is a board-manager install reapplied by `build.py` on every build. (TASK-844)

### Fixed
- HTTP static-file serving crash under heap fragmentation (precise root-cause fix, beta.3). Analysis pinned the fault to the ESP8266 core: serving a static asset (`streamFile()`) makes the core's `BufferedStreamDataSource` allocate one TCP segment (~1460 bytes) per chunk with an unchecked, non-throwing `new[]`; under heap fragmentation that returns NULL and the following copy writes to a NULL destination (the ROM `memcpy` fault seen in the field). Static-file serving is now gated on the largest contiguous block (`HTTP_SERVE_MIN_MAXBLOCK`, 2048 bytes, sized just above the ~1460-byte allocation): when the contiguous block is too small the request gets a `503` to retry, instead of crashing. This replaces the beta.2 gate, whose threshold sat right at the cliff. (TASK-843)
- HTTP-load heap-fragmentation crash (root cause). A zero-browser A/B test confirmed that HTTP request serving is the dominant heap fragmenter: with no web UI traffic the device ran for 41 minutes at a healthy contiguous-block floor and zero crashes, while under continuous browser/dashboard load the largest contiguous block collapsed and a later allocation faulted (ROM `memcpy` to a NULL destination). The steady-state web handler now backs off under fragmentation: `httpServer.handleClient()` is skipped while the largest contiguous block is below the safe floor, letting the heap coalesce (clients retry); flash-upload handlers are never gated so OTA and PIC updates still complete. New `http_fragskips` counter on telnet and MQTT stats. Under heavy load the UI may briefly slow or refuse a request instead of rebooting. (TASK-841)
- Heap-fragmentation crash-loop on long-running ESP8266 devices (crash-proofing). A 6-version field-bisect showed the largest contiguous heap block collapsing to a few hundred bytes within a minute (while total free still looked adequate), after which the next contiguous allocation returned NULL and was written, faulting the device. The publish and socket paths are now crash-proofed: MQTT and WebSocket sends gate on the largest contiguous block (not just total free) and skip gracefully when it is too small, emergency heap recovery no longer allocates a listener socket while heap is critical (it stops the OTGWstream listener and re-arms it once heap recovers), and MQTT reconnect now uses exponential back-off to cut lwIP socket churn during an outage. New `mqtt_fragskips` and `ws_fragskips` counters are exposed on telnet and MQTT stats. (TASK-837)

### Added
- Active gateway overrides are now surfaced over MQTT and in the Web UI, so override state set on the PIC is visible to Home Assistant and the dashboard (7d391106).
- WiFi signal quality (percentage and label) is shown alongside RSSI in the telnet welcome banner (TASK-834).

### Documentation
- Documented the intentional unauthenticated network identity exposed on `/api/v2/device/info` (trusted-LAN security model) (TASK-807).
- Corrected stale `is_value_valid` and `print_f88` source comments.

## [1.6.1] - 2026-05-31

Focused follow-up to v1.6.0: MQTT on-change publishing as the default, a redesigned boiler-unsupported diagnostics panel, and MQTT/Web UI reliability fixes. Full notes: [RELEASE_NOTES_1.6.1.md](RELEASE_NOTES_1.6.1.md).

### Changed
- MQTT on-change publishing is now the default (ADR-081). New setting `MQTTonChangePublishing` defaults to `true`, and the publish interval defaults to `60` seconds: changed OpenTherm values publish immediately, unchanged values refresh once per minute. On upgrade, a config that still has `MQTTinterval=0` is migrated once to `60` (persisted via the deferred settings write). Untick "Publish on change" (or set `MQTTonChangePublishing=false`) to restore legacy publish-every-message behaviour.
- Boiler-unsupported diagnostics panel on the Statistics tab is now a table (MsgID, Description, OpenTherm Name, Direction) with human-readable names, styled as a notice card in light and dark themes; `/api/v2/otgw/boiler-support` now also returns the friendly name.
- MQTT periodic publishes are spread with timer jitter so the 5-minute and 60-second timers no longer fire together and cause a burst-publish heap spike.

### Fixed
- MQTT: a partial or failed chunk write now drops the TCP connection instead of finalising a truncated discovery or value publish.
- Web UI: CSS is served with `no-cache` + ETag revalidation instead of a long-lived cache, so styling updates appear after a firmware flash (one hard refresh may be needed the first time after upgrading).
- Mobile settings field labels no longer wrap awkwardly on narrow screens; statistics table column resizing corrected; boiler diagnostics tooltip exposed.

### Added
- `scripts/capture-mqtt-debug.bat`: Windows helper to capture MQTT debug output for issue reports.

## [1.6.0] - 2026-05-28

MQTT and HA reliability overhaul, static IP support, bilateral OT-bus diagnostics, and mainloop performance improvements. Full notes: [RELEASE_NOTES_1.6.0.md](RELEASE_NOTES_1.6.0.md).

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
