## Hoofdstuk 5: SAT - Smart Autotune Thermostaat

### Wat doet SAT?

SAT (Smart Autotune Thermostat) verandert de OpenTherm Gateway van een passieve protocol-brug in een actieve verwarmingscontroller. In plaats van simpelweg de thermostaat de ketel te laten aansturen, berekent SAT zelf de optimale aanvoertemperatuur op basis van de buitentemperatuur, de gewenste ruimtetemperatuur en hoe uw woning daadwerkelijk reageert.

**Hoe het verschil eruitziet in de praktijk:**

Zonder SAT: de wandthermostaat schakelt de ketel volledig aan (maximale temperatuur) totdat het setpoint bereikt is, schakelt dan uit, de kamer koelt af, en de cyclus herhaalt zich. Dit geeft temperatuurschommelingen, onnodig hoge aanvoertemperaturen en veel aan/uit-cycli.

Met SAT: de ketel draait continu op een lage aanvoertemperatuur die net genoeg warmte levert voor de actuele buitentemperatuur. Op een milde herfstdag draait de ketel op 30-35 C; op een koude winterdag op 50-55 C. De ruimtetemperatuur blijft stabiel binnen fracties van een graad. Moderne condenserende ketels zijn ontworpen voor precies dit gebruik en bereiken hun hoogste rendement bij lage aanvoertemperaturen.

SAT draait volledig op het ESP-apparaat. Er is geen Home Assistant, geen cloud en geen internetverbinding vereist voor de basiswerking.

---

### 5.1 Vereisten en compatibiliteit

**Ketelcompatibiliteit**
SAT werkt met elke OpenTherm-compatibele ketel. Dit omvat de meeste moderne condenserende CV-ketels van merken als Atag, Baxi, Ferroli, Geminox, Ideal, Immergas, Intergas, Itho, Nefit, Radiant, Remeha, Sime, Vaillant, Viessmann, Worcester en andere. SAT bevat een manufacturer table met quirk flags per merk. Auto-detect is de standaardinstelling.

SAT geeft een `CS=` (Control Setpoint) opdracht aan de PIC co-processor, die dit als aanvoertemperatuur-setpoint in de OpenTherm-bus injecteert. De ketel moduleert zijn aanvoertemperatuur naar gelang de belasting.

De OTGW moet in gateway mode staan (GW=1), niet in bypass of monitor mode.

**Niet-modulerende ketels**
Bij ketels die niet of slecht moduleren (vast toerental of hoge minimale modulatie) werkt de PWM-modus beter dan de continue modus. SAT kan automatisch wisselen tussen de twee modi.

**Verwarmingssysteemtypes**
SAT ondersteunt vier systeemtypes: auto-detect, radiatoren, warmtepomp en vloerverwarming. Vloerverwarming gebruikt een lagere aanvoertemperatuur (max 50 C) en een vlakkere verwarmingscurve.

**Ruimtetemperatuurmeting**
SAT heeft een ruimtetemperatuurmeting nodig. Mogelijke bronnen (in prioriteitsvolgorde):
1. OpenTherm bericht-ID 24 (als uw thermostaat dit stuurt)
2. Externe push via MQTT (`sat/indoor_temp`)
3. Externe push via REST API
4. Multi-area gewogen gemiddelde (tot 4 zones)
5. BLE-sensor (alleen ESP32, Xiaomi LYWSD03MMC of BTHome v2)

---

### 5.2 SAT inschakelen

#### Via de webinterface

1. Open de webinterface (`http://otgw.local/`).
2. Ga naar het tabblad **SAT** of **Instellingen > SAT**.
3. Zet de schakelaar **SAT inschakelen** op aan.
4. Stel het verwarmingssysteem in (auto-detect, radiatoren, warmtepomp of vloerverwarming).
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

### 5.3 Configuratieparameters

