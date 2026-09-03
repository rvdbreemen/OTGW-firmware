# OTGW-firmware v1.7.5 Release Notes

**Release date:** 2026-09-04
**Branch:** `otgw-1.x.x` (1.x maintenance line, ESP8266)
**Previous stable release:** [v1.7.4](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.7.4) (2026-08-10)

## Overview

v1.7.5 closes a credential leak, makes updating the PIC firmware from the web work again, and adds a cumulative hot-water total for the Home Assistant Energy dashboard.

If you have set an admin password on your gateway, the security fix alone is worth this upgrade: `settings.ini` was served without authentication, and it holds that password and your MQTT password in cleartext.

There is one breaking change: port 25238, the OTmonitor bridge, now accepts one client instead of two. See "Breaking changes" below.

## Security

### `settings.ini` was served without authentication (TASK-1099, ADR-056)

The file holds `httppasswd` and `MQTTpasswd` in cleartext, and the file-serving path had no authentication check while every sibling operation had one. An unauthenticated client on the local network could read both credentials from a password-protected gateway and replay them. The read is now gated with the same check as the delete and reboot operations, including a leading `//settings.ini` alias that resolves to the same file. Verified on a live gateway: with a password set the request returns 401 without credentials and 200 with them.

If you run a gateway with an admin password on a network you do not fully trust, change that password and your MQTT password after upgrading.

### A CORS preflight could reach protected routes unauthenticated (TASK-1100, ADR-054, ADR-056)

`checkHttpAuth()` short-circuited `OPTIONS`, so a preflight reached the delete, reboot and reset routes without credentials. In the same area, the same-origin check could never read its own headers: `collectHeaders()` registered only `If-None-Match`, so `Origin` and `Referer` were invisible and the check always passed. Both are fixed, which turns origin enforcement on for every route that calls `checkHttpAuth()`. Verified on hardware: an unauthenticated `OPTIONS` returns 401, and an authenticated cross-origin POST returns 403.

### The `/upload` same-origin check ran after the file was written (TASK-1103, TASK-1101)

A cross-origin POST overwrote the target on disk and was refused only afterwards. The check now runs at the first chunk, before the file is opened, so a rejected upload writes nothing.

## Bug fixes

### Updating the PIC firmware from the web did not work

Three separate defects sat between the button and the PIC, reported by Schelte Bron, author of the PIC firmware.

The WebSocket server stopped listening for good after a single WiFi reconnect. `refreshServicesAfterWifiReconnect()` dropped stale clients with a call that also closes the listening socket and clears the running flag, and `begin()` is only ever called from `setup()`. One reconnect left port 81 refusing every connection until the next reboot. Measured on a gateway with seven days of uptime: port 81 answered with a TCP reset while a freshly booted sibling accepted the handshake normally. (TASK-1107)

Clicking flash then did nothing at all, because the browser gated the whole operation on that WebSocket. It waited five seconds and gave up with "Connection timed out. Cannot track progress." before the upgrade request was ever sent, even though the flash back end was healthy. Flash progress rides on HTTP polling anyway (ADR-025), and that poller is already armed by the time the gate runs, so a missing WebSocket costs live log lines and not the flash. The request now goes out either way. Verified by driving the endpoint with port 81 closed: gateway.hex went from 6.6 to 6.8 in 27 seconds, 0 errors, 0 retries. (TASK-1108, ADR-025)

Output on the OTmonitor bridge only arrived once a complete line had been assembled. The serial-to-network path on port 25238 buffered bytes until a CR or LF arrived, so a prompt carrying no newline never reached a connected client, and a CR, LF or NUL inside a payload was consumed as a terminator and replaced by a synthesised CRLF. The diagnose PIC firmware declares its prompt with a trailing sentinel byte that is never transmitted, so the prompt stayed stranded while the PIC blocked waiting for input. Transport and parsing are now separate concerns: every byte is forwarded verbatim as it is read, coalesced in a 64 byte buffer so a burst does not become one TCP segment per byte, while line assembly continues to feed the OpenTherm parser only. Verified on hardware: 69 of 98 reads on port 25238 no longer end on a line terminator, which the previous code could not produce. (TASK-1109, ADR-095)

### Boot commands were typed into the diagnose firmware's menu (TASK-1121)

The diagnose PIC image is a menu that reads single keystrokes, so the default `GW=1` boot command never arrived as a command: `G`, `W`, `=`, `1` and CR each selected a menu entry and started a test. Reported by Schelte Bron. Boot commands are now skipped on a PIC identified as diagnose, and are sent once the PIC has identified itself rather than blindly at the end of `setup()`, where no banner has been read yet and there is no firmware type to test. A PIC that never sends a banner still gets its boot commands after three seconds, so the gate can only ever spare a confirmed diagnose PIC. Verified on a gateway running diagnose 2.2.

### The HTTP heap gate could hold itself shut (TASK-1039, ADR-091, ADR-092)

`canServeHttp()` withholds `handleClient()` below the contiguous-block threshold, but `handleClient()` is also the only code that drains the web server's unclaimed-connection queue, and a pending connection releases its buffers only at refcount zero. While the gate was shut, every pending connection kept its pcb and buffers, so the block the gate waited for could never return. In field logs the largest block then oscillated between 480 and 1872 bytes and never crossed 2048 again for the rest of the run. Pending connections are now reaped directly, without running the handler or its multipart parser.

### The file explorer had a batch of defects (TASK-1096, TASK-1098, TASK-1101 to TASK-1104)

