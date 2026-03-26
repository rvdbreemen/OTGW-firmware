---
# METADATA
Document Title: Code Review — v1.2.0 → v1.3.0-beta (dev branch HEAD)
Review Date: 2026-03-20 05:08:11 UTC
Branch Reviewed: v1.2.0 (tag) → origin/dev (HEAD 369e706)
Target Version: v1.3.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: PRIORITIZED REVIEW REPORT
PR Branch: copilot/review-codewijzigingen-sinds-laatste-release
Commit: 369e706
Status: COMPLETE
---

# Code Review: v1.2.0 → v1.3.0-beta

## Baseline

**Last release:** `v1.2.0` (tag `v1.2.0`, commit `594171e`, also tagged `v1.2.0-production`)  
**Review HEAD:** `origin/dev` — commit `369e706`  
**Scope:** 105 real commits (excl. CI `version.h` bumps), 114 files changed, ~14 000 insertions / ~3 700 deletions

---

## Samenvatting / Executive Summary

De `dev`-branch bevat een omvangrijke maar grotendeels goed-onderbouwde voorbereiding voor v1.3.0. De wijzigingen omvatten drie grote thema's:

1. **ADR-051 dual-struct refactor** — alle globale variabelen zijn gegroepeerd in `OTGWState state` en `OTGWSettings settings`. Dit verbetert de onderhoudbaarheid en testbaarheid significant.
2. **MQTT publish-interval en throttling** — nieuw per-ID intervalgate met change-detection, conform ADR-052.
3. **OTA/LittleFS hardening** — fix voor drie serieuze regressen in de OTA-flow uit v1.2.0.

Tegelijk zijn er een aantal problemen die aandacht vereisen vóór een stabiele release. De meest kritieke zijn gedocumenteerd hieronder.

**Eindoordeel:** Niet klaar voor productierelease in de huidige staat. De geïdentificeerde kritieke issues moeten opgelost zijn. De architecturale richting (ADR-051, ADR-052, ADR-050) is positief en correct.

---

## 1. Kritieke Issues

### K1 — `state` locale shadowing in `shouldForceWifiConfigPortal()`

**Bestand:** `src/OTGW-firmware/OTGW-firmware.ino:78`  
**Ernst:** Kritiek (latente correctheidsbug)

Functie `shouldForceWifiConfigPortal()` declareert een locale variabele `WifiPortalResetState state = { ... }`. Dit **shadowed** de globale `OTGWState state` zonder compilerwaarschuwing op ESP8266 toolchain. Elke verwijzing naar `state.resetCount` in die functie slaat correct op de locale struct, maar een toekomstige ontwikkelaar die `state.otgw.bOnline` wil lezen loopt stil mis. De globale naam `state` is te generiek voor een embedded codebase — dit is een onderhoudsrisico.

**Aanbeveling:** Hernoem de lokale variabele naar `portalState` of `rtcState`. Overweeg ook de globale struct te hernoemen naar `otgwState` om conflicten te vermijden.

```cpp
// Huidig (verwarrend):
WifiPortalResetState state = { WIFI_PORTAL_RESET_MAGIC, 0 };

// Voorstel:
WifiPortalResetState portalState = { WIFI_PORTAL_RESET_MAGIC, 0 };
```

---

### K2 — `TRACKED_TIME_UNSEEN == TRACKED_TIME_MODULUS` (rand-conditie MQTT throttle)

**Bestand:** `src/OTGW-firmware/OTGW-Core.ino:219-220`  
**Ernst:** Kritiek (subtiele logicabug)

```cpp
static constexpr uint16_t TRACKED_TIME_UNSEEN = 0xFFFFU;   // = 65535
static constexpr uint32_t TRACKED_TIME_MODULUS = 65535UL;  // = 65535
```

`currentTrackedSeconds()` retourneert `(millis() / 1000) % 65535`. Op exact 65535 seconden na boot (≈18,2 uur) retourneert de functie `0`. Op dat moment geldt voor een slot waarvan de `lastTime == 0xFFFF`:

```
elapsedTrackedSeconds(0, 0xFFFF) = (65535 − 65535) + 0 = 0
```

