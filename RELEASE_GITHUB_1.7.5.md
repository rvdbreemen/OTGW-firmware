**A gateway with an admin password was handing out that password, and the PIC firmware update from the web is working again.**

v1.7.5 is a security and fix release for the 1.x (ESP8266) line. It also adds a cumulative hot-water total for the Home Assistant Energy dashboard, and ships the current PIC images.

One breaking change: port 25238 now accepts one client instead of two. Details below.

Full release notes: [RELEASE_NOTES_1.7.5.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.5.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## Security

- **`settings.ini` was served without authentication.** It holds `httppasswd` and `MQTTpasswd` in cleartext, and the file-serving path had no auth check while every sibling operation did, so an unauthenticated client on your LAN could read both credentials off a password-protected gateway and replay them. Now gated, including the `//settings.ini` alias that resolves to the same file. Verified on a live gateway. If your gateway has an admin password and sits on a network you do not fully trust, change that password and your MQTT password after upgrading. (TASK-1099, ADR-056)
- **A CORS preflight could reach protected routes unauthenticated**, and the same-origin check could never see its own headers, so it always passed. Both fixed, which turns origin enforcement on for every route behind `checkHttpAuth()`. (TASK-1100, ADR-054, ADR-056)
- **The `/upload` same-origin check ran after the file had been written.** A cross-origin POST overwrote the target and was refused afterwards. The check now runs before the file is opened. (TASK-1103, TASK-1101)

## Bug fixes

- **Updating the PIC firmware from the web works again.** Three defects sat between the button and the PIC, all reported by Schelte Bron, author of the PIC firmware. The WebSocket server stopped listening for good after a single WiFi reconnect, because dropping stale clients also closed the listening socket and `begin()` only runs from `setup()`. The flash button then gated the whole operation on that WebSocket and gave up before ever sending the request, even though flash progress rides on HTTP polling. And port 25238 buffered until a line terminator arrived, so a prompt carrying no newline never reached a client at all. Every byte is now forwarded verbatim, coalesced in a 64 byte buffer, with line assembly kept for the OpenTherm parser only. (TASK-1107, TASK-1108, TASK-1109, ADR-025, ADR-095)
- **Boot commands are no longer typed into the diagnose firmware's menu.** The diagnose image reads single keystrokes, so the default `GW=1` arrived as `G`, `W`, `=`, `1` and CR and selected menu entries at every boot. They are now skipped on a diagnose PIC, and sent once the PIC has identified itself instead of blindly at the end of `setup()`. Verified on a gateway running diagnose 2.2. (TASK-1121)
- **The HTTP heap gate could hold itself shut.** It withheld the only code that drains the pending-connection queue, and a pending connection releases its buffers only at refcount zero, so the memory the gate waited for could never return. In field logs the largest block never crossed the threshold again for the rest of the run. (TASK-1039, ADR-091, ADR-092)
- **A batch of file explorer defects.** Uploads always went to the root, the free-space guard could be defeated by an integer underflow above 95.2 percent full, an upload that wrote nothing still reported success, names with `#`, `?`, `%` or `+` could not be opened or downloaded, the delete handler could run off its path buffer, and the bootstrap helper page kept showing until a reboot. (TASK-1096, TASK-1098, TASK-1101 to TASK-1104)
- **The telegraf endpoint bypassed the REST rate limit** that the otmonitor endpoint already respected, though both route to the same handler. (TASK-1090, ADR-086)
- Also carried from the beta cycle: the "boiler does not implement" panel could name an id your boiler answers (GH #677), settings values over 149 characters were truncated and reported as saved, and the DHW flow rate sensor went unavailable on a unit-versus-device-class mismatch (GH #675).

## New

- **Cumulative DHW water total for the Energy dashboard.** The gateway integrates MsgID 19 itself and publishes `dhw_water_total` as an auto-discovered sensor (`device_class: water`, `L`, `state_class: total_increasing`), selectable in the Energy dashboard water section with no helper and no YAML. A gap longer than 60 seconds between frames counts as a gap in the measurement, not as flow. Nothing is published until a MsgID 19 frame has actually decoded, so a bus that never carries one does not gain a meter pinned at zero. (GH #675, TASK-1091, ADR-093, ADR-094)
- **The bundled PIC images are now gateway 6.8 and diagnose 2.2**, byte-identical to the current upstream files, so a fresh flash no longer starts behind. Gateway 6.8 also carries the PIC-side fix behind GH #677 and GH #678, where MsgID 24 stopped reaching the boiler after updating to PIC 6.7. (TASK-1122)
- **The PIC firmware table marks the cells a download actually changed**, so a refresh that fetched something new no longer looks identical to one that did not. (TASK-1110)

## Breaking change: port 25238 accepts one client instead of two

The OTmonitor bridge is a bidirectional serial link and the library underneath has no per-client stream identity: the read path serves whichever client happens to have data. Two clients writing at once spliced their bytes into a single command stream toward the PIC, so one client's partial line could concatenate with another's into a malformed command. The port now has a single slot.

The cost, stated plainly: the second slot existed for "Home Assistant plus one debug consumer", and the library cannot tell a reader from a writer, so a passive second consumer is refused as well. If you run the Home Assistant OpenTherm Gateway integration and OTmonitor against 25238 at the same time from different machines, the second connection is now refused. A reconnect from the same address still takes the session over, so a client that crashed reclaims the port immediately.

If that is you, please say so in `#beta-testing` on Discord. Serving one writer alongside several readers is possible, but it needs a per-client read API in the library, and whether that work is worth doing depends on how many people are actually in this situation. (TASK-1115)

## Upgrade notes

Flash **both** the firmware and the filesystem. A new firmware with an old filesystem keeps serving the previous web interface, which is the most common cause of "the fix is not there" after an update. Settings are preserved.

Update over the air from the web interface, or use `flash_otgw.bat` or `flash_otgw.sh` from the assets below.

## Thank you

Schelte Bron, author of the OpenTherm Gateway PIC firmware, found and reported the serial transparency problem, the PIC update failures and the boot commands landing in the diagnose menu. Four of the fixes here exist because of those reports.

Jeroenll opened GH #675, which became both the flow rate correction and the new water total. RonVervoort and dafdaf01 reported the MsgID 24 regression that turned out to be PIC-side and is addressed by the bundled gateway 6.8. Thanks also to everyone who flashed a beta and reported back.
