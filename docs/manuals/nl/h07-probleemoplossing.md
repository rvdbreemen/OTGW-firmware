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

De PIC-microcontroller (het OpenTherm-protocol­verwerkende deel van de hardware) communiceert via een seriele verbinding met de ESP. Als de PIC niet reageert, ziet het apparaat geen OpenTherm-activiteit.

Controleer:
- Is de boiler ingeschakeld en verbonden via de OTGW-klemmen?
- Is de OpenTherm-bus correct aangesloten op de MASTER- en SLAVE-klemmen?
- Heeft de PIC stroom? Sommige OTGW-versies voeden de PIC via de OpenTherm-bus.

**Oorzaak 2: Verkeerde baudrate**

De seriële verbinding tussen ESP en PIC werkt op 9600 baud. Bij een firmwaremismatch of na een incorrecte PIC-flash kan de communicatie uitvallen.

Controleer via de Telnet-debuglog (`telnet <ip>`): verschijnen er regelmatig regels die beginnen met `T:`, `B:`, `R:`, `A:`? Zo niet, is er mogelijk een serieel communicatieprobleem.

**Oorzaak 3: PIC in diagnostische modus of interfacemodus**

Als de PIC-firmware in een niet-gateway­modus draait (bijv. door een eerdere OTmonitor-sessie), verwerkt de ESP de uitvoer niet als verwachte OTGW-regels.

Controleer de Telnet-log op de huidige gateway­modus. Verstuur via de Web UI of MQTT het commando `GW=G` om de PIC terug te zetten in gateway­modus.

**Oorzaak 4: Seriele overrun**

Bij hoog berichtenverkeer op de OpenTherm-bus kan er een seriele overrun optreden. Versie 2.0.0 rapporteert overruns als events via MQTT en WebSocket, zodat ze zichtbaar zijn.

Controleer in de Telnet-log op meldingen met `SERIAL OVERRUN` of bekijk het WebSocket-event­log in de Web UI.

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
3. Zorg dat dit overeenkomt met de `discovery_prefix` in de Home Assistant MQTT-integratie­configuratie.

#### Oorzaak 2: MQTT-integratie niet geconfigureerd in Home Assistant

Home Assistant auto-discovery vereist dat de MQTT-integratie is geconfigureerd en verbonden met dezelfde broker als de firmware.

Controleer: **Instellingen** > **Apparaten en Services** > **MQTT** en zorg dat de integratie actief is.

#### Oorzaak 3: Discovery-berichten verlopen of niet opnieuw verzonden

De firmware verstuurt discovery-berichten bij iedere MQTT-verbinding. U kunt een hernieuwde discovery forceren door de MQTT-verbinding te verbreken en te herstellen, of door de firmware te herstarten.

#### Oorzaak 4: Retained berichten van een eerdere installatie

Verouderde retained discovery-berichten van een eerdere firmware­versie of ander topic­prefix kunnen conflicten veroorzaken. Verwijder de retained berichten via een MQTT-client (bijv. MQTT Explorer) of via de Home Assistant MQTT-debug­pagina.

---

### 7.4 WiFi blijft wegvallen

#### Symptomen

- Het apparaat valt regelmatig weg uit het netwerk.
- De Web UI is intermitterend niet bereikbaar.
- Telnet-log toont herhaalde WiFi-verbindingspogingen.

#### Mogelijke oorzaken

**Zwak WiFi-signaal**

Controleer de signaalsterkte (RSSI). De Web UI toont signaal­kwaliteitsbalkjes in de header (toegevoegd in v2.0.0). RSSI hoger dan -70 dBm is doorgaans stabiel; lager dan -80 dBm geeft problemen.

Oplossingen:
- Plaats een WiFi-repeater dichter bij het OTGW-apparaat.
- Wijzig het WiFi-kanaal op uw router om interferentie te vermijden.

**IP-adresconflict bij gebruik van statisch IP**

Wanneer een statisch IP is ingesteld dat ook door DHCP wordt uitgedeeld, kan een ander apparaat hetzelfde adres krijgen. Schakel over op DHCP of reserveer het adres in uw router.

**AP-fallback activeert onterecht**

Wanneer het WiFi-signaal langdurig wegvalt, probeert de firmware maximaal 10 keer opnieuw te verbinden. Na 10 mislukte pogingen herstart het apparaat zichzelf. Dit is bedoeld als fail-safe, niet als normale werking. Controleer de WiFi-stabilteit als u regelmatige herstarts ziet.

