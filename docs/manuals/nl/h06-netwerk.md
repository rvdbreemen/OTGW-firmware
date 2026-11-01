## Hoofdstuk 6: Netwerkconfiguratie

Dit hoofdstuk beschrijft alle netwerkaspecten van OTGW-firmware: hoe het apparaat verbinding maakt met uw thuisnetwerk, hoe hostnamen worden geadverteerd, hoe de tijd wordt gesynchroniseerd en welke poorten het apparaat gebruikt. Het geeft ook richtlijnen voor gebruik achter een reverse proxy en voor firewallinstellingen.

---

### 6.1 WiFi-installatie en de AP-fallbackmodus

#### 6.1.1 Eerste verbinding

Bij de eerste opstart beschikt het apparaat nog over geen opgeslagen WiFi-instellingen. De firmware start dan automatisch de WiFiManager captive portal:

1. Het apparaat start als WiFi-accesspoint met de SSID `<hostname>-<MAC>`, standaard iets als `OTGW2-A4:CF:12:21:B4:F8`.
2. Verbind uw telefoon of laptop met dit netwerk.
3. Er opent automatisch een captiveportalpagina (of navigeer handmatig naar `192.168.4.1`).
4. Kies uw thuisnetwerk uit de lijst, vul het wachtwoord in en sla op.
5. Het apparaat herstart en verbindt met uw netwerk.

Zodra de verbinding is gemaakt, slaat de firmware de gegevens op in flash. Bij iedere volgende opstart verbindt het apparaat automatisch.

#### 6.1.2 Automatisch herstel bij WiFi-uitval

De firmware implementeert een tweelaags herstelmechanisme:

- **Laag 1**: De WiFi SDK (`WiFi.setAutoReconnect(true)`) herstelt kortdurende onderbrekingen (typisch binnen 30 seconden) transparant op radioniveau.
- **Laag 2**: Bij langere uitval neemt de toestandsmachine `loopWifi()` het over. Deze is volledig non-blocking: elke aanroep keert binnen minder dan een milliseconde terug, zodat de watchdog, de OpenTherm-berichtenverwerking, de MQTT-keepalives en de Web UI gewoon blijven draaien tijdens een herverbindingspoging. Per poging krijgt WiFi 30 seconden om scan, associatie en DHCP te voltooien. In productiebuilds herstart het apparaat na 10 mislukte pogingen (maximaal 300 seconden voor de reboot). In betabuilds activeert de firmware na 2 mislukte pogingen de AP-fallbackmodus (zie paragraaf 6.1.3) in plaats van te herstarten. Deze waarden zijn vastgelegd in ADR-075 (vervangt de oorspronkelijke 5 seconden / 15 retries uit ADR-047) om `WiFi.begin()` een volledig associatievenster te geven en te voorkomen dat de SDK auto-reconnect halverwege wordt afgebroken.

Bij herstel van de verbinding:
- Zet de firmware de geconfigureerde hostnaam opnieuw voor de volgende DHCP-uitwisseling.
- Worden MQTT, de WebSocket-verbinding en de Telnet-debugserver automatisch opnieuw gestart.

De firmware doet zelf geen DHCP-aanroepen: de WiFi SDK regelt DHCP als onderdeel van het herverbinden.

#### 6.1.3 AP-fallbackmodus (beta)

Beschikbaar in betabuilds (v2.0.0-beta en later). Wanneer WiFi-pogingen uitgeput zijn, schakelt het apparaat over naar AP-fallbackmodus in plaats van eindeloos te herstarten. In productiereleases herstart het apparaat na 10 mislukte pogingen.

Bij opstart, als er credentials opgeslagen zijn maar WiFi niet bereikbaar is, slaan betabuilds de WiFiManager config portal over en gaan direct naar AP-fallback.

- **SSID**: `OTGW-<laatste 3 bytes van het MAC-adres in hoofdletters hex>`, bijvoorbeeld `OTGW-AABBCC`
- **IP-adres**: `192.168.4.1`
- **Wachtwoord**: `otgw1234`
- **MQTT**: uitgeschakeld in fallbackmodus
- **OTA-update**: beschikbaar vanuit fallbackmodus, zodat u een gecorrigeerde firmware kunt flashen zonder serieel toegang

In AP-fallbackmodus probeert de firmware elke 5 minuten opnieuw verbinding te maken met het geconfigureerde WiFi-netwerk. Zodra WiFi beschikbaar is, worden alle services automatisch hersteld en wordt de AP afgesloten.

