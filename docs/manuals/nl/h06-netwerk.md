## Hoofdstuk 6: Netwerkconfiguratie

Dit hoofdstuk beschrijft alle netwerkaspecten van OTGW-firmware: hoe het apparaat verbinding maakt met uw thuisnetwerk, hoe hostnamen worden geadverteerd, hoe de tijd wordt gesynchroniseerd en welke poorten het apparaat gebruikt. Het geeft ook richtlijnen voor gebruik achter een reverse proxy en voor firewallinstellingen.

---

### 6.1 WiFi-installatie en de AP-fallbackmodus

#### 6.1.1 Eerste verbinding

Bij de eerste opstart beschikt het apparaat nog over geen opgeslagen WiFi-instellingen. De firmware start dan automatisch de WiFiManager captive portal:

1. Het apparaat start als WiFi-accesspoint met de SSID `<hostname>-<MAC>`, standaard iets als `otgw-AABBCCDD`.
2. Verbind uw telefoon of laptop met dit netwerk.
3. Er opent automatisch een captiveportalpagina (of navigeer handmatig naar `192.168.4.1`).
4. Kies uw thuisnetwerk uit de lijst, vul het wachtwoord in en sla op.
5. Het apparaat herstart en verbindt met uw netwerk.

Zodra de verbinding is gemaakt, slaat de firmware de gegevens op in flash. Bij iedere volgende opstart verbindt het apparaat automatisch.

#### 6.1.2 Automatisch herstel bij WiFi-uitval

De firmware implementeert een tweelaags herstelmechanisme:

- **Laag 1**: De WiFi SDK (`WiFi.setAutoReconnect(true)`) herstelt kortdurende onderbrekingen (typisch binnen 30 seconden) transparant op radioniveau.
- **Laag 2**: Bij langere uitval neemt de toestandsmachine `loopWifi()` het over. Deze is volledig non-blocking: elke aanroep keert binnen minder dan een milliseconde terug, zodat de watchdog, de OpenTherm-berichtenverwerking, de MQTT-keepalives en de Web UI gewoon blijven draaien tijdens een herverbindingspoging. Per poging krijgt WiFi 30 seconden om scan, associatie en DHCP te voltooien. In productiebuilds herstart het apparaat na 10 mislukte pogingen. In betabuilds activeert de firmware na 2 mislukte pogingen de AP-fallbackmodus (zie paragraaf 6.1.3) in plaats van te herstarten.

Bij herstel van de verbinding:
- Herstelt de firmware de geconfigureerde hostnaam.
- Stuurt de firmware een DHCP-heraankondiging zodat de router de juiste hostnaam leert.
- Worden MQTT, de WebSocket-verbinding en de Telnet-debugserver automatisch opnieuw gestart.

Sinds de fix voor issue #525 wordt de SDK DHCP-client uitsluitend herstart wanneer het station *niet* verbonden is. Het aanroepen van `wifi_station_dhcpc_start()` op een geassocieerd station reset het IP-adres naar `0.0.0.0` en voorkomt herstel na een routerreboot.

#### 6.1.3 AP-fallbackmodus (beta)

Beschikbaar in betabuilds (v2.0.0-beta en later, alle platformen). Wanneer WiFi-pogingen uitgeput zijn, schakelt het apparaat over naar AP-fallbackmodus in plaats van eindeloos te herstarten. In productiereleases herstart het apparaat na 10 mislukte pogingen.

Bij opstart, als er credentials opgeslagen zijn maar WiFi niet bereikbaar is, slaan betabuilds de WiFiManager config portal over en gaan direct naar AP-fallback.

- **SSID**: `OTGW-<laatste 3 bytes van het MAC-adres in hoofdletters hex>`, bijvoorbeeld `OTGW-AABBCC`
- **IP-adres**: `192.168.4.1`
- **Wachtwoord**: `otgw123`
- **MQTT**: uitgeschakeld in fallbackmodus
- **OTA-update**: beschikbaar vanuit fallbackmodus, zodat u een gecorrigeerde firmware kunt flashen zonder serieel toegang

In AP-fallbackmodus probeert de firmware elke 5 minuten opnieuw verbinding te maken met het geconfigureerde WiFi-netwerk. Zodra WiFi beschikbaar is, worden alle services automatisch hersteld en wordt de AP afgesloten.

**Herstelstappen:**
1. Verbind met de SSID `OTGW-XXXXXX` met wachtwoord `otgw123`.
2. Open `http://192.168.4.1` in uw browser.
3. Pas de WiFi-instellingen aan via de Settings-pagina of gebruik Reset WiFi.
4. Herstart het apparaat.

