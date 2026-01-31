After first flash, or, when you alter your WiFi network, then you need to configure your wifi settings.

1. Wait for the ESP to go into the AP config mode.
1. Open your WiFi setting (smartphone or computer)
1. Connect to network **OTGW-XX-XX-XX-XX-XX**
1. A configuration screen should pop up. If this does not happen, just go to **192.168.4.1**
1. Now configure your WiFi setup.
1. After the device is connected to WiFi, go to [http://otgw.local](http://otgw.local) or enter the IP address of your ESP

Note: **otgw.local** requires mDNS which is not supported on Android. You can find out the IP address of your gateway by looking at the serial output (9600 baud) while connected to the USB port (it will tell you the IP address on boot) or by checking your router. **Do not connect to the USB port if the ESP is still attached to the OTGW!**

In the Web UI you can go to Settings by clicking: 
![settings icon](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/2021-01-12-settings-icon.png)

[Read more on how to continue to setup the OTGW firmware on the ESP 8266.](https://github.com/rvdbreemen/OTGW-firmware/wiki/How-to-setup-another-OTGW-using-the-WebUI)


