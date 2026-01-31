The ESP firmware directly supports one or more DS18B20 sensors you can read out using the firmware. And you can control some of the pins based on the status bits of example CH on/off or flame on/off to control a relay for example. _This does require knowledge of soldering, and the sensors reading are only sent to the MQTT hub for now._

# Temperature sensor and Relay control
The firmware can use the free GPIO pins on the ESP 8266 to read a DS18B20 or control a relay for switching purposes. For this you have to use a free pin on the ESP, since not all pins are used you may use any free pins, configurable in the Webui settings page. 

# Available pins of ESP
OTGW was not designed with this in mind, BUT you can do it anyway. You should not use any of the "in use pins".  OTGW version 2.3 has additional holes next to all pins, so you can easily solder your wires. 

On the OTGW PCB the following pins are free for use.
| Pin  | GPIO  | OTGW v2.0 (NodeMCU) | OTGW v2.3 (Wemos D1 mini) | Remark |
|---|:-:|:-:|:-:|---|
| D3 | 0  | X | X| |
| D6 | 12  |X | X| |
| D7 | 13  |X | X| |
| SD3| 10 | X | | Not available on Wemos D1 Mini|

Any of these pins can be used to connect your sensor. All the other pins are in use, and should not be used to connect anything. See the pinout of the different ESP devkits below. Red circles mark the free pins on each model that can be used with the OTGW.

# Wiring the DS18B20 sensor
To measure temperature you can wire up one or more DS18B20 temperature sensors, most likely you want to use the more industrial version with a "metal" temperature sensor that you can strap to your heating pipes. You can wire up multiple sensors on one pin, so you can easily measure different temperatures using this solution.

Make sure to use a pull-up resistor of 4k7 Ohm between Vcc (3.3V) and your ESP pin. If you wish to wire multiple sensors then you may just want to use a prototype breadboard for this. 

![wiring picture for the ds18b20 sensors](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/wiring-of-the-ds18b20.png)

# Configuration the sensors
The GPIO pin needs to be set up in the webui, make sure to set up the correct GPIO, based on the table and pinout on this page. Then after saving, give a reset to your OTGW. This way during the boot of the OTGW the sensor(s) will be detected, and from then on the sensors readings will be sent to MQTT. 

## Sensors to MQTT 
The sensor values are read out based on the interval given, make sure you do not try to read out with a very high frequency. No less then 5 second intervals, but normally an interval of 30 or 60 seconds is just fine. The sensor data is streamed to MQTT, there automatically sensor values will appear in the MQTT hub.

## Automation using MQTT
The reasoning behind the current setup is that the temperature sensor(s) will be used to measure temperatures. When you use multiple sensors its not clear what each value is, or means to the firmware. The design choice here is to stream it to MQTT so you are flexible to use it, for example. You can measure Touside, Treturn, Tboilerflow. You could just use the sensors do tune and graph your values, or, you could even send the outside temperature back to the OTGW [[Using the MQTT set commands]].

There is no limit to your imagination, so use your favorite home automation solution to do this.

# Controlling a relay using a GPIO pin 
The firmware can control a GPIO pin to control a relay. This could be used to have a ON/OFF relay based on a bit from the status of your boiler. You can control a relay to switch a valve or pump (or both) based on the Central Heating bit. That way every time your boiler is turned on for the production of Central Heating water, then you can turn on your pump accordingly. 

Use one of the free pins from the table to do this. Go to the WebUI to configure the bits that control the GPIO. 

# Pinout of NodeMCU 
Red circles mark the free pins on NodeMCU. The design of v2.0 (and before) had no use for the free pins, so the pins are not connected to anything on the board itself. You could solder wires to the top of NodeMCU or to the bottom of the OTGW board as you wish.

![pinout picture of nodemcu](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/nodemcu-pinout-ds18b20.png)


# Pinout of Wemos D1 Mini
Red circles mark the free pins on Wemos D1 Mini. OTGW version 2.3 has additional holes on the PCB you can use to solder your wires.

![pinout picture of wemos d1 mini](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/wemosd1mini-pinout-ds18b20.png)


# Basics with the DS18B20 sensors
In case you have issues getting it going, first validate your setup using your ESP and the sensor only. Follow this link and follow the instructions and test your sensors.

Link to external site: https://www.best-microcontroller-projects.com/ds18b20.html




