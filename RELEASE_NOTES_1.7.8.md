# OTGW-firmware v1.7.8 Release Notes

**Release date:** 2026-11-01
**Previous release:** v1.7.7
**Platform:** ESP8266 (1.x maintenance line)
**Compare:** [v1.7.7...v1.7.8](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.7...v1.7.8)

## Overview

v1.7.8 is a network reliability release for the 1.x (ESP8266) line, driven by long field captures on GH #681 and GH #682. The gateway now recovers by itself from a WiFi link that stays associated but passes no traffic, from a failed broker name lookup, and from an access point that was away for a while. It no longer dozes in modem-sleep, keeps the router's address entry for it fresh, and joins WiFi under its own hostname. When something does go wrong, it says why: in `reboot_log.txt`, in the telnet banner and in `/api/v2/device/info`. Commands the PIC never confirms, or refuses, are now reported on MQTT and in Home Assistant. And the web interface works behind a reverse proxy, including a Home Assistant sidebar panel.

No breaking changes versus v1.7.7. A few behaviour changes are listed below.

## New features

### Dropped and refused PIC commands are reported (TASK-1431, TASK-1442)

After 5 tries the gateway drops a command the PIC never confirms. Until now only the debug log and the web log said so. The dropped command is now published on `otgw-firmware/command_dropped` (for example `TT=22.0`, not retained), `otgw-firmware/command_retry_exceeded` turns `ON` (retained) until the next confirmed command, and `/api/v2/device/info` counts the drops in `cmd_dropped`. Home Assistant gets a **Last Dropped Command** sensor and a **Command Dropped** binary sensor.

A command the PIC refuses with an error code, such as `ZZ: NG` for an unknown command or `PR: BV` for a register an older PIC does not have, used to be matched on its two letters and removed as if it were confirmed. It is now reported the same way, with the code (`ZZ=1 (NG)`), and is not retried. Checked with a host test on the real queue code: the old code fails it, the new code passes.

Asked for by sjorsjuhmaniac on Tweakers.

### The WiFi outage record: telnet banner and device/info (TASK-1278, TASK-1434)

The ESP closes every TCP connection, telnet included, the moment the link drops, so a disconnect could not be seen in a telnet capture. The firmware now records each outage: the 802.11 reason code with its name (first and last), the BSSID and channel of the access point that was left, how long the link was down, how many disconnect events it took, and whether the firmware's own dead-link check caused it. The telnet banner prints that record, the access point now joined, and the number of outages since boot. The same record is in `/api/v2/device/info` under keys starting with `wifi_`.

Checked on a bench gateway with router-forced disconnects: a deauth shows reason 2, a 90 s access-list block shows one outage of 101 s with 31 events, a dead-link reconnect shows reason 8 marked as self-caused. Asked for by mrfox7688 on GH #682.

### `reboot_log.txt` says why the gateway restarted itself (TASK-1247)

Every planned restart (WiFi supervisor, nightly restart, firmware update, reboot from telnet or the web interface, config portal timeout) was logged as the same `Software/System restart (4)`. The reason now follows on the same line, for example `- WiFi: too many reconnect failures`.

### The web interface works behind a reverse proxy and in a Home Assistant sidebar panel (TASK-1378)

The pages built every address on the root of the host, so behind a proxy that publishes the gateway on a path (Home Assistant ingress, or `https://example.lan/otgw/`) every request went to the proxy and the header showed `[version]` with "Error: HTTP 404". All addresses are now relative to the page, in the main page, FSexplorer and the update page. The live OpenTherm log cannot pass a proxy (it uses port 81); the page now says so, and commands keep working.

- FSexplorer upload and delete now answer with a complete `303 See Other`. Before, nginx and HA ingress answered 502.
- The PIC flash page keeps polling until the flash has run and then shows the result, instead of staying on "started" when it has no live log connection (behind a proxy, and in Safari).
- Home Assistant shows a **Visit** link on the gateway's device page (`configuration_url`, set to the gateway's IP).

Checked on a bench gateway with Home Assistant 2026.10.4 and the hass_ingress panel over HTTP and HTTPS, and with nginx on a sub-path. Setup guide: [docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md](docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md).

## Bug fixes

### A silent WiFi link no longer keeps the gateway off the network for an hour (TASK-1248, TASK-1246)

On GH #682 a gateway stayed associated, with an IP address, while no traffic passed, twice for about 72 minutes. The WiFi supervisor only acts when the link drops. The gateway now pings its router three times a minute. With no answer for 8 minutes in a row it forces a WiFi reconnect, and if traffic does not return within another 8 minutes it restarts, with `WiFi: connected but no traffic` in `reboot_log.txt`.

Guards against false alarms: it only arms once the router has answered a ping; it does nothing while MQTT is connected, a REST client is talking to the gateway, or a tool on port 25238 is sending; it waits for a PIC firmware update to finish; and after 3 such restarts in a row it only reconnects, at most once every 15 minutes, until the link has stayed up for an hour. Tested on a bench gateway whose router kept it associated but dropped its traffic: 1.7.7 stayed off for the whole 25-minute test, this version reconnected after about 8 minutes and restarted 8 minutes later.

In the field on GH #682 the check armed and never had to act: the losses there healed by themselves within about 2 minutes, as it is designed to tolerate.

### MQTT comes back after a failed broker name lookup (TASK-1250)

With the broker set by name (the default is `homeassistant.local`), one failed lookup, for example while the router was still starting, kept MQTT down until the next WiFi reconnect or restart. Now a failed lookup keeps the last good address for that name, and the name is looked up again after every 42-second reconnect wait. A renamed broker never falls back to the previous broker's address; when the new name cannot be looked up, the session to the old broker is closed. Brokers set by IP address were not affected.