#### Kernparameters

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satenabled` | `false` | bool | SAT in-/uitschakelen |
| `satheatsystem` | `0` | 0-3 | 0=auto-detect, 1=radiatoren, 2=warmtepomp, 3=vloerverwarming |
| `sattargettemp` | `20.0` | 5-30 C | Gewenste ruimtetemperatuur |
| `satcoefficient` | `1.5` | 0.1-5.0 | Steilheid van de verwarmingscurve |
| `satdeadband` | `0.1` | 0.05-2.0 C | PID-deadband breedte |
| `satinterval` | `30` | 10-300 s | Regelinterval |
| `satexternaltemp` | `false` | bool | Externe ruimtetemperatuurbron gebruiken |
| `satpwmautoswitch` | `true` | bool | Automatisch wisselen tussen PWM en continu |
| `satforcepwm` | `false` | bool | PWM-modus forceren ongeacht ketelmodulatie |
| `satmaxrelmod` | `100` | 0-100 % | Maximale relatieve modulatie naar ketel (MM= commando) |
| `satmanufacturer` | `0` | 0-18 | Ketelmerkt (0 = auto-detect) |

#### Preset-temperaturen

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satpresetcomfort` | `21.0` | 15-28 C | Comfort-voorinstelling |
| `satpreseteco` | `18.0` | 10-22 C | Eco-voorinstelling |
| `satpresetaway` | `15.0` | 5-18 C | Weg-voorinstelling |
| `satpresetsleep` | `16.0` | 5-22 C | Slaap-voorinstelling |
| `satpresethome` | `18.0` | 5-28 C | Thuis-voorinstelling |
| `satpresetactivity` | `10.0` | 5-20 C | Activiteit-voorinstelling (gebruikt bij raamdetectie) |
| `satpresetsync` | `false` | bool | Preset-wijzigingen synchroniseren met secundaire entiteiten via MQTT |

#### PID-afstemming

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satautogains` | `true` | bool | Automatische PID-gain berekening uit verwarmingscurve |
| `satkpmanual` | `5.0` | 0-100 | Handmatige proportionele gain (bij autogains = false) |
| `satkimanual` | `0.0005` | 0-1 | Handmatige integrale gain |
| `satkdmanual` | `0.0` | 0-10 | Handmatige differentiaalte gain |
| `satautotune` | `false` | bool | Automatische PID-gains bijsturing inschakelen |
| `satautotunerate` | `0.02` | 0-1 | Bijstellingspercentage per afstemcyclus (2%) |

#### Geavanceerde regeling

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satmaxsetpoint` | `65.0` | 30-80 C | Maximaal toegestaan setpoint (veiligheidsplafond) |
| `satflameoffoffset` | `0.0` | 0-5 C | Setpoint-offset bij vlam uit (anti-cycling hysterese) |
| `satovershootmargin` | `2.0` | 0-10 C | Overshoot-marge voor cyclusclassificatie |
| `satmodsupdelay` | `20.0` | 0-60 s | Modulatie-onderdrukkingsvertraging |
| `satmodsupoffset` | `1.0` | 0-5 C | Modulatie-onderdrukkingsoffset onder setpoint |
| `satflowoffset` | `2.0` | 0-10 C | Continue modus: maximale setpoint-daling t.o.v. keteltemperatuur |
| `satcyclesperhour` | `3` | 2-6 | Doelcycli per uur |
| `satpushsetpoint` | `false` | bool | SAT-doel naar thermostaatdisplay sturen (TC= commando) |

#### Coefficient kiezen

De coefficient is de belangrijkste instelparameter. Een hogere coefficient betekent een steilere curve: de aanvoertemperatuur stijgt sneller als het buiten kouder wordt.

| Isolatieniveau | Type woning | Aanbevolen coefficient |
|---|---|---|
| Uitstekend | Passiefhuis, HR++-glas | 0,5 - 0,8 |
| Goed | Gebouwd na 2000, spouwmuurisolatie | 0,8 - 1,2 |
| Gemiddeld | 1970-2000, gedeeltelijke isolatie | 1,2 - 1,8 |
| Matig | Voor 1970, enkel glas, geen spouwmuur | 1,8 - 2,5 |

Begin met de aanbevolen waarde voor uw woningtype. Als de woning bij koude buitentemperaturen niet warm genoeg wordt, verhoog de coefficient licht. Als de woning overkookt, verlaag hem. SAT publiceert ook een Heating Curve Recommendation (zie paragraaf 5.11) om de juiste waarde te vinden.

---

### 5.4 De verwarmingscurve

De verwarmingscurve is de basis van weerscompenserende verwarming. Ze beantwoordt de vraag: "Welke aanvoertemperatuur heeft de ketel nodig bij de huidige buitentemperatuur om de doeltemperatuur te handhaven?"

#### De formule

```
setpoint = basis_offset + (coefficient / 4) x curvewaarde

waarbij:
  curvewaarde = 4 x (T_doel - 20)
              + 0,03 x (T_buiten - 20)^2
              - 0,4 x (T_buiten - 20)
```

