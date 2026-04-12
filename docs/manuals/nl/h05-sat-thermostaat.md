## Hoofdstuk 5: SAT - Smart Autotune Thermostaat

### Wat doet SAT?

SAT (Smart Autotune Thermostat) verandert de OpenTherm Gateway van een passieve protocol-brug in een actieve verwarmingscontroller. In plaats van simpelweg de thermostaat de ketel te laten aansturen, berekent SAT zelf de optimale aanvoertemperatuur op basis van de buitentemperatuur, de gewenste ruimtetemperatuur en hoe uw woning daadwerkelijk reageert.

**Hoe het verschil eruitziet in de praktijk:**

Zonder SAT: de wandthermostaat schakelt de ketel volledig aan (maximale temperatuur) totdat het setpoint bereikt is, schakelt dan uit, de kamer koelt af, en de cyclus herhaalt zich. Dit geeft temperatuurschommelingen, onnodig hoge aanvoertemperaturen en veel aan/uit-cycli.

Met SAT: de ketel draait continu op een lage aanvoertemperatuur die net genoeg warmte levert voor de actuele buitentemperatuur. Op een milde herfstdag draait de ketel op 30-35 °C; op een koude winterdag op 50-55 °C. De ruimtetemperatuur blijft stabiel binnen fracties van een graad. Moderne condenserende ketels zijn ontworpen voor precies dit gebruik en bereiken hun hoogste rendement bij lage aanvoertemperaturen.

SAT draait volledig op het ESP-apparaat. Er is geen Home Assistant, geen cloud en geen internetverbinding vereist voor de basiswerking.

---

### Vereisten en compatibiliteit

**Ketelcompatibiliteit**
SAT werkt met elke OpenTherm-compatibele ketel. Dit omvat de meeste moderne condenserende CV-ketels van merken als Intergas, Vaillant, Remeha, Worcester Bosch, Nefit, Baxi, Viessmann en andere.

SAT geeft een `CS=` (Control Setpoint) opdracht aan de PIC co-processor, die dit als aanvoertemperatuur-setpoint in de OpenTherm-bus injecteert. De ketel modules zijn aanvoertemperatuur naar gelang de belasting.

**Niet-modulerende ketels**
Bij ketels die niet of slecht moduleren (vast toerental of hoge minimale modulatie) werkt de PWM-modus beter dan de continue modus. SAT kan automatisch wisselen tussen de twee modi.

**Vloerverwarming versus radiatoren**
Beide stelsels worden ondersteund. Vloerverwarming gebruikt een lagere aanvoertemperatuur (max 50 °C) en een vlakkere verwarmingscurve.

**Ruimtetemperatuurmeting**
SAT heeft een ruimtetemperatuurmeting nodig. Mogelijke bronnen (in prioriteitsvolgorde):
1. OpenTherm bericht-ID 24 (als uw thermostaat dit stuurt)
2. Externe push via MQTT (`sat/indoor_temp`)
3. Externe push via REST API
4. BLE-sensor (alleen ESP32, Xiaomi LYWSD03MMC)

---

### SAT inschakelen

#### Via de webinterface

1. Open de webinterface (`http://otgw.local/`).
2. Ga naar het tabblad **SAT** of **Instellingen > SAT**.
3. Zet de schakelaar **SAT inschakelen** op aan.
4. Stel het verwarmingssysteem in (radiatoren of vloerverwarming).
5. Voer de gewenste doeltemperatuur in.
6. Klik op **Opslaan**.

#### Via MQTT

```
Onderwerp: OTGW/set/otgw-AABBCCDDEEFF/sat/enabled
Waarde:    true
```

#### Via REST API

```bash
curl -X POST -d '{"name":"satenabled","value":"true"}' \
  http://otgw.local/api/v2/settings
```

---

