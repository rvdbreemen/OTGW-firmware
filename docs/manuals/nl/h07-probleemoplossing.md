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

**ESP8266 SDK auto-reconnect race condition**

In zeldzame gevallen kan een race condition in de ESP8266 SDK ervoor zorgen dat automatisch opnieuw verbinden mislukt. De tweelaagsige herstellogica van de firmware (ADR-047) is specifiek ontworpen om dit op te vangen.

---

### 7.5 OTA-update mislukt

#### Symptomen

- De OTA-pagina geeft een foutmelding na het uploaden.
- Het apparaat herstart niet na een upload.
- De firmware versie is na de herstart ongewijzigd.

#### Controlelijst

| Probleem | Oplossing |
|---------|-----------|
| Verkeerd `.bin`-bestand | Zorg dat u het juiste platformbestand gebruikt (ESP8266 `.bin` op ESP8266, ESP32 `.bin` op ESP32) |
| Bestand te groot | Controleer de beschikbare OTA-ruimte (zichtbaar in `build.py`-uitvoer) |
| HTTP-wachtwoord vergeten | Stel het wachtwoord opnieuw in via seriele verbinding of reset de firmware |
| Verbinding verbroken tijdens upload | Probeer opnieuw via een stabielere verbinding (bedraad netwerk of dichterbij de router) |
| Browser cached versie | Ververs met Ctrl+F5 en probeer opnieuw |

#### Na een mislukte OTA-update

Als de firmware niet meer reageert na een mislukte update:

1. Verbind het apparaat via USB.
2. Flash de firmware opnieuw via `build.py` of PlatformIO.
3. De ESP8266/ESP32 bootloader is robuust: een mislukte OTA-update overschrijft de actieve firmware niet totdat de upload volledig is geverifieerd.

> **WAARSCHUWING: Flash PIC-firmware NOOIT via WiFi.**
>
> De ESP-firmware kan veilig via WiFi worden geüpdatet (OTA), maar de PIC-microcontroller op de OTGW-print is een afzonderlijke chip met een eigen flash-pad. Het gebruik van OTmonitor via de TCP-seriele socket om PIC-firmware te uploaden (bijvoorbeeld een upgrade naar PIC v6.6) **staat bekend om het brick'en van de PIC**: een WiFi-hapering halverwege de overdracht laat de PIC in een half-geprogrammeerde toestand achter, waaruit alleen in-circuit herprogrammering nog redding biedt.
>
> Flash PIC-firmware altijd via een **bedrade USB-seriele verbinding** met OTmonitor of het daarvoor bedoelde PIC-programmeergereedschap. Dit is een hardware-beperking, geen firmware-bug, en de OTGW-firmware kan dit niet omzeilen.

---

### 7.6 Web UI laadt niet

#### Symptomen

- `http://otgw.local` geeft geen reactie of een foutpagina.
- De pagina laadt deels maar toont lege secties.

#### Stap 1: Controleer netwerkbereikbaarheid

```bash
ping otgw.local
```

Reageert de ping niet, controleer dan:
- Is het apparaat verbonden met WiFi? Controleer de router voor verbonden apparaten.
- Is mDNS beschikbaar? Probeer het IP-adres rechtstreeks in plaats van `otgw.local`.

#### Stap 2: Controleer de HTTP-server

Open de Telnet-debuglog en probeer de pagina opnieuw te laden. Verschijnen er HTTP-verzoeken in de log? Zo niet, reageert de webserver niet.

#### Stap 3: LittleFS niet gemount

Als de LittleFS-partitie (het bestandssysteem met de webbestanden) niet kon worden gemount, zijn de HTML/CSS/JS-bestanden niet beschikbaar. De Telnet-log toont dan een melding als `LittleFS mount failed`.

Oplossing: upload het filesysteem opnieuw via `build.py --filesystem` of PlatformIO.

#### Stap 4: Geheugengebrek

Op ESP8266 is het beschikbare geheugen (~40KB DRAM) beperkt. Bij ernstig geheugengebrek kunnen HTTP-verzoeken mislukken. De Web UI toont de vrije heap in de voettekst. Zie paragraaf 7.7 voor de heap health tiers en wat u kunt doen bij een lage heap.

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

