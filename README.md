# OTGW-firmware

[![Join the chat at https://gitter.im/OTGW-firmware/community](https://badges.gitter.im/OTGW-firmware/community.svg)](https://gitter.im/OTGW-firmware/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Opentherm firmware - Nodoshop version

This is a new firmware for the Nodoshop version of the OpenTherm Gateway.  
It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/188-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  

The features of this OTGW firmware are:
- parsing the protocol on the NodeMCU (8266)
- enable telnet listening (interpreted data and debugging)
- send MQTT messages for every change  (parsed OT message)
- integrate with Home Assistant (and Domoticz)
- serial interface on port 1023 for original OTmonitor application (bi-directional)
- a REST API (http://<ip>/api/v0/otgw/{id})
- sending a MQTT command to OTGW (topic: OTGW/command) 
- settings for Hostname and MQTT in the webUI (just compile and edit in webUI)

To do:
- InfluxDB client to do direct logging 
- OTmonitor Web UI (standalone interface)

Looking for the documentation, go here (work in progress):  <br> https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware
  
| Version | Release notes |
|-|-|
| 0.5.0 | Implemented the UI for settings (restapi, read/write file in json) |
| 0.4.2 | Bi-directional OTmon on port 1023 (aka ser2net) |   
| 0.4.1 | MQTT command sending now works, topic: OTGW/command and |   
| 0.4.0 | RestAPI implemented - as simple as <ip>/api/v0/otgw/{id} to get the latest values |   
| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi <br> No more default Debug to Serial, only to port 23 telnet |   
| 0.3.0 | Read only OTmon stream implementend on port 1023 (debug port remains on port 23 - telnet) |   
| 0.2.0 | Auto-discovery throug MQTT implemented for integration with home assistant (and domoticz)     |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)   Watchdog implemented |