### Configuratieparameters

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satenabled` | `false` | bool | SAT in-/uitschakelen |
| `satsystem` | `0` | 0=radiatoren, 1=vloerverwarming | Type verwarmingssysteem |
| `sattargettemp` | `20.0` | 5-30 °C | Gewenste ruimtetemperatuur |
| `satcoefficient` | `1.5` | 0.1-5.0 | Steilheid van de verwarmingscurve |
| `satdeadband` | `0.25` | 0.05-2.0 °C | PID-deadband breedte |
| `satinterval` | `30` | 10-300 s | Regelinterval |
| `satexternaltemp` | `false` | bool | Externe ruimtetemperatuurbron gebruiken |
| `satpresetcomfort` | `21.0` | 15-28 °C | Comfort-voorinstelling |
| `satpreseteco` | `18.0` | 10-22 °C | Eco-voorinstelling |
| `satpresetaway` | `15.0` | 5-18 °C | Weg-voorinstelling |
| `satpresetslapen` | `16.0` | 5-22 °C | Slaap-voorinstelling |
| `satpwmautoswitch` | `true` | bool | Automatisch wisselen tussen PWM en continu |

---

### De verwarmingscurve

De verwarmingscurve is de basis van weerscompenserende verwarming. Ze beantwoordt de vraag: "Welke aanvoertemperatuur heeft de ketel nodig bij de huidige buitentemperatuur om de doeltemperatuur te handhaven?"

#### De formule

```
setpoint = basis_offset + (coëfficiënt / 4) × curvewaarde

waarbij:
  curvewaarde = 4 × (T_doel - 20)
              + 0,03 × (T_buiten - 20)²
              - 0,4 × (T_buiten - 20)
