# OTGW-firmware v1.7.6 Release Notes

**Release date:** 2026-09-28
**Previous release:** v1.7.5
**Platform:** ESP8266 (1.x maintenance line)

## Overview

v1.7.6 is a reliability release for the 1.x (ESP8266) line, driven by field reports on GH #682 and GH #685. It fixes a chain of MQTT defects that ended in "malformed packet" disconnects and, in the last case, corrupted values arriving in Home Assistant. It also fixes the Home Assistant OpenTherm Gateway integration failing to connect over port 25238, and brings back two clients on that port under a rule that keeps their commands apart.

One breaking change versus v1.7.5: port 25238 accepts two clients again, with one writer at a time. Details below.

## Bug fixes

### MQTT: corrupted values and malformed-packet disconnects (GH #682)

Four defects on the same path, found and fixed in turn over the beta cycle:

- **Half a PUBLISH header could stay on the wire.** When the TCP send buffer stayed full, `beginPublish()` wrote part of the header and reported failure, the connection stayed up, and the broker read the next packet as the payload that header promised. Mosquitto then dropped the gateway with "malformed packet". A failed header write now drops the link. (TASK-1134)
- **A publish that cannot be framed is no longer started.** Every publish path, discovery included, checks the send buffer for the header and topic before starting, and waits briefly for room instead of starting a packet it cannot finish. Two counters, `mqtt_sndbuf_skips` and `mqtt_desync_drops`, are readable in `/api/v2/device/info`. (TASK-1154, TASK-1155)
- **The 5-minute status burst no longer loses its last dozen topics.** The send-buffer check waited only a few scheduler turns, shorter than one acknowledgement round trip, so the `otgw-pic/settings/*` topics were skipped every 5 minutes. It now waits up to 200 ms of wall-clock time. On a bench gateway all 15 settings topics arrived in every burst. Reported by jaronbor. (TASK-1163, TASK-1164)
- **Home Assistant could receive values with their last two bytes replaced by `E0 00`**, for example `10.\xe0\x00` on TSet. When a publish could not be written completely, the gateway dropped the link with PubSubClient's `disconnect()`, which first sends the MQTT DISCONNECT packet (`E0 00`). The broker was still inside the unfinished PUBLISH and took those bytes as payload. The link is now closed without writing anything, so the broker discards the unfinished frame. Measured on a bench gateway against a broker that stops reading mid-burst: the previous build put `E0 00` into 6 of 6 truncated frames, this build into 0 of 8. Reported by mrfox7688. (TASK-1166)

### Port 25238: the Home Assistant integration could not connect (GH #685)

The first command a client sent right after connecting was read and thrown away at accept. The integration writes `PS=0` about 20 ms after the handshake and waits for the answer, so it failed with `cannot_connect` while OpenTherm frames kept streaming and made the port look healthy. Reported by petrister. (TASK-1146)

### The boiler and thermostat could stay "connected" after the PIC went silent

The liveness timeout was only evaluated when a frame arrived, so a PIC that stopped talking left the flags on their last value until a reboot, and the publishes were also gated on PIC presence. Silence is now evaluated on a 3 second tick and published. (TASK-1135)

### Other fixes

- The debug console could drop output without any sign of it. The library now reports the real byte count and counts what was lost as `telnet_tx_dropped` and `otgwstream_tx_dropped` in `/api/v2/device/info`. (TASK-1148)
- PIC functions stayed disabled after a missed boot detection while diagnose or interface firmware was loaded. Recovery now triggers on any recognised firmware banner. (TASK-1126)
- Two dashboards open at the same moment could leave one of them refused forever; the poller now re-phases after a refusal. (TASK-1090)
- An interrupted upload leaked a file handle, and the main page kept streaming to browsers that had gone away. Both fixed. (TASK-793, TASK-841, TASK-843)
- The USB serial capture scripts read at 115200 baud while the firmware talks to the PIC at 9600. They now use 9600. (TASK-1140)

