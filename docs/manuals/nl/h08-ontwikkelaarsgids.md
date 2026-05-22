## Hoofdstuk 8: Ontwikkelaarsgids

Dit hoofdstuk is de primaire referentie voor iedereen die bijdraagt aan of ontwikkelt met de OTGW-firmware broncode. Het behandelt de projectindeling, het bouwsysteem, het architectuurmodel, belangrijke codeerpatronen en de stapsgewijze workflows voor het toevoegen van nieuwe REST-eindpunten, MQTT-topics en instellingen. Lees dit hoofdstuk voordat je ook maar een regel code schrijft.

---

### 8.1 Projectindeling

De broncode bevindt zich in `src/OTGW-firmware/`. Elk `.ino`-bestand vormt een logische module; de Arduino IDE en PlatformIO behandelen alle `.ino`-bestanden in dezelfde map als één single translation unit, terwijl `.cpp`-bestanden afzonderlijk worden gecompileerd. `OTGW-firmware.h` wordt als eerste verwerkt: het is nu vooral een aggregator die de per-component `*types.h`-headers includeert en de globale `settings` en `state` instanties definieert (ADR-079).

#### Bronbestanden

| Bestand | Domein |
|---------|--------|
| `OTGW-firmware.ino` | Hoofdbestand: `setup()`, `loop()`, bootsequentie, LED-beheer |
| `OTGW-Core.ino` | OpenTherm-protocoldecodering, PIC serieel I/O, commandowachtrij, MQTT-throttle |
| `OTGW-Core.h` | OpenTherm-datastructuren: `OTdataStruct`, `OTLibMessageID`, frame type enums |
| `OTGW-firmware.h` | Top-level aggregator: includeert de per-component `*types.h` headers, definieert de aggregate `OTGWSettings` / `OTGWState`, forward declarations, gedeelde constanten |
| `*types.h` | Per-component state + settings structheaders (ADR-079 / ADR-081). Eén bestand per subsysteem: `SATtypes.h`, `OTDirecttypes.h`, `Devicetypes.h`, `Hardwaretypes.h`, `Networktypes.h`, `PICtypes.h`, `OTBustypes.h`, `Flashtypes.h`, `Uptimetypes.h`, `NTPtypes.h`, `Sensorstypes.h`, `S0types.h`, `Outputstypes.h`, `Webhooktypes.h`, `UItypes.h` |
| `MQTTstuff.ino` | MQTT-client state machine, HA auto-discovery, commandoabonnementen, `sendMQTTData()` |
| `networkStuff.ino` | WiFi, mDNS, LLMNR, NTP, OTA, platformabstractie netwerk |
| `restAPI.ino` | REST API v2 dispatchtabel, alle resource handlers, HTTP Basic Auth, CSRF, bestandsserving |
| `FSexplorer.ino` | LittleFS-browser: bestandslijst, upload/verwijder-eindpunten |
| `jsonStuff.ino` | Low-level JSON formatting helpers: `sendJsonMapEntry()`, `sendJsonKVLine()`, streaming map builders |
| `settingStuff.ino` | Instellingen laden/opslaan (LittleFS JSON), `updateSetting()` validatie, gedebouncede flush, side-effect dispatch |
| `webSocketStuff.ino` | WebSocket-server poort 81, OT-logstreaming, heap backpressure gate |
| `helperStuff.ino` | Heap health monitor, `canSendWebSocket()`, `canPublishMQTT()`, LittleFS-statushelpers |
| `OTDirect.ino` | ESP32-only: directe GPIO OpenTherm-bus via ISR, OTDirect operating modes, command queue met coalesce-by-MsgID |
| `SATcontrol.ino` | SAT-regellus, verwarmingscurve, boiler state machine, setpoint-injectie, master enable/disable |
| `SATpid.ino` | PID v3 implementatie: proportioneel, integraal, derivaat met deadband |
| `SATcycles.ino` | Cyclusclassificatie, overshootdetectie, anti-cycling |
| `SATpressure.ino` | SAT boilerdrukbewaking, lage-drukwaarschuwing, trenddetectie |
| `SATweather.ino` | Open-Meteo weersophaling, buitentemperatuur voor SAT-verwarmingscurve |
| `SATble.ino` | ESP32 BLE kamertemperatuursensor scanner op NimBLE-Arduino (ADR-092), BTHome v2-protocol |
| `MQTTHaDiscovery.cpp` | Gegenereerde PROGMEM-tabellen voor MQTT Home Assistant auto-discovery (sensors, binary sensors, climate, number) |
| `mqtt_discovery_verify.cpp` | Compile-time sanity checks op de discovery-tabellen (geen runtime-kosten) |
| `sensors_ext.ino`, `s0PulseCount.ino`, `OLED.ino` | Dallas DS18B20 temperatuursensoren, S0-pulsteller, OLED-display |
| `Ethernet.ino` | W5500 SPI-Ethernet runtime-probe en failover (alleen ESP32) |
| `boards.h` | Boardspecifieke pin maps en feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`) |
| `safeTimers.h` | Timer-macro's: `DECLARE_TIMER_SEC()`, `DUE()`, `RESTART_TIMER()` |
| `platform.h`, `platform_esp8266.h`, `platform_esp32.h` | Platformabstractielaag |

#### Web Assets

De browser-SPA bevindt zich in `src/OTGW-firmware/data/`. Deze bestanden worden naar LittleFS geflasht:

| Bestand | Doel |
|---------|------|
| `index.html` | Single-page application shell (~11 KB, altijd streamen, nooit in RAM laden) |
| `index.js` | Hoofd-SPA JavaScript: live OT-log, WebSocket-client, instellingenformulieren |
| `graph.js` | ECharts-gebaseerde realtime temperatuurgrafieken |
| `index.css` | Stylesheet met donker/licht thema-ondersteuning |

#### Bouwinfrastructuur

| Bestand / Map | Doel |
|--------------|------|
| `build.py` | Primair bouwscript: roept PlatformIO (of arduino-cli) intern aan, verpakt firmware- en LittleFS-artefacten, produceert per-target distributiezips |
| `flash_esp.py` | Platform-onafhankelijke flasher: detecteert de seriele poort, downloadt eventueel de releasezip, schrijft firmware + filesystem voor ESP8266 en ESP32 |
| `platformio.ini` | PlatformIO-project: `esp8266` en `esp32` omgevingen |
| `evaluate.py` | Statische codekwaliteitscontrole: PROGMEM-gebruik, onveilige patronen, String-klasse audit |
| `tools/generate_mqttha_data.py` | Legacy-regenerator voor `MQTTHaDiscovery.cpp` uit een gearchiveerd `mqttha.cfg`-template |
| `scripts/` | PlatformIO pre-build scripts: versie-injectie, bibliotheekpatching |
| `libraries/` | Vendored Arduino-bibliotheken gebruikt door beide bouwsystemen |
| `docs/` | C4-architectuurdocs, ADRs, API-referenties, functiedocumentatie |
| `other-projects/` | Read-only referentiecodebases (PIC-firmware, OTmonitor, OT-Thing, SAT) |
| `backlog/` | Projecttaakbeheer (Backlog.md) |

---

### 8.2 Bouwsysteem

#### build.py (releasebouwen)

Het primaire bouwscript voor het produceren van release-artefacten. Het roept PlatformIO (of arduino-cli) intern aan, verzorgt versie-inbedding, filesystemverpakking en artefactverzameling.

```bash
python build.py                                   # Volledige build voor ESP8266 + ESP32 (PlatformIO, incrementeel)
python build.py --target esp8266                  # Alleen ESP8266
python build.py --target esp32                    # Alleen ESP32
python build.py --target all                      # Beide targets (standaard)
python build.py --backend platformio              # PlatformIO backend expliciet (standaard)
python build.py --backend arduino-cli             # Legacy arduino-cli backend (alleen ESP8266)
python build.py --pio                             # Verkort voor --backend platformio
python build.py --arduino-cli                     # Verkort voor --backend arduino-cli
python build.py --firmware                        # Alleen firmware, geen filesystem
python build.py --filesystem                      # Alleen filesystem
python build.py --clean                           # Verwijder build-artefacten, behoud libdeps-cache
python build.py --distclean                       # Verwijder build-artefacten + cores + bibliotheken-cache
python build.py --merged                          # Bouw een samengevoegde firmware+filesystem-binary
python build.py --merged --compress               # Samengevoegde binary, gzip-gecomprimeerd
python build.py --no-install-cli                  # Sla arduino-cli install-controle over
python build.py --no-color                        # Schakel gekleurde uitvoer uit
python build.py --help                            # Toon help
```

Standaard gebruikt `build.py` de PlatformIO-backend en bouwt beide targets (ESP8266 + ESP32). De `--arduino-cli` vlag selecteert de legacy arduino-cli backend, die alleen ESP8266 ondersteunt. Het script leest de Git-tag voor versienummerinbedding en plaatst uitvoer in de `build/`-map.

**Python-versie**: PlatformIO vereist Python 3.10 tot en met 3.13. Python 3.14 wordt afgewezen door PlatformIO's eigen versiecontrole. `build.py` geeft voorrang aan de door PlatformIO zelf meegeleverde Python in `~/.platformio/penv/` (meestal 3.11.x) wanneer beschikbaar, zodat een systeembrede Python 3.14-installatie het bouwen niet blokkeert zolang de PlatformIO-virtualenv aanwezig is.

**esptool**: `build.py` gebruikt esptool v5 voor het maken van samengevoegde binaries (`--merged`). Het wordt automatisch geinstalleerd bij het eerste gebruik via `pip install esptool` als het nog niet aanwezig is.

#### PlatformIO

PlatformIO is het voorkeursbouwsysteem en wordt gebruikt voor zowel ESP8266 als ESP32. Twee omgevingen zijn gedefinieerd in `platformio.ini`:

| Omgeving | Target | Board | Core | CPU |
|----------|--------|-------|------|-----|
| `esp8266` | Wemos D1 mini / NodeMCU (NodoShop OTGW) | `d1_mini` | Arduino Core 2.7.4 LTS via `espressif8266@2.6.3` (GCC 4.8.2) | 160 MHz |
| `esp32` | NodoShop OTGW32 (ESP32-S3) | `esp32-s3-devkitc-1` | pioarduino espressif32 fork | 240 MHz |

De ESP8266-omgeving is vastgepind op `espressif8266@2.6.3` (Arduino Core 2.7.4 LTS). De firmware is kort getest op `espressif8266@4.2.1` (Core 3.1.2 / GCC 10.3) maar teruggedraaid na veldmeldingen over unaligned PROGMEM-leesoperaties door word-aligned `strstr`/`strncmp`-toegang tot flash. Core 2.7.4 is de productiedoelversie voor zowel de 1.5.x- als de 2.0.0-lijn.

De ESP8266 LittleFS-partitie is 2 072 576 bytes (circa 2 MB). De ESP32-layout is `4M2M` (2 MB LittleFS, `boot_app0.bin` op `0xe000`, DIO flash mode). Alles is geconfigureerd in `platformio.ini` via de boardopties.

```bash
pio run                          # Bouw alle omgevingen
pio run -e esp8266               # Bouw alleen ESP8266
pio run -e esp32                 # Bouw alleen ESP32
pio run -e esp8266 -t upload     # Bouw en upload ESP8266
pio run -e esp8266 -t uploadfs   # Upload LittleFS-filesystem (ESP8266)
pio run -e esp32 -t upload       # Bouw en upload ESP32
pio run -e esp32 -t uploadfs     # Upload LittleFS-filesystem (ESP32)
```

De pre-build scripts in `scripts/` verzorgen versie-injectie (`platformio_version.py`) en compatibiliteitspatching van bibliotheken (`patch_pio_libs.py`).

##### ESP8266-specifieke PlatformIO-opmerkingen

De ESP8266-build sluit `OTDirect.ino` (ESP32-only) uit via `build_src_filter`. PlatformIO's ctags-scanner verwerkt alle `.ino`-bestanden ongeacht het filter, dus ESP32-specifieke types (`OpenThermResponseStatus`, `OTDirectMode`, `OTDirectRequestOrigin`) worden als `-D`-vlaggen gestubbed om ctags-gerelateerde forward declaration fouten te voorkomen. Deze stubs hebben geen runtime-effect.

De `OpenTherm Library` is expliciet uitgesloten in `lib_ignore` omdat PlatformIO's LDF-scanner `#if`-bewakers negeert en de ESP32-only OT-bibliotheek anders ook voor ESP8266 zou compileren.

