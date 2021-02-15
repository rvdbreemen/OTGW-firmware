# OTGW-firmware for NodeMCU - ESP 8266 

[![Join the chat at https://gitter.im/OTGW-firmware/community](https://badges.gitter.im/OTGW-firmware/community.svg)](https://gitter.im/OTGW-firmware/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Opentherm Nodoshop firmware

This is a custom firmware for the Nodoshop NodeMCU/ESP8266 version of the OpenTherm Gateway. 

It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/188-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  (also location of the OTGW PIC firmware) 

The goal of this project is to become a fully functioning ESP8266 firmware that operates the OTGW as a standalone application. With a WebUI, MQTT and REST API, integration with Home Assistant (using MQTT discovery) and a TCP connection for serial connection (on port 25238).


**Breaking change: With version 0.7.2 (and up) the LitteFS filesystem is used. This means you do need to reflash your filesystem, settings are lost in the process.**


The features of this Nodosop OpenTherm NodeMCU firmware are:

- breaking change: implemented LittleFS -  (v0.7.2+)
- reliable OTGW PIC firmware upgrade (to latest version)
- parsing the OT protocol on the NodeMCU (8266)
- parsing all known OT protocol message ID's (2.2+2.3b), including Heating/Ventilation and Remeha specific msgid's 
- enable telnet listening (interpreted data and debugging)
- send MQTT messages for every change  (parsed OT message)
- integrate with Home Assistant (and Domoticz)
- serial interface on port 25238 for original OTmonitor application (bi-directional)
- simple REST API (http://<ip>/api/v0/otgw/{id})
- simple REST API (http://<ip>/api/v1/otgw/id/{id} or http://<ip>/api/v1/otgw/label/{textlabel eg. Tr or Toutside} 
- sending commands thru MQTT (topic: OTGW/command) 
- sending commands thru REST API (/api/v1/otgw/command/{any command})
- settings for Hostname and MQTT in the webUI (just compile and edit in webUI)
- OTmonitor Web UI (standalone interface)
- reliable OTA upgrades ofr NodeMCU (v0.6.0+)

**Warning: Do not flash your OTGW PIC firmware through wifi. Instead use the _NEW_ reliable PIC firmware upgrade, just goto the File Explorer tab and click the PIC upgrade button**



To do:
- InfluxDB client to do direct logging 

Looking for the documentation, go here (work in progress):  <br> https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware
  
| Version | Release notes |
|-|-|
| 0.7.5 | Complete set of status bits in UI and Central Heating 2 information |
| 0.7.4 | Integration of the otgw-pic firmware upgrade code - upgrade to pic firmware version 5.0 (by Schelte Bron) |
| 0.7.3 | Adding MQTT disable/enable option<br>Adding MQTT long password (max. 100 chars)<br>Adding executeCommand API (verify and return response for commands)<br>Adding RESTAPI /api/v1/otgw/cmdrsp/{command} that returns {response from command}<br>Added uptime and otgw fwversion in devinfo UI |
| 0.7.2 | Breaking change: Moving over to LittleFS. This means you need to reflash your device using a USB cable.   |
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
| 0.2.0 | Auto-discovery throug MQTT implemented for integration with home assistant (and domoticz)     |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)   Watchdog implemented |

Shoutout to early adopters helping me out testing and discussing the firmware in development. For pushing features, testing and living on the edge. 

So shoutout to the following people, for testing, discussing, and feedback on development: 
* @vampywiz17 
* @Stemplar 
* @tjfsteele 
* @proditaki

A big thank should goto **Schelte Bron** @hvlx for amazing work on the OpenTherm Gateway and for providing access to the upgrade routines of the PIC. Enabling this custom firmware a reliable way to upgrade you PIC firmware.

If you want to thank Schelte Bron for his work on the OpenTherm Gateway project, just head over to his homepage and donate to him: https://otgw.tclcode.com/

In case you want to buy me a coffee, head over here:

<a href="https://www.buymeacoffee.com/rvdbreemen"><img src="https://img.buymeacoffee.com/button-api/?text=Buy me a coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00"></a>
