## Hoofdstuk 9: API-referentie

Dit hoofdstuk beschrijft alle externe interfaces van de OTGW-firmware: de REST API v2, de WebSocket-interface, de MQTT-interface en het Telnet-debugprotocol.

---

### 9.1 REST API -- Overzicht

#### Basis-URL

```
http://<hostname>/api/v2/
```

Vervang `<hostname>` door het IP-adres of de mDNS-naam van het apparaat (standaard: `OTGW.local`).

#### Versioning

| Versie | Pad | Status |
|--------|-----|--------|
| v2 | `/api/v2/` | Huidig, aanbevolen |
| v1 | `/api/v1/` | Verouderd; retourneert 410 Gone |
| v0 | `/api/v0/` | Verouderd; retourneert 410 Gone |

#### Berichtformaat

Alle verzoeken en antwoorden gebruiken `application/json`. Aanvraagtekst bij POST/PUT mag zowel JSON als een enkelvoudige waarde als platte tekst zijn (afhankelijk van het eindpunt).

#### Foutformaat

```json
{
  "error": {
    "status": 400,
    "message": "Omschrijving van de fout"
  }
}
```

Veelvoorkomende foutcodes:

| Code | Betekenis |
|------|-----------|
| 400 | Bad Request: ongeldige JSON of ontbrekend veld |
| 401 | Unauthorized: authenticatie vereist |
| 403 | Forbidden: CSRF-controle mislukt |
| 404 | Not Found: onbekend eindpunt of bron |
| 405 | Method Not Allowed |
| 410 | Gone: verouderde API-versie (v0, v1) |
| 413 | Request Entity Too Large: commando te lang |
| 414 | URI Too Long |
| 500 | Internal Server Error |
| 503 | Service Unavailable: weinig geheugen, geen OT-interface, enz. |

#### Authenticatie

Alle POST- en PUT-verzoeken vereisen HTTP Basic Auth wanneer een HTTP-wachtwoord is ingesteld. GET-verzoeken vereisen authenticatie alleen voor gevoelige gegevens (instellingen, PIC-instellingen). CORS `OPTIONS`-verzoeken worden altijd toegestaan (RFC 7231). Dezelfde-oorsprong-validatie via `Origin`/`Referer`-headers beschermt tegen CSRF-aanvallen (ADR-054).

---

### 9.2 REST API -- Eindpunten

#### GET /api/v2/health

Systeemgezondheid opvragen. Geen authenticatie vereist.

**Neveneffect:** Elke aanroep schrijft een klein testbestand (`/.health`) naar LittleFS om te controleren of het bestandssysteem beschrijfbaar is. Poll met een interval van minimaal 30-60 seconden.

**Antwoord:**
```json
{
  "health": {
    "status": "UP",
    "uptime": "2d 3h 45m",
    "heap": 25600,
    "networkmode": "wifi",
    "wifirssi": -65,
    "mqttconnected": "true",
    "otgwconnected": "true",
    "picavailable": "true",
    "littlefsMounted": "true"
  }
}
```

| Veld | Type | Beschrijving |
|------|------|-------------|
| `status` | string | `"UP"` of `"DEGRADED"` |
| `uptime` | string | Uptime in leesbaar formaat |
| `heap` | int | Vrij heap-geheugen in bytes |
| `networkmode` | string | `"wifi"`, `"ethernet"` of `"ap"` |
| `wifirssi` | int | WiFi-signaalsterkte in dBm (0 in AP- of Ethernet-modus) |
| `mqttconnected` | string | MQTT-verbinding actief |
| `otgwconnected` | string | PIC of OTDirect actief |
| `picavailable` | string | PIC aanwezig en reagerend |
| `littlefsMounted` | string | LittleFS bestandssysteem gezond |

---

#### GET /api/v2/settings

Huidige instellingen opvragen. Authenticatie vereist.

**Antwoord (fragment):**
```json
{
  "settings": {
    "hostname": "OTGW",
    "mqttbroker": "192.168.1.10",
    "mqttport": "1883",
    "mqtttoptopic": "OTGW",
    "mqttuniqueid": "otgw-AABBCCDDEEFF",
    "ntptimezone": "Europe/Amsterdam",
    "ssid": "MijnWifiNetwerk",
    "nightlyrestart": false,
    "nightlyrestarthour": 4
  }
}
```

Het veld `ssid` is alleen-lezen: het geeft het momenteel verbonden WiFi-netwerk terug. Bij gebruik van Ethernet retourneert `/api/v2/device/info` de waarde `"Wired"`.

**Nachtelijke herstart:** Het veld `nightlyrestart` (boolean) schakelt een dagelijkse geplande herstart in. Het veld `nightlyrestarthour` (0-23) bepaalt het uur waarop de herstart plaatsvindt.

---

#### POST /api/v2/settings

Instellingen bijwerken. Authenticatie vereist.

**Verzoektekst:**
```json
{
  "name": "mqttbroker",
  "value": "192.168.1.20"
}
```

De firmware valideert elk veld afzonderlijk en coalesceert flash-schrijfbewerkingen (2-seconden debounce). Serviceherstarts (MQTT, NTP, mDNS) worden uitgesteld tot na het wegschrijven.

