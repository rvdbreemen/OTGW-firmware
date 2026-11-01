## Hoofdstuk 1: Introductie

### Wat is OTGW-firmware?

OTGW-firmware is de open-source firmware voor de NodoShop OpenTherm Gateway (OTGW). De firmware draait op een ESP32-S3 Wi-Fi-microcontroller (een LOLIN S3 Mini of S3 Mini Pro) in de OTGW Classic en verbindt de OpenTherm verwarmingsbus in uw woning met uw thuisnetwerk, een domotica-platform en een webbrowser.

De firmware kent twee actieve lijnen. De 2.0.0-lijn (de `dev`-branch die hier beschreven wordt) is uitsluitend voor de ESP32-S3: hij is gebouwd op een async web stack plus FreeRTOS en draait op de NodoShop OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro in de Wemos D1 mini-voet. De PIC blijft zitten; één firmware-image (`esp32-combo`) herkent hem bij het opstarten. De 1.x maintenance / LTS-lijn is de firmware voor de Wemos D1 mini (ESP8266); wie terug wil naar 1.x, plaatst de Wemos D1 mini terug. Beide lijnen worden gebouwd met PlatformIO.

Nieuw in de 2.0.0-lijn:

- Een herontworpen, mobielvriendelijke v2 Web UI, de enige web-UI op de 2.0.0-lijn (de klassieke UI is verwijderd).
- BLE-sensorondersteuning: Xiaomi MiBeacon (inclusief encrypted v4/v5 via een per-sensor bindkey), ATC/pvvx custom firmware, en BTHome v2.
- Een first-time-setup onboarding wizard die eenmalig draait op een vers apparaat en opnieuw te starten is vanuit Instellingen.
- Een unified Home Assistant heat/cool/off climate entity, aangestuurd door de OpenTherm-statusbits.

Zonder firmware is de OpenTherm Gateway een slimme maar moeilijk toegankelijke hardware. Met OTGW-firmware wordt het een volledig netwerk-connected verwarmingscontroller die:

- Alle 80+ OpenTherm-berichttypen realtime decodeert en publiceert
- Via MQTT meer dan 250 Home Assistant-entiteiten automatisch aanmaakt, zonder handmatige YAML
- MQTT discovery payloads opslaat in flash (PROGMEM) en asynchroon publiceert via een bitmap-gestuurd drip-mechanisme, waardoor grote RAM-buffers overbodig worden
- Een browsergebaseerde interface biedt voor monitoring, configuratie en firmware-updates
- Een REST API v2 beschikbaar stelt voor scripts, dashboards en directe integratie
- Een TCP serial bridge aanbiedt op poort 25238, compatibel met OTmonitor
- Optioneel de slimme thermostaat SAT (Smart Autotune Thermostat) uitvoert, volledig op het apparaat zelf
- Weergegevens ophaalt via Open-Meteo voor buitentemperatuur en zonnestand wanneer er geen lokale sensor beschikbaar is
- Een nachtelijke herstart kan inplannen om heap-fragmentatie op te lossen bij langlopende apparaten

De firmware is ontworpen voor een vertrouwd thuisnetwerk. Er is geen clouddienst, geen app-store-account en geen internetverbinding vereist. NTP-tijdsynchronisatie en de optionele weergegevens voor SAT via Open-Meteo zijn de enige online functies.

---

### De OpenTherm Gateway hardware

#### Het OpenTherm-protocol

OpenTherm is een open communicatieprotocol voor de verbinding tussen cv-thermostaten en cv-ketels. Het vervangt de eenvoudige aan/uit-schakelaar van oudere systemen door een bidirectionele digitale verbinding op twee draden, waarmee de thermostaat een gewenste ruimtetemperatuur of aanvoertemperatuur kan opgeven en de ketel zijn toestand, temperaturen, modulatieniveau en storingen kan melden.

Een OpenTherm-bus werkt op 24 V gelijkspanning. De communicatie verloopt via Manchester-codering met 1 bit per milliseconde. De meeste moderne cv-ketels van merken als Intergas, Vaillant, Remeha, Worcester Bosch en Nefit ondersteunen OpenTherm.

#### De PIC co-processor