##### ESP32-specifieke PlatformIO-opmerkingen

PlatformIO's ctags-scanner verwerkt alle `.ino`-bestanden ongeacht `#if`-bewakers en genereert forward declarations voor elke gevonden functie. Op ESP32 (`HAS_PIC=0`) wordt `OTGWSerial.h` nooit geincludeerd, waardoor `OTGWFirmware` (een enum type gebruikt als parameter in `fwreportinfo()`) onbekend is op het moment dat de ctags-forward declaration wordt gecompileerd. De bouwvlag `-DOTGWFirmware=int` stelt het type tijdelijk in als `int` zodat de gegenereerde forward declaration compileert. De eigenlijke `fwreportinfo()`-functie bevindt zich in een `#if HAS_PIC`-blok en wordt nooit gecompileerd op ESP32. Dit volgt hetzelfde patroon als de OTDirect type-stubs voor ESP8266.

#### Arduino IDE

De Arduino IDE wordt ondersteund voor alleen ESP8266. Installeer de ESP8266 Arduino-core en open `src/OTGW-firmware/OTGW-firmware.ino`. Vereiste bibliotheken staan in `libraries/`. Voeg die map toe aan het Arduino sketchbook-bibliotheekpad. De IDE ondersteunt niet het ESP32-target of `OTDirect.ino`.

#### evaluate.py

Voer de codekwaliteitscontrole uit na wijzigingen om veelvoorkomende ESP8266-specifieke fouten te detecteren:

```bash
python evaluate.py                  # Volledige evaluatie (alle bestanden)
python evaluate.py --quick          # Alleen essentiele controles
python evaluate.py --report         # Genereer gedetailleerd JSON-rapport
python evaluate.py --output FILE    # Aangepast rapportpad (standaard: evaluation-report.json)
python evaluate.py --verbose        # Toon alle controles, niet alleen fouten
python evaluate.py --no-color       # Schakel gekleurde uitvoer uit
```

Het script voert uitgebreide evaluaties uit, waaronder:
- Codekwaliteitsmetrieken (PROGMEM-gebruik, onveilige patronen, String-klasse audit)
- Build system validatie
- Gezondheidscontrole van afhankelijkheden
- Documentatiedekking
- Beveiligingsanalyse
- Geheugen- en bronnenanalyse

