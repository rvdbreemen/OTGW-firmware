**De Nederlandse wiki kan je hier vinden: [[NL-Start]]**

# Welcome to the OTGW-firmware documentation wiki!

Intended for use on a devkit based on ESP8266 such as WeMos D1 Mini (ESP) connected to an OpenTherm Gateway (OTGW).

OTGW hardware:
* [[Versions of OTGW]]

Install the firmware for the first time:
* [[How to install firmware on your ESP]]
* [[Configure WiFi settings using the Web UI]]

Compile the code:
* [[How to compile the OTGW firmware|How-to-compile-the-OTGW-firmware]]

## REST API Documentation

The firmware supports multiple API versions. Start here based on your needs:

**For new projects / modern integration (recommended):**
* [[1-API-v3-Overview|1-API-v3-Overview]] - Latest REST API v3 with HATEOAS
* [[2-Quick-Start-Guide|2-Quick-Start-Guide]] - 30-second setup for API v3
* [[3-Complete-API-Reference|3-Complete-API-Reference]] - Full endpoint documentation
* [[7-HATEOAS-Navigation-Guide|7-HATEOAS-Navigation-Guide]] - Using HATEOAS for discovery

**For upgrading from older versions:**
* [[4-Migration-Guide|4-Migration-Guide]] - Upgrade paths from v0/v1/v2 to v3
* [[5-Changelog|5-Changelog]] - Version history and breaking changes

**General REST API information:**
* [[How to use the REST API]] - Legacy API documentation (v0/v1)
* [[8-Troubleshooting|8-Troubleshooting]] - Common issues and solutions
* [[6-Error-Handling-Guide|6-Error-Handling-Guide]] - Error codes and handling

More information:
* [[Upgrade to new release using Over The Air flashing]]
* [[How to setup another OTGW using the WebUI]]
* [[How to upload files to LittleFS]]
* [[How to debug the OTGW firmware]]
* [[The meaning of the blue LEDs]]
* [[How to use with OTmonitor]]
* [[How to use it with Home Assistant, Homey or Domoticz or others]]
* [[How to send commands to the OTGW with MQTT or REST or Net2Ser]]
* [[Using the MQTT set commands]]
* [[Using DS18B20 sensor with ESP firmware]]