Dit levert `elapsed = 0`, wat kleiner is dan elk ingesteld interval > 0. Het slot wordt **nooit** als "first seen" herkend (want `hasTrackedTime(0xFFFF) == false` → `firstSeen == true` → directe publicatie), dus in de praktijk publiceert het toch. Maar de `resetMqttTrackedState()`-functie zet `mqttlastsent[i] = TRACKED_TIME_UNSEEN` terwijl de gewone slot-update `setPackedSlot` de waarde `0xFFFF` als timestamp kan opslaan bij een modulus-overloop, waarna `hasTrackedTime(0xFFFF) == false` ten onrechte `firstSeen = true` triggert na een reset.

**Aanbeveling:** Gebruik een schildwacht-waarde die **nooit** door `currentTrackedSeconds()` kan worden geproduceerd, bijv. `0xFFFE`:

```cpp
static constexpr uint16_t TRACKED_TIME_UNSEEN  = 0xFFFEU; // sentinel: nooit geldig
static constexpr uint32_t TRACKED_TIME_MODULUS = 65535UL; // modulus = 0xFFFF
```

Of gebruik modulus `65536` (power-of-two, simpelere bits):

```cpp
static constexpr uint32_t TRACKED_TIME_MODULUS = 65536UL;
static constexpr uint16_t TRACKED_TIME_UNSEEN  = 0xFFFFU; // 0xFFFF < 65535 nooit bereikt
```

---

### K3 — `readLatestCrashLog()` roept `LittleFS.begin()` aan terwijl LittleFS al gemount is

**Bestand:** `src/OTGW-firmware/helperStuff.ino:298`  
**Ernst:** Hoog (mogelijke datastoring in edge case)

```cpp
if (!LittleFS.begin()) {
    return false;
}
```

`LittleFS.begin()` op een al-gemounte filesystem is op ESP8266 normaal een no-op. Maar de functie controleert de `LittleFSmounted` flag niet. Zodra `readLatestCrashLog()` aangeroepen wordt tijdens of na een OTA-operatie (waarbij `LittleFS.end()` opgeroepen is) én `LittleFS.begin()` slaagt maar de firmware daarna direct reboot, wordt LittleFS onverwacht in een interne state gelaten. Bovendien wordt LittleFS niet opnieuw gesloten na gebruik.

**Aanbeveling:**

```cpp
bool readLatestCrashLog(char* summary, size_t summarySize, char* details, size_t detailsSize) {
  if (!LittleFSmounted) return false;  // gebruik bestaande flag
  // ... rest van functie
}
```

---

### K4 — `writeJsonStringKV()` gebruikt globale `cMsg` als escape-buffer

**Bestand:** `src/OTGW-firmware/settingStuff.ino`  
**Ernst:** Hoog (re-entrantie-gevaar)

De comment zegt: _"writeSettings() holds no yield() between calls, so cMsg cannot be clobbered mid-write."_ Dit is een gefragile garantie die breekt zodra:
- Een interrupt-handler of timer-callback `sendMQTTData()` aanroept (dat ook `cMsg` gebruikt),
- Of iemand in de toekomst een `yield()` toevoegt tussen schrijfstappen.

**Aanbeveling:** Gebruik een lokale buffer in `writeJsonStringKV`:

```cpp
static void writeJsonStringKV(File& file, const __FlashStringHelper* key, const char* value, bool withComma) {
  char escapeBuf[sizeof(cMsg)];  // lokale stack, geen gedeelde staat
  escapeJsonStringTo(value, escapeBuf, sizeof(escapeBuf));
  file.printf_P(PSTR("  \"%S\": \"%s\"%s\n"), reinterpret_cast<PGM_P>(key), escapeBuf, withComma ? "," : "");
}
```

`cMsg` is 150 bytes; een lokale kopie past prima op de stack.

---

### K5 — `expandedPayload[384]` op de CONT-stack in `sendWebhookPost()`

**Bestand:** `src/OTGW-firmware/webhook.ino`  
**Ernst:** Hoog (stack overflow risico)

```cpp
static int sendWebhookPost(HTTPClient& http, const char* url, bool stateOn) {
  char expandedPayload[384];  // 384 bytes op de 4 KB CONT-stack
```

De ESP8266 heeft een CONT-stack van 4 KB. Gecombineerd met de HTTPClient-call-chain kan dit de stack satureren. De vorige implementatie gebruikte een `static` buffer (wat re-entrantie-hazard had), maar 384 bytes op de stack is ook riskant.

**Aanbeveling:** Gebruik een statische buffer met een reentrancy-guard (de webhook state machine is al single-threaded):