Belangrijkste patronen die worden gemarkeerd:
- `Serial.print()`-aanroepen (mogen nooit verschijnen na initialisatie)
- Letterlijke tekenreeksen niet verpakt in `F()` of `PSTR()`
- `String`-klasse in hot paths
- `strcpy`/`sprintf` zonder grenzen (moet `strlcpy`/`snprintf_P` zijn)
- `strncmp_P`/`strstr_P` op binaire data (gebruik `memcmp_P`)

#### tools/generate_mqttha_data.py (legacy-regenerator)

De MQTT Home Assistant auto-discovery-metadata bevindt zich in gecompileerde vorm als `src/OTGW-firmware/MQTTHaDiscovery.cpp`. Dat bestand is de **source of truth** en is in de repository opgenomen. Het bevat gestructureerde PROGMEM-arrays voor sensors, binary sensors, climate en number entities, geindexeerd op OT-message-ID.

Het teksttemplate `mqttha.cfg` dat dit bestand oorspronkelijk genereerde is gearchiveerd naar `docs/archive/mqttha.cfg` en maakt geen deel meer uit van de build. Mocht je `MQTTHaDiscovery.cpp` uit een bijgewerkt template willen regenereren, herstel dan `mqttha.cfg` naar `src/OTGW-firmware/data/mqttha.cfg` en voer uit:

```bash
python tools/generate_mqttha_data.py
```

Voor normaal ontwikkelwerk pas je `MQTTHaDiscovery.cpp` rechtstreeks aan en houd je het consistent met de streaming-discovery-consumer in `MQTTstuff.ino`. De legacy-generators `tools/generate_mqttha_progmem.py` en `tools/generate_mqttha_readable.py` zijn behouden als referentie maar worden vervangen door `generate_mqttha_data.py`.

Het gegenereerde bestand is bewust in een eigen `.cpp` translation unit geplaatst om de Xtensa single-TU section/relocation-explosie te voorkomen die optreedt wanneer grote PROGMEM-data in de hoofd-sketch wordt geplaatst.

---

### 8.3 Boardhardwarevarianten

De firmware ondersteunt twee officiële Nodoshop-hardwarevarianten, gedefinieerd in `boards.h`. Het board wordt geselecteerd tijdens de build via een `BOARD_*`-preprocessorvlag in `platformio.ini`. Wanneer geen expliciete vlag is ingesteld, wordt het board automatisch afgeleid van het MCU-type (`ESP8266` of `ESP32`).

**Deze vlaggen zijn boardniveau, niet MCU-niveau.** De aanwezigheid van een PIC, Ethernet-hardware of een directe OpenTherm GPIO-interface is een eigenschap van het specifieke Nodoshop-printboard, niet van de MCU-familie. Het vervangen van een ESP8266 door een ESP32 op het OTGW WiFi-board is geen officiële Nodoshop-configuratie en wordt hier niet gemodelleerd.

#### Compilatievlaggen

Vier compilatietijdvlaggen in `boards.h` beschrijven de hardwaremogelijkheden van elk board:

| Vlag | Type | OTGW WiFi (ESP8266) | OTGW32 (ESP32) | Betekenis |
|------|------|---------------------|----------------|-----------|
| `HAS_PIC` | `0` / `1` | `1` | `0` | Board heeft een PIC-co-processor die de OpenTherm-bus afhandelt via UART |
| `HAS_DIRECT_OT` | `0` / `1` | `0` | `1` | Board stuurt de OpenTherm-bus rechtstreeks aan via GPIO en de OTDirect ISR |
| `HAS_ETH_CAPABLE` | `0` / `1` | `0` | `1` | Board heeft een W5500 SPI-Ethernetmodule |
| `HAS_OLED_CAPABLE` | `0` / `1` | `0` | `1` | Board bevat de onboard SSD1306 128x64 I2C-OLED (runtime-probe op adres `0x3C`) |

Gebruik deze vlaggen in conditionele compilatie:

```cpp
#if HAS_PIC
  // PIC-specifieke code: addCommandToQueue(), OTGWSerial, fwreportinfo(), etc.
#endif

#if HAS_DIRECT_OT
  // OTDirect GPIO-code: loopOTDirect(), OTDirectMode, etc.
#endif

#if HAS_ETH_CAPABLE
  // Ethernetcode: loopEthernet(), state.hw.bEthernetPresent, etc.
#endif
```

#### W5500-runtimedetectie

`HAS_ETH_CAPABLE=1` betekent dat het boardontwerp een W5500-module bevat, maar de module hoeft niet op elke unit fysiek aanwezig te zijn. Bij het opstarten leest `probeW5500()` in `Ethernet.ino` het VERSION-register van de W5500 via SPI:

- Als het register de verwachte waarde teruggeeft, wordt `state.hw.bEthernetPresent` op `true` gezet en wordt Ethernet geïnitialiseerd.
- Als het lezen mislukt of een onverwachte waarde teruggeeft, wordt `state.hw.bEthernetPresent` op `false` gezet en draait de firmware in WiFi-only modus.

Er is geen gebruikersconfiguratie vereist. `loopEthernet()` bewaakt zichzelf met `if (!state.hw.bEthernetPresent) return;` en doet niets wanneer er geen hardware aanwezig is.

---

### 8.4 C4-architectuuroverzicht

De firmware gebruikt een vierniveau C4-model, gedocumenteerd in `docs/c4/`. Lees het relevante niveau voordat je code aanraakt.

#### Niveau 1: Context (`c4-context.md`)

De OTGW-firmware bevindt zich tussen de OpenTherm-verwarmingsbus en het domotica-netwerk. Externe actoren zijn: kamerthermostaat, ketel (via PIC co-processor of OTDirect GPIO op ESP32), MQTT-broker, Home Assistant, NTP-server, webbrowser, OTmonitor TCP-client, Dallas/S0/BLE-sensoren.

#### Niveau 2: Container (`c4-container.md`)

Twee deployment targets delen een codebase:
- **ESP8266 (OTGW)**: NodoShop-board met PIC16F co-processor. De PIC handelt de OpenTherm-bus af via UART aan 9600 baud.
- **ESP32 (OTGW32)**: NodoShop-board met directe GPIO OpenTherm-interface (`OTDirect`), W5500 Ethernet, BLE en OLED. Geen PIC; de ESP32 stuurt de OT-bus rechtstreeks aan via interrupt-gedreven Manchester-codering.

#### Niveau 3: Component (`c4-component.md`)

Zeven componenten, elk met een eigen document in `docs/c4/c4-component-*.md`:

| Component | Primaire bestanden | Verantwoordelijkheid |
|-----------|-------------------|---------------------|
| OpenTherm Core | `OTGW-Core.ino`, `OTDirect.ino` | Protocoldecode, PIC UART, commandowachtrij |
| Netwerk en connectiviteit | `networkStuff.ino` | WiFi, NTP, mDNS, Ethernet failover |
| Integratielaag | `MQTTstuff.ino`, `restAPI.ino`, `jsonStuff.ino` | MQTT, REST API, HA discovery |
| Configuratie en status | `settingStuff.ino`, `OTGW-firmware.h` | Instellingenpersistentie, OTGWSettings/State |
| Smart Thermostat (SAT) | `SAT*.ino`, `SATcontrol.ino`, `SATpid.ino` | Verwarmingscurve, PID, cyclusbeheer |
| Sensoren en hardware | `sensorStuff.ino` | Dallas, S0, GPIO-relais, OLED |
| Webinterface | `webSocketStuff.ino`, `helperStuff.ino`, `data/` | WebSocket, telnet, heap gate, browser-SPA |