## New features

### A diagnose screen in the web interface (TASK-1127)

When the PIC runs Schelte Bron's diagnostic firmware, the gateway recognises it and shows a PIC diagnose page, so the test menu no longer needs a separate terminal tool. It is the home screen while that firmware is loaded and disappears when a gateway PIC is detected again.

### Device Info shows who holds port 25238 (TASK-1167)

`/api/v2/device/info` and the Device Info page show the number of connected clients (`otgwstream_clients`), their addresses (`otgwstream_client_ip`) and the last refused address (`otgwstream_last_refused_ip`). If OTmonitor cannot connect, this shows what holds the port.

### A reboot command on the telnet console (TASK-1089)

`R` on the telnet console requests a deferred reboot. It is the recovery route for a gateway whose HTTP heap gate has engaged, which can then be neither flashed nor rebooted over HTTP.

### The Linux, WSL and macOS capture script matches the Windows one (TASK-1156, TASK-1161)

`capture-otgw.sh` now collects the same sources as `capture-mqtt-debug.bat` and writes one transcript to attach. It runs on the bash that ships with macOS and is attached to every release.

## Breaking changes

### Port 25238 accepts two clients again, with one writer at a time (ADR-097, TASK-1167)

v1.7.5 limited the port to one client. v1.7.6 allows two, for example the Home Assistant OpenTherm Gateway integration next to OTmonitor:

- Both clients receive everything the PIC sends.
- Only one client at a time can send to the PIC. It keeps that right until it has been quiet for 100 ms; the other client's command waits and then goes through whole. A command can be delayed by up to 100 ms.
- Every byte reaches the PIC exactly as sent. The port stays fully transparent.
- Once a client sends binary data, as during an OTmonitor PIC upgrade, it keeps the right to send until it has been quiet for 3 seconds. Upgrading the PIC over the network remains discouraged; use the web interface's PIC flash page.
- A third connection is refused. From the address of a connected client, it replaces that client instead.

Verified on a bench gateway with the serial side captured: every byte value arrives unchanged, 4804 concurrent commands from two clients arrived without a single mix-up, and heap with two clients over 30 minutes matched a single client.

## Known limitations

- A port-25238 client that takes nothing from the gateway for one second loses the output of that moment and is disconnected after a few seconds. This is not new; earlier versions lost the same bytes without counting them. `otgwstream_tx_dropped` now shows it.
- Tools that assume they are alone on the line, such as the Home Assistant integration's `pyotgw` library, can be confused by answers to the other tool's commands, most visibly while they connect.
- When the MQTT broker stops reading in the middle of a large message, the gateway can pause for up to about 8 seconds before it drops the link and reconnects. Since this release the reconnect never delivers a corrupted value.

## Upgrade notes

- Flash both the firmware and the filesystem. Settings are preserved.
- Coming from v1.7.5 there is nothing to migrate. If you ran OTmonitor and Home Assistant side by side and stayed on v1.7.4 because of the one-client limit, you can move up.
- After upgrading, `mqtt_sndbuf_skips` shows a few dozen right after boot from the discovery burst. That is normal; a value still climbing after boot has settled points at a stalling network link.

## Thank you

mrfox7688 ran five betas on GH #682 and sent broker logs, Home Assistant logs and counter readings each time. Every MQTT fix in this release came out of those reports, including the last one, where the exact bytes of a corrupted value pointed straight at the cause. jaronbor reported the same problem and confirmed the skipped-topics fix.

petrister found that the Home Assistant integration could not connect over port 25238. Schelte Bron and iandury_ asked for two clients on that port again. Schelte Bron also reviewed the diagnose screen. Thanks as well to Appiejs, DenSinH and SiFU-WU for their reports, and to everyone who flashed a beta.

## Full detail

See [CHANGELOG.md](CHANGELOG.md) for the complete list, including internal changes.
