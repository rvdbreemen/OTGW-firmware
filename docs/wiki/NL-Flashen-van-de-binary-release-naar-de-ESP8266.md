Om firmware op je ESP te installeren

1. Als je ESP is aangesloten op de OTGW, verbreek dan de verbinding met de gateway en trek de ESP uit de OTGW.
2. Sluit de ESP aan op een pc met behulp van een Micro USB-kabel. Als er geen driver is, probeer: [WeMos](https://github.com/wemos/ch340_driver/raw/master/CH341SER_WIN_3.5.ZIP) of [NodeMCU](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
3. Download [.bin-bestanden](https://github.com/rvdbreemen/OTGW-firmware/releases/latest) en [esptool](https://github.com/espressif/esptool/releases/download/v4.5.1/esptool-v4.5.1-win64.zip) (je moet dit uitpakken).
4. Hernoem de .bin-bestanden naar `ino.bin` en `fs.bin`. Zorg dat `esptool` op je pad staat.
5. Typ `esptool write_flash 0x0 ino.bin 0x200000 fs.bin`
6. Er zullen verschillende regels informatie verschijnen terwijl de ESP wordt geschreven. Het blauwe licht moet snel knipperen en dan langzaam.
7. Ontkoppel de ESP en installeer deze in je OTGW (zorg dat deze correct is geplaatst!), en zet vervolgens de OTGW aan.

Ga nu verder met [het configureren van de wifi](https://github.com/rvdbreemen/OTGW-firmware/wiki/NL-Configueer-de-WIFI-instellingen-met-de-Web-UI).