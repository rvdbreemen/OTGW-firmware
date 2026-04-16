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
3. Start de add-on. Ga naar de tabblad **Configuratie** en voeg een gebruiker toe:

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

4. Klik op **Opslaan**.

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

#### Debuggen van het publicatiefilter

Als u verbinding maakt via telnet (poort 23) en de toets `g` indrukt, wordt de MQTT-poort-debug ingeschakeld. De firmware logt dan elke beslissing:

```
MQTT gate id=25 src=S slot=25 prev=0x0000 curr=0x0510 first=true changed=true interval=false last=0 now=42 => publish [tracked update]
MQTT gate id=25 src=S slot=25 prev=0x0510 curr=0x0510 first=false changed=false interval=false last=42 now=71 => skip [suppressed by interval]
```

Dit is handig bij het vaststellen waarom een sensor wel of niet verschijnt in Home Assistant.

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

---

### Home Assistant auto-discovery

#### Hoe het werkt

OTGW-firmware implementeert de MQTT-auto-discovery van Home Assistant. De firmware publiceert discovery-payloads naar het geconfigureerde prefix (standaard `homeassistant/`).

Een discovery-payload is een JSON-bericht dat Home Assistant vertelt:
- Welk type entiteit het is (sensor, binaire sensor, klimaatentiteit)
- Wat de naam en het unieke ID zijn
- Op welk MQTT-topic de waarden worden gepubliceerd
- Op welk topic opdrachten kunnen worden gestuurd (voor klimaatentiteiten)
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
  "device": { "identifiers": ["otgw-AABBCCDDEEFF"], "name": "OTGW", "model": "...", ... }
}
```

#### Async drip publisher

Vanaf v2.0.0 gebruikt de firmware een asynchrone "drip" publisher voor discovery. In plaats van alle 345 discovery-berichten in een keer te versturen bij het opstarten, publiceert de firmware elke 3 seconden een entiteit op de achtergrond. Een volledige discovery-cyclus duurt daardoor ongeveer 17 minuten, maar deze aanpak is veel vriendelijker voor het beperkte geheugen van de ESP8266 en voor de MQTT-broker.

De drip publisher bevat twee beschermingsmechanismen:

- **Heap guard.** Als het vrije geheugen onder 8 KB zakt, wordt de discovery-publicatie overgeslagen totdat er weer voldoende geheugen beschikbaar is. Dit voorkomt crashes door geheugentekort tijdens periodes van hoge netwerkactiviteit.
- **Adaptief interval.** Bij geheugendruk wordt het interval automatisch verbreed van 3 seconden naar 30 seconden. Zodra het geheugen herstelt, keert het interval terug naar 3 seconden. Zo blijft het systeem stabiel terwijl de discovery toch vordert.

Tijdens de eerste 17 minuten verschijnen entiteiten geleidelijk in Home Assistant. Sensoren die nog niet ontdekt zijn, worden zichtbaar zodra ze gepubliceerd worden. Wanneer alle entiteiten ontdekt zijn, wordt de status onthouden en is er geen nieuwe discovery nodig, tenzij u er handmatig een afdwingt.

#### Discovery-templates in PROGMEM

Discovery-templates worden bij het compileren in het flashgeheugen (PROGMEM) opgeslagen in plaats van bij runtime uit het LittleFS-bestandssysteem geladen. Dit is transparant voor de gebruiker, maar elimineert bestandssysteem-I/O tijdens discovery en vermindert het RAM-gebruik enigszins. Het bestand `mqttha.cfg` wordt niet meer tijdens runtime gelezen; het dient alleen als bron voor de build-time codegenerator.

#### Wanneer wordt discovery opnieuw uitgevoerd?

De firmware publiceert alle discovery-configuraties opnieuw in twee situaties:

1. **Firmware-opstart of wijziging van MQTT-instellingen.** Alle discovery-ID's worden in de wachtrij geplaatst voor drip publishing.
2. **Home Assistant herstart.** De firmware bewaakt `homeassistant/status`. Wanneer HA weer online komt, worden alle ID's opnieuw in de wachtrij geplaatst.

Bij een eenvoudige MQTT-herverbinding (bijvoorbeeld een korte netwerkonderbreking) wordt discovery *niet* opnieuw uitgevoerd. De broker bewaart de discovery-berichten (retained), dus het opnieuw publiceren bij elke herverbinding zou onnodig zijn.

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

De exacte set sensoren is afhankelijk van welke OpenTherm bericht-ID's uw ketel en thermostaat uitwisselen. Alle 80+ gedefinieerde bericht-ID's kunnen een entiteit genereren.

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

#### Klimaatentiteit

Als SAT is ingeschakeld, maakt de firmware een SAT-klimaatentiteit aan in Home Assistant. Via deze entiteit kunt u:

- De doeltemperatuur instellen (5-30 °C, in stappen van 0,5 °C)
- De bedrijfsmodus selecteren (`off`, `heat` / continuous, `pwm`)
- Een voorinstelling kiezen (comfort, eco, weg, slaap) indien geconfigureerd
- De huidige ruimtetemperatuur aflezen
- De actuele SAT-modus aflezen

#### SAT-sensorentiteiten

Wanneer SAT is ingeschakeld, worden ook diagnostische entiteiten automatisch aangemeld bij Home Assistant. Deze worden gepubliceerd onder het topic-prefix `sat/`.

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
