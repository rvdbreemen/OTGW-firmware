## Hoofdstuk 8: Ontwikkelaarsgids

Dit hoofdstuk is bedoeld voor ontwikkelaars die willen bijdragen aan de OTGW-firmware, nieuwe functionaliteit willen toevoegen, of de interne werking willen begrijpen. Kennis van Arduino C/C++ en het ESP8266/ESP32-platform wordt verondersteld.

---

### 8.1 Projectindeling

De broncode bevindt zich in `src/OTGW-firmware/`. Elk `.ino`-bestand vormt een logische module; de Arduino IDE en PlatformIO behandelen alle `.ino`-bestanden in dezelfde map als één vertaaleenheid (single translation unit). Declaratievolgorde telt dus: `OTGW-firmware.h` wordt als eerste verwerkt en bevat alle globale structs en `extern`-verklaringen.

| Bestand | Beschrijving |
|---------|-------------|
| `OTGW-firmware.ino` | Hoofdbestand: `setup()`, `loop()`, `doBackgroundTasks()` |
| `OTGW-firmware.h` | Globale types, structs (`OTGWSettings`, `OTGWState`), constanten, macro's |
| `OTGW-Core.ino` | OpenTherm-protocolverwerking, PIC-seriële communicatie, commandowachtrij, firmware-upgrade |
| `OTDirect.ino` | ESP32-only: directe GPIO OpenTherm zonder PIC (OTGW32-hardware) |
| `MQTTstuff.ino` | MQTT-client, publiceer/abonneer, Home Assistant auto-discovery |
| `restAPI.ino` | REST API v2, dispatchtabel `kV2Routes[]`, JSON-helpers |
| `jsonStuff.ino` | JSON-hulpfuncties: `sendStartJsonMap()`, `sendJsonMapEntry()`, `sendEndJsonMap()` |
| `networkStuff.ino` | WiFi, OTA, mDNS, LLMNR, NTP, Telnet-debugserver |
| `Ethernet.ino` | W5500 Ethernet-ondersteuning (ESP32 + HAS\_ETH\_CAPABLE) |
| `settingStuff.ino` | Instellingen laden/opslaan via LittleFS, validatie, side-effects |
| `FSexplorer.ino` | Bestandsbeheer via web, LittleFS-bestandsserver |
| `webSocketStuff.ino` | WebSocket-server (poort 81), realtime OpenTherm-logstroom |
| `SAT*.ino` | Smart Autotune Thermostat: SATcontrol, SATpid, SATcycles, SATpressure, SATweather, SATble |
| `sensors_ext.ino` | Dallas DS18B20 temperatuursensoren, S0-pulsteller |
| `OLED.ino` | SSD1306 OLED-display (optioneel) |
| `outputs_ext.ino` | GPIO-relaisuitgang |
| `s0PulseCount.ino` | S0-pulsteller voor energiemeting |
| `webhook.ino` | HTTP-webhook bij boilerstatuswijziging |
| `helperStuff.ino` | Heap-monitoring, crashlog, diverse hulpfuncties |
| `handleDebug.ino` | Telnet-debuginvoer en -afhandeling |
| `versionStuff.ino` | Versiebeheer, buildinfo |
| `version.h` | `_VERSION`, `_VERSION_MAJOR/MINOR/PATCH` |
| `safeTimers.h` | Timer-macro's: `DECLARE_TIMER_SEC()`, `DUE()`, etc. |
| `Debug.h` | Debugmacro's: `DebugTln()`, `DebugTf()`, `DebugT()` |
| `boards.h` | Boardspecifieke pinnummers en compilatievlaggen |
| `platform.h` / `platform_esp8266.h` / `platform_esp32.h` | Platformabstractielaag |

De testbare bestanden van de bibliotheek bevinden zich in `src/libraries/`. Dit zijn lokale kopieën; gebruik nooit externe packagemanagers om ze bij te werken zonder de projectspecifieke patches van `scripts/patch_pio_libs.py` te controleren.

---

### 8.2 Bouwsysteem

