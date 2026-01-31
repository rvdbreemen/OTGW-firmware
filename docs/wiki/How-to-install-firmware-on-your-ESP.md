To install firmware on your ESP

Prepare:
1. If your ESP is attached to the OTGW, disconnect the gateway then unplug the ESP from the OTGW.
1. Connect the ESP to a PC or Mac using a Micro USB lead (make sure the cable supports data-transfer, not just power)
1. Download [.bin files](https://github.com/rvdbreemen/OTGW-firmware/releases/latest) and rename the .bin files to `ino.bin` and `fs.bin`

For Windows:
1. If no driver, try: [WeMos](https://github.com/wemos/ch340_driver/raw/master/CH341SER_WIN_3.5.ZIP) or [NodeMCU](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
1. Download [esptool](https://github.com/espressif/esptool/releases/download/v4.5.1/esptool-v4.5.1-win64.zip) (you will need to unzip it).
1. Make sure `esptool` is on your path.
1. Type `esptool write_flash 0x0 ino.bin 0x200000 fs.bin` 

For Mac:
1. Open a terminal and navigate to the folder with the bin files.
1. Run `pip install esptool` , or follow these [instructions](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html).
1. Run: `esptool.py -p /dev/tty.usbserial-120  write_flash 0x0 ino.bin 0x200000 fs.bin` (Replace `/dev/tty.usbserial-120` with the name of your serial port. You should be able to find it by inspecting the output from `ls /dev/tty*`) 

Once the installer is running:
1. Several lines of information will appear as the ESP is written. The blue light should blink fast then slowly.
1. Unplug the ESP and install into your OTGW (be sure it's the right way round!), then power on the OTGW.

Now move on to [configure the wifi](https://github.com/rvdbreemen/OTGW-firmware/wiki/Configure-WiFi-settings-using-the-Web-UI).