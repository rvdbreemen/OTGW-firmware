## Hoofdstuk 10: Bijlagen

---

### 10.1 OpenTherm Bericht-ID Snelreferentie

De OpenTherm-specificatie definieert 256 bericht-ID's (0–127 voor master→slave en 128–255 voor slave→master antwoorden). De OTGW verwerkt alle gedefinieerde ID's. De onderstaande tabel toont de meest gebruikte ID's.

**Datatypes:**
- `FLAG8` — 8 statusbits als vlaggen
- `F8.8` — 16-bits vaste komma getal (hoog byte geheel, laag byte breuk), bereik ±128 met 1/256 resolutie
- `U8` — 8-bits unsigned integer (0–255)
- `S8` — 8-bits signed integer (-128–127)
- `U16` — 16-bits unsigned integer (0–65535)
- `S16` — 16-bits signed integer
- `U8/U8` — twee afzonderlijke 8-bits waarden in hoog en laag byte

**Richtingen:**
- `M→B` — master (thermostaat) naar boiler (WRITE of READ verzoek)
- `B→M` — boiler naar master (READ antwoord of ACK)

| ID | Naam | Richting | Datatype | Eenheid | OTGW-label |
|----|------|----------|----------|---------|-----------|
| 0 | Status | `M→B` / `B→M` | FLAG8 / FLAG8 | - | `status` |
| 1 | Control Setpoint | `M→B` | F8.8 | °C | `controlsetpoint` |
| 2 | Master Config/MemberID | `M→B` | FLAG8 / U8 | - | `master_configuration` |
| 3 | Slave Config/MemberID | `B→M` | FLAG8 / U8 | - | `slave_configuration` |
| 5 | Fault Flags / Code | `B→M` | FLAG8 / U8 | - | `ASF_flags` |
| 6 | Remote Parameter Settings | `B→M` | FLAG8 / FLAG8 | - | `RBP_flags_transfer_enable` |
| 9 | Remote Room Setpoint | `M→B` | F8.8 | °C | — |
| 14 | Maximum Relative Modulation Level | `M→B` | F8.8 | % | `maxrelmodlvl` |
| 16 | Room Setpoint | `M→B` | F8.8 | °C | `roomsetpoint` |
| 17 | Relative Modulation Level | `B→M` | F8.8 | % | `relmodlvl` |
| 18 | CH Water Pressure | `B→M` | F8.8 | bar | `chwaterpressure` |
| 19 | DHW Flow Rate | `B→M` | F8.8 | l/min | `dhw_flowrate` |
| 24 | Room Temperature | `M→B` | F8.8 | °C | `roomtemperature` |
| 25 | Boiler Water Temperature | `B→M` | F8.8 | °C | `boilertemperature` |
| 26 | DHW Temperature | `B→M` | F8.8 | °C | `dhwtemperature` |
| 27 | Outside Temperature | `B→M` | F8.8 | °C | `outsidetemperature` |
| 28 | Return Water Temperature | `B→M` | F8.8 | °C | `returnwatertemperature` |
| 33 | Exhaust Temperature | `B→M` | S16 | °C | `exhaust_temperature` |
| 48 | DHW Setpoint Bounds | `B→M` | S8 / S8 | °C | `dhwboundaries` |
| 49 | Max CH Setpoint Bounds | `B→M` | S8 / S8 | °C | `maxchboundaries` |
| 56 | DHW Setpoint | `M→B` / `B→M` | F8.8 | °C | `dhwsetpoint` |
| 57 | Max CH Water Setpoint | `M→B` / `B→M` | F8.8 | °C | `maxchwatersetpoint` |
| 70 | Status V/H (Ventilatie) | `B→M` | FLAG8 / FLAG8 | - | `status_vh_slave` |
| 100 | Remote Override Function | `B→M` | FLAG8 | - | `remote_override_function` |

Statusvlaggen (bericht-ID 0) — Master (hoog byte):