| Parameter | Radiatoren | Vloerverwarming | Omschrijving |
|---|---|---|---|
| `basis_offset` | 27,2 C | 20,0 C | Aanvoertemp bij curvewaarde = 0 |
| `coefficient` | 0,5-3,0 (typisch 1,5) | 0,5-2,0 (typisch 1,0) | Steilheid van de curve |
| `T_doel` | Gebruikersinstelling | Gebruikersinstelling | Gewenste ruimtetemperatuur |
| `T_buiten` | OpenTherm-bus of sensor | OpenTherm-bus of sensor | Actuele buitentemperatuur |

De kwadratische term `0,03 x (T_buiten - 20)^2` modelleert de niet-lineair toenemende warmteverliezen bij extreem koude buitentemperaturen.

#### Rekenvoorbeelden (radiatoren, coefficient = 1,5)

| Buitentemperatuur | Doeltemp | Setpoint | Interpretatie |
|---|---|---|---|
| 15 C | 20 C | ~28 C | Milde dag, lage aanvoertemperatuur |
| 5 C | 20 C | ~32 C | Koele dag, gematigd |
| -5 C | 20 C | ~38 C | Koude dag, hogere aanvoer |
| -10 C | 21 C | ~43 C | Vriezend, duidelijke verwarming |
| -20 C | 21 C | ~53 C | Extreme kou, bijna op maximum |

---

### 5.5 Bedrijfsmodi: continu en PWM

#### Continue modus (standaard)

In continue modus stuurt SAT het PID-uitvoersignaal direct als aanvoertemperatuur-setpoint. De ketel moduleert zijn brander om deze temperatuur te bereiken en te handhaven. Dit is de optimale modus voor moderne condenserende ketels met goede modulatie (10-100% vermogen).

De continue modus bevat klemlogica: als het PID-setpoint snel daalt maar het ketelwater nog warm is, wordt het minimum-setpoint geklemd op `keteltemperatuur - flow_offset` (standaard 2 C). Dit voorkomt dat de ketel stopt en direct weer opstart (kort-cycleren).

#### PWM-modus

Bij ketels met een hoge minimale modulatie (bijv. 30-40% minimum) kan de continue modus resulteren in te veel warmteafgifte bij mild weer. PWM-modus lost dit op door de vlam aan/uit te schakelen met een berekende duty cycle. Minimale vlam-AAN-tijd: 180 seconden (om te korte brandercycli te voorkomen).

**PWM forceren**: Met `satforcepwm = true` wordt PWM-modus geforceerd ongeacht het ketelgedrag.

#### Automatisch wisselen

Met `satpwmautoswitch = true` (standaard) bewaakt SAT de ketelprestaties via cyclusclassificatie (zie paragraaf 5.11) en wisselt automatisch:

- **Continu naar PWM:** als de aanvoertemperatuur langer dan 60 seconden boven het setpoint + overshoot-marge blijft (ketel kan niet voldoende moduleren).
- **PWM naar continu:** als de aanvoertemperatuur langer dan 180 seconden meer dan 2 C onder het setpoint blijft (onvoldoende vermogen) of na 300 seconden verzadiging.

| Situatie | Aanbevolen modus |
|---|---|
| Moderne condenserende ketel met goede modulatie | Continu |
| Ketel met hoge minimale modulatie (>30%) | PWM |
| Vloerverwarming (trage respons) | Continu |
| Oversized ketel met radiatoren | PWM |
| Niet zeker | Begin met Continu, automatisch wisselen aan |

---

### 5.6 Het veiligheidssysteem

SAT implementeert zes onafhankelijke verdedigingslagen. Bij elke activering van een veiligheidslaag stuurt SAT `CS=0` naar de PIC, waarmee het setpoint-override wordt vrijgegeven en de wandthermostaat de ketel opnieuw direct bestuurt.

**Fundamentele veiligheidsprincipes:**
- **Veilig bij falen:** elke fout resulteert in `CS=0`. De ketel valt terug op de wandthermostaat.
- **Geen automatisch herstel:** SAT blijft uitgeschakeld totdat de gebruiker het expliciet opnieuw inschakelt.
- **Onafhankelijke lagen:** een bug in een laag raakt de andere lagen niet.

**Laag 1: Opstart-veiligheid.** Bij elke opstart, herstart of stroomuitval stuurt SAT als eerste handeling `CS=0` naar de PIC. Zo wordt voorkomen dat de ketel onbeperkt op het laatste setpoint blijft draaien terwijl de ESP nog opstart.

**Laag 2: Uitschakel-veiligheid.** Wanneer SAT wordt uitgeschakeld (via instellingen, MQTT of REST API), wordt onmiddellijk `CS=0` gestuurd, de PID wordt gereset en alle toestandsvariabelen worden gewist.

