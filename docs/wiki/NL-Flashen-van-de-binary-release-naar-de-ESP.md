Firmware installeren op uw ESP

1. Als uw ESP is aangesloten op de OTGW, koppelt u de gateway los en koppelt u de ESP los van de OTGW.
1. Sluit het ESP aan op een pc met behulp van een micro-USB-kabel. Als er geen stuurprogramma is, probeer dan: [WeMos](https://github.com/wemos/ch340_driver/raw/master/CH341SER_WIN_3.5.ZIP) of [NodeMCU](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
1. Download [.bin bestanden](https://github.com/rvdbreemen/OTGW-firmware/releases/latest) en [esptool](https://github.com/espressif/esptool/releases/download/v4.5.1/esptool-v4.5.1-win64.zip) (u moet het uitpakken).
1. Hernoem de .bin bestanden naar 'ino.bin' en 'fs.bin'. Zorg ervoor dat 'esptool' op je pad ligt.
1. Typ 'esptool write_flash 0x0 ino.bin 0x200000 fs.bin' 
1. Er verschijnen verschillende regels informatie terwijl de ESP wordt geschreven. Het blauwe lampje moet snel en dan langzaam knipperen.
1. Haal de stekker van de ESP uit het stopcontact en installeer deze in uw OTGW (zorg ervoor dat deze op de juiste manier is!), en schakel vervolgens de OTGW in.