```cpp
static char expandedPayload[384];
static bool inWebhookPost = false;
if (inWebhookPost) { DebugTln(F("Webhook: re-entry blocked")); return -1; }
inWebhookPost = true;
// ... gebruik expandedPayload ...
inWebhookPost = false;
```

---

## 2. Belangrijke Issues

### I1 — Lazy `new` voor MQTT autoconfig-buffers (ADR-004 schending)

**Bestand:** `src/OTGW-firmware/MQTTstuff.ino:70-81`  
**Ernst:** Gemiddeld

```cpp
pMqttAutoConfigBuffers = new MQTTAutoConfigBuffers();
```

ADR-004 stelt dat dynamische heap-allocatie verboden is op kritieke paden. De comment zegt "acceptable for embedded", maar geeft geen motivatie waarom statische allocatie hier niet kan. Bij OOM retourneert `new` `nullptr` en wordt autodiscovery stil uitgeschakeld — zonder melding aan de gebruiker.

**Aanbeveling:** Maak de buffer statisch maar conditioneel gecompileerd (of gebruik lazy-init met een statische pool):

```cpp
static MQTTAutoConfigBuffers sMqttAutoConfigBuffers;
static bool sMqttAutoConfigBuffersUsed = false;
static MQTTAutoConfigBuffers* getMqttAutoConfigBuffers() {
  sMqttAutoConfigBuffersUsed = true;
  return &sMqttAutoConfigBuffers;
}
```

---

### I2 — `handleCommandSubmit()` valideert geen alfanumeriek command-prefix

**Bestand:** `src/OTGW-firmware/restAPI.ino:100-113`  
**Ernst:** Gemiddeld

```cpp
if ((cmdLen < 3) || (cmdStr[2] != '=')) {
    sendApiError(400, F("Invalid command format (expected XX=value)"));
    return;
}
```

Er wordt niet gecontroleerd of `cmdStr[0]` en `cmdStr[1]` geldig OTGW-commando-letters zijn (uppercase A-Z). Een aanvaller op het lokale netwerk kan `\x00\x00=` insturen, dat door `strlcpy` als lege string behandeld wordt maar `addOTWGcmdtoqueue` bereikt.

**Aanbeveling:**

```cpp
if (!isalpha((unsigned char)cmdStr[0]) || !isalpha((unsigned char)cmdStr[1]) || cmdStr[2] != '=') {
    sendApiError(400, F("Invalid command format (expected XX=value)"));
    return;
}
```

---

### I3 — `OTGWState state` en `OTGWSettings settings` **gedefinieerd** in een header

**Bestand:** `src/OTGW-firmware/OTGW-firmware.h:186,278`  
**Ernst:** Gemiddeld (architectureel risico)

```cpp
OTGWState state;      // regel 186 in .h
OTGWSettings settings; // regel 278 in .h
```

In Arduino worden alle `.ino`-bestanden aaneengevoegd tot één `.cpp`-bestand; `OTGW-firmware.h` wordt dus maar eenmalig meegecompileerd. Dit werkt correct voor de huidige build. Maar:
- Als ooit een pure `.cpp` library-bestand het header includeert, krijgt men multiple-definition linker errors.
- Het pattern staat haaks op de aanbeveling van ADR-044 (extern declarations in .h, definities in .ino).

**Aanbeveling:** Verplaats de definities naar `OTGW-firmware.ino` of een dedicated `globals.ino`. Voeg `extern` declarations toe in de header:

```cpp
// OTGW-firmware.h
extern OTGWState state;
extern OTGWSettings settings;

// globals.ino (of OTGW-firmware.ino)
OTGWState state;
OTGWSettings settings;
```

---

### I4 — `DECLARE_TIMER_SEC` met `settings.sensors.iInterval` op globaal niveau

**Bestand:** `src/OTGW-firmware/OTGW-firmware.ino:42-43`  
**Ernst:** Gemiddeld

```cpp
DECLARE_TIMER_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
DECLARE_TIMER_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);
```

`DECLARE_TIMER_SEC` evalueert het interval-argument op het moment van declaratie (static initialisatie). Op dat moment zijn de settings nog niet van LittleFS geladen — `settings.sensors.iInterval` heeft de default-waarde (20). Als de gebruiker een ander interval configureert, wordt de timer **niet** herinitialiseerd na `readSettings()`. Dit is een regressie t.o.v. v1.2.0 waar dit hetzelfde gedrag had, maar door de struct-refactor is het nu explicieter zichtbaar.

