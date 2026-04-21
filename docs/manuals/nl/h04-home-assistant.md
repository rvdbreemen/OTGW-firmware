## Hoofdstuk 4: Home Assistant-integratie

### Vereisten

Voor de integratie van OTGW-firmware met Home Assistant heeft u nodig:

- **Een werkende MQTT-broker** op uw thuisnetwerk. De meest gebruikte optie is de Mosquitto MQTT-broker als Home Assistant add-on.
- **De Home Assistant MQTT-integratie** geconfigureerd en verbonden met diezelfde broker.
- **OTGW-firmware geconfigureerd** met het IP-adres (of hostnaam) van de broker en optioneel inloggegevens.

Home Assistant zelf is niet vereist voor de basisfunctionaliteit van de firmware (MQTT-publicatie werkt ook zonder HA), maar is het primaire integratieplatform waarvoor auto-discovery is ontworpen.

---

### MQTT-broker instellen

#### Mosquitto als Home Assistant add-on (aanbevolen)

Als u Home Assistant OS of Home Assistant Supervised gebruikt, kunt u Mosquitto installeren als officiële add-on:

1. Ga in Home Assistant naar **Instellingen > Add-ons > Add-on Store**.
2. Zoek naar **Mosquitto broker** en klik op **Installeren**.
3. Start de add-on. Ga naar de tab **Configuratie** en voeg een gebruiker toe:

```yaml
logins:
  - username: otgw
    password: uw-wachtwoord
```

4. Sla op en herstart de add-on.
5. Ga naar **Instellingen > Apparaten en services > MQTT** en controleer de broker-verbinding.

Het IP-adres van de broker is in dit geval het IP-adres van uw Home Assistant-server. De standaard TCP-poort is `1883`.

#### Externe MQTT-broker

Als u al een zelfstandige Mosquitto-installatie, EMQX, HiveMQ of een andere broker heeft, gebruik dan het IP-adres en de inloggegevens van die broker. Zorg dat de OTGW en Home Assistant dezelfde broker gebruiken.

---

### Firmware configureren voor MQTT

1. Open de webinterface van de OTGW (`http://otgw.local/`).
2. Ga naar **Instellingen > MQTT**.
3. Vul de volgende velden in:

| Veld | Waarde | Voorbeeld |
|---|---|---|
| Broker | IP of hostnaam van de MQTT-broker | `192.168.1.10` |
| Poort | TCP-poort (standaard 1883) | `1883` |
| Gebruikersnaam | MQTT-gebruikersnaam (optioneel) | `otgw` |
| Wachtwoord | MQTT-wachtwoord (optioneel) | `uw-wachtwoord` |
| TopTopic | Bovenste namespace voor alle topics | `OTGW` |
| UniqueID | Uniek apparaat-ID in topics | `otgw-{MAC}` |
| HA Discovery | Aanvinken voor auto-discovery | aangevinkt |
| HA Prefix | Auto-discovery topic-prefix | `homeassistant` |
| Device Manufacturer | Fabrikant in HA device block | `NodoShop` |
| Device Model | Modelnaam in HA device block | `OTGW` |
| HA Discovery Auto-Verify | Automatische dagelijkse verificatie of alle verwachte HA discovery-configuraties nog in de broker aanwezig zijn; publiceert ontbrekende configs opnieuw. Voorkomt dat entiteiten offline gaan na een broker-herstart. | aangevinkt |

4. Klik op **Opslaan**.

De velden Device Manufacturer en Device Model zijn nieuw in 2.0.0 en instelbaar, zodat dezelfde firmware op verschillende hardware (klassieke NodoShop OTGW, ESP32 OT-Shield, etc.) correct gelabeld wordt in Home Assistant (**Instellingen > Apparaten en services > MQTT > OTGW**). Als verwijzing naar de oorsprong van het project wordt het topic `otgw-pic/designer` altijd gepubliceerd met de waarde `Schelte Bron`, ter credit aan de auteur van de originele PIC-firmware.

De firmware verbindt onmiddellijk met de broker en publiceert het geboorte-bericht (`online`). Daarna beginnen de auto-discovery payloads te verschijnen.

---