**Herstelstappen:**
1. Verbind met de SSID `OTGW-XXXXXX` met wachtwoord `otgw1234`.
2. Open `http://192.168.4.1` in uw browser.
3. Pas de WiFi-instellingen aan via de Settings-pagina of gebruik Reset WiFi.
4. Herstart het apparaat.

#### 6.1.4 WiFi-gegevens wissen

| Methode | Hoe |
|---|---|
| Web UI | Settings-pagina, klik op de knop "Reset WiFi" |
| Driedubbele reset | Druk de hardware-resetknop 3 keer achter elkaar in, elke druk binnen 10 seconden na de vorige (forceert het configuratieportaal; geen power-cycle, niet ingedrukt houden) |
| Telnet-debugconsole | Stuur het commando `resetwifi` op poort 23 |

---

### 6.2 Ethernet

De OTGW Classic heeft geen Ethernet en gebruikt dus altijd WiFi; de Ethernet-instellingen verschijnen alleen op hardware met Ethernet.

---

### 6.3 Statisch IP vs. DHCP

Standaard gebruikt het apparaat DHCP. Dit is de aanbevolen instelling voor de meeste situaties. Als uw router DHCP-reserveringen ondersteunt, is het toewijzen van een vast IP-adres op basis van MAC-adres de beste aanpak.

Voor een statisch IP geconfigureerd in de firmware:

1. Open de **Settings-pagina** in de Web UI.
2. Vul het gewenste IP-adres, subnetmasker, gateway en DNS-server in.
3. Sla op en herstart.

Laat alle velden leeg om terug te keren naar DHCP.

> Controleer bij het gebruik van een statisch IP altijd of het adres buiten het DHCP-bereik van uw router valt om adresconflicten te vermijden.

---

### 6.4 mDNS en hostnaam

De standaard hostnaam op 2.0.0 is `OTGW2`. Het apparaat is hiermee bereikbaar als `otgw2.local` in browsers en andere mDNS-compatibele clients (macOS, iOS, Linux met Avahi, Windows 10+ met Bonjour).

De 1.x-firmware houdt de standaardnaam `OTGW` (`otgw.local`). Een 1.x- en een 2.0.0-gateway in hetzelfde netwerk gebruiken dus niet meer dezelfde naam. Een apparaat dat van 1.x is bijgewerkt, houdt de hostnaam uit zijn instellingen, meestal `OTGW`. Alleen een nieuwe installatie of een reset waarbij `settings.ini` verloren gaat, begint met `OTGW2`.

De 2.x-firmware heeft geen LLMNR-responder. Het apparaat reageert dus zelf niet op de kale naam `otgw2` (zonder `.local`): gebruik `otgw2.local` of het IP-adres.

De hostnaam is instelbaar via de Settings-pagina. De nieuwe hostnaam werkt door in het DHCP-verzoek, de mDNS-advertentie, het MQTT client ID en de Web UI-titel.

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

De firmware biedt een optionele geplande nachtelijke herstart. Deze functie is standaard uitgeschakeld.

| Instelling | Standaard | Beschrijving |
|---|---|---|
| Nightly Restart | `uit` | Inschakelen om eenmaal per dag te herstarten op het geconfigureerde uur |
| Nightly Restart Hour | `4` | Lokaal uur (0-23) waarop de herstart plaatsvindt |

Voorwaarden voor de herstart:
- De instelling moet ingeschakeld zijn.
- NTP moet ingeschakeld en gesynchroniseerd zijn.
- Het apparaat moet langer dan 1 uur draaien (voorkomt herstartlussen na een recente herstart).

De herstart veroorzaakt een korte serviceonderbreking van ongeveer 30 seconden.

---

### 6.6 OTA-updates

OTA (Over-The-Air) staat voor draadloze firmware-updates. Hiermee kunt u de firmware bijwerken zonder USB-kabel.

Het OTA-update-endpoint is `/update`. Het valt onder de beschermde-admin-grens uit ADR-056 (vervangt ADR-054). Wanneer `settingHTTPpasswd` leeg is, is het endpoint open. Wanneer een wachtwoord is ingesteld, vereist het endpoint HTTP-authenticatie met gebruikersnaam `admin` (de hoofdfirmware vraagt Digest, de safeboot-updater Basic). Hetzelfde wachtwoord wordt bij het starten van het netwerk doorgegeven aan de OTA-updateserver, zodat een wijziging zonder her-flash actief wordt na de eerstvolgende reboot.