#### Wat zijn seriële overruns?

Een seriële overrun treedt op wanneer de PIC meer data naar de ESP stuurt dan de firmware kan verwerken voordat de UART-buffer vol is. Individuele bytes gaan verloren. De firmware detecteert dit via de hardware foutvlag van de UART.

#### Symptomen

- Onvolledige OpenTherm-regels in de debuglog.
- Sporadische MQTT-berichten voor berichttypen die normaal gesproken consistent zijn.
- WebSocket-event van type `serial_overrun` zichtbaar in de Web UI.

#### Hoe ermee omgaan

Seriële overruns duiden vaak op een achterliggend probleem:

1. **MQTT-verbindingsproblemen**: Een drukke MQTT-verbinding kan de verwerking vertragen en indirecte overruns veroorzaken.
2. **Heapdruk**: Bij laag geheugen vertraagt de firmware. Controleer de vrije heap (zie paragraaf 7.7).
3. **OTmonitor actief op poort 25238**: Wanneer OTmonitor actief is en commands stuurt, kan dit extra belasting geven.

#### OTGW online-offline-flikkeringen opgelost in v2.0.0

In versies voor 2.0.0 konden herhaalde disconnectie-events op de MQTT-verbinding zorgen voor een cascade van publicaties, waardoor de OTGW-beschikbaarheidsberichten snel achter elkaar `online` en `offline` publiceerden. De stackoverloopproblemen in `publishToSourceTopic` (te grote lokale buffers op de 4KB CONT-stack) zijn opgelost door alle grote buffers statisch te maken. Dit elimineert de crashes die de flikkeringen veroorzaakten.

Als u flikkeringen ziet na een upgrade naar v2.0.0, controleer dan:
- Of u daadwerkelijk v2.0.0 draait (zie Web UI voettekst).
- Of er geen andere oorzaak is (WiFi-onstabiliteit, MQTT-broker herstart).

---

### 7.9 LED-patronen

De firmware gebruikt status-LEDs om de apparaatstatus aan te geven. Op ESP32-borden (OTGW32) worden de LEDs via PWM gedimd voor een lagere helderheid.

| LED-patroon | Betekenis |
|-------------|-----------|
| LED2 knippert 1x per seconde, LED1 uit | Geen WiFi-verbinding. Het apparaat probeert opnieuw te verbinden. |
| LED1 knippert 1x per seconde, LED2 knippert 2x per seconde | WiFi is verbonden maar er is al meer dan 10 seconden geen OpenTherm-verkeer ontvangen. Controleer de PIC- en boilerverbinding. |
| Beide LEDs uit (normale werking) | WiFi verbonden, OpenTherm-verkeer actief. LEDs worden alleen kort gebruikt voor verwerkingsindicatie. |
| Snelle knipperingen tijdens opstarten | Setup-fase. Drie snelle knipperingen signaleren voltooiing van een setup-stap. |

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
| 6 | OTDirect frame handling en PI-lus (standaard aan, alleen ESP32) |
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
| OTGW online/offline-flikkeringen, seriele overruns tijdens MQTT-bursts | `publishToSourceTopic()` reserveerde ~1,6 KB aan lokale buffers op de 4 KB CONT-stack. | Opgelost. Grote buffers omgezet naar statisch of pre-allocated scratch-geheugen. Zie paragraaf 7.8. |

---

### 7.11 Crash log

Vanaf v2.0.0 slaat de firmware crash-informatie op in flash-geheugen wanneer een onafgehandelde exception of watchdog-reset optreedt. Na een crash-reboot is deze informatie op meerdere manieren beschikbaar:

1. **Web UI:** De Device Info-sectie op de hoofdpagina toont crash log details wanneer beschikbaar.
2. **REST API:** `GET /api/v2/device/crashlog` retourneert een JSON-object met `available`, `summary` en `details` velden.
3. **Telnet:** Register-dumps en exception-oorzaken worden getoond tijdens het opstarten.

Voeg bij het melden van een bug op GitHub altijd de crash log uitvoer toe. Het `summary`-veld bevat het type exception en `details` bevat registerwaarden (epc1, epc2, excvaddr, etc.) die ontwikkelaars helpen de crash in de code te lokaliseren.

