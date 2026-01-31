Om de firmware te gebruiken met de originele OTmonitor-software (link: https://otgw.tclcode.com/otmonitor.html)

Om de OTmonitor-software te gebruiken met deze firmware:
1. Download en installeer de OTmonitor-app 
1. Start de applicatie en druk op F5 (menu: Options-> Settings)
1. Klik op de [Connection] in de linker navigatiebalk (-> Connection)
![configuration otmonitor](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/2021-01-12-OTmonitor_configuration.png)
4. Selecteer de TCP-verbinding, voer het **IP-nummer** of **otgw.local** in. De **otgw.local** kan problemen geven op Windows, dit in verband met matige ondersteuning van Bonjour. In dat geval achterhaal het IP adres via de micro-usb middels een seriële verbinding met je ESP8266.
5. Voer de externe poort **25238** in (dit was poort 1023 op versies voor 0.7.1)
6. En klik op de knop [CONNECT]. 

En als alles goed werkt, zie je nu de OTmonitor-software aan het werk.


**Waarschuwing: UPGRADE NOOIT DE PIC FIRMWARE VIA DE OTMONITOR SOFTWARE.**
Hoewel het wel mogelijk is om via de OTmonitor de PIC firmware van de OTGW te upgraden, wordt dit ten zeerste afgeraden. De firmware heeft sinds versie 0.7.5 ingebouwde PIC firmware upgrade dankzij Schelte Bron.
