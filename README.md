# OTGW-firmware
Opentherm firmware - Nodoshop version

This is a new firmware for the Nodoshop version of the OpenTherm Gateway.  
It can be found here: https://www.nodo-shop.nl/nl/opentherm-gateway/188-opentherm-gateway.html  
More information on this gateway can be read here: http://otgw.tclcode.com/  

The goal of this OTGW firmware is:
- parsing the protocol on the NodeMCU (8266)
- send MQTT messages for every change
- send to InfluxDB directly 
- poll a simple REST API
- enable telnet listening (interpreted data and debugging)

version 0.0.1 - parsing of OT protocol (to telnet)
version 0.1.0 - MQTT sending added
Version 0.2.0 - auto-discovery for home assistant integration (and Domiticz)