### MQTT-publicatiefilter (intervalpoort)

Standaard publiceert de firmware elke OpenTherm-waarde zodra die ontvangen wordt. Op drukke installaties kunnen dit tientallen berichten per seconde zijn. De instelling `MQTT Interval` maakt het mogelijk het berichtenverkeer te beperken.

#### Hoe het werkt

De instelling `MQTT Interval` (instellingenpagina, veld `iInterval`) bepaalt wanneer een waarde gepubliceerd wordt:

| Waarde `iInterval` | Publiceergedrag |
|---|---|
| `0` (standaard) | Publiceer bij elk ontvangen bericht, geen filtering |
| `> 0` (seconden) | Publiceer alleen als: de waarde voor het eerst gezien wordt (`firstSeen`), de waarde veranderd is ten opzichte van de vorige publicatie (`valueChanged`), of het ingestelde interval verstreken is (`intervalElapsed`) |

Met een interval van 60 seconden wordt een stabiele keteltemperatuur maximaal eenmaal per minuut gepubliceerd. Een waarde die tussentijds verandert, wordt alsnog direct gepubliceerd.

Het interval geldt alleen voor OpenTherm-databerichten. SAT-topics, verbindingsstatus, versie-informatie en andere niet-OT-topics worden altijd gepubliceerd zodra ze wijzigen.

#### MQTT debuggen

Maak via telnet verbinding op poort 23. Er zijn twee afzonderlijke debug-schakelaars voor MQTT:

| Toets | Vlag | Wat het logt |
|---|---|---|
| `3` | `bMQTT` | MQTT module-trace: verbinden, subscriben, publiceren, discovery, fouten |
| `g` | `bMQTTGate` | Per-bericht-beslissingen van het publicatiefilter |

Schakel `g` in om te zien waarom een bericht wel of niet door het publicatiefilter komt:

```
MQTT gate id=25 src=S slot=25 prev=0x0000 curr=0x0510 first=true changed=true interval=false last=0 now=42 => publish [tracked update]
MQTT gate id=25 src=S slot=25 prev=0x0510 curr=0x0510 first=false changed=false interval=false last=42 now=71 => skip [suppressed by interval]
```

Schakel `3` in voor een hoger-niveau MQTT-trace bij het onderzoeken van verbindings-, subscriptie- of discovery-problemen. De twee vlaggen zijn onafhankelijk: de gate-log is volume-intensief, dus in de praktijk schakelt u die alleen kortstondig in tijdens het diagnosticeren van een specifieke sensor.

---

### MQTT topic-structuur

#### Publish topics (firmware → broker)

Alle gepubliceerde waarden staan onder de namespace:

```
{TopTopic}/value/{UniqueId}/{topic}
```

Met standaardinstellingen wordt dit:

```
OTGW/value/otgw-AABBCCDDEEFF/{topic}
```

