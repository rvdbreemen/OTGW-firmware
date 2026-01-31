# Integration with the OTGW-firmware 
The OTGW-firmware can be integrated using the following interfaces:
* Serial over the network connection
* MQTT Integration
* [[Rest API Integration|How to use the REST API]]

# Serial 2 network 
This is the traditional integration you should able to use with any Home Automation that has a serial-based integration for OTGW. In the setup of the OTGW in the documentation, you should enter **port 25238** (before 0.7.1 it was port 1023) to make the connection. Yes, this is the same as with _the OTmonitor application_

Warning: Do not use the serial over network option with the native HA integration at the same time as the MQTT integration. The behavior will most likely become unpredictable.


# MQTT integration
However, I build an MQTT interface for the purpose of integrating OTGW with your Home Assistant or any other home automation solution for that matter.

## MQTT Discovery - for Home Assistant
Just go to the settings page, and setup your MQTT broker (can be part of Home Assistant). Once you have setup the MQTT broker, and it connects. The OTGW-firmware will send the [Home Assistant MQTT Discovery messages](https://www.home-assistant.io/docs/mqtt/discovery/). That way Home Assistant can automagically configure the topics, you will the information as sensors in your device list.

The MQTT Discovery topic is (setting in the plugin): **homeassistant**

## Integration with Homey 
For Homey there is a Homey App for integration using the MQTT interface of OTGW of this firmware available:
https://homey.app/en-us/app/com.gruijter.otgw/OpenTherm-Gateway-MQTT/

This app was created by the user @gruijter. Read his post on the Homey Forum on his motivation: https://community.homey.app/t/77190

## MQTT - for Domoticz
Thanks to the user @prodiaki we now have a working Domoticz integration going, it's a work in progress, but it works:
https://github.com/proditaki/OTGW-Domoticz-MQTT-Client

Install Python plugin for OTGW via MQTT in Domoticz
Communication between OTGW and Domoticz via MQTT client is also possible through a Python plugin. Installation can be done through the following steps:

* Download the Zip file from https://github.com/proditaki/OTGW-Domoticz-MQTT-Client on the Code tab via the Code button and extract it.
* In the directory structure of Domoticz, in the folder domoticz/plugins, create a new folder called OTGW
* Place the extracted files (mqtt.py and plugin.py) in the OTGW folder
* Restart Domoticz
* Open Setup, Hardware and request a list of available hardware types via the pull-down menu. Select OTGW MqttClient
* Enter appropriate data in the required fields: Name, MQTT Server address, port (1883), OTGW Server address, Discovery topic (homeassistant) and OTGW topic (OTGW) and enable the hardware. Note that topic names are case sensitive. Finish with Add
* Choose Setup, Settings, System and click the Allow for 5 minutes button under Hardware/Devices.
* Check via Setup, Devices whether new devices appear there. If necessary, press the filter settings for Not Used and, if desired, give a Refresh.
* Add any new devices you want and give them an appropriate name.

### Hints on solving any problems with MQTT & Domoticz integration

* If no new hardware appears yet, a reboot of the OTGW can also help.
* Have a look at the Domoticz log file…


### Classic Domoticz integration (using port 25238)
When used with the classic OTGW Domoticz plugin the command "PS=1" (Print State) is used. This causes the RAW openTherm messages to stop flowing from OTGW, so parsing on the NodeMCU stops too. This causes the WebUI to stop updating. With release 0.8.4, this is detected and noted in the WebUI.


## MQTT Discovery configuration file
On the ESP filesystem, you can find this file: **mqttha.cfg**
The format is simple enough, the first is the topic of the home assistant configuration topic. The second part is the "template" for home assistant that it will use to configure.

You can use the list of topics below to build your own MQTT integration based on these topics. If you connect [a MQTT Explorer](http://mqtt-explorer.com/) to your broker, you can see ALL messages, there are more than you think.

List of configured MQTT topics can best be seen in MQTT Explorer (http://mqtt-explorer.com/) on the MQTT Hub.