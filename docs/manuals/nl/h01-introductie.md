## Hoofdstuk 1: Introductie

### Wat is OTGW-firmware?

OTGW-firmware is de open-source firmware voor de NodoShop OpenTherm Gateway (OTGW). De firmware draait op een ESP8266 of ESP32 Wi-Fi-microcontroller op het OTGW-printje en verbindt de OpenTherm verwarmingsbus in uw woning met uw thuisnetwerk, een domotica-platform en een webbrowser.

Zonder firmware is de OpenTherm Gateway een slimme maar moeilijk toegankelijke hardware. Met OTGW-firmware wordt het een volledig netwerk-connected verwarmingscontroller die:

- Alle 80+ OpenTherm-berichttypen realtime decodeert en publiceert
- Via MQTT meer dan 250 Home Assistant-entiteiten automatisch aanmaakt, zonder handmatige YAML
- Een browsergebaseerde interface biedt voor monitoring, configuratie en firmware-updates
- Een REST API v2 beschikbaar stelt voor scripts, dashboards en directe integratie
- Een TCP serial bridge aanbiedt op poort 25238, compatibel met OTmonitor
- Optioneel de slimme thermostaat SAT (Smart Autotune Thermostat) uitvoert, volledig op het apparaat zelf

De firmware is ontworpen voor een vertrouwd thuisnetwerk. Er is geen clouddienst, geen app-store-account en geen internetverbinding vereist. NTP-tijdsynchronisatie en de optionele weergegevens voor SAT zijn de enige optionele online functies.

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
- MQTT-communicatie
- SAT-regeling
- Temperatuursensoren (Dallas DS18B20, S0-puls, BLE)
- OTA-updates

---

### Nieuwe functies in v2.0.0

v2.0.0 is de eerste release met volledige ondersteuning voor het nieuwe ESP32-platform en de OTGW32-hardware. De voornaamste toevoegingen ten opzichte van v1.x zijn:

**ESP32 / OTGW32-ondersteuning**
- Volledige compilatie en werking op ESP32 naast de bestaande ESP8266
- OTDirect: directe GPIO-implementatie van OpenTherm, zonder PIC co-processor
- W5500 SPI Ethernet: bekabeld netwerk als failover of primair netwerk
- BLE temperatuursensoren: passieve Bluetooth LE-scan van Xiaomi LYWSD03MMC via BTHome v2
- Aangepaste partitietabel met twee OTA-sloten van 1,5 MB en 768 KB LittleFS

**Netwerk en installatie**
- AP-fallbackmodus: als Wi-Fi drie keer achter elkaar mislukt, opent het apparaat een accesspoint
- Wi-Fi-signaalniveauindicator in de webinterface (balkjes-icoon)
- Reset WiFi-knop op de instellingenpagina
- Driedubbele reset-procedure om Wi-Fi-instellingen te wissen zonder serieel

**Build en tooling**
- Gecentraliseerde PlatformIO-build voor zowel ESP8266 als ESP32
- Verbeterd `build.py`-script met ESP32-target

**Geen breaking changes**
- MQTT-topicstructuur ongewijzigd ten opzichte van v1.x
- REST API-eindpunten ongewijzigd
- Instellingen van v1.3.x worden automatisch geladen zonder conversie

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
| PlatformIO target | `nodoshop_esp8266` | `nodoshop_esp32` |
| Build flag | `BOARD_NODOSHOP_ESP8266` | `BOARD_NODOSHOP_ESP32` |
| OTA-slots | 2 (standaard ESP8266) | 2 x 1,5 MB (custom partitie) |
| OLED-display | Ondersteund (I2C) | Ondersteund (I2C) |
| Dallas DS18B20 | Ondersteund (1-Wire) | Ondersteund (1-Wire) |
| S0-pulstemeller | Ondersteund | Ondersteund |
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

---

### Hoe dit handboek te gebruiken

Dit handboek is ingedeeld langs de gebruikelijke opstapvolgorde:

- **Hoofdstuk 2** behandelt de hardware-installatie en het flashen van de firmware.
- **Hoofdstuk 3** beschrijft de webinterface en alle functies die daarin beschikbaar zijn.
- **Hoofdstuk 4** legt uit hoe u OTGW-firmware koppelt aan Home Assistant via MQTT en auto-discovery.
- **Hoofdstuk 5** beschrijft SAT, de ingebouwde slimme thermostaat.

Technische termen als MQTT, REST API, OTA, BLE en OLED worden in dit document in het Engels gebruikt, omdat dat de gangbare termen zijn in de Nederlandse domotica-gemeenschap.

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
Nieuwe releases worden gepubliceerd als GitHub Releases en bevatten voor- en afgebakende binaries voor zowel ESP8266 als ESP32. De webinterface biedt een ingebouwde update-functie waarmee u direct kunt updaten vanuit de lijst met beschikbare releases.

**Documentatie**
De volledige technische documentatie, inclusief C4-architectuurdiagrammen, API-referentie en ADR's, staat in de `docs/`-map van de repository.