Voorbeelden:

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature        → "45.50"
OTGW/value/otgw-AABBCCDDEEFF/flamestatus              → "ON"
OTGW/value/otgw-AABBCCDDEEFF/roomtemperature          → "20.80"
OTGW/value/otgw-AABBCCDDEEFF/otgw-firmware/version    → "2.0.0"
```

De meeste waarden worden gepubliceerd met `retain = true`, zodat clients die later subscriben de laatste waarde meteen ontvangen.

#### Command topics (broker → firmware)

Opdrachten worden gestuurd naar:

```
{TopTopic}/set/{UniqueId}/{commando}
```

Voorbeelden:

```
OTGW/set/otgw-AABBCCDDEEFF/setpoint       → "21.5"
OTGW/set/otgw-AABBCCDDEEFF/dhw            → "ON"
OTGW/set/otgw-AABBCCDDEEFF/otgwcmnd       → "TT=21.00"
```

#### Geboorte- en LWT-berichten

| Topic | Waarde | Omschrijving |
|---|---|---|
| `{TopTopic}/value/{UniqueId}` | `online` | Gepubliceerd bij verbinden (retained) |
| `{TopTopic}/value/{UniqueId}` | `offline` | Gepubliceerd door broker bij verbindingsverlies (LWT, retained) |

Home Assistant gebruikt het `online`/`offline`-bericht om de beschikbaarheidsstatus van het apparaat bij te houden.

#### Diagnostische topics

De firmware publiceert eenmaal per uur een heap- en discovery-statistiekenoverzicht naar:

```
{TopTopic}/otgw-firmware/stats/heap
```

De payload is een retained JSON-object met 17 velden:

| Veld | Omschrijving |
|---|---|
| `free_heap` | Vrij heap-geheugen in bytes |
| `max_block` | Grootste aaneengesloten alloceerbaar blok in bytes |
| `frag_pct` | Heap-fragmentatiepercentage |
| `ws_drops` | WebSocket-berichten laten vallen vanwege heap-druk |
| `mqtt_drops` | MQTT-publicaties laten vallen vanwege heap-druk |
| `enter_low` | Aantal keren dat de heap in LOW-toestand terechtkwam |
| `enter_warning` | Aantal keren dat de heap in WARNING-toestand terechtkwam |
| `enter_critical` | Aantal keren dat de heap in CRITICAL-toestand terechtkwam |
| `drip_burst_skip` | Discovery drip-cycli overgeslagen tijdens post-Status-burst-cooldown |
| `drip_cooldown_skip` | Discovery drip-cycli overgeslagen tijdens heap-druk-cooldown |
| `drip_slowmode` | Aantal discovery drip-cycli uitgevoerd in slow mode (interval 10 s) |
| `disc_verify_runs` | Aantal voltooide dagelijkse discovery-verificatieruns |
| `disc_republish_triggered` | Aantal keren dat een verificatie ontbrekende configs vond en heruitgave triggerde |
| `disc_last_missing` | Aantal ontbrekende discovery-configs bij de meest recente verificatie |
| `disc_last_orphan` | Aantal onverwachte retained discovery-topics bij de meest recente verificatie |
| `disc_published_topics` | Totaal aantal bijgehouden discovery-topics |
| `disc_last_verify_epoch` | Unix-tijdstempel van de meest recente verificatierun |

Dit topic is nuttig voor het bewaken van langlopende installaties: een lage `free_heap`, stijgende `frag_pct` of herhaalde `enter_critical`-tellers zijn vroege signalen dat een nachtelijke herstart aan te raden is.

---

### Home Assistant auto-discovery

#### Hoe het werkt

OTGW-firmware implementeert de MQTT-auto-discovery van Home Assistant. De firmware publiceert discovery-payloads naar het geconfigureerde prefix (standaard `homeassistant/`).

Een discovery-payload is een JSON-bericht dat Home Assistant vertelt:
- Welk type entiteit het is (sensor, binary sensor, climate, switch, select, number)
- Wat de naam en het unieke ID zijn
- Op welk MQTT-topic de waarden worden gepubliceerd
- Op welk topic opdrachten kunnen worden gestuurd (voor climate, switch, select, number)
- Welke meeteenheid van toepassing is

Voorbeeld van een discovery-payload voor de aanvoertemperatuur:

```json
{
  "name": "Boiler flow temperature",
  "unique_id": "otgw-AABBCCDDEEFF_boilertemperature",
  "state_topic": "OTGW/value/otgw-AABBCCDDEEFF/boilertemperature",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "availability_topic": "OTGW/value/otgw-AABBCCDDEEFF",
  "payload_available": "online",
  "payload_not_available": "offline",
  "device": { "identifiers": ["otgw-AABBCCDDEEFF"], "name": "OTGW", "manufacturer": "NodoShop", "model": "OTGW" }
}
```

#### Streaming discovery architectuur

Vanaf v2.0.0 gebruikt de firmware een streaming discovery pipeline (geïmplementeerd in `MQTTHaDiscovery.cpp`). Elke discovery-payload wordt on the fly opgebouwd en in blokken van 128 bytes naar de MQTT-client geschreven, vergelijkbaar met hoe ESPHome zijn discovery-berichten opbouwt. Er wordt nooit een volledige JSON-string in RAM gebufferd. Dit houdt de geheugendruk laag (ongeveer 200 bytes werkgeheugen per entiteit) en voorkomt de heap-fragmentatieproblemen die de oudere aanpak op de ESP8266 opleverde.

De streaming-functies (`streamSensorDiscovery`, `streamBinarySensorDiscovery`, `streamClimateDiscovery`, `streamNumberDiscovery`, `streamDallasSensorDiscovery`, `streamSatSwitchDiscovery`, `streamSatSelectDiscovery`) vervangen de eerdere PROGMEM-template aanpak. Streaming staat altijd aan; er is geen schakelaar om het uit te zetten.

Het oude bestand `mqttha.cfg` dat eerdere versies op LittleFS opsloegen, wordt niet meer tijdens runtime gebruikt. Het is gearchiveerd in `docs/archive/mqttha.cfg` en dient enkel nog als referentie voor de oorspronkelijke template-indeling.

#### Bitmap-gestuurde drip publisher

Discovery is incrementeel. Een 256-bits "done" bitmap houdt bij welke OpenTherm message-ID's al een discovery-publicatie hebben gekregen. Twee entry points sturen de pipeline aan:

- `doAutoConfigure()` wist de bitmap en streamt alle entiteiten in één ronde. Wordt gebruikt bij opstarten, bij handmatig forceren, en wanneer Home Assistant zelf herstart.
- `doAutoConfigureMsgid(OTid)` publiceert alleen de discovery-items die bij één OpenTherm message-ID horen, en alleen als de bitmap aangeeft dat deze ID nog niet gedaan is. Deze wordt opportunistisch aangeroepen terwijl OT-verkeer langskomt, zodat de discovery-set voor een bepaalde ketel natuurlijk wordt opgebouwd terwijl berichten gezien worden.

Twee pseudo-ID's worden gebruikt om niet-OT-entiteiten aan de bitmap op te hangen:

- ID `0` bevat de twee climate-entiteiten (thermostaat en DHW-control), de 13 SAT-switches en de SAT-select.
- ID `27` bevat de number-entiteit voor buitentemperatuur-override.

Dallas-sensoren hebben een eigen pad (`streamDallasSensorDiscovery`) omdat hun discovery afhankelijk is van het 1-Wire-adres dat pas tijdens runtime bekend wordt; ze worden via `configSensors()` gepubliceerd zodra die adressen beschikbaar zijn.

#### Heap-bescherming

Elke `stream*Discovery`-functie controleert de vrije heap (`STREAM_HEAP_MIN = 4000` bytes) voordat een payload opgebouwd wordt en geeft `false` terug als het geheugen te laag is. De drip-loop probeert het dan later opnieuw, wanneer het geheugen hersteld is. Zo blijft discovery doorlopen bij tijdelijke heap-druk zonder dat het apparaat crasht.

#### Wanneer wordt discovery opnieuw uitgevoerd?

De firmware publiceert alle discovery-configuraties opnieuw in drie situaties:

1. **Firmware-opstart of wijziging van MQTT-instellingen.** De bitmap wordt gewist en `doAutoConfigure()` streamt elke entiteit.
2. **Home Assistant herstart.** De firmware bewaakt `homeassistant/status`. Wanneer HA weer online komt, wordt `doAutoConfigure()` opnieuw aangeroepen.
3. **Dagelijkse automatische verificatie (wanneer `MQTTdiscoveryAutoVerify` is ingeschakeld).** Eenmaal per dag controleert de firmware of alle verwachte discovery-configuraties nog retained aanwezig zijn in de broker. Ontbrekende configs worden automatisch opnieuw gepubliceerd. Dit zelfherstellend mechanisme voorkomt dat HA-entiteiten onbeschikbaar worden na een broker-herstart waarbij retained berichten verloren zijn gegaan.

Bij een eenvoudige MQTT-herverbinding (bijvoorbeeld een korte netwerkonderbreking) wordt discovery *niet* opnieuw uitgevoerd. De broker bewaart de discovery-berichten (retained), dus opnieuw publiceren bij elke herverbinding zou onnodig zijn.

#### Volledige discovery handmatig afdwingen

Druk in de telnet-debugconsole op `F` (hoofdletter). Dit wist de done-bitmap en verstuurt discovery voor elke entiteit opnieuw. Hetzelfde is te bereiken via de REST API:

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

---

### Auto-ontdekte entiteiten

#### Sensoren (selectie)

| Entiteitnaam | Topic (suffix) | Eenheid | Omschrijving |
|---|---|---|---|
| Boiler aanvoertemperatuur | `boilertemperature` | °C | CH aanvoertemperatuur |
| Retourtemperatuur | `returnwatertemperature` | °C | CH retourtemperatuur |
| Ruimtetemperatuur | `roomtemperature` | °C | Gemeten ruimtetemperatuur |
| Buitentemperatuur | `outsidetemperature` | °C | Buitentemperatuur |
| Warmwatertemperatuur | `dhwtemperature` | °C | DHW temperatuur |
| Relatieve modulatie | `relmodlvl` | % | Modulatieniveau van de ketel |
| CH waterdruk | `chwaterpressure` | bar | Systeemdruk |
| Aanvoersetpoint | `controlsetpoint` | °C | Aanvraag aanvoertemperatuur |
| Ruimtetemperatuursetpoint | `roomsetpoint` | °C | Gewenste ruimtetemperatuur |
| OEM diagnostiekcode | `oemdiagnosticcode` | - | Ketelspecifieke diagnosecode |

De exacte set sensoren is afhankelijk van welke OpenTherm bericht-ID's uw ketel en thermostaat uitwisselen. Elke gedefinieerde bericht-ID kan een of meer entiteiten genereren.

#### Binaire sensoren (selectie)

| Entiteitnaam | Topic (suffix) | Omschrijving |
|---|---|---|
| Vlam | `flamestatus` | Branderstatus (aan/uit) |
| CV actief | `chmodus` | Centrale verwarming actief |
| Warm water actief | `dhwmode` | Warmwaterbereiding actief |
| Storing | `faultindicator` | Ketelstoring aanwezig |
| Koeling actief | `coolingactive` | Koeling actief |
| CH2 actief | `ch2modus` | Tweede CV-circuit actief |
| Diagnostische indicator | `diagnosticindicator` | Diagnostische indicator ketel |
| PIC beschikbaar | `otgw-pic/picavailable` | PIC co-processor aanwezig |
| Ketel verbonden | `otgw-pic/boiler_connected` | Ketelcommunicatie OK |
| Thermostaat verbonden | `otgw-pic/thermostat_connected` | Thermostaatcommunicatie OK |

#### Climate-entiteiten

Er worden twee climate-entiteiten gepubliceerd:

- Thermostaat-climate: toont de huidige ruimtetemperatuur en het actieve setpoint en laat u vanuit het HA-dashboard een nieuwe doeltemperatuur instellen.
- DHW-climate: exposeert de warmwaterregeling als climate-entiteit.

Als SAT is ingeschakeld, is de thermostaat-climate de SAT-gestuurde variant, met ondersteuning voor modus- en preset-keuze.

#### Number-entiteit

Voor het overschrijven van de buitentemperatuur wordt een number-entiteit gepubliceerd; hiermee kan Home Assistant een waarde pushen alsof die van een bekabelde buitensensor komt.

#### SAT-switches en -select (nieuw in 2.0.0)

Wanneer SAT is ingeschakeld, publiceert de streaming-pipeline 13 boolean switches en 1 select, waarmee Home Assistant direct de SAT-feature-vlaggen kan aansturen. Alle entiteitsnamen worden voorafgegaan door de hostname.

Switches:

| Suffix | Doel |
|---|---|
| `SAT_Solar_Gain` | Zon-gain compensatie inschakelen |
| `SAT_Summer_Simmer` | Summer simmer auto-disable inschakelen |
| `SAT_Comfort_Adjust` | Comfort adjust inschakelen |
| `SAT_Multi_Area` | Multi-area gewogen ruimtetemperatuur inschakelen |
| `SAT_Auto_Tune` | Automatische PID-gain tuning inschakelen |
| `SAT_Simulation` | SAT draaien in simulatiemodus (geen echte ketelregeling) |
| `SAT_Window_Detection` | Openraamdetectie inschakelen |
| `SAT_Force_PWM` | PWM-modus afdwingen, ongeacht ketelmodulatie |
| `SAT_Push_Setpoint` | Control-setpoint via TC= naar de ketel pushen |
| `SAT_OVP_Enabled` | Overshoot prevention-kalibratie inschakelen |
| `SAT_Preset_Sync` | Climate-preset synchroniseren naar secondaire entiteiten |
| `SAT_DHW_Enabled` | DHW-regeling binnen SAT inschakelen |
| `SAT_PWM_Auto_Switch` | Automatisch wisselen tussen continuous- en PWM-modus |

Select:

| Suffix | Opties | Doel |
|---|---|---|
| `SAT_Heating_System` | `0`, `1`, `2`, `3` | Type verwarmingssysteem (radiator, vloerverwarming, laagtemperatuur, warmtepomp) |

Elke switch gebruikt `true`/`false` als payload en is retained; de select publiceert en accepteert bovenstaande numerieke strings.

#### SAT-sensorentiteiten

Wanneer SAT is ingeschakeld, worden ook diagnostische sensor-entiteiten automatisch aangemeld bij Home Assistant. Deze worden gepubliceerd onder het topic-prefix `sat/`.

**Kern SAT-topics (altijd gepubliceerd als SAT actief is):**

| Topic-suffix | Omschrijving | Eenheid |
|---|---|---|
| `sat/setpoint` | Definitief aanvoertemperatuur-setpoint naar de ketel | °C |
| `sat/target` | Doelruimtetemperatuur | °C |
| `sat/mode` | Bedrijfsmodus: `off`, `continuous` of `pwm` | - |
| `sat/heating_curve` | Berekende verwarmingscurvewaarde | °C |
| `sat/pid_output` | PID-gecorrigeerde uitvoer | °C |
| `sat/error` | PID-fout (doel minus ruimtetemp) | °C |
| `sat/room_temp` | Ruimtetemperatuur gebruikt door PID | °C |
| `sat/outside_temp` | Buitentemperatuur gebruikt door de verwarmingscurve | °C |
| `sat/boiler_status` | Actuele ketelstatus (tekstlabel) | - |
| `sat/active` | Of SAT actief is | bool |
| `sat/safety_tripped` | Of een veiligheidslaag is geactiveerd | bool |
| `sat/simulation` | Simulatiemodus aan/uit | - |

**SAT-commando-topics (firmware abonneert hierop):**

| Topic-suffix | Geaccepteerde waarden | Omschrijving |
|---|---|---|
| `sat/target` | Getal, bijv. `21.0` | Stel doelruimtetemperatuur in |
| `sat/control_mode` | `off`, `continuous`, `pwm`, `auto` | Stel bedrijfsmodus in |
| `sat/enabled` | `true` / `false` | SAT in- of uitschakelen |
| `sat/indoor_temp` | Getal, bijv. `20.5` | Stuur binnentemperatuur van externe sensor |
| `sat/outdoor_temp` | Getal, bijv. `5.2` | Stuur buitentemperatuur van externe bron |

---

### Commando's sturen vanuit Home Assistant

#### Setpoint instellen

Stuur een getal (als tekst) naar het setpoint-commando-topic:

```yaml
# Home Assistant automation voorbeeld
service: mqtt.publish
data:
  topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
  payload: "21.5"
