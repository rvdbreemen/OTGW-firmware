## Hoofdstuk 11: De OpenTherm Gateway OTGW32

---

Firmware 2.0 ondersteunt precies twee hardwareconfiguraties:

1. **De OTGW Classic met een ESP32-S3-module.** De traditionele gateway met de PIC-coprocessor en een LOLIN S3 Mini of S3 Mini Pro in de modulesocket (hoofdstuk 2).
2. **De OpenTherm Gateway OTGW32.** Een print met een ESP32-S3 die zelf OpenTherm spreekt, met eigen aansluitingen naar de ketel en naar de thermostaat. Op deze print zit geen PIC. Dit hoofdstuk beschrijft hem. Hoofdstuk 12 zet de twee naast elkaar.

**Verwacht op de OTGW32 niet:** het bypass-relais (hardwareversie 1.1 heeft er plaats voor; het is bij levering niet gemonteerd en de firmware gebruikt het niet), een kant-en-klaar klemmenblok (het 10-polige blok J4 is in de fabriek niet gemonteerd: u monteert het zelf), PIC-firmware-updates (er is geen PIC) of Bluetooth-kamersensoren (die vragen extra geheugen dat deze print niet heeft).

> **De OTGW32 is nieuw en de firmware ervoor heeft nog niet in een echt huis gedraaid.** De hardwareversies 1.0 en 1.1 lijken sterk op elkaar; versie 1.1 heeft een optioneel bypass-relais dat niet is gemonteerd bij levering en dat de firmware niet gebruikt. Firmware 2.0 ondersteunt hem, maar de firmware heeft tot nu toe alleen op een testapparaat gedraaid, zonder thermostaat en zonder echte ketel op de bus (een gesimuleerde ketel, en masterstand zonder thermostaat). Nog niet in het veld bewezen: hoe het zich gedraagt met verschillende ketels en thermostaten, lange runs, de opties Ethernet en OLED op echte hardware, en OTmonitor (een pc-programma voor de OpenTherm Gateway) via poort 25238. Draait u een OTGW32? Stuur dan bij uw melding een capture mee (zie hoofdstuk 7).

**Welke heeft u?** Na de eerste flash laat het apparaat het zien: de webinterface heeft op een OTGW Classic een tabblad Advanced, PIC firmware en op een OTGW32 niet, en `hardware_type` in de device info (`/api/v2/device/info`) is `otgw-classic` of `otgw32`. Weet u niet zeker welke print u heeft, vraag het dan in het Discord-supportkanaal voordat u flasht: een fabrieksinstallatie wist het apparaat.

### 11.1 Wat het is

- **Processor en geheugen:** een ESP32-S3 met 4 MB flash en zonder PSRAM (extra werkgeheugen; de LOLIN S3 Mini-module heeft het wel). De firmware controleert tijdens het draaien of er PSRAM is.
- **OpenTherm:** de ESP32-S3 stuurt zelf de ketelzijde en de thermostaatzijde aan. De firmware noemt dit OT-Direct (de ESP32 doet het werk dat de PIC op de Classic doet). Er is geen PIC, dus er is ook geen PIC-firmware om bij te werken.
- **Opties in de firmware:** bekabeld Ethernet (een W5500-chip), een klein OLED-schermpje en een ingang voor een Dallas-temperatuursensor worden door de firmware ondersteund. In onze tests zijn ze nog niet op echte OTGW32-hardware gezien.
- **Aansluitblok:** het 10-polige klemmenblok (J4) is in de fabriek niet gemonteerd; u monteert het zelf. Klem 1 is de klem die het dichtst bij de resetknop zit. Klem 1 en 2 zijn de OpenTherm-lijnen van de ketel en klem 3 en 4 zijn de OpenTherm-lijnen van de thermostaat. Klem 5 tot en met 10 voeren voeding, massa, een digitaal-ingangspad met het opschrift DI en de 1-wire-lijn, maar de volgorde kennen we nog niet. Sluit er niets op aan zolang die niet is gedocumenteerd. Het DI-pad is waarschijnlijk de SW-lijn; waar het op is aangesloten en of u het mag gebruiken, is niet bevestigd.
- **Wat het apparaat meldt:** `GET /api/v2/device/info` toont `hardware_type` `otgw32` en de hardwaremodus `OT-Direct`. MQTT publiceert hetzelfde als `otgw-firmware/hardware_type` en `otgw-firmware/hardware_mode`.

