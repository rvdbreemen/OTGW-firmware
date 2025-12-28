# OTGW-firmware (ESP8266) for NodoShop OpenTherm Gateway

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

This repository contains the **ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW)**. It runs on the ESP8266 “devkit” that is part of the NodoShop OTGW and turns the gateway into a standalone network device with:
- a Web UI
- MQTT (including Home Assistant MQTT Auto Discovery)
- a REST API
- a TCP serial socket compatible with OTmonitor

The primary goal is **reliable Home Assistant integration** via MQTT and auto discovery, while keeping the OTGW behaviour compatible with existing OpenTherm tooling.

## History and scope

The OpenTherm Gateway itself (hardware + PIC firmware + OTmonitor tooling) originates from **Schelte Bron’s OTGW project**. This firmware builds on that ecosystem by running on the ESP8266 inside the **NodoShop OTGW** to expose OTGW data and controls over the network.

This project is therefore **primarily designed for the NodoShop OTGW hardware with an ESP8266** (NodeMCU / Wemos D1 mini depending on hardware revision). If you have a different OTGW build, it may work, but NodoShop OTGW compatibility is the main target.

## Hardware support

Starting with hardware version 2.3, the included ESP8266 devkit changed from NodeMCU to a Wemos D1 mini. Both are supported by this firmware.

| NodoShop OTGW version | ESP8266 devkit |
|---|---|
| 1.x–2.0 | NodeMCU ESP8266 devkit |
| 2.3–2.x | Wemos D1 mini ESP8266 devkit |

## Documentation and links

- Wiki / documentation (recommended starting point): https://github.com/rvdbreemen/OTGW-firmware/wiki
- NodoShop OTGW product page: https://www.nodo-shop.nl/nl/opentherm-gateway/211-opentherm-gateway.html
- Original OTGW project site (Schelte Bron): http://otgw.tclcode.com/
- OTGW PIC firmware downloads: http://otgw.tclcode.com/download.html
- GitHub releases (prebuilt firmware binaries): https://github.com/rvdbreemen/OTGW-firmware/releases

## Quick start (high level)

The exact steps and screenshots live in the wiki, but the general flow is:

1. Flash the latest firmware release to your ESP8266 (and flash the matching LittleFS image when required by the release).
2. Connect the OTGW to your network and open the Web UI via `http://<device-ip>/`.
3. Configure MQTT (broker, credentials, topic prefix) and enable Home Assistant MQTT Auto Discovery.
4. Add the MQTT integration in Home Assistant; entities should appear automatically.

## Security note

The Web UI and APIs are designed for use on a trusted local network. Do not expose the device directly to the internet; use a VPN or a properly secured reverse proxy if remote access is needed.

## Community and support

- Discord: https://discord.gg/zjW3ju7vGQ
- Issues / bug reports: https://github.com/rvdbreemen/OTGW-firmware/issues

## What you can do with this firmware

### Web UI
- Configure the gateway via HTTP (default port 80).
- Manage settings stored on LittleFS.
- Perform PIC firmware maintenance (see warning below).

### MQTT (including Home Assistant Auto Discovery)
- Publishes parsed OpenTherm values to MQTT using a configurable topic prefix.
- Supports Home Assistant MQTT Auto Discovery (Home Assistant Core v2021.2.0+).
- Accepts OTGW commands via MQTT (topic structure depends on your configured prefix; see the wiki for exact topics and examples).

### REST API
- Read OpenTherm values via `/api/v0/` and `/api/v1/` endpoints.
- Send OTGW commands via `/api/v1/otgw/command/...` (POST/PUT).

### TCP serial socket (OTmonitor compatible)
- Exposes a TCP socket on port `25238` for OTmonitor and other tools that speak the OTGW serial protocol.

### Extra sensors
- Dallas temperature sensors (e.g. DS18B20) with Home Assistant discovery support.
- S0 pulse counter for kWh meters on a configurable GPIO.

## Important warnings / breaking changes

- **Do not flash OTGW PIC firmware over Wi-Fi using OTmonitor.** You can brick the PIC. Use the built-in PIC firmware upgrade feature instead (based on code by Schelte Bron).
- **Breaking change (v0.8.0):** MQTT topic naming conventions changed; MQTT-based integrations may need updates.
- **Breaking change (v0.7.2+):** LittleFS is used; switching required a full reflash via USB and settings are not preserved.

## Roadmap ideas