```

| Parameter | Radiatoren | Vloerverwarming | Omschrijving |
|---|---|---|---|
| `basis_offset` | 27,2 °C | 20,0 °C | Aanvoertemp bij curvewaarde = 0 |
| `coëfficiënt` | 0,5-3,0 (typisch 1,5) | 0,5-2,0 (typisch 1,0) | Steilheid van de curve |
| `T_doel` | Gebruikersinstelling | Gebruikersinstelling | Gewenste ruimtetemperatuur |
| `T_buiten` | OpenTherm-bus of sensor | OpenTherm-bus of sensor | Actuele buitentemperatuur |

De kwadratische term `0,03 × (T_buiten - 20)²` modelleert de niet-lineair toenemende warmteverliezen bij extreem koude buitentemperaturen.

#### Rekenvoorbeelden (radiatoren, coëfficiënt = 1,5)

| Buitentemperatuur | Doeltemp | Setpoint | Interpretatie |
|---|---|---|---|
| 15 °C | 20 °C | ≈ 28 °C | Milde dag, lage aanvoertemperatuur |
| 5 °C | 20 °C | ≈ 32 °C | Koele dag, gematigd |
| -5 °C | 20 °C | ≈ 38 °C | Koude dag, hogere aanvoer |
| -10 °C | 21 °C | ≈ 43 °C | Vriezend, duidelijke verwarming |
| -20 °C | 21 °C | ≈ 53 °C | Extreme kou, bijna op maximum |

#### Coëfficiënt kiezen

De coëfficiënt is de belangrijkste instelparameter. Een hogere coëfficiënt betekent een steilere curve: de aanvoertemperatuur stijgt sneller als het buiten kouder wordt.

| Isolatieniveau | Type woning | Aanbevolen coëfficiënt |
|---|---|---|
| Uitstekend | Passiefhuis, HR++-glas | 0,5 - 0,8 |
| Goed | Gebouwd na 2000, spouwmuurisolatie | 0,8 - 1,2 |
| Gemiddeld | 1970-2000, gedeeltelijke isolatie | 1,2 - 1,8 |
| Matig | Voor 1970, enkel glas, geen spouwmuur | 1,8 - 2,5 |

Begin met de aanbevolen waarde voor uw woningtype. Als de woning bij koude buitentemperaturen niet warm genoeg wordt, verhoog de coëfficiënt licht. Als de woning overkookt of te veel energie verbruikt, verlaag hem.

---

### Bedrijfsmodi: continu en PWM

#### Continue modus (standaard)

In continue modus stuurt SAT het PID-uitvoersignaal direct als aanvoertemperatuur-setpoint. De ketel moduleert zijn brander om deze temperatuur te bereiken en te handhaven. Dit is de optimale modus voor moderne condenserende ketels met goede modulatie (10-100% vermogen).

De continue modus bevat ook een klemlogica: als het PID-setpoint snel daalt maar het ketelwater nog warm is, wordt het minimum-setpoint geklemd op `keteltemperatuur - 5 °C`. Dit voorkomt dat de ketel stopt en direct weer opstart (kort-cycleren).

#### PWM-modus

Bij ketels met een hoge minimale modulatie (bijv. 30-40% minimum) kan de continue modus resulteren in te veel warmteafgifte bij mild weer. PWM-modus lost dit op door de vlam aan/uit te schakelen met een berekende duty cycle.

Werking:
1. PID-uitvoer wordt omgezet naar een duty cycle: `duty = (pid_output - basis_offset) / (max_setpoint - basis_offset)`
2. Binnen elk regelinterval is de vlam AAN voor `duty × interval` en UIT voor de rest
3. Bij AAN wordt het volledige PID-setpoint gestuurd; bij UIT wordt `CS=10` gestuurd om de vlam te onderdrukken
4. Minimale vlam-AAN-tijd: 30 seconden (om te korte brandercycli te voorkomen)

#### Automatisch wisselen

Met `satpwmautoswitch = true` (standaard) bewaakt SAT de ketelprestaties en wisselt automatisch:

- **Continu naar PWM:** als de aanvoertemperatuur 60 seconden lang meer dan 3 °C boven het setpoint blijft, kan de ketel niet voldoende moduleren.
- **PWM naar continu:** als de aanvoertemperatuur 180 seconden lang meer dan 2 °C onder het setpoint blijft, geeft de PWM te weinig vermogen.

| Situatie | Aanbevolen modus |
|---|---|
| Moderne condenserende ketel met goede modulatie | Continu |
| Ketel met hoge minimale modulatie (>30%) | PWM |
| Vloerverwarming (trage respons) | Continu |
| Oversized ketel met radiatoren | PWM |
| Niet zeker | Begin met Continu, automatisch wisselen aan |

---

### Het veiligheidssysteem

SAT implementeert zes onafhankelijke verdedigingslagen. Bij elke activering van een veiligheidslaag stuurt SAT `CS=0` naar de PIC, waarmee het setpoint-override wordt vrijgegeven en de wandthermostaat de ketel opnieuw direct bestuurt.

**Fundamentele veiligheidsprincipes:**
- **Veilig bij falen:** elke fout resulteert in `CS=0`. De ketel valt terug op de wandthermostaat.
- **Geen automatisch herstel:** SAT blijft uitgeschakeld totdat de gebruiker het expliciet opnieuw inschakelt.
- **Onafhankelijke lagen:** een bug in één laag raakt de andere lagen niet.

#### De zes veiligheidslagen

**Laag 1: Opstart-veiligheid**
Bij elke opstart, herstart of stroomuitval stuurt SAT als eerste handeling `CS=0` naar de PIC. Zo wordt voorkomen dat de ketel onbeperkt op het laatste setpoint blijft draaien terwijl de ESP nog opstart.

**Laag 2: Uitschakel-veiligheid**
Wanneer SAT wordt uitgeschakeld (via instellingen, MQTT of REST API), wordt onmiddellijk `CS=0` gestuurd, de PID wordt gereset en alle toestandsvariabelen worden gewist.

**Laag 3: Verlopen sensorwaarden**
- Externe ruimtetemperatuur: vervalt na 5 minuten zonder update
- Externe buitentemperatuur: vervalt na 10 minuten zonder update

Bij vervallen van de externe waarde valt SAT automatisch terug op de waarden van de OpenTherm-bus (zonder alarm). Als ook die niet beschikbaar zijn, activeert Laag 5.

**Laag 4: Harde temperatuurgrens**
Na elke berekening wordt het setpoint hard begrensd:
- Vloerverwarming: maximum 50 °C (beschermt vloerconstructie en screed)
- Radiatoren: maximum 80 °C (fysieke ketelgrens)
- Minimum: 10 °C (voorkomt bevriezing, staat vlam-uit toe)

**Laag 5: Aaneengesloten sensor-mislukkingen**
Als de ruimtetemperatuur 10 opeenvolgende keren ongeldig of ontbrekend is, activeert SAT een veiligheidsafsluiting. Elke geldige meting reset de teller.

**Laag 6: PIC-communicatiefout**
Als de PIC-co-processor 5 opeenvolgende regelcycli niet bereikbaar is, activeert SAT een veiligheidsafsluiting. Een geslaagde `CS=`-opdracht reset de teller.

#### Herstel na veiligheidsafsluiting

Na een veiligheidsafsluiting:
1. Het `safety_tripped`-veld is zichtbaar in de webinterface (rode badge), MQTT (`sat/safety_tripped`) en REST API.
2. SAT blijft inactief totdat de gebruiker hem expliciet opnieuw inschakelt.
3. Herstel via webinterface: SAT-schakelaar uit en opnieuw aan.
4. Herstel via MQTT: publiceer `true` naar `OTGW/set/{UniqueId}/sat/enabled`.

---

### Simulatiemodus

SAT beschikt over een simulatiemodus voor testen zonder een echte ketel. In simulatiemodus:
- Berekent SAT alle setpoints en PID-waarden normaal
- Stuurt SAT de opdrachten **niet** naar de PIC
- Zijn alle MQTT-topics en REST API-waarden beschikbaar met gesimuleerde data

De simulatiemodus is nuttig voor:
- Testen van Home Assistant-automatiseringen
- Controleren of de verwarmingscurve logische waarden geeft
- Demonstraties zonder actieve verwarmingsinstallatie

Activeren via instellingen (`satsimulation = true`) of REST API:
```bash
curl -X POST -d '{"name":"satsimulation","value":"true"}' \
  http://otgw.local/api/v2/settings
