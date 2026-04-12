## Hoofdstuk 6: Netwerkconfiguratie

Dit hoofdstuk beschrijft alle netwerkaspecten van OTGW-firmware: hoe het apparaat verbinding maakt met uw thuis­netwerk, hoe hostnamen worden geadverteerd, hoe de tijd wordt gesynchroniseerd en welke poorten het apparaat gebruikt. Het geeft ook richtlijnen voor gebruik achter een reverse proxy en voor firewall­instellingen.

---

### 6.1 WiFi-installatie en de AP-fallbackmodus

#### Eerste verbinding

Bij de eerste opstart beschikt het apparaat nog over geen opgeslagen WiFi-instellingen. De firmware start dan automatisch de WiFiManager captive portal:

1. Het apparaat start als WiFi-accesspoint met de SSID `<hostname>-<MAC>`, standaard iets als `otgw-00112233AABB`.
2. Verbind uw telefoon of laptop met dit netwerk.
3. Er opent automatisch een captivaportal­pagina (of navigeer handmatig naar `192.168.4.1`).
4. Kies uw thuisnetwerk uit de lijst, vul het wachtwoord in en sla op.
5. Het apparaat herstart en verbindt met uw netwerk.

Zodra de verbinding is gemaakt, slaat de firmware de gegevens op in flash. Bij iedere volgende opstart verbindt het apparaat automatisch.

#### AP-fallbackmodus

Zijn de opgeslagen gegevens niet meer geldig (SSID gewijzigd, wachtwoord veranderd), dan activeert de firmware na het mislukken van de verbindingspoging opnieuw de captive portal. U herkent dit aan de `<hostname>-<MAC>` SSID die opnieuw zichtbaar wordt in de WiFi-scanner.

De portal-SSID bevat geen Info- of Erase-knoppen: die zijn om veiligheidsredenen uitgeschakeld.

#### WiFi-gegevens wissen

Er zijn twee manieren om opgeslagen WiFi-gegevens te wissen en zo de captive portal te forceren:

**Methode 1: Via de Web UI (aanbevolen)**

1. Ga naar de Settings-pagina in de Web UI.
2. Het huidige verbonden SSID staat weergegeven als een alleen-lezen veld.
3. Klik op de knop **Reset WiFi**.
4. Het apparaat wist de gegevens en herstart naar de captive portal.

**Methode 2: Driedubbele hardware-reset (noodherstel)**

Wanneer de Web UI niet bereikbaar is:

1. Druk de hardware-resetknop van het ESP8266/ESP32-board **3 keer binnen 10 seconden** in.
2. Bij de derde reset detecteert de firmware het patroon.
3. De WiFi-gegevens worden gewist en de captive portal opent.

> Deze methode werkt ook als u bent opgesloten en geen toegang hebt tot de Web UI of de REST API. Druk te langzaam (meer dan 10 seconden voor alle drie resets), dan werkt de detectie niet en start het apparaat normaal op.

#### Automatisch herstel bij WiFi-uitval

De firmware implementeert een twee­laags herstel­mechanisme (ADR-047):

- **Laag 1**: De WiFi SDK probeert automatisch opnieuw verbinding te maken bij kortdurende onderbrekingen (typisch binnen 30 seconden).
- **Laag 2**: Een toestandsmachine in de firmware (`loopWifi()`) detecteert langere uitvallen. Na maximaal 10 mislukte verbindingspogingen herstart het apparaat zichzelf automatisch.

Bij herstel van de verbinding worden MQTT, de WebSocket-verbinding en de Telnet-debugserver automatisch opnieuw gestart.

---

### 6.2 Ethernet (W5500) op ESP32

Op ESP32-boards met een aangesloten W5500 SPI Ethernet-controller kan de firmware wired Ethernet gebruiken als primaire netwerkverbinding. Dit is uitsluitend van toepassing op ESP32-hardware; ESP8266-boards ondersteunen geen Ethernet.

#### Bedrading