#### build.py (aanbevolen voor releases)

Het `build.py`-script bovenin de repository bouwt firmware én filesystem en verzorgt versienummering:

```bash
python build.py              # Bouw firmware + filesystem (LittleFS-image)
python build.py --firmware   # Alleen firmware, geen filesystem
python build.py --clean      # Verwijder build-artefacten en bouw opnieuw
```

Het script roept intern PlatformIO aan en plaatst de uitvoer in `build/`.

#### PlatformIO (dagelijks gebruik)

```bash
# Bouw voor ESP8266 (standaard Nodoshop OTGW)
pio run -e esp8266

# Bouw voor ESP32 (OTGW32)
pio run -e esp32

# Bouw alle omgevingen
pio run

# Flash naar apparaat
pio run -e esp8266 -t upload

# Filesystem uploaden (LittleFS)
pio run -e esp8266 -t uploadfs

# Seriële monitor (9600 baud)
pio device monitor
```

De twee omgevingen in `platformio.ini`:

| Omgeving | Platform | Board | Notitie |
|----------|---------|-------|---------|
| `esp8266` | `espressif8266@4.2.1` | `d1_mini` | Arduino Core 3.1.2, GCC 10.3, 160 MHz |
| `esp32` | pioarduino fork espressif32 | `esp32dev` | Arduino-ESP32 v3.3.5, Ethernet/BLE |

Bibliotheken worden gedefinieerd in `[env]`-sectie van `platformio.ini` en lokaal geplaatst in `src/libraries/`. De PlatformIO-packagemanager wordt gebruikt voor downloadbare bibliotheken; het script `scripts/patch_pio_libs.py` past bij de build automatisch noodzakelijke patches toe.

#### Arduino IDE (ESP8266 only)

Open `src/OTGW-firmware/OTGW-firmware.ino` in de Arduino IDE. Stel in:
- Board: "LOLIN(Wemos) D1 mini"
- Flash size: "4MB (FS: 2MB OTA: ~1019KB)"
- CPU frequency: 160 MHz

Zorg dat alle bibliotheken uit `src/libraries/` zijn geïnstalleerd.

#### Kwaliteitscontrole

```bash
python evaluate.py           # Controleer PROGMEM-gebruik, onveilige patronen
python evaluate.py --quick   # Snelle scan (subset van checks)
```

---

### 8.3 C4-architectuuroverzicht

De architectuur is gedocumenteerd in vier niveaus (C4-model), te vinden in `docs/c4/`:

| Niveau | Bestand | Inhoud |
|--------|---------|--------|
| Context | `c4-context.md` | Systeem, gebruikers, externe afhankelijkheden |
| Container | `c4-container.md` | ESP8266/ESP32-firmware, PIC, LittleFS, externe diensten |
| Component | `c4-component.md` | Index van alle componentdocumenten |
| Code | `c4-code-*.md` | Functielijsten per bronbestand met regelnummers |

**Verplichte lesvolgorde** bij de start van elke sessie:
1. `c4-context.md` — begrijp het systeem en zijn gebruikers
2. `c4-component.md` — vind de juiste component voor de code die je aanraakt
3. Relevante `c4-code-*.md` — exacte functiehandtekeningen en regelnummers

Wanneer je code aanraakt zonder eerst de C4-documentatie te raadplegen, werk je met aannames in plaats van feiten. Dat is de kortste weg naar een bug.

---

### 8.4 Sleutelpatronen

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

#### Geen re-entrancy

De watchdog-functie `feedWatchDog()` roept geen `yield()` aan (uitgecommentarieerd). `handleOTGW()` gebruikt `static`-lokale buffers als beveiliging tegen re-entrancy vanuit `doBackgroundTasks()`.

---

### 8.5 Een nieuw REST-eindpunt toevoegen

Voeg een route toe in drie stappen. Geen wijzigingen in de router zelf nodig.

**Stap 1: Voeg een route-constante toe** (bovenin `restAPI.ino`):

```cpp
static const char kRouteMijnDing[] PROGMEM = "mijnding";
```