| Bit | Naam | Beschrijving |
|-----|------|-------------|
| 0 | CH-enable | CV-vrijgave van thermostaat |
| 1 | DHW-enable | Warmwater-vrijgave |
| 2 | Cooling-enable | Koeling vrijgave |
| 3 | OTC active | Buitentemperatuurcompensatie actief |
| 4 | CH2-enable | Tweede CV-circuit vrijgave |
| 5 | Summer/winter mode | Zomer/winter schakelaar |
| 6 | DHW blocking | Warmwater blokkering |

Statusvlaggen (bericht-ID 0) — Slave (laag byte):

| Bit | Naam | Beschrijving |
|-----|------|-------------|
| 0 | Fault indicator | Stoormelding actief |
| 1 | CH mode | CV actief (vlam of pomp) |
| 2 | DHW mode | Warmwater actief |
| 3 | Flame status | Vlam actief |
| 4 | Cooling active | Koeling actief |
| 5 | CH2 active | Tweede CV-circuit actief |
| 6 | Diagnostic indicator | Diagnostische melding |

Volledige specificatie: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`.

---

### 10.2 Woordenlijst

| Term | Uitleg |
|------|--------|
| **ADR** | Architecture Decision Record — document dat een architectuurkeuze met context en afweging vastlegt. Zie `docs/adr/`. |
| **BLE** | Bluetooth Low Energy — draadloze verbinding voor SAT-temperatuursensoren (ESP32 only). |
| **BTHome** | Open BLE-protocol voor thuisautomatisensoren, ondersteund door SAT. |
| **C4** | C4-model — architectuurvisualisatiemethode in vier niveaus: Context, Container, Component, Code. Zie `docs/c4/`. |
| **CH** | Central Heating — ruimteverwarming (de CV). |
| **DHW** | Domestic Hot Water — warmwatervoorziening (sanitair warm water). |
| **ESP8266** | Espressif microcontroller met ingebouwde WiFi, gebruikt op het Nodoshop OTGW-bord (~40 KB bruikbaar DRAM). |
| **ESP32** | Espressif microcontroller met WiFi, Bluetooth en meer RAM, gebruikt op het OTGW32-bord. |
| **F8.8** | OpenTherm-getal met vaste komma: 16 bits, waarvan 8 bit geheel en 8 bit breuk (resolutie 1/256 ≈ 0,004). |
| **HA** | Home Assistant — open source domotica-platform, primaire integratiedoelstelling. |
| **LLMNR** | Link-Local Multicast Name Resolution — naamresolutie op Windows-netwerken, poort 5355 UDP. |
| **LittleFS** | Bestandssysteem voor flash-geheugen op ESP8266/ESP32, gebruikt voor instellingen, HTML, bestanden. |
| **LWT** | Last Will and Testament — MQTT-bericht dat automatisch wordt gepubliceerd bij verbreking van de verbinding. |
| **mDNS** | Multicast DNS — zorgt dat het apparaat bereikbaar is als `<hostname>.local`. Poort 5353 UDP. |
| **MQTT** | Message Queuing Telemetry Transport — lichtgewicht publish/subscribe-protocol voor IoT. Poort 1883 TCP. |
| **NTP** | Network Time Protocol — tijdsynchronisatie via internet. Poort 123 UDP. |
| **OLED** | Organic Light Emitting Diode — klein display op het OTGW32-bord, aangestuurd via I2C (SSD1306). |
| **OT** | OpenTherm — communicatieprotocol tussen thermostaat en cv-ketel. |
| **OTGW** | OpenTherm Gateway — de hardware die de OT-bus afluistert en stuurt; hier ook: het hele project. |
| **OTGW32** | Nieuwe ESP32-S3 boardvariant met directe OT-busaansluiting (geen PIC nodig). |
| **OTDirect** | ESP32-implementatie die direct via GPIO de OT-bus communiceert, zonder PIC-microcontroller. |
| **OTA** | Over-The-Air — firmware-update via WiFi. |
| **PIC** | Microchip PIC16F-microcontroller op het Nodoshop OTGW-bord; verzorgt de OT-buscommunicatie op ESP8266. |
| **PID** | Proportioneel-Integrerend-Differentiërend — regelalgoritme gebruikt door SAT voor ruimtetemperatuurregeling. |
| **PROGMEM** | Flash-geheugenopslag voor constante tekenreeksen op AVR/ESP8266; via `F()` en `PSTR()` in code. |
| **REST** | Representational State Transfer — architectuurstijl voor HTTP-API's. |
| **S0** | Pulsteller-interface (DIN 43864) voor energiemeters; geeft pulsen per kWh. |
| **SAT** | Smart Autotune Thermostat — ingebedde PID-ruimtetemperatuurregelaar in de firmware. |
| **SSD1306** | I2C OLED-displaycontroller, 128×64 pixels. |
| **TCP** | Transmission Control Protocol — verbindingsgericht transportprotocol. |
| **TRV** | Thermostatic Radiator Valve — thermostatische radiatorkraan. |
| **UDP** | User Datagram Protocol — verbindingsloos transportprotocol (NTP, mDNS). |
| **W5500** | Wiznet hardwarecontroller voor wired Ethernet, gebruikt op OTGW32. |
| **WebSocket** | Protocol voor tweerichtingscommunicatie over HTTP-poort; gebruikt voor realtime OT-logstroom. |
| **WiFiManager** | Arduino-bibliotheek voor WiFi-configuratie via een tijdelijk access point. |

---

### 10.3 Standaardinstellingentabel

De volgende tabel toont de belangrijkste instellingen met hun standaardwaarden. Alle instellingen worden opgeslagen in `/settings.ini` op LittleFS als JSON.

#### Apparaatinstellingen

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `hostname` | `"OTGW"` | mDNS-naam en apparaatnaam |
| `httppasswd` | `""` (leeg) | HTTP Basic Auth-wachtwoord; leeg = geen authenticatie |
| `ledblink` | `true` | Status-LED knippering ingeschakeld |
| `darktheme` | `false` | Donker thema in web-UI |
| `mydebug` | `false` | Uitgebreide debuguitvoer via Telnet |

#### MQTT-instellingen

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `mqttenable` | `true` | MQTT ingeschakeld |
| `mqttbroker` | `"homeassistant.local"` | MQTT-brokerhostnaam of IP |
| `mqttport` | `1883` | MQTT-poort |
| `mqttuser` | `""` | MQTT-gebruikersnaam (optioneel) |
| `mqttpasswd` | `""` | MQTT-wachtwoord (optioneel) |
| `mqtttoptopic` | `"OTGW"` | Basisonderwerp (TopTopic) |
| `mqttuniqueid` | `"otgw-{MAC}"` | Unieke apparaat-ID |
| `mqtthaprefix` | `"homeassistant"` | Home Assistant discovery-prefix |
| `mqttinterval` | `0` | Publicatie-interval (0 = direct) |
| `mqttotmessage` | `false` | Ruwe OT-berichten publiceren |
| `mqttseparatesources` | `false` | Brongescheiden onderwerpen |
| `mqttHaRebootDetect` | `true` | HA herstart detecteren en rediscovery starten |

#### NTP-instellingen

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `ntpenable` | `true` | NTP-tijdsynchronisatie ingeschakeld |
| `ntptimezone` | `"Europe/Amsterdam"` | IANA-tijdzone |
| `ntphostname` | `"pool.ntp.org"` | NTP-server |
| `ntpsendtime` | `false` | Tijd doorsturen naar OT-bus |

#### Sensoren (Dallas DS18B20)

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `gpiosensors` | `false` | Dallas-sensoren ingeschakeld |
| `gpiosensorspin` | `10` | GPIO-pin |
| `gpiosensorsinterval` | `20` | Uitleesinterval (seconden) |
| `gpiosensorslegacyformat` | `false` | Oud sensoradresformaat |

#### S0-pulsteller

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `s0enable` | `false` | S0-pulsteller ingeschakeld |
| `s0pin` | `12` | GPIO-pin |
| `s0debouncetime` | `80` | Debouncetijd (ms) |
| `s0pulsekw` | `1000` | Pulsen per kWh |
| `s0interval` | `60` | Rapportage-interval (seconden) |

#### SAT-instellingen (selectie)

| Instellingsnaam | Standaardwaarde | Beschrijving |
|----------------|-----------------|-------------|
| `satenable` | `false` | SAT ingeschakeld |
| `sattargettemp` | `20.0` | Doel-kamertemperatuur (°C) |
| `satheatingcurvecoeff` | `1.5` | Stooklijncoëfficiënt |
| `satcontrolinterval` | `30` | Regelinterval (seconden) |
| `satdeadband` | `0.1` | PID-dode band (°C) |
| `satpresetcomfort` | `21.0` | Comfortvoorinstelling (°C) |
| `satpreseteco` | `18.0` | Eco-voorinstelling (°C) |
| `satpresetaway` | `15.0` | Afwezig-voorinstelling (°C) |
| `satovershootmargin` | `2.0` | Overshoottolerantie (°C) |
| `satmaxmodulation` | `100` | Maximale modulatie (%) |

---

### 10.4 Poortenoverzicht

De firmware opent de volgende netwerk-poorten. Alle diensten zijn beschikbaar op het lokale netwerk; geen poortdoorschakeling via de router is vereist of aanbevolen.

| Poort | Protocol | Dienst | Richting | Notitie |
|-------|----------|--------|----------|---------|
| 80 | TCP | HTTP-server | Inkomend | Web-UI, REST API v2, bestandsserver |
| 81 | TCP | WebSocket-server | Inkomend | Realtime OpenTherm-logstroom (`ws://`) |
| 23 | TCP | Telnet-debugserver | Inkomend | Debuguitvoer via `DebugTln()` / `DebugTf()` |
| 25238 | TCP | Serieel-over-TCP (ser2net) | Inkomend | Transparante PIC-seriële toegang |
| 1883 | TCP | MQTT-client | Uitgaand | Verbinding met MQTT-broker |
| 123 | UDP | NTP-client | Uitgaand | Tijdsynchronisatie |
| 5353 | UDP | mDNS | In/Uit | Naamresolutie (`<hostname>.local`) |
| 5355 | UDP | LLMNR | In/Uit | Naamresolutie op Windows-netwerken |