### 11.2 De werkstanden

OT-Direct kent deze standen. Kies er een in de webinterface (Settings, OT-Direct mode; de webinterface vraagt om bevestiging), via REST (`POST /api/v2/otdirect/mode?mode=...`) of met het commando `GW=`.

| Stand | Wat hij doet | Commando |
|---|---|---|
| **Gateway** (standaard) | Zit tussen thermostaat en ketel zoals de PIC dat doet: stuurt de berichten van de thermostaat door, beantwoordt er een aantal zelf en past de overrides toe (SAT, `CS=`, `TT=` enzovoort). | `GW=1` |
| **Monitor** | Stuurt elk bericht ongewijzigd door en logt alles. Er wordt niets overschreven. | `GW=0` of `GW=M` |
| **Master** | Het apparaat is de OpenTherm-master en vraagt de ketel zelf uit. Een thermostaat is optioneel: blijft er een aangesloten, dan beantwoordt het apparaat die uit zijn cache en geeft het de instellingen ervan door aan de ketel. | `GW=2` of `GW=S` |
| **Loopback** | Een teststand met gesimuleerde ketelantwoorden. Er is geen hardware voor nodig. | `GW=L` |

Een vijfde stand, **bypass** (de thermostaat via een relais rechtstreeks op de ketel), staat op sommige plaatsen genoemd, maar deze release heeft geen ondersteuning voor dat relais (hardwareversie 1.1 heeft een optioneel relais, niet gemonteerd bij levering, en de firmware gebruikt het niet): kiest u bypass, dan valt het apparaat terug op gateway en antwoordt `GW=P` met `NG`. Reken dus niet op bypass.

De stand staat in de instelling `OTDmode` en blijft na een herstart bewaard. In gatewaystand luistert het apparaat bij het opstarten 5 seconden naar de thermostaat. Hoort het er geen, dan draait het alleen die start in masterstand; bij de volgende start kijkt het opnieuw.

### 11.3 Eerste installatie en bijwerken

- **Eerste installatie via USB:** gebruik `flash_otgw.bat` (Windows) of `./flash_otgw.sh` (Linux en macOS) uit de release-zip. Een fabrieksinstallatie wist het apparaat: het WiFi-wachtwoord en de instellingen zijn weg. De tool vindt de USB-poort zelf. (De oudere flashgids geldt alleen voor de 1.x ESP8266.)
- **Bijwerken via WiFi:** open `http://<device-ip>/update` en geef de release-zip. De instellingen blijven bewaard; bewaar ze toch eerst.

> **Voor u bijwerkt:** gedurende 1 tot 2 minuten is de thermostaat niet met de ketel verbonden en draait de ketel op zijn eigen instellingen. Kies een moment waarop niemand de komende minuten een warm huis nodig heeft.
- **Flashindeling:** safeboot 960 KB, firmware 2240 KB, webbestanden 768 KB. Een apparaat met een oudere indeling heeft de eenmalige USB-migratie nodig (`flash_otgw --migrate --host <ip>`).

### 11.4 Hoe het apparaat merkt dat het een OTGW32 is

De release-image is een en dezelfde voor beide configuraties. Bij het opstarten zoekt de firmware, in de standaardinstelling (Board mode: Auto), maximaal 3 keer naar een PIC. Antwoordt er een PIC, dan is het apparaat een OTGW Classic en wordt dat onthouden. Antwoordt er geen en heeft de print geen PSRAM, dan start OT-Direct. Een module met PSRAM waarop geen PIC antwoordt, geldt niet als OTGW32: die blijft in een verminderde toestand met de PIC-verbinding open.

U kunt Board mode zelf op OT-Direct zetten. Het apparaat zoekt dan eenmalig naar een PIC en start OT-Direct als er geen is; dat scheelt bij elke start tot twee zoekpogingen.

### 11.5 Wat de webinterface toont

De webinterface volgt wat het apparaat meldt (`otcommandinterface`), niet de naam van de print:

- Het tabblad **PIC firmware** (Advanced) is verborgen.
- De verbindingskaart toont **OT-Direct** in plaats van de PIC-verbinding; de commandoprompt toont `OT ›` in plaats van `PIC ›`.
- De panelen **OT-Direct overrides** en **OT-Direct status** en de instelling voor de OT-Direct-stand worden getoond.
- De rest is gelijk: Home, Monitor, SAT, Settings.

### 11.6 Grenzen door 4 MB flash en geen PSRAM

- **Grafiekgeschiedenis** bewaart 1 uur. Apparaten met PSRAM bewaren een langere geschiedenis (tot 7 dagen).
- **Bluetooth (BLE) kan niet worden aangezet** zonder PSRAM: de instelling is zichtbaar maar wordt geweigerd. Een Bluetooth-kamersensor kan dus op een OTGW32 niet voor SAT dienen.
- **REST:** voorlopig worden maximaal 2 verzoeken tegelijk beantwoord. Een derde krijgt HTTP 503 `Retry-After: 1`. De grens is voor deze printklasse niet gemeten.
- **Diagnose-recorder:** een kleine buffer van ongeveer 2 minuten in plaats van de grote.
- **MQTT:** een apparaat zonder extra geheugen verstuurt pieken in stappen; de grenzen zijn daarvoor nog niet gekalibreerd (zie Known issues in de releasenotes).
- **Pinnen:** bekabeld Ethernet (W5500) gebruikt SPI op GPIO 11, 12, 13, 14 en 15, en de 1-Wire-ingang voor Dallas-sensoren zit op GPIO 4 (hardware-informatie van een OTGW32-bezitter, in overeenstemming met de pinnentabel in `docs/hardware/PINOUT.md`). De standaardpinnen van de firmware voor de Dallas-sensor (GPIO 10), de S0-teller (GPIO 12) en de GPIO-uitgang (GPIO 16) zijn pinnen die de print zelf gebruikt. De firmware weigert daarom de Dallas-standaard en start geen sensoren totdat u een pin kiest: zet de Dallas-pin op 4. Voor de S0-teller en de GPIO-uitgang is op deze print geen vrije pin bevestigd: het klemmenblok heeft een digitaal-ingangspad (DI, GPIO 5 in het schema) waarvan het gebruik niet is bevestigd, en de ESP32-S3-module zelf heeft ongebruikte pinnen die niet naar een connector zijn doorgevoerd.

### 11.7 Wat gelijk is aan de OTGW Classic

- **SAT** is beschikbaar. Het is op OT-Direct de enige regelaar die in de firmware is ingebouwd; zonder SAT regelt de thermostaat de ketel zoals gebruikelijk. In gatewaystand werkt het via het regelsetpunt en het CH-enable-bit van het doorgestuurde statusbericht. Een capture met een echte thermostaat waarin de ketel SAT volgt op een OTGW32, ontbreekt nog.
- **MQTT, Home Assistant, de REST-API en de webinterface** werken gelijk. De discovery is just-in-time, dus een OTGW32 krijgt alleen entiteiten voor de OpenTherm-berichten die hij heeft gezien.
- **Poort 25238** (voor OTmonitor en de OpenTherm Gateway-integratie van Home Assistant) wordt op OT-Direct regel voor regel nagebootst en staat standaard uit. Hij is op een OTGW32 niet met OTmonitor getest. Een PIC-firmware-upgrade via OTmonitor bestaat op deze print niet.
- **Retourtemperatuur:** in gatewaystand vraagt het apparaat die aan de ketel in de ruimte van een ondersteund thermostaatverzoek en antwoordt de thermostaat uit zijn cache.

### 11.8 Wat getest is, en wat niet

Getest op een OTGW32-testapparaat: detectie bij het opstarten (OT-Direct, geen PIC), loopback en masterstand, SAT met de ingebouwde simulatie, MQTT en de webinterface, bijwerken via WiFi met safeboot, en geheugen- en MQTT-metingen zonder PSRAM.

Niet getest op een OTGW32: een echte thermostaat op de bus, een echte ketel in gateway- of monitorstand, Ethernet, het OLED-scherm, OTmonitor via poort 25238, de eerste installatie via USB, en lange runs. Dat laten de meldingen uit het veld zien.
