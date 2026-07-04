## Hoofdstuk 9: API-referentie

Dit hoofdstuk beschrijft alle externe interfaces van de OTGW-firmware: de REST API v2, de WebSocket-interface, de MQTT-interface en het Telnet-debugprotocol.

Dit hoofdstuk is een gebruikersgericht overzicht. Voor de volledige, machine-leesbare specificatie (request/response schemas, statuscodes, voorbeelden) raadpleeg de OpenAPI 3.1-documenten in de repository:

- `docs/api/openapi.yaml`: volledige REST API-specificatie, gezaghebbende bron
- `docs/api/MQTT.md`: volledige MQTT-topicreferentie met payload-voorbeelden
- `docs/api/WEBSOCKET_FLOW.md`: WebSocket-verbindingslevenscyclus en berichtformaat

Wanneer dit hoofdstuk en de OpenAPI-specificatie elkaar tegenspreken, wint OpenAPI. Meld het als issue zodat dit hoofdstuk kan worden gecorrigeerd.

---

### 9.1 REST API -- Overzicht

#### Basis-URL

```
http://<hostname>/api/v2/
```

Vervang `<hostname>` door het IP-adres of de mDNS-naam van het apparaat (standaard: `OTGW.local`).

#### Versioning

Er zijn drie gereserveerde prefixen; slechts een is actief:

| Versie | Pad | Status |
|--------|-----|--------|
| v0 | `/api/v0/` | Legacy, verwijderd. Retourneert HTTP 410 Gone. |
| v1 | `/api/v1/` | Standaard (verwijderd in 2.x). Retourneert HTTP 410 Gone. |
| v2 | `/api/v2/` | Huidig, aanbevolen. Alle nieuwe integraties gebruiken deze prefix. |

#### Route-dispatch (ADR-050)

Intern worden `/api/v2/`-verzoeken afgehandeld door een statische routetabel `kV2Routes[]` in `src/OTGW-firmware/restAPI.ino`. Elk top-level resourcepadsegment wijst naar een handlerfunctie:

`health`, `settings`, `sensors`, `device`, `flash`, `pic`, `otdirect` (alleen OTGW32), `firmware`, `filesystem`, `simulate`, `otgw`, `webhook`, `sat`, `discovery`, `debug`, `network`.

Een nieuw eindpunt toevoegen kost twee regels: registreer een handler en voeg een regel toe aan `kV2Routes[]`. Zo blijft de API stabiel over builds heen: het dispatch-oppervlak is klein, expliciet en herleesbaar.

CORS-preflight `OPTIONS`-verzoeken op elk `/api/v2/...`-pad worden centraal afgehandeld en retourneren HTTP 204 met de noodzakelijke CORS-headers voor browserclients.

#### Berichtformaat

Alle verzoeken en antwoorden gebruiken `application/json`. Aanvraagtekst bij POST/PUT mag zowel JSON als een enkelvoudige waarde als platte tekst zijn (afhankelijk van het eindpunt).

#### Foutformaat

Alle fouten worden gegenereerd door de helperfunctie `sendApiError(httpCode, F("message"))` en gebruiken dezelfde JSON-envelope:

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
| 409 | Conflict: actie geweigerd vanwege toestand (verificatie al actief, markerquotum bereikt) |
| 410 | Gone: verouderde API-versie (v0, v1) |
| 413 | Request Entity Too Large: commando te lang |
| 414 | URI Too Long: URI overschrijdt de 50-byte parserbuffer |
| 429 | Too Many Requests: cooldown actief (bijv. `/api/v2/discovery/republish`) |
| 500 | Internal Server Error: weinig geheugen (< 4 KB), bestandssysteemfout |
| 503 | Service Unavailable: MQTT niet verbonden, geen OT-interface, PIC-flash bezig |

#### Authenticatie