**Let op:** HTTPS (443) en WSS zijn doelbewust niet geïmplementeerd (ADR-003). Het apparaat is ontworpen voor gebruik op een vertrouwd lokaal netwerk. Gebruik een reverse proxy of VPN voor externe toegang indien gewenst.

---

### 10.5 Nuttige Links

#### Project en hardware

| Beschrijving | URL |
|-------------|-----|
| GitHub-repository | `https://github.com/rvdbreemen/OTGW-firmware` |
| NodoShop OTGW-hardware (ESP8266) | `https://www.nodo-shop.nl/nl/opentherm-gateway` |
| NodoShop OTGW32-hardware (ESP32) | `https://www.nodo-shop.nl/nl/` (zoek op OTGW32) |
| Firmware releases | `https://github.com/rvdbreemen/OTGW-firmware/releases` |
| Wijzigingslogboek | `CHANGELOG.md` in de repository |

#### Integraties

| Beschrijving | URL |
|-------------|-----|
| Home Assistant OTGW-integratie (HACS) | `https://github.com/mvdw73/otgw-mqtt` |
| Home Assistant PubSubClient MQTT | Ingebouwd in HA, geen extra installatie |
| OTGW Community Forum (Tweakers) | `https://gathering.tweakers.net/forum/list_messages/1754099` |
| Discord-server (ontwikkelaars) | Zie `README.md` voor uitnodigingslink |

