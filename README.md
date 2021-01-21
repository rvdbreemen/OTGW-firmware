# OTGW-firmware for NodeMCU - ESP 8266 

[![Join the chat at https://gitter.im/OTGW-firmware/community](https://badges.gitter.im/OTGW-firmware/community.svg)](https://gitter.im/OTGW-firmware/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Opentherm firmware - Nodoshop version

This is a custom firmware for the Nodoshop NodeMCU/ESP8266 version of the OpenTherm Gateway. 

It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/188-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  (also location of the OTGW PIC firmware) 



The features of this Custom OTGW NodeMCU (ESP8266) firmware are:
- parsing the protocol on the NodeMCU (8266)
- enable telnet listening (interpreted data and debugging)
- send MQTT messages for every change  (parsed OT message)
- integrate with Home Assistant (and Domoticz)
- serial interface on port 1023 for original OTmonitor application (bi-directional)
- simple REST API (http://<ip>/api/v0/otgw/{id})
- simple REST API (http://<ip>/api/v1/otgw/id/{id} or http://<ip>/api/v1/otgw/label/{textlabel eg. Tr or Toutside} 
- sending commands thru MQTT (topic: OTGW/command) 
- sending commands thru REST API (/api/v1/otgw/command/{any command})
- settings for Hostname and MQTT in the webUI (just compile and edit in webUI)
- OTmonitor Web UI (standalone interface)
- reliable OTA upgrades (v0.6.0+)

**WARNING: Do not upgrade your PIC thru port 1023! Connect Your OTGW to your serialport instead for upgrade.**

To do:
- InfluxDB client to do direct logging 

Looking for the documentation, go here (work in progress):  <br> https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware
  
| Version | Release notes |
|-|-|
| 0.7.0 | Added all 2.3b msgids Ventilation/Heat Recovery. And Remeha msgids. Thanks @STemplar <br>Added OTGW pic reset on bootup.<br> Translate dutch to english.  |
| 0.6.1 | Bugfix: setting page did not always work correctly, now it does. |
| 0.6.0 | Standalone UI for simple OT monitor purposes and deviceinformation, moved index.html to SPIFF <br>OTA is possible after flashing 0.6.0 (Hardware watchdog is fed, during flash uploads now) |
| 0.5.1 | REST APIs, v1, for OTmonitor values, GetByLabel, GetByID, POST otgw/command/{command} |
| 0.5.0 | Implemented the UI for settings (restapi, read/write file in json) |
| 0.4.2 | Bi-directional serial communication on port 1023 (aka ser2net) for use with OTmonitor application|   
| 0.4.1 | MQTT command sending now works, topic: OTGW/command and |   
| 0.4.0 | RestAPI implemented - as simple as <ip>/api/v0/otgw/{id} to get the latest values |   
| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi <br> No more default Debug to Serial, only to port 23 telnet |   
| 0.3.0 | Read only Serial stream implementend on port 1023 (debug port remains on port 23 - telnet) |   
| 0.2.0 | Auto-discovery throug MQTT implemented for integration with home assistant (and domoticz)     |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)   Watchdog implemented |