**Antwoord bij succes:**
```json
{"status": "ok", "saved": true}
```

**Settings whitelist:** Alleen bekende instellingsnamen worden geaccepteerd. De volledige lijst omvat onder andere: `hostname`, `httppasswd`, `ledblink`, `nightlyrestart`, `nightlyrestarthour`, `mqttenable`, `mqttbroker`, `mqttbrokerport`, `mqttuser`, `mqttpasswd`, `mqtttoptopic`, `mqtthaprefix`, `mqttuniqueid`, `mqttotmessage`, `mqttinterval`, `mqttseparatesources`, `ntpenable`, `ntptimezone`, `ntphostname`, `webhookenable`, `webhookurlon`, `webhookurloff`, `webhooktriggerbit`, `webhookpayload`, `webhookcontenttype`, `satenabled`, `satsystem`, `sattargettemp`, `satcoefficient`, `satdeadband`, `satinterval`, en nog veel meer SAT-gerelateerde instellingen.

---

#### POST /api/v2/otgw/commands

Een OTGW-commando naar de PIC of OTDirect sturen. Authenticatie vereist.

**Verzoektekst (JSON):**
```json
{"command": "TT=21.0"}
```

**Verzoektekst (platte tekst):**
```
TT=21.0
```

Het commando wordt gevalideerd en in de commandowachtrij geplaatst.

**Antwoord bij succes (HTTP 202 Accepted):**
```json
{"status": "queued"}
```

**Antwoord wanneer geen OT-interface beschikbaar (HTTP 503):**
```json
{"error": {"status": 503, "message": "No OT command interface detected - commands disabled"}}
```

Veelgebruikte commando's:

| Commando | Beschrijving |
|----------|-------------|
| `TT=21.0` | Tijdelijk kamersetpoint 21,0 C |
| `TC=20.0` | Constant kamersetpoint |
| `GW=1` | Gateway-modus inschakelen |
| `GW=0` | Monitor-modus (transparant) |
| `CS=55` | Control setpoint (aanvoertemperatuur) |
| `SH=80` | Maximale CV-watertemperatuur |
| `HW=1` | Warmwater aan |
| `HW=0` | Warmwater uit |
| `HW=P` | Warmwater eenmalige puls |
| `OT=12.0` | Buitentemperatuur doorgeven |
| `SB=15.0` | Setback-temperatuur |

Alias: `POST /api/v2/otgw/command` (achterwaartse compatibiliteit)

---

#### POST /api/v2/otgw/discovery

Home Assistant auto-discovery herstarten. Authenticatie vereist. Alternatief pad: `/api/v2/otgw/autoconfigure`.

**Verzoek:** geen parameters

**Antwoord (HTTP 202 Accepted):**
```json
{"status": "accepted"}
```

Discovery gebruikt een asynchrone bitmap-gestuurde drip publisher: alle bericht-ID's worden als "pending" gemarkeerd en vervolgens een voor een vanuit de hoofdlus gepubliceerd. Zie "MQTT Discovery Drip Publisher" hieronder voor meer details.

---

#### GET /api/v2/otgw/otmonitor

Alle huidige OpenTherm-waarden opvragen in gestructureerd key-value formaat. Elk veld bevat waarde, eenheid en laatste update-tijdstempel. Compatibel met Telegraf en OTmonitor.

Alias: `GET /api/v2/otgw/telegraf`

---

#### GET /api/v2/otgw/messages/{id}

OpenTherm-berichtwaarde opvragen op bericht-ID (0-127). Alias: `/api/v2/otgw/id/{id}`.

**Voorbeeld:** `GET /api/v2/otgw/messages/25`

**Antwoord:**
```json
{
  "label": "boilertemperature",
  "value": 55.25,
  "unit": "C"
}
```

---

#### GET /api/v2/otgw/label/{label}

OpenTherm-berichtwaarde opvragen op labelnaam (niet hoofdlettergevoelig).

---

#### GET /api/v2/sensors

Alle Dallas DS18B20 temperatuursensoren en S0-pulsteller opvragen. Alias: `GET /api/v2/sensors/status`.

**Antwoord:**
```json
{
  "sensors": {
    "dallas_enabled": true,
    "dallas_detected": true,
    "dallas_count": 2,
    "dallas_gpio": 4,
    "dallas_poll_sec": 30,
    "simulated": false,
    "devices": {
      "28FF64D1841703F1": {"temp": 21.5, "epoch": 1774548600},
      "28AB12CD34EF0012": {"temp": 18.3, "epoch": 1774548600}
    },
    "s0": {
      "enabled": false,
      "gpio": 0,
      "poll_sec": 10,
      "pulses": 0,
      "total": 0,
      "power_kw": 0.000,
      "epoch": 0
    }
  }
}
```

---

#### GET /api/v2/sensors/labels

Sensorlabels opvragen.

#### POST /api/v2/sensors/labels

Sensorlabels bijwerken. Authenticatie vereist.

**Verzoektekst:**
```json
{
  "28FF64D1841703F1": "Cv-aanvoer",
  "28AB12CD34EF0012": "Woonkamer"
}
```

---

#### GET /api/v2/device/info