#### 6.1.4 WiFi-gegevens wissen

| Methode | Hoe |
|---|---|
| Web UI | Settings-pagina, klik op de knop "Reset WiFi" |
| Driedubbele reset | Druk de hardware-resetknop 3 keer binnen 10 seconden in |
| Telnet-debugconsole | Stuur het commando `resetwifi` op poort 23 |

---

### 6.2 Ethernet (ESP32 + W5500)

Wired Ethernet is beschikbaar op ESP32-boards met een W5500 SPI Ethernet-controller. Niet beschikbaar op ESP8266.

#### 6.2.1 Hardwaredetectie

De firmware leest bij het opstarten het SPI VERSION-register (adres 0x0039) van de W5500. Als het register 0x04 retourneert, wordt de chip herkend als W5500 en wordt Ethernet-ondersteuning geactiveerd. Er is geen aparte instelling nodig: als de chip aanwezig en correct bedraad is, wordt hij automatisch gebruikt.

#### 6.2.2 GPIO-aansluitingen

De W5500 is aangesloten via de SPI-bus. De exacte pinout hangt af van het boardvariant dat is geconfigureerd in `boards.h`. Raadpleeg de hardwaredocumentatie van uw specifieke OTGW32-bord voor de precieze GPIO-nummers.

Typische aansluitingen:

| W5500 signaal | ESP32 GPIO |
|---|---|
| CS (chip select) | Zie boards.h `PIN_SPI_CS` |
| INT (interrupt) | Zie boards.h `PIN_SPI_INT` |
| RST (reset) | Zie boards.h `PIN_SPI_RST` |
| SCK (SPI clock) | Zie boards.h `PIN_SPI_SCK` |
| MISO | Zie boards.h `PIN_SPI_MISO` |
| MOSI | Zie boards.h `PIN_SPI_MOSI` |

#### 6.2.3 Prioriteit boven WiFi

Ethernet heeft altijd prioriteit. Wanneer de W5500 aanwezig is en een kabel is aangesloten, wordt WiFi uitgeschakeld. Als de kabel tijdens bedrijf wordt verwijderd, detecteert de firmware dit (elke 5 seconden gecontroleerd) en schakelt automatisch terug naar WiFi. Wordt de kabel teruggeplaatst, dan schakelt de firmware terug naar Ethernet. Deze failover verloopt zonder herstart en zonder configuratie.

#### 6.2.4 MAC-adres

De firmware leidt een uniek lokaal-beheerd MAC-adres af van het ESP32-eFuse MAC. Dit MAC-adres verschilt van het WiFi MAC-adres maar is per apparaat uniek en consistent over herstarts.

#### 6.2.5 Statisch IP op Ethernet

Ethernet ondersteunt een eigen statische IP-configuratie, los van WiFi. Configureer dit op de Settings-pagina onder het Ethernet-gedeelte. Wanneer de velden leeg zijn, gebruikt Ethernet DHCP met een timeout van 1 seconde bij het opstarten.

---

### 6.3 Statisch IP vs. DHCP

Standaard gebruikt het apparaat DHCP voor zowel WiFi als Ethernet. Dit is de aanbevolen instelling voor de meeste situaties. Als uw router DHCP-reserveringen ondersteunt, is het toewijzen van een vast IP-adres op basis van MAC-adres de beste aanpak.

Voor een statisch IP geconfigureerd in de firmware:

1. Open de **Settings-pagina** in de Web UI.
2. Vul het gewenste IP-adres, subnetmasker, gateway en DNS-server in.
3. Sla op en herstart.

Laat alle velden leeg om terug te keren naar DHCP.

> Controleer bij het gebruik van een statisch IP altijd of het adres buiten het DHCP-bereik van uw router valt om adresconflicten te vermijden.

---

### 6.4 mDNS en hostnaam

De standaard hostnaam is `otgw`. Het apparaat is hiermee bereikbaar als `otgw.local` in browsers en andere mDNS-compatibele clients (macOS, iOS, Linux met Avahi, Windows 10+ met Bonjour).

mDNS werkt op zowel ESP8266 als ESP32. Wanneer het netwerkinterface wisselt (bijvoorbeeld van WiFi naar Ethernet), registreert de firmware de mDNS-hostnaam automatisch opnieuw op het nieuwe interface.