Browserverzoeken naar beschermde admin-endpoints (waaronder `/update`, `/upload`, `/ReBoot`, `/ResetWireless` en `POST /api/v2/settings`) worden bovendien gecontroleerd op overeenkomst van `Origin`/`Referer` met `Host`. Dit is een lichte CSRF-mitigatie, geen volledig sessieframework: verzoeken zonder `Origin` of `Referer` (curl, OTmonitor, scripts) worden toegestaan voor achterwaartse compatibiliteit; afwijkende origins worden afgewezen met HTTP 403.

**Stappen (ESP32-S3, 4 MB flash):**
1. Open `http://<ip>/update` in uw browser (het IP-adres, niet `otgw2.local`).
2. Kies de release-zip, of de bestanden `.ino.bin` en `.littlefs.bin`.
3. Druk op **Update** en laat het tabblad open tot er Done staat. De pagina doet de overdracht naar safeboot, de firmware-upload, 35 s wachttijd en de filesystem-upload, en controleert het resultaat.

De formulieren onder **Advanced** uploaden een bestand per keer. Zie [Firmware-updates via WiFi](../../guides/WIFI_FIRMWARE_UPDATE.md) (Engels) voor wat behouden blijft, wat opnieuw begint en wat u doet als een stap mislukt.

OTA is ook beschikbaar vanuit de AP-fallbackmodus, zodat u een apparaat dat vastzit op een slechte firmware kunt herstellen zonder serieel toegang.

#### 6.6.1 ESP32: Merged binaries alleen via USB

De OTA-updatepagina accepteert geen merged binaries (`*-merged*.bin`, `*-migrate-*.bin`): dat zijn USB-images voor `flash_otgw` en de pagina weigert ze. Gebruik de release-zip of de bestanden `.ino.bin` en `.littlefs.bin`.

#### 6.6.2 Beperkingen

- Flash nooit PIC-firmware via OTA. De OTA-updatepagina is uitsluitend voor ESP-firmware. Het flashen van PIC-firmware over het netwerk met OTmonitor kan de PIC permanent beschadigen.

---

### 6.8 Poortgebruik

| Poort | Protocol | Functie | Opmerkingen |
|---|---|---|---|
| 80 | TCP / HTTP | Web UI, REST API en OTA-update | Alle webroutes, bestandsbediening, firmware-update |
| 80 | TCP / WebSocket (`/ws`) | Live OpenTherm-logstream | Zelfde poort als de Web UI, pad `/ws` (poort 81 was de 1.x-lijn) |
| 23 | TCP / Telnet | Debugconsole | Tekstuele debuglog; bediend door SimpleTelnet-bibliotheek |
| 25238 | TCP | Seriele bridge (ser2net) | Ruwe OTGW PIC-serieel over TCP; bediend door SimpleTelnet-bibliotheek; OTmonitor-compatibel |
| 123 | UDP (uitgaand) | NTP (SNTP) | Alleen uitgaand |
| 5353 | UDP | mDNS | Lokale naamresolutie |
| 1883 | TCP (uitgaand) | MQTT | Uitgaand naar uw MQTT-broker |

---

### 6.9 Firewall- en routeroverwegingen

OTGW-firmware is uitsluitend bedoeld voor gebruik op een vertrouwd lokaal netwerk. Er hoeven geen poorten te worden doorgestuurd op uw router.

mDNS (`otgw2.local`) werkt alleen binnen een enkel Layer 2 broadcast-domein. Als Home Assistant op een ander VLAN staat, gebruik dan het statische IP-adres of een DNS-vermelding in plaats van `.local`.

---

### 6.10 Gebruik achter een reverse proxy

De webinterface, de REST API en de live log werken achter een reverse proxy, ook op een pad (bijvoorbeeld `https://example.lan/otgw/` of Home Assistant ingress) en via HTTPS: de interface bouwt elke URL op ten opzichte van de pagina waar hij vandaan komt, en de live log is een WebSocket op `ws` onder dat pad (`wss://` op een HTTPS-pagina). De proxy moet de Host-header van de browser doorgeven (anders wordt met een gateway-wachtwoord elke wijziging met 403 geweigerd) en WebSocket doorgeven. Zie [docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md](../../guides/REVERSE_PROXY_AND_HA_INGRESS.md) voor een getest nginx-voorbeeld en het hoofdstuk Home Assistant-integratie voor het HA-paneel.

---