Apparaatinformatie opvragen (firmware, hardware, netwerk). Geen authenticatie vereist.

**Antwoord (fragment):**
```json
{
  "device": {
    "author": "Robert van den Breemen",
    "fwversion": "2.0.0",
    "hostname": "OTGW",
    "board": "esp8266",
    "hardwaremode": "pic",
    "networkmode": "wifi",
    "ipaddress": "192.168.1.100",
    "macaddress": "AA:BB:CC:DD:EE:FF",
    "ssid": "MijnWifiNetwerk",
    "picavailable": true,
    "otgwconnected": true,
    "otcommandinterface": true,
    "otdirectavailable": false
  }
}
```

Op OTGW32-hardware: het veld `otdirectavailable` is `true` en aanvullende `otd*`-velden zijn aanwezig (`otdmode`, `otdbypass`, `otdmonitor`, `otdmaster`, `otdstepup`, `otdthermostat`, `otdsetback`, `otdschedtotal`, `otdschedactive`, `otdscheddisabled`, `otdoverrides`).

Het veld `otcommandinterface` is `true` wanneer een PIC- of OT-direct-interface aanwezig en actief is.

---

#### GET /api/v2/device/time

Actuele systeemtijd opvragen (NTP-gesynchroniseerd).

**Antwoord:**
```json
{
  "devtime": {
    "dateTime": "2026-04-12T10:00:00+02:00",
    "epoch": 1712870400,
    "message": "OpenTherm Gateway",
    "psmode": false,
    "otgwsimulation": false,
    "freeheap": 25600,
    "networkmode": "wifi",
    "wifiquality": 80
  }
}
```

---

#### GET /api/v2/device/crashlog

Meest recente crash-logboek opvragen. Retourneert een leeg object als er geen crash-logboek beschikbaar is.

---

#### GET /api/v2/sat

Smart Autotune Thermostat status opvragen. Authenticatie vereist. Alias: `GET /api/v2/sat/status`.

Voeg `?detail=full` toe voor uitgebreide diagnostiek, waaronder synchronisatie-gezondheid, drukmetrieken, cyclusdiagnostiek, PID-foutstatistieken en auto-tune scores.

**Antwoord (fragment):**
```json
{
  "enabled": true,
  "active": true,
  "control_mode": 1,
  "target_temp": 21.0,
  "room_temp": 20.8,
  "outside_temp": 8.0,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "final_setpoint": 43.1,
  "boiler_status": 3,
  "flame_status": "healthy",
  "cycle_class": "good",
  "safety_tripped": false,
  "pressure": 1.5,
  "pressure_alarm": false
}
```

---

#### POST /api/v2/sat/target

SAT doeltemperatuur instellen. Authenticatie vereist.

**Verzoektekst:**
```
21.5
```
of:
```json
{"value": "21.5"}
```

Geldig bereik: 5,0 tot 30,0 graden C.

---

#### POST /api/v2/sat/externaltemp

Binnentemperatuur aanleveren vanuit een externe sensor. Authenticatie vereist. Verloopt na 5 minuten als de waarde niet wordt ververst.

**Verzoektekst:**
```
20.8
```

---

#### POST /api/v2/sat/externaloutdoor

Buitentemperatuur aanleveren vanuit een externe bron. Overschrijft de OT MsgID 27-waarde en de Open-Meteo-weerbron. Verloopt na 10 minuten.

---

#### Overige SAT-eindpunten