De W5500 is aangesloten via de SPI-bus. De exacte pinout hangt af van het boardvariant dat is geconfigureerd in `boards.h`. Raadpleeg de hardware­documentatie van uw specifieke OTGW32-bord voor de precieze GPIO-nummers.

Typische aansluitingen:

| W5500 pin | ESP32 GPIO |
|-----------|-----------|
| MOSI | Zie boards.h `PIN_SPI_MOSI` |
| MISO | Zie boards.h `PIN_SPI_MISO` |
| SCK | Zie boards.h `PIN_SPI_SCK` |
| CS | Zie boards.h `PIN_SPI_CS` |
| RST | Zie boards.h `PIN_ETH_RST` |

#### Inschakelen en detectie

De W5500-ondersteuning wordt automatisch geactiveerd wanneer de firmware het hardware-feature-flag `HAS_ETH_CAPABLE` heeft en een W5500-chip detecteert via het SPI VERSION-register bij het opstarten. Er is geen aparte instelling om Ethernet aan te zetten: als de chip aanwezig en correct bedraad is, wordt hij gebruikt.

#### Prioriteit boven WiFi

Wanneer Ethernet actief is, heeft het prioriteit:

1. Bij opstart probeert de firmware DHCP op het Ethernet-interface (timeout: 1 seconde).
2. Slaagt DHCP, dan schakelt WiFi uit. Het apparaat opereert volledig via Ethernet.
3. Faalt DHCP of er is geen kabel, dan valt de firmware terug op WiFi.
4. Tijdens bedrijf controleert de firmware elke 5 seconden de kabelstatus.
5. Wordt de kabel verwijderd, dan herstart WiFi automatisch.
6. Wordt de kabel teruggeplaatst, dan schakelt de firmware terug naar Ethernet.

Deze automatische failover verloopt zonder herstart en zonder configuratie.

#### MAC-adres

De firmware leidt een uniek lokaal-beheerd MAC-adres af van het ESP32-eFuse MAC. Dit MAC-adres verschilt van het WiFi MAC-adres maar is per apparaat uniek en consistent.

---

### 6.3 Statisch IP vs. DHCP

Standaard gebruikt het apparaat DHCP voor zowel WiFi als Ethernet. Dit is de aanbevolen instelling voor de meeste situaties.

#### DHCP (standaard)

- Het apparaat vraagt automatisch een IP-adres aan bij de router.
- Gebruikt mDNS (`otgw.local`) zodat u het apparaat op naam kunt bereiken, ongeacht het IP-adres.
- Geen handmatige configuratie vereist.

#### Statisch IP

Wanneer u een vast IP-adres vereist (bijv. voor strikte firewallregels of wanneer mDNS niet beschikbaar is):

1. Open de **Settings-pagina** in de Web UI.
2. Vul het gewenste IP-adres, subnetmasker, gateway en DNS-server in.
3. Sla op en herstart.

> Controleer bij het gebruik van een statisch IP altijd of het adres buiten het DHCP-bereik van uw router valt om adresconflicten te vermijden. Een conflict veroorzaakt intermitterende verbindingsproblemen die lastig te diagnosticeren zijn.

---

### 6.4 mDNS en hostnaam

#### Standaardhostnaam

De standaard hostnaam is `otgw`. Het apparaat is hiermee bereikbaar als `otgw.local` in browsers en andere mDNS-compatibele clients.

#### Hostnaam aanpassen

De hostnaam is instelbaar via de Settings-pagina. Pas ook de hostnaam aan in uw MQTT-instellingen als u die gebruikt voor het bepalen van topic-prefixen.

#### mDNS

De firmware registreert:

- `<hostname>.local` als mDNS-hostnaam (UDP poort 5353, alle platforms)
- Een HTTP-service op poort 80, zodat applicaties die mDNS-service-discovery ondersteunen het apparaat automatisch vinden

#### LLMNR (Windows, ESP8266)

