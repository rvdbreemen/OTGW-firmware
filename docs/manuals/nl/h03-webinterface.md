## Hoofdstuk 3: De Webinterface

### Overzicht

De webinterface van OTGW-firmware is een single-page application (SPA) die door het apparaat zelf wordt geserveerd. U heeft geen app, geen cloud en geen account nodig. Open een browser en navigeer naar `http://otgw2.local/` of het IP-adres van uw apparaat.

De interface werkt in de huidige plus twee voorgaande major-versies van Google Chrome, Mozilla Firefox en Apple Safari. Andere browsers worden niet officieel ondersteund.

**Let op:** op de 2.0.0-lijn is de webinterface Engelstalig. De Nederlandse UI-teksten die er nog in zaten, zijn verwijderd (TASK-569). Deze handleiding blijft Nederlands; de schermteksten waarnaar verwezen wordt, zijn in het Engels.

De SPA wordt als één gzip-bestand (`v2.html.gz`, met stijlen, JavaScript en lettertypen erin) opgeslagen op het LittleFS-bestandssysteem van het apparaat. Communicatie met de firmware verloopt via de REST API (JSON over HTTP) voor instellingen en opdrachten, en via WebSocket voor de realtime OpenTherm-log.

Een persistent banner bovenaan elke pagina waarschuwt zodra de firmware-buildhash niet overeenkomt met de hash van het geflashte LittleFS-image, met een directe link naar de flash utility.

> **Eén UI**: De 2.0.0-firmware levert één web-UI, de **v2 UI**, bereikbaar op `http://<apparaat>/` (en op `/index.html` voor oude bladwijzers). De klassieke UI en de schakelknop **Classic UI** / **New UI** zijn verwijderd in TASK-1223.12, zodat het bestandssysteem in de kleinere LittleFS-partitie van de WiFi-update-indeling past (ADR-191). Welke functies de klassieke UI had en v2 niet, staat in `docs/history/2026-10-15-retired-classic-web-ui-features.md`.

---

### De v2-webinterface

De v2 UI is de webinterface van het apparaat. Ze gebruikt dezelfde REST- en WebSocket-endpoints van de firmware als voorheen, dus u verliest geen configuratie als u bijwerkt vanaf een build die nog de klassieke UI had.

**Bijwerken vanaf een build met de klassieke UI.** U hoeft niets te doen. De oude `ui_usev2`-vlag en de klassieke weergave-voorkeuren in `settings.ini` worden genegeerd en verdwijnen bij de volgende keer dat de instellingen worden opgeslagen.

**Header en navigatie.** De v2-header is een donkere strip in zowel het lichte als het donkere thema. Hij toont identiteits-chips (hostnaam en firmwareversie, plus IP-adres en Wi-Fi-signaalbalkjes), een connectiviteits-status pill, een live connectiviteitssamenvatting, een klok, de themaknop, en een **SIMULATION**-badge die alleen verschijnt zolang OT-replay-simulatie actief is. De hoofdnavigatie is een rij onderstreepte tekst-tabbladen: **Home**, **SAT**, **Monitor**, **Settings** en **Advanced**. De subnavigatie binnen een pagina gebruikt dezelfde onderstreepte tekst-tab-stijl.

#### Home-pagina

De Home-pagina biedt meerdere weergaven via een **View**-keuzemenu bovenaan:

- **System view**: een live schema van het verwarmingssysteem (ketel of warmtepomp, aanvoer- en retourleidingen, de ruimteverwarming, kamer, DHW, druk- en modulatiewaarden). Bij een warmtepomp, of een ketel die een warmwatervoorraadvat meldt, staat in de statusregel "Hot water demand · charging the tank" zolang warm water wordt gemaakt; een gasketel zonder vat toont "Hot water running · tap open".
- **At a glance**: een telefoon-eerst weergave rond een enkele grote kamertemperatuur-dial met stapknoppen.
- **Mission control**: een live strip-grafiek (flow / return / setpoint / modulatie) plus een ruwe OpenTherm-frame-ticker.

De kamertemperatuur in alle drie de weergaven (het System-schema, de At a glance-dial en chipstrook, en de Mission control-cellen) volgt SAT zolang SAT de temperatuur beheert: ze tonen de kamertemperatuur waarop SAT regelt. De eigen waarde van de thermostaat (MsgID 24) staat dan in een tooltip bij de uitlezing in System en At a glance. Is SAT uit, of heeft SAT geen geldige kamertemperatuur, dan tonen ze zoals voorheen MsgID 24 van de thermostaat.

Wanneer de gateway waarden op de OT-bus injecteert (actieve gateway-overrides), verschijnt een "injected"-badge op het Home-schema en op de Connection-kaart; klikken opent een zwevend detailpaneel met de actieve overrides. De badge blijft staan als één verversing van de overrides-lijst mislukt (bijvoorbeeld doordat de gateway bij een uitgeputte heap de verbinding sluit); de volgende verversing beslist.

#### Monitor-pagina

De Monitor-pagina heeft vijf sub-tabbladen:

- **Log**: een live console gevoed door de WebSocket-stream, één OpenTherm-frame per regel (dichte weergave). Een werkbalk bevat Pause, **Clear**, **Download**, een frame-filter en toggle-chips voor Auto-scroll, Timestamps, **SAT only** (alleen SAT-narratieregels tonen), **Auto-download** (log elke 15 minuten opslaan) en **Stream to file** (frames rechtstreeks naar een lokaal bestand schrijven in Chrome/Edge). Onder de werkbalk verstuurt een commandobalk een ruw OTGW-commando (bijvoorbeeld `PS=1`, `TT=20.5`, `GW=1`); het commando echoot naar de log en het promptlabel volgt de live command interface (een `PIC ›`-prompt op PIC-hardware, `OT ›` op OT-Direct).
- **Statistics**: een tabel per bericht-ID (msgID, omschrijving, richting, interval, aantal, waarde) met een zoekveld, gevolgd door een paneel **Active gateway overrides** en een paneel **Boiler unsupported messages**.
- **OT Support**: een matrix van alle 128 OpenTherm-bericht-ID's, kleurgecodeerd naar waar elk ID is gezien (thermostaat + ketel, alleen thermostaat, alleen ketel, of nooit gezien). Klik een cel om een detailpaneel met het gedecodeerde bericht vast te zetten.
- **Graph**: een flow / return / setpoint / modulatie-grafiek die opent met de geschiedenis die de gateway heeft bijgehouden (het laatste uur in stappen van 30 seconden op een bord zonder PSRAM, of de laatste 7 dagen in stappen van 2 minuten op een bord met PSRAM; die staat in het RAM van de gateway, dus na een herstart begint ze leeg) en daarna live doorloopt met 5 seconden per punt; de Dallas-sensorlijnen beginnen bij het laden van de pagina. Een lijn heeft een onderbreking waar meer dan 2,5 minuut aan gegevens ontbreekt. Zolang de kamertemperatuur niet geldig is (SAT aan: SAT meldt hem als verloren; anders heeft de thermostaat hem 4,5 minuut niet gemeld) stopt de kamerlijn en loopt er een stippellijn in een andere kleur (roze) langs de onderkant van de grafiek; de CSV-export heeft dan een lege kamerwaarde voor die metingen. De geschiedenis die de gateway zelf bijhoudt toont dit niet. De grafiek heeft venster-chips (10 min, 1 uur, 4 u, 24 u, en op een bord met PSRAM ook 7 dagen; het 7-dagenvenster haalt de browser uitgedund op bij de gateway, met één punt per 10 minuten, terwijl het live deel tot 24 uur lang 5 seconden per punt houdt), een tijdas op ronde kloktijden, losse **PNG**-exportknop (met de legenda van de zichtbare lijnen, de aslabels en de tijdas, in het huidige thema) en **CSV**-exportknop, en **Auto-PNG** / **Auto-CSV**-chips voor periodieke auto-save. Het regelsetpoint (control setpoint), het thermostaatsetpoint en het SAT-doel worden als gestreepte lijn getekend; vanaf beta.396 tonen hun legenda-items hetzelfde streepje, op het scherm en in de PNG. Het SAT-doel is alleen een eigen lijn (en een eigen CSV-kolom) zolang SAT is ingeschakeld; de geschiedenis van de gateway bevat het alleen voor een venster waarin een SAT-waarde zat. Klik op een legenda-item om die lijn te verbergen of weer te tonen; die keuze onthoudt je browser.
- **Connection**: een OT-bus- en connectiviteitskaart. De OT-bus wordt gemodelleerd als twee aparte links (thermostaat en ketel), en de kaart scheidt **MODE** (een instelling, zoals gateway of monitor) van **HEALTH** met een vijf-toestanden-vocabulaire: Connected, Degraded, Disconnected, Off en Unknown (ADR-155). Knooppunten omvatten thermostaat, ketel, OTGW, router (met een Wi-Fi-signaalsterkte-icoon en dBm), MQTT-broker en deze browser. Op OT-Direct-hardware laat een paneel **OT-Direct overrides** u stored responses en response modifiers (SR/CR/RM/CM/UI/KI) rechtstreeks instellen of wissen.

#### SAT-pagina

Een aparte Smart Autotune Thermostat-pagina biedt de thermostaatbediening in drie cumulatieve detaillagen: **Thermostat** (eenvoudig), **Control** (operationeel) en **Technical** (de regellus). Ze is gekoppeld aan de SAT REST-endpoints. Statusmeldingen op de pagina beginnen met "SAT paused" als SAT de ketel heeft teruggegeven: "SAT paused: no OpenTherm connection, the boiler is back under normal control" en "SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler". Op een prerelease-firmware (bèta) staat een melding "SAT controls your boiler. This is a beta firmware." met het verzoek om capture-bestanden; SAT zelf heet niet meer experimenteel (het aan/uit-warmtepomptype met 3 uur cyclus nog wel). In de weerkaart noemt de kaart, zolang Home Assistant een verse `sat/sun_elevation` publiceert, die als bron van de zonstand en vraagt ze om geen locatie; staat het ophalen van weer aan zonder ingestelde breedte- en lengtegraad, dan vraagt ze er voor het ophalen zelf nog wel om. Zie Hoofdstuk 5 voor de volledige beschrijving van SAT, presets, de stooklijn en diagnostiek.

De kaart **Hot water** staat in de SAT-weergave **Control**. Met **Standalone DHW control** zet je SAT's eigen warmwaterregeling aan of uit: SAT stuurt de warmwatertemperatuur alleen naar de ketel als die aan staat, en zolang hij uit staat is de schuifregelaar grijs. De schuifregelaar loopt van 30 tot 60 °C, ingeperkt tot het warmwaterbereik van de ketel als de ketel dat doorgeeft (MsgID 48); een instelling onder 30 °C staat als **off**. Bij een ketel met een warmwatervoorraadvat toont de kaart ook **DHW enable (HW=)**, dat het warmwater-aan-commando van de gateway stuurt.

---

#### Settings-pagina

De Settings-pagina wordt aangestuurd door de REST API en presenteert instellingen met leesbare labels, categorieën en hints, plus een zoekveld en een rail met categorielinks. Een opslaanbalk houdt niet-opgeslagen wijzigingen bij met **Discard**- en **Save settings**-acties. De SAT-categorie wordt omschreven als "Smart Autotune Thermostat". Extra panelen zijn onder meer een **BLE-sensor**roster (gedetecteerde Bluetooth Low Energy-sensoren; op een bord zonder PSRAM is de schakelaar **Enable BLE sensors** uitgeschakeld en meldt de kaart "BLE needs PSRAM", omdat de firmware daar BLE niet laat aanzetten) en, in de Webhook-groep, een **Send test call**-actie die de echte opgeslagen ON-webhook afvuurt. Geavanceerde OT-Direct- en SAT-instellingen zijn hier ook beschikbaar.

#### Advanced-pagina

De Advanced-pagina bundelt de power-user-schermen in vier sub-tabbladen:

- **PIC firmware**: toont PIC-device, -type en -firmwareversie, een "Check for updates"-actie, een lijst van beschikbare firmwarebestanden met een flash-voortgangsbalk, en een gecachete gateway-instellingen (`PR=`)-tabel. Verborgen op hardware zonder PIC.
- **Debug Information**: apparaatinfo-groepen, de crash log (of een "geen crash logs"-melding), leesbare Wi-Fi-labels en een ruwe debug-dump.
- **File System**: een in-pagina FSexplorer (bladeren, uploaden, gebruiksbalk) met een link naar de klassieke `/FSexplorer`.
- **System**: live apparaatstatus (link, OT-rewrite-modus, simulatie) en **System Actions**-knoppen: Update Firmware (OTA sketch / filesystem), ReBoot, **Run setup wizard**, Reset Wireless en Home.

#### Eerste-keer setup-wizard

Op een echt vers apparaat toont de v2 UI eenmalig een onboarding-wizard voor de eerste installatie (bestaande installaties worden gemigreerd zodat hij niet opnieuw verschijnt). Hij is op elk moment opnieuw te starten via **Advanced > System > Run setup wizard**.

#### Mobiel gebruik en asset-caching

De v2 UI is ontworpen om bruikbaar te zijn op een smartphone. De assets worden geserveerd met een `no-cache`-beleid (ADR-163), zodat de browser na een filesystem-OTA-update de nieuwe assets direct oppikt zonder handmatig de cache te wissen.

---

### De klassieke webinterface

De klassieke webinterface van de 1.x-lijn zit niet meer in 2.x (ADR-192): 2.0.0 heeft alleen de v2-webinterface die hierboven staat beschreven. De secties hieronder beschrijven de instellingen, de updatepagina en het bestandsbeheer, die de v2-webinterface ook heeft.

### Instellingenpagina

De instellingenpagina is ingedeeld in secties. Alle instellingen worden opgeslagen in `/settings.ini` op de LittleFS-flash en overleven een herstart.

Naast het label van elke instelling staat een **i**-knop. Beweeg de muis erheen, selecteer hem met het toetsenbord of tik erop voor een uitleg van een of twee zinnen over die instelling. Een klik of tik houdt de tekst open en **Esc** sluit hem. De teksten zitten in de firmware (ze worden geleverd via `/settings-help.json`), niet in het bestandssysteem-image.

Klik op **Opslaan** onderaan een sectie om de wijzigingen toe te passen. De meeste instellingen zijn onmiddellijk actief; voor sommige (zoals Wi-Fi-instellingen) is een herstart vereist.

#### Wi-Fi sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Hostname | mDNS- en DHCP-hostnaam, bereikbaar als `<hostname>.local` | `OTGW2` (1.x: `OTGW`) |
| Wi-Fi Network (SSID) | Alleen-lezen weergave van het huidig verbonden Wi-Fi-netwerk | (huidige SSID) |
| Reset WiFi | Knop naast het SSID-veld. Wist de opgeslagen Wi-Fi-gegevens en herstart het apparaat in Access Point (AP) modus | n.v.t. |

Om naar een ander Wi-Fi-netwerk over te schakelen, klikt u op **Reset WiFi** en bevestigt u de dialoog. Het apparaat herstart, start het installatienetwerk `<hostname>-<MAC-adres>` (bijvoorbeeld `OTGW2-A4:CF:12:21:B4:F8`, zie hoofdstuk 2) en opent opnieuw de captive portal, zodat u tegen het nieuwe netwerk kunt authenticeren. Dezelfde actie is beschikbaar onder **FSexplorer > System Actions** via de knop *Reset Wireless*.

#### Wi-Fi-scan

De v2-webinterface heeft geen Wi-Fi-scanpaneel; om van netwerk te wisselen gebruikt u Reset WiFi en het setup-access point (zie hoofdstuk 2). Het REST-endpoint `GET /api/v2/network/scan` geeft nog wel de netwerken in de buurt: de eerste aanroep start de scan en geeft `{"status":"scanning"}`, een latere aanroep geeft `{"status":"ready","networks":[...]}` met per netwerk `ssid`, `rssi`, `channel`, `secured` en `connected`. Tijdens een OTA of een PIC-flash weigert het een scan (restAPI.ino:2419-2423).

#### MQTT sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Broker | IP-adres of hostnaam van de MQTT-broker | (leeg) |
| Poort | TCP-poort van de MQTT-broker | `1883` |
| Gebruikersnaam | MQTT-gebruikersnaam (optioneel) | (leeg) |
| Wachtwoord | MQTT-wachtwoord (optioneel) | (leeg) |
| TopTopic | Bovenste MQTT-topic-namespace | `OTGW` |
| UniqueID | Uniek apparaat-ID in de topic-structuur | `otgw-{MAC}` |
| HA discovery prefix | Prefix voor de discovery-topics; moet gelijk zijn aan die in Home Assistant. Discovery zelf staat altijd aan | `homeassistant` |

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

De nachtelijke herstart veroorzaakt een korte onderbreking van ongeveer 30 seconden. De functie is alleen actief wanneer NTP is ingeschakeld en gesynchroniseerd, zodat het apparaat de juiste lokale tijd kent.

#### GitHub controleren op nieuwe versies

Vanaf firmware 2.0.0-beta.387. De web-UI kan melden wanneer er een nieuwere firmware op GitHub staat. Er verschijnt dan een melding in de kopregel naast de firmwareversie, met een link naar de releasepagina. Vanaf beta.393 is die melding ook op een telefoon te zien (een venster van 480 px of smaller).

- Wie vraagt het aan GitHub: je browser, niet de gateway. De gateway blijft op je lokale netwerk en stuurt hiervoor niets naar internet.
- Wat er wordt verstuurd: je browser vraagt api.github.com hooguit elke 6 uur om de lijst met OTGW-firmware-releases. Zoals bij elke website ziet GitHub je internet-IP-adres. Het adres van de gateway gaat niet mee: het verzoek bevat geen referrer en geen cookies.
- Eerste keer: de controle staat uit tot je ja zegt. De web-UI vraagt het één keer, ongeveer 30 seconden nadat de pagina is geladen, en niet zolang de setup-wizard of de simulatievraag open staat. Vanaf beta.393 is die vraag een knop in de kopregel, naast de knoppen met hostnaam en versie; een klik erop toont de uitleg met Nee en Ja, en sluiten zonder antwoord betekent niet nu (de knop blijft staan). Vóór beta.393 opende de vraag als venster over de pagina. Je antwoord wordt op de gateway opgeslagen en geldt voor elke browser.
- Uitzetten: Instellingen > Gedrag > "GitHub controleren op nieuwe versies". Staat het uit, dan doet de browser helemaal geen verzoek naar GitHub.
- Bèta's: een bètafirmware krijgt nieuwere bèta's en stabiele releases te zien. Een stabiele firmware alleen stabiele releases, tenzij je ook "Ook bètaversies melden" aanzet.
- Melding verbergen: de × ernaast verbergt die versie tot er een nieuwere is.

#### Apparaat sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| LED-functies | Functietoewijzing voor de LED-indicatoren op de OTGW | `F/X/O/M/P/C` |
| GPIO-instellingen | GPIO-pinconfiguratie voor relais en S0 | (apparaatspecifiek) |
| Dallas GPIO-pin | GPIO-pin voor de 1-Wire DS18B20 bus | (apparaatspecifiek) |
| OLED | OLED-display inschakelen | uitgeschakeld |

#### OpenTherm Gateway (PIC)

Deze instellingen gelden voor de OTGW Classic met zijn PIC. Op hardware zonder PIC doen ze niets.

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Run commands at boot | Stuurt de opstartcommando's hieronder bij het starten naar de PIC | Uit |
| Boot commands | PIC-commando's gescheiden door puntkomma's, bijvoorbeeld `PS=0;GW=1` (maximaal 128 tekens) | `GW=1` in het meegeleverde bestandssysteem-image |
| PIC temperature sensor | Leest elke 3 minuten de temperatuursensor op de PIC en publiceert die naar MQTT (`otgw-pic/temperature_reading`) en Home Assistant (sensor `PIC_Temperature_Reading`). Een PIC zonder sensor publiceert niets, dus er verschijnt geen entity. Laat uit als er geen sensor is aangesloten | Uit |

De meting van de PIC-temperatuursensor is een temperatuur. De regel **Temperatuursensor** onder **Advanced > PIC-firmware** is iets anders: die laat zien waarvoor de sensor dient (`O` buiten, `R` retourwater).

#### OT-Direct modus (hardware zonder PIC)

Op hardware zonder PIC verschijnt een dropdown om de OT-Direct bedrijfsmodus te selecteren:

| Modus | Omschrijving |
|---|---|
| Bypass | Thermostaat rechtstreeks doorverbonden met de ketel; geen OT-verwerking. **Niet beschikbaar in deze release:** er is geen relaisondersteuning, kiezen valt terug op gateway. |
| Gateway | Volledige override-verwerking (standaard) |
| Monitor | Transparante doorgifte; berichten worden gelogd maar niet gewijzigd |
| Master / Standalone | Geen thermostaat nodig; de gateway is de enige OT-master |
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

**Opmerking:** De firmware ondersteunt uitsluitend HTTP en WS (geen HTTPS, geen WSS). Stel HTTP Basic Auth alleen in op een vertrouwd thuisnetwerk. Voor externe toegang wordt een VPN aangeraden; achter een HTTPS-reverse proxy die WebSocket doorgeeft werkt de hele interface, ook de live log (zie paragraaf 6.10).

#### Webhook sectie

| Instelling | Omschrijving | Standaard |
|---|---|---|
| Webhook URL | HTTP POST-URL voor uitgaande statusmeldingen | (leeg) |
| Triggerbits | OpenTherm-statusbits die een webhook activeren | (selecteerbaar) |

Webhooks sturen een HTTP POST naar de opgegeven URL wanneer geselecteerde statusbits veranderen (bijv. vlam aan/uit, storing). De payload is JSON. Dit is nuttig voor integratie met Node-RED of andere HTTP-gebaseerde automatiseringen.

---

### OTA firmware-update via de webinterface

De 2.x-firmware haalt zelf geen updates van GitHub op: download de release-zip en geef die aan de update-pagina.

#### Update in een keer op ESP32-S3 (4 MB flash)

De flash utility op `http://<ip>/update` installeert firmware en webbestanden in een keer:

1. Open de pagina met het IP-adres (niet met `otgw2.local`: de updater heeft geen mDNS-naam).
2. Kies de release-zip, of de bestanden `.ino.bin` en `.littlefs.bin`. De pagina controleert de bestanden op inhoud en weigert USB-images, de updater zelf en bestanden uit verschillende builds.
3. Druk op **Update** en laat het tabblad open tot er Done staat (2 tot 3 minuten). De unit herstart twee keer. De pagina schakelt over naar de safeboot-updater, installeert de firmware, wacht op de nieuwe firmware, installeert de webbestanden en controleert beide.

Instellingen en sensornamen blijven behouden, en ook de SAT-geleerde waarden (energietotalen, stooklijngegevens en markeringen, regelaarstoestand), de geleerde OpenTherm-profielen, het herstartlogboek en het totaal van de warmwatermeter. Een bestand dat groter is geworden dan zijn limiet (2 tot 3 KB) valt af. Bestanden die u in de File Explorer uploadde, het opstartdetectielogboek en de opschoonstatus van MQTT-topics beginnen opnieuw. **Download a settings backup** is een aparte link op de pagina en geen onderdeel van de update. De formulieren onder **Advanced** houden de oude werkwijze met een upload per keer. Herstelstappen en de volledige lijst: [Firmware-updates via WiFi](../../guides/WIFI_FIRMWARE_UPDATE.md) (Engels).

#### Handmatig uploaden

Als alternatief kunt u een firmwarebinary handmatig uploaden:

1. Download de release-zip van de GitHub-releasepagina en pak het `.ino.bin`-bestand eruit.

2. Op de update-pagina, selecteer **Handmatig uploaden**.

3. Sleep het bestand naar het uploadgebied of klik om het te selecteren.

4. Klik op **Uploaden**. De voortgangsbalk geeft de uploadstatus aan.

5. Na upload herstart het apparaat automatisch.

#### PIC-firmware updaten

Op dezelfde pagina kunt u ook de PIC co-processor firmware updaten. Selecteer het `.hex`-bestand voor de PIC en klik op **PIC uploaden**. De firmware flasht de PIC via de seriele verbinding.

**Waarschuwing:** Onderbreek de PIC-firmwareupdate nooit. Als de voeding wegvalt tijdens het flashen, kan de PIC beschadigd raken en is handmatig herprogrammeren via een PIC-programmer noodzakelijk.

---

### Bestandsbeheer (FSexplorer)

De FSexplorer is bereikbaar via de Geavanceerd-tab onder **File system contents**, of rechtstreeks op `http://otgw2.local/FSexplorer.html`. De pagina geeft toegang tot het LittleFS-bestandssysteem van het apparaat.

Mogelijkheden:

- Bladeren door bestanden en submappen. Mappen staan bovenaan en de lijst is alfabetisch gesorteerd (hoofdletter-ongevoelig). In een submap verschijnt een `.. (Parent)`-link om terug te navigeren.
- Bestanden uploaden via het **Upload File** formulier. Een voortgangsbalk toont de uploadstatus en de bestandsgrootte wordt vooraf gecontroleerd tegen de beschikbare vrije ruimte.
- Bestanden downloaden via de **Download** link in de rij van een bestand.
- Bestanden verwijderen via de **Delete** link.
- Beschikbare en gebruikte schijfruimte van de LittleFS-partitie bekijken onderaan de lijst.

Onder de bestandenlijst staat een **System Actions**-paneel met snelkoppelingen naar **Update Firmware** (alleen op desktopbrowsers), **ReBoot**, **Reset Wireless** (identieke actie als Reset WiFi op de Settings-pagina) en **Exit FSexplorer** (terug naar de hoofdpagina).

**Let op:** Verwijder geen actieve assets zoals `index.html`, `index.js`, `sat.js`, `graph.js`, `components.css`, `ds-tokens.css` of `/settings.ini`. Dit kan de werking van de webinterface verstoren. Herstel is mogelijk door het LittleFS-bestandssysteem opnieuw te flashen via USB.

Wanneer `index.html` ontbreekt (bijvoorbeeld omdat het LittleFS-image nog niet is geflasht), serveren de FSexplorer-routes `FSexplorer.html` direct als fallback-pagina, zodat u alsnog een filesystem-image kunt uploaden.

Ontbreekt ook de bestandsbeheerpagina zelf (geen `FSexplorer.html.gz` op het filesystem), dan worden bestaande bestanden gewoon geserveerd, bijvoorbeeld het herstartlog; alleen een verzoek om een bestand dat niet bestaat krijgt de uploadpagina (beta.244, TASK-1926).

De firmware streamt alle bestanden in plaats van ze in RAM te bufferen, dus er is geen harde uploadlimiet anders dan de beschikbare ruimte in de LittleFS-partitie. Het `index.html`-bestand is op 2.0.0 enkele tientallen kilobytes groot en wordt altijd via `streamFile()` geserveerd om het RAM te ontzien (TASK-668).

#### Asset-caching en de ETag-flow

De webinterface gebruikt een ETag-gebaseerde cachingstrategie gecombineerd met versioned URLs voor JavaScript:

- `index.html` wordt geserveerd met de huidige filesystem-hash als `ETag`. De browser cachet de pagina maar revalideert altijd via `If-None-Match`. Een ongewijzigd filesystem antwoordt met `304 Not Modified`; een geflasht filesystem antwoordt met `200` en de nieuwe inhoud.
- `index.js` en `graph.js` worden geladen met een `?v=<fsHash>`-querystring. Aanvragen met de juiste hash krijgen `Cache-Control: public, max-age=86400`; een kale aanvraag zonder `?v=` krijgt `Cache-Control: no-cache`, zodat een verouderde URL nooit oude JavaScript serveert.
- Wanneer een vooraf gegzipte `index.js.gz` of `graph.js.gz` op het LittleFS-image staat, kiest de server deze automatisch en laat `streamFile()` de `Content-Encoding: gzip`-header zetten (TASK-304/TASK-433).

---

### Licht en donker thema

De webinterface ondersteunt een licht en een donker thema. De schakelaar staat rechtsboven in de navigatiebalk (zon/maan-icoon). Uw keuze wordt opgeslagen in de `localStorage` van de browser en blijft behouden bij het opnieuw laden van de pagina.

Het thema past de achtergrond, tekst, grafieken en tabelkleuren aan. De ECharts-temperatuurgrafieken en SAT-grafieken schakelen automatisch mee. Alle UI-elementen, inclusief Wi-Fi-signaalbalkjes, statusindicatoren en het OT-Direct paneel, worden correct weergegeven in beide thema's.
