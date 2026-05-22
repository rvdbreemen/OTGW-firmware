## Hoofdstuk 1: Introductie

### Wat is OTGW-firmware?

OTGW-firmware is de open-source firmware voor de NodoShop OpenTherm Gateway (OTGW). De firmware draait op een ESP8266 of ESP32 Wi-Fi-microcontroller op het OTGW-printje en verbindt de OpenTherm verwarmingsbus in uw woning met uw thuisnetwerk, een domotica-platform en een webbrowser.

De firmware wordt geleverd als een enkele, uniforme codebase die zowel de ESP8266-gebaseerde OTGW (NodeMCU of Wemos D1 mini) als het nieuwere ESP32-gebaseerde OTGW32-printje ondersteunt. Beide platformen worden volledig ondersteund en gebouwd vanuit dezelfde broncode via PlatformIO.

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

De firmware is ontworpen voor een vertrouwd thuisnetwerk. Er is geen clouddienst, geen app-store-account en geen internetverbinding vereist. NTP-tijdsynchronisatie, optionele OTA-updates via GitHub en de optionele weergegevens voor SAT via Open-Meteo zijn de enige online functies.

---

### De OpenTherm Gateway hardware

#### Het OpenTherm-protocol

OpenTherm is een open communicatieprotocol voor de verbinding tussen cv-thermostaten en cv-ketels. Het vervangt de eenvoudige aan/uit-schakelaar van oudere systemen door een bidirectionele digitale verbinding op twee draden, waarmee de thermostaat een gewenste ruimtetemperatuur of aanvoertemperatuur kan opgeven en de ketel zijn toestand, temperaturen, modulatieniveau en storingen kan melden.

Een OpenTherm-bus werkt op 24 V gelijkspanning. De communicatie verloopt via Manchester-codering met 1 bit per milliseconde. De meeste moderne cv-ketels van merken als Intergas, Vaillant, Remeha, Worcester Bosch en Nefit ondersteunen OpenTherm.

#### De PIC co-processor

Het NodoShop OTGW-printje bevat een PIC16F88 of PIC16F1847 microcontroller die de OpenTherm bus elektrisch afhandelt. De PIC zit fysiek in serie op de bus tussen de thermostaat en de ketel. Berichten worden onderschept, doorgegeven en op verzoek aangepast. De PIC communiceert met de ESP via UART op 9600 baud als ASCII-regels.

Op de ESP32 (OTGW32) is ook OTDirect beschikbaar: een directe GPIO-implementatie van OpenTherm zonder PIC, via hardware interrupts en Manchester-codering rechtstreeks op ESP32-GPIO-pinnen.

#### De ESP-module

De ESP8266 of ESP32 op het printje voert de eigenlijke firmware uit. Hij verzorgt:

- Wi-Fi (en optioneel bekabeld Ethernet via W5500 op de ESP32)
- De webserver, WebSocket-server en REST API
- MQTT-communicatie met PROGMEM-gebaseerde auto-discovery
- SAT-regeling met weercompensatie via Open-Meteo
- Temperatuursensoren (Dallas DS18B20, S0-puls, BLE op ESP32)
- OTA-updates
- Telnet debug log via SimpleTelnet (multi-client)

#### Platformabstractie (boards.h)

