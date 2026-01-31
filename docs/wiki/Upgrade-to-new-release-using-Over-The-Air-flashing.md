To upgrade the firmware once you are set up, you can perform an Over-The-Air (OTA) upgrade via the web interface.

1. Download the latest release from the Github release page: https://github.com/rvdbreemen/OTGW-firmware/releases
2. Now go to your OTGW WebUI, using: http://otgw.local or just goto the IP of your OTGW directly, for example: http://192.168.0.10
3. In the webUI click on the file-explorer icon in the navigation bar.

![File Explorer Button](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/otgw-fsexplorer-button.png)

4. To save your personal settings, download _settings.ini_ so you can upload it after you upgrade. In case you modified other files, like the _mqttha.cfg_, then download those too.

5. In the file explorer you need to click on the [Update Firmware] button.

![Update Firmware button](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/otgw-fsexplorer-update-firmware.png)

6. In the flash firmware utility UI you need to upload both binary files for each release. 

![Firmware flash utility webpage](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/otwg-firmware-flashing-utility.png)

7. Choose your firmware, make sure to select the correct _ino.bin_, then click the flash firmware button. 
8. Be patient while flashing, this does take a little while. 
9. Now repeat step 7 with _mklittlefs.bin_ and click the flash littlefs button.
10. If you saved the settings, upload _settings.ini_ using the file explorer, or use the settings page and set up your OTGW again.
11. Always reboot after uploading the _settings.ini_ file to make sure the firmware reads the settings back to memory.

The wifi settings should not be overwritten by OTA flashing, if you need that, then it's best to flash your device as an initial setup and erase your ESP8266 before you flash the first time.


