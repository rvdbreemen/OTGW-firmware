**MQTT stays clean under a slow network, and Home Assistant and OTmonitor can share port 25238 again.**

v1.7.6 is a reliability release for the 1.x (ESP8266) line, driven by field reports on GH #682 and GH #685.

One breaking change: port 25238 accepts two clients again, with one writer at a time. Details below.

Full release notes: [RELEASE_NOTES_1.7.6.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.6.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## Bug fixes

- **No more corrupted MQTT values or "malformed packet" disconnects when the network is slow.** Four defects on the same publish path, fixed in turn: half a PUBLISH header could stay on the wire; a publish could start without room to finish; the 5-minute status burst skipped its last dozen topics; and the recovery itself wrote the MQTT DISCONNECT packet (`E0 00`) into an unfinished message, which is how Home Assistant received values like `10.\xe0\x00`. The link is now closed without writing anything. Measured on a bench gateway against a broker that stops reading: `E0 00` in 6 of 6 broken frames before, 0 of 8 after. New counters `mqtt_sndbuf_skips` and `mqtt_desync_drops` in `/api/v2/device/info`. (GH #682, TASK-1134, TASK-1154, TASK-1164, TASK-1166)
- **The Home Assistant OpenTherm Gateway integration can connect over port 25238 again.** The first command after connecting was discarded, so the integration failed with `cannot_connect`. (GH #685, TASK-1146)
- **Boiler and thermostat no longer stay "connected" after the PIC goes silent.** (TASK-1135)
- Also: the debug console reports lost output instead of hiding it (`telnet_tx_dropped`, `otgwstream_tx_dropped`), PIC functions recover from any PIC firmware banner, two dashboards opened together no longer starve one of them, an interrupted upload no longer leaks a file handle, and the USB capture scripts read at the right baud rate.

## New

- **A diagnose screen in the web interface** when the PIC runs Schelte Bron's diagnostic firmware. (TASK-1127)
- **Device Info shows who holds port 25238**: connected clients, their addresses and the last refused address. (TASK-1167)
- **A reboot command (`R`) on the telnet console**, the recovery route when the web server is gated under memory pressure. (TASK-1089)
- **`capture-otgw.sh` for Linux, WSL and macOS** now matches the Windows capture tool and ships with every release. (TASK-1161)

## Breaking change: port 25238 accepts two clients again

Two clients can connect, for example the Home Assistant integration and OTmonitor. Both receive everything the PIC sends, and only one at a time can send: it keeps that right until it has been quiet for 100 ms, so commands can no longer get mixed. Every byte reaches the PIC unchanged. A client that sends binary data (an OTmonitor PIC upgrade) keeps the right for 3 seconds of silence. A third connection is refused, unless it comes from the address of a connected client, which it then replaces. (ADR-097, TASK-1167)

## Known limitations

- A port-25238 client that takes nothing for one second loses that output and is disconnected after a few seconds. Not new, now counted in `otgwstream_tx_dropped`.
- Tools that assume they are alone on the line, such as the Home Assistant integration, can be confused by answers to the other tool's commands.
- If the MQTT broker stops reading mid-message, the gateway can pause up to about 8 seconds before it reconnects. It no longer delivers a corrupted value when it does.

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

## Thank you

mrfox7688 ran five betas on GH #682 and sent logs and counter readings every time; every MQTT fix here came out of those reports. jaronbor reported and confirmed the same problem. petrister found the port 25238 connect failure, and Schelte Bron and iandury_ asked for two clients again. Thanks also to Appiejs, DenSinH, SiFU-WU and everyone who flashed a beta.