**Stap 2: Schrijf de handler-functie**:

```cpp
static void handleMijnDing(const char words[][API_WORD_LEN], uint8_t wc,
                            HTTPMethod method, const char* originalURI) {
  if (method == HTTP_GET) {
    // Stuur JSON-antwoord via chunked transfer
    sendStartJsonMap(nullptr);         // begin JSON-object
    sendJsonMapEntry(F("status"), F("ok"), true);
    // true = laatste veld (geen komma achteraan)
    sendEndJsonMap();
  } else {
    sendApiError(405, F("Method Not Allowed"));
  }
}
```

Handtekening is altijd identiek aan het voorbeeld hierboven. `words[]` bevat de URI-tokens na `/api/v2/`; `wc` is het aantal tokens.

**Stap 3: Voeg een vermelding toe in `kV2Routes[]`**:

```cpp
static const ApiRoute kV2Routes[] = {
  { kRouteHealth,     handleHealth },
  // ... bestaande routes ...
  { kRouteMijnDing,   handleMijnDing },   // nieuw
  { nullptr,          nullptr }           // sentinel blijft altijd als laatste
};
```

Het eindpunt is nu bereikbaar op `GET /api/v2/mijnding`.

**Authenticatie** voor schrijfoperaties:

```cpp
if (!checkHttpAuth()) return;   // Stuur automatisch 401; return daarna direct
```

**Foutafhandeling**:

```cpp
sendApiError(400, F("Ongeldige parameter"));  // JSON: {"error":{"status":400,...}}
```

---

### 8.6 Een nieuw MQTT-onderwerp toevoegen

#### Publiceren

Gebruik `sendMQTTData()` voor enkelvoudige publicaties:

```cpp
// Publiceer naar {TopTopic}/value/{UniqueId}/mijnwaarde
sendMQTTData(F("mijnwaarde"), F("42"), true);  // true = retained
```

Voor OpenTherm-gerelateerde waarden die worden gestuurd vanuit `OTGW-Core.ino`, gebruik `publishToSourceTopic()`:

```cpp
// Publiceert naar boiler/thermostat/gateway subtopic afhankelijk van berichtrichting
publishToSourceTopic("roomtemperature", "20.5", OT_MSGTYPE_READ_ACK);
```

Publiceer nooit grote buffers op de stack. Gebruik `static` of de gedeelde scratch-buffer `mqttAutoCfgScratch` (alleen in de MQTT-module, bewaakt door een `inUse`-vlag).

#### Abonneren en verwerken

De firmware abonneert op `{TopTopic}/set/{UniqueId}/#`. Inkomende berichten worden afgehandeld in `handleMQTTcallback()` in `MQTTstuff.ino`. Voeg een nieuwe `MQTT_set_cmd_t`-vermelding toe aan de opdrachttabel (rond regel 540 in `MQTTstuff.ino`):

```cpp
static const MQTT_set_cmd_t MQTTsetCmds[] PROGMEM = {
  // ...
  { PSTR("mijncommando"), PSTR("XX"), PSTR("temperature") },
  // ...
};
```

Of behandel het expliciet in de callback voor niet-standaard logica.

#### Home Assistant auto-discovery

Voeg een configuratieregel toe aan `data/mqttha.cfg`. Dit bestand wordt gelezen door `doAutoConfigure()` en genereert discovery-berichten voor Home Assistant. Raadpleeg de bestaande vermeldingen in dat bestand voor de opmaak.

---

### 8.7 Een nieuwe instelling toevoegen

Instellingen volgen het patroon uit ADR-051: twee niveaus, Hongaarse prefixen.

**Stap 1: Voeg het veld toe aan de juiste struct in `OTGW-firmware.h`**:

```cpp
struct MQTTSettingsSection {
  // ... bestaande velden ...
  bool   bMijnOptie  = false;   // b = bool
  char   sMijnString[41] = "";  // s = string/char[]
  int    iMijnGetal  = 0;       // i = int
  float  fMijnFloat  = 1.0f;    // f = float
};
```