**Aanbeveling:** Zorg dat timers opnieuw geïnitialiseerd worden na `readSettings()`, of gebruik de timer-macro's pas ná `readSettings()` in `setup()`.

---

### I5 — Webhook timeout van 3000ms naar 1000ms — false negatives op traag LAN

**Bestand:** `src/OTGW-firmware/webhook.ino:203`  
**Ernst:** Laag-gemiddeld

```cpp
http.setTimeout(1000); // 1-second timeout (was 3s; local LAN should respond <500ms)
```

ADR-048 motiveert de 1-second timeout met "local LAN should respond <500ms". In de praktijk kan een overbelaste Home Assistant instantie of een switch met ARP-miss langer dan 1 seconde nodig hebben voor de eerste response. Dit leidt tot stille webhook-mislukkingen.

**Aanbeveling:** Maak de timeout configureerbaar (bijv. via `settings.webhook.iTimeoutMs`) met default 2000ms. Dit vermijdt silent failures zonder de main loop te lang te blokkeren.

---

### I6 — `mqttlastsent[256]` initialisatie via statische constructor vóór `setup()`

**Bestand:** `src/OTGW-firmware/OTGW-Core.ino:336-350`  
**Ernst:** Laag

```cpp
struct TrackingStateInitializer {
  TrackingStateInitializer() {
    clearMsgLastUpdated();
    resetMqttTrackedState();
  }
};
static TrackingStateInitializer gTrackingStateInitializer;
```

Statische constructors worden uitgevoerd vóór `setup()`. Op ESP8266 is `millis()` dan al geldig, maar de `DebugTln` calls in de initialisatie werken nog niet (TelnetStream is niet gestart). In de huidige implementatie zijn er geen debug calls in de constructors, maar dit is een subtiel contract dat toekomstige code kan schenden.

**Aanbeveling:** Overweeg een expliciet `initTrackingState()` functie die vanuit `setup()` aangeroepen wordt.

---

## 3. Bugs gefixed (positief)

De volgende bugs uit v1.2.0 zijn correct opgelost in v1.3.0-beta:

| # | Bug | Commit | Bestand |
|---|-----|--------|---------|
| B1 | `isValidIP()` verwierp geldige adressen met een 255-octet (bijv. `10.255.0.1`) | `e9c8dca` | `helperStuff.ino:141-161` |
| B2 | OTA: LittleFS corruptie bij WiFi-reconnect tijdens flash | `ab692e1` | `OTGW-ModUpdateServer-impl.h` |
| B3 | OTA POST-handler checkte `Update.hasError()` maar niet `_updaterError` na `begin()`-failure | `56f19aa` | `OTGW-ModUpdateServer-impl.h:97` |
| B4 | `updateRebootCount()` logde `rebootCount` (global, waarde 0) i.p.v. `_reboot` (local, werkelijke waarde) | `helperStuff.ino` | `helperStuff.ino:208` |
| B5 | `checkGPIOConflict()` gebruikte `strcasecmp_P` met interne PGM_P discriminator (ADR-patroon overtreding) | `settingStuff.ino` | `settingStuff.ino:60` |
| B6 | `expandPayload()` detecteerde template-truncatie niet; payload kon stil afgebroken worden | `webhook.ino` | `webhook.ino:109` |

---

## 4. Refactoring-advies

### R1 — `shouldForceWifiConfigPortal()` → hernoem lokale `state`

**Prioriteit:** Hoog (K1 hierboven)  
Hernoem `WifiPortalResetState state` naar `WifiPortalResetState portalState` in `OTGW-firmware.ino`.

### R2 — Verplaats `OTGWState state` en `OTGWSettings settings` uit de header

**Prioriteit:** Hoog (I3 hierboven)  
Verplaats definities naar een `.ino`-bestand, bewaar `extern`-declaraties in de header. Dit is conform ADR-044.

### R3 — `TRACKED_TIME_UNSEEN` / `TRACKED_TIME_MODULUS` sentinel-conflict

**Prioriteit:** Hoog (K2 hierboven)  
Kies een sentinel-waarde die nooit door `currentTrackedSeconds()` geproduceerd wordt.

### R4 — `writeJsonStringKV()` — verwijder `cMsg`-afhankelijkheid