Op ESP8266 wordt ook LLMNR (Link-Local Multicast Name Resolution, UDP poort 5355) geregistreerd. Dit maakt het apparaat vindbaar onder de naam `<hostname>` (zonder `.local`) in Windows-netwerkomgevingen, ook zonder Bonjour. LLMNR is niet beschikbaar op ESP32.

De hostnaam is instelbaar via de Settings-pagina. De nieuwe hostnaam werkt door in het DHCP-verzoek, de mDNS-advertentie, het MQTT client ID en de Web UI-titel.

#### Bekende beperking op ESP8266

De ESP8266 SDK bevat een bug waarbij `configTime()` (de NTP-initialisatiefunctie) de WiFi-hostnaam terugzet naar `ESP-XXXXXX`. De firmware detecteert dit en herstelt de hostnaam automatisch, gevolgd door een DHCP-heraankondiging. Dit gebeurt eenmalig na de eerste NTP-synchronisatie en is voor de gebruiker niet zichtbaar.

---

### 6.5 NTP-tijdsynchronisatie

#### 6.5.1 Instellingen

| Instelling | Standaard | Beschrijving |
|---|---|---|
| NTP-server | `pool.ntp.org` | Elk NTP-serveradres (hostnaam of IP) |
| Tijdzone | `Europe/Amsterdam` | IANA-tijdzonenaam (zie hieronder) |
| NTP ingeschakeld | `true` | Kan worden uitgeschakeld als er geen internettoegang is |

#### 6.5.2 Tijdzoneafhandeling (AceTime)

De firmware gebruikt de AceTime-bibliotheek (4.x) voor tijdzoneafhandeling. U configureert de tijdzone met standaard IANA-tijdzonenamen (bijvoorbeeld `Europe/Amsterdam`, `America/New_York`, `Asia/Tokyo`). Deze namen worden opgezocht in de ingebouwde tijdzonedatabase van AceTime, die DST-regels en historische wijzigingen automatisch afhandelt.

In tegenstelling tot een POSIX-tijdzonestring hoeft u geen DST-overgangsregels handmatig op te geven. Voer gewoon de IANA-naam in en AceTime regelt de rest.

Als de geconfigureerde tijdzonenaam ongeldig is of niet wordt gevonden in de database, valt de firmware terug op de standaardwaarde (`Europe/Amsterdam`) en registreert een waarschuwing in de debuglog.

#### 6.5.3 Waarvoor wordt NTP gebruikt?

De firmware controleert de tijd elke 30 minuten opnieuw. Na elke geslaagde NTP-synchronisatie stuurt de firmware tijdcommando's naar de boiler-PIC (`SC=HH:MM/D`, `SR=21:MM,DD`, `SR=22:YH,YL`).

NTP-tijd is ook vereist voor de nachtelijke herstart (zie paragraaf 6.5.4) en voor tijdstempels in debuglogs en MQTT-berichten.

#### 6.5.4 Nachtelijke herstart voor geheugenherstel

De ESP8266 heeft last van heap-fragmentatie over langere tijd. De firmware biedt een optionele geplande nachtelijke herstart om geheugen terug te winnen. Deze functie is standaard uitgeschakeld.

| Instelling | Standaard | Beschrijving |
|---|---|---|
| Nightly Restart | `uit` | Inschakelen om eenmaal per dag te herstarten op het geconfigureerde uur |
| Nightly Restart Hour | `4` | Lokaal uur (0-23) waarop de herstart plaatsvindt |

Voorwaarden voor de herstart:
- De instelling moet ingeschakeld zijn.
- NTP moet ingeschakeld en gesynchroniseerd zijn.
- Het apparaat moet langer dan 1 uur draaien (voorkomt herstartlussen na een recente herstart).

De herstart veroorzaakt een korte serviceonderbreking van ongeveer 30 seconden.

#### 6.5.5 ESP8266 SDK-tijdbug

De ESP8266 SDK initialiseert `time()` met de waarde `0xFFFFFFFF` (jaar 2106) voordat de eerste SNTP-synchronisatie is voltooid. De firmware herkent deze foutieve waarde en negeert deze, zodat tijdstempels nooit worden vergiftigd door een verkeerde begintijd.

---

### 6.6 OTA-updates

OTA (Over-The-Air) staat voor draadloze firmware-updates. Hiermee kunt u de firmware bijwerken zonder USB-kabel.

Het OTA-update-endpoint is `/update`. Wanneer een HTTP-wachtwoord is ingesteld, is de pagina beveiligd met HTTP Basic Auth.