Toegang overal in de firmware: `settings.mqtt.bMijnOptie`.

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
pendingSideEffects |= SIDE_EFFECT_MQTT;
```

**Stap 4: Voeg het UI-veld toe** in `data/index.html` (of `index.js`) als het zichtbaar moet zijn in de web-interface. Gebruik de bestaande invulvelden als sjabloon.

---

### 8.8 Codeerstijl

| Element | Conventie | Voorbeeld |
|---------|-----------|-----------|
| Variabelen | camelCase | `settingHostname`, `lastReset` |
| Constanten / defines | UPPER\_CASE | `CMSG_SIZE`, `ON`, `OFF` |
| Functies | camelCase | `startWiFi()`, `readSettings()` |
| Globale instellingen | `setting`-prefix | `settingMqttBroker` (verouderd; nieuwe code gebruikt `settings.mqtt.sBroker`) |
| Struct-velden | Hongaars prefix + camelCase | `bEnabled`, `sHostname`, `fTargetTemp`, `iInterval` |
| Enum-klassen | PascalCase | `OTGWHardwareMode`, `HeapHealthLevel` |
| PROGMEM-strings | Inline `F()` of `PSTR()` | `F("tekst")`, `PSTR("format %d")` |

Leidend principe: schrijf code die een andere ontwikkelaar na zes maanden nog direct kan begrijpen. Slim is minder waard dan duidelijk.

---

### 8.9 Debugtools

#### Telnet (poort 23)

Alle debuguitvoer gaat naar de Telnet-server, nooit naar `Serial.print()` (de seriële poort is exclusief voor PIC-communicatie na initialisatie).

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

Schakel extra debuguitvoer in via `settings.bMyDEBUG = true` of de REST API:
```
POST /api/v2/settings  {"mydebug": "true"}
```

#### evaluate.py

Statische analyse van de broncode:

```bash
python evaluate.py           # Volledig rapport: PROGMEM-gebruik, onveilige patronen
python evaluate.py --quick   # Snelle scan
```

Het script waarschuwt onder andere voor:
- Gebruik van `Serial.print()` na de initialisatiefase
- Ontbrekende `F()` of `PSTR()` bij letterlijke tekenreeksen
- `String`-klasse in hot paths
- `strncmp_P`/`strstr_P` op binaire data

---

### 8.10 Timerbeheer

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

### 8.11 Commandowachtrij

Stuur **nooit** rechtstreeks naar de PIC-seriële poort. Gebruik altijd:

```cpp
addCommandToQueue(cmd, strlen(cmd));           // Standaard, 1000 ms vertraging
addCommandToQueue(cmd, strlen(cmd), true);     // Verplicht (force = true)
addCommandToQueue(cmd, strlen(cmd), false, 500); // Aangepaste vertraging (ms)
```

De wachtrij is een FIFO-buffer met retry/backoff. Hij levert commando's één voor één aan de PIC en handelt seriële fouten af zonder verlies. Voorbeeldcommando's:

```cpp
addCommandToQueue(PSTR("TT=20.50"), 8);   // Tijdelijk setpoint 20.5 °C
addCommandToQueue(PSTR("GW=1"), 4);       // Gateway-modus aan
addCommandToQueue(PSTR("CS=1"), 4);       // Control setpoint 1
```

Zie `OTGW-Core.ino` en `docs/adr/ADR-016-opentherm-command-queue.md` voor volledige semantiek.

---

### 8.12 OpenTherm-berichtverwerking

De gegevensstroom van PIC naar MQTT verloopt als volgt:

```
PIC UART (9600 baud)
      |
      v
handlePICSerial()            -- niet-blokkerend, leest regels uit serieële buffer
      |
      v
handleOTGW()                 -- verwerkt complete regel (T/B/R/A prefix)
      |
      v
processOT(line, msgType)     -- decodeert OT-berichttype en -waarde
      |
      +---> OTcurrentSystemState.*   -- bijwerken van de globale toestandsstructuur
      |
      +---> sendMQTTData()           -- publiceer naar MQTT (met throttle)
      |
      +---> sendEventToWebSocket()   -- stuur naar WebSocket-clients