- InfluxDB client for direct logging
- Live Web UI updates via websockets
- Streaming OT message log via websockets

## Release notes

For release artifacts, see https://github.com/rvdbreemen/OTGW-firmware/releases. A running summary is kept below.

<details>
<summary>Release notes table</summary>

| Version | Release notes |
|-|-|
| 0.10.4 | Feature: Optional NTP time sync to OTGW (new `NTPsendtime` setting)<br>Feature: PIC firmware update checks are no longer automatic; only run when manually triggered by the user<br>Feature: MQTT password is masked in the Web UI<br>Feature: Add basic CSRF protection for sensitive REST API endpoints (Origin/Referer validation)<br>Update: Bundled PIC16F1847 gateway firmware updated<br>Docs: Add OpenTherm Protocol Specification v4.2<br>Other: Many memory/robustness fixes (reduce heap fragmentation by replacing `String`/unsafe formatting with bounded buffers, fix serial buffer overflow causing data loss, fix multiple buffer overruns/bounds checks, harden REST API parsing/method validation and command queue handling, and improve URL parsing/escaping and redirect sanitization). |
| 0.10.3 | Web UI: Mask MQTT password field and support running behind a reverse proxy (auto-detect http/https)<br>Home Assistant: Improve discovery templates (remove empty unit_of_measurement and add additional sensors/boundary values)<br>Fix: Status functions and REST API status reporting<br>CI: Improved GitHub Actions build/release workflow and release artifacts. |
| 0.10.2 | Bugfix: issue #213 which caused 0 bytes after update of PIC firwmare (dropped to Adruino core 2.7.4)<br>Update to filesystem to include latest PIC firmware (6.5 and 5.8, released 12 march 2023)<br>Fix: Back to correct hostname to wifi (credits to @hvxl)<br>Fix: Adding a little memory for use with larger settings.|
| 0.10.1 | Beter build processes to generate consistant quality using aruidno-cli and github actions (Thx to @hvxl and @DaveDavenport)<br>Maintaince to sourcetree, removed cruft, time.h library, submodules<br>Fix: parsing VH Status Master correctly<br>Enhancement: Stopping send time commands on detections of PS=1 mode<br>Fix: Mistake in MQTT configuration of auto discovery template for OEM fault code<br>Added wifi quality indication (so you can understand better)<br>Remove: Boardtype, as it was static in compiletime building|
| 0.10.0 | Updated: Added support fox 6.x firmware (pic16f1847) (Thanks to @hvxl / Schelte Bron)<br>Added reporting of "firmware type"<br>Improved: DHCP can override NTP settings now<br>Improved: Sending SC command on the minute (00 second), after reset ESP all commands (SR 21, SR 22) are resend<br>Bugfix: bitwise not bytewise AND operation for ASF flags OEM codes<br>Readout S0 output from configurable GPIO, interupt rtn added for this, enhanced Dallas-type sensor logic (autoconfigure, code cleanup) (Thanks to @RobR) <br>Web UI improvements by @rlagerwij and @Nicole|
| 0.9.5 | Improved: WebUI improved by community<br>Bugfix: Device Online status indicator for Home Assistant<br>Improved: Update of 5.x series (pic16f88) firmwares, preparing for 6.x (pic16f1847) updates.<br>Bugfix: Prevent spamming OTGW firmware website in case of rebootloop<br>Added: Unique useragent|
| 0.9.4 | Update: New firmware included gateway version 5.3 for PIC P16F88.<br>Update: Preventing >5.x PIC firmwares to be detected, incompatible (for now)| 
| 0.9.3 | Bugfix: Small buffer of serial input, broke the PS=1 command, causing integrations of Domoticz and HA to break<br>Added: Setting for HA reboot detections, this enables a user to change the behaviour of HA reboot detection<br>Bugfix: PIC version detection fixed<br>Improving: Top topics parsing broke with 0.9.2, now you can once more use "/Myhome/OTGW/" as your toptopics |
| 0.9.2 | New feature: Just In Time Home Assistant Auto Discovery topics. Now only sensors that actually have msgids from OpenTherm are send to Home Assistant. (thanks to @rlagerweij)<br>Improvement: Climate Entity (Home Assistant) got improved to detect Thermostat availablity (by @sergantd)<br>Bugfix: Alternating values on status bits (thanks @binsentsu)<br>Bugfix: Blue blinking leds of nodemcu should be off using WebUI (thanks @fsfikke)<br>New feature: Reset wifi button in webUI (thanks @DaveDavenport)<br>Improved: More UI improvements (thanks @rlagerweij)<br>Improved: Serial handling improvements<br>Fixed: Codecleanup (removal of errorprone string functions), removal of potential bufferoverflow, removed all warnings in code compile (thanks @DaveDavenport)<br>Improved: Reboot logging, now includes external watchdog reason.|
| 0.9.1 | New feature: Added new set commands topics for most OTGW features, read more on the wiki<br>New feature: Reset bootlog to filesystem, for debug purposes<br>Improved: Stability, due to removal of ESP based auto-wifi-reconnect<br>Improved: the OT decoding algoritm, so values on MQTT, REST and WebUI now should be more reliable<br>Added: Override decoding of B and T when followed by A and R of the same MsgID, because this means OTGW overrides messages<br>Improved: No messages on versions when not connected to internet<br>Added: Proper msgid 100: remote override room setpoint flags decoding<br>Added: Missing some msgids to OT decoding|
| 0.9.0 | New: Adding time setup commands for Thermostat<br>Fixed: Improved OT status (incl. VH and Solar) message decoding<br>Fixed: Statusbit decoding in webUI<br>Improved: Better wifi auto-reconnect (ESP based)<BR>Improved: Wifi reconnection logic, reboot if 15 min not connected<br>New: NTP hostname setting in webUI<br>Changed: removed ezTime NTP library, moved to ConfigTime NTP and AceTime|
| 0.8.6 | Improving wifi reconnect (without reboot)<br>Fix: Double definition to a HA sensor<br>Adding: OEMDiagnosticCode topic to HA Discovery<br>Bugfix: UI now labels OEM DiagnosticCode correctly, and added the real OEM Fault code|
| 0.8.5 | Bugfix: Queue bug never sending the command (reporter: @jvinckers)<br>Small improvement to status parsing, only resturned status from slave gets parsed now.|
| 0.8.4 | Adding MsgID for Solar Storage<br>Verbose Status parsing for Ventlation / Heatrecovery<br>Adding msgid 113/114 unsuccessful burnerstart / flame too low<br>Added smartpower configruation detection<br>Added 2.3 spec status bits for (summer/winter time, dhw blocking, service indicator, electric production)<br>Adding PS=1 detection (WebUI notification)<br>Fix: restore settings issue|
| 0.8.3 | New feature: Unique ID is configurable (thanks to @RobR)<br>New feature: GPIO pins follow status bits (master/slave) (thanks to @sjorsjuhmaniac)<br>Improved: Detecting online status of thermostat and boiler<br>Improved: MQTT Debug error logging<br>Fixed bug: reconnect MQTT timer and changed wait for reconnect to 42 seconds<br>Added: Rest API command now uses queues for sending commands<br>Fixed bug: msgid 32/33 type switch around<br>Changed: Solar Storage and Collector now proper names (breaking change)|  
| 0.8.2 | Added: Command Queue to MQTT command topic<br>Bugfix: Values not updating in WebUI fixed<br>Added: verbose debug modes<br> Added check for littlefs githash<br>Added: Interval setting for sensor readout<br>Adding: Send OTGW commands on boot<br>Bugfix: Hostname now actually changes if needed.|  
| 0.8.1 | Improved ot msg processing<br>MQTT: added `otgw-firmware/version`, `otgw-firmware/reboot_count`, `otgw-firmware/version` and `otgw-firmware/uptime` (seconds)<br>Bugfix: typoo in topic name `master_low_off_pomp_control_function` -> `master_low_off_pump_control_function`<br>Bugfix: Home Assistant thermostat operation mode (flame icon) template<br>Feature: Add support for Dallas temperature sensors, defaults GPIO10, pushes data to `otgw-firmware/sensors/<Dallas-sensor-ID>` |
| 0.8.0 | **Breaking Change: MQTT topic naming convention has changed from `<mqtt top prefix>/<sensor>` to `<mqtt top prefix>/value/<node id>/<sensor>` for data published and `<mqtt top prefix>/set/<node id>/<command>` for subscriptions** <br> Update Home Assistant Discovery: add OTGW as a device and group all exposed entities as children <br> Update Home Assistant Discovery: add climate (thermostat) entity, uses temporary temperature override (OTGW `TT` command) (Home Assistant Core v2021.2.0+)<br> Bugfix #14: reduce MQTT connect timeout < the watchdog timeout to prevent reboot on a timeout<br> Adding LLMNR responder (http://otgw/ will work now too)<br>New REST API: Telegraf endpoint (/api/v1/otgw/telegraf)<br> Fixing bugs in core OTGW message processor for ASF flags|
| 0.7.8 | Update Home Assistant Discovery <br> Flexible Home Assistant prefix <br> Bugfix: Removed hardcoded OTGW topic <br> Bugfix: NTP timezone discovery removed |
| 0.7.7 | UI improved: Only show updates values in web UI <br> Bugifx: Serial not found error when sending commands thru MQTT fixed |
| 0.7.6 | PIC firmware integration done. <br> New setting: NTP configurable <br> New setting: heartbeat led on/off <br> Update to REST API to include epoch of last update to message|
| 0.7.5 | Complete set of status bits in UI and Central Heating 2 information |
| 0.7.4 | Integration of the otgw-pic firmware upgrade code - upgrade to pic firmware version 5.0 (by Schelte Bron) |
| 0.7.3 | Adding MQTT disable/enable option<br>Adding MQTT long password (max. 100 chars)<br>Adding executeCommand API (verify and return response for commands)<br>Added uptime and otgw fwversion in devinfo UI |
| 0.7.2 | **Breaking change: Moving over to LittleFS. This means you need to reflash your device using a USB cable.** |
| 0.7.1 | Adding reset gateway to enter self-programming mode more reliable. <br> Changed to port 25238 for serial TCP connections (default of OTmonitor application by Schelte Bron)<br>Bugfix: Settings UI works even with "browserplugins". Thanks @STemplar   |
| 0.7.0 | Added all Ventilation/Heat Recovery msgids (2.3b OT spec). Plus Remeha msgids. Thanks @STemplar <br>Added OTGW pic reset on bootup.<br> Translate dutch to english. <br>Bugfix: Serial flushing & writebuffer checking to prevent overflow during flashing.  |
| 0.6.1 | Bugfix: setting page did not always work correctly, now it does. |
| 0.6.0 | Standalone UI for simple OT monitor purposes and deviceinformation, moved index.html to SPIFF <br>OTA is possible after flashing 0.6.0 (Hardware watchdog is fed, during flash uploads now) |
| 0.5.1 | REST APIs, v1, for OTmonitor values, GetByLabel, GetByID, POST `otgw/command/{command}` |
| 0.5.0 | Implemented the UI for settings (restapi, read/write file in json) |
| 0.4.2 | Bi-directional serial communication on port 25238 (aka ser2net) for use with OTmonitor application|   
| 0.4.0 | RestAPI implemented - as simple as `<ip>/api/v0/otgw/{id}` to get the latest values |   
| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi <br> No more default Debug to Serial, only to port 23 telnet |   
| 0.3.0 | Read only Serial stream implementend on port 25238 (debug port remains on port 23 - telnet) |   
| 0.2.0 | Auto-discovery through MQTT implemented for integration with Home Assistant |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)<br>Watchdog feeding implemented |