#### Specificaties en referentie-implementaties

| Beschrijving | URL |
|-------------|-----|
| OpenTherm-specificatie v4.2 (PDF) | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` |
| Bericht-ID referentie (uitgebreid) | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md` |
| OTGW PIC-firmware (Schelte Bron) | `https://otgw.tclcode.com/firmware.html` |
| OTmonitor (referentieclient) | `https://otgw.tclcode.com/` |
| OT-Thing (ESP32 referentie) | `other-projects/OT-Thing-OTGW32/` (lokale kopie) |
| SAT Python-referentie (thermo-nova) | `other-projects/SAT-releases-thermo-nova/` (lokale kopie) |
| PIC-firmware broncode | `other-projects/otgw-6.6/` (lokale kopie) |

#### Bibliotheken

| Bibliotheek | Gebruik |
|------------|---------|
| PubSubClient | MQTT-client (`https://github.com/knolleary/pubsubclient`) |
| WebSockets | WebSocket-server (`https://github.com/Links2004/arduinoWebSockets`) |
| WiFiManager | WiFi-initialisatieportal (`https://github.com/tzapu/WiFiManager`) |
| AceTime | Tijdzonebeheer, IANA-database |
| DallasTemperature + OneWire | DS18B20 temperatuursensoren |

---

### 10.6 Wijzigingslogboeksamenvatting

