**The gateway recovers by itself from silent WiFi links, failed broker lookups and access point outages, and says why it went down.**

v1.7.8 is a network reliability release for the 1.x (ESP8266) line, driven by long field captures on GH #681 and GH #682. No breaking changes versus v1.7.7.

Full release notes: [RELEASE_NOTES_1.7.8.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.8.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## New

- **Dropped and refused PIC commands are reported.** `otgw-firmware/command_dropped` (for example `TT=22.0`, or `ZZ=1 (NG)` for a refusal), `otgw-firmware/command_retry_exceeded`, `cmd_dropped` in `/api/v2/device/info`, and two Home Assistant entities: Last Dropped Command and Command Dropped. (TASK-1431, TASK-1442)
- **The WiFi outage record**: reason code and name, access point, channel, duration and event count of the last outage, in the telnet banner and as `wifi_*` keys in `/api/v2/device/info`. (TASK-1278, TASK-1434)
- **`reboot_log.txt` names the reason for every planned restart**, for example `- WiFi: too many reconnect failures`. (TASK-1247)
- **The web interface works behind a reverse proxy and in a Home Assistant sidebar panel** (HA ingress, nginx on a sub-path), including FSexplorer upload and PIC flashing. HA shows a Visit link on the device page. Guide: [REVERSE_PROXY_AND_HA_INGRESS.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md). (TASK-1378)

## Bug fixes

- **A WiFi link that stays associated but passes no traffic** is now detected by pinging the router: reconnect after 8 minutes, restart after another 8, with guards against false alarms. Before, the gateway could stay off the network for over an hour. (TASK-1248)
- **MQTT comes back after a failed broker name lookup** instead of staying down until a restart. (TASK-1250)
- **The WiFi radio no longer dozes in modem-sleep**, so routers with a short inactivity check (ASUS, Broadcom) stop dropping it. (TASK-1261)
- **A gratuitous ARP every 60 s** keeps the router's address entry fresh. Confirmed in the field: no more 20-minute stalls on OPNsense. (TASK-1290)
- **After an access point outage the gateway rejoins within seconds**, instead of opening the setup portal for 4 minutes. Bench test: 7 s instead of 194 s. (ADR-099, TASK-1262)
- **The gateway joins WiFi under its own hostname**, and no longer sometimes stays off the network for 6 minutes after a restart. (TASK-1301)
- A filesystem update over WiFi no longer crashes on mDNS traffic (TASK-1285). The PIC reset cause no longer reaches HA as a raw `0xFF` byte (GH #688, TASK-1430). Uptime counts from boot (TASK-1289). `reboot_log.txt` is readable again (TASK-1253). The RF battery-code entity gets its value (TASK-1209).

## Behaviour changes

- `TP=` commands are no longer replaced in the command queue; each one is sent in turn.
- The WiFi radio stays on, which uses somewhat more power.

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

## Thank you

Special shoutout to **mrfox7688**, who ran the capture script around the clock for weeks, posted a detailed analysis of every beta on GH #682, and confirmed the GH #688 fix.

Thanks to **theaprilhare**, who pointed at modem-sleep with ASUS router logs, found the OPNsense ARP expiry, and confirmed both fixes with long test runs; **DenSinH** for the GH #681 reports; and **sjorsjuhmaniac** (Tweakers) for asking to see dropped commands in Home Assistant.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