Alle POST- en PUT-verzoeken vereisen HTTP Basic Auth wanneer een HTTP-wachtwoord is ingesteld (ADR-056). De gebruikersnaam is de apparaathostname (standaard `OTGW`). Authenticatie wordt centraal gecontroleerd voordat een mutatiehandler wordt uitgevoerd.

GET-verzoeken vereisen authenticatie alleen voor gevoelige gegevens (`/api/v2/settings`, `/api/v2/sat/*`, `/api/v2/debug`, PIC-instellingen). CORS `OPTIONS`-verzoeken worden altijd toegestaan (RFC 7231). Dezelfde-oorsprong-validatie via `Origin`/`Referer`-headers beschermt tegen CSRF-aanvallen (ADR-054). Beide controles, HTTP Basic Auth en CSRF, moeten slagen voordat een mutatie wordt uitgevoerd.

---

### 9.2 REST API -- Eindpunten

De onderstaande secties beschrijven de meest gebruikte eindpunten met gebruiksaanwijzingen en voorbeelden. Dit is geen uitputtende lijst. Voor de complete set eindpunten, inclusief elk veld, elke enum-waarde en elke foutvorm, zie `docs/api/openapi.yaml`. Dit bestand kan worden weergegeven als interactieve documentatie met Swagger UI, Redoc of elk ander OpenAPI-compatibel hulpmiddel.

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

Alle Dallas DS18B20 temperatuursensoren en S0-pulsteller opvragen. Geen authenticatie vereist.

#### GET /api/v2/sensors/status