```

Of via de klimaatentiteit (als SAT actief is):

```yaml
service: climate.set_temperature
target:
  entity_id: climate.otgw
data:
  temperature: 21.5
```

#### Warm water inschakelen

```yaml
service: mqtt.publish
data:
  topic: "OTGW/set/otgw-AABBCCDDEEFF/dhw"
  payload: "ON"
```

#### Ruwe OTGW-commando's sturen

Voor gevorderde gebruikers: u kunt ruwe PIC-commando's sturen via het `otgwcmnd`-topic. Dit is gelijkwaardig aan het typen van een commando in de TCP serial bridge.

```yaml
service: mqtt.publish
data:
  topic: "OTGW/set/otgw-AABBCCDDEEFF/otgwcmnd"
  payload: "TT=21.00"
```

**Let op:** Ruwe commando's worden direct doorgestuurd naar de PIC zonder validatie. Gebruik uitsluitend geldige OTGW-commando's. Raadpleeg de PIC-firmware documentatie voor de volledige commandolijst.

---

### Voorbeeldautomatiseringen

#### Weg-modus: setpoint verlagen als iedereen weg is

```yaml
automation:
  - alias: "OTGW: verlaag setpoint bij afwezigheid"
    trigger:
      - platform: state
        entity_id: group.personen_thuis
        to: "not_home"
    action:
      - service: climate.set_preset_mode
        target:
          entity_id: climate.otgw
        data:
          preset_mode: "away"
