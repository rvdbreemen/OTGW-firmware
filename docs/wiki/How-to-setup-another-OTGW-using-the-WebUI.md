On the setting page seen below, you can setup the device hostname and the MQTT settings to connect to an MQTT broker.

![Hostname of OTGW gateway](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-hostname.png)

If you want another name for your device, change it here. 

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-mqtt.png)

The MQTT settings can be set up here, the following parameters: MTTQ broker address (by name or IP), the MQTT port can be changed (default: 1883), username/password, and finally the top topic for this firmware. _Important again if you implement multiple OTGW devices in one setup._

You can change the home assistant prefix. You **only** do that if you change your home assistant setup, normally this default is good. 

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-ntp.png)

Also you can modify your timezone here, and even disable the NTP function completely when you have firewalled your device to the internet.
You can setup your own NTP server (e.g. local router or an alternative NTP server of your choice)

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-blueled.png)

You can disable the blue blinking status led here. 

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-gpiosensor.png)

This enables you to setup Dallas-type temperture sensors DS18B20 on your ESP8266 pins that are free. This requires some hacking of your pins, like bending them. 

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-gpiooutput.png)
 
This enables you to control free pins on your ESP devkit that are unused, based on the status bit of message 0 of the OpenTherm protocol.

![MQTT settings](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-bootcmd.png)

This enables you to setup boot time commands. They are send after every reboot of your OTGW / ESP 8266. You can send multiple commands using the ";" as a seperator. So sending `GW=1`. Would send the following commands to OTGW: `GW=1`. _(Feature found in 0.9.1+, before that it was limited to 1 command)_


# Saving your configuration changes
To finish setting up the firmware, just hit the [SAVE] button. The settings are stored in the devices flash memory. 
Just to make sure, hit the [RESET] button. 

If you now go [BACK] you will see the OpenTherm WebUI to monitor your boiler and thermostat in real time. Enjoy!