Het volledige wijzigingslogboek staat in `CHANGELOG.md` in de repository. Hieronder een samenvatting van de meest opvallende wijzigingen per versie.

#### v2.0.0-beta (11 april 2026)

Dit is een grote versiesprong met uitgebreide nieuwe functionaliteit:

**Nieuw:**
- Dual-platform ondersteuning: één codebase voor ESP8266 (Nodoshop OTGW) en ESP32 (OTGW32)
- OTDirect: directe GPIO OpenTherm-communicatie op ESP32 zonder PIC-microcontroller
- W5500 Ethernet-ondersteuning op OTGW32 met automatische WiFi/Ethernet-overschakeling
- SAT (Smart Autotune Thermostat): volledig PID-regelalgoritme met stooklijnadvies, multi-zone, drukbewaking, zomerschemaschakelaar, BLE-sensoren
- OLED-display via SSD1306 (ESP32)
- AP-fallback modus (beta builds): tijdelijk access point bij ontbrekend WiFi
- PlatformIO-ondersteuning voor ESP32-builds

**Opgelost:**
- Foutieve MQTT-routering van OTGW-antwoordberichten
- NTP-tijdbewaker negeerde verkeerde initiële SDK-waarde
- SAT LittleFS-migratie veroorzaakte Exception 3 (PROGMEM-pointer naar LittleFS)
- SAT stack-overdruk bij weather JSON-verwerking

**Gewijzigd:**
- Settings/state architectuur: 62+ vlakke globals vervangen door `OTGWSettings` en `OTGWState` structs (ADR-051)
- Netwerktoegang geabstraheerd via `isNetworkUp()` en `getActiveIP()`
- LittleFS-bestandenindeling voor SAT hernoemd naar `/sat/`

#### v1.3.5 (6 april 2026)

- Firmware publiceert uptime en versie naar MQTT bij verbinding
- WiFi-herverbindingsregressie opgelost: time-out vergroot van 5 s naar 30 s voor stabielere ESP8266-verbinding

#### v1.3.4 (1 april 2026)

- Ondersteuning voor MQTT wanneer alleen een thermostaat is verbonden (geen boiler vereist)
- MQTT-throttle: stabiele waarden worden niet meer permanent onderdrukt na een mislukte publicatie

#### v1.3.3 (31 maart 2026)

- PIC-loze werking: alle PIC-functies worden automatisch uitgeschakeld wanneer geen PIC wordt gevonden; auto-herstel indien PIC later verschijnt
- REST API retourneert 503 bij PIC-afhankelijke aanvragen wanneer PIC niet aanwezig is
- Dashboard toont geen lege OT-waarden meer voor niet-ondersteunde bericht-ID's