### The WiFi radio no longer dozes (TASK-1261)

The ESP8266 was left in its default modem-sleep. Routers that check every few seconds whether a station is active (Broadcom-based routers such as ASUS use 5 seconds) disconnected it for inactivity. Modem-sleep is now off after every connect. Telnet key `r` shows the sleep mode (0 = off). theaprilhare ran beta.4 for 25 hours on an ASUS RT-AX86U and confirmed sleep mode 0. Reported with router logs on GH #681.

### The router's address entry for the gateway no longer expires (TASK-1290)

On GH #681 an OPNsense router lost its ARP entry for the gateway every 20 minutes, asked again, got the answer late, and traffic stalled for 15 to 90 s each time. The gateway now sends a gratuitous ARP every 60 s while it has an address, as Tasmota does with SetOption41. Confirmed in the field by theaprilhare: 24 hours with the static ARP entry removed, 86,706 of 87,000 pings answered, longest gap 3.1 s, no 20-minute stalls.

### An access point that was away no longer costs minutes after it returns (ADR-099, TASK-1262)

After 10 failed reconnects the gateway restarts. The boot after that tried the saved network for 2 minutes and then opened the WiFi setup portal for 4 minutes, during which it could not rejoin even with the access point back. That boot now skips the portal, keeps retrying the saved network every 30 seconds, and finishes starting up without the network; telnet and mDNS come up when WiFi returns. The portal still opens after a power cut, on a first install and with the triple reset. Bench test with the access point blocked for 14 minutes: the old build came back 194 s after the block ended, this build after 7 s. Reported as 12 and 24 minute outages on GH #681.

### The gateway joins WiFi under its own name, and no longer sometimes stays off for 6 minutes (TASK-1301, TASK-1262)

The ESP8266 starts connecting to the saved network by itself at power-up, and the firmware started a second connect on top of that. Usually the router then registered the gateway as `ESP-XXXXXX`, and about one start in twenty the chip got stuck connecting. The automatic connect at power-up is now switched off (written to flash once), and the firmware connects once, with its hostname set. Bench test with hardware resets: the old build leased `ESP-22B8E1` on 28 of 34 starts and got stuck on 3 of about 47; this build leased `OTGW` on 60 of 60 and never got stuck.

### More fixes

- **A filesystem update over WiFi no longer crashes when other devices send mDNS queries.** A failed allocation in the mDNS responder crashed the gateway (`Exception (2)`) and could cut off the update, leaving settings at defaults. Checked by sending large mDNS queries during an update: old build crashed 2 of 2, this build completed 3 of 3. (TASK-1285)
- **The PIC reset cause no longer reaches Home Assistant as an unreadable byte.** A `0xFF` answer is now published as the text `0xFF`. Confirmed by mrfox7688 on GH #688: the "Can't decode payload" warning is gone. (TASK-1430)
- **The uptime counts from the moment the gateway started**, not from the end of start-up (about 7 s late on a normal start, minutes when waiting for WiFi). (TASK-1289)
- **`reboot_log.txt` is readable again**: every line ends in CRLF, older logs are repaired on first boot, and entries written before NTP say `time not set` instead of `2106-02-07`. (TASK-1253)
- **The RF sensor battery-code entity in Home Assistant receives its value.** Its discovery state topic was cut off at 47 characters. The entity keeps its unique id. (TASK-1209)
- **The GPIO output trigger bit in the settings list offers 0 to 15**, matching what the gateway stores.
- **The capture scripts keep capturing when the gateway drops off the network**, run without a broker, and switch the debug toggles they enabled back off. (TASK-1249)

## Behaviour changes

None of these needs action, but they are visible:

- **`TP=` commands are no longer replaced in the command queue.** Each `TP=` waits its turn; other commands still replace a waiting one with the same code. (TASK-1431)
- **The WiFi radio stays on**, which uses somewhat more power than modem-sleep. (TASK-1261)
- **The ESP8266 automatic WiFi connect at power-up is switched off in flash.** The firmware connects itself. (TASK-1301)
- **New MQTT topics** `otgw-firmware/command_dropped` and `otgw-firmware/command_retry_exceeded`, two new Home Assistant entities, and new `wifi_*` and `cmd_dropped` keys in `/api/v2/device/info`. Existing topics and keys are unchanged.
- **Home Assistant discovery** carries `configuration_url`. The rest of the payload is unchanged (265 configs compared before and after).

## Documentation

- The MQTT guide now says that a command sent while the gateway reconnects is lost, and why the clean session stays. (TASK-1212)
- The 1.x guides match the code again: FLASH_GUIDE (EN/NL), triple reset, MQTT, the Dallas labels API, PIC firmware, BUILD, DOMOTICZ and the upgrade guides.

## Breaking changes

No breaking changes versus v1.7.7. No MQTT topic renames, no REST API removals, no settings-format changes, and no migration on upgrade.

## Upgrade

Flash **both** firmware and filesystem. Settings are preserved.

## Thank you

Special shoutout to **mrfox7688**, who ran the capture script around the clock on a NAS for weeks, posted day-by-day analyses of every beta on GH #682 (channel changes, moving the gateway away from the flue, ping statistics from the NAS), asked for the WiFi counters and the outage record, and confirmed the reset-cause fix on GH #688.

theaprilhare brought router logs from an ASUS RT-AX86U that pointed at modem-sleep and ran beta.4 for 25 hours, then found that the OPNsense ARP expiry caused the 20-minute stalls and confirmed the gratuitous ARP fix with a 24-hour ping test. DenSinH reported the connection issues on GH #681 and kept reporting as settings changed. sjorsjuhmaniac asked on Tweakers for dropped commands to be visible in Home Assistant.