#### Niveau 4: Code (`c4-code-*.md`)

Gedetailleerde functiehandtekeningen, regelnummers en afhankelijkheden tussen bestanden voor elke module. Lees het relevante `c4-code-*.md`-bestand voordat je een niet-triviale functie wijzigt.

---

### 8.5 Sleutelpatronen

#### Coöperatieve scheduling

De ESP8266 heeft geen preëmptieve multitasking. De `loop()`-functie roept `doBackgroundTasks()` aan, dat reeksen taken één voor één uitvoert. Er is geen RTOS, geen interrupt-gebaseerde taakwisseling voor applicatiecode.

Kritische implicaties:
- Functies mogen niet blokkeren. Gebruik timers (`DUE()`), nooit `delay()`.
- `feedWatchDog()` in lange lussen voorkomt een reset door de watchdog.
- `doBackgroundTasks()` kan re-entrant worden aangeroepen vanuit bestandsleesfuncties (via `yield()`). Buffers die gedeeld worden over een yield-venster moeten `static` of lokaal zijn.

#### Statische buffers

RAM is schaars (~40 KB bruikbaar DRAM op de ESP8266). Richtlijnen:
- Gebruik `char[]`-buffers met `strlcpy()`, `snprintf_P()`, `strncat()`.
- Gebruik nooit de `String`-klasse in kritieke paden (heap-fragmentatie, ADR-004).
- Grote buffers op de stack veroorzaken stack overflow (CONT-stack: ~4 KB). Maak ze `static`.

#### PROGMEM

Alle letterlijke tekenreeksen moeten in flash blijven, niet in RAM:

```cpp
// Correct: tekenreeks in flash
DebugTln(F("Verbonden met MQTT broker"));
DebugTf(PSTR("Temperatuur: %d graden\r\n"), temp);
snprintf_P(buf, sizeof(buf), PSTR("Waarde: %s"), value);
const char myStr[] PROGMEM = "Lange vaste tekenreeks";

// Vergelijking met PROGMEM-tekenreeks
if (strcasecmp_P(veld, PSTR("hostname")) == 0) { ... }

// Binaire data vergelijken: gebruik memcmp_P, NOOIT strncmp_P op binaire data
if (memcmp_P(datamem + ptr, banner, bannerLen) == 0) { ... }
```

Wissel nooit een PROGMEM-pointer uit voor een RAM-pointer en omgekeerd: dit veroorzaakt Exception (2) crashes.

Het MQTT-pad voegt twee ESP8266 PROGMEM-veiligheidshelpers toe (gedeclareerd in `MQTTstuff.h`): `pgm_strncmp_PP()` (byte-voor-byte PROGMEM/PROGMEM vergelijking) en `writeMqttProgmemChunk()` (stuurt een PROGMEM-payload rechtstreeks naar de broker zonder eerst in RAM te laden). Gebruik ze wanneer beide operanden in flash leven; geef nooit een `PGM_P` mee aan `printf("%s", ...)` of `MQTTclient.write()`.

#### Geen re-entrancy

De watchdog-functie `feedWatchDog()` roept geen `yield()` aan (uitgecommentarieerd). `handleOTGW()` gebruikt `static`-lokale buffers als beveiliging tegen re-entrancy vanuit `doBackgroundTasks()`.

#### Geen ArduinoJson

Voeg ArduinoJson niet toe aan dit project. JSON wordt:
- **Opgebouwd**: met `snprintf_P`, `sendJsonMapEntry()` en chunked `httpServer.sendContent()`.
- **Geparsed**: met de eigen `parseJsonKVLine()` helper (key-value line scanner, voldoende voor de platte settings JSON en MQTT-commandopayloads).

#### Heap Backpressure Gate

Twee gate-functies beschermen de heap-gezondheid tijdens normaal gebruik:

```cpp
bool canPublishMQTT();     // false wanneer heap onder HEAP_LOW drempel
bool canSendWebSocket();   // false wanneer heap onder HEAP_WARNING drempel
```

Controleer altijd de juiste gate voordat je MQTT-berichten verstuurt in achtergrondtaken. De heap-monitor in `helperStuff.ino` definieert vier niveaus: `HEAP_HEALTHY` (>8 KB), `HEAP_LOW` (5-8 KB), `HEAP_WARNING` (3-5 KB), `HEAP_CRITICAL` (<3 KB).

#### Settings / State scheiding (ADR-051, geamendeerd door ADR-079 en ADR-081)

Alle persistente configuratie leeft in `OTGWSettings settings` (geserialiseerd naar `/settings.ini` op LittleFS). Alle transiente runtime-status leeft in `OTGWState state`. Beide structs gebruiken tweeniveau benoemde subsecties met Hongaarse prefixen:

```cpp
settings.mqtt.sBroker          // persistent: MQTT-brokerhostnaam
settings.sat.fHeatingCurveCoeff // persistent: SAT-verwarmingscurvecoefficient
state.otgw.bOnline             // transient: PIC serieel verbinding actief
state.pic.sFwversion           // transient: PIC-firmwareversiestring
```

Volgens ADR-079 declareert elk subsysteem zijn sectiestructs in een `*types.h`-header (`SATtypes.h` bevat zowel `SATRuntimeSection` als `SATSection`; `OTDirecttypes.h` bevat zowel `OTDirectSection` als `OTDirectSettingsSection`). `OTGW-firmware.h` `#include`t deze headers en definieert de aggregate `OTGWSettings` / `OTGWState` plus de globale `settings` en `state`. ADR-081 vereenvoudigt de regel: bestaat er al een `<Component>stuff.h`-sibling, dan vouwen de types in dat bestand in plaats van een aparte `<Component>types.h` aan te maken. Veldtoegang blijft ongewijzigd: nog steeds `settings.sat.<veld>` / `state.sat.<veld>`.

Persisteer nooit transiente status. Benader nooit `state`-velden vanuit `writeSettings()`. Bij het toevoegen van een nieuw subsysteem heeft een nieuwe `<Component>types.h` de voorkeur boven het laten groeien van `OTGW-firmware.h`.

#### Platformabstractielaag (ADR-061)

Alle ESP8266/ESP32 SDK-verschillen zijn geïsoleerd in `platform.h`, `platform_esp8266.h` en `platform_esp32.h`. Applicatiecode includeert uitsluitend `platform.h` en roept de platformonafhankelijke functies aan. Roep nooit platform-specifieke SDK-functies rechtstreeks aan in `.ino`-bestanden.

Belangrijke functies:

| Functie | Beschrijving |
|---------|-------------|
| `platformSetHostname(hostname)` | Stel de WiFi-hostnaam in voor DHCP |
| `platformGetHostname()` | Lees de huidige hostnaam terug |
| `platformRestartDHCP()` | Forceer een DHCP-heraankondiging (bijv. na hostnaamwijziging) |
| `platformFreeHeap()` | Vrij heap in bytes |
| `platformMaxFreeBlock()` | Grootste aaneengesloten vrij blok (fragmentatie-indicator) |
| `platformRestart()` | Platformveilige herstart |
| `platformCoreVersion()` | Arduino-core versiestring |
| `platformChipId()` | Uniek chip-ID |
| `platformGetMacAddress(mac)` | Vul een 6-byte buffer met het MAC-adres |

