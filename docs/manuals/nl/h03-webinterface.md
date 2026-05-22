## Hoofdstuk 3: De Webinterface

### Overzicht

De webinterface van OTGW-firmware is een single-page application (SPA) die door het apparaat zelf wordt geserveerd. U heeft geen app, geen cloud en geen account nodig. Open een browser en navigeer naar `http://otgw.local/` of het IP-adres van uw apparaat.

De interface werkt in de huidige plus twee voorgaande major-versies van Google Chrome, Mozilla Firefox en Apple Safari. Andere browsers worden niet officieel ondersteund.

**Let op:** op de 2.0.0-lijn is de webinterface Engelstalig. De resterende Nederlandse UI-strings die uit het OTTHING-platform waren overgenomen, zijn verwijderd (TASK-569). Deze handleiding blijft Nederlands; de schermteksten waarnaar verwezen wordt, zijn in het Engels.

De SPA bestaat uit de bestanden `index.html`, `index.js`, `sat.js`, `graph.js`, `sat-slider.js`, `theme-toggle.js`, `echarts-theme.js`, en de design-system CSS (`ds-tokens.css`, `components.css`), allemaal opgeslagen op het LittleFS-bestandssysteem van het apparaat. ECharts wordt vanaf het jsDelivr CDN geladen met SRI-hash. Communicatie met de firmware verloopt via de REST API (JSON over HTTP) voor instellingen en opdrachten, en via WebSocket voor de realtime OpenTherm-log.

Een persistent banner bovenaan elke pagina waarschuwt zodra de firmware-buildhash niet overeenkomt met de hash van het geflashte LittleFS-image, met een directe link naar de flash utility.

---

### Navigatiebalk en tabbladen

De navigatiebalk bovenaan de pagina bevat de volgende tabbladen:

| Tabblad | Functie |
|---|---|
| **Home** | Startpagina met live OpenTherm-log, OT-Direct status (OTGW32) en temperatuurgrafieken |
| **SAT** | SAT-thermostaat dashboard: temperatuurkaarten, presets, stooklijn, PID-diagnostiek |
| **Settings** | Alle apparaat- en netwerkinstellingen, inclusief nachtelijke herstart |
| **Advanced** | Uitklapmenu met PIC firmware, Webhook, Debug Information en File system contents (FSexplorer) |

Naast de tabbladen toont de header rechts de Wi-Fi-signaalbalkjes (of Ethernet-icoon), het heap-geheugen, de huidige tijd, de hostnaam en versie van het apparaat, en een zon/maan-knop voor het lichte of donkere thema.

---

### Startpagina: live OpenTherm-log

#### De log-stream

Het centrale element van de startpagina is de live OpenTherm-log. Elk bericht dat via de OpenTherm-bus wordt uitgewisseld tussen thermostaat en ketel, wordt in realtime weergegeven. De log wordt via een gewone WebSocket-verbinding gestreamd (`ws://<apparaat>/ws` op poort 80); er is geen herladen van de pagina nodig.

> **Alleen HTTP/WS**: De firmware ondersteunt geen HTTPS of WSS. Dit is een bewuste keuze voor een apparaat dat op een vertrouwd thuisnetwerk draait met beperkte ESP8266-resources. De REST API kan achter een HTTPS-reverse proxy gezet worden, maar de live OpenTherm-log gaat uit van een directe `ws://`-verbinding en werkt niet via een HTTPS-proxy die `ws://` niet naar `wss://` overbrugt. Gebruik een VPN voor toegang van buitenaf.

#### Berichtstructuur

Elk logbericht heeft de volgende opbouw:

```
[tijdstip] [richting] [bericht-ID] [omschrijving]: [waarde]
```

Voorbeelden:
```
12:34:56 T  ID=0  Status: CH=ON DHW=OFF Flame=ON
12:34:57 B  ID=25 Boiler flow temperature: 45.50 °C
12:34:58 T  ID=1  Control Setpoint: 45.00 °C
```