| Methode | Pad | Beschrijving |
|---------|-----|-------------|
| POST | `/api/v2/sat/humidity` | Luchtvochtigheid (0-100%) |
| POST | `/api/v2/sat/preset` | Voorinstelling activeren (`comfort`, `eco`, `away`, `sleep`, `activity`, `home`) |
| POST | `/api/v2/sat/enable` | SAT in-/uitschakelen (`true`/`false` of `1`/`0`) |
| POST | `/api/v2/sat/mode` | Bedrijfsmodus instellen (`continuous`, `pwm`, `auto`) |
| POST | `/api/v2/sat/window` | Raamopendetectie (`open`/`closed`, `1`/`0`, `ON`) |
| POST | `/api/v2/sat/flush` | PID-integraal en cyclusvenster wissen |
| POST | `/api/v2/sat/reset_integral` | Alleen PID-integraal resetten |
| POST | `/api/v2/sat/area/{0-3}` | Gebiedstemperatuur aanleveren (multi-area modus) |
| GET | `/api/v2/sat/weather` | Weerdata gebruikt door de verwarmingscurve |
| POST | `/api/v2/sat/settings/{naam}` | SAT-instelling bijwerken (spiegelt MQTT `sat/*`-commando's) |

---

#### OTDirect (ESP32 / OTGW32)

Deze eindpunten zijn alleen beschikbaar op OTGW32-hardware (`HAS_DIRECT_OT`). Op standaard ESP8266+PIC retourneren alle eindpunten 503 Service Unavailable.

##### GET /api/v2/otdirect/status

Retourneert de bedrijfsmodus, schema-tellingen, thermostaatverbindingsstatus, bypass-relaisstatus en ketelverbindingsstatus.

**Antwoord:**
```json
{
  "otdirect_status": {
    "mode": "gateway",
    "bypass": false,
    "stepup": true,
    "monitor_mode": false,
    "master_mode": false,
    "thermostat_connected": true,
    "setback_active": false,
    "schedule_total": 12,
    "schedule_active": 11,
    "schedule_disabled": 1,
    "overrides_active": 2,
    "ot_online": true,
    "thermostat": true,
    "boiler": true
  }
}
```

##### POST /api/v2/otdirect/mode

Bedrijfsmodus instellen.

**Query parameter:** `mode=gateway|monitor|bypass|master|loopback`

| Modus | Beschrijving |
|-------|-------------|
| `gateway` | Volledige gateway: scheduler + thermostaat-doorschakeling + overrides (standaard) |
| `monitor` | Transparante doorvoer: alle frames ongewijzigd doorgestuurd |
| `bypass` | Thermostaat rechtstreeks op ketel via relais; OTDirect inactief |
| `master` | Zelfstandige OT-master: alleen scheduler, geen thermostaat verwacht |
| `loopback` | Interne test: gesimuleerde ketelantwoorden, geen hardware nodig |

##### GET /api/v2/otdirect/settings

Retourneert de opgeslagen OT-direct-configuratie, inclusief verwarmingscurve en PI-kamercompensatieparameters.

**Antwoord:**
```json
{
  "otdirect_settings": {
    "mode": 1,
    "setback_temp": 15.0,
    "setback_timeout": 300,
    "ch_mode": 0,
    "flow_temp": 55.0,
    "flow_max": 80.0,
    "room_setpoint": 20.0,
    "gradient": 1.5,
    "exponent": 1.3,
    "offset": 0.0,
    "room_comp": false,
    "kp": 1.0,
    "ki": 0.01,
    "kboost": 2.0
  }
}
```

##### POST /api/v2/otdirect/settings

OT-direct-instellingen bijwerken. Alleen meegegeven parameters worden gewijzigd.

##### GET /api/v2/otdirect/overrides

Retourneert alle actieve stored response overrides en response modifiers. Gebruikt chunked JSON-streaming voor potentieel grote lijsten.

##### POST /api/v2/otdirect/overrides

Overrides beheren per bericht-ID.

**Query parameters:**
- `action=sr&msgid=X&value=HHHH` : stored response instellen
- `action=cr&msgid=X` : stored response wissen
- `action=rm&msgid=X&value=HHHH` : response modifier instellen
- `action=cm&msgid=X` : response modifier wissen
- `action=ui&msgid=X` : bericht-ID markeren als onbekend
- `action=ki&msgid=X` : bericht-ID markeren als bekend

---

#### Webhook

##### POST /api/v2/webhook/test

Een test-webhook versturen. Authenticatie vereist.

**Query parameter:** `state=on|off|1|0` : te simuleren payload-status.

---

#### Simulation

| Methode | Pad | Beschrijving |
|---------|-----|-------------|
| GET | `/api/v2/simulate` | Huidige simulatiestatus |
| POST | `/api/v2/simulate/start` | Simulatiemodus inschakelen |
| POST | `/api/v2/simulate/stop` | Simulatiemodus uitschakelen |

---

#### Flash, firmware en bestandssysteem

| Methode | Pad | Beschrijving |
|---------|-----|-------------|
| GET | `/api/v2/flash/status` | Flash-status (ESP en PIC) |
| GET | `/api/v2/firmware/files` | Beschikbare firmwarebestanden |
| GET | `/api/v2/filesystem/files` | Bestandslijst op LittleFS |
| GET | `/api/v2/filesystem/hash-check` | Controleer of firmware- en bestandssysteem-hash overeenkomen |

---

#### PIC

| Methode | Pad | Beschrijving |
|---------|-----|-------------|
| GET | `/api/v2/pic/flash-status` | PIC flash-status |
| GET | `/api/v2/pic/update-check` | Controleer op PIC-firmwareupdate |
| GET | `/api/v2/pic/settings` | PIC-instellingen (via PR=-commando's) |

---

### 9.3 WebSocket-API

#### Verbinding

```
ws://<hostname>:81/
```

De WebSocket-server luistert op poort 81 (gescheiden van HTTP op poort 80). Alleen platte `ws://`, geen `wss://` (ADR-003, ADR-005). Geen authenticatie (vertrouwd LAN-model). Maximaal 3 gelijktijdige verbindingen.

#### Verbindingsstroom

```
Browser                        ESP8266
=======                        =======
                               [Luistert op poort 81]
new WebSocket("ws://host:81/")
    |--- verbindingsverzoek --->|
    |<-- verbinding geaccepteerd|
    |                           |
    |<====== berichtenstroom ===|
    |   (OpenTherm-frames, etc.)|
```

Gegevensstroom is primair van server naar client. De browser stuurt automatisch Pong-antwoorden op Ping-berichten van de server.

#### Berichtformaat

Elk bericht is een JSON-object met een `type`-veld:

```json
{"type": "otlog", "frame": "B4001FF04", "msgid": 0, "msgtype": "READ_ACK", "source": "boiler", "timestamp": 1712870400}
```

#### Berichttypen

| Type | Richting | Beschrijving |
|------|----------|-------------|
| `otlog` | Server naar Client | OpenTherm-frame van PIC of OTDirect |
| `flash` | Server naar Client | OTA-voortgang in procenten |
| `sat` | Server naar Client | SAT-statusverandering |

#### Frame-prefixen in `otlog`

| Prefix | Beschrijving |
|--------|-------------|
| `T` | Bericht van thermostaat naar boiler |
| `B` | Antwoord van boiler |
| `R` | Verzoek ingespoten door gateway |
| `A` | Antwoord van gateway |

#### Herverbindingsgedrag

De web-UI verbindt automatisch opnieuw bij verbindingsverlies (3 seconden vertraging). Er is geen berichtgeschiedenis; de stream is alleen live.

#### Heap backpressure

Bij laag vrij geheugen stopt de server met het versturen van WebSocket-berichten om het systeem te beschermen. Berichten die in deze periode worden overgeslagen, zijn definitief verloren.

---

### 9.4 MQTT-API

#### Onderwerpsstructuur

Alle MQTT-onderwerpen volgen de naamruimteconventie:

```
Publiceren:   {TopTopic}/value/{UniqueId}/{onderwerp}
Abonneren:    {TopTopic}/set/{UniqueId}/{commando}
```

Standaardwaarden:
- `{TopTopic}` = `OTGW`
- `{UniqueId}` = `otgw-{MAC-adres}` (bijv. `otgw-AABBCCDDEEFF`)

**Voorbeeld:**
```
Publicatie:   OTGW/value/otgw-AABBCCDDEEFF/boilertemperature  -> "55.25"
Abonnement:   OTGW/set/otgw-AABBCCDDEEFF/setpoint             -> "21.0"
```

#### Verbindingslevenscyclus

Bij verbinding:
1. **Geboorte-bericht**: Publiceert `"online"` naar `{TopTopic}/value/{UniqueId}` (retained)
2. **Last Will**: Bij verbreking wordt automatisch `"offline"` gepubliceerd (retained)
3. **Abonnement**: Op `{TopTopic}/set/{UniqueId}/#` voor binnenkomende commando's
4. **Abonnement**: Op `homeassistant/status` voor Home Assistant levenscyclusdetectie
5. **Herpublicatie**: Alle OT-waarden worden opnieuw gepubliceerd zodat HA de huidige status ontvangt

Let op: Discovery-status wordt niet gewist bij herverbinding. MQTT retained-berichten overleven een ESP-herverbinding op de broker. Discovery wordt alleen opnieuw geactiveerd bij (a) firmware-opstart of MQTT-instellingswijziging, en (b) Home Assistant herstart (gedetecteerd via `homeassistant/status`).

#### Gepubliceerde onderwerpen (selectie)

Tenzij anders vermeld zijn alle gepubliceerde onderwerpen retained.

**Firmwareinfo** (bij start en elke 5 minuten):

| Onderwerp | Beschrijving |
|-----------|-------------|
| `otgw-firmware/version` | Firmwareversie |
| `otgw-firmware/uptime` | Uptime in seconden (niet retained) |
| `otgw-firmware/reboot_count` | Aantal herstarts |
| `otgw-firmware/reboot_reason` | Reden laatste herstart |
| `otgw-firmware/board` | Hardware-identificatie |
| `otgw-firmware/hardware_mode` | Hardwaremodus (`pic` of `otdirect`) |
| `otgw-firmware/network_mode` | Netwerkmodus (`wifi`, `ethernet` of `ap`) |

**PIC Gateway-info**:

| Onderwerp | Waarden | Beschrijving |
|-----------|---------|-------------|
| `otgw-pic/version` | `"5.4"` | PIC-firmwareversie |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | PIC aanwezig |
| `otgw-pic/boiler_connected` | `"ON"` / `"OFF"` | Ketelverbinding |
| `otgw-pic/thermostat_connected` | `"ON"` / `"OFF"` | Thermostaatverbinding |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway-modus actief |
| `otgw-pic/otgw_connected` | `"ON"` / `"OFF"` | Seriele verbinding met PIC |

**OT Direct-info (alleen OTGW32)**:

| Onderwerp | Beschrijving |
|-----------|-------------|
| `otgw-otdirect/available` | OT-direct hardware aanwezig |
| `otgw-otdirect/mode` | Bedrijfsmodus (gateway, monitor, bypass, master, loopback) |
| `otgw-otdirect/bypass` | Bypass-relais actief |
| `otgw-otdirect/thermostat_connected` | Thermostaatframe ontvangen binnen timeout |
| `otgw-otdirect/setback_active` | Setback-override actief |
| `otgw-otdirect/schedule_active` | Aantal actieve schema-items |
| `otgw-otdirect/overrides_active` | Aantal actieve write-overrides |
| `otgw-otdirect/boiler_connected` | Ketel OT-bus actief |
| `otgw-otdirect/ot_online` | OT seriele bus actief |

Op standaard ESP8266+PIC wordt alleen `otgw-otdirect/available` gepubliceerd met waarde `"OFF"`.

**OpenTherm numerieke waarden** (bij ontvangst):

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `controlsetpoint` | C | Aanvoertemperatuur setpoint |
| `roomsetpoint` | C | Kamersetpoint |
| `roomtemperature` | C | Kamertemperatuur |
| `boilertemperature` | C | Ketelwatertemperatuur |
| `returnwatertemperature` | C | Retourwatertemperatuur |
| `dhwtemperature` | C | Warmwatertemperatuur |
| `dhwsetpoint` | C | Warmwater setpoint |
| `maxchwatersetpoint` | C | Maximale CV-watertemperatuur |
| `outsidetemperature` | C | Buitentemperatuur |
| `chwaterpressure` | bar | CV-waterdruk |
| `relmodlvl` | % | Relatief modulatieniveau |
| `maxrelmodlvl` | % | Maximaal modulatieniveau |
| `exhaust_temperature` | C | Uitlaattemperatuur |
| `boiler_fan_speed` | rpm | Ventilatortoerental |
| `electrical_current_burner_flame` | uA | Vlamsignaalstroom |
| `dhw_flowrate` | l/min | Warmwaterdebiet |

**OpenTherm statusvlaggen** (bericht-ID 0):

| Onderwerp | Waarden | Beschrijving |
|-----------|---------|-------------|
| `flamestatus` | `"ON"` / `"OFF"` | Vlam actief |
| `chmodus` | `"ON"` / `"OFF"` | CV actief |
| `dhwmode` | `"ON"` / `"OFF"` | Warmwater actief |
| `chenable` | `"ON"` / `"OFF"` | CV-vrijgave door thermostaat |
| `faultindicator` | `"ON"` / `"OFF"` | Storingsmelding |
| `diagnosticindicator` | `"ON"` / `"OFF"` | Diagnostische indicator |
| `status_master` | tekst | Volledige master-statusweergave |
| `status_slave` | tekst | Volledige slave-statusweergave |

**SAT-onderwerpen** (elke 30 s, wanneer SAT actief is):

| Onderwerp | Retained | Beschrijving |
|-----------|----------|-------------|
| `sat/mode` | ja | Bedrijfsmodus (`off`, `continuous`, `pwm`) |
| `sat/active` | ja | Controller actief |
| `sat/setpoint` | ja | Aanvoertemperatuur setpoint (C) |
| `sat/target` | ja | Gewenste kamertemperatuur (C) |
| `sat/heating_curve` | ja | Verwarmingscurve berekende waarde (C) |
| `sat/pid_output` | ja | PID-output = curve + P + I + D |
| `sat/error` | nee | PID-fout = doel - kamertemperatuur (C) |
| `sat/pid_p` | nee | Proportionele PID-term |
| `sat/pid_i` | nee | Integrerende PID-term |
| `sat/pid_d` | nee | Differentierende PID-term |
| `sat/room_temp` | nee | Gemeten kamertemperatuur (C) |
| `sat/outside_temp` | nee | Buitentemperatuur (C) |
| `sat/boiler_status` | nee | Ketelstatuscode (0=Uit ... 14=Koeling) |
| `sat/flame_status` | nee | Vlamstatus (`healthy`, `short_cycling`, ...) |
| `sat/cycle_class` | nee | Cyclusclassificatie (`good`, `overshoot`, ...) |
| `sat/safety_tripped` | nee | Veiligheidsuitschakeling actief |
| `sat/pressure` | nee | CV-systeemdruk, glad (bar) |
| `sat/pressure_alarm` | nee | Druk-alarm actief |
| `sat/power` | nee | Geschat ketelvermogen (kW) |
| `sat/energy_total` | ja | Cumulatieve energie (kWh, voor HA-energiedashboard) |
| `sat/curve_recommendation` | nee | Verwarmingscurve-aanbeveling |
| `sat/preset_comfort` | ja | Comfort-voorinstelling (C) |
| `sat/preset_eco` | ja | Eco-voorinstelling (C) |
| `sat/preset_away` | ja | Afwezig-voorinstelling (C) |
| `sat/preset_sleep` | ja | Slaap-voorinstelling (C) |
| `sat/thermal_coeff` | ja | Geleerde thermische afname-coefficient |
| `sat/window_open` | nee | Raamopendetectie |
| `sat/solar_gain` | nee | Zonne-opbrengstcompensatie actief |
| `sat/summer_active` | nee | Zomer-simmeronderdrukking actief |

Aanvullende settings echo topics, BLE-sensoronderwerpen en multi-zone-onderwerpen zijn gedocumenteerd in `backlog/docs/doc-1 - sat-mqtt-topics.md`.

**Dallas temperatuursensoren**:

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `{sensor_adres}` | C | Temperatuur per sensoradres (bijv. `28FF64D1841703F1`) |

**S0-pulsteller**:

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `s0pulsecount` | - | Pulsen in huidig interval |
| `s0pulsecounttot` | - | Totaal aantal pulsen |
| `s0powerkw` | kW | Berekend vermogen |

**Ruw OpenTherm-bericht (optioneel)**:

Wanneer `mqttotmessage` is ingeschakeld:

| Onderwerp | Beschrijving |
|-----------|-------------|
| `otmessage` | Ruw OpenTherm-frame zoals ontvangen van de PIC |

#### Geabonneerde onderwerpen (commando's)

De firmware abonneert op `{TopTopic}/set/{UniqueId}/#`.

**Directe besturingscommando's:**

| Onderwerpsuffix | Voorbeeld payload | OT-commando | Beschrijving |
|----------------|-------------------|-------------|-------------|
| `command` | `"TT=20.5"` | (rauw) | Ruw OTGW-commando |
| `setpoint` | `"20.5"` | `TT=20.5` | Tijdelijk kamersetpoint |
| `constant` | `"20.5"` | `TC=20.5` | Constant kamersetpoint |
| `outside` | `"12.0"` | `OT=12.0` | Buitentemperatuur |
| `hotwater` | `"1"` / `"0"` / `"P"` | `HW=...` | Warmwater aan/uit/puls |
| `gatewaymode` | `"1"` / `"0"` | `GW=...` | Gateway/monitor-modus |
| `maxchsetpt` | `"80"` | `SH=80` | Max. CV-watertemperatuur |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max. warmwatertemperatuur |
| `maxmodulation` | `"100"` | `MM=100` | Max. modulatieniveau |
| `ctrlsetpt` | `"55"` | `CS=55` | Aanvoersetpoint direct |
| `chenable` | `"1"` / `"0"` | `CH=...` | CV-vrijgave |
| `coolingenable` | `"1"` / `"0"` | `CE=...` | Koeling vrijgave |
| `summermode` | `"1"` / `"0"` | `SM=...` | Zomermodus (opgeslagen in flash) |

Let op: Commando's worden alleen verwerkt wanneer een OT-interface (PIC of OTDirect) beschikbaar is. Zonder interface worden MQTT-commando's genegeerd met een debugmelding.

**SAT-commando's:**

| Onderwerpsuffix | Payload | Beschrijving |
|----------------|---------|-------------|
| `sat/target` | `"21.0"` | SAT doeltemperatuur |
| `sat/enabled` | `"true"` / `"false"` | SAT in-/uitschakelen |
| `sat/control_mode` | `"continuous"` / `"pwm"` / `"auto"` | Bedrijfsmodus |
| `sat/preset` | `"comfort"` / `"eco"` / `"away"` / `"sleep"` | Voorinstelling activeren |
| `sat/indoor_temp` | `"20.8"` | Externe kamertemperatuur |
| `sat/outdoor_temp` | `"7.5"` | Externe buitentemperatuur |
| `sat/humidity` | `"58"` | Luchtvochtigheid (%) |
| `sat/window` | `"open"` / `"closed"` / `"1"` / `"0"` | Raamsignaal |
| `sat/valves_open` | `"true"` / `"false"` | TRV-kleppen open |
| `sat/flush` | (willekeurig) | Kortstondige data wissen (PID-integraal + cyclusvenster) |
| `sat/reset_integral` | (willekeurig) | PID-integraal resetten |
| `sat/area/{0-3}` | `"20.5"` | Gebiedstemperatuur (multi-area modus) |
| `sat/sun_elevation` | `"32.5"` | Zonhoogte vanuit HA |
| `sat/heating_curve` | `"1.5"` | Verwarmingscurve-coefficient bijwerken |
| `sat/deadband` | `"0.25"` | PID deadband bijwerken |
| `sat/boiler_capacity` | `"24.0"` | Ketelcapaciteit (kW) bijwerken |

Meer dan 40 aanvullende SAT-instellingen zijn beschikbaar via MQTT. De volledige lijst is gedocumenteerd in `backlog/docs/doc-1 - sat-mqtt-topics.md`.

**OTGW32-commando's (alleen OTGW32)**:

| Onderwerpsuffix | Payload | Beschrijving |
|----------------|---------|-------------|
| `otgw32/room_temp` | `"20.5"` | Kamertemperatuur voor OT-direct verwarmingscurve |
| `otgw32/room_setpoint` | `"21.0"` | Kamersetpoint voor OT-direct |

#### Brongescheiden onderwerpen (optioneel)

Wanneer `settings.mqtt.bSeparateSources = true`, worden OpenTherm-waarden aanvullend gepubliceerd naar:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat   (T-berichten)
{TopTopic}/value/{UniqueId}/{label}/boiler        (B-berichten)
{TopTopic}/value/{UniqueId}/{label}/gateway       (R/A-berichten)
```

Dit maakt het mogelijk om te onderscheiden of een waarde afkomstig is van de thermostaat, de ketel of de gateway.

#### Home Assistant Auto-Discovery

Wanneer `mqtthadiscovery` is ingeschakeld, publiceert de firmware discovery-payloads naar het `homeassistant/`-prefix (configureerbaar via `mqtthaprefix`).

##### Discovery Drip Publisher

Discovery gebruikt een asynchrone bitmap-gestuurde drip publisher. In plaats van alle 200+ entity-configuraties in een burst te verzenden:

1. Alle bericht-ID's worden als "pending" gemarkeerd in een bitmap.
2. Vanuit de hoofdlus wordt per interval (normaal 2 seconden) een discovery-bericht gepubliceerd.
3. Bij weinig vrij geheugen (< 8000 bytes) wordt het interval vertraagd.
4. Bij voldoende geheugen keert het interval terug naar normaal.

Een volledige discovery-cyclus duurt circa 6-8 minuten bij normaal tempo.

Discovery wordt geactiveerd:
- Bij firmware-opstart of MQTT-instellingswijziging (alle ID's pending).
- Bij aanroep van `POST /api/v2/otgw/discovery` (alle ID's pending).
- Wanneer Home Assistant `"online"` stuurt naar `homeassistant/status` na offline te zijn geweest.

**Integratie-opmerking:** Na het activeren van discovery verschijnen entiteiten geleidelijk in Home Assistant gedurende enkele minuten. Dit is normaal gedrag.

Discovery-sjablonen zijn gecompileerd in PROGMEM (flash-geheugen) tijdens het bouwen vanuit het `mqttha.cfg`-bestand, waardoor LittleFS I/O tijdens discovery-publicatie wordt geelimineerd.

#### Retain-vlaggen samenvatting

| Type onderwerp | Retained |
|---------------|----------|
| Geboorte/LWT (`{naamruimte}`) | Ja |
| Firmwareinfo | Ja |
| PIC-info | Ja |
| OT Direct-info | Ja |
| OpenTherm numerieke waarden | Ja |
| SAT setpoints, modus, health-vlaggen | Ja |
| SAT PID-termen, temperaturen (vluchtig) | Nee |
| S0-pulsteller | Ja |
| Uptime, fouten | Nee |

---

### 9.5 Telnet-debugprotocol

#### Verbinding

```bash
telnet <ip-adres> 23
# of:
nc <ip-adres> 23
```

Geen authenticatie. Alleen beschikbaar op het lokale netwerk (LAN-vertrouwensmodel). De debug-uitvoer is alleen-lezen; de Telnet-verbinding accepteert geen commando's.

#### Uitvoerformaat

De Telnet-server (poort 23) streamt alle debuguitvoer die via `DebugTln()`, `DebugT()` en `DebugTf()` wordt gegenereerd. Dit omvat:

- Opstartberichten (WiFi-verbinding, MQTT-verbinding, NTP-synchronisatie)
- Inkomende en uitgaande OTGW-seriele berichten
- MQTT-publicaties en -abonnementen
- REST API-verzoeken, statuscode en timing (wanneer `bRestAPI` debug-vlag actief is)
- SAT-regelkringuitvoer: setpointberekening, PID-termen, ketelstatus
- Timer-gebeurtenissen en achtergrondtaken
- Storings- en waarschuwingsmeldingen
- Heap-statistieken

**Voorbeeld van typische uitvoer:**
```
[MQTT] Verbonden met broker 192.168.1.10:1883
[OT]   T80000200 (ID:0 Master status)
[OT]   B40000200 (ID:0 Boiler Ack)
[MQTT] Publiceer boilertemperature = 55.25
[SAT]  Control loop: Tr=20.5 Tboiler=62.3 setpoint=43.1 error=0.50
REST GET /api/v2/health => 200 v2/health 12ms
```

#### Debug-vlaggen

| Vlag | Categorie | Standaard |
|------|---------|---------|
| `bOTmsg` | OpenTherm-berichten | aan |
| `bMQTT` | MQTT-verkeer | uit |
| `bRestAPI` | REST API-verzoeken | uit |
| `bSensors` | Dallas-sensorscan | uit |
| `bSAT` | SAT-regelkring | aan |
| `bOTDirect` | OTDirect frame-afhandeling | aan |
| `bMQTTGate` | MQTT publish-gating en value-change deduplicatie | uit |

Debug-vlaggen kunnen tijdens runtime worden in-/uitgeschakeld via de REST API of het web-UI. Hoogfrequente vlaggen (`bMQTT`, `bRestAPI`) dienen in normaal gebruik uitgeschakeld te blijven.

#### Let op: seriele poort

De UART is na initialisatie exclusief in gebruik voor PIC-communicatie. `Serial.print()` is na de initialisatiefase **verboden** in de broncode. Alle debuguitvoer gaat uitsluitend via Telnet.

#### TCP-seriebrug (poort 25238)

Een apart TCP-socket op poort 25238 biedt toegang tot de ruwe OTGW-seriestream. Dit is bedoeld voor tools als OTmonitor die directe protocoltoegang nodig hebben:

```bash
nc otgw.local 25238
```

De uitvoer bestaat uit onverwerkte ASCII-regels van de PIC: regels als `T80000200`, `B4001FF04`, `R4001FF04`. Via deze poort kunnen ook ruwe PIC-commando's worden verstuurd.

---

### 9.6 Poortreferentie

| Poort | Protocol | Dienst |
|-------|----------|--------|
| 80 | TCP (HTTP) | Web-UI, REST API v2, bestandsserver |
| 81 | TCP (WebSocket) | Live OT-logstream, realtime-grafieken |
| 23 | TCP (Telnet) | Debug-console (alleen-lezen) |
| 25238 | TCP (raw) | TCP-seriebrug naar OTGW PIC |
| 1883 | TCP (MQTT) | MQTT-brokerverbinding (uitgaand) |
| 123 | UDP (SNTP) | NTP-tijdsynchronisatie (uitgaand) |
| 5353 | UDP (mDNS) | `otgw.local` naamresolutie |
| 5355 | UDP (LLMNR) | Windows naamresolutie |