---

### 7.12 ESP32-specifieke probleemoplossing (OTGW32)

Het OTGW32-bord gebruikt een ESP32-S3 en beschikt over hardwaremogelijkheden die niet aanwezig zijn op de ESP8266-versie.

#### OTDirect (direct OpenTherm zonder PIC)

De OTGW32 heeft geen PIC-microcontroller. In plaats daarvan verwerkt de ESP32-S3 het OpenTherm-protocol rechtstreeks via GPIO (OTDirect). Als de OpenTherm-communicatie niet werkt:

1. Controleer of de 18V step-up converter is ingeschakeld (hardwarevereiste voor OT-signalering).
2. Bekijk de Telnet-debuglog voor OTDirect-gerelateerde berichten. Schakel debug-kanaal 6 in via de Telnet-console voor gedetailleerde OTDirect frame handling.
3. Zorg dat de boiler en thermostaat op de juiste MASTER- en SLAVE-klemmen zijn aangesloten.

#### BLE-temperatuursensoren

De ESP32 ondersteunt BLE (Bluetooth Low Energy) temperatuursensor-scanning. Als BLE-sensoren niet verschijnen:

1. Controleer of BLE-scanning is ingeschakeld in Settings.
2. BLE-sensoren worden gescand op een timer-bewaakt interval. Wacht minstens een volledige scancyclus na het inschakelen.
3. Bekijk de Telnet-debuglog voor BLE-scanresultaten.

#### Ethernet (W5500)

Het OTGW32-bord ondersteunt een W5500 Ethernet-module als alternatief voor WiFi:

1. Ethernet is alleen beschikbaar op borden waar `HAS_ETH_CAPABLE` is ingesteld (OTGW32).
2. Wanneer Ethernet is aangesloten en actief, kan WiFi als fallback blijven functioneren.
3. Bekijk de Telnet-debuglog voor Ethernet link status berichten.

---

### 7.13 Terugzetten naar fabrieksinstellingen

"Fabrieksinstellingen" voor OTGW-firmware betekent: alle opgeslagen instellingen (WiFi, MQTT, NTP, enzovoort) verwijderen zodat het apparaat terugkeert naar de standaardconfiguratie.

#### Methode 1: Via de Web UI (volledig)

1. Ga naar de **Settings-pagina** in de Web UI.
2. Klik op **Reset Settings** (als beschikbaar) of gebruik de REST API.

Via de REST API:

```
GET http://otgw.local/api/v2/device/settings/reset
```

Dit wist alle instellingen en herstart het apparaat met standaardwaarden.

#### Methode 2: WiFi-gegevens wissen (WiFi-reset)

Wilt u alleen de WiFi-verbinding herstellen maar overige instellingen bewaren:

1. Klik op **Reset WiFi** in de Settings-pagina, of
2. Gebruik de driedubbele hardware-reset (zie paragraaf 6.1).

#### Methode 3: Firmware en filesysteem opnieuw flashen (volledige reset)

Voor een volledige reset inclusief alle bestanden:

```bash
python build.py --clean
```

Dit wist alle opgeslagen gegevens inclusief instellingen, WiFi-gegevens en de LittleFS-bestandspartitie, en plaatst een volledig schone installatie.

---

### 7.14 Hulp krijgen

#### GitHub Issues

De primaire plek voor bugrapporten en vragen over de firmware is de GitHub issues-pagina:

```
https://github.com/rvdbreemen/OTGW-firmware/issues
```

Voeg bij een bugrapport altijd het volgende mee:
- Firmwareversie (zichtbaar in de Web UI voettekst of via `/api/v2/device/info`)
- Platform (ESP8266 of ESP32)
- Crash log (indien beschikbaar, via Web UI of `/api/v2/device/crashlog`)
- Een beschrijving van het probleem en de stappen om het te reproduceren
- Relevante uitvoer uit de Telnet-debuglog

#### Discord

Het project heeft een Discord-server voor real-time discussies, vragen en aankondigingen. De link staat op de GitHub-projectpagina. De kanalen zijn in het Engels; houd dat in acht bij het stellen van vragen.

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
Entered config mode SSID: otgw-XXXXXXXXXXXX
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