- **T** = bericht van de thermostaat naar de ketel (thermostat to boiler)
- **B** = bericht van de ketel naar de thermostaat (boiler to thermostat)
- **A** = beantwoord bericht (answer, verwerkt door de OTGW)

Het bericht-ID correspondeert met het OpenTherm-protocol. Alle 80+ gedefinieerde bericht-ID's worden herkend en worden weergegeven met een beschrijvende naam en de gedecodeerde waarde.

#### Filtermogelijkheden

Boven de loglijst staat een werkbalk. U kunt:

- Op tekst filteren: typ een woord of waarde om alleen overeenkomende regels te tonen
- Op bericht-ID filteren: voer een numeriek ID in
- Auto-scroll aan- of uitzetten en zelf scrollen om automatisch te pauzeren
- Tijdstempels tonen of verbergen
- **Capture**-modus inschakelen voor een grotere in-memory buffer (tot ongeveer 1 miljoen regels)
- **Stream**-modus inschakelen om de log live naar een lokaal bestand te schrijven (alleen Chrome/Edge)
- De zichtbare log handmatig downloaden of via **Auto** elke 15 minuten automatisch laten opslaan

#### Sub-tabbladen Log, Statistics en Graph

De OpenTherm Monitor heeft drie sub-tabbladen:

- **Log**: de live gestreamde berichten zoals hierboven beschreven.
- **Statistics**: een sorteerbare tabel per bericht-ID met hexadecimaal en decimaal ID, richting, omschrijving, interval (s) en laatst gemeten waarde.
- **Graph**: een ECharts-tijdreeksgrafiek met instelbaar tijdvenster (10 minuten tot 24 uur), losse knoppen voor **Screenshot** en **Export Data** (CSV), plus bijbehorende **Auto-save PNG**- en **Auto-save CSV**-checkboxen voor periodieke export.

#### Commandobalk

Onder de log bevindt zich een commandobalk waarmee u rechtstreeks opdrachten kunt versturen (bijv. `TT=20.5`, `GW=R`). Op ESP8266 zijn dit PIC-commando's. Op OTGW32 is de commandobalk ook beschikbaar wanneer OT-Direct de command interface levert.

---

### OT-Direct statuspaneel (alleen OTGW32)

Op OTGW32-apparaten met OT-Direct verschijnt er een statuspaneel boven de OpenTherm-log op de startpagina. Dit paneel toont:

- **Mode**: de huidige OT-Direct bedrijfsmodus (Bypass, Gateway, Monitor, Master/Standalone of Loopback Test).
- **OT Bus**: of de OpenTherm-bus online is, met een statusindicator.
- **Thermostat**: of er een thermostaat op de bus is gedetecteerd.
- **Boiler**: of de ketel reageert.
- **Setback / Step-Up**: of setback- of step-up-overrides actief zijn.
- **Schedule**: aantal actieve schema's.
- **Active Overrides**: aantal actieve bus-overrides. Klik om de lijst uit te vouwen.

Het uitgevouwen override-gedeelte biedt de mogelijkheid om nieuwe overrides rechtstreeks vanuit de browser toe te passen. Selecteer een actie (SR, CR, RM, CM, UI, KI), voer het bericht-ID en optioneel een hex-waarde in en klik op **Apply**. Dit is een geavanceerde functie bedoeld voor diagnostiek en testen.

Het paneel pollt `/api/v2/otdirect/status` elke 5 seconden zolang het Home-tabblad zichtbaar is.

---

### Startpagina: realtime temperatuurgrafieken

Onder de OpenTherm-log toont de startpagina een temperatuurgrafiek op basis van Apache ECharts. De grafiek heeft tot vijf Y-assen en toont een rollend venster van 24 uur.

**Standaard weergegeven temperatuurreeksen:**