Uploads went to the root whatever directory you were browsing, which made it impossible to place a self-built PIC hex in its `pic16f<type>` directory. The free-space guard could be defeated by an integer underflow once the filesystem passed 95.2 percent full, so the interface accepted a file that could not be stored. An upload that wrote nothing still answered with a success redirect. Names containing `#`, `?`, `%` or `+` could not be opened, downloaded or reliably listed. The delete handler could run off the end of its path buffer. The bootstrap helper page kept showing after a successful upload until a reboot. All fixed and verified on a live gateway.

### The telegraf endpoint bypassed the REST rate limit (TASK-1090, ADR-086)

`/api/v2/otgw/telegraf` and `/api/v2/otgw/otmonitor` route to the same handler and return the same payload, but only otmonitor was listed as rate limited, so a client on the telegraf path polled uncapped. Both paths now share one budget.

### Older fixes carried in from the beta cycle

The "boiler does not implement" panel could name a message id the boiler demonstrably answers (GH #677, TASK-1080). Settings values longer than 149 characters were silently truncated and reported as saved (TASK-1082, TASK-1083). The DHW flow rate sensor went unavailable in Home Assistant because its unit string did not match its device class (GH #675, TASK-1092).

## New features

### Cumulative DHW water total for the Energy dashboard (GH #675, TASK-1091, ADR-093, ADR-094)

The Energy dashboard needs a cumulative volume, not a rate, and Home Assistant MQTT discovery cannot create the integration and template helpers that would build one host-side, so every user had to wire that up by hand against their own entity id. The gateway now integrates MsgID 19 itself and publishes `dhw_water_total` as an auto-discovered sensor (`device_class: water`, unit `L`, `state_class: total_increasing`), selectable in the Energy dashboard water section with no configuration.

Two properties are deliberate. The gateway does not poll MsgID 19, so an interval longer than 60 seconds between frames counts as a gap in the measurement and adds nothing: a bus that falls silent at 8 l/min must not keep booking water that never flowed. And the total is not persisted, because `total_increasing` reads the drop after a reboot as a meter reset and keeps the long-run sum. Nothing is published, and no entity is created, until a MsgID 19 frame has actually decoded, so installations whose thermostat never requests that id do not gain a meter pinned at zero.

### Bundled PIC images updated to gateway 6.8 and diagnose 2.2 (TASK-1122)

The filesystem image shipped gateway 6.6 and diagnose 2.1. Both are now the current upstream images, byte-identical to the files on Schelte Bron's site, so a fresh flash no longer starts behind. Gateway 6.8 also carries the PIC-side fix behind GH #677 and GH #678, where MsgID 24 stopped reaching the boiler after updating to PIC 6.7.

### The PIC firmware table marks what a download changed (TASK-1110)

The refresh button rewrote the version cell in place and never touched the size cell, so a click that downloaded a new image looked exactly like a click on a file that was already current. Both cells now go through one helper that compares before it writes, so a value that actually changed is highlighted for about fifteen seconds and an unchanged value stays visually quiet.

## Breaking changes

### Port 25238 accepts one client instead of two (TASK-1115)

The OTmonitor bridge is a bidirectional serial link, and the library underneath has no per-client stream identity: the read path serves whichever connected client happens to have data. Two clients writing at the same time spliced their bytes into a single command stream toward the PIC, so one client's partial line could concatenate with another's into a malformed command. Within one client the order always held; between clients it did not. The port now has a single slot, which removes that failure mode outright.

The cost is stated plainly: the second slot existed for "Home Assistant plus one debug consumer", and the library cannot tell a reader from a writer, so a passive second consumer is refused as well. If you run the Home Assistant OpenTherm Gateway integration and OTmonitor against port 25238 at the same time from different machines, the second connection is now refused. A reconnect from the same address still takes the session over, so a client that crashed or was killed reclaims the port immediately instead of waiting for a timeout.

If this affects you, please say so in `#beta-testing` on Discord. Serving one writer alongside several readers is possible, but it needs a per-client read API in the library, and whether that work is worth doing depends on how many people are actually in this situation.

## Behaviour changes

Configured boot commands are sent up to three seconds later than before. They were already queued rather than written directly to the serial port, and the queue only drains from the same one-second task, so the change to when the PIC actually sees them is smaller than the change to when they are queued.

Origin enforcement is now active on every route that calls `checkHttpAuth()`, which it was not before. This only engages on a gateway with an admin password set; a passwordless gateway is unaffected (ADR-054, ADR-056).

## Upgrade notes

Flash **both** the firmware and the filesystem. A new firmware with an old filesystem keeps serving the previous web interface, which is the most common cause of "the fix is not there" after an update. Settings are preserved.

Update over the air from the web interface, or use `flash_otgw.bat` or `flash_otgw.sh` from the release assets.

After upgrading, if you had an admin password set on a network you do not fully trust, change that password and your MQTT password.

## Thank you

Schelte Bron, author of the OpenTherm Gateway PIC firmware, found and reported the serial transparency problem, the PIC update failures and the boot commands landing in the diagnose menu. Four of the fixes in this release exist because of those reports.

Jeroenll opened GH #675, which became both the flow rate correction and the new water total. RonVervoort and dafdaf01 reported the MsgID 24 regression that turned out to be PIC-side and is addressed by the bundled gateway 6.8. Thanks also to everyone who flashed a beta and reported back.

## Full detail

Per-commit detail is in [`CHANGELOG.md`](CHANGELOG.md). Architectural rationale is in the linked ADRs under [`docs/adr/`](docs/adr/).