Sinds v2.0.0 bevat de firmware een `boards.h`-header die pin-mappings, feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`) en hardwaremogelijkheden per printvariant definieert. Dit scheidt platformspecifieke configuratie netjes van de applicatielogica.

---

### Nieuwe functies in v2.0.0

v2.0.0 is een grote platformrelease. Het levert volledige ondersteuning voor het ESP32-platform en de OTGW32-hardware, naast het bestaande ESP8266-pad, in een enkele uniforme codebase. Instellingenbestanden van v1.3.x worden automatisch geladen zonder conversie.

v2.0.0 verkeert op het moment van schrijven nog in alpha. De bestaande functionaliteit is feature-compleet, maar SAT, OTDirect en de nieuwe MQTT-topicnamen (zie hieronder) worden nog verder verfijnd. Volg de GitHub Releases-pagina voor de actuele alpha-tag.

**Breaking change in deze release.** v2.0.0 gebruikt standaard een nieuwe, zelfbeschrijvende set MQTT-topicnamen (ADR-106). Zevenendertig OT-spec-georiënteerde binary_sensor-topics krijgen een duidelijker, Home Assistant-vriendelijk label (bijvoorbeeld: `master_ch_enable` wordt `central_heating_enabled`). Gebruikers met bestaande automations op de oude labels kunnen `settings.mqtt.bUseLegacyOtTopics` op `true` zetten om het v1.x-gedrag te behouden. De twee modi sluiten elkaar uit: of de nieuwe namen, of de oude namen worden gepubliceerd, nooit beide tegelijk. De REST API is ongewijzigd ten opzichte van v1.x.

**ESP32 / OTGW32-ondersteuning**
- Volledige compilatie en werking op ESP32 naast de bestaande ESP8266
- OTDirect: directe GPIO-implementatie van OpenTherm, zonder PIC co-processor, met vijf werkingsmodi (thermostaat, ketel, gateway, monitor en gecombineerd master+slave)
- W5500 SPI Ethernet: bekabeld netwerk met dynamische runtime-failover van en naar Wi-Fi (zonder reboot)
- BLE temperatuursensoren: passieve Bluetooth LE-scan van Xiaomi LYWSD03MMC via BTHome v2 (maximaal 4 sensoren)
- Aangepaste partitietabel met twee OTA-sloten van 1,5 MB en 768 KB LittleFS

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
- Streaming MQTT discovery: discovery payloads worden gecompileerd in flash (PROGMEM) en asynchroon gepubliceerd via een bitmap-gestuurd drip-mechanisme dat elke entiteit rechtstreeks naar de broker streamt. Dit vervangt de eerdere LittleFS- en RAM-gebaseerde aanpak en elimineert grote staging-buffers.
- SAT switches en select entities zijn toegevoegd aan de HA discovery via dezelfde streaming-pipeline (TASK-284), samen met runtime-gedetecteerde Dallas-sensoren, de climate- en number-entiteiten, en HA button- en select-entiteiten voor PIC-commando's.
- Just-in-time MQTT discovery (ADR-100): discovery-configs voor OpenTherm MsgIDs worden nu pas gepubliceerd wanneer de ketel of thermostaat die MsgID voor het eerst zendt, in plaats van bij elke boot alle 256 IDs in bulk te publiceren. Dit houdt het broker-register beperkt tot de IDs die uw hardware daadwerkelijk gebruikt en vermindert MQTT-verkeer bij het opstarten.
- Platte MQTT-topics per waarde (ADR-101): elke sensor of regelwaarde wordt als platte scalar op zijn eigen topic gepubliceerd. Geen samengestelde JSON-state-topics. Dit is een bewuste keuze die auto-discovery transparant houdt en een eigen HA-component overbodig maakt.

**Netwerk en stabiliteit**
- AP-fallbackmodus: als Wi-Fi drie keer achter elkaar mislukt, opent het apparaat een accesspoint
- Wi-Fi-signaalniveauindicator in de webinterface (balkjes-icoon)
- Driedubbele reset-procedure om Wi-Fi-instellingen te wissen zonder serieel
- Nachtelijke herstart: configureerbare automatische reboot (tijdstip en dagkeuze) om heap-fragmentatie op te lossen bij langlopende ESP8266-apparaten

**Webinterface**
- De webinterface is uitsluitend Engelstalig. Resterende Nederlandse strings uit de OTTHING-platformport zijn verwijderd (TASK-569). Dit handboek blijft Nederlandstalig.

**Build en tooling**
- Gecentraliseerde PlatformIO-build voor zowel ESP8266 als ESP32
- Platformabstractie via `boards.h` met feature flags per printvariant
- SimpleTelnet: uniforme multi-client telnet-bibliotheek ter vervanging van TelnetStream en ESPTelnet uit v1.x
- Toolchain: ESP8266-builds blijven op Arduino core 2.7.4 (de LTS-basis). De 1.4.x-lijn stapte tijdelijk over op core 3.1.2, maar de v1.5.x-LTS-lijn is teruggekeerd naar 2.7.4 voor veldgeteste stabiliteit (lwIP ging mee terug naar de 2.7.4-release). De tijdafhandeling is overgezet op AceTime 4.x; dat staat los van de core-versie en levert een kleinere, snellere tijd-bibliotheek op.
- Verbeterd `build.py`-script en `flash_esp.py` met ESP32-ondersteuning

---

### Platformvergelijking: ESP8266 vs ESP32

| Eigenschap | ESP8266 (OTGW v1.x) | ESP32 (OTGW32) |
|---|---|---|
| CPU | 80/160 MHz, 1 core | 240 MHz, 2 cores |
| RAM | ~80 KB bruikbaar DRAM | ~300 KB bruikbaar DRAM |
| Flash | 4 MB (2 MB voor LittleFS) | 4 MB (768 KB voor LittleFS) |
| OpenTherm | Via PIC co-processor | Via PIC of OTDirect (GPIO) |
| Ethernet | Niet ondersteund | W5500 SPI (optioneel) |
| BLE sensoren | Niet ondersteund | Xiaomi LYWSD03MMC via BTHome v2 |
| Bluetooth | Niet aanwezig | Aanwezig, passieve scan |
| OLED-display | Ondersteund (I2C) | Ondersteund (I2C) |
| Dallas DS18B20 | Ondersteund (1-Wire) | Ondersteund (1-Wire) |
| S0-pulsteller | Ondersteund | Ondersteund |
| PlatformIO target | `esp8266` | `esp32` |
| Build flag | `BOARD_NODOSHOP_ESP8266` | `BOARD_NODOSHOP_ESP32` |
| OTA-slots | 2 (standaard ESP8266) | 2 x 1,5 MB (custom partitie) |
| Status | Productie | Nieuw in v2.0.0 |

---

### Systeemvereisten

Voor gebruik van OTGW-firmware heeft u nodig:

**Hardware**
- NodoShop OpenTherm Gateway met ESP8266 of ESP32 (OTGW32)
- Een OpenTherm-compatibele cv-ketel
- Een OpenTherm-compatibele thermostaat (of gebruik SAT als thermostaat)
- Een thuisnetwerk met Wi-Fi (802.11 b/g/n, 2,4 GHz) of bekabeld Ethernet (alleen OTGW32 met W5500)

**Optioneel maar aanbevolen**
- Een MQTT-broker op het thuisnetwerk (bijv. Mosquitto als Home Assistant add-on)
- Home Assistant voor volledige auto-discovery integratie

**Voor de webinterface**
- Een moderne webbrowser: Google Chrome, Mozilla Firefox of Apple Safari (laatste twee versies)
- Geen app-installatie, geen account, geen cloud

**Firmware flashen (eenmalig)**
- Een USB-kabel (micro-USB of USB-C, afhankelijk van de ESP-module) voor de eerste installatie
- Python 3.x voor het `flash_esp.py`-script, of PlatformIO voor compilatie vanuit broncode

De firmware vereist geen internetverbinding voor normaal gebruik. Een internetverbinding wordt optioneel gebruikt voor: NTP-tijdsynchronisatie, OTA-updates via GitHub, en de Open-Meteo weer-API voor buitentemperatuur en zonnestand in SAT.

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
python flash_esp.py --port COM3 firmware.bin
```

Tabellen met instellingen vermelden altijd de standaardwaarde en het geldige bereik.

---

### Community en ondersteuning

**GitHub**
De broncode, releases, bug tracker en pull requests staan op:
`https://github.com/rvdbreemen/OTGW-firmware`

Meld fouten en verbeterverzoeken via de GitHub Issues-pagina. Voeg bij een bugreport altijd toe: firmwareversie, platformtype (ESP8266/ESP32), een beschrijving van het probleem en indien beschikbaar de uitvoer van de telnet-debuglog (poort 23).

**Discord**
De community is actief op de OTGW Discord-server. Hier kunt u terecht voor hulp bij installatie, configuratie en Home Assistant-integratie. Beginners zijn welkom; de community is internationaal en communiceert in het Engels.

**Releases**
Nieuwe releases worden gepubliceerd als GitHub Releases en bevatten voorgebouwde binaries voor zowel ESP8266 als ESP32. De webinterface biedt een ingebouwde update-functie waarmee u direct kunt updaten vanuit de lijst met beschikbare releases.

**Documentatie**
De volledige technische documentatie, inclusief C4-architectuurdiagrammen, API-referentie en ADR's, staat in de `docs/`-map van de repository.