| Reeks | Bron |
|---|---|
| Aanvoertemperatuur (CH flow) | OpenTherm Msg ID 25 |
| Retourtemperatuur (CH return) | OpenTherm Msg ID 28 |
| Warmwatertemperatuur (DHW) | OpenTherm ID 26 of 27 |
| Ruimtetemperatuur (room) | OpenTherm ID 24 of SAT-invoer |
| Buitentemperatuur (outside) | OpenTherm ID 27 of SAT-weerdata |
| Dallas sensoren (optioneel) | DS18B20 1-Wire sensoren |

De grafiek schaalt de Y-assen automatisch op de aanwezige data. U kunt op een legenda-item klikken om een reeks te verbergen. De grafiek past zich aan het licht of donker thema aan.

Temperatuurdata wordt opgeslagen in een clientzijdig rolbuffer van 24 uur. Bij het openen van de pagina laadt de webinterface de historische data via de REST API.

---

### SAT-tabblad

Het SAT-tabblad (Smart Autotune Thermostat) biedt een eigen dashboard voor de ingebouwde thermostaatfunctionaliteit. Het tabblad is altijd zichtbaar in de navigatiebalk. Wanneer SAT is uitgeschakeld, toont het dashboard de huidige status en een inschakelschakelaar.

#### Header en bediening

De SAT-header bevat:

- **Statusbadge**: toont "Disabled", "Idle", "Heating" of de huidige SAT-status.
- **Simulatiebadge**: wordt getoond wanneer de SAT-simulatiemodus actief is.
- **Inschakelschakelaar**: een schakelaar om SAT in of uit te schakelen zonder naar de instellingen te navigeren.
- **Weergaveselector**: kies tussen Thermostat (eenvoudig), Expert en Diagnostics. Elke weergave toont progressief meer detail.
- **Instellingenknop**: navigeert naar de SAT Settings pagina voor volledige configuratie.

#### Temperatuurkaarten

Vier temperatuurkaarten zijn altijd zichtbaar in alle weergaven:

| Kaart | Beschrijving |
|---|---|
| Room Temperature | Huidige kamertemperatuur |
| Target Temperature | Gewenst setpoint, met +/- knoppen om in stappen van 0,5 graden bij te stellen |
| Outside Temperature | Buitentemperatuur via OT-bus of weerdienst |
| Boiler Setpoint | Huidig berekend aanvoertemperatuur-setpoint |

#### Presets en modi

Een rij preset-knoppen (Activity, Away, Eco, Home, Comfort, Sleep) schakelt temperatuurpresets met een enkele klik. Daaronder selecteren modusknoppen tussen Continuous en PWM verwarmingsmodus. Er is ook een simulatieschakelaar beschikbaar.

#### Expert- en Diagnostics-weergaven

De Expert-weergave voegt secties toe voor:

- **Weerdata**: buitentemperatuur, luchtvochtigheid, windsnelheid via Open-Meteo (indien ingeschakeld).
- **Temperatuurhistorie**: een ECharts-grafiek binnen het SAT-dashboard.
- **Stooklijn (Heating Curve)**: een inklapbare grafiek met de berekende stooklijn.
- **Control status**: huidige modus, ketelstatus, actieve preset, verwarmingssysteemtype, PID-output, fout, coefficient, deadband, overshoot margin, modulatiewaarden en OVP-kalibratie.
- **PID-controller**: individuele P-, I-, D-termen en afstemparameters (Kp, Ki, Kd).
- **PWM en cyclustracking**: duty cycle, vlamstatus, cyclusaantal, overshoot-data.
- **Slimme functies**: solar gain, zomermodus, thermisch leren, comfort offset, simmer index, auto-tune status.
- **Externe sensoren**: statusindicatoren voor binnen- en buitensensorbronnen.

De Diagnostics-weergave voegt toe:

- **Health indicators**: gekleurde stippen voor Device, Cycle, Flame, Pressure, Setpoint Sync en Modulation Sync.
- **Simulatie en diagnostiek**: details over simulatiemodus, fallback-status, OVP-fase en -waarde.
- **Raw data**: een inklapbare sectie met de ruwe JSON-data van de SAT API.

#### SAT Settings pagina

