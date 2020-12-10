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
- implement interface for original OTmon application 

To do:
- implement a REST API
- InfluxDB client to do direct logging 
- OT command sending interface

Looking for the documentation, go here (work in progress):   https://github.com/rvdbreemen/OTGW-firmware/wiki/Documentation-of-OTGW-firmware
  
| Version | Release notes |
|-|-|

| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi  No more default Debug to Serial, only to port 23 telnet |   
| 0.3.0 | OTmon stream implementend on port 1023 (debug port remains on port 23 - telnet) |   
| 0.2.0 | Auto-discovery throug MQTT implemented for integration with home assistant (and domoticz)     |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)   Watchdog implemented |