```

#### Buitentemperatuur doorsturen naar SAT

Als uw thermostaat geen buitentemperatuur via OpenTherm publiceert, kunt u een weerstation of de Home Assistant weerservice gebruiken om de buitentemperatuur door te sturen naar SAT:

```yaml
automation:
  - alias: "OTGW: buitentemperatuur naar SAT sturen"
    trigger:
      - platform: state
        entity_id: sensor.buitentemperatuur
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload_template: "{{ states('sensor.buitentemperatuur') }}"
```

#### Melding bij ketelstoring

```yaml
automation:
  - alias: "OTGW: melding bij ketelstoring"
    trigger:
      - platform: state
        entity_id: binary_sensor.otgw_storing
        to: "on"
    action:
      - service: notify.mobile_app
        data:
          message: "Ketelstoring gedetecteerd! Controleer de ketel."
          title: "OTGW Waarschuwing"
```

---

### Handmatige HA-configuratie zonder auto-discovery

Als u auto-discovery niet wenst in te schakelen, kunt u entiteiten handmatig definiëren in `configuration.yaml`. Dit is optioneel en voor de meeste gebruikers niet nodig.

#### Sensor (YAML-voorbeeld)

```yaml
mqtt:
  sensor:
    - name: "Ketel aanvoertemperatuur"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/boilertemperature"
      unit_of_measurement: "°C"
      device_class: temperature
      availability_topic: "OTGW/value/otgw-AABBCCDDEEFF"
      payload_available: "online"
      payload_not_available: "offline"

    - name: "Relatieve modulatie"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/relmodlvl"
      unit_of_measurement: "%"