Door op de knop **Settings** in de SAT-header te klikken, opent u een aparte SAT Settings pagina. Instellingen zijn georganiseerd in inklapbare groepen: Thermostat, Heating, PID, DHW, Pressure, Smart Features, Safety, Energy, Weather, Sync en Advanced. Elke groep heeft een eigen **Save**-knop om wijzigingen afzonderlijk op te slaan.

Boven de algemene groepen verschijnen twee extra panelen op ESP32-builds:

- **BLE Sensors**: lijst van Bluetooth Low Energy-sensoren die de firmware heeft ontdekt (Xiaomi LYWSD03MMC met ATC/pvvx custom firmware, BTHome v2). Elke rij toont het MAC-adres, een bewerkbare naam, hoe oud de laatste advertisement is, en de acties **Select** en **Forget**. De roster is gemaximeerd op acht items en wordt ook via MQTT naar Home Assistant gepubliceerd. Het veld `SATblemac` onderaan is alleen contextuele weergave: de actieve sensor wordt in dit paneel gekozen in plaats van met de hand getypt (TASK-508).
- **DS18B20 Area Sensor Mapping**: koppelt elk SAT-zonegebied via een dropdown aan een gedetecteerde DS18B20-sensor. De gekozen mapping wordt elke pollcyclus naar SAT doorgegeven, zodat een bekabelde sensor als bron voor de kamertemperatuur van een specifiek gebied kan dienen.

#### DHW-bediening

Een DHW-sectie (Domestic Hot Water) is zichtbaar op alle SAT-weergaven. Deze biedt een schuifregelaar om de warmwatertemperatuur in te stellen (40-60 graden C) en een optionele forceerknop om handmatig warmwaterverwarming te activeren.

---

### Netwerkstatusindicator

Rechtsboven in de navigatiebalk staat een netwerkindicator die de huidige verbindingsstatus weergeeft:

| Indicator | Betekenis |
|---|---|
| Wi-Fi balkjes (1-4 gevuld) | Verbonden via Wi-Fi, signaalsterkte gebaseerd op RSSI met kwadratische mapping |
| Wi-Fi balkjes (leeg) | Wi-Fi verbonden maar zwak signaal |
| "AP" badge | Apparaat draait als accesspoint (fallbackmodus) |
| Ethernet-icoon | Verbonden via bekabeld Ethernet (alleen OTGW32), vervangt het Wi-Fi-icoon |
| Rood kruis | Geen netwerkverbinding |

De indicator wordt automatisch bijgewerkt via de periodieke apparaattijd-poll (`/api/v2/device/time`) en bij het laden van de pagina via `/api/v2/device/info`. De Wi-Fi-balkjes zijn correct zichtbaar in zowel het lichte als het donkere thema.

---

### Heap-geheugenweergave

De headerbalk toont een heap-geheugenindicator met het huidige vrije heap-geheugen en het grootste vrije blok in bytes (bijvoorbeeld `Heap: (12480 / 8192)`). Deze informatie wordt bijgewerkt bij elke apparaattijd-poll en biedt een snelle manier om de geheugenconditie van het apparaat te monitoren zonder de debug console te openen.

---

### Instellingenpagina

De instellingenpagina is ingedeeld in secties. Alle instellingen worden opgeslagen in `/settings.ini` op de LittleFS-flash en overleven een herstart.

Klik op **Opslaan** onderaan een sectie om de wijzigingen toe te passen. De meeste instellingen zijn onmiddellijk actief; voor sommige (zoals Wi-Fi-instellingen) is een herstart vereist.

#### Wi-Fi sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Hostname | mDNS- en DHCP-hostnaam, bereikbaar als `<hostname>.local` | `otgw` |
| Wi-Fi Network (SSID) | Alleen-lezen weergave van het huidig verbonden Wi-Fi-netwerk | (huidige SSID) |
| Reset WiFi | Knop naast het SSID-veld. Wist de opgeslagen Wi-Fi-gegevens en herstart het apparaat in Access Point (AP) modus | n.v.t. |

Om naar een ander Wi-Fi-netwerk over te schakelen, klikt u op **Reset WiFi** en bevestigt u de dialoog. Het apparaat herstart, start een AP met de naam `OTGW-XXXXXX` (waarbij `XXXXXX` afgeleid is van het chip-ID) en opent opnieuw de captive portal, zodat u tegen het nieuwe netwerk kunt authenticeren. Dezelfde actie is beschikbaar onder **FSexplorer > System Actions** via de knop *Reset Wireless*.

#### Wi-Fi-scan (Settings-pagina)

Onder de Wi-Fi-sectie staat een Wi-Fi-scanpaneel (TASK-585). Met de knop **Scan** wordt `/api/v2/network/scan` aangeroepen; het resultaat vult een dropdown met gedetecteerde SSID's, signaalsterkte en beveiligingsvlag. Een netwerk uit de dropdown selecteren vult automatisch het SSID-veld, zodat een Reset WiFi-handeling daarna direct aansluit op het gekozen netwerk zonder dat de SSID met de hand getypt hoeft te worden.

#### MQTT sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Broker | IP-adres of hostnaam van de MQTT-broker | (leeg) |
| Poort | TCP-poort van de MQTT-broker | `1883` |
| Gebruikersnaam | MQTT-gebruikersnaam (optioneel) | (leeg) |
| Wachtwoord | MQTT-wachtwoord (optioneel) | (leeg) |
| TopTopic | Bovenste MQTT-topic-namespace | `OTGW` |
| UniqueID | Uniek apparaat-ID in de topic-structuur | `otgw-{MAC}` |
| HA Discovery | Home Assistant auto-discovery inschakelen | uitgeschakeld |
| HA Prefix | Prefix voor auto-discovery topics | `homeassistant` |

#### NTP sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| NTP-server | Hostnaam of IP van de NTP-server | `pool.ntp.org` |
| Tijdzone | POSIX-tijdzone string (bijv. `CET-1CEST,M3.5.0,M10.5.0/3`) | (leeg) |

#### Nachtelijke herstart

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Scheduled Nightly Restart | Dagelijkse automatische herstart in- of uitschakelen om heap-geheugen te herstellen | uitgeschakeld |
| Nightly Restart Hour | Lokaal uur (0-23) waarop de herstart plaatsvindt | `4` (04:00) |

De nachtelijke herstart veroorzaakt een korte onderbreking van ongeveer 30 seconden. De functie is alleen actief wanneer NTP is ingeschakeld en gesynchroniseerd, zodat het apparaat de juiste lokale tijd kent. Deze functie is vooral nuttig op ESP8266 waar lange uptimes tot heap-fragmentatie kunnen leiden.

#### Apparaat sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| LED-functies | Functietoewijzing voor de LED-indicatoren op de OTGW | `F/X/O/M/P/C` |
| GPIO-instellingen | GPIO-pinconfiguratie voor relais en S0 | (apparaatspecifiek) |
| Dallas GPIO-pin | GPIO-pin voor de 1-Wire DS18B20 bus | (apparaatspecifiek) |
| OLED | OLED-display inschakelen | uitgeschakeld |

#### OT-Direct modus (alleen OTGW32)

Op OTGW32-apparaten verschijnt een dropdown om de OT-Direct bedrijfsmodus te selecteren:

| Modus | Omschrijving |
|---|---|
| Bypass | Thermostaat direct verbonden met ketel via relais; geen OT-verwerking |
| Gateway | Volledige override-verwerking (standaard) |
| Monitor | Transparante doorgifte; berichten worden gelogd maar niet gewijzigd |
| Master / Standalone | Geen thermostaat nodig; OTGW32 fungeert als enige OT-master |
| Loopback Test | Gesimuleerde keteldata voor testen (geen echte ketelcommunicatie) |

Bij het wijzigen van de modus verschijnt een bevestigingsdialoog die de gevolgen uitlegt voordat de wijziging wordt toegepast.