Op ESP8266 wordt ook LLMNR (Link-Local Multicast Name Resolution, UDP poort 5355) geregistreerd. Dit maakt het apparaat vindbaar onder de naam `<hostname>` (zonder `.local`) in Windows-netwerkomgevingen, ook zonder Bonjour.

#### Bekende beperking op ESP8266

De ESP8266 SDK bevat een bug waarbij `configTime()` (de NTP-initialisatiefunctie) de WiFi-hostnaam terugzet naar `ESP-XXXXXX`. De firmware detecteert dit en herstelt de hostnaam automatisch, gevolgd door een DHCP-heraankondiging. Dit gebeurt eenmalig na de eerste NTP-synchronisatie en is voor de gebruiker niet zichtbaar.

---

### 6.5 NTP-tijdsynchronisatie

#### Waarvoor wordt NTP gebruikt?

NTP-tijdsynchronisatie heeft twee doelen:

1. **Boilertijd instellen**: De firmware stuurt periodiek tijdcommando's naar de PIC (`SC=HH:MM/DOW`, `SR=21:` voor datum, `SR=22:` voor jaar), zodat de boiler de correcte tijd kent voor tijdgerelateerde functies.
2. **Logstempels**: Tijdstempels in MQTT-berichten, de Web UI en de Telnet-debuglog zijn alleen zinvol als de tijd gesynchroniseerd is.

#### NTP in- en uitschakelen

NTP staat standaard ingeschakeld. U kunt het uitschakelen via de Settings-pagina als uw netwerk geen internettoegang heeft en u geen lokale NTP-server gebruikt.

#### NTP-server instellen

De standaard NTP-server is `pool.ntp.org`. U kunt een eigen server opgeven, bijv. een lokale NTP-server op uw thuisnetwerk:

1. Ga naar **Settings** in de Web UI.
2. Vul onder NTP het serveradres in (hostnaam of IP).
3. Sla op en herstart.

#### Tijdzone instellen

De firmware gebruikt de POSIX-tijdzone-notatie voor lokale tijdconversie. Voorbeelden:

| Tijdzone | POSIX-string |
|----------|-------------|
| Midden-Europa (CET/CEST) | `CET-1CEST,M3.5.0,M10.5.0/3` |
| Groot-Brittannie (GMT/BST) | `GMT0BST,M3.5.0/1,M10.5.0` |
| UTC | `UTC0` |

Stel de tijdzone in via de Settings-pagina.

#### NTP-synchronisatiestatus

Na het opstarten doorloopt de firmware de volgende toestanden:

1. `TIME_NOTSET`: Nog geen synchronisatie geprobeerd.
2. `TIME_WAITFORSYNC`: Wacht op eerste synchronisatie.
3. `TIME_SYNC`: Tijd gesynchroniseerd.
4. `TIME_NEEDSYNC`: Tijd verlopen, hernieuwde synchronisatie nodig.

De Telnet-debuglog meldt de lokale datum en tijd zodra de eerste synchronisatie slaagt.

> De firmware bevat een beveiliging tegen een bekende ESP8266 SDK-bug waarbij het tijdsregister voor de eerste synchronisatie de waarde `0xFFFFFFFF` bevat (het jaar 2106). Deze waarde wordt herkend en genegeerd, zodat uw tijdstempels niet worden vergiftigd door een verkeerde begintijd.

---

### 6.6 OTA-updates

OTA (Over-The-Air) staat voor draadloze firmware­updates. Hiermee kunt u de firmware bijwerken zonder USB-kabel.

#### OTA activeren

OTA is ingebouwd en altijd beschikbaar via de webinterface op:

```
http://otgw.local/update
```

of via het IP-adres van het apparaat. U ziet hier een pagina waarmee u een `.bin`-bestand kunt uploaden.

#### Beveiliging

Wanneer u een HTTP-wachtwoord hebt ingesteld in de Settings-pagina (`sHTTPpasswd`), is de OTA-updatepagina beveiligd met HTTP Basic Auth. Zonder wachtwoord is de pagina voor iedereen op het lokale netwerk bereikbaar.