Het NodoShop OTGW-printje bevat een PIC16F88 of PIC16F1847 microcontroller die de OpenTherm bus elektrisch afhandelt. De PIC zit fysiek in serie op de bus tussen de thermostaat en de ketel. Berichten worden onderschept, doorgegeven en op verzoek aangepast. De PIC communiceert met de ESP via UART op 9600 baud als ASCII-regels.

De firmware kent ook een OT-Direct-modus, alleen voor hardware zonder PIC. Op de OTGW Classic herkent de firmware de PIC bij het opstarten en draait hij in PIC gateway mode.

#### De ESP-module

De ESP32-S3 (LOLIN S3 Mini of S3 Mini Pro) in de Wemos D1 mini-voet voert de eigenlijke firmware uit. Hij verzorgt:

- Wi-Fi
- De webserver, WebSocket-server en REST API
- MQTT-communicatie met PROGMEM-gebaseerde auto-discovery
- SAT-regeling met weercompensatie via Open-Meteo
- Temperatuursensoren (Dallas DS18B20, S0-puls, BLE)
- OTA-updates
- Telnet debug log via SimpleTelnet (multi-client)

#### Platformabstractie (boards.h)

Sinds v2.0.0 bevat de firmware een `boards.h`-header die pin-mappings, feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`) en hardwaremogelijkheden per printvariant definieert. Dit scheidt platformspecifieke configuratie netjes van de applicatielogica.

---

### Nieuwe functies in v2.0.0

v2.0.0 is een grote platformrelease. Het is de ESP32-S3-only async + FreeRTOS-lijn; ESP8266 is verplaatst naar de 1.x maintenance / LTS-lijn. Instellingenbestanden van v1.3.x worden automatisch geladen zonder conversie.

v2.0.0 verkeert op het moment van schrijven nog in alpha. De bestaande functionaliteit is feature-compleet, maar SAT, OTDirect en de nieuwe MQTT-topicnamen (zie hieronder) worden nog verder verfijnd. Volg de GitHub Releases-pagina voor de actuele alpha-tag.

**Breaking change in deze release.** v2.0.0 gebruikt standaard een nieuwe, zelfbeschrijvende set MQTT-topicnamen (ADR-106). Zevenendertig OT-spec-georiënteerde binary_sensor-topics krijgen een duidelijker, Home Assistant-vriendelijk label (bijvoorbeeld: `master_ch_enable` wordt `central_heating_enabled`). Gebruikers met bestaande automations op de oude labels kunnen `settings.mqtt.bUseLegacyOtTopics` op `true` zetten om het v1.x-gedrag te behouden. De twee modi sluiten elkaar uit: of de nieuwe namen, of de oude namen worden gepubliceerd, nooit beide tegelijk. De REST API is ongewijzigd ten opzichte van v1.x.

**ESP32-S3-ondersteuning**
- Draait op de OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro in de Wemos D1 mini-voet. De PIC blijft zitten; één `esp32-combo`-image herkent de PIC bij het opstarten.
- OT-Direct-modus met vijf werkingsmodi (thermostaat, ketel, gateway, monitor en gecombineerd master+slave), alleen op hardware zonder PIC
- Ethernet met runtime-failover van en naar Wi-Fi: alleen met Ethernet-hardware; de OTGW Classic heeft geen Ethernet
- BLE temperatuursensoren: passieve Bluetooth LE-scan van Xiaomi MiBeacon (plaintext en encrypted v4/v5 via een per-sensor bindkey), ATC/pvvx custom firmware, en BTHome v2
- Aangepaste 4 MB-partitietabel: een safeboot-updater, één app-slot van 2240 KB en 768 KB LittleFS

**SAT (Smart Autotune Thermostat)**
- Ingebouwde weergecompenseerde PID-verwarmingsregelaar die volledig op het apparaat draait
- Weercompensatie via Open-Meteo: gratis weer-API voor buitentemperatuur en zonnestand wanneer er geen lokale sensor is
- Summer Simmer Index: onderdrukt verwarming bij hoge buitentemperaturen
- Zonnewarmtecompensatie: detecteert zonnewinst en verlaagt het setpoint om oververhitting te voorkomen
- Multi-zone ondersteuning: tot vier temperatuurbronnen (OT, MQTT, BLE, Dallas)
- Drukbewaking met alarm bij lage druk of snel drukverlies
- OPV-kalibratie: vindt de overpressure valve temperatuur van de ketel
- Zes presets: comfort, eco, afwezig, slaap, activiteit, thuis

**MQTT en Home Assistant**
- 250+ auto-discovery entiteiten: climate entity, SAT-sensoren, BLE-sensoren, drukbewaking, OLED-status
- Unified climate entity met off/heat/cool: modus en actie worden afgeleid uit de OpenTherm-statusbits en op eigen topics gepubliceerd (zie Hoofdstuk 4)
- Streaming MQTT discovery: discovery payloads worden gecompileerd in flash (PROGMEM) en asynchroon gepubliceerd via een bitmap-gestuurd drip-mechanisme dat elke entiteit rechtstreeks naar de broker streamt. Dit vervangt de eerdere LittleFS- en RAM-gebaseerde aanpak en elimineert grote staging-buffers.
- SAT switches en select entities zijn toegevoegd aan de HA discovery via dezelfde streaming-pipeline (TASK-284), samen met runtime-gedetecteerde Dallas-sensoren, de climate- en number-entiteiten, en HA button- en select-entiteiten voor PIC-commando's.
- Just-in-time MQTT discovery (ADR-100): discovery-configs voor OpenTherm MsgIDs worden nu pas gepubliceerd wanneer de ketel of thermostaat die MsgID voor het eerst zendt, in plaats van bij elke boot alle 256 IDs in bulk te publiceren. Dit houdt het broker-register beperkt tot de IDs die uw hardware daadwerkelijk gebruikt en vermindert MQTT-verkeer bij het opstarten.
- Platte MQTT-topics per waarde (ADR-101): elke sensor of regelwaarde wordt als platte scalar op zijn eigen topic gepubliceerd. Geen samengestelde JSON-state-topics. Dit is een bewuste keuze die auto-discovery transparant houdt en een eigen HA-component overbodig maakt.

**Netwerk en stabiliteit**
- AP-fallbackmodus: als Wi-Fi drie keer achter elkaar mislukt, opent het apparaat een accesspoint
- Wi-Fi-signaalniveauindicator in de webinterface (balkjes-icoon)
- Driedubbele reset-procedure om Wi-Fi-instellingen te wissen zonder serieel
- Nachtelijke herstart: configureerbare automatische reboot (tijdstip en dagkeuze) om heap-fragmentatie op te lossen bij langlopende apparaten

**Webinterface**
- Herontworpen v2 Web UI (`v2.html`): mobielvriendelijk en de enige web-UI op de 2.0.0-lijn; de klassieke UI is verwijderd zodat alles in de LittleFS-partitie van de WiFi-update-indeling past. Bevat een first-time-setup onboarding wizard die eenmalig draait op een vers apparaat en opnieuw te starten is vanuit Instellingen.
- De webinterface is uitsluitend Engelstalig. Resterende Nederlandse strings zijn verwijderd (TASK-569). Dit handboek blijft Nederlandstalig.

**Build en tooling**
- PlatformIO-build met één `esp32-combo`-image, via `build.bat` (Windows) of `./build.sh` (Linux/macOS)
- Platformabstractie via `boards.h` met feature flags per printvariant
- SimpleTelnet: uniforme multi-client telnet-bibliotheek ter vervanging van TelnetStream en ESPTelnet uit v1.x
- Toolchain: de tijdafhandeling is overgezet op AceTime 4.x; dat staat los van de core-versie en levert een kleinere, snellere tijd-bibliotheek op.
- Flash-scripts `flash_otgw.bat` / `./flash_otgw.sh` in de zip voor de eerste USB-flash

---

### Platformvergelijking

De 2.0.0-lijn draait uitsluitend op ESP32-S3. De tabel laat zien wat hij ondersteunt op de OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro.

| Eigenschap | OTGW Classic + LOLIN S3 Mini / S3 Mini Pro |
|---|---|
| CPU | 240 MHz, 2 cores |
| Flash | 4 MB (768 KB voor LittleFS) |
| OpenTherm | Via PIC co-processor (OT-Direct-modus alleen op hardware zonder PIC) |
| Ethernet | Niet aanwezig |
| BLE sensoren | Xiaomi LYWSD03MMC via BTHome v2 |
| Bluetooth | Aanwezig, passieve scan |
| OLED-display | Optioneel (I2C-header van de Classic) |
| Dallas DS18B20 | Ondersteund (1-Wire) |
| S0-pulsteller | Ondersteund |
| PlatformIO target | `esp32-combo` |
| Build flag | `BOARD_NODOSHOP_ESP32_COMBO` |
| OTA-slots | Safeboot-updater + één app-slot van 2240 KB |
| Status | Nieuw in v2.0.0 |

---

### Systeemvereisten

Voor gebruik van OTGW-firmware heeft u nodig:

**Hardware**
- NodoShop OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro (ESP32-S3) in de Wemos D1 mini-voet
- Een OpenTherm-compatibele cv-ketel
- Een OpenTherm-compatibele thermostaat (of gebruik SAT als thermostaat)
- Een thuisnetwerk met Wi-Fi (802.11 b/g/n, 2,4 GHz)

**Optioneel maar aanbevolen**
- Een MQTT-broker op het thuisnetwerk (bijv. Mosquitto als Home Assistant add-on)
- Home Assistant voor volledige auto-discovery integratie

**Voor de webinterface**
- Een moderne webbrowser: Google Chrome, Mozilla Firefox of Apple Safari (laatste twee versies)
- Geen app-installatie, geen account, geen cloud

**Firmware flashen (eenmalig)**
- Een USB-kabel om de S3 Mini de eerste keer te flashen
- Het flash-script uit de zip: `flash_otgw.bat` (Windows) of `./flash_otgw.sh` (Linux, macOS)

De firmware vereist geen internetverbinding voor normaal gebruik. Een internetverbinding wordt optioneel gebruikt voor: NTP-tijdsynchronisatie en de Open-Meteo weer-API voor buitentemperatuur en zonnestand in SAT.

---

### Hoe dit handboek te gebruiken

Dit handboek is ingedeeld langs de gebruikelijke opstapvolgorde:

- **Hoofdstuk 2** behandelt de hardware-installatie en het flashen van de firmware.
- **Hoofdstuk 3** beschrijft de webinterface en alle functies die daarin beschikbaar zijn.
- **Hoofdstuk 4** legt uit hoe u OTGW-firmware koppelt aan Home Assistant via MQTT en auto-discovery.
- **Hoofdstuk 5** beschrijft SAT, de ingebouwde slimme thermostaat.

Technische termen als MQTT, REST API, OTA, BLE, OLED, PROGMEM en PlatformIO worden in dit document in het Engels gebruikt, omdat dat de gangbare termen zijn in de Nederlandse domotica-gemeenschap.

Codefragmenten en commando's staan in kaders:

```bash
./flash_otgw.sh
```

Tabellen met instellingen vermelden altijd de standaardwaarde en het geldige bereik.

---

### Community en ondersteuning

**GitHub**
De broncode, releases, bug tracker en pull requests staan op:
`https://github.com/rvdbreemen/OTGW-firmware`

Meld fouten en verbeterverzoeken via de GitHub Issues-pagina. Voeg bij een bugreport altijd toe: firmwareversie, firmwarelijn (1.x of 2.x), een beschrijving van het probleem en indien beschikbaar de uitvoer van de telnet-debuglog (poort 23).

**Discord**
De community is actief op de OTGW Discord-server. Hier kunt u terecht voor hulp bij installatie, configuratie en Home Assistant-integratie. Beginners zijn welkom; de community is internationaal en communiceert in het Engels.

**Releases**
Nieuwe releases worden gepubliceerd als GitHub Releases en bevatten voorgebouwde binaries: voor de 2.x-lijn de `esp32-combo`-zip, voor de 1.x-lijn de binaries voor de Wemos D1 mini (ESP8266). De webinterface biedt een ingebouwde update-functie waarmee u direct kunt updaten vanuit de lijst met beschikbare releases.

**Documentatie**
De volledige technische documentatie, inclusief C4-architectuurdiagrammen, API-referentie en ADR's, staat in de `docs/`-map van de repository.
