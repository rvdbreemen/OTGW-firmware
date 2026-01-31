Na de eerste keer flashen, of, als iets aan je WIFI netwerk verandert, dan kan je de wifi als volgt configureren.

1. De ESP8266 zal na een paar tellen in de AP config mode gaan (blauw led blijft branden).
1. Open je WiFi instellingen (laptop of computer)
1. En verbind met het netwerk: **OTGW-XX-XX-XX-XX-XX**
1. Een configuratie scherm zou moeten verschijnen (captive portal). Als dat niet gebeurt binnen 5 seconden, ga dan met een browser naar het volgende IP adres **192.168.4.1**
1. Nu kan se WiFi setup eenvoudig gedaan worden. Kies je netwerk uit en stel het wachtwoord in en sla de instellingen op.
1. Nadat de ESP8266 verbonden is met de WIFI (blauwe leds moeten uitgaan met knipperen, daarna zal er 1 led om de seconde knipperen). Om met het Web UI te verbindingen gan naar [http://otgw.local](http://otgw.local)

Notitie: Indien dat niet werkt via **http://otgw.local**, dan komt dat doordat Multicast DNS niet voor iedereen werkt (known issue). Het IP adres kan dan gevonden worden via de USB kabel aansluiting. Kijk met de Arduino IDE op de seriële poort (9600 baud). Reset de ESP, en kijk in de output, na het verbinden met de WIFI zal het IP adres getoond worden in de seriële output. U kunt ook inloggen op uw router en het IP-adres van aangesloten apparaten bekijken.

In het web UI kan je naar de instellingen gaan om de overige instellingen te doen voor de firmware:
![settings icon](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/2021-01-12-settings-icon.png)

[Lees meer over hoe verder te gaan met het configureren van de OTGW firmware van de ESP8266.](https://github.com/rvdbreemen/OTGW-firmware/wiki/NL-Configureren-van-de-OTGW-firmware-instellingen)