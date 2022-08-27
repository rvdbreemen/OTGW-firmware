# ESP8266 based OTGW-firmware for Nodoshop hardware

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

OpenTherm Nodoshop OTGW hardware - an ESP8266 firmware

This project is an firmware for the Nodoshop OTGW hardware, based on ESP8266 devkits.

Starting with version 2.3 of the Nodoshop hardware the devkit has changed from NodeMCU to a Wemos D1mini. This is fully supported by the hardware and this firmware.

Supporting hardware version are:
| Version | Hardware supported |
|-|-|
|1.x-2.0|NodoMCU ESP8266 devkit|
|2.3-2.x|Wemos D1mini ESP8266 devkit|

It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/211-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  (also location of the OTGW PIC firmware) 

The goal of this project is to become a fully functioning ESP8266 based OTGW-firmware that operates the OTGW as a standalone application. Providing:
- a WebUI
- bidirection MQTT support
- a REST API
- automatic integration with Home Assistant (Home Assistant Core v2021.2.0+)
- and a TCP socket for serial connection

**Breaking change: With version 0.8.0 MQTT Discovery topic naming convention has changed significantly, this will break MQTT based applications.**
**Breaking change: With version 0.7.2 (and up) the LitteFS filesystem is used. This means you need to reflash your device using a usb cable, all settings are lost in the process.**


The features of this Nodosop OpenTherm Gateware ESP8266 based firmware are:
- configuration via the webUI on port 80: flash your ESP8266 and edit settings via the webUI
- userfriendly file handling using LittleFS (breaking change v0.7.2+)
- parsing the OT protocol on the ESP8266
- parsing all known OT protocol message ID's (OpenTherm v2.2+2.3b), including Heating/Ventilation and Remeha specific msgid's
- wide range of connection and data sharing options:
  - telnet (interpreted data and debugging)
  - MQTT (publishing every parsed OT message, publish commands to this topic `OTGW/set/<node id>/command`)
  - simple REST GET API (`http://<ip>/api/v0/otgw/{id}`)
  - simple REST GET API (`http://<ip>/api/v1/otgw/id/{id}` or `http://<ip>/api/v1/otgw/label/{textlabel eg. Tr or Toutside}`
  - simple REST PUT or POST commands on `/api/v1/otgw/command/{any command})`
  - serial interface on port 25238 for original OTmonitor application (bi-directional)
  - OTmonitor Web UI (standalone interface)
- automatic integration with Home Assistant using _Home Assistant Discovery_ (Home Assistant Core v2021.2.0+)
- integration with any MQTT based Home Automation solution, like Domoticz (plugin available) & OpenHAB
- reliable OTGW PIC upgrades (v0.6.0+), to the latest firmware available at http://otgw.tclcode.com/download.html
- cleaner RestAPI's for Telegraf OTmonitor integration
- readout Dallas-type temperture sensors (eg. DS18B20) connected to GPIO
- readout S0 output counter and timing from kWh meter connected to configurable GPIO
 
**Warning: Never flash your OTGW PIC firmware through wifi using OTmonitor application, you can brick your OTGW PIC. Instead use the buildin PIC firmware upgrade feature (based on code by Schelte Bron)**

To do:
- InfluxDB client to do direct logging 
- Instant update of webUI using websockets
- Showing log of OT messages using websockets847

Looking for the documentation, go here (work in progress):  <br> https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware

| Version | Release notes |
|-|-|
| 0.10.1 | Bugfix, the S0 interrupt where not initialised and checked and send to MQ. Stupid rebase/cleanup error|
| 0.10.0 | Readout S0 output from configurable GPIO, interupt rtn added for this, enhanced Dallas-type sensor logic (autoconfigure, code cleanup)|
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
| 0.8.0 | **Breaking Change: MQTT topic naming convention has changed from `<mqqt top prefix>/<sensor>` to `<mqtt top prefix>/value/<node id>/<sensor>` for data publshed and `<mqtt top prefix>/set/<node id>/<command>` for subscriptions** <br> Update Homeasssistant Discovery: add OTGW as a device and group all exposed entities as childs <br> Update Homeasssistant Discovery: add climate (thermostat) enity, uses temporary temperature override (OTGW `TT` command) (Home Assistant Core v2021.2.0+)<br> Bugfix #14: reduce MQTT connect timeout < the watchdog timeout to prevent reboot on a timout<br> Adding LLMNR responder (http://otgw/ will work now too)<br>New restapi: Telegraf endpoint (/api/v1/otgw/telegraf)<br> Fixing bugs in core OTGW msg processor for ASF flas|
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


## Credits
Shoutout to early adopters helping me out testing and discussing the firmware in development. For pushing features, testing and living on the edge. 

So shoutout to the following people for the collaboration on development: 
* @sjorsjuhmaniac for improving the MQTT naming convention and HA integration, adding climate entity and otgw device 
* @vampywiz17     early adopter and tester 
* @Stemplar       reporting issues realy on
* @proditaki      for creating Domiticz plugin for OTGW-firmware
* @tjfsteele      for endless hours of testing

A big thank should goto **Schelte Bron** @hvxl for amazing work on the OpenTherm Gateway project and for providing access to the upgrade routines of the PIC. Enabling this custom firmware a reliable way to upgrade you PIC firmware.

If you want to thank Schelte Bron for his work on the OpenTherm Gateway project, just head over to his homepage and donate to him: https://otgw.tclcode.com/

## Buy me a coffee
In case you want to buy me a coffee, head over here:

<a href="https://www.buymeacoffee.com/rvdbreemen"><img src="https://img.buymeacoffee.com/button-api/?text=Buy me a coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00"></a>
