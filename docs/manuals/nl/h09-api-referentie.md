## Hoofdstuk 9: API-referentie

Dit hoofdstuk beschrijft alle externe interfaces van de OTGW-firmware: de REST API v2, de WebSocket-interface, de MQTT-interface en het Telnet-debugprotocol.

---

### 9.1 REST API — Overzicht

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

#### Authenticatie

Schrijfbewerkingen (POST, PUT) vereisen HTTP Basic Auth wanneer een HTTP-wachtwoord is ingesteld (`settings.sHTTPpasswd`). GET-verzoeken vereisen over het algemeen geen authenticatie, tenzij uitdrukkelijk vermeld. Alle mutaties die via het web-UI worden ingediend, gebruiken dezelfde mechanismen.

CORS: `OPTIONS`-verzoeken worden altijd toegestaan (RFC 7231). Dezelfde-oorsprong-validatie via `Origin`/`Referer`-headers beschermt tegen CSRF-aanvallen (ADR-054).

---

### 9.2 REST API — Eindpunten

#### GET /api/v2/health

Systeem gezondheidsmetriek. Geen authenticatie vereist.

**Verzoek:** geen parameters

**Antwoord:**
```json
{
  "health": {
    "uptime": 12345,
    "heap_free": 22000,
    "heap_min": 18000,
    "heap_health": "healthy",
    "mqtt_connected": true,
    "otgw_connected": true,
    "version": "2.0.0"
  }
}
```