**Prioriteit:** Gemiddeld (K4 hierboven)  
Gebruik een lokale buffer. Elimineert een verborgen shared-state afhankelijkheid.

### R5 — `parseJsonKVLine()` in `settingStuff.ino` — ontbrekende `\uXXXX` escape-handling

**Bestand:** `src/OTGW-firmware/settingStuff.ino`  
**Prioriteit:** Laag

De nieuwe JSON-parser in `parseJsonKVLine()` verwerkt `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t` correct, maar heeft geen fallback voor `\uXXXX` unicode-escapes. Een settings-bestand met een Unicode-escape (bijv. gegenereerd door een externe tool) parset stil verkeerd.

**Aanbeveling:** Voeg een `\u`-case toe die de 4 hex-digits overslaat en een vervangende `?` schrijft, of reject het veld met een waarschuwing.

### R6 — `OTGW-Core.ino` section map — houd synchroniciteit met actuele regelgetallen

**Bestand:** `src/OTGW-firmware/OTGW-Core.ino:8-38`  
**Prioriteit:** Laag

De sectie-map bovenaan het bestand bevat regelgetallen die snel verouderd raken bij codewijzigingen. Overweeg de sectie-headers (`//===================[ ... ]===================`) als het enige navigatiemechanisme te gebruiken, zonder regelgetallen.

---

## 5. Architectuur-observaties

### A1 — ADR-051 dual-struct refactor: correct uitgevoerd

De migratie van ~60 losse globals naar `OTGWState state` en `OTGWSettings settings` is consistent doorgevoerd in alle bestanden. De struct-definitions in `OTGW-firmware.h` bieden duidelijke sub-sections met comments. Dit verbetert de leesbaarheid en testbaarheid significant. Aandachtspunt: definitie in header (I3).

### A2 — ADR-050 route-dispatch table: sterke verbetering

De migratie van één monolithische `processAPI()` switch naar afzonderlijke `handle*()` functies per resource is correct. Elke handler is maximaal ~25 regels; toevoegen van een endpoint vereist nu slechts één handler + één tabelregel. Dit is een significante onderhoudsverbetering.

### A3 — ADR-052 MQTT publish eligibility: correct en volledig

De `shouldPublishMQTTForID()` + `shouldPublishMQTTForPSField()` implementatie met packed uint32 throttle-slots is doordacht. Change-detection op waardebasis voorkomt onnodige MQTT-traffic. De `OTPublishGate` RAII-wrapper voor `mqttPublishAllowed` is correct en voorkomt gate-leakage.

### A4 — networkStuff.h → networkStuff.ino: conform ADR-044

Het verplaatsen van functie-bodies uit `networkStuff.h` naar `networkStuff.ino` is architectureel correct. De header bevat nu alleen declaraties/prototypes; dit voorkomt mogelijke ODR-issues.

### A5 — WiFiManager upgrade v2.0.17

De upgrade van RC naar stable is positief. De nieuwe WiFi-portal via triple-reset (ADR-043) is elegant geïmplementeerd met RTC user memory.

### A6 — OTGW simulation mode

De simulatiemodus (replay van `/otgw_simulation.log`) is een waardevolle development/test feature. De implementatie via `state.debug.bOTGWSimulation` is correct. Aanbeveling: zorg dat simulatiemodus bij productie-flash standaard `false` is (dit is al het geval door de default in de struct).

---

## 6. Security-analyse

| Item | Bevinding | Ernst |
|------|-----------|-------|
| REST API command validation | `handleCommandSubmit()` valideert commandoformat maar niet alfanumeriek prefix (I2) | Laag |
| Webhook URL policy | `isLocalUrl()` wordt gecontroleerd vóór HTTP-aanroep; extern verkeer wordt geblokkeerd | OK |
| OTA authenticatie | `_authenticated` flag correct gecontroleerd; `_updaterError` nu ook gecheckt | OK |
| Settings JSON parsing | `parseJsonKVLine()` correct afgeschermd tegen overflows | OK |
| CORS headers | `sendApiOptions()` stuurt `Access-Control-Allow-Origin: *` — acceptabel voor lokaal netwerk | OK (lokaal) |
| Crash log leakage | `readLatestCrashLog()` exposeert interne crash-info via REST API — kan gevoel van foutinformatie zijn, maar dit is bewust gedrag voor diagnostiek | Laag |

---

## 7. Performance-analyse