**ESP8266 SDK auto-reconnect race condition**

In zeldzame gevallen kan een race condition in de ESP8266 SDK ervoor zorgen dat automatisch opnieuw verbinden mislukt. De tweelaagsige herstel­logica van de firmware (ADR-047) is specifiek ontworpen om dit op te vangen.

---

### 7.5 OTA-update mislukt

#### Symptomen

- De OTA-pagina geeft een foutmelding na het uploaden.
- Het apparaat herstart niet na een upload.
- De firmware versie is na de herstart ongewijzigd.

#### Controlelijst

| Probleem | Oplossing |
|---------|-----------|
| Verkeerd `.bin`-bestand | Zorg dat u het juiste platform­bestand gebruikt (ESP8266 `.bin` op ESP8266, ESP32 `.bin` op ESP32) |
| Bestand te groot | Controleer de beschikbare OTA-ruimte (zichtbaar in `build.py`-uitvoer) |
| HTTP-wachtwoord vergeten | Stel het wachtwoord opnieuw in via seriele verbinding of reset de firmware |
| Verbinding verbroken tijdens upload | Probeer opnieuw via een stabielere verbinding (bedraad netwerk of dichterbij de router) |
| Browser cached versie | Ververs met Ctrl+F5 en probeer opnieuw |

#### Na een mislukte OTA-update

Als de firmware niet meer reageert na een mislukte update:

1. Verbind het apparaat via USB.
2. Flash de firmware opnieuw via `build.py` of PlatformIO.
3. De ESP8266/ESP32 bootloader is robuust: een mislukte OTA-update overschrijft de actieve firmware niet totdat de upload volledig is geverifieerd.

---

### 7.6 Web UI laadt niet

#### Symptomen

- `http://otgw.local` geeft geen reactie of een foutpagina.
- De pagina laadt deels maar toont lege secties.

#### Stap 1: Controleer netwerk­bereikbaarheid

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

Op ESP8266 is het beschikbare geheugen (~40KB DRAM) beperkt. Bij ernstig geheugengebrek kunnen HTTP-verzoeken mislukken. De Web UI toont de vrije heap in de voettekst. Beneden 5KB is een herstart aanbevolen.

---

### 7.7 Seriële overrun en OTGW online-offline­flikkeringen

#### Wat zijn seriële overruns?

Een seriële overrun treedt op wanneer de PIC meer data naar de ESP stuurt dan de firmware kan verwerken voordat de UART-buffer vol is. Individuele bytes gaan verloren. De firmware detecteert dit via de hardware fout­vlag van de UART.

#### Symptomen

- Onvolledige OpenTherm-regels in de debuglog.
- Sporadische MQTT-berichten voor berichttypen die normaal gesproken consistent zijn.
- WebSocket-event van type `serial_overrun` zichtbaar in de Web UI.

#### Hoe ermee omgaan

Seriële overruns duiden vaak op een achterliggend probleem:

1. **MQTT-verbinding­problemen**: Een drukke MQTT-verbinding kan de verwerking vertragen en indirecte overruns veroorzaken.
2. **Heap­druk**: Bij laag geheugen vertraagt de firmware. Controleer de vrije heap.
3. **OTmonitor actief op poort 25238**: Wanneer OTmonitor actief is en commands stuurt, kan dit extra belasting geven.

#### OTGW online-offline­flikkeringen opgelost in v2.0.0

In versies voor 2.0.0 konden herhaalde disconnectie­events op de MQTT-verbinding zorgen voor een cascade van publice­ringen, waardoor de OTGW-beschikbaarheids­berichten snel achter elkaar `online` en `offline` publiceerden. De stack­overloopproblemen in `publishToSourceTopic` (te grote lokale buffers op de 4KB CONT-stack) zijn opgelost door alle grote buffers statisch te maken. Dit elimineert de crashes die de flikkeringen veroorzaakten.

Als u flikkeringen ziet na een upgrade naar v2.0.0, controleer dan:
- Of u daadwerkelijk v2.0.0 draait (zie Web UI voettekst).
- Of er geen andere oorzaak is (WiFi-onstabiliteit, MQTT-broker herstart).

---

### 7.8 Telnet-debuglog: verbinden en lezen

De Telnet-debuglog op poort 23 is het krachtigste diagnose­hulpmiddel dat de firmware biedt. Alle interne events worden hier in real-time gerapporteerd.

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