```

#### Binaire sensor (YAML-voorbeeld)

```yaml
mqtt:
  binary_sensor:
    - name: "Ketel vlam"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/flamestatus"
      payload_on: "ON"
      payload_off: "OFF"
      device_class: heat

    - name: "Ketelstoring"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/faultindicator"
      payload_on: "ON"
      payload_off: "OFF"
      device_class: problem
```

#### Klimaatentiteit voor SAT (YAML-voorbeeld)

```yaml
mqtt:
  climate:
    - name: "OTGW Thermostaat"
      mode_state_topic: "OTGW/value/otgw-AABBCCDDEEFF/sat/mode"
      mode_command_topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/control_mode"
      temperature_state_topic: "OTGW/value/otgw-AABBCCDDEEFF/sat/target"
      temperature_command_topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/target"
      current_temperature_topic: "OTGW/value/otgw-AABBCCDDEEFF/sat/room_temp"
      modes:
        - "off"
        - "heat"
      min_temp: 5
      max_temp: 30
      temp_step: 0.5
```

**Opmerking:** Bij handmatige configuratie werkt `availability_topic` naar behoren zolang u het juiste `{TopTopic}/value/{UniqueId}`-topic gebruikt en `payload_available: "online"` instelt.

---

### Nachtelijke herstart

De firmware ondersteunt een optionele geplande nachtelijke herstart om heap-geheugen vrij te maken. Dit is nuttig voor langlopende installaties waar geleidelijke heap-fragmentatie uiteindelijk instabiliteit kan veroorzaken.

| Instelling | Standaard | Omschrijving |
|---|---|---|
| Nightly Restart | uit | Schakel de dagelijkse geplande herstart in of uit |
| Nightly Restart Hour | 4 | Uur (0-23) waarop de herstart plaatsvindt. Alleen actief als NTP ingeschakeld en gesynchroniseerd is. |

Deze instellingen zijn beschikbaar in de Web UI (instellingenpagina), via de REST API (`/api/v2/settings`) en via het MQTT settings-topic. De herstart veroorzaakt een korte onderbreking van ongeveer 30 seconden.