Op ESP8266 is `MDNS_NEEDS_UPDATE` gedefinieerd als `1`: de hoofdlus moet dan `MDNS.update()` aanroepen. Op ESP32 is dat niet nodig. De hoofdlus bewaakt dit met `#if defined(MDNS_NEEDS_UPDATE)`.

De klasse `PlatformDir` in `platform.h` biedt een uniforme interface voor directoryiteratie over LittleFS. Gebruik `PlatformDir` in plaats van filesystem-API's direct aan te roepen.

---

### 8.6 Een nieuw REST-eindpunt toevoegen

De REST API v2 gebruikt een gecentraliseerde dispatchtabel (ADR-050) in `restAPI.ino`. Een eindpunt toevoegen vereist:

1. Schrijf de handler-functie.
2. Voeg een vermelding toe aan `kV2Routes[]`.
3. Geen wijzigingen aan de router.

#### Stap 1: Schrijf de handler

Handler-handtekening:

```cpp
void handleMijnDing(const char words[][API_WORD_LEN], uint8_t wc,
                    HTTPMethod method, const char* originalURI)
```

Parameters:
- `words`: URI-tokens na `/api/v2/`, bijv. `words[0]` = `"mijnding"`, `words[1]` = `"status"`.
- `wc`: Aantal tokens.
- `method`: `HTTP_GET`, `HTTP_POST`, etc.
- `originalURI`: De volledige originele URI-string.

Voorbeeldhandler voor `GET /api/v2/mijnding/status`:

```cpp
void handleMijnDing(const char words[][API_WORD_LEN], uint8_t wc,
                    HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) {
    httpServer.send(405, F("application/json"),
                    F("{\"error\":{\"status\":405,\"message\":\"Method Not Allowed\"}}"));
    return;
  }
  if (!checkHttpAuth()) return;

  char buf[CMSG_SIZE];
  snprintf_P(buf, sizeof(buf),
    PSTR("{\"mijnwaarde\":%d,\"status\":\"ok\"}"),
    mijnGlobaleWaarde);
  httpServer.send(200, F("application/json"), buf);
}
```

Regels:
- Gebruik nooit `Serial.print()` voor debug. Gebruik `DebugTln()` of `DebugTf()`.
- Gebruik `CMSG_SIZE` (512 bytes) als standaard responsbuffer, tenzij je een groter antwoord nodig hebt.
- Voor antwoorden groter dan `CMSG_SIZE`, gebruik `httpServer.sendContent()` in een chunked loop met `httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN)`.
- Alle mutatie-eindpunten (POST, PUT, DELETE) moeten `checkHttpAuth()` aanroepen en direct terugkeren als het `false` retourneert.
- Gebruik `sendApiError(httpCode, F("bericht"))` voor consistente JSON-foutantwoorden.
- Bestandsserving moet `httpServer.streamFile()` gebruiken voor bestanden groter dan 2 KB.

#### Stap 2: Registreer in de dispatchtabel

Zoek `kV2Routes[]` in `restAPI.ino`. Voeg een vermelding toe:

```cpp
{ PSTR("mijnding"), handleMijnDing },
```

Het eerste veld is de PROGMEM-resourcenaam die overeenkomt met het eerste URI-token na `/api/v2/`. Het tweede is de handler function pointer. Volgorde in de tabel heeft geen invloed op routering.

Dat is alles. De router in `processAPI()` matcht `words[0]` tegen de tabel en dispatcht automatisch.

---

### 8.7 Een nieuw MQTT-topic toevoegen

#### Publiceren

Alle MQTT-publicaties verlopen via `sendMQTTData()` in `MQTTstuff.ino`:

```cpp
void sendMQTTData(const char* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const __FlashStringHelper* value,
                  const bool retain = false);
```

De functie voegt automatisch de geconfigureerde topic-namespace (`{TopTopic}/value/{UniqueId}/`) toe. De functie controleert ook `canPublishMQTT()` voordat er wordt verstuurd.

Voorbeeld: publiceer een nieuwe float-waarde:

```cpp
if (canPublishMQTT()) {
  char payload[16];
  dtostrf(mijnFloatWaarde, 0, 2, payload);
  sendMQTTData(F("mijnnieuwetopic"), payload, true);  // retain=true voor sensorwaarden
}
```

Voor brongescheiden topics (wanneer `settings.mqtt.bSeparateSources` is ingeschakeld), gebruik `publishToSourceTopic()`:

```cpp
publishToSourceTopic(label, valueStr, sourceFlag);  // sourceFlag: 'T', 'B', of 'A' (Answer)
```

Publiceer nooit grote buffers op de stack. Gebruik `static` of de gedeelde scratch-buffer `mqttAutoCfgScratch` (alleen in de MQTT-module, bewaakt door een `inUse`-vlag). Voor periodiek gepubliceerde waarden, roep `sendMQTTData()` aan binnen een `DUE()`-controle zodat de broker niet wordt overspoeld.

#### Abonneren en verwerken

De firmware abonneert op `{TopTopic}/set/{UniqueId}/#` bij MQTT-verbinding. Inkomende berichten worden gedispatcht in `mqttCallback()` in `MQTTstuff.ino`. De dispatchtabel is `setcmds[]`, een array van `MQTT_set_cmd_t`-structs (ADR-078):

```cpp
const MQTT_set_cmd_t setcmds[] PROGMEM = {
  { PSTR("setpoint"),    PSTR("TT"), PSTR("temperature") },
  { PSTR("mijncommando"), PSTR("XX"), PSTR("raw")        },  // nieuw
  // ...
};
```

Velden:
- `setcmd`: Het MQTT-topicsuffix (wat na `{UniqueId}/` komt).
- `otgwcmd`: De tweeletterige OTGW PIC-commandocode die naar `addCommandToQueue()` wordt gestuurd.
- `ottype`: Waardetype: `"temperature"`, `"on/off"`, `"level"`, `"raw"`, of `"int"`.

Voor commando's die niet naar een PIC-commando mappen (bijv. SAT-specifieke topics), voeg een aparte handler toe in `mqttCallback()` voor de tabeldispatch.

#### Home Assistant auto-discovery

HA discovery-payloads worden tijdens runtime opgebouwd uit compile-time PROGMEM-tabellen in `MQTTHaDiscovery.cpp`. De streaming-discovery-emitter in `MQTTstuff.ino` (`doAutoConfigure()` / `doAutoConfigureMsgid()`) doorloopt die tabellen en streamt elk discovery-bericht rechtstreeks naar de broker zonder een volledige payload in RAM te bufferen. `MQTTHaDiscovery.cpp` is de source of truth; het oudere `mqttha.cfg`-template is gearchiveerd naar `docs/archive/`.

Discovery is **just-in-time** (ADR-100): de OT-specifieke configs worden pas gepubliceerd zodra de bijbehorende MsgID daadwerkelijk op de bus is gezien, met een broker-restart-heuristiek die het opnieuw publiceren bij reconnect afhandelt. Topics volgen de **flat per-value** vorm (ADR-101): één MQTT-topic per waarde, nooit aggregaten in JSON. ADR-106 bepaalt de standaard topicnamen (zelfbeschrijvend); de oude geaggregeerde vorm blijft bereikbaar via de `bUseLegacyOtTopics`-schakelaar.