| Item | Bevinding |
|------|-----------|
| MQTT autodiscovery buffering | Lazy `new` (I1) vermijdt 1200+200-byte permanente RAM-reservering; bij OOM stille failure |
| MQTT throttle | `mqttlastsent[256]` = 1 KB; `mqttlastsentstatusbit[16]` = 32 bytes; totaal ~1,1 KB extra voor throttle state — acceptabel |
| Webhook payload | 384-byte stack-allocatie in `sendWebhookPost()` (K5) — risico bij diepe call-stack |
| FSexplorer streaming | Statische buffers vrijgegeven en chunked streaming gebruikt — verbeterd t.o.v. v1.2.0 |
| LittleFS read in `updateRebootCount()` | Nu met `readBytesUntil` i.p.v. `readStringUntil` — elimineert heap-allocatie |

---

## 8. Testdekking-advies

Er zijn geen geautomatiseerde tests aanwezig (Arduino/ESP8266 leent zich slecht voor unit tests in CI). Aanbevelingen voor handmatige regressietests vóór v1.3.0-release:

1. **MQTT throttling:** Zet `settings.mqtt.iInterval = 30`. Verifieer dat identieke OT-waarden niet vaker dan elke 30 seconden gepubliceerd worden. Verifieer dat gewijzigde waarden direct gepubliceerd worden.
2. **MQTT eerste publicatie:** Start device op, controleer dat alle kanalen direct gepubliceerd worden (firstSeen=true).
3. **MQTT reconnect republish:** Verbreek MQTT-verbinding, herstel. Controleer dat status-topics opnieuw gepubliceerd worden.
4. **OTA firmware flash:** Flash firmware, controleer dat device herstart en LittleFS intact is.
5. **OTA filesystem flash:** Flash filesystem, controleer dat settings bewaard blijven.
6. **OTA annulering:** Start flash, verbreek verbinding midden in. Controleer dat `state.flash.bESPactive = false` gereset wordt en device responsive blijft.
7. **isValidIP regressietest:** Verifieer dat `10.255.0.1`, `192.168.1.255` en `172.16.255.254` als geldig geaccepteerd worden; `255.255.255.255`, `0.0.0.0` als ongeldig.
8. **Triple-reset WiFi portal:** Reset device 3× snel achter elkaar; verifieer dat WiFi config-portal opent.
9. **GPIO conflict:** Configureer dezelfde GPIO voor sensor en S0; verifieer waarschuwing in telnet output.
10. **Webhook truncatie:** Configureer een payload-template die > 384 bytes expandeert; verifieer log-waarschuwing over truncatie.

---

## 9. Technische schuld

| Item | Locatie | Grootte |
|------|---------|---------|
| `cMsg` shared-buffer anti-pattern | Meerdere bestanden | Groot |
| Timers geïnitialiseerd met default-settings voor `readSettings()` | `OTGW-firmware.ino:42-43` | Gemiddeld |
| Sectie-map met hardcoded regelgetallen | `OTGW-Core.ino:8-38` | Klein |
| `parseJsonKVLine()` mist `\uXXXX` support | `settingStuff.ino` | Klein |
| `readLatestCrashLog()` controleert `LittleFSmounted` niet | `helperStuff.ino:298` | Klein |
| Commit `commits.txt` en `plan.md` committed in repository root | repository root | Klein |

---

## 10. Eindoordeel

**Status: NIET KLAAR voor stable release — v1.3.0-beta is correct gelabeld.**

De codebase heeft duidelijk vooruitgang geboekt op architectuurvlak (ADR-051, ADR-052, ADR-050), heeft drie serieuze OTA-bugs van v1.2.0 gerepareerd, en heeft correcte MQTT-throttling geïmplementeerd. Echter:

- **K1, K2, K3, K4, K5** zijn bugs/risico's die in productie kunnen manifesteren.
- **I1, I2, I3** zijn architecturele issues die de stabiliteit en onderhoudbaarheid raken.

**Minimale vereisten vóór stable release:**
1. Fix K1 (state-shadowing)
2. Fix K2 (TRACKED_TIME_UNSEEN sentinel)
3. Fix K3 (LittleFS.begin check)
4. Fix K4 (cMsg reentrancy in writeJsonStringKV)
5. Fix K5 (webhook stack allocation)
6. Fix I2 (command prefix validatie)
7. Fix I3 (globals in header)

Alle overige issues zijn verbetersuggesties die in een volgende sprint opgepakt kunnen worden.