**Stappen:**
1. Navigeer naar `http://otgw.local/update` in uw browser.
2. Selecteer het firmwarebestand.
3. Klik op Upload.
4. Het apparaat flashed de firmware, verifieert de image en herstart.

OTA is ook beschikbaar vanuit de AP-fallbackmodus, zodat u een apparaat dat vastzit op een slechte firmware kunt herstellen zonder serieel toegang.

#### 6.6.1 ESP32: Merged binary-ondersteuning

Op ESP32 accepteert de OTA-updatepagina een merged binary (firmware + bestandssysteem gecombineerd in een enkel `.bin`-bestand). De updateserver detecteert het merged formaat automatisch en schrijft elk onderdeel naar de juiste partitie. U hoeft firmware en bestandssysteem niet apart te uploaden.

#### 6.6.2 Beperkingen

- Flash nooit PIC-firmware via OTA. De OTA-updatepagina is uitsluitend voor ESP-firmware. Het flashen van PIC-firmware over het netwerk met OTmonitor kan de PIC permanent beschadigen.
- Op ESP8266 is de beschikbare OTA-ruimte beperkt door de flashgrootte. Het `build.py`-script rapporteert de beschikbare ruimte.

---

### 6.7 ESP8266: lwIP Low Memory-variant en Arduino-core 3.1.2

Sinds v2.0.0-beta is de ESP8266-build overgezet van Arduino-core 2.7.4 naar 3.1.2 en wordt gebouwd met de lwIP v2 Low Memory-variant (`ip=lm2f`, TCP MSS=536). Ten opzichte van de Higher Bandwidth-variant halveert dit ruwweg de TCP-ontvangst- en verzendbuffers per socket, wat belangrijk is gezien het beperkte werkgeheugen van de ESP8266 (~40KB bruikbaar RAM). De lwIP-variant is een keuze op buildtijd en vereist geen gebruikersconfiguratie.

Op het MQTT-publicatiepad activeert de firmware bovendien WiFiClient sync-mode (`setSync(true)`). Hierdoor wordt de tijdelijke ~1KB `TCP_SND_BUF`-kopie binnen de Arduino WiFiClient overgeslagen: writes gaan rechtstreeks naar lwIP in plaats van via een tussenbuffer. Samen met `setNoDelay(true)` daalt het data-plane-RAM per MQTT-verbinding van ongeveer 4,8KB naar 3,7KB. Dit geldt uitsluitend voor het MQTT-publicatiepad; de webserver, WebSocket en telnet-sockets blijven de standaard (gebufferde) WiFiClient-modus gebruiken.

---

### 6.8 Poortgebruik

| Poort | Protocol | Functie | Opmerkingen |
|---|---|---|---|
| 80 | TCP / HTTP | Web UI, REST API en OTA-update | Alle webroutes, bestandsbediening, firmware-update |
| 81 | TCP / WebSocket | Live OpenTherm-logstream | Gebruikt door de Web UI voor realtime data |
| 23 | TCP / Telnet | Debugconsole | Tekstuele debuglog; bediend door SimpleTelnet-bibliotheek |
| 25238 | TCP | Seriele bridge (ser2net) | Ruwe OTGW PIC-serieel over TCP; bediend door SimpleTelnet-bibliotheek; OTmonitor-compatibel |
| 123 | UDP (uitgaand) | NTP (SNTP) | Alleen uitgaand |
| 5353 | UDP | mDNS | Lokale naamresolutie (beide platformen) |
| 5355 | UDP | LLMNR | Windows-naamresolutie (alleen ESP8266) |
| 1883 | TCP (uitgaand) | MQTT | Uitgaand naar uw MQTT-broker |

---

### 6.9 Firewall- en routeroverwegingen

OTGW-firmware is uitsluitend bedoeld voor gebruik op een vertrouwd lokaal netwerk. Er hoeven geen poorten te worden doorgestuurd op uw router.

mDNS (`otgw.local`) werkt alleen binnen een enkel Layer 2 broadcast-domein. Als Home Assistant op een ander VLAN staat, gebruik dan het statische IP-adres of een DNS-vermelding in plaats van `.local`.

---

### 6.10 Gebruik achter een reverse proxy

De REST API en Web UI werken correct achter een HTTPS-terminating reverse proxy (Nginx, Caddy, etc.). De WebSocket live log (poort 81) vereist een plain `ws://`-verbinding en ondersteunt geen `wss://`. Een reverse proxy die WebSocket-verbindingen opwaardeert naar WSS zal de live log in de webinterface breken.

---