Om een nieuwe discoverable entity toe te voegen, plaats een extra vermelding in de passende PROGMEM-array (sensors, binary sensors, climate, number) in `MQTTHaDiscovery.cpp`, met bestaande vermeldingen als voorbeeld. Velden die per entity varieren (device class, unit, state class, icon, entity category) zijn als enum-waarden gecodeerd om flash-gebruik te beperken. Bouw de firmware opnieuw en forceer discovery (toets `F` op de telnet-debugconsole) om de nieuwe entity te publiceren.

---

### 8.8 Een nieuwe instelling toevoegen

Instellingen volgen het patroon uit ADR-051: twee niveaus, Hongaarse prefixen.

**Stap 1: Voeg het veld toe aan de juiste sectiestruct.** Voor bestaande subsystemen woont de struct in de bijbehorende `<Component>types.h` (SAT → `SATtypes.h`, OTDirect → `OTDirecttypes.h`, HA device-identiteit → `Devicetypes.h`, MQTT → in de aggregate `OTGWSettings` in `OTGW-firmware.h`):

```cpp
struct MQTTSettingsSection {
  // ... bestaande velden ...
  bool   bMijnOptie  = false;   // b = bool
  char   sMijnString[41] = "";  // s = string/char[]
  int    iMijnGetal  = 0;       // i = int
  float  fMijnFloat  = 1.0f;    // f = float
};
```

Voor een nieuw subsysteem: maak een eigen `<Component>types.h`, include die vanuit `OTGW-firmware.h` en voeg de sectie toe aan `OTGWSettings` / `OTGWState`. Toegang overal in de firmware: `settings.mqtt.bMijnOptie`.

**Stap 2: Voeg serialisatie toe in `writeSettings()` (`settingStuff.ino`)**:

```cpp
writeJsonBoolKV(f, PSTR("mijnoptie"), settings.mqtt.bMijnOptie);
writeJsonStringKV(f, PSTR("mijnstring"), settings.mqtt.sMijnString);
```

**Stap 3: Voeg parsering toe in `updateSetting()` (`settingStuff.ino`)**:

```cpp
} else if (strcasecmp_P(field, PSTR("mijnoptie")) == 0) {
  settings.mqtt.bMijnOptie = EVALBOOLEAN(newValue);
  settingsDirty = true;
} else if (strcasecmp_P(field, PSTR("mijnstring")) == 0) {
  strlcpy(settings.mqtt.sMijnString, newValue, sizeof(settings.mqtt.sMijnString));
  settingsDirty = true;
```

Wanneer een instelling een service-herstart vereist (bijv. MQTT opnieuw verbinden):

```cpp
pendingSideEffects |= SIDE_EFFECT_MQTT;  // of SIDE_EFFECT_NTP, SIDE_EFFECT_MDNS
```

De `settingsDirty`-vlag triggert een uitgestelde LittleFS-schrijfoperatie (doorgaans na een 2-seconden debounce timer). `writeSettings()` slaat de daadwerkelijke flashbewerking over als de geserialiseerde inhoud gelijk is aan de versie op schijf, zodat een no-op `updateSetting()`-rondreis de flash niet slijt.

**Stap 4: Stel beschikbaar via REST API (optioneel)**

`GET /api/v2/settings` retourneert alle instellingen. Het antwoord wordt opgebouwd in `sendDeviceSettings()`. Voeg het nieuwe veld toe aan de JSON-uitvoer daar. Voor `POST /api/v2/settings` itereert de bestaande `postSettings()`-functie over de JSON-body en roept `updateSetting()` aan voor elk sleutel-waardepaar, dus nieuwe instellingen worden automatisch geaccepteerd zonder codewijzigingen in de POST-handler.

**Stap 5: Web UI-veld (optioneel)**

Instellingenvelden bevinden zich in `data/index.js`. Voeg een vermelding toe aan het instellingenformulier-configuratieobject volgens het bestaande patroon. Voor een boolean toggle:

```javascript
{ key: 'mijnoptie', label: 'Mijn Nieuwe Functie', type: 'checkbox', section: 'device' }
```

Voor een tekstveld:

```javascript
{ key: 'mijnstring', label: 'Mijn Stringwaarde', type: 'text', section: 'device', maxlength: 40 }
```

---

### 8.9 Codeerstijl

| Element | Conventie | Voorbeeld |
|---------|-----------|-----------|
| Variabelen | camelCase | `settingHostname`, `lastReset` |
| Constanten / defines | UPPER\_CASE | `CMSG_SIZE`, `ON`, `OFF` |
| Functies | camelCase | `startWiFi()`, `readSettings()` |
| Globale instellingen | `setting`-prefix | `settingMqttBroker` (verouderd; nieuwe code gebruikt `settings.mqtt.sBroker`) |
| Struct-velden | Hongaars prefix + camelCase | `bEnabled`, `sHostname`, `fTargetTemp`, `iInterval` |
| Enum-klassen | PascalCase | `OTGWHardwareMode`, `HeapHealthLevel` |
| PROGMEM-strings | Inline `F()` of `PSTR()` | `F("tekst")`, `PSTR("format %d")` |

#### Veiligheidsregels

- Schrijf nooit naar `Serial` na de bootsequentie. De UART is de PIC serieel link. Gebruik `DebugTln(F("..."))` in plaats daarvan.
- Stuur nooit OTGW-commando's rechtstreeks naar de serieel poort. Gebruik altijd `addCommandToQueue(cmd, len)`.
- Valideer altijd buffergroottes voor stringoperaties. Gebruik `strlcpy()`, `snprintf_P()`, nooit `strcpy()` of `sprintf()`.
- Bewak `#ifdef ESP8266` / `#ifdef ESP32`-blokken waar platformspecifieke hardware betrokken is, inclusief debug-macrosecties die platformspecifieke I/O gebruiken.
- Gebruik `enum class` voor interne discriminatoren, geen string tokens.

Leidend principe: schrijf code die een andere ontwikkelaar na zes maanden nog direct kan begrijpen. Slim is minder waard dan duidelijk.

---

### 8.10 Debugtools

#### Telnet (poort 23)

Alle debuguitvoer gaat naar de Telnet-server, nooit naar `Serial.print()` (de seriële poort is exclusief voor PIC-communicatie na initialisatie). De Telnet-server is geïmplementeerd met de SimpleTelnet-bibliotheek (`SimpleTelnet<1> debugTelnet` op poort 23, gedeclareerd in `OTGW-firmware.h` en gedefinieerd in `networkStuff.ino`). SimpleTelnet vervangt de oudere TelnetStream- en ESPTelnet-bibliotheken.

Verbinden:
```bash
telnet <ip-adres> 23
# of via netcat:
nc <ip-adres> 23
```

Debugmacro's (gedefinieerd in `Debug.h`):

| Macro | Gebruik |
|-------|---------|
| `DebugTln(tekst)` | Regel met newline, ondersteunt `F()` |
| `DebugT(tekst)` | Zonder newline |
| `DebugTf(PSTR("fmt"), ...)` | Printf-stijl met PROGMEM-formaatstring |
| `DebugFlush()` | Doorsturen van buffers |

Bij verbinding toont de firmware een banner met de actuele status en alle beschikbare debugtoetsen. Druk op `h` om het menu opnieuw te tonen. De huidige toetsen:

| Toets | Actie |
|-------|-------|
| `1` | Zet OT-berichtparsering aan/uit (`state.debug.bOTmsg`) |
| `2` | Zet REST API-handler­log aan/uit (`state.debug.bRestAPI`) |
| `3` | Zet MQTT-module­log aan/uit (`state.debug.bMQTT`) |
| `4` | Zet sensor­module­log aan/uit (`state.debug.bSensors`) |
| `5` | Zet SAT-regellus­log aan/uit (`state.debug.bSAT`) |
| `6` | Zet OTDirect-framelog aan/uit (`state.debug.bOTDirect`, ESP32) |
| `g` | Zet MQTT interval-gating­log aan/uit (`state.debug.bMQTTGate`) |
| `d` | Zet Dallas-sensorsimulatie aan/uit (`state.debug.bSensorSim`) |
| `s` / `S` | Zet OTGW serieel-simulatie-replay aan/uit |
| `r` | Herstel WiFi, Telnet, OTGW-stream en MQTT |
| `F` | Forceer MQTT HA-discovery voor alle bericht-ID's |
| `q` | Herlees instellingen uit LittleFS |
| `p` | Reset PIC handmatig |
| `h` | Toon het volledige help-menu |

De macro's sturen uitvoer naar de `debugTelnet`-instantie (een `SimpleTelnet<1>` op poort 23). Er wordt nooit naar `Serial` geschreven. Modulespecifieke conditionele macro's volgen een per-modulevlag in `state.debug`:

| Vlag | Modulemacro's | Beschrijving |
|------|--------------|-------------|
| `state.debug.bOTmsg` | `OTDebugTln()`, `OTDebugTf()` | OpenTherm-berichtparsering |
| `state.debug.bRestAPI` | `RESTDebugTln()`, `RESTDebugTf()` | REST API-handlerlogging |
| `state.debug.bMQTT` | `MQTTDebugTln()`, `MQTTDebugTf()` | MQTT-client en publish/subscribe |
| `state.debug.bMQTTGate` | (inline checks) | MQTT interval-gatingbeslissingen |
| `state.debug.bSensors` | `SensorDebugTln()`, `SensorDebugTf()` | Dallas, S0, sensorpolling |
| `state.debug.bSAT` | `SATDebugTln()`, `SATDebugTf()` | SAT-regellus, cycli, verwarmingscurve |
| `state.debug.bOTDirect` | `OTDDebugTln()`, `OTDDebugTf()` | OTDirect GPIO frame handling (ESP32) |

#### Browser Console Debug

Wanneer `window.otgwDebug = true` is ingesteld in de browserconsole voor het laden van de pagina, logt de JavaScript-SPA uitgebreide diagnostiek. Het SAT-subsysteem heeft een aparte vlag: `window.SAT_DEBUG = true`.

#### evaluate.py

Na het aanbrengen van wijzigingen, voer altijd uit:

```bash
python evaluate.py
```

Dit controleert op de patronen die het meest waarschijnlijk crashes veroorzaken op ESP8266. Verhelp alle waarschuwingen voordat je een pull request indient.

---

### 8.11 Timerbeheer

De firmware gebruikt een coöperatief timer-systeem uit `safeTimers.h`. Gebruik nooit `delay()` of `millis()`-vergelijkingen direct.

#### Timers declareren

```cpp
// In een functie of als globale variabele:
DECLARE_TIMER_SEC(timerMijnTaak, 30, SKIP_MISSED_TICKS);  // elke 30 seconden
DECLARE_TIMER_MS(timerSnel, 250, CATCH_UP_MISSED_TICKS);   // elke 250 ms
DECLARE_TIMER_MIN(timerUur, 60, SKIP_MISSED_TICKS);        // elk uur
```

#### Timer controleren

```cpp
if (DUE(timerMijnTaak)) {
  // Wordt elke 30 seconden uitgevoerd
  voerTaakUit();
}
```

#### Timer-type kiezen

| Type | Gedrag bij gemiste ticks |
|------|------------------------|
| `SKIP_MISSED_TICKS` | Sla over; volgende tick op normaal interval |
| `CATCH_UP_MISSED_TICKS` | Haal in; vuur meerdere keren achter elkaar |
| `SKIP_MISSED_TICKS_WITH_SYNC` | Zoals SKIP maar met synchronisatie (O(1)) |

#### Timer aanpassen tijdens runtime

```cpp
CHANGE_INTERVAL_SEC(timerMijnTaak, 60, SKIP_MISSED_TICKS);
RESTART_TIMER(timerMijnTaak);
```

De `timerFlushSettings`-timer (2 seconden debounce) coalesceert meerdere instellingswijzigingen tot één flash-schrijfoperatie. Dit beschermt de flash tegen slijtage.

---

### 8.12 Commandowachtrij

Stuur **nooit** rechtstreeks naar de PIC-seriële poort. Gebruik altijd `addCommandToQueue()`. Op OTGW32 (zonder PIC) routeert dezelfde API naar de OTDirect-wachtrij, die **coalesceert op MsgID** (TASK-494): zit er al een frame voor dezelfde OT-MsgID in de wachtrij, dan wordt de bestaande entry in situ vervangen, zodat meerdere producenten die dezelfde MsgID binnen één OT-cyclus (~115 ms) uitgeven de bus niet overspoelen.

```cpp
// Signatuur:
void addCommandToQueue(const char* buf, int len,
                       const bool forceQueue = false,
                       const int16_t delay = 1000);   // inter-command verzendvertraging in ms

addCommandToQueue(cmd, strlen(cmd));                  // Standaard, 1000 ms vertraging
addCommandToQueue(cmd, strlen(cmd), true);            // forceQueue: omzeil deduplicatie
addCommandToQueue(cmd, strlen(cmd), false, 500);      // Aangepaste verzendvertraging (ms)
```

De wachtrij is een FIFO-buffer met retry/backoff. Hij levert commando's een voor een aan de PIC en handelt seriele fouten af zonder verlies. Op OTGW32 toont `loopOTDirect()` daarnaast een high-water-markeerdiagnostiek op de telnet-console. Voorbeeldcommando's:

```cpp
addCommandToQueue(PSTR("TT=20.50"), 8);   // Tijdelijk setpoint 20.5 graden Celsius
addCommandToQueue(PSTR("GW=1"), 4);       // Gateway-modus aan
addCommandToQueue(PSTR("CS=1"), 4);       // Control setpoint 1
```

Zie `OTGW-Core.ino` en `docs/adr/ADR-016-opentherm-command-queue.md` voor volledige semantiek.

---

### 8.13 OpenTherm-berichtverwerking

Het begrijpen van deze pipeline is essentieel voordat je OT-berichtverwerking wijzigt:

```
Ketel/Thermostaat
       |
  OpenTherm-bus (2-draads)
       |
  PIC co-processor (ESP8266) of OTDirect GPIO ISR (ESP32)
       |
  UART 9600 baud ASCII-regel (bijv. "BA000001\r")   [ESP8266]
  of gedecodeerd OT frame struct                     [ESP32]
       |
  handleOTGW() / OTGW-Core.ino
    - leest serieel byte voor byte in sRead[256] (static)
    - bij CR: dispatcht naar processOT()
       |
  processOT(buf, len) / OTGW-Core.ino
    - parset frame type prefix (T=thermostaat, B=ketel, R=response, A=answer, E=error)
    - extraheert 8-hex-digit datawoord
    - decodeert message ID (bits 23-16)
    - decodeert message type (bits 31-28)
    - extraheert getypeerde waarde via OTValueType (f8.8, s16, u16, flag8, status, datetime, ...)
    - werkt OTcurrentSystemState bij (het globale OTdataStruct)
    - controleert mqttlastsent[msgId] throttle timestamp
    - roept sendMQTTData(label, value) aan als throttle toestaat
    - roept sendEventToWebSocket(frame) aan voor elk frame
    - roept publishToSourceTopic() aan bij ingeschakelde bronscheiding
       |
  sendMQTTData(topic, value) / MQTTstuff.ino
    - controleert canPublishMQTT()
    - voegt {TopTopic}/value/{UniqueId}/ namespace toe
    - roept PubSubClient.publish() aan
       |
  MQTT-broker
       |
  Home Assistant / automatiseringsclients
```

De `sRead[256]`-buffer in `handleOTGW()` is een `static` lokale variabele, wat het re-entrancy-veilig maakt. De functie kan worden aangeroepen vanuit zowel de hoofdlus als vanuit de yield-punten van MQTT autoconfig's bestandsleesfunctie.

---

### 8.14 Architecture Decision Records (ADRs)

ADRs documenteren belangrijke architectuurbeslissingen en bevinden zich in `docs/adr/`. Er zijn momenteel meer dan 100 ADRs.

#### Wanneer een ADR raadplegen

Raadpleeg relevante ADRs voordat je wijzigingen aanbrengt aan:
- Architectuur of componentgrenzen
- API-contracten (MQTT, REST, WebSocket)
- Nieuwe afhankelijkheden (bibliotheken)
- Build- of CI-tooling

Voor puur refactoren, bugfixes en kleine functies binnen bestaande patronen is geen ADR vereist.

#### Sleutel-ADRs

| ADR | Onderwerp | Impact |
|-----|-----------|--------|
| ADR-003 | Alleen HTTP/WS, geen HTTPS/WSS | Plain HTTP en WebSocket; beveiligingsmodel is vertrouwd LAN |
| ADR-004 | Static buffer allocation | Geen `String` in hot paths; alle buffers `char[]` |
| ADR-009 | PROGMEM string literals | Alle literals in flash via `F()` / `PSTR()` |
| ADR-014 | Dual build system | `build.py` wrapt PlatformIO (standaard) en arduino-cli (alleen ESP8266) |
| ADR-016 | OpenTherm command queue | Altijd `addCommandToQueue()` |
| ADR-019 | REST API versioning | `/api/v2/` is huidig; v0/v1 retourneren 410 Gone |
| ADR-028 | File streaming | Bestanden >2 KB moeten `streamFile()` gebruiken, nooit laden |
| ADR-040 | MQTT source-specific topics | Bronscheiding topicstructuur |
| ADR-042 | Streaming JSON | Geen ArduinoJson; handmatige `snprintf_P`-constructie |
| ADR-049 | String-verbod op protocolpaden | Geen `String`-klasse op MQTT/REST/WebSocket hot paths |
| ADR-050 | Centralized API dispatch | Enkele `kV2Routes[]`-tabel; geen per-route routerwijzigingen |
| ADR-051 | Dual encapsulating structs | `OTGWSettings` / `OTGWState`-architectuur |
| ADR-053 | Large feature buffer allocation | Statische allocatie voor grote per-feature buffers |
| ADR-061 | ESP8266/ESP32 platform abstraction | Gebruik `platform.h`-abstracties, geen directe SDK-aanroepen |
| ADR-063 | OTGW32 hardware support | ESP32-S3 NodoShop OTGW32-board met directe GPIO OT-bus |
| ADR-064 | OTDirect operating-mode architectuur | Gateway / Monitor / Thermostat / Off modi op ESP32 |
| ADR-069 | SAT PID v3 implementatie | Huidige PID-regelwet voor Smart Autotune Thermostat |
| ADR-072 | SAT platformcompatibiliteitslaag | SAT bouwt op zowel ESP8266 als ESP32 via gedeelde abstracties |
| ADR-079 | Per-component type-headers | Elk subsysteem bezit zijn state + settings structs in `<Component>types.h` |
| ADR-081 | Types vouwen in `<Component>stuff.h` | Amendement op ADR-079: fold in de bestaande stuff-sibling waar die al bestaat |
| ADR-090 | Re-entrancy guard pattern voor gedeelde scratch buffers | `MQTTAutoConfigSessionLock` en de `inUse`-vlag op `publishToSourceTopic` |
| ADR-092 | NimBLE-Arduino voor BLE-scanner | OTGW32 SAT BLE vervangt Bluedroid door NimBLE-Arduino 2.x |
| ADR-100 | JIT HA-discovery + smart reconnect | OT-configs pas gepubliceerd als de MsgID is gezien; broker-restart-heuristiek bewaakt republish |
| ADR-101 | Flat per-value MQTT-topics | Eén topic per waarde; geen geaggregeerde JSON-payloads |
| ADR-106 | Zelfbeschrijvende MQTT-topicnamen | Nieuwe standaard topicnaamgeving; legacy vorm via `bUseLegacyOtTopics` |

#### Levenscyclus

`Proposed` → `Accepted` → `Deprecated` of `Superseded`. Een `Accepted` ADR is bindend. Om een beslissing te herzien: maak een nieuwe ADR die de oude supersedes; markeer de oude als "Superseded by ADR-XXX".

#### Nieuwe ADR aanmaken

Maak een nieuwe ADR aan wanneer je wijziging: een nieuwe afhankelijkheid introduceert, een API-contract wijzigt, het bouwsysteem verandert, of architectuurgrenzen verschuift. Gebruik het formaat:

```
# ADR-NNN-Korte-Titel
## Status: Proposed
## Context
## Decision
## Consequences
## Related
```

Om een Accepted ADR terug te draaien: maak een nieuwe ADR die deze superseded en markeer de oude als "Superseded by ADR-NNN".

---

### 8.15 Bijdragenworkflow

1. Fork de repository op GitHub.
2. Maak een feature branch aan vanaf `dev`: `git checkout -b feature/mijn-functie dev`.
3. Zoek een bestaande backlog-taak: `backlog search "mijn onderwerp" --plain`. Maak er een aan als er geen is.
4. Zet de taak op in progress en wijs aan jezelf toe: `backlog task edit <id> -s "In Progress" -a @jouwnaam`.
5. Schrijf een implementatieplan in de taak voordat je code schrijft.
6. Implementeer de wijziging. Markeer acceptatiecriteria als gereed terwijl je vordert.
7. Voer `python evaluate.py` uit en los alle gerapporteerde problemen op.
8. Bouw voor beide targets en bevestig dat er geen compileerfouten zijn: `python build.py` (of rechtstreeks `pio run`).
9. Test op echte hardware indien mogelijk. Als er geen hardware beschikbaar is, gebruik dan de OTGW seriele-simulatie-replay (toets `s` / `S` op de telnet-debugconsole, aangestuurd door `state.debug.bOTGWSimulation`) of de Dallas-sensorsimulatie (toets `d`).
10. Schrijf een final summary in de backlog-taak.
11. Open een pull request naar de `dev`-branch, niet `main`.
12. Vermeld het backlog-taak-ID in de PR-beschrijving.

Pull requests naar `main` worden niet direct geaccepteerd. Alle merges naar `main` verlopen via `dev` en worden gecoordineerd door de maintainer.