#### Beveiliging sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Protected Endpoints Password | HTTP Basic Auth wachtwoord voor beveiligde admin-endpoints. De gebruikersnaam is vast `admin` | (leeg = uitgeschakeld) |

Wanneer een wachtwoord is ingesteld, vragen de volgende acties om authenticatie:

- Instellingen lezen en wijzigen
- Bestanden uploaden, downloaden en verwijderen
- Reboot en Reset Wireless
- OTA firmware- en filesystem-updates
- Webhook-test

Live monitoring, sensorwaarden en de WebSocket-stream blijven zonder authenticatie toegankelijk, zodat dashboards en integraties niet stuk gaan.

De 2.0.0-firmware vraagt de HTTP Basic Auth-challenge direct op de root (`/`) op, zodat de browser de credentials in de cache heeft voordat de geladen UI zelf een REST-call doet. Dit voorkomt dat halverwege een sessie alsnog een loginpopup verschijnt. De server verzamelt ook de `Origin`- en `Referer`-headers van elk verzoek, zodat de REST-laag een same-origin CSRF-controle kan afdwingen (ADR-056).

**Opmerking:** De firmware ondersteunt uitsluitend HTTP en WS (geen HTTPS, geen WSS). Stel HTTP Basic Auth alleen in op een vertrouwd thuisnetwerk. Voor externe toegang wordt een VPN aangeraden; een HTTPS-reverse proxy werkt wel voor de REST API, maar de live OpenTherm-log via WebSocket vereist een plain `ws://`-verbinding.

#### Webhook sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Webhook URL | HTTP POST-URL voor uitgaande statusmeldingen | (leeg) |
| Triggerbits | OpenTherm-statusbits die een webhook activeren | (selecteerbaar) |

Webhooks sturen een HTTP POST naar de opgegeven URL wanneer geselecteerde statusbits veranderen (bijv. vlam aan/uit, storing). De payload is JSON. Dit is nuttig voor integratie met Node-RED of andere HTTP-gebaseerde automatiseringen.

---

### OTA firmware-update via de webinterface

#### GitHub release updater

De update-pagina (onder **Geavanceerd > Firmware-update**) haalt automatisch de lijst van beschikbare releases op van GitHub. Naast elke release staat een badge:

| Badge | Betekenis |
|---|---|
| Geinstalleerd | Dit is de huidige versie |
| Update | Nieuwere versie beschikbaar |
| Terugzetten | Oudere versie (downgrade mogelijk) |

Klik op **Update** om de geselecteerde versie te installeren. De firmware:

1. Downloadt de binary via HTTPS van GitHub
2. Schrijft de binary naar het inactieve OTA-slot
3. Herstart het apparaat
4. De pagina pollt `/api/v2/health` totdat het apparaat weer reageert
5. Toont bevestiging met het nieuwe versienummer

**Internetverbinding vereist:** De download van GitHub vereist dat het apparaat internettoegang heeft. Als uw netwerk geen directe internettoegang biedt, gebruik dan handmatig uploaden.

#### Handmatig uploaden

Als alternatief kunt u een firmwarebinary handmatig uploaden:

1. Download het juiste `.bin`-bestand van de GitHub-releasepagina:
   - `firmware-esp8266.bin` voor ESP8266
   - `firmware-esp32.bin` voor ESP32

2. Op de update-pagina, selecteer **Handmatig uploaden**.

3. Sleep het bestand naar het uploadgebied of klik om het te selecteren.

4. Klik op **Uploaden**. De voortgangsbalk geeft de uploadstatus aan.

5. Na upload herstart het apparaat automatisch.

#### PIC-firmware updaten

Op dezelfde pagina kunt u ook de PIC co-processor firmware updaten. Selecteer het `.hex`-bestand voor de PIC en klik op **PIC uploaden**. De firmware flasht de PIC via de seriele verbinding.

**Waarschuwing:** Onderbreek de PIC-firmwareupdate nooit. Als de voeding wegvalt tijdens het flashen, kan de PIC beschadigd raken en is handmatig herprogrammeren via een PIC-programmer noodzakelijk.