**Laag 3: Verlopen sensorwaarden.** Externe ruimtetemperatuur: vervalt na 5 minuten zonder update. Externe buitentemperatuur: vervalt na 10 minuten. BLE-sensorwaarden: vervallen na 5 minuten. Bij vervallen valt SAT automatisch terug op de waarden van de OpenTherm-bus (zonder alarm). Als ook die niet beschikbaar zijn, activeert Laag 5.

**Laag 4: Harde temperatuurgrens.** Na elke berekening wordt het setpoint hard begrensd: vloerverwarming max 50 C, radiatoren max 80 C, globaal veiligheidsplafond instelbaar (standaard 65 C). Minimum: 10 C.

**Laag 5: Aaneengesloten sensor-mislukkingen.** Als de ruimtetemperatuur 10 opeenvolgende keren ongeldig of ontbrekend is, activeert SAT een veiligheidsafsluiting. Elke geldige meting reset de teller.

**Laag 6: PIC-communicatiefout.** Als de PIC-co-processor 5 opeenvolgende regelcycli niet bereikbaar is, activeert SAT een veiligheidsafsluiting. Een geslaagde `CS=`-opdracht reset de teller.

#### Herstel na veiligheidsafsluiting

1. Het `safety_tripped`-veld is zichtbaar in de webinterface (rode badge), MQTT (`sat/safety_tripped`) en REST API.
2. SAT blijft inactief totdat de gebruiker hem expliciet opnieuw inschakelt.
3. Herstel via webinterface: SAT-schakelaar uit en opnieuw aan.
4. Herstel via MQTT: publiceer `true` naar `OTGW/set/{UniqueId}/sat/enabled`.

---

### 5.7 Simulatiemodus

SAT beschikt over een simulatiemodus voor testen zonder een echte ketel. In simulatiemodus:
- Berekent SAT alle setpoints en PID-waarden normaal.
- Stuurt SAT de opdrachten **niet** naar de PIC.
- Zijn alle MQTT-topics en REST API-waarden beschikbaar met gesimuleerde data.
- Reageert de gesimuleerde ruimtetemperatuur op het berekende setpoint met instelbare verwarmings- en afkoelsnelheden (`fSimHeatRate`, `fSimCoolRate`).

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

### 5.8 Weerintegratie (Open-Meteo)

SAT kan buitentemperatuur, luchtvochtigheid, windsnelheid en een 24-uurs uurtemperatuurverwachting ophalen via de Open-Meteo API. Dit is een gratis API zonder API-sleutel.

Weerdata levert:
- Buitentemperatuur voor de verwarmingscurve (als terugval of primaire bron).
- Luchtvochtigheiddata voor thermisch-comfortaanpassing.
- Zonne-elevatiedata voor solar gain gating.
- Voorspellingsdata voor proactieve setpoint-planning.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satweatherenable` | `false` | bool | Weerdata ophalen inschakelen |
| `satweatherlat` | `0.0` | -90 tot 90 | Breedtegraad (via browserlocatie of handmatig) |
| `satweatherlon` | `0.0` | -180 tot 180 | Lengtegraad |
| `satweatherinterval` | `900` | 300-3600 s | Poll-interval (standaard 15 min, minimum 5 min) |

De webinterface kan uw locatie automatisch detecteren via de browser. Weerdata wordt opgehaald via plain HTTP (geen HTTPS).

---

### 5.9 Solar Gain Compensatie

Solar gain compensatie detecteert wanneer zonlicht door de ramen de kamer verwarmt en verlaagt het aanvoertemperatuur-setpoint om oververhitting te voorkomen.

**Hoe het werkt:**
1. SAT volgt de stijgsnelheid van de binnentemperatuur met een EMA (exponentieel voortschrijdend gemiddelde).
2. Wanneer de stijgsnelheid het ingestelde minimum overschrijdt en de ketelmodulatie laag is (<20%), wordt een solar gain conditie gedetecteerd.
3. Als zonne-elevatiedata beschikbaar is (via de weather API), controleert SAT ook of de zon boven de minimale elevatie staat.
4. De conditie moet 10 minuten aanhouden voordat activering plaatsvindt (om valse triggers te voorkomen).
5. Na activering wordt het setpoint verlaagd met `fSolarSetpointOffset` (standaard 2 C).
6. Wanneer de stijgsnelheid 10 minuten onder de drempel blijft, wordt solar gain gewist.
7. Optioneel wordt de PID integral term bevroren tijdens solar gain om integral windup te voorkomen.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satsolargain` | `false` | bool | Solar gain compensatie inschakelen |
| `satsolarminrise` | `0.5` | 0.1-5.0 C/uur | Minimale stijgsnelheid om te activeren |
| `satsolaroffset` | `2.0` | 0.5-5.0 C | Setpoint-verlaging tijdens solar gain |
| `satsolarminelevation` | `12.0` | 0-90 graden | Minimale zonne-elevatie voor activering |
| `satsolarfreezeintegral` | `true` | bool | PID integral bevriezen tijdens solar gain |