#### v1.3.2 (29 maart 2026)

- Bestandsverwijdering betrouwbaar gemaakt: gebruik van gedeelde `cMsg`-buffer veroorzaakte achtergrondoverschrijving
- Bestandslijst: SRAM-zware `dirMap[]`-array vervangen door directe LittleFS-streaming

#### v1.3.1 (28 maart 2026)

- CS-override interferentie opgelost: setpointcommando's van HA worden niet meer overschreven
- PR-commandowachtrij-matching gecorrigeerd
- `strstr_P` crash opgelost: verwisseld PROGMEM/RAM-argument veroorzaakte Exception (2)
- Ser2net-bewustzijn: commandowachtrij detecteert OTmonitor op poort 25238 en pauzeert verwerking

#### v1.3.x serie (2025–2026)

- Home Assistant auto-discovery aanzienlijk uitgebreid
- SAT-subsysteem geïntroduceerd in vroege versies
- REST API v2 dispatchtabel ingevoerd
- OTDirect eerste ESP32-implementaties
- Diverse PROGMEM- en stack-bugfixes

Voor de volledige historiek van alle versies, zie `CHANGELOG.md`. Alle releases zijn beschikbaar als GitHub Release met bijbehorende firmware-binaries voor ESP8266 en ESP32.

---

### 10.7 Bekende beperkingen en voorbehouden

#### ESP8266 geheugen

De ESP8266 beschikt over ongeveer 40 KB bruikbaar DRAM. Dit stelt harde grenzen aan:
- Maximaal aantal gelijktijdig verbonden WebSocket-clients (20)
- Omvang van JSON-antwoorden (chunked transfer voor alles boven ~2 KB)
- SAT-ringbuffer: slechts 30 slots op ESP8266 versus meer op ESP32
- MQTT-clientbuffer: 384 bytes voor inkomende berichten

Wanneer het vrije heap-geheugen onder drempelwaarden zakt, weigert de firmware nieuwe WebSocket-verbindingen en MQTT-publicaties om crash te voorkomen. De gezondheidsdrempelwaarden:

| Niveau | Vrij heap | Gedrag |
|--------|-----------|--------|
| `healthy` | > 15 KB | Normaal |
| `low` | 10–15 KB | Waarschuwing in gezondheids-API |
| `warning` | 6–10 KB | WebSocket-berichten gepauzeerd |
| `critical` | < 6 KB | Noodmodus, gedeeltelijke sluiting |

#### Geen HTTPS of WSS

De firmware ondersteunt opzettelijk geen HTTPS of WSS (ADR-003). Dit is een bewuste keuze voor het lokale thuisnetwerk. Gebruik een reverse proxy (zoals Nginx of Caddy) voor HTTPS als externe toegang gewenst is. WebSocket over zo'n proxy werkt voor de REST API; de live OT-logstroom via WebSocket (poort 81) vereist echter directe toegang.

#### PIC-firmware en OTmonitor

Gebruik nooit OTmonitor om de PIC-firmware over WiFi bij te werken via de OTGW-firmware. Dit kan de PIC onherstelbaar beschadigen. Gebruik uitsluitend de web-UI van de OTGW-firmware voor PIC-firmware-updates.

#### Single translation unit

Alle `.ino`-bestanden worden als één vertaaleenheid gecompileerd. Dit betekent dat statische lokale variabelen in één bestand zichtbaar kunnen zijn vanuit een ander bestand als er per ongeluk naamconflicten zijn. Gebruik unieke namen voor statische locals.

#### Coöperatieve scheduling

Er is geen preëmptieve taakwisseling. Lange bewerkingen (zoals LittleFS-bestandsleesbewerkingen in loops) moeten periodiek `yield()` of `doBackgroundTasks()` aanroepen om andere taken te laten draaien. Zie `handleOTGW()` en `doAutoConfigure()` voor voorbeelden.