Alias van `GET /api/v2/sensors`. Toegevoegd in 2.x zodat externe monitoringtools een voorspelbaar `/status`-pad kunnen pollen, zoals bij `/health` en `/sat/status`. Beide paden leveren exact dezelfde payload.

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
    "psram_found": 1,
    "psram_size": 2097152,
    "psram_free": 2000000,
    "flash_size": 8388608,
    "internal_free": 180000,
    "internal_maxblk": 110000,
    "chip_model_est": "ESP32-S3FH4R2",
    "picavailable": true,
    "otgwconnected": true,
    "otcommandinterface": true,
    "otdirectavailable": false
  }
}
```

De ESP32-S3 chip- en geheugenvelden voeden het Debug-scherm (alle integers, bytes tenzij anders vermeld): `psram_found` (`1` als PSRAM aanwezig is, `0` als afwezig), `psram_size` (totale PSRAM, 0 indien afwezig), `psram_free` (vrije PSRAM), `flash_size` (grootte flash-chip), `internal_free` (vrij intern, niet-PSRAM heap), `internal_maxblk` (grootste vrije interne heap-blok) en `chip_model_est` (string; best-effort ESP32-S3 package-onderdeelnummer afgeleid uit flash + PSRAM, bijvoorbeeld `"ESP32-S3FH4R2"`, of `"ESP32-S3FN8"` zonder PSRAM).

Op OTGW32-hardware: het veld `otdirectavailable` is `true` en aanvullende `otd*`-velden zijn aanwezig (`otdmode`, `otdbypass`, `otdmonitor`, `otdmaster`, `otdstepup`, `otdthermostat`, `otdsetback`, `otdschedtotal`, `otdschedactive`, `otdscheddisabled`, `otdoverrides`).

Het veld `otcommandinterface` is een string met de actieve OT-interface: `"PIC"`, `"OT-Direct"` of `"None"` (compatibiliteitsopmerking: eerdere versies retourneerden een boolean).

Op Ethernet-capabele hardware (OTGW32) zijn aanvullende velden zichtbaar voor de bedrade-link-status: `ethernetpresent` (PHY gedetecteerd) en `ethernetlink` (kabel aangesloten en link actief). Wanneer de actieve netwerkmodus `ethernet` is, retourneert het `ssid`-veld de letterlijke string `"Wired"` en wordt `wifirssi` als `0` gerapporteerd. De runtime kan zonder reboot tussen WiFi en Ethernet failoveren (TASK-581); `networkmode` weerspiegelt het momenteel actieve transport.

Wanneer het apparaat in AP-fallback-modus draait (alleen-prerelease vangnet voor onbereikbare WiFi), is `apfallback: true` aanwezig en bevat `ssid` de AP-SSID.

Aanvullende velden ontsluiten MQTT-discovery-telemetrie (`disc_published_topics`, `disc_pending_ids`, `disc_verify_runs`, `disc_republish_triggered`, `disc_last_missing`, `disc_last_orphan`, `disc_last_outcome`) en REST-handler-timing (`perf_device_info_total_ms`, `perf_settings_total_ms`, `perf_sat_status_total_ms`, …) voor ADR-062- en TASK-361-observability.

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
| GET | `/api/v2/sat/weather/needs-setup` | Geeft aan of de weer-onboardingwizard getoond moet worden (TASK-511) |
| POST | `/api/v2/sat/settings/{naam}` | SAT-instelling bijwerken (spiegelt MQTT `sat/*`-commando's) |
| GET / POST / DELETE | `/api/v2/sat/markers` | Verwarmingscurve-kalibratiemarkers beheren (TASK-586) |
| GET / PATCH | `/api/v2/sat/sensor-areas` | DS18B20-naar-SAT-area-mapping (TASK-587) |
| GET | `/api/v2/sat/ble/discovery` | BLE-roster opvragen (alleen ESP32, TASK-508) |
| POST | `/api/v2/sat/ble/select` | BLE-sensor selecteren via `{"mac": "..."}` (alleen ESP32) |
| POST | `/api/v2/sat/ble/label` | BLE-sensorlabel zetten via `{"mac": "...", "label": "..."}` (alleen ESP32) |
| POST / DELETE | `/api/v2/sat/ble/forget` | BLE-roster-slot wissen via `{"mac": "..."}` (alleen ESP32) |
| POST / PUT | `/api/v2/sat/ble/bindkey` | Encrypted-MiBeacon bindkey per sensor zetten via `{"mac": "...", "key": "<32 hex>"}` (alleen ESP32). Als het MAC-adres nieuw is wordt er een roster-slot voor toegewezen; een lege `key` wist de opgeslagen bindkey. De bindkey is een **alleen-schrijven geheim**: gevalideerd (leeg of exact 32 hex-tekens, anders 400), nooit gelogd, nooit teruggegeven (roster/discovery tonen alleen een `has_bindkey`/`has_key`-boolean). HTTP 507 wanneer de roster vol is. |
| POST / PUT | `/api/v2/sat/ble/rescan` | Zonder body: een actieve-scan-burst starten om geadverteerde namen te verversen (alleen ESP32). |

##### POST /api/v2/sat/settings/{naam}

Het volledige instellingenvocabulaire bevat onder andere: `heating_curve`, `boiler_capacity`, `deadband`, `max_modulation`, `dhw_setpoint`, `dhw_enabled`, `dhw_enable`, `interval`, `ovp_value`, `ovp_enabled`, `ovp_start`, `ovp_stop`, `push_setpoint`, `flame_off_offset`, `force_pwm`, `flow_offset`, `summer_simmer`, `summer_threshold`, `summer_min_hours`, `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `simulation`, `ble_enable`, `ble_mac`, `ble_interval`, `preset_sync`, `preset_sync_topic`, `multi_area`, `multi_area_count`, `auto_tune`, `auto_tune_rate`, `mod_sup_delay`, `mod_sup_offset`, `target_temp_step`, `min_pressure`, `max_pressure`, `max_pressure_drop`, `preset_comfort`, `preset_eco`, `preset_away`, `preset_sleep`, `preset_activity`, `preset_home`, `solar_gain`, `solar_freeze_integral`, `window_detection`, `pwm_auto_switch`, `sensor_max_age`, `error_monitoring`, `auto_gains_value`, `cycles_per_hour`, `valve_offset`, `heating_mode`, `heating_system`, `manufacturer`, `control_mode`, `preset`, `reset_integral`. De namen `ovp_start`, `ovp_stop` en `reset_integral` accepteren geen waarde. Onbekende namen retourneren HTTP 404.

##### GET / POST / DELETE /api/v2/sat/markers

Beheer van door de gebruiker geplaatste kalibratiemarkers op de verwarmingscurve (TASK-586). Markers worden bewaard in `/sat_markers.json` op LittleFS. De web-UI toont ze als ankerpunten op de verwarmingscurvegrafiek.

- **GET** retourneert de markerarray (of `[]` indien leeg).
- **POST** voegt een marker toe. Body: `{"outside_temp": 5.0, "flow_temp": 55.0, "label": "optionele tekst"}`. Bereik: `outside_temp` -15..25, `flow_temp` 10..90. Retourneert HTTP 201 met `{"id": N}`. Maximum 20 markers; een extra `POST` retourneert HTTP 409.
- **DELETE** `/api/v2/sat/markers/{id}` verwijdert de marker met de gegeven id.

##### GET / PATCH /api/v2/sat/sensor-areas

Beheer van de DS18B20-naar-SAT-area-mapping voor multi-area modus (TASK-587). Elk van vier areas (`0..3`) kan worden gekoppeld aan een Dallas-sensor via diens 16-tekens ROM-adres.

- **GET** retourneert de huidige mapping:
  ```json
  {"areas":[{"index":0,"sensor":"28FF64D1841703F1"},{"index":1,"sensor":""},{"index":2,"sensor":""},{"index":3,"sensor":""}]}
  ```
- **PATCH** (of POST / PUT) werkt een mapping bij. Body: `{"area": 0, "sensor": "28FF64D1841703F1"}`. Een lege `sensor`-string wist het slot. Het 16-teken ROM-adres moet hex zijn; anders HTTP 400.

##### SAT BLE-roster (alleen ESP32)

De BLE-roster toont nabije BLE-temperatuursensoren die door de ESP32-S3 worden gedetecteerd en laat de gebruiker er een promoveren tot actieve kamertemperatuurbron (TASK-508). Alle vier de eindpunten retourneren HTTP 404 wanneer het opgegeven MAC-adres niet in de roster staat. Op ESP8266-builds zijn deze paden afwezig (route geeft 404).

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

| Modus | PIC-code | Beschrijving |
|-------|----------|-------------|
| `gateway` | `GW=1` | Volledige gateway: scheduler + thermostaat-doorschakeling + overrides (standaard) |
| `monitor` | `GW=0` | Transparante doorvoer: alle frames ongewijzigd doorgestuurd (PIC-parity, TASK-438) |
| `bypass` | `GW=P` | Thermostaat rechtstreeks op ketel via relais; OTDirect inactief |
| `master` | `GW=S` | Zelfstandige OT-master: alleen scheduler, geen thermostaat verwacht |
| `loopback` | `GW=L` | Interne test: gesimuleerde ketelantwoorden, geen hardware nodig |

Opmerking (TASK-438): `monitor` is nu gekoppeld aan `GW=0` voor parity met PIC-firmwareconventies. De pre-2.0-mapping die `GW=0` voor bypass gebruikte, is verplaatst naar de alias `GW=P`.

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

#### MQTT Discovery-verificatie (ADR-062)

Deze eindpunten ontsluiten de bitmap-gestuurde discovery-publisher en zijn verificatievenster. Ze vormen de runtime-contract gebruikt door de Maintenance-pagina van de web-UI en door HA-integratiescripts.

##### GET /api/v2/discovery

Retourneert de huidige discovery-status en tellers. `last_outcome` is een van `clean`, `missing`, `aborted_heap`, `aborted_disconnect`, `unknown`.

```json
{
  "verification": {"active": false, "last_epoch": 1774548600, "last_missing": 0, "last_orphan": 0, "last_outcome": "clean"},
  "counters":     {"published_topics": 217, "pending_ids": 0, "verify_runs": 4, "republish_triggered": 1},
  "settings":     {"auto_verify": true}
}
```

##### POST /api/v2/discovery/verify

Start een verificatievenster. De firmware abonneert zich op zijn eigen discovery-prefix, telt retained config-topics en rapporteert `missing` en `orphan` totalen. Authenticatie vereist.

Geweigerd met HTTP 503 wanneer MQTT niet verbonden is of het vrije geheugen onder de verificatie-drempel ligt. Geweigerd met HTTP 409 wanneer er al een verificatievenster loopt of de drip-publisher nog pending ID's heeft. Bij succes HTTP 202 met `{"status":"verification_started","expected":N,"window_ms":15000}`.

##### POST /api/v2/discovery/republish

Markeert alle discovery-topics als pending voor de drip-publisher. Authenticatie vereist.

Onderworpen aan een 60 s cooldown om te voorkomen dat de drip-queue permanent niet-leeg blijft (TASK-356). Opeenvolgende aanroepen binnen de cooldown retourneren HTTP 429 met het aantal resterende seconden. Bij succes HTTP 200 met `{"status":"marked_pending","count":N}`.

#### Debug-dump

##### GET /api/v2/debug

Retourneert een enkel plat JSON-object met build-identiteit, runtime-state, instellingen, MQTT-status, OT-bus-status, heap- en discovery-tellers, SAT-runtime en (waar van toepassing) OTDirect-runtime. Authenticatie vereist.

Dit is de primaire diagnostische snapshot bij issue-rapportage; de veldenset is bewust breed en valt niet onder het stabiele contract. Gevoelige waarden (`http_passwd`, `mqtt.passwd`, `sat.weather_key`) zijn gemaskeerd als `"***"` wanneer ze zijn ingesteld.

#### Netwerk

##### GET /api/v2/network/scan

Start en polt een asynchrone WiFi-scan (TASK-585). Wordt gebruikt door de WiFi-kiezer op de Settings-pagina.

- Eerste aanroep: start de scan en retourneert `{"status":"scanning"}`.
- Vervolgaanroepen tijdens de scan: retourneren `{"status":"scanning"}`.
- Na voltooiing: retourneert eenmaal `{"status":"ready","count":N,"networks":[...]}`. Elke `networks`-entry: `{"ssid":"...", "rssi":-60, "channel":6, "secured":true, "connected":true}`.

Geweigerd met HTTP 503 zolang een PIC-flash-operatie loopt (de radio kan dan geen CPU delen). De scanbuffer wordt vrijgegeven nadat het resultaat is geserveerd.

---

### 9.3 WebSocket-API

Voor de volledige verbindingslevenscyclus, broadcast-gating-regels en de heap-backpressure-toestandsmachine, zie `docs/api/WEBSOCKET_FLOW.md`.

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

De SPA voorkomt re-entry van `initOTLogWebSocket()` zodat een snelle pagina-reload geen dubbele WebSocket-clients lekt op de firmware (TASK-563).

#### Heap backpressure

Bij laag vrij geheugen stopt de server met het versturen van WebSocket-berichten om het systeem te beschermen. Berichten die in deze periode worden overgeslagen, zijn definitief verloren.

---

### 9.4 MQTT-API

Voor de volledige topicinventaris (alle gepubliceerde waarden, alle geaccepteerde commando's, retained/niet-retained vlaggen en payload-voorbeelden), zie `docs/api/MQTT.md`. De tabellen in dit hoofdstuk zijn een snelle referentie voor de meest gebruikte topics. Binnenkomende commandotopics worden verwerkt door `handleMQTTcallback()` in `MQTTstuff.ino`, dat het laatste topicsegment parseert en naar de bijbehorende handler routeert.

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

De firmware abonneert op `{TopTopic}/set/{UniqueId}/#`. Het laatste segment na `set/{UniqueId}/` bepaalt het commando; `handleMQTTcallback()` routeert elke match naar het juiste OTGW PIC-commando of de interne handler.

**Directe besturingscommando's (snelle referentie):**

Publiceer een platte-tekstpayload naar deze topics. Deze tabel is een snelle referentie voor de meest gebruikte commandotopics; de gezaghebbende lijst staat in `docs/api/MQTT.md`.

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
| `outside_temp` | `"8.0"` | `OT=8.0` | Buitentemperatuur (alias van `outside`) |
| `setback` | `"15.0"` | `SB=15.0` | Setback-temperatuur |
| `ventsetpt` | `"50"` | `VS=50` | Ventilatie-setpoint |
| `temperaturesensor` | `"O"` / `"R"` | `TS=O` / `TS=R` | Temperatuursensorfunctie (outside / return) |
| `addalternative` / `delalternative` | `"34"` | `AA=…` / `DA=…` | OT-MsgID toevoegen aan/verwijderen uit alternatieve-rotatie |
| `unknownid` / `knownid` | `"34"` | `UI=…` / `KI=…` | MsgID als onbekend / bekend markeren |
| `priomsg` | `"5"` | `PM=5` | Prioriteitsbericht instellen |
| `setresponse` / `clearrespons` | `"34:0000"` / `"34"` | `SR=…` / `CR=…` | Stored response zetten / wissen |
| `resetcounter` | `"H"` | `RS=H` | PIC-teller resetten (`H`, `T`, `B`, `M`, `S`) |
| `gpioa` / `gpiob` | `"0".."7"` | `GA=…` / `GB=…` | GPIO A/B functie instellen |
| `leda`..`ledf` | letter | `LA=…`..`LF=…` | LED A..F functie (B/C/E/F/H/M/O/P/R/T/W/X) |
| `resetgateway` | (genegeerd) | hardware-reset | PIC hardware-resetten (payload genegeerd) |

Let op: Commando's worden alleen verwerkt wanneer een OT-interface (PIC of OTDirect) beschikbaar is. Zonder interface worden MQTT-commando's in de standaard-debugstream zichtbaar gemaakt en daarna verwijderd (niet langer stilletjes gedropt; zie commit 5571d9b7).

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

Wanneer `settings.mqtt.bSeparateSources = true`, worden OpenTherm-waarden aanvullend gepubliceerd met een sibling-suffix-shape (ADR-097):

```
{TopTopic}/value/{UniqueId}/{label}_thermostat   (T-frame)
{TopTopic}/value/{UniqueId}/{label}_boiler        (B-frame)
{TopTopic}/value/{UniqueId}/{label}_gateway       (R-frame)
{TopTopic}/value/{UniqueId}/{label}_answer        (A-frame, proxy-antwoord)
```

Sibling-suffix-namen (`<label>_<source>`) vervangen de oudere subtopic-shape zodat het HA-discovery `state_topic`-regex aan een enkel segment blijft gebonden. Proxy-antwoorden (A-prefix) worden geleid naar `_answer`, niet gecollapst op `_thermostat`, conform ADR-103.

Dit maakt het mogelijk om te onderscheiden of een waarde afkomstig is van de thermostaat, de ketel, de gateway of een proxy-antwoord.

#### Zelfbeschrijvende onderwerpnamen (ADR-106, standaard in 2.0.0)

Uitgebrachte 2.0.0-firmware publiceert OpenTherm-waarden onder zelfbeschrijvende onderwerpnamen: het label bevat de OT-semantische rol en eenheid (bijvoorbeeld `tboiler_c`, `controlsetpoint_c`, `chmodus`, `flame_status`). De korte legacy-namen uit de 1.4.x-lijn (`boilertemperature`, `controlsetpoint`, …) blijven beschikbaar via de toggle `settings.mqtt.bUseLegacyOtTopics = true` voor gebruikers die hun dashboards of automatiseringen niet kunnen migreren. Dit is een breaking change voor verse installaties; de toggle is het ondersteunde downgradepad.

#### HA-Core alias-topics (ADR-105, opt-in)

Wanneer `settings.mqtt.bHaCoreAliasEnable = true`, worden 37 aanvullende topic-aliassen gepubliceerd onder namen die overeenkomen met de Home Assistant Core `opentherm_gw`-integratie. Dit geeft gebruikers die migreren van die integratie drop-in-compatibiliteit voor bestaande automatiseringen. De aliassen zijn pure mirrors; firmware-side semantiek blijft ongewijzigd.

#### Home Assistant Auto-Discovery

Wanneer `mqtthadiscovery` is ingeschakeld, publiceert de firmware discovery-payloads naar het `homeassistant/`-prefix (configureerbaar via `mqtthaprefix`).

##### Discovery Drip Publisher

Discovery gebruikt een asynchrone bitmap-gestuurde drip publisher. In plaats van alle 200+ entity-configuraties in een burst te verzenden:

1. Bericht-ID's worden als "pending" gemarkeerd in een bitmap.
2. Vanuit de hoofdlus wordt per interval (normaal 2 seconden) een discovery-bericht gepubliceerd.
3. Bij weinig vrij geheugen (< 8000 bytes) wordt het interval vertraagd.
4. Bij voldoende geheugen keert het interval terug naar normaal.

##### Just-in-time discovery (TASK-578)

De 2.0.0-firmware publiceert OT-waarde-discovery-configs uitsluitend wanneer de bijbehorende MsgID minstens een keer op de bus is waargenomen. De drip-publisher loopt door alle bekende ID's, maar voegt een ID pas toe aan de pending-bitmap nadat `msglastupdated[id]` is gezet. Dit elimineert het eerdere gedrag waarbij entities werden geadverteerd voor MsgID's die de ketel nooit uitwisselt; de HA-entitylijst sluit nu aan op wat het apparaat daadwerkelijk rapporteert.

Discovery wordt geactiveerd:
- Bij firmware-opstart of MQTT-instellingswijziging (alle in aanmerking komende ID's pending).
- Bij aanroep van `POST /api/v2/otgw/discovery` of `POST /api/v2/discovery/republish` (alle in aanmerking komende ID's pending).
- Wanneer Home Assistant `"online"` stuurt naar `homeassistant/status` na offline te zijn geweest, mits de broker-restart-heuristiek (ADR-100) geen sessieverlies meldt.
- Wanneer een nieuw MsgID voor het eerst na opstart wordt waargenomen (single-ID JIT-publicatie).

**Integratie-opmerking:** Na het activeren van discovery verschijnen entiteiten geleidelijk in Home Assistant gedurende enkele minuten. Dit is normaal gedrag.

Discovery-payloads bevatten apparaat-metadata, entiteitnamen, meeteenheid, device class, state topics en een availability topic. De `availability`-referentie is gekoppeld aan de MQTT-clientverbinding (ADR-102), niet aan OT-bus-liveness; HA markeert entities alleen als `unavailable` wanneer de firmware echt los van de broker is.

Discovery-sjablonen zijn gecompileerd in PROGMEM (flash-geheugen) tijdens het bouwen vanuit het `mqttha.cfg`-bestand, waardoor LittleFS I/O tijdens discovery-publicatie wordt geelimineerd.

Pending-state-fan-out voor retained sensors (cycle class, flame status, OEM fault code) wordt niet langer gehinderd door de 250 ms rate-gate (ADR-104); first-seen-waarden bereiken HA in de eerste publish-lus.

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
- REST API-verzoeken (wanneer `bRestAPI` debug-vlag actief is): methode, URI, HTTP-statuscode, geselecteerd routesegment en totale handler-wandkloktijd in milliseconden
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
REST POST /api/v2/otgw/commands => 202 v2/otgw 4ms
```

Het laatste getal is de tijd doorgebracht in de handler, gemeten van URI-parse tot responsflush. Handig om trage eindpunten of flash-/LittleFS-onderbrekingen op te sporen zonder externe profiler.

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