#### Nuttige logpatronen herkennen

| Logpatroon | Betekenis |
|-----------|-----------|
| `WiFi connected, IP: 192.168.x.x` | WiFi-verbinding geslaagd |
| `MQTT: Connected` | MQTT-verbinding actief |
| `NTP synced: 2026-04-11 ...` | NTP-synchronisatie gelukt |
| `T:XXXX YYYY` | OpenTherm-frame ontvangen van thermostaat (T = Thermostat) |
| `B:XXXX YYYY` | OpenTherm-frame ontvangen van boiler (B = Boiler) |
| `SERIAL OVERRUN` | UART-buffer overgelopen |
| `Heap: XXXXX` | Vrij geheugen in bytes |
| `MQTT: Connection refused` | MQTT-verbinding geweigerd door broker |
| `LittleFS mount failed` | Bestandssysteem niet beschikbaar |

#### Debuglog opslaan

Wilt u een langere sessie vastleggen:

```bash
nc <ip-adres> 23 | tee otgw-debug.log
```

---

### 7.9 Terugzetten naar fabrieksinstellingen

"Fabrieksinstellingen" voor OTGW-firmware betekent: alle opgeslagen instellingen (WiFi, MQTT, NTP, enzovoort) verwijderen zodat het apparaat terugkeert naar de standaard­configuratie.

#### Methode 1: Via de Web UI (volledig)

1. Ga naar de **Settings-pagina** in de Web UI.
2. Klik op **Reset Settings** (als beschikbaar) of gebruik de REST API.

Via de REST API:

```
GET http://otgw.local/api/v2/device/settings/reset
```

Dit wist alle instellingen en herstart het apparaat met standaard­waarden.

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

### 7.10 Hulp krijgen

#### GitHub Issues

De primaire plek voor bugrapporten en vragen over de firmware is de GitHub issues-pagina:

```
https://github.com/rvdbreemen/OTGW-firmware/issues
```

Voeg bij een bugrapport altijd het volgende mee:
- Firmware­versie (zichtbaar in de Web UI voettekst of via `/api/v2/device/info`)
- Platform (ESP8266 of ESP32)
- Een beschrijving van het probleem en de stappen om het te reproduceren
- Relevante uitvoer uit de Telnet-debuglog

#### Discord

Het project heeft een Discord-server voor real-time discussies, vragen en aankondigingen. De link staat op de GitHub-projectpagina. De kanalen zijn in het Engels; houd dat in acht bij het stellen van vragen.

#### Tweakers

Voor Nederlandstalige discussies is het Tweakers-forum een actieve community voor OTGW-gebruikers.

---

### 7.11 Debuglog lezen: veelvoorkomende foutpatronen

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
| -4 | Verbindings­time-out |
| -3 | Verbinding verbroken |
| -2 | Broker niet bereikbaar |
| -1 | Foutief protocol |
| 1 | Verkeerde protocol­versie |
| 2 | Afgewezen client-ID |
| 3 | Broker niet beschikbaar |
| 4 | Fout gebruikersnaam/wachtwoord |
| 5 | Niet geautoriseerd |

#### OpenTherm-problemen

```
SERIAL OVERRUN detected
```

UART-buffer overgelopen. Zie paragraaf 7.7.

```
PIC not responding
```

De PIC reageert niet op het verwachte OTGW-startbericht. Controleer de seriele verbinding en de PIC-voeding.

```
OT frame parse error: XXXXXXXX
```

Een ontvangen OpenTherm-frame kon niet worden ge­decodeerd. Dit kan duiden op ruis op de OpenTherm-bus of een defecte verbinding.

#### NTP-problemen

```
NTP: time not yet set
NTP: waiting for sync...
NTP: Synced, local time: 2026-04-11 14:32:00
```

Normale NTP-opstart­reeks. Als de firmware lang in `waiting for sync` blijft, controleer:
- Is NTP ingeschakeld in de instellingen?
- Is de NTP-server bereikbaar (poort 123 UDP outbound)?
- Heeft het apparaat een werkende internetverbinding of lokale NTP-server?

#### Geheugenprobleem

```
Heap low: 3200 bytes free
```

Het vrije geheugen is onder een kritieke drempel gezakt. Dit kan leiden tot instabiliteit. Herstart het apparaat. Als het probleem aanhoudt, meld het als issue op GitHub met de bijbehorende firmwareversie en debuglog.