```

Berichtprefixen van de PIC:
- `T` — bericht van thermostaat naar boiler (WRITE_DATA)
- `B` — antwoord van boiler (WRITE_ACK of READ_ACK)
- `R` — verzoek ingespoten door gateway
- `A` — antwoord van gateway

Het globale object `OTcurrentSystemState` (type `OTdataStruct`) is de canonieke bron van alle gedecodeerde OpenTherm-waarden. Andere modules lezen hieruit; schrijven gaat alleen via `processOT()`.

---

### 8.13 Architecture Decision Records (ADRs)

ADRs documenteren belangrijke architectuurbeslissingen en bevinden zich in `docs/adr/`. Er zijn momenteel meer dan 75 ADRs.

#### Wanneer een ADR raadplegen

Raadpleeg relevante ADRs voordat je wijzigingen aanbrengt aan:
- Architectuur of componentgrenzen
- API-contracten (MQTT, REST, WebSocket)
- Nieuwe afhankelijkheden (bibliotheken)
- Build- of CI-tooling

Voor puur refactoren, bugfixes en kleine functies binnen bestaande patronen is geen ADR vereist.

#### Sleutel-ADRs

| ADR | Titel | Samenvatting |
|-----|-------|-------------|
| ADR-003 | HTTP-only | Geen HTTPS of WSS, nooit |
| ADR-004 | Geen `String`-klasse in hot paths | Gebruik `char[]`, `strlcpy`, `snprintf_P` |
| ADR-005 | WebSocket realtime streaming | Poort 81, platte ws:// |
| ADR-009 | PROGMEM voor letterlijke tekenreeksen | `F()` en `PSTR()` verplicht |
| ADR-016 | OpenTherm-commandowachtrij | Altijd `addCommandToQueue()`, nooit direct serial |
| ADR-051 | Settings/State architectuur | `OTGWSettings` vs `OTGWState`, sub-secties, Hongaarse prefixen |
| ADR-054 | HTTP Basic Auth + CSRF | Beveiligingsmodel voor REST API |

#### Levenscyclus

`Proposed` → `Accepted` → `Deprecated` of `Superseded`. Een `Accepted` ADR is bindend. Om een beslissing te herzien: maak een nieuwe ADR die de oude supersedes; markeer de oude als "Superseded by ADR-XXX".

#### Nieuwe ADR aanmaken

```bash
# Gebruik het adr-skill commando als dat beschikbaar is, anders handmatig:
# Kopieer de ADR-README.md template, nummer opvolgend aan de laatste ADR
```

Formaat (korte versie):
```
# ADR-NNN-Korte-Titel
## Status: Proposed
## Context
## Decision
## Consequences
## Related
```

---

### 8.14 Bijdragenworkflow

```
1. Fork de repository op GitHub
2. Maak een feature branch aan vanaf 'dev':
      git checkout dev
      git pull origin dev
      git checkout -b feature/mijn-functie

3. Maak een backlog-taak aan (zie CLAUDE.md):
      backlog task create "Mijn functie" -d "Beschrijving" --ac "Criterium 1"

4. Schrijf code, volg de richtlijnen in dit hoofdstuk

5. Controleer kwaliteit:
      python evaluate.py

6. Bouw lokaal:
      pio run -e esp8266

7. Test op hardware (of simulatie indien van toepassing)

8. Commit met beschrijvende commit-boodschap:
      git commit -m "feat: voeg mijn-functie toe voor ..."

9. Open een Pull Request naar de 'dev' branch
   - Beschrijf wat je hebt gedaan en waarom
   - Voeg teststappen toe
   - Verwijs naar de backlog-taak

10. Wacht op review; de 'dev' branch wordt door de maintainer samengevoegd
```

Branches worden **nooit** rechtstreeks samengevoegd naar `main` zonder expliciete goedkeuring van de maintainer.
