Op de instellingenpagina hieronder kun je de hostnaam van het apparaat en de MQTT-instellingen instellen om verbinding te maken met een MQTT-broker.

![Hostnaam van OTGW-gateway](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-hostname.png)

Als je een andere naam voor je apparaat wilt, verander deze dan hier.

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-mqtt.png)

Hier kun je de MQTT-instellingen configureren, met de volgende parameters: adres van de MQTT-broker (op naam of IP), de MQTT-poort kan worden gewijzigd (standaard: 1883), gebruikersnaam/wachtwoord, en ten slotte het hoofdonderwerp voor deze firmware. _Belangrijk opnieuw als je meerdere OTGW-apparaten in één opstelling implementeert._

Je kunt het voorvoegsel van Home Assistant wijzigen. Dit doe je **alleen** als je je Home Assistant-opstelling wijzigt, normaal gesproken is deze standaardinstelling goed.

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-ntp.png)

Ook kun je hier je tijdzone aanpassen, en zelfs de NTP-functie volledig uitschakelen wanneer je apparaat met een firewall van het internet is afgeschermd.
Je kunt je eigen NTP-server instellen (bijv. lokale router of een alternatieve NTP-server naar keuze)

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-blueled.png)

Hier kun je de blauwe knipperende statusled uitschakelen.

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-gpiosensor.png)

Dit stelt je in staat om Dallas-type temperatuursensoren DS18B20 op je vrije ESP8266-pinnen in te stellen. Dit vereist wat knutselwerk aan je pinnen, zoals het buigen ervan.

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-gpiooutput.png)

Hiermee kun je vrije pinnen op je ESP-devkit die niet gebruikt worden bedienen, gebaseerd op het statusbit van bericht 0 van het OpenTherm-protocol.

![MQTT-instellingen](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/OTGWsettings-bootcmd.png)

Dit stelt je in staat om opstart commando's in te stellen. Ze worden verzonden na elke herstart van je OTGW / ESP 8266. Je kunt meerdere commando's verzenden met ";" als scheidingsteken. Dus het verzenden van `GW=1` zou de volgende commando's naar OTGW sturen:  `GW=1`. _(Functie gevonden in 0.9.1+, daarvoor was het beperkt tot 1 commando)_

# Je configuratiewijzigingen opslaan
Om de firmware-installatie af te ronden, klik je gewoon op de [OPSLAAN]-knop. De instellingen worden opgeslagen in het flashgeheugen van het apparaat.
Voor de zekerheid, druk je op de [RESET]-knop.

Als je nu teruggaat [TERUG], zie je de OpenTherm WebUI om je ketel en thermostaat in realtime te monitoren. Veel plezier!