---

### 5.10 Cyclusbewaking

SAT volgt vlam aan/uit-overgangen en classificeert elke voltooide verwarmingscyclus. Dit stuurt automatische PWM/continu-moduswisseling aan en levert diagnostische data.

**Verzamelde metrics per cyclus:**
- Duur (seconden)
- Maximale aanvoertemperatuur
- Overshoot-tijd (seconden boven setpoint + marge)
- Aanvoer-setpoint-fout
- Aanvoer-retour temperatuurverschil (gemiddeld)

**Cyclusclassificatie:**
- NORMAL: gezonde cyclus met passende duur en geen langdurige overshoot.
- SHORT_CYCLING: cyclus korter dan 60 seconden (ketel kan niet laag genoeg moduleren).
- OVERSHOOT: aanvoertemperatuur overschreed setpoint + overshoot-marge langer dan 60 seconden.
- UNDERHEAT: aanvoertemperatuur bleef meer dan 2 C onder het setpoint langer dan 180 seconden.

**Rolling statistieken:**
- Cycli per uur teller (rollend 60-minuten venster, max 6).
- 4-uur rollend venster met per-cyclus records.
- EMA-afgevlakte fracties: duty ratio, overshoot-fractie, underheat-fractie, lange-cyclus-fractie.

**Heating Curve Recommendation (HCR):**
SAT verzamelt intra-day foutmonsters (het verschil tussen de ruimtetemperatuur en het doel) en berekent een dagelijkse mediaan. Met een rollend venster van maximaal 30 dagelijkse medianen (opgeslagen op LittleFS in `/sat/sat_hcr.json`) publiceert SAT een aanbeveling:

| Conditie | Aanbeveling |
|---|---|
| Dagelijkse mediaan fout > +0,5 C gedurende 3+ opeenvolgende dagen | DECREASE (kamer consequent te warm) |
| Dagelijkse mediaan fout < -0,5 C gedurende 3+ opeenvolgende dagen | INCREASE (kamer consequent te koud) |
| Anders | HOLD (curve is goed afgestemd) |

De aanbeveling wordt gepubliceerd via MQTT als `sat/curve_recommendation` en getoond in de webinterface. De data overleeft herstarts.

---

### 5.11 Drukbewaking

SAT bewaakt de CV-waterdruk van de ketel (OT MsgID 18) en genereert alarmen wanneer de druk buiten het ingestelde bereik valt.

**Kenmerken:**
- EMA-afgevlakte druklezing om ruis te filteren.
- Lineaire regressie voor detectie van drukvaldebiet (bar/uur).
- 120 seconden bevestigingsvenster voordat het alarm actief wordt (voorkomt valse triggers door korte transienten).

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satminpressure` | `0.8` | 0-5 bar | Drempel voor lage-drukalarm |
| `satmaxpressure` | `2.5` | 0-5 bar | Drempel voor hoge-drukalarm |
| `satmaxpressuredrop` | `0.3` | 0-2 bar/uur | Maximale drukdalingssnelheid |

**MQTT-topics:**
- `sat/ch_pressure`: actuele ruwe OT MsgID 18 lezing (bar).
- `sat/ch_pressure_status`: `ok`, `low`, of `high`.

---

### 5.12 Zomeronderdrukking (Summer Simmer)

Indien ingeschakeld, schakelt SAT automatisch de verwarming uit wanneer de buitentemperatuur gedurende een instelbaar aantal uren boven een drempel blijft. Dit voorkomt dat de ketel onnodig draait bij warm weer.

**Hoe het werkt:**
1. SAT controleert de buitentemperatuur elke 5 minuten.
2. Wanneer de buitentemperatuur op of boven de drempel blijft, worden uren opgeteld.
3. Wanneer het opgetelde aantal uren het minimum bereikt, activeert de zomermodus en wordt verwarming onderdrukt.
4. De zomermodus deactiveert wanneer de buitentemperatuur onder `drempel - 2 C` daalt en de opgetelde uren weer tot nul aflopen.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satsummersimmer` | `false` | bool | Automatische zomeruitschakeling inschakelen |
| `satsummerthreshold` | `18.0` | 5-35 C | Buitentemperatuur drempel |
| `satsummerminhours` | `6` | 1-48 uur | Aaneengesloten uren boven drempel om te activeren |