> Het project is uitsluitend bedoeld voor gebruik op een vertrouwd thuisnetwerk, niet rechtstreeks op het internet. Zie ADR-032 voor de security­overwegingen. Stel een wachtwoord in als u het apparaat niet volledig vertrouwt op uw netwerk.

#### OTA-beperkingen

- **Nooit PIC-firmware via OTA flashen**: Via de OTA-update­pagina kan uitsluitend de ESP-firmware worden geupload, niet de PIC-firmware. Het flashen van PIC-firmware over het netwerk met OTmonitor kan de PIC permanent beschadigen.
- **Schetsgrootte**: Op ESP8266 is de beschikbare OTA-ruimte beperkt door de flashgrootte. De `build.py`-tool rapporteert de beschikbare ruimte.

#### Processtappen voor een OTA-update

1. Download het nieuwe `.bin`-bestand van de GitHub releases-pagina.
2. Navigeer naar `http://otgw.local/update`.
3. Klik op **Choose File** en selecteer het `.bin`-bestand.
4. Klik op **Update**.
5. Wacht tot het apparaat herstart (typisch 30-60 seconden).
6. Ververs de pagina en controleer de firmwareversie in de Web UI.

---

### 6.7 Poortgebruik

Het apparaat luistert op vier TCP-poorten:

| Poort | Protocol | Functie |
|-------|----------|---------|
| 80 | HTTP | Web UI, REST API (`/api/v1/`, `/api/v2/`), OTA-update (`/update`) |
| 81 | WebSocket | Live OpenTherm-logstream voor de Web UI |
| 23 | TCP (Telnet) | Firmware-debuglog (real-time, leesbaar via `telnet <ip>`) |
| 25238 | TCP (raw) | Serieel bridge (OTmonitor-compatibel, HA built-in integration) |

#### Poort 80: HTTP

Alle gebruikersinteractie gaat via poort 80: de Web UI, alle REST API-endpoints en de OTA-updatepagina. Optionele HTTP Basic Auth beschermt de UI en het API wanneer een wachtwoord is ingesteld.

#### Poort 81: WebSocket

De Web UI maakt een WebSocket-verbinding op poort 81 voor de live OpenTherm-log. Dit is een ongeauthenticeerde stroom; het is bewuste keuze conform de security-policy (vertrouwd LAN). WSS (beveiligde WebSocket) wordt niet ondersteund.

#### Poort 23: Telnet-debugserver

De volledige firmware-debuglog is real-time beschikbaar via Telnet. Verbinden:

```bash
telnet <ip-adres>
```

of met netcat:

```bash
nc <ip-adres> 23
```

De log bevat alle `DebugTln()`/`DebugTf()`-uitvoer: WiFi-events, MQTT-verbinding, NTP-synchronisatie, OpenTherm-verwerking en foutmeldingen.

> Nooit `Serial.print()` gebruiken voor debug­uitvoer. De UART is exclusief gereserveerd voor de PIC-serieelverbinding. Alle debug­uitvoer loopt via Telnet poort 23.

#### Poort 25238: TCP serieel bridge

Poort 25238 is de OTmonitor-compatibele TCP serieel bridge. Hierop wordt de ruwe PIC-serieel­stroom doorgegeven, precies zoals een directe serieel­verbinding. Gebruik:

- OTmonitor (stel als host het IP-adres of `otgw.local` in, poort 25238)
- Home Assistant built-in OTGW-integration
- `nc <ip> 25238` voor handmatige inspectie

> Flashen van PIC-firmware via deze poort met OTmonitor is gevaarlijk en kan de PIC permanent beschadigen. Gebruik uitsluitend de webgebaseerde PIC-flashfunctie in de Web UI.

---

### 6.8 Firewall- en routeroverwegingen