---

### Bestandsbeheer (FSexplorer)

De FSexplorer is bereikbaar via de Geavanceerd-tab onder **File system contents**, of rechtstreeks op `http://otgw.local/FSexplorer.html`. De pagina geeft toegang tot het LittleFS-bestandssysteem van het apparaat.

Mogelijkheden:

- Bladeren door bestanden en submappen. Mappen staan bovenaan en de lijst is alfabetisch gesorteerd (hoofdletter-ongevoelig). In een submap verschijnt een `.. (Parent)`-link om terug te navigeren.
- Bestanden uploaden via het **Upload File** formulier. Een voortgangsbalk toont de uploadstatus en de bestandsgrootte wordt vooraf gecontroleerd tegen de beschikbare vrije ruimte.
- Bestanden downloaden via de **Download** link in de rij van een bestand.
- Bestanden verwijderen via de **Delete** link.
- Beschikbare en gebruikte schijfruimte van de LittleFS-partitie bekijken onderaan de lijst.

Onder de bestandenlijst staat een **System Actions**-paneel met snelkoppelingen naar **Update Firmware** (alleen op desktopbrowsers), **ReBoot**, **Reset Wireless** (identieke actie als Reset WiFi op de Settings-pagina) en **Exit FSexplorer** (terug naar de hoofdpagina).

**Let op:** Verwijder geen actieve assets zoals `index.html`, `index.js`, `sat.js`, `graph.js`, `components.css`, `ds-tokens.css` of `/settings.ini`. Dit kan de werking van de webinterface verstoren. Herstel is mogelijk door het LittleFS-bestandssysteem opnieuw te flashen via USB.

Wanneer `index.html` ontbreekt (bijvoorbeeld omdat het LittleFS-image nog niet is geflasht), serveren de FSexplorer-routes `FSexplorer.html` direct als fallback-pagina, zodat u alsnog een filesystem-image kunt uploaden.

De firmware streamt alle bestanden in plaats van ze in RAM te bufferen, dus er is geen harde uploadlimiet anders dan de beschikbare ruimte in de LittleFS-partitie. Het `index.html`-bestand is op 2.0.0 enkele tientallen kilobytes groot en wordt altijd via `streamFile()` geserveerd om de beperkte ESP8266-heap te ontzien (TASK-668).

#### Asset-caching en de ETag-flow

De webinterface gebruikt een ETag-gebaseerde cachingstrategie gecombineerd met versioned URLs voor JavaScript:

- `index.html` wordt geserveerd met de huidige filesystem-hash als `ETag`. De browser cachet de pagina maar revalideert altijd via `If-None-Match`. Een ongewijzigd filesystem antwoordt met `304 Not Modified`; een geflasht filesystem antwoordt met `200` en de nieuwe inhoud.
- `index.js` en `graph.js` worden geladen met een `?v=<fsHash>`-querystring. Aanvragen met de juiste hash krijgen `Cache-Control: public, max-age=86400`; een kale aanvraag zonder `?v=` krijgt `Cache-Control: no-cache`, zodat een verouderde URL nooit oude JavaScript serveert.
- Wanneer een vooraf gegzipte `index.js.gz` of `graph.js.gz` op het LittleFS-image staat, kiest de server deze automatisch en laat `streamFile()` de `Content-Encoding: gzip`-header zetten (TASK-304/TASK-433).

---

### Licht en donker thema

De webinterface ondersteunt een licht en een donker thema. De schakelaar staat rechtsboven in de navigatiebalk (zon/maan-icoon). Uw keuze wordt opgeslagen in de `localStorage` van de browser en blijft behouden bij het opnieuw laden van de pagina.

Het thema past de achtergrond, tekst, grafieken en tabelkleuren aan. De ECharts-temperatuurgrafieken en SAT-grafieken schakelen automatisch mee. Alle UI-elementen, inclusief Wi-Fi-signaalbalkjes, statusindicatoren en het OT-Direct paneel, worden correct weergegeven in beide thema's.