**Summer Simmer Index:** SAT berekent ook een thermisch-comfortindex op basis van de Summer Simmer-formule (binnentemperatuur en luchtvochtigheid). Deze index wordt optioneel als PID-invoer gebruikt in plaats van de ruwe ruimtetemperatuur wanneer `satthermalcomfort` is ingeschakeld, wat vochtigheidsgebaseerde temperatuurregeling biedt.

---

### 5.13 Multi-area temperatuur

SAT ondersteunt tot 4 temperatuurinvoerzones met configureerbare gewichten. De effectieve ruimtetemperatuur die door de PID-controller wordt gebruikt, is het gewogen gemiddelde van alle geldige, niet-verlopen area-temperaturen.

Zones worden aangeboden via MQTT:
```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/area/0" -m "21.5"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/area/1" -m "20.8"
```

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satmultiarea` | `false` | bool | Multi-area gewogen temperatuur inschakelen |
| `satmultiareacount` | `0` | 0-4 | Aantal geconfigureerde zones |
| `satareaweight0..3` | `1.0` | 0-10 | Gewicht per zone |

Elke area-waarde vervalt na 5 minuten zonder update. Verlopen zones worden uitgesloten van het gewogen gemiddelde. Als alle zones verlopen zijn, valt SAT terug op de primaire ruimtetemperatuurbron.

---

### 5.14 Multi-zone PID-regeling

Voor woningen met meerdere onafhankelijk geregelde verwarmingszones (bijv. aparte radiatorcircuits of zoneafsluiters) ondersteunt SAT tot 4 PID-zones. Elke zone voert zijn eigen PID-berekening uit, en het hoogste (meest veeleisende) setpoint wint.

Zones ontvangen hun temperatuur en doel via MQTT. Zone 1 komt altijd overeen met de primaire SAT-doeltemperatuur. Zones 2-4 ontvangen onafhankelijke doelen en temperatuurinvoer.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satzonecount` | `1` | 1-4 | Aantal actieve verwarmingszones |
| `satzonetimeout` | `300` | 60-3600 s | Seconden zonder update voordat een zone inactief wordt |

Wanneer `satzonecount` 1 is (standaard), werkt SAT in single-zone modus en heeft deze functie geen overhead.

---

### 5.15 BLE-temperatuursensor (alleen ESP32)

Op ESP32-builds kan SAT BLE-temperatuursensoren scannen en als ruimtetemperatuurinvoer gebruiken. Ondersteunde sensorformaten:

- **ATC/pvvx custom firmware** (Xiaomi LYWSD03MMC met custom firmware): service data UUID 0x181A. Rapporteert temperatuur, luchtvochtigheid en batterijniveau.
- **BTHome v2**: service data UUID 0xFCD2. Standaard BTHome-protocol voor temperatuur- en luchtvochtigheidssensoren.

Maximaal 4 BLE-sensoren worden gelijktijdig gevolgd.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satbleenable` | `false` | bool | BLE-temperatuursensor scanning inschakelen |
| `satblemac` | `""` | MAC-adres | Koppelen aan specifieke sensor (leeg = alles accepteren) |
| `satbleinterval` | `30` | 10-300 s | Scaninterval |

BLE-temperatuurwaarden vervallen na 5 minuten zonder nieuw advertisement. SAT valt terug op MQTT- of OT-buswaarden wanneer BLE-data verlopen is.

---

### 5.16 Thermisch comfort (vochtigheidscorrectie)

SAT kan de effectieve doeltemperatuur aanpassen op basis van binnenlucht-vochtigheid. Bij hoge luchtvochtigheid voelt de lucht warmer aan, dus kan het doel licht worden verlaagd. Bij lage luchtvochtigheid wordt het doel licht verhoogd.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satcomfortadjust` | `false` | bool | Vochtigheidsgebaseerde doelaanpassing inschakelen |
| `satcomforthumidity` | `50.0` | 20-80 % | Referentievochtigheid (geen aanpassing bij dit niveau) |
| `satcomfortmaxoffset` | `1.0` | 0-3 C | Maximale doeltemperatuur-aanpassing |

Vochtigheidsdata komt van: BLE-sensor, MQTT-push, of weather API fallback. Waarden vervallen na 30 minuten (`iHumidityTimeoutS`).