OTGW-firmware is uitsluitend bedoeld voor gebruik op een vertrouwd lokaal netwerk. Het apparaat heeft geen authenticatie op poorten 81 en 25238, conform ADR-032 (geen authenticatie op LAN).

#### Aanbevolen firewallconfiguratie

- Blokkeer alle poorten (80, 81, 23, 25238) op de internetgrens van uw router.
- Het apparaat heeft uitsluitend toegang nodig tot:
  - Uw WiFi-router/DHCP-server (intern)
  - Uw MQTT-broker (intern, of VPN)
  - De NTP-server (UDP poort 123, outbound; of een lokale NTP-server)

#### Toegang op afstand

Gebruik een VPN (bijv. WireGuard) voor toegang op afstand. Stel het apparaat nooit direct bloot aan het internet.

#### MQTT-broker op een andere host

Als uw MQTT-broker op een andere server in uw netwerk draait (wat gebruikelijk is), zorgt u ervoor dat de broker bereikbaar is op de ingestelde poort (standaard 1883). De firmware ondersteunt alleen onbeveiligd MQTT (poort 1883) en MQTT over TLS (poort 8883) is niet geimplementeerd.

---

### 6.9 Gebruik achter een reverse proxy

#### HTTP

De REST API en Web UI werken volledig achter een HTTPS-terminating reverse proxy (bijv. nginx, Caddy). De firmware zelf spreekt alleen HTTP; de proxy verzorgt de TLS-terminatie.

Minimale nginx-configuratie:

```nginx
location /otgw/ {
    proxy_pass http://otgw.local/;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
}
```

#### WebSocket

De WebSocket-verbinding (poort 81, live OT-log) veronderstelt plain HTTP/WS. WSS (WebSocket Secure) wordt niet ondersteund. Wanneer de Web UI via HTTPS wordt aangeboden via een reverse proxy, zal de browser een mixed-content-fout geven voor de WS-verbinding op poort 81, tenzij de proxy ook de WebSocket-verbinding termineert en via `wss://` doorstuurt.

> Dit is een bekende beperking. Het project draait op een vertrouwd LAN; de aanbeveling is VPN voor externe toegang in plaats van HTTPS-reverse proxy voor de volledige stack.

---

### 6.10 AP-fallback: wanneer activeert het en hoe herstelt u

#### Wanneer activeert de AP-fallback?

De AP-fallbackmodus activeert in de volgende situaties:

| Situatie | Gedrag |
|----------|--------|
| Geen opgeslagen WiFi-gegevens (eerste opstart) | Direct AP-modus |
| Opgeslagen gegevens kloppen niet meer (SSID/wachtwoord gewijzigd) | AP-modus na mislukte verbindingspoging |
| Driedubbele reset gedetecteerd | Gegevens gewist, AP-modus |
| Reset WiFi via Web UI | Gegevens gewist, herstart naar AP-modus |

#### Hoe herkent u de AP-modus?

- De SSID `<hostname>-<MAC>` verschijnt in de WiFi-scannerlijst van uw apparaat.
- De Telnet-debuglog (indien bereikbaar) toont: `Entered config mode SSID: otgw-XXXX`.
- De Web UI is niet bereikbaar op `otgw.local`.

#### Herstelstappen

1. Verbind uw telefoon of laptop met de SSID `otgw-<MAC>`.
2. Er opent automatisch een configuratiepagina. Zo niet: navigeer naar `http://192.168.4.1`.
3. Klik op **Configure WiFi**.
4. Selecteer uw netwerk, vul het wachtwoord in en sla op.
5. Het apparaat herstart en verbindt met uw netwerk.
6. Controleer of `otgw.local` weer bereikbaar is.

#### Wat als de portal niet verschijnt?

- Controleer of u verbonden bent met de `otgw-<MAC>` SSID.
- Open handmatig `http://192.168.4.1` in een browser.
- Sommige apparaten (Android) blokkeren verbindingen via een accesspoint zonder internet. Schakel in dat geval de mobiele data tijdelijk uit.

---