```

---

### Home Assistant-integratie

#### Klimaatentiteit

Wanneer SAT is ingeschakeld en MQTT met auto-discovery is geconfigureerd, maakt Home Assistant automatisch een klimaatentiteit aan:

| Entiteit | Beschrijving |
|---|---|
| `climate.otgw_sat` | Volledige klimaatentiteit met doeltemperatuur en modus |

Via deze entiteit kunt u:
- De doeltemperatuur instellen (slider of numeriek invoerveld)
- Een voorinstelling kiezen (comfort, eco, weg, slaap)
- De huidige ruimtetemperatuur aflezen
- De actuele SAT-modus aflezen (uit, continu, PWM)

#### SAT-sensorentiteiten

SAT publiceert ook afzonderlijke diagnostische entiteiten via auto-discovery:

| Entiteit | Omschrijving |
|---|---|
| `sensor.otgw_sat_setpoint` | Definitief aanvoertemperatuur-setpoint dat naar de ketel gestuurd wordt |
| `sensor.otgw_sat_heating_curve` | Berekende waarde van de verwarmingscurve |
| `sensor.otgw_sat_pid_output` | PID-uitvoer (curve + P + I + D) |
| `sensor.otgw_sat_error` | PID-fout (doeltemp minus ruimtetemp) |
| `sensor.otgw_sat_mode` | Bedrijfsmodus (uit, continu, pwm) |
| `sensor.otgw_sat_boiler_status` | Ketelstatuswaarde (0-14) |
| `sensor.otgw_sat_room_temp` | Ruimtetemperatuur gebruikt door PID |
| `sensor.otgw_sat_outside_temp` | Buitentemperatuur gebruikt door de verwarmingscurve |

#### Externe ruimtetemperatuur doorsturen

Als uw thermostaat geen ruimtetemperatuur via OpenTherm (bericht-ID 24) stuurt, gebruik dan een Home Assistant-automatisering om de waarde van een ander sensorentiteit door te sturen:

```yaml
automation:
  - alias: "Ruimtetemperatuur naar SAT"
    trigger:
      - platform: state
        entity_id: sensor.woonkamer_temperatuur
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp"
          payload_template: "{{ states('sensor.woonkamer_temperatuur') }}"
```

#### Buitentemperatuur doorsturen

Als de OpenTherm-bus geen buitentemperatuur (bericht-ID 27) bevat:

```yaml
automation:
  - alias: "Buitentemperatuur naar SAT"
    trigger:
      - platform: state
        entity_id: sensor.buitentemperatuur
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload_template: "{{ states('sensor.buitentemperatuur') }}"
```

De externe temperatuurwaarden vervallen automatisch na respectievelijk 5 en 10 minuten, zodat SAT terugvalt op de OpenTherm-buswaarden als de automatisering stopt.

---

### Bekende beperkingen

- SAT is niet ontworpen voor systemen met warmtepompen die geen OpenTherm-modulatie ondersteunen. Gebruik in dat geval de PWM-modus.
- Ruimtetemperatuurregeling is minder nauwkeurig als de ruimtetemperatuursensor niet in de verwarmde ruimte staat (bijv. in een gang of dichtbij een raam).
- SAT bewaakt één zone. Multi-zone ondersteuning is aanwezig (tot 4 zones via MQTT), maar de optellogica is een gemiddelde; afzonderlijke ketelcircuits per zone worden niet ondersteund.
- De PIC co-processor houdt het laatste `CS=`-setpoint vast als de ESP uitvalt. Laag 1 (opstart-veiligheid) corrigeert dit bij de volgende start.
- BLE-temperatuursensoren zijn alleen beschikbaar op de ESP32 (OTGW32).

---

### Probleemoplossing

| Symptoom | Mogelijke oorzaak | Oplossing |
|---|---|---|
| SAT stuurt geen setpoints | PIC niet bereikbaar | Controleer de seriële verbinding; controleer `otgw-pic/picavailable` via MQTT |
| Woning wordt niet warm genoeg | Coëfficiënt te laag | Verhoog de coëfficiënt stapsgewijs met 0,2 |
| Ketel schakelt te vaak aan en uit | Coëfficiënt te hoog of PWM-modus nodig | Verlaag de coëfficiënt of schakel over op PWM |
| Veiligheidslaag activeert herhaaldelijk | Ruimtetemperatuur ontbreekt | Controleer MQTT-push of OpenTherm ID 24 |
| SAT schakelt zichzelf uit | Veiligheidslaag 5 of 6 | Controleer sensorgeldigheid en PIC-communicatie; zie `sat/safety_tripped` |
| Temperatuur schommelt sterk | Deadband te smal of slechte ketelmodulatie | Vergroot de deadband of gebruik PWM |
| Setpoint stijgt niet ver genoeg bij vriesweer | `satcoefficient` te laag | Verhoog coëfficiënt; controleer buitentemperatuurmeting |
| Home Assistant klimaatentiteit verschijnt niet | Auto-discovery uitgeschakeld of MQTT niet verbonden | Controleer MQTT-verbinding en HA-discovery instelling |
| Setpoint altijd op minimum (10 °C) | SAT actief maar veiligheidslaag getripped | Herstart SAT via webinterface of MQTT |
| SAT-tab niet zichtbaar in webinterface | SAT nog niet ingeschakeld | SAT inschakelen in instellingen |
