# OTGW-firmware for NodeMCU - ESP 8266 

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

OpenTherm Nodoshop firmware

This is a custom firmware for the Nodoshop NodeMCU/ESP8266 version of the OpenTherm Gateway. 

It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/188-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  (also location of the OTGW PIC firmware) 

The goal of this project is to become a fully functioning ESP8266 firmware that operates the OTGW as a standalone application. Providing:
- a WebUI
- bidirection MQTT support
- a REST API
- automatic integration with Home Assistant (Home Assistant Core v2021.2.0+)
- and a TCP socket for serial connection

**Breaking change: With version 0.8.0: MQTT Discovery topic naming convention has changed significantly, this will break MQTT based applications.**
**Breaking change: With version 0.7.2 (and up) the LitteFS filesystem is used. This means you need to reflash your device using a usb cable, all settings are lost in the process.**


The features of this Nodosop OpenTherm NodeMCU firmware are:
- configuration via the webUI on port 80: flash your nodeMCU and edit settings via the webUI
- userfriendly file handling using LittleFS (breaking change v0.7.2+)
- parsing the OT protocol on the NodeMCU (8266)
- parsing all known OT protocol message ID's (OpenTherm v2.2+2.3b), including Heating/Ventilation and Remeha specific msgid's
- wide range of connection and data sharing options:
  - telnet (interpreted data and debugging)
  - MQTT (publishing every parsed OT message, subscribing for commands on OTGW/set/<node id>/command)
  - simple REST API (http://<ip>/api/v0/otgw/{id})
  - simple REST API (http://<ip>/api/v1/otgw/id/{id} or http://<ip>/api/v1/otgw/label/{textlabel eg. Tr or Toutside}, commands on /api/v1/otgw/command/{any command})
  - serial interface on port 25238 for original OTmonitor application (bi-directional)
  - OTmonitor Web UI (standalone interface)
- automatic integration with Home Assistant using _Home Assistant Discovery_ (Home Assistant Core v2021.2.0+)
- integration with any MQTT based Home Automation solution, like Domoticz (plugin available) & OpenHAB
- reliable OTA upgrades of `OTGW-firmare` (the nodeMCU) itself (v0.6.0+)
- reliable OTGW PIC upgrades, to the latest firmware available at http://otgw.tclcode.com/download.html
- cleaner RestAPI's for Telegraf OTmonitor integration
- readout Dallas-type temperture sensors (eg. DS18B20) connected to GPIO
 
**Warning: Never flash your OTGW PIC firmware through wifi using OTmonitor application, you can brick your OTGW PIC. Instead use the buildin PIC firmware upgrade feature (based on code by Bron Schelte)**

To do:
- InfluxDB client to do direct logging 
- Instant update of webUI using websockets
- Showing log of OT messages using websockets

Looking for the documentation, go here (work in progress):  <br> https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware

| Version | Release notes |
|-|-|
| 0.8.2 | Added: Command Queue to MQTT command topic<br>Bugfix: Values not updating in WebUI fixed<br>Added: verbose debug modes<br>
Added check for littlefs githash<br>Added: Interval setting for sensor readout<br>Adding: Send OTGW commands on boot<br>Bugfix: Hostname now actually changes if needed.|  
| 0.8.1 | Improved ot msg processing<br>MQTT: added `otgw-firmware/version`, `otgw-firmware/reboot_count`, `otgw-firmware/version` and `otgw-firmware/uptime` (seconds)<br>Bugfix: typoo in topic name `master_low_off_pomp_control_function` -> `master_low_off_pump_control_function`<br>Bugfix: Home Assistant thermostat operation mode (flame icon) template<br>Feature: Add support for Dallas temperature sensors, defaults GPIO10, pushes data to `otgw-firmware/sensors/<Dallas-sensor-ID>` |
| 0.8.0 | **Breaking Change: MQTT topic naming convention has changed from `<mqqt top prefix>/<sensor>` to `<mqtt top prefix>/value/<node id>/<sensor>` for data publshed and `<mqtt top prefix>/set/<node id>/<command>` for subscriptions** <br> Update Homeasssistant Discovery: add OTGW as a device and group all exposed entities as childs <br> Update Homeasssistant Discovery: add climate (thermostat) enity, uses temporary temperature override (OTGW `TT` command) (Home Assistant Core v2021.2.0+)<br> Bugfix #14: reduce MQTT connect timeout < the watchdog timeout to prevent reboot on a timout<br> Adding LLMNR responder (http://otgw/ will work now too)<br>New restapi: Telegraf endpoint (/api/v1/otgw/telegraf)<br> Fixing bugs in core OTGW msg processor for ASF flas|
| 0.7.8 | Update Home Assistant Discovery <br> Flexible Home Assistant prefix <br> Bugfix: Removed hardcoded OTGW topic <br> Bugfix: NTP timezone discovery removed |
| 0.7.7 | UI improved: Only show updates values in web UI <br> Bugifx: Serial not found error when sending commands thru MQTT fixed |
| 0.7.6 | PIC firmware integration done. <br> New setting: NTP configurable <br> New setting: heartbeat led on/off <br> Update to REST API to include epoch of last update to message|
| 0.7.5 | Complete set of status bits in UI and Central Heating 2 information |
| 0.7.4 | Integration of the otgw-pic firmware upgrade code - upgrade to pic firmware version 5.0 (by Schelte Bron) |
| 0.7.3 | Adding MQTT disable/enable option<br>Adding MQTT long password (max. 100 chars)<br>Adding executeCommand API (verify and return response for commands)<br>Adding RESTAPI /api/v1/otgw/cmdrsp/{command} that returns {response from command}<br>Added uptime and otgw fwversion in devinfo UI |
| 0.7.2 | **Breaking change: Moving over to LittleFS. This means you need to reflash your device using a USB cable.** |
| 0.7.1 | Adding reset gateway to enter self-programming mode more reliable. <br> Changed to port 25238 for serial TCP connections (default of OTmonitor application by Schelte Bron)<br>Bugfix: Settings UI works even with "browserplugins". Thanks @STemplar   |
| 0.7.0 | Added all Ventilation/Heat Recovery msgids (2.3b OT spec). Plus Remeha msgids. Thanks @STemplar <br>Added OTGW pic reset on bootup.<br> Translate dutch to english. <br>Bugfix: Serial flushing & writebuffer checking to prevent overflow during flashing.  |
| 0.6.1 | Bugfix: setting page did not always work correctly, now it does. |
| 0.6.0 | Standalone UI for simple OT monitor purposes and deviceinformation, moved index.html to SPIFF <br>OTA is possible after flashing 0.6.0 (Hardware watchdog is fed, during flash uploads now) |
| 0.5.1 | REST APIs, v1, for OTmonitor values, GetByLabel, GetByID, POST otgw/command/{command} |
| 0.5.0 | Implemented the UI for settings (restapi, read/write file in json) |
| 0.4.2 | Bi-directional serial communication on port 25238 (aka ser2net) for use with OTmonitor application|   
| 0.4.1 | MQTT command sending now works, topic: OTGW/command and |   
| 0.4.0 | RestAPI implemented - as simple as <ip>/api/v0/otgw/{id} to get the latest values |   
| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi <br> No more default Debug to Serial, only to port 23 telnet |   
| 0.3.0 | Read only Serial stream implementend on port 25238 (debug port remains on port 23 - telnet) |   
| 0.2.0 | Auto-discovery through MQTT implemented for integration with Home Assistant (and Domoticz)     |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)   Watchdog implemented |


## Credits
Shoutout to early adopters helping me out testing and discussing the firmware in development. For pushing features, testing and living on the edge. 

So shoutout to the following people for the collaboration on development: 
* @sjorsjuhmaniac for improving the MQTT naming convention and HA integration, adding climate entity and otgw device 
* @vampywiz17     early adopter and tester 
* @Stemplar       reporting issues realy on
* @proditaki      for creating Domiticz plugin for OTGW-firmware
* @tjfsteele      for endless hours of testing

A big thank should goto **Schelte Bron** @hvlx for amazing work on the OpenTherm Gateway and for providing access to the upgrade routines of the PIC. Enabling this custom firmware a reliable way to upgrade you PIC firmware.

If you want to thank Schelte Bron for his work on the OpenTherm Gateway project, just head over to his homepage and donate to him: https://otgw.tclcode.com/

## Buy me a coffee
In case you want to buy me a coffee, head over here:

<a href="https://www.buymeacoffee.com/rvdbreemen"><img src="https://img.buymeacoffee.com/button-api/?text=Buy me a coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00"></a>