---

### 5.17 Raamdetectie

SAT kan open ramen detecteren via MQTT-invoer en de verwarming verlagen naar de Activity preset-temperatuur zolang het raam open is.

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/window" -m "open"
```

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satwindowdetection` | `false` | bool | Raamdetectie inschakelen |
| `satwindowminopentime` | `60` | 10-600 s | Minimum seconden dat raam open moet zijn voordat actie volgt |

---

### 5.18 Warmwater (DHW) regeling

SAT kan het warmwater-setpoint regelen in standalone of fallback mode.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satdhwenabled` | `false` | bool | Warmwaterregeling inschakelen |
| `satdhwsetpoint` | `0.0` | 30-60 C | Warmwater-setpoint (0 = inactief) |

---

### 5.19 Vermogen- en energieregistratie

SAT schat het ketelvermogen en houdt het cumulatieve energieverbruik bij.

| Parameter | Standaard | Bereik | Omschrijving |
|---|---|---|---|
| `satboilercapacity` | `24.0` | 1-100 kW | Ketel thermisch vermogen voor vermogensberekening |
| `satboilerratedkw` | `0.0` | 0-100 kW | Nominaal ingangsvermogen ketel (0 = uitgeschakeld) |
| `satboilerefficiency` | `0.92` | 0-1.0 | Ketelrendement |

Gepubliceerd via MQTT als `sat/power` (W) en `sat/energy_total` (kWh).

---

### 5.20 Home Assistant-integratie

#### Klimaatentiteit

Wanneer SAT is ingeschakeld en MQTT met auto-discovery is geconfigureerd, maakt Home Assistant automatisch een klimaatentiteit aan:

| Entiteit | Beschrijving |
|---|---|
| `climate.otgw_sat` | Volledige klimaatentiteit met doeltemperatuur en modus |

Via deze entiteit kunt u:
- De doeltemperatuur instellen (slider of numeriek invoerveld)
- De bedrijfsmodus selecteren (`off`, `continuous`, `pwm`)
- Een voorinstelling kiezen (comfort, eco, weg, slaap, thuis, activiteit) indien geconfigureerd
- De huidige ruimtetemperatuur aflezen
- De actuele SAT-modus aflezen (`off`, `continuous`, `pwm`)

#### SAT-sensorentiteiten

SAT publiceert ook afzonderlijke diagnostische entiteiten via auto-discovery:

| Entiteit | Topic-suffix | Omschrijving |
|---|---|---|
| `sensor.otgw_sat_setpoint` | `sat/setpoint` | Definitief aanvoertemperatuur-setpoint naar de ketel (C) |
| `sensor.otgw_sat_heating_curve` | `sat/heating_curve` | Berekende waarde van de verwarmingscurve (C) |
| `sensor.otgw_sat_pid_output` | `sat/pid_output` | PID-uitvoer (curve + P + I + D) (C) |
| `sensor.otgw_sat_error` | `sat/error` | PID-fout (doeltemp minus ruimtetemp) (C) |
| `sensor.otgw_sat_mode` | `sat/mode` | Bedrijfsmodus: `off`, `continuous` of `pwm` |
| `sensor.otgw_sat_boiler_status` | `sat/boiler_status` | Ketelstatus (tekstlabel) |
| `sensor.otgw_sat_room_temp` | `sat/room_temp` | Ruimtetemperatuur gebruikt door PID (C) |
| `sensor.otgw_sat_outside_temp` | `sat/outside_temp` | Buitentemperatuur gebruikt door de verwarmingscurve (C) |
| `sensor.otgw_sat_pwm_duty` | `sat/pwm_duty` | PWM-duty-cycle (0-1) |
| `sensor.otgw_sat_power` | `sat/power` | Geschat ketelvermogen (W) |
| `sensor.otgw_sat_energy_total` | `sat/energy_total` | Gecumuleerde energieschatting (kWh) |
| `sensor.otgw_sat_ch_pressure` | `sat/ch_pressure` | CV-waterdruk (bar) |
| `sensor.otgw_sat_ch_pressure_status` | `sat/ch_pressure_status` | Drukstatus: ok, low of high |
| `sensor.otgw_sat_curve_recommendation` | `sat/curve_recommendation` | Verwarmingscurve-aanbeveling: INCREASE, DECREASE of HOLD |
| `binary_sensor.otgw_sat_safety_tripped` | `sat/safety_tripped` | Of een veiligheidslaag is geactiveerd |
| `binary_sensor.otgw_sat_modulation_reliable` | `sat/modulation_reliable` | Of modulatiefeedback van de ketel betrouwbaar is |
| `binary_sensor.otgw_sat_setpoint_mismatch` | `sat/setpoint_mismatch` | Setpoint-mismatch tussen SAT en ketel |
| `binary_sensor.otgw_sat_thermal_model_valid` | `sat/thermal_model_valid` | Of het thermisch model voldoende data heeft |
| `binary_sensor.otgw_sat_solar_gain` | `sat/solar_gain` | Solar gain compensatie actief |
| `binary_sensor.otgw_sat_auto_tune_active` | `sat/auto_tune_active` | Auto-tune bezig |
| `binary_sensor.otgw_sat_summer_active` | `sat/summer_active` | Zomeronderdrukking actief |

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

De externe temperatuurwaarden vervallen automatisch na respectievelijk 5 minuten (binnentemp) en 10 minuten (buitentemp), zodat SAT terugvalt op de OpenTherm-buswaarden als de automatisering stopt.

---

### 5.21 Bekende beperkingen

- Niet alle thermostaten sturen ruimtetemperatuur via OT MsgID 24. Als de uwe dat niet doet, gebruik dan een externe sensor via MQTT, REST API, of BLE (ESP32).
- Buitentemperatuur (MsgID 27) wordt zelden via de bus uitgewisseld. Een externe push, weather API fetch, of REST API push is doorgaans nodig.
- BLE-temperatuursensoren zijn alleen beschikbaar op de ESP32 (OTGW32).
- SAT bestuurt het aanvoertemperatuur-setpoint, maar de wandthermostaat bepaalt nog steeds of de verwarming is ingeschakeld of uitgeschakeld.
- Multi-zone ondersteuning draait onafhankelijke PID-lussen per zone, maar SAT bestuurt een enkele ketel. Afzonderlijke ketelcircuits per zone worden niet ondersteund.
- De PIC co-processor houdt het laatste `CS=`-setpoint vast als de ESP uitvalt. Laag 1 (opstart-veiligheid) corrigeert dit bij de volgende start.
- Weerdata wordt opgehaald via plain HTTP. Er wordt geen HTTPS gebruikt.

---

### 5.22 Probleemoplossing

| Symptoom | Mogelijke oorzaak | Oplossing |
|---|---|---|
| Woning wordt niet warm genoeg | Coefficient te laag | Verhoog de coefficient stapsgewijs met 0,2. Controleer `sat/curve_recommendation`. |
| Ketel schakelt te vaak aan en uit | Coefficient te hoog of PWM-modus nodig | Verlaag de coefficient of schakel over op PWM. Verlaag `satcyclesperhour`. |
| SAT stuurt geen setpoints | PIC niet bereikbaar | Controleer de seriele verbinding; controleer `otgw-pic/picavailable` via MQTT. |
| Veiligheidslaag activeert herhaaldelijk | Ruimtetemperatuur ontbreekt | Controleer MQTT-push of OpenTherm ID 24. |
| SAT schakelt zichzelf uit | Veiligheidslaag 5 of 6 | Controleer sensorgeldigheid en PIC-communicatie; zie `sat/safety_tripped`. |
| Temperatuur schommelt sterk | Deadband te smal of slechte ketelmodulatie | Vergroot de deadband of gebruik PWM. |
| Setpoint stijgt niet ver genoeg bij vriesweer | Coefficient te laag | Verhoog coefficient; controleer buitentemperatuurmeting. |
| Home Assistant klimaatentiteit verschijnt niet | Auto-discovery uitgeschakeld of MQTT niet verbonden | Controleer MQTT-verbinding en HA-discovery instelling. |
| Setpoint altijd op minimum (10 C) | SAT actief maar veiligheidslaag getripped | Herstart SAT via webinterface of MQTT. Controleer `sat/safety_tripped`. |
| SAT-tab niet zichtbaar in webinterface | SAT nog niet ingeschakeld | SAT inschakelen in instellingen. |
| Drukalarm blijft triggeren | Drukbereik te smal of ketelvulprobleem | Pas `satminpressure`/`satmaxpressure` aan. Controleer de drukmanometer van de ketel. |
| Solar gain activeert nooit | Stijgsnelheidsdrempel te hoog of zonne-elevatie ontbreekt | Verlaag `satsolarminrise`. Schakel weather API in voor zonne-elevatie. |
| Zomermodus activeert niet | Drempel te hoog of uren te lang | Verlaag `satsummerthreshold` of `satsummerminhours`. |
