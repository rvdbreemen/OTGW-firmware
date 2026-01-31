# Integratie met de OTGT-firmware 
De OTGW-firmware je op meerdere manieren aansturen:
* Seriele verbinding over het netwerk
* MQTT Integratie
* [[Rest API integratie|NL-Hoe de REST API te gebruiken]]

# Serie 2 netwerk
Dit is de traditionele integratie die u zou moeten kunnen gebruiken met elke Home Automation die een seriële integratie voor OTGW heeft. Bij het instellen van de OTGW in de documentatie moet u **poort 25238** invoeren (vóór 0.7.1 was het poort 1023) om de verbinding tot stand te brengen. Ja, dit is hetzelfde als bij _de OTmonitor-applicatie_


# MQTT-integratie
Ik bouw echter een MQTT-interface om OTGW te integreren met je Home Assistant of een andere domotica-oplossing.


## MQTT Discovery - voor Home Assistant
Ga gewoon naar de instellingenpagina en stel uw MQTT-broker in (kan onderdeel zijn van Home Assistant). Zodra de MQTT-broker is ingesteld wordt er verbinding gemaakt. De OTGW-firmware stuurt de [Home Assistant MQTT Discovery messages](https://www.home-assistant.io/docs/mqtt/discovery/). Op die manier kan de firmware Home Assistant automatisch configureren. De informatie worden als losse sensoren in uw apparaatlijst getoond.

Het MQTT Discovery-onderwerp is (instelling in de plug-in): **homeassistant**


## Integratie met Homey 
Er is voor Homey een Homey App die middels MQTT de integratie van de OTGW doet:
https://homey.app/en-us/app/com.gruijter.otgw/OpenTherm-Gateway-MQTT/

Deze app is gemaakt door @gruijter. Lees deze blogpost op het Homey Forum: https://community.homey.app/t/77190

MQTT - voor Domoticz
Met dank aan @prodiaki hebben we nu een werkende Domoticz integratie. De ontwikkeling is recent gestart, dus wie weet hoe dit verder gaat: https://github.com/proditaki/OTGW-Domoticz-MQTT-Client

Installatie Python plugin voor OTGW via MQTT in Domoticz
Communicatie tussen OTGW en Domoticz via MQTT client kan ook middels een Python plugin. Installatie kan middels de volgende stappen:

* Download vanaf https://github.com/proditaki/OTGW-Domoticz-MQTT-Client op tabblad Code via de knop Code de Zip-file en pak deze uit.
* Creëer in de directorystructuur van Domoticz in de map domoticz/plugins een nieuwe map met de naam OTGW
* Plaats de uitgepakte bestanden (mqtt.py en plugin.py) in de map OTGW
* Herstart Domoticz
* Open Setup, Hardware en vraag via pulldownmenu een lijst van beschikbare hardwaretypes op. Selecteer OTGW MqttClient
* Vul in de benodige velden passende gegevens: Name, MQTT Server address, port (1883), OTGW Server address, Discovery topic  homeassistant) en OTGW topic (OTGW) en enable de hardware. Let op dat topic-namen hoofdlettergevoelig zijn. Sluit af met Add
( Kies Setup, Settings, System en klik bij Hardware/Devices op de knop Allow for 5 minutes.
* Controleer via Setup, Devices of daar nieuwe apparaten verschijnen. Druk zo nodig bij filterinstellingen voor Not Used en geef desgewenst een Refresh.
* Voeg alle gewenste nieuwe apparaten toe en geef ze een passende naam. 

### Hints bij het oplossen van eventuele problemen met Domoticz en MQTT
Probeer dan eens het volgende:
* Verwijder ook de config topic onder home assistant (via bv. MQTT Explorer)
* Mocht er nog geen nieuwe hardware verschijnen dan kan een reboot van de OTGW ook helpen.
* Kijk eens in de logfile van Domoticz…

### Probelemen met klassieke OTGW Domoticz integratie plugin
Als je de klassieke OTGW Domoticz plugin gebruikt, dan wordt het command "PS=1" (Print State) gebruikt. Dit stopt het doorgeven van RAW OpenTherm messages van de OTGW naar de NodeMCU. Dit heeft dan vervolgens het gevolg dat er geen WebUI updates meer zijn. Vanaf release 0.8.4 wordt dit gedetecteerd en als melding weergeven in het WebUI.

## MQTT Discovery-configuratiebestand
Op het ESP-bestandssysteem kun je dit bestand vinden: **mqttha.cfg**
Het formaat is eenvoudig genoeg, het eerste is het onderwerp van het homeassistant configuratieonderwerp. Het tweede deel is de "sjabloon" voor de thuisassistent die het zal gebruiken om te configureren.

U kunt de onderstaande lijst met onderwerpen gebruiken om uw eigen MQTT-integratie te bouwen op basis van deze onderwerpen. Als je [een MQTT Explorer](http://mqtt-explorer.com/)  met je broker verbindt, kun je ALLE berichten zien, er zijn er meer dan je denkt.


Een lijst van MQTT topics kan je het beste zien met MQTT Explorer (http://mqtt-explorer.com/) op de MQTT broker.