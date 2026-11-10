## Hoofdstuk 7: Probleemoplossing

Dit hoofdstuk beschrijft veelvoorkomende problemen, hun oorzaken en de stappen om ze op te lossen. De structuur gaat van het meest basale (het apparaat reageert niet) naar meer specifieke integratieproblemen.

---

### 7.1 OTGW lijkt offline na opstarten

#### Symptomen

- De Web UI toont de OTGW als offline of "not available".
- MQTT-topic `otgw-pic/picavailable` heeft waarde `offline`.
- Er verschijnen geen OpenTherm-berichten in de live log.

#### Veelvoorkomende oorzaken en oplossingen

**Oorzaak 1: PIC reageert niet**

De PIC-microcontroller (het OpenTherm-protocolverwerkende deel van de hardware) communiceert via een seriele verbinding met de ESP. Als de PIC niet reageert, ziet het apparaat geen OpenTherm-activiteit.

Controleer:
- Is de boiler ingeschakeld en verbonden via de OTGW-klemmen?
- Is de OpenTherm-bus correct aangesloten op de MASTER- en SLAVE-klemmen?
- Heeft de PIC stroom? Sommige OTGW-versies voeden de PIC via de OpenTherm-bus.

**Oorzaak 2: Verkeerde baudrate**

De seriële verbinding tussen ESP en PIC werkt op 9600 baud. Bij een firmwaremismatch of na een incorrecte PIC-flash kan de communicatie uitvallen.

Controleer via de Telnet-debuglog (`telnet <ip>`): verschijnen er regelmatig regels die beginnen met `T:`, `B:`, `R:`, `A:`? Zo niet, is er mogelijk een serieel communicatieprobleem.

**Oorzaak 3: PIC in diagnostische modus of interfacemodus**

Als de PIC-firmware in een niet-gatewaymodus draait (bijv. door een eerdere OTmonitor-sessie), verwerkt de ESP de uitvoer niet als verwachte OTGW-regels.

Controleer de Telnet-log op de huidige gatewaymodus. Verstuur via de Web UI of MQTT het commando `GW=G` om de PIC terug te zetten in gatewaymodus.

**Oorzaak 4: Seriele overrun**

Bij hoog berichtenverkeer op de OpenTherm-bus kan er een seriele overrun optreden. Versie 2.0.0 rapporteert overruns als events via MQTT en WebSocket, zodat ze zichtbaar zijn.

Controleer in de Telnet-log op meldingen met `SERIAL OVERRUN` of bekijk het WebSocket-eventlog in de Web UI.

---

### 7.1a PIC niet gevonden op een OTGW Classic met een S3 Mini

#### Symptomen

U heeft uw OTGW Classic (de NodoShop-print met de PIC-chip) omgebouwd van een Wemos D1 mini naar een LOLIN S3 Mini en 2.0.0 geflasht. De Web UI werkt, maar er komt geen OpenTherm-verkeer binnen. Open `http://<ip-van-uw-otgw>/api/v2/device/info` in een browser en zoek deze twee velden:

```json
"hardwaremode": "Degraded",
"otcommandinterface": "None",
```

Op een werkende Classic staat er twee keer `"PIC"`. `Degraded` betekent: de firmware zocht bij het opstarten de PIC en kreeg geen antwoord. `None` betekent: er is geen weg om OpenTherm te praten.

#### Snelle controles (begin hier)

1. **Controleer hoe de S3 Mini in de voet zit.** De S3 Mini heeft aan elke kant twee pinrijen. Alleen de **buitenste** rijen passen op de voetafdruk van de Wemos D1 mini; de binnenste rijen zijn extra S3-pinnen en blijven buiten de voet. Zit de module een rij verschoven, of is een pin verbogen of niet helemaal ingedrukt, dan is de seriële verbinding met de PIC weg. Kijk van opzij: elke buitenste pin moet helemaal in de voet zitten.
2. **Haal de stroom van de hele print.** Stroom eraf, tien seconden wachten, stroom erop. Een herstart vanuit de Web UI herstart alleen de ESP.
3. **Zet Board mode op Auto-detect.** Ga in de Web UI naar Settings, System, **Board mode**. Kies **Auto-detect**, sla op en herstart. Kies op een Classic niet **OT-Direct (no PIC)**: die stand is voor een print zonder PIC.
4. **Vergelijk met de oude module.** Heeft u de D1 mini met 1.x-firmware nog? Zet die terug. Ziet 1.x de PIC wel, dan zijn PIC en print in orde en zit het probleem tussen de S3 Mini en de voet.

Nog steeds `Degraded`? Verzamel het bewijs hieronder en vraag om hulp (paragraaf 7.14).

#### Wat de firmware bij het opstarten doet

Bij elke start, zodra WiFi werkt, zoekt de firmware de PIC. Ze reset de PIC en wacht op het end-of-text-byte (ETX, 0x03) dat de bootloader van de PIC na een reset stuurt. Komt de ETX, dan is er een PIC en wordt de modus `PIC`. Komt er niets, dan wordt het `Degraded`.

De instelling **Board mode** bepaalt hoe vaak de firmware het probeert en wat ze onthoudt (ADR-201):

| Board mode | Wat er bij het opstarten gebeurt |
|---|---|
| Auto-detect (0, standaard) | Maximaal 3 pogingen, 60 ms uit elkaar. Een gevonden PIC wordt opgeslagen als "Classic"; daarna neemt elke start het vaste PIC-pad, dat de PIC nog steeds controleert. Een misser wordt **niet** opgeslagen, dus de volgende start probeert het opnieuw. Op een S3 Mini zonder antwoord van de PIC blijft de firmware `Degraded` en schakelt ze niet over op OT-Direct. |
| PIC (Classic) (1) | Eén poging per start. Geen antwoord geeft `Degraded`. |
| OT-Direct (no PIC) (2) | Kijkt één keer of er een PIC is. Antwoordt er een, dan gaat ze terug naar Classic en slaat dat op. Antwoordt er geen, dan start ze OT-Direct, en dat is fout op een Classic. |

Zolang er geen PIC bekend is, stuurt de firmware ook elke minuut `PR=A` naar de PIC en logt `PIC is unknown, probe pic using PR=A`. Antwoordt de PIC daarop, dan wordt de modus `PIC` zonder herstart.

#### Het opstartverslag: `/bootdetect.log`

De detectie bij het opstarten loopt **voordat** de Telnet-server start. Een Telnet-capture laat haar dus nooit zien, ook niet over een herstart heen. De firmware schrijft het resultaat daarom naar een klein bestand op het apparaat. Open de FSexplorer (`http://<ip>/FSexplorer.html`) en download `/bootdetect.log`. De nieuwste start staat bovenaan; het bestand bewaart de laatste 30 starts.

Een Classic met S3 Mini die de PIC vond:

```text
boot#12 t=6843ms eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36
```

Een Classic waarvan de PIC niet antwoordde:

```text
boot#13 t=6851ms eMode=3 pic=0 mode=0 pro=0 RST=12 RX=44 TX=43 I2C=35/36
```

(De `t=`-waarden zijn voorbeelden; bij u staan er andere getallen.)

| Veld | Betekenis |
|---|---|
| `eMode` | Uitkomst: `0` nog niet bepaald, `1` PIC, `2` OT-Direct, `3` Degraded |
| `pic` | `1` als de PIC antwoordde |
| `mode` | De instelling Board mode na de detectie: `0` auto, `1` PIC op S3 Mini, `2` OT-Direct, `3` PIC op S3 Mini Pro |
| `pro` | `1` als een S3 Mini Pro herkend is (aan de bewegingssensor op de module) |
| `RST` `RX` `TX` | GPIO's voor de PIC-reset en de seriële verbinding. S3 Mini: 12, 44, 43 |
| `I2C` | SDA/SCL-pinnen. S3 Mini: 35/36 |

Staat er op elke regel `pic=0` terwijl `RST`, `RX` en `TX` 12, 44 en 43 tonen? Dan gebruikte de firmware de juiste pinnen en antwoordde de PIC niet. Dat wijst naar de plaatsing in de voet, de module of de PIC zelf, niet naar een instelling.

#### Wat u meestuurt als u om hulp vraagt

1. `/bootdetect.log` (downloaden via de FSexplorer).
2. De uitvoer van `/api/v2/device/info`.
3. Een capture met het capture-script uit de flash-zip (`capture-mqtt-debug.bat` op Windows, `capture-mqtt-debug.sh` op Linux, `capture-mqtt-debug-macos.sh` op macOS). Start het, herstart de gateway vanuit de Web UI terwijl het loopt, laat het een paar minuten lopen, stop met `Q` en post de upload-zip. Daarin staan de `PR=A`-pogingen per minuut en het herstartlog; de detectie bij het opstarten zelf komt uit `/bootdetect.log`.
4. Foto's van opzij van beide pinrijen van de S3 Mini in de voet.
5. De firmwareversie en de printversie die op de print staat.

---

### 7.2 MQTT maakt geen verbinding

#### Symptomen

- Geen MQTT-berichten ontvangen in Home Assistant of uw MQTT-client.
- MQTT-status in de Web UI toont "Disconnected".
- Telnet-log bevat herhaalde verbindingspogingen.

#### Diagnostiek

Verbind met de Telnet-debuglog:

```bash
telnet <ip-adres>
```

Zoek naar regels als:

```
MQTT: Connecting to broker 192.168.1.10:1883
MQTT: Connected
```

of foutmeldingen als:

```
MQTT: Connection refused (wrong credentials)
MQTT: Connection refused (broker unavailable)
```

#### Controlelijst

| Oorzaak | Oplossing |
|---------|-----------|
| Fout broker-adres | Controleer het IP-adres of hostnaam in Settings |
| Fout poort | Standaard is 1883; controleer of uw broker op een andere poort luistert |
| Verkeerde inloggegevens | Controleer gebruikersnaam en wachtwoord in Settings |
| Broker niet bereikbaar | Ping het broker-adres vanuit een ander apparaat op hetzelfde netwerk |
| Broker accepteert geen anonieme verbindingen | Stel gebruikersnaam en wachtwoord in de firmware in |
| Topic-prefix conflict | Controleer of `settingMQTTtopTopic` uniek is per apparaat |

#### MQTT LWT (Last Will and Testament)

De firmware publiceert bij een gecontroleerde afsluiting `offline` naar het LWT-topic. Bij een onverwachte herstart verzendt de broker automatisch `offline`. Controleer of het LWT-topic correct is geconfigureerd als u de beschikbaarheidsstatus in Home Assistant wilt gebruiken.

---

### 7.3 Home Assistant-entiteiten verschijnen niet

#### Symptomen

- MQTT werkt, maar er verschijnen geen entiteiten in Home Assistant.
- Slechts een deel van de 250+ entiteiten is zichtbaar.

#### Oorzaak 1: Verkeerde discovery-prefix

Home Assistant luistert standaard op `homeassistant/` voor auto-discovery berichten. De firmware gebruikt dezelfde standaard. Controleer:

1. Ga naar Settings in de Web UI.
2. Controleer het veld **HA MQTT Prefix** (standaard `homeassistant`).
3. Zorg dat dit overeenkomt met de `discovery_prefix` in de Home Assistant MQTT-integratieconfiguratie.

#### Oorzaak 2: MQTT-integratie niet geconfigureerd in Home Assistant

Home Assistant auto-discovery vereist dat de MQTT-integratie is geconfigureerd en verbonden met dezelfde broker als de firmware.

Controleer: **Instellingen** > **Apparaten en Services** > **MQTT** en zorg dat de integratie actief is.

#### Oorzaak 3: Discovery is geleidelijk (drip publisher)

Vanaf v2.0.0 verstuurt de firmware discovery-berichten niet meer in een burst, maar via een async drip publisher: een bericht per 3 seconden. Met circa 250 entiteiten duurt een volledige discovery-cyclus ongeveer 12 tot 15 minuten onder normale omstandigheden. Wanneer het geheugen onder druk staat, wordt het interval verlaagd naar 30 seconden per bericht, wat de totale tijd tot meer dan een uur kan verlengen. Dit geleidelijke proces is bewust ontworpen om heap-uitputting te voorkomen.

**Wacht dus geduldig af.** Entiteiten verschijnen stuk voor stuk in Home Assistant. Na een firmware-update of MQTT-herverbinding worden alle discovery-berichten automatisch opnieuw ingepland. Een herstart is niet nodig.

#### Oorzaak 4: Retained berichten van een eerdere installatie

Verouderde retained discovery-berichten van een eerdere firmwareversie of ander topicprefix kunnen conflicten veroorzaken. Verwijder de retained berichten via een MQTT-client (bijv. MQTT Explorer) of via de Home Assistant MQTT-debugpagina.

---

### 7.4 WiFi blijft wegvallen

#### Symptomen

- Het apparaat valt regelmatig weg uit het netwerk.
- De Web UI is intermitterend niet bereikbaar.
- Telnet-log toont herhaalde WiFi-verbindingspogingen.

#### Mogelijke oorzaken

**Zwak WiFi-signaal**

Controleer de signaalsterkte (RSSI). De Web UI toont signaalkwaliteitsbalkjes in de header (toegevoegd in v2.0.0). RSSI hoger dan -70 dBm is doorgaans stabiel; lager dan -80 dBm geeft problemen.

Oplossingen:
- Plaats een WiFi-repeater dichter bij het OTGW-apparaat.
- Wijzig het WiFi-kanaal op uw router om interferentie te vermijden.

**IP-adresconflict bij gebruik van statisch IP**

Wanneer een statisch IP is ingesteld dat ook door DHCP wordt uitgedeeld, kan een ander apparaat hetzelfde adres krijgen. Schakel over op DHCP of reserveer het adres in uw router.

**AP-fallback activeert onterecht**

Wanneer het WiFi-signaal langdurig wegvalt, probeert de firmware maximaal 10 keer opnieuw te verbinden. Na 10 mislukte pogingen herstart het apparaat zichzelf. Dit is bedoeld als fail-safe, niet als normale werking. Controleer de WiFi-stabilteit als u regelmatige herstarts ziet.

---

### 7.5 OTA-update mislukt

#### Symptomen

- De OTA-pagina geeft een foutmelding na het uploaden.
- Het apparaat herstart niet na een upload.
- De firmware versie is na de herstart ongewijzigd.

#### Controlelijst

| Probleem | Oplossing |
|---------|-----------|
| Verkeerd bestand | Gebruik de esp32-combo-zip van de release |
| Bestand te groot | Gebruik de esp32-combo-zip van de release; die firmware past in het firmwareslot |
| HTTP-wachtwoord vergeten | Stel het wachtwoord opnieuw in via seriele verbinding of reset de firmware |
| Verbinding verbroken tijdens upload | Probeer opnieuw via een stabielere verbinding (bedraad netwerk of dichterbij de router) |
| Browser cached versie | Ververs met Ctrl+F5 en probeer opnieuw |

#### Na een mislukte OTA-update

Als de firmware niet meer reageert na een mislukte update:

1. Een firmware-update via WiFi loopt via de safeboot-updater. Start de nieuwe firmware niet goed op, dan keert de unit terug naar safeboot en kunt u de update opnieuw uitvoeren.
2. Helpt dat niet, verbind de S3 Mini dan via USB en flash de zip opnieuw met `flash_otgw.bat` (Windows) of `./flash_otgw.sh` (Linux/macOS). Zonder opties wist het script de flash: WiFi-gegevens en instellingen gaan verloren.
3. Een unit op de oude partitie-indeling (een vroege 2.0.0-alpha) heeft eenmalig `--migrate --host <ip>` nodig; dat bewaart `settings.ini` en zet de instellingen terug. Draai het script met `--help` voor alle opties.

> **WAARSCHUWING: Flash PIC-firmware NOOIT via WiFi.**
>
> De ESP-firmware kan veilig via WiFi worden geüpdatet (OTA), maar de PIC-microcontroller op de OTGW-print is een afzonderlijke chip met een eigen flash-pad. Het gebruik van OTmonitor via de TCP-seriele socket om PIC-firmware te uploaden (bijvoorbeeld een upgrade naar PIC v6.6) **staat bekend om het brick'en van de PIC**: een WiFi-hapering halverwege de overdracht laat de PIC in een half-geprogrammeerde toestand achter, waaruit alleen in-circuit herprogrammering nog redding biedt.
>
> Flash PIC-firmware altijd via een **bedrade USB-seriele verbinding** met OTmonitor of het daarvoor bedoelde PIC-programmeergereedschap. Dit is een hardware-beperking, geen firmware-bug, en de OTGW-firmware kan dit niet omzeilen.

---

### 7.6 Web UI laadt niet

#### Symptomen

- `http://otgw2.local` geeft geen reactie of een foutpagina.
- De pagina laadt deels maar toont lege secties.

#### Stap 1: Controleer netwerkbereikbaarheid

```bash
ping otgw2.local
```

Reageert de ping niet, controleer dan:
- Is het apparaat verbonden met WiFi? Controleer de router voor verbonden apparaten.
- Is mDNS beschikbaar? Probeer het IP-adres rechtstreeks in plaats van `otgw2.local`.

#### Stap 2: Controleer de HTTP-server

Open de Telnet-debuglog en probeer de pagina opnieuw te laden. Verschijnen er HTTP-verzoeken in de log? Zo niet, reageert de webserver niet.

#### Stap 3: LittleFS niet gemount

Als de LittleFS-partitie (het bestandssysteem met de webbestanden) niet kon worden gemount, zijn de HTML/CSS/JS-bestanden niet beschikbaar. De Telnet-log toont dan een melding als `LittleFS mount failed`.

Oplossing: installeer de release-zip opnieuw via de `/update`-pagina, of upload de bestanden via `/fsexplorer`.

#### Stap 4: Geheugengebrek

Bij ernstig geheugengebrek kunnen HTTP-verzoeken mislukken. De Web UI toont de vrije heap in de voettekst. Zie paragraaf 7.7 voor de heap health tiers en wat u kunt doen bij een lage heap.

---

### 7.7 Heap health monitoring

De firmware bewaakt continu het vrije heap-geheugen en classificeert het in vier niveaus:

| Niveau | Vrije heap drempel | Gedrag |
|--------|-------------------|--------|
| HEALTHY | Boven 8.192 bytes | Normale werking. Alle services actief. |
| LOW | 5.120 tot 8.191 bytes | Berichtfrequentie begint af te nemen. MQTT discovery drip-interval vertraagt van 3s naar 30s. |
| WARNING | 3.072 tot 5.119 bytes | WebSocket- en MQTT-berichten worden beperkt (throttled). Niet-essentiele verzendingen worden overgeslagen. |
| CRITICAL | Onder 3.072 bytes | Alle niet-essentiele operaties geblokkeerd. Emergency heap recovery wordt automatisch uitgevoerd. |

Het huidige heap-niveau wordt elke 60 seconden gelogd in de Telnet-debuglog. Zoek naar regels die beginnen met `Heap:` gevolgd door het aantal vrije bytes en de tier-naam. De Web UI voettekst toont ook de huidige vrije heap-waarde.

#### Emergency heap recovery

Wanneer de vrije heap onder de CRITICAL-drempel zakt, probeert de firmware automatisch geheugen terug te winnen door:

1. De MQTT-buffer te verkleinen tot het minimumformaat.
2. Een yield uit te voeren zodat de netwerkstack pending buffers kan vrijgeven.

De Telnet-log toont `Emergency heap recovery starting` met het aantal herwonnen bytes. Als het probleem na herstarts aanhoudt, overweeg dan de nachtelijke herstart te activeren (zie paragraaf 7.7.1).

#### 7.7.1 Nachtelijke herstart

Als preventieve maatregel tegen geleidelijke heap-fragmentatie biedt de firmware een optionele geplande nachtelijke herstart. Wanneer ingeschakeld in Settings, herstart het apparaat automatisch op het geconfigureerde uur (lokale tijd, gebaseerd op uw NTP-tijdzone). De herstart vindt alleen plaats als het apparaat langer dan een uur draait, om herstartlussen na een recente reboot te voorkomen.

Inschakelen: ga naar Settings in de Web UI, schakel "Nightly Restart" in en kies het gewenste herstartuur (standaard: 04:00).

---

### 7.8 Seriële overrun en OTGW online-offline-flikkeringen

#### Wat is een seriële overrun?

Een seriële overrun treedt op wanneer de PIC meer data naar de ESP stuurt dan de firmware leest voordat de UART-buffer vol is. Individuele bytes gaan verloren. De firmware detecteert dit via de hardware overrun-vlag van de UART.

#### Symptomen

- Een `Serial Overrun`-regel in de Telnet-debugconsole.
- Een `Serial Overrun`-event in het eventlog van de Web UI en op het MQTT-topic `event_report`. Het event wordt onderdrukt in de eerste 15 seconden nadat de PIC is gestart.
- Onvolledige OpenTherm-regels in de debuglog.
- OTGW-beschikbaarheid die snel wisselt tussen `online` en `offline`.

#### Wat te doen

1. **Controleer de MQTT-verbinding.** Een drukke of instabiele MQTT-verbinding vertraagt de verwerking en kan indirecte overruns veroorzaken (zie paragraaf 7.2).
2. **Controleer de vrije heap.** Bij laag geheugen vertraagt de firmware (zie paragraaf 7.7).
3. **Controleer of OTmonitor actief is op poort 25238.** OTmonitor die actief is en commands stuurt, geeft extra belasting.
4. **Sluit andere oorzaken van flikkeringen uit:** WiFi-onstabiliteit (paragraaf 7.4) of een herstart van de MQTT-broker.
5. **Verzamel bewijs.** Verbind met de Telnet-debugconsole en noteer `Exception`- of `Stack smashing`-meldingen. Voeg het crashlog (paragraaf 7.11) en de Telnet-log toe aan een GitHub-issue.

Op de 1.x-lijn werd dit veroorzaakt door grote stackbuffers in `publishToSourceTopic()`; op 2.x gebruikt die functie een statische buffer.

---

### 7.9 LED-patronen

De firmware gebruikt status-LEDs om de apparaatstatus aan te geven. Op de S3 Mini worden de LEDs via PWM gedimd voor een lagere helderheid.

| LED-patroon | Betekenis |
|-------------|-----------|
| LED2 knippert 1x per seconde, LED1 uit | Geen WiFi-verbinding. Het apparaat probeert opnieuw te verbinden. |
| LED1 knippert 1x per seconde, LED2 knippert 2x per seconde | WiFi is verbonden maar er is al meer dan 10 seconden geen OpenTherm-verkeer ontvangen. Controleer de PIC- en boilerverbinding. |
| Beide LEDs uit (normale werking) | WiFi verbonden, OpenTherm-verkeer actief. LEDs worden alleen kort gebruikt voor verwerkingsindicatie. |
| Snelle knipperingen tijdens opstarten | Setup-fase. Drie snelle knipperingen signaleren voltooiing van een setup-stap. |

Een OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro toont de WiFi-status ook met de RGB-pixel op de LOLIN. Die knippert één keer per seconde (de instelling `LED blink` heeft er geen invloed op):

| Kleur pixel | Betekenis |
|---|---|
| Rood | WiFi niet verbonden. |
| Paars | Er staat een accesspoint open: het WiFi-configuratieportaal, of de AP-fallback als het opgeslagen netwerk bij het opstarten onbereikbaar is. Maak daarmee verbinding om WiFi in te stellen. |
| Groen | WiFi verbonden. |

`/api/v2/device/info` meldt de aangestuurde pixel-GPIO als `status_pixel_gpio` (`0` als er geen is).

De fysieke OTGW-print heeft ook indicator-LEDs die door de PIC worden aangestuurd:
- **Groene LED**: PIC draait en communiceert.
- **Gele LED**: Boilercommunicatie actief.
- **Rode LED**: Fout of storing.

LED-knipperen kan volledig worden uitgeschakeld in Settings (`LED blink`).

---

### 7.10 Telnet-debuglog: verbinden en lezen

De Telnet-debuglog op poort 23 is het krachtigste diagnosehulpmiddel dat de firmware biedt. Alle interne events worden hier in real-time gerapporteerd. Naast passieve logging accepteert de Telnet-console ook enkele-toets commando's voor live diagnostiek.

#### Verbinden

**Linux/macOS:**

```bash
telnet <ip-adres>
# of:
nc <ip-adres> 23
```

**Windows (PowerShell):**

```powershell
telnet <ip-adres>
```

Telnet is in Windows standaard uitgeschakeld. Activeer het via **Programma's en onderdelen** > **Windows-onderdelen in- of uitschakelen** > **Telnet Client**.

**Alternatief:** Gebruik PuTTY met verbindingstype "Raw" op poort 23.

#### Debugmenu-toetsen (v2.0.0)

Druk op elk moment op `h` om het helpmenu te tonen met de huidige apparaatstatus en de stand van de toggles. De toetsenset is gewijzigd ten opzichte van v1.3.5; als u de oude indeling gewend bent, raadpleeg dan onderstaande tabellen.

**Debuglog-toggles** (elke toets schakelt een categorie aan of uit):

| Toets | Categorie |
|---|---|
| 1 | OT-berichten parsen |
| 2 | API-afhandeling (REST) |
| 3 | MQTT-module |
| 4 | Sensormodules |
| 5 | SAT-regellus, cycles en HCR (standaard aan) |
| 6 | OTDirect frame handling (standaard aan) |
| g | MQTT interval gating (laat zien waarom een bericht wel of niet is gepubliceerd) |
| n | NTP-tijdsynchronisatiedetails |
| d | Dallas-sensor simulatiehelper |

**Commando's:**

| Toets | Actie |
|---|---|
| h | Toon helpmenu met huidige status en togglestanden |
| q | Forceer herinlezen van `settings.json` vanuit flash |
| F | Forceer volledige Home Assistant-discovery voor ALLE message IDs (wist de interne discovery-bitmap en publiceert alles opnieuw) |
| r | Herverbind WiFi, Telnet, OTGW seriele stream en MQTT |
| p | Reset de PIC handmatig |
| a | Verstuur `PR=A` naar de PIC om firmwareversie, type en device ID op te vragen |
| s / S | Toggle OTGW seriele simulatie-replay (voor ontwikkelen zonder PIC) |
| b | Knipper LED 1 vijf keer |
| i | Initialiseer relay-uitgangen |
| u | Zet de geconfigureerde GPIO-uitgang AAN |
| o | Zet de geconfigureerde GPIO-uitgang UIT |
| j | Lees de huidige GPIO-uitgangstatus |
| l | Toggle de generieke MyDEBUG-vlag |
| f | Toon de MyDEBUG-vlagstand |

De `F`-toets is handig na een Home Assistant MQTT-wissing of na een wijziging van het topic-prefix: het forceert een volledige herpublicatie van alle discovery-items in plaats van te wachten op de volgende natuurlijke trigger.

De `r`-toets is de snelste manier om te herstellen van een tijdelijke netwerkhapering zonder het apparaat uit te schakelen.

#### Nuttige logpatronen herkennen

| Logpatroon | Betekenis |
|-----------|-----------|
| `WiFi connected, IP: 192.168.x.x` | WiFi-verbinding geslaagd |
| `MQTT: Connected` | MQTT-verbinding actief |
| `MQTT: heap before connect = XXXX` | Heap-diagnose vlak voor elke MQTT-(her)verbinding; nuttig om heapdruk te correleren met mislukte verbindingen |
| `MQTT: heap after birth = XXXX` | Heap-snapshot direct na de birth/online-publicatie |
| `MQTT: heap after discovery republish = XXXX` | Heap-snapshot na een discovery-herpublicatie |
| `Hour changed: X -> Y, heap=ZZZZ` | Uur-hartslag vanuit `hourChanged()`; een gestaag dalende heap hier wijst op een trage lek |
| `NTP synced: 2026-04-11 ...` | NTP-synchronisatie gelukt |
| `T:XXXX YYYY` | OpenTherm-frame ontvangen van thermostaat (T = Thermostat) |
| `B:XXXX YYYY` | OpenTherm-frame ontvangen van boiler (B = Boiler) |
| `SERIAL OVERRUN` | UART-buffer overgelopen |
| `Exception (2)` | Illegale geheugentoegang. In v2.0.0 is de klasse crashes in `strncmp_P`/`strstr_P` op binaire data opgelost. Ziet u dit nog op 2.0.0, leg dan de crashlog vast en meld het als bug. |
| `Exception (3)` | Load/store alignment-fout; vrijwel altijd een PROGMEM-pointer doorgegeven aan een functie die RAM verwacht. De bekende Arduino Core 3.1.2+-variant is in 2.0.0 opgelost. |
| `NTP: bogus initial time ignored` | Normaal bij de eerste opstart; de SDK geeft soms `2106-02-07` terug voor de eerste echte sync. De guard werkt correct. |
| `Heap: XXXXX (HEALTHY)` | Normaal heap-niveau, elke 60 seconden gelogd |
| `Heap: XXXXX (LOW)` | Heap wordt laag; berichtfrequentie vermindert |
| `HEAP-CRITICAL: Blocking WebSocket` | Heap kritiek laag; WebSocket-berichten worden gedropt |
| `HEAP-CRITICAL: Blocking MQTT` | Heap kritiek laag; MQTT-berichten worden gedropt |
| `Emergency heap recovery starting` | Automatisch herstel geactiveerd vanwege kritiek lage heap |
| `[drip] publishing discovery for OT ID ...` | MQTT discovery publiceert een entiteit tegelijk (normaal) |
| `[drip] slowed to 30s (heap pressure)` | Discovery-interval verhoogd vanwege heapdruk |
| `Nightly restart triggered` | Geplande herstart wordt uitgevoerd op het ingestelde uur |
| `MQTT: Connection refused` | MQTT-verbinding geweigerd door broker |
| `LittleFS mount failed` | Bestandssysteem niet beschikbaar |

#### Debuglog opslaan

Wilt u een langere sessie vastleggen:

```bash
nc <ip-adres> 23 | tee otgw-debug.log
```

---

### 7.10a In v2.0.0 opgeloste problemen (wat u nog in oudere builds kunt tegenkomen)

Komt u van een oudere firmware of leest u oudere forumdraadjes, dan komen onderstaande problemen regelmatig voor op v1.3.5 en eerder. Ze zijn opgelost in v2.0.0. Mocht u een van deze symptomen alsnog op een bevestigde v2.0.0-build tegenkomen, meld dit dan als GitHub-issue met de bijbehorende crashlog.

| Oud symptoom | Oorzaak (vóór 2.0.0) | Status in 2.0.0 |
|---|---|---|
| Willekeurige `Exception (3)`-reboots, vaak tijdens MQTT-activiteit | PROGMEM-pointer doorgegeven aan een functie die word-aligned leest (Arduino Core 3.1.2+ is strenger). Een flash-lezing op `0x402xxxxx` met word-alignment veroorzaakt de exception. | Opgelost. PROGMEM-veilige helpers (`pgm_strncmp_PP`, `pgm_read_char`) worden overal gebruikt; `writeMqttProgmemChunk()` voor PROGMEM naar MQTT. |
| `Exception (2)` tijdens het parsen van OT-frames | `strncmp_P` / `strstr_P` gebruikt op binaire data. Deze helpers nemen C-strings aan en lopen door over ingesloten NUL-bytes heen. | Opgelost. Binaire vergelijkingen gebruiken nu `memcmp_P`. |
| Vloedgolf van MQTT-berichten en hameren op de broker direct na een reconnect | Volledige MQTT-discovery-burst werd in één keer opnieuw verstuurd bij iedere reconnect, waardoor de heap uitgeput raakte en de broker overbelast werd. | Opgelost. Discovery is nu async en drip-published (zie 7.3). Bij een reconnect blijft de bitmap bewaard; gebruik `F` in de Telnet-console om zo nodig een volledige herpublicatie af te dwingen. |
| Klok springt vlak na opstart naar `2106-02-07` | De SDK geeft soms een onzinnige `time_t` terug vóór de eerste geslaagde NTP-uitwisseling. | Opgelost. De NTP-code negeert nu pre-epoch/post-2106-waarden tot een echte sync slaagt. Schakel toets `n` in de Telnet-console in om de handshake te volgen. |
| Apparaat onbereikbaar na een routerherstart totdat de OTGW wordt uit- en weer ingeschakeld | DHCP-lease-vernieuwing liep in een race met de WiFi-reconnectlogica, waardoor de interface zonder IP achterbleef. | Opgelost. DHCP-vernieuwing en WiFi-herstel zijn gecoördineerd (ADR-047 tweelaagse herstellogica). |
| OTGW online/offline-flikkeringen, seriele overruns tijdens MQTT-bursts (1.x-lijn) | `publishToSourceTopic()` reserveerde ~1,6 KB aan lokale buffers op de stack (1.x-lijn). | Opgelost. De buffer is statisch. Zie paragraaf 7.8. |

---

### 7.11 Crash log

Vanaf v2.0.0 slaat de firmware crash-informatie op in flash-geheugen wanneer een onafgehandelde exception of watchdog-reset optreedt. Na een crash-reboot is deze informatie op meerdere manieren beschikbaar:

1. **Web UI:** De Device Info-sectie op de hoofdpagina toont crash log details wanneer beschikbaar.
2. **REST API:** `GET /api/v2/device/crashlog` retourneert een JSON-object met `available`, `summary` en `details` velden.
3. **Telnet:** Register-dumps en exception-oorzaken worden getoond tijdens het opstarten.

Voeg bij het melden van een bug op GitHub altijd de crash log uitvoer toe. Het `summary`-veld bevat het type exception en `details` bevat registerwaarden (epc1, epc2, excvaddr, etc.) die ontwikkelaars helpen de crash in de code te lokaliseren.

---

### 7.12 BLE-temperatuursensoren

De S3 Mini ondersteunt BLE (Bluetooth Low Energy) temperatuursensor-scanning. Als BLE-sensoren niet verschijnen:

1. Controleer of BLE-scanning is ingeschakeld in Settings.
2. BLE-sensoren worden gescand op een timer-bewaakt interval. Wacht minstens een volledige scancyclus na het inschakelen.
3. Bekijk de Telnet-debuglog voor BLE-scanresultaten.

---

### 7.13 Terugzetten naar fabrieksinstellingen

"Fabrieksinstellingen" voor OTGW-firmware betekent: alle opgeslagen instellingen (WiFi, MQTT, NTP, enzovoort) verwijderen zodat het apparaat terugkeert naar de standaardconfiguratie.

#### Methode 1: Via de Web UI (volledig)

1. Ga naar de **Settings-pagina** in de Web UI.
2. Klik op **Reset Settings** (als beschikbaar) of gebruik de REST API.

Via de REST API:

```
GET http://otgw2.local/api/v2/device/settings/reset
```

Dit wist alle instellingen en herstart het apparaat met standaardwaarden.

#### Methode 2: WiFi-gegevens wissen (WiFi-reset)

Wilt u alleen de WiFi-verbinding herstellen maar overige instellingen bewaren:

1. Klik op **Reset WiFi** in de Settings-pagina, of
2. Gebruik de driedubbele hardware-reset: druk de resetknop 3 keer achter elkaar in, elke druk binnen 10 seconden na de vorige (zie paragraaf 6.1.4). Dit forceert het configuratieportaal zodat u nieuwe gegevens kunt invoeren.

#### Methode 3: Firmware en filesysteem opnieuw flashen (volledige reset)

Voor een volledige reset inclusief alle bestanden flasht u de release-zip via USB met `flash_otgw.bat` (Windows) of `./flash_otgw.sh` (Linux/macOS), zonder opties:

```bash
./flash_otgw.sh
```

Het script wist de flash en schrijft firmware en filesysteem opnieuw. Alle opgeslagen gegevens verdwijnen, inclusief instellingen, WiFi-gegevens en de LittleFS-bestandspartitie.

---

### 7.14 Hulp krijgen

#### GitHub Issues

De primaire plek voor bugrapporten en vragen over de firmware is de GitHub issues-pagina:

```
https://github.com/rvdbreemen/OTGW-firmware/issues
```

Voeg bij een bugrapport altijd het volgende mee:
- Firmwareversie (zichtbaar in de Web UI voettekst of via `/api/v2/device/info`)
- Bord (bijvoorbeeld OTGW Classic met LOLIN S3 Mini)
- Crash log (indien beschikbaar, via Web UI of `/api/v2/device/crashlog`)
- Een beschrijving van het probleem en de stappen om het te reproduceren
- Relevante uitvoer uit de Telnet-debuglog

#### Discord

Het project heeft een Discord-server voor real-time discussies, vragen en aankondigingen. De uitnodigingslink is `https://discord.gg/zjW3ju7vGQ`. De kanalen zijn in het Engels; houd dat in acht bij het stellen van vragen.

#### Tweakers

Voor Nederlandstalige discussies is het Tweakers-forum een actieve community voor OTGW-gebruikers.

---

### 7.15 Debuglog lezen: veelvoorkomende foutpatronen

Hieronder een overzicht van patronen die regelmatig in de debuglog verschijnen en wat ze betekenen.

#### WiFi-problemen

```
WiFi disconnected
Reconnecting... (attempt 1/10)
```

Het apparaat heeft de WiFi-verbinding verloren en probeert opnieuw te verbinden. Na 10 pogingen volgt een herstart. Controleer het WiFi-signaal.

```
Entered config mode SSID: OTGW2-XX:XX:XX:XX:XX:XX
```

De AP-fallbackmodus is actief. Verbind met dit SSID om de WiFi-instellingen opnieuw te configureren.

#### MQTT-problemen

```
MQTT: Connecting to 192.168.1.10:1883
MQTT: Connection failed, rc=-2
```

Foutcode `-2` betekent dat de broker niet bereikbaar is (netwerkfout). Controleer het broker-adres en of de broker actief is.

```
MQTT: Connection refused, rc=5
```

Foutcode `5` betekent fout inloggegevens. Controleer gebruikersnaam en wachtwoord in de instellingen.

| MQTT rc-code | Betekenis |
|-------------|-----------|
| -4 | Verbindingstime-out |
| -3 | Verbinding verbroken |
| -2 | Broker niet bereikbaar |
| -1 | Foutief protocol |
| 1 | Verkeerde protocolversie |
| 2 | Afgewezen client-ID |
| 3 | Broker niet beschikbaar |
| 4 | Fout gebruikersnaam/wachtwoord |
| 5 | Niet geautoriseerd |

#### OpenTherm-problemen

```
SERIAL OVERRUN detected
```

UART-buffer overgelopen. Zie paragraaf 7.8.

```
PIC not responding
```

De PIC reageert niet op het verwachte OTGW-startbericht. Controleer de seriele verbinding en de PIC-voeding.

```
OT frame parse error: XXXXXXXX
```

Een ontvangen OpenTherm-frame kon niet worden gedecodeerd. Dit kan duiden op ruis op de OpenTherm-bus of een defecte verbinding.

#### NTP-problemen

```
NTP: time not yet set
NTP: waiting for sync...
NTP: Synced, local time: 2026-04-11 14:32:00
```

Normale NTP-opstartvolgorde. Als de firmware lang in `waiting for sync` blijft, controleer:
- Is NTP ingeschakeld in de instellingen?
- Is de NTP-server bereikbaar (poort 123 UDP outbound)?
- Heeft het apparaat een werkende internetverbinding of lokale NTP-server?

#### Geheugenproblemen

```
Heap: 3200 (WARNING)
```

Het vrije geheugen zakt onder de WARNING-drempel. De firmware beperkt automatisch WebSocket- en MQTT-verkeer. Zie paragraaf 7.7 voor de volledige tabel met heap health tiers.

```
Emergency heap recovery starting (heap=2800 bytes)
```

De firmware probeert automatisch geheugen terug te winnen. Als dit regelmatig voorkomt, overweeg de nachtelijke herstart in te schakelen (paragraaf 7.7.1).

```
HEAP-CRITICAL: Blocking MQTT (dropped 5 msgs, heap=2500 bytes)
```

Heap is kritiek laag. MQTT-berichten worden gedropt om het apparaat stabiel te houden. Herstart het apparaat als directe maatregel. Meld het als issue op GitHub met de bijbehorende firmwareversie en debuglog als het probleem aanhoudt.