| Veld | Type | Beschrijving |
|------|------|-------------|
| `uptime` | int | Uptime in seconden |
| `heap_free` | int | Vrij heap-geheugen in bytes |
| `heap_min` | int | Minimaal vrij heap-geheugen ooit gemeten |
| `heap_health` | string | `"healthy"`, `"low"`, `"warning"`, `"critical"` |
| `mqtt_connected` | bool | MQTT-verbinding actief |
| `otgw_connected` | bool | PIC of OTDirect actief |
| `version` | string | Firmwareversie |

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
    "ssid": "MijnWifiNetwerk"
  }
}
```

Het veld `ssid` is alleen-lezen (type `"r"`): het geeft het momenteel verbonden WiFi-netwerk terug. Bij gebruik van Ethernet retourneert `/api/v2/device/info` de waarde `"Wired"`.

---

#### POST /api/v2/settings

Instellingen bijwerken. Authenticatie vereist.

**Verzoektekst:**
```json
{
  "hostname": "MijnOTGW",
  "mqttbroker": "192.168.1.20",
  "mqttport": "1883"
}
```

Meerdere velden kunnen in één verzoek worden bijgewerkt. De firmware valideert elk veld afzonderlijk en coalesceert flash-schrijfbewerkingen (2-seconden debounce). Serviceherstarts (MQTT, NTP, mDNS) worden uitgesteld tot na het wegschrijven.

**Antwoord bij succes:**
```json
{"status": "ok", "saved": true}
```

**Antwoord bij fout:**
```json
{"error": {"status": 400, "message": "Ongeldige waarde voor mqttport"}}
```

---

#### POST /api/v2/otgw/commands

Een OTGW-commando naar de PIC sturen. Authenticatie vereist.

**Verzoektekst (JSON):**
```json
{"command": "TT=21.0"}
```

**Verzoektekst (platte tekst):**
```
TT=21.0
```

Het commando wordt gevalideerd op formaat (`LL=waarde`, twee letters gevolgd door `=`) en in de commandowachtrij geplaatst.

**Antwoord bij succes (HTTP 202 Accepted):**
```json
{"status": "queued"}
```

**Antwoord bij fout (HTTP 400):**
```json
{"error": {"status": 400, "message": "Invalid command format (expected LL=value)"}}
```

**Antwoord wanneer geen OT-interface beschikbaar (HTTP 503):**
```json
{"error": {"status": 503, "message": "No OT command interface detected - commands disabled"}}
```

Veelgebruikte commando's:

| Commando | Beschrijving |
|----------|-------------|
| `TT=21.0` | Tijdelijk kamersetpoint 21,0 °C |
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

---

#### POST /api/v2/otgw/discovery

Home Assistant auto-discovery herstarten. Authenticatie vereist. Alternatief pad: `/api/v2/otgw/autoconfigure`.

**Verzoek:** geen parameters

**Antwoord:**
```json
{"status": "ok", "message": "Auto-discovery started"}
```

---

#### GET /api/v2/otgw/status

Actuele status van de OpenTherm Gateway en OpenTherm-bus.

**Antwoord (fragment):**
```json
{
  "otgw": {
    "pic_available": true,
    "pic_version": "5.4",
    "gateway_mode": true,
    "boiler_connected": true,
    "thermostat_connected": true,
    "flame": false,
    "ch_active": false,
    "dhw_active": false,
    "boiler_temp": 35.2,
    "room_temp": 20.5,
    "room_setpoint": 20.0,
    "control_setpoint": 0.0
  }
}
```

---

#### GET /api/v2/otgw/messages/{id}

OpenTherm-berichtwaarde opvragen op bericht-ID (0–127). Alias: `/api/v2/otgw/id/{id}`.

**Voorbeeld:** `GET /api/v2/otgw/messages/25`

**Antwoord:**
```json
{
  "id": 25,
  "label": "boilertemperature",
  "value": "55.25",
  "unit": "C",
  "last_updated": 12
}
```

`last_updated` is het aantal seconden geleden dat dit bericht voor het laatst werd gepubliceerd.

---

#### GET /api/v2/sensors

Alle Dallas DS18B20 temperatuursensoren opvragen.

**Antwoord:**
```json
{
  "sensors": [
    {
      "id": "28FF64D1841703F1",
      "label": "Cv-aanvoer",
      "temp_c": 55.25,
      "last_seen": 8
    },
    {
      "id": "28AB12CD34EF0012",
      "label": "",
      "temp_c": 21.0,
      "last_seen": 8
    }
  ]
}
```

---

#### GET /api/v2/sensors/labels

Sensorlabels opvragen.

#### POST /api/v2/sensors/labels

Sensorlabels bijwerken.

**Verzoektekst:**
```json
{
  "28FF64D1841703F1": "Cv-aanvoer",
  "28AB12CD34EF0012": "Woonkamer"
}
```

---

#### GET /api/v2/device/info

Apparaatinformatie opvragen (firmware, hardware, netwerk).

**Antwoord (fragment):**
```json
{
  "device": {
    "hostname": "OTGW",
    "version": "2.0.0",
    "board": "Nodoshop OTGW (ESP8266)",
    "hardware_mode": "PIC",
    "network_mode": "WiFi",
    "ip": "192.168.1.100",
    "mac": "AA:BB:CC:DD:EE:FF",
    "ssid": "MijnWifiNetwerk",
    "uptime": 12345,
    "heap_free": 22000
  }
}
```

---

#### GET /api/v2/device/time

Actuele systeemtijd opvragen (NTP-gesynchroniseerd).

**Antwoord:**
```json
{
  "time": {
    "epoch": 1712870400,
    "iso": "2026-04-12T10:00:00+02:00",
    "timezone": "Europe/Amsterdam",
    "ntp_synced": true
  }
}
```

---

#### GET /api/v2/sat

Smart Autotune Thermostat status opvragen. Voeg `?detail=full` toe voor uitgebreide diagnostiek.

**Antwoord (fragment):**
```json
{
  "sat": {
    "enabled": true,
    "mode": "pwm",
    "active": true,
    "setpoint": 43.1,
    "target": 21.0,
    "room_temp": 20.8,
    "outside_temp": 8.0,
    "pid_p": 0.82,
    "pid_i": 0.03,
    "pid_d": -0.04,
    "flame_status": "healthy",
    "cycle_class": "good"
  }
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

---

#### POST /api/v2/sat/externaltemp

Binnentemperatuur aanleveren vanuit een externe sensor. Authenticatie vereist.

**Verzoektekst:**
```
20.8
```

---

#### Overige SAT-eindpunten

| Methode | Pad | Beschrijving |
|---------|-----|-------------|
| POST | `/api/v2/sat/externaloutdoor` | Buitentemperatuur aanleveren |
| POST | `/api/v2/sat/humidity` | Luchtvochtigheid (0–100%) |
| POST | `/api/v2/sat/window` | Raamopendetectie (`open` / `closed`) |
| POST | `/api/v2/sat/preset` | Voorinstelling activeren (`comfort`, `eco`, `away`, `sleep`, `home`) |
| POST | `/api/v2/sat/flush` | PID-integraal en cyclusvenster wissen |
| POST | `/api/v2/sat/reset_integral` | Alleen PID-integraal resetten |

---

### 9.3 WebSocket-API

#### Verbinding

```
ws://<hostname>:81/
```

De WebSocket-server luistert op poort 81 (gescheiden van HTTP op poort 80). Alleen platte `ws://` — geen `wss://` (ADR-003, ADR-005). Geen authenticatie (vertrouwd LAN-model).

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
{"type": "ot_log", "data": "T80000200", "prefix": "T", "timestamp": 1712870400}
```

#### Berichttypen

| Type | Richting | Beschrijving |
|------|----------|-------------|
| `ot_log` | Server → Client | OpenTherm-frame van PIC of OTDirect |
| `keepalive` | Server → Client | Hartslag elke 30 seconden |
| `flash_progress` | Server → Client | OTA-voortgang in procenten |
| `system_event` | Server → Client | Systeemgebeurtenissen (herstart, verbinding) |

#### Frame-prefixen in `ot_log`

| Prefix | Beschrijving |
|--------|-------------|
| `T` | Bericht van thermostaat naar boiler |
| `B` | Antwoord van boiler |
| `R` | Verzoek ingespoten door gateway |
| `A` | Antwoord van gateway |
| `>` | Doorgestuurd (uitgaand) |
| `<` | Ontvangen |
| `!` | Fout |
| `*` | Systeemgebeurtenis |

#### Herverbindingsgedrag

De web-UI verbindt automatisch opnieuw bij verbindingsverlies. Maximaal 20 clients kunnen tegelijk verbonden zijn. Bij overvolle heap negeert de firmware nieuwe WebSocket-berichten om RAM te beschermen.

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
Publicatie:   OTGW/value/otgw-AABBCCDDEEFF/boilertemperature  → "55.25"
Abonnement:   OTGW/set/otgw-AABBCCDDEEFF/setpoint             → "21.0"
```

#### Verbindingslevenscyclus

Bij verbinding:
1. **Geboorte-bericht**: Publiceert `"online"` naar `{TopTopic}/value/{UniqueId}` (retained)
2. **Last Will**: Bij verbreking wordt automatisch `"offline"` gepubliceerd (retained)
3. **Abonnement**: Op `{TopTopic}/set/{UniqueId}/#` voor binnenkomende commando's
4. **Abonnement**: Op `homeassistant/status` voor Home Assistant levenscyclusdetectie

#### Gepubliceerde onderwerpen (selectie)

Tenzij anders vermeld zijn alle gepubliceerde onderwerpen retained.

**Firmwareinfo** (bij start en elke 5 minuten):

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `otgw-firmware/version` | - | Firmwareversie |
| `otgw-firmware/uptime` | s | Uptime in seconden (niet retained) |
| `otgw-firmware/reboot_count` | - | Aantal herstarts |
| `otgw-firmware/reboot_reason` | - | Reden laatste herstart |

**PIC Gateway-info**:

| Onderwerp | Waarden | Beschrijving |
|-----------|---------|-------------|
| `otgw-pic/version` | `"5.4"` | PIC-firmwareversie |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | PIC aanwezig |
| `otgw-pic/boiler_connected` | `"ON"` / `"OFF"` | Boilerverbinding |
| `otgw-pic/thermostat_connected` | `"ON"` / `"OFF"` | Thermostaatverbinding |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway-modus actief |

**OpenTherm numerieke waarden** (bij ontvangst):

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `controlsetpoint` | °C | Aanvoertemperatuur setpoint |
| `roomsetpoint` | °C | Kamersetpoint |
| `roomtemperature` | °C | Kamertemperatuur |
| `boilertemperature` | °C | Ketelwatertemperatuur |
| `returnwatertemperature` | °C | Retourwatertemperatuur |
| `dhwtemperature` | °C | Warmwatertemperatuur |
| `dhwsetpoint` | °C | Warmwater setpoint |
| `maxchwatersetpoint` | °C | Maximale CV-watertemperatuur |
| `outsidetemperature` | °C | Buitentemperatuur |
| `chwaterpressure` | bar | CV-waterdruk |
| `relmodlvl` | % | Relatief modulatieniveau |
| `maxrelmodlvl` | % | Maximaal modulatieniveau |
| `exhaust_temperature` | °C | Uitlaattemperatuur |
| `boiler_fan_speed` | rpm | Ventilatortoerentl. |
| `electrical_current_burner_flame` | µA | Vlamsignaalstroom |
| `dhw_flowrate` | l/min | Warmwaterdebiet |

**OpenTherm statusvlaggen** (bericht-ID 0):

| Onderwerp | Waarden | Beschrijving |
|-----------|---------|-------------|
| `flamestatus` | `"ON"` / `"OFF"` | Vlam actief |
| `chmodus` | `"ON"` / `"OFF"` | CV actief |
| `dhwmode` | `"ON"` / `"OFF"` | Warmwater actief |
| `chenable` | `"ON"` / `"OFF"` | CV-vrijgave door thermostaat |
| `faultindicator` | `"ON"` / `"OFF"` | Storingsmelding |

**SAT-onderwerpen** (elke 30 s, wanneer SAT actief is):

| Onderwerp | Eenheid | Retained | Beschrijving |
|-----------|---------|----------|-------------|
| `sat/mode` | - | ja | Bedrijfsmodus (`off`, `continuous`, `pwm`) |
| `sat/active` | bool | ja | Controller actief |
| `sat/setpoint` | °C | ja | Aanvoertemperatuur setpoint |
| `sat/target` | °C | ja | Gewenste kamertemperatuur |
| `sat/room_temp` | °C | nee | Gemeten kamertemperatuur |
| `sat/outside_temp` | °C | nee | Buitentemperatuur |
| `sat/pid_p` | °C | nee | Proportionele PID-term |
| `sat/pid_i` | °C | nee | Integrerende PID-term |
| `sat/pid_d` | °C | nee | Differentiërende PID-term |
| `sat/flame_status` | - | nee | Vlamstatus (`healthy`, `short_cycling`, ...) |
| `sat/cycle_class` | - | nee | Cyclusclassificatie (`good`, `overshoot`, ...) |
| `sat/pressure` | bar | nee | CV-systeemdruk (glad) |
| `sat/energy_total` | kWh | ja | Cumulatieve energie |

**Dallas temperatuursensoren**:

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `{sensor_adres}` | °C | Temperatuur per sensoradres (bijv. `28FF64D1841703F1`) |

**S0-pulsteller**:

| Onderwerp | Eenheid | Beschrijving |
|-----------|---------|-------------|
| `s0pulsecount` | - | Pulsen in huidig interval |
| `s0pulsecounttot` | - | Totaal aantal pulsen |
| `s0powerkw` | kW | Berekend vermogen |

#### Geabonneerde onderwerpen (commando's)

De firmware abonneert op `{TopTopic}/set/{UniqueId}/#`. Onderwerpsuffix en geldige waarden:

| Onderwerpsuffix | Voorbeeld payload | OT-commando | Beschrijving |
|----------------|-------------------|-------------|-------------|
| `command` | `"TT=20.5"` | (rauw) | Ruw OTGW-commando |
| `setpoint` | `"20.5"` | `TT=20.5` | Tijdelijk kamersetpoint |
| `constant` | `"20.5"` | `TC=20.5` | Constant kamersetpoint |
| `outside` | `"12.0"` | `OT=12.0` | Buitentemperatuur |
| `hotwater` | `"1"` / `"0"` / `"P"` | `HW=…` | Warmwater aan/uit/puls |
| `gatewaymode` | `"1"` / `"0"` | `GW=…` | Gateway/monitor-modus |
| `setback` | `"15.0"` | `SB=15.0` | Setback-temperatuur |
| `maxchsetpt` | `"80"` | `SH=80` | Max. CV-watertemperatuur |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max. warmwatertemperatuur |
| `maxmodulation` | `"100"` | `MM=100` | Max. modulatieniveau |
| `ctrlsetpt` | `"55"` | `CS=55` | Aanvoersetpoint direct |
| `chenable` | `"1"` / `"0"` | `CH=…` | CV-vrijgave |
| `coolingenable` | `"1"` / `"0"` | `CE=…` | Koeling vrijgave |
| `sat/target` | `"21.0"` | — | SAT doeltemperatuur |
| `sat/mode` | `"pwm"` | — | SAT bedrijfsmodus |
| `sat/externaltemp` | `"20.8"` | — | Externe kamertemperatuur |
| `sat/preset` | `"comfort"` | — | SAT voorinstelling activeren |
| `sat/window` | `"open"` / `"closed"` | — | Raamsignaal |

#### Retain-vlaggen samenvatting

| Type onderwerp | Retained |
|---------------|----------|
| Geboorte/LWT (`{naamruimte}`) | Ja |
| Firmwareinfo | Ja |
| PIC-info | Ja |
| OpenTherm numerieke waarden | Ja |
| SAT setpoints, modus, health-vlaggen | Ja |
| SAT PID-termen, temperaturen (vluchtig) | Nee |
| S0-pulsteller | Ja |
| Uptime, fouten | Nee |

#### Brongescheiden onderwerpen (optioneel)

Wanneer `settings.mqtt.bSeparateSources = true`, worden OpenTherm-waarden aanvullend gepubliceerd naar:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat   (T-berichten)
{TopTopic}/value/{UniqueId}/{label}/boiler        (B-berichten)
{TopTopic}/value/{UniqueId}/{label}/gateway       (R/A-berichten)
```

Dit maakt het mogelijk om te onderscheiden of een waarde afkomstig is van de thermostaat, de boiler of de gateway.

---

### 9.5 Telnet-debugprotocol

#### Verbinding

```bash
telnet <ip-adres> 23
# of:
nc <ip-adres> 23
```

Geen authenticatie. Alleen beschikbaar op het lokale netwerk (LAN-vertrouwensmodel).

#### Uitvoerformaat

De Telnet-server (poort 23) streamt alle debuguitvoer die via `DebugTln()`, `DebugT()` en `DebugTf()` wordt gegenereerd. Dit omvat:

- Opstartberichten (WiFi-verbinding, MQTT-verbinding, NTP-synchronisatie)
- Inkomende en uitgaande OTGW-seriële berichten (wanneer `settings.bMyDEBUG = true`)
- MQTT-publicaties en -abonnementen
- REST API-verzoeken en -antwoorden
- Timer-gebeurtenissen en achtergrondtaken
- Storings- en waarschuwingsmeldingen

**Voorbeeld van typische uitvoer:**
```
[WiFi] Verbonden, IP: 192.168.1.100
[MQTT] Verbonden met broker 192.168.1.10:1883
[NTP]  Gesynchroniseerd: 2026-04-12T10:00:00+02:00
[OT]   T80000200 (ID:0 Master→Boiler status)
[OT]   B40000200 (ID:0 Boiler→Ack)
[MQTT] Publiceer boilertemperature = 55.25
```

#### Uitgebreid debuggen

```bash
# Via REST API debuggen inschakelen:
curl -u user:pass -X POST http://OTGW.local/api/v2/settings \
  -H "Content-Type: application/json" \
  -d '{"mydebug": "true"}'
```

of via MQTT:
```
Topic:   OTGW/set/otgw-AABBCCDDEEFF/command
Payload: SB=15.0
```

#### Let op: seriële poort

De UART is na initialisatie exclusief in gebruik voor PIC-communicatie. `Serial.print()` is na de initialisatiefase **verboden** in de broncode. Alle debuguitvoer gaat uitsluitend via Telnet.