</details>

## Credits
Shoutout to early adopters helping me out testing and discussing the firmware in development. For pushing features, testing and living on the edge. 

So shoutout to the following people for the collaboration on development: 
* @hvxl           for all his work on the OTGW hardware, PIC firmware and ESP coding. 
* @sjorsjuhmaniac for improving the MQTT naming convention and HA integration, adding climate entity and otgw device 
* @vampywiz17     early adopter and tester 
* @Stemplar       reporting issues realy on
* @proditaki      for creating Domiticz plugin for OTGW-firmware
* @tjfsteele      for endless hours of testing
* @DaveDavenport  for fixing all known and unknown issues with the codebase, it's stable with you
* @DutchessNicole for fixing the Web UI over time
* @RobR           for his work in the s0 counter implementation

And for all those people that keep reporting issue, pushing for more and helping other in the community all the time.

A big thank should goto **Schelte Bron** @hvxl for amazing work on the OpenTherm Gateway project and for providing access to the upgrade routines of the PIC. Enabling this custom firmware a reliable way to upgrade you PIC firmware. If you want to thank Schelte Bron for his work on the OpenTherm Gateway project, just head over to his homepage and donate to him: https://otgw.tclcode.com/

## Buy me a coffee
In case you want to buy me a coffee, head over here:

<a href="https://www.buymeacoffee.com/rvdbreemen"><img src="https://img.buymeacoffee.com/button-api/?text=Buy me a coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00"></a>

## License

MIT. See `LICENSE`.
