# ESP8266 Firmware Handleiding

*→ Also available in [English](FLASH_GUIDE_EN.md)*

Deze handleiding beschrijft wat de OTGW-firmware toevoegt bovenop de PIC-firmware, en
alle methoden voor het flashen van de OTGW-firmware op je ESP8266-apparaat (NodeMCU of
Wemos D1 mini).

Zie voor informatie over de PIC-microcontroller firmware de
[PIC Firmware Handleiding](PIC_FIRMWARE_NL.md).

---

## Wat de OTGW-firmware toevoegt bovenop de PIC

Naast de PIC zit een **ESP8266 Wi-Fi-module** met daarop de OTGW-firmware. De PIC is de
bus-level elektrische interface; de ESP8266 is waar alle intelligentie zit. Hij begrijpt
het OpenTherm-protocol volledig, decodeert elk bericht dat de PIC doorgeeft en verbindt
jouw verwarmingssysteem met je smart home.

### Volledig begrip van het OpenTherm-protocol

De OTGW-firmware is geen passieve doorvoer. Hij bevat een complete OpenTherm-decoder die
elk raw frame parseert dat de PIC doorgeeft:

- Alle **80+ OpenTherm-bericht-ID's** uit de OpenTherm-specificatie (v2.2 en v4.2) worden
  gedecodeerd: verwarmingssetpoints, keteltemperaturen, waterdruk, modulatieniveau, warm
  tapwater (WTW), zonneboilers, tweede verwarmingscircuit (CH2), ventilatie, luchtvochtigheid,
  operationele tellers, foutcodes en meer.
- Elke gedecodeerde waarde wordt opgeslagen onder een naam (`TBoiler`, `TSet`,
  `FlameStatus`, `FaultCode`, …) zodat de rest van de firmware met betekenisvolle data
  werkt in plaats van ruwe bytes.
- Het live OpenTherm-berichtenlogboek in de webinterface toont elk berichttype, richting
  en gedecodeerde waarde in real time.

### De ketel bedienen via de PIC

De OTGW-firmware is ook de commandolaag. Wanneer jij (of jouw smart home) het gedrag van
de ketel wilt wijzigen, vertaalt de OTGW-firmware die aanvraag naar het juiste PIC-seriële
commando en plaatst het in een wachtrij voor aflevering:

- Tijdelijke kamertemperatuuroverschrijving (`TT`-commando)
- Overschrijving van het CV-watertemperatuursetpoint (`SW`-commando)
- WTW-setpointoverschrijving, injectie van buitentemperatuur en alle andere OTGW-commando's
- Een commandowachtrij zorgt dat commando's op volgorde worden verstuurd zonder de
  seriële buffer te overlopen

### Home Assistant-integratie via MQTT

De OTGW-firmware is de aanbevolen integratielaag voor Home Assistant:

- **309 auto-discovery-configuraties** voor 80+ bericht-ID's — entiteiten verschijnen
  automatisch in Home Assistant, inclusief een klimaatentiteit met temperatuurregeling.
- Gescheiden MQTT-topics per bron (perspectief thermostaat vs. cv-ketel) voor nauwkeurige
  statusweergave.
- Instelbaar publicatie-interval om data vers te houden zonder de broker te overbelasten.
- **Webhook-ondersteuning** — stuur HTTP-aanroepen wanneer statusbits veranderen (vlam aan,
  storing gedetecteerd, CV actief, WTW actief, enz.).

### REST-API

Een volledig gedocumenteerde REST-API (`/api/v2/`) laat elk hulpmiddel de gateway
bevragen of bedienen:

- Gedecodeerde OpenTherm-data opvragen per bericht-ID of leesbare naam.
- OTGW-commando's versturen en PIC-gateway-instellingen uitlezen.
- Apparaatinstellingen, Dallas-sensorlabels en diagnostische data beheren.
- OpenAPI 3.0-specificatie beschikbaar voor Swagger UI, Postman of codegeneratie.

### Webinterface

- Live OpenTherm-berichtenlogboek via WebSocket — filterbaar, pauzeerbaar, met decodering
  per bericht.
- Real-time grafieken voor keteltemperaturen, setpoints, waterdruk en modulatieniveau.
- PIC-gateway-instellingenpaneel — alle 15 PIC-configuratieregisters leesbaar vanuit de
  browser.
- Firmware- en bestandssysteem-OTA-updates met gezondheidscheck na herstart.
- Bestandssysteemverkenner met uploaden, downloaden en verwijderen.
- Donker/licht thema-schakelaar.

### Externe sensoren en meters

- **Dallas-temperatuursensoren** (DS18B20/DS18S20/DS1822) met aangepaste labels,
  real-time grafieken en Home Assistant-auto-discovery — volledig onafhankelijk van
  OpenTherm.
- **S0-pulsteller** — kWh-meterpulstelling op een instelbare GPIO-pin.

### TCP-seriële socket voor OTmonitor

Een ruwe TCP-socket op poort **25238** stelt het OTGW-seriële protocol beschikbaar zodat
OTmonitor en andere tools met de PIC kunnen communiceren. De OTGW-firmware detecteert
ser2net-verkeer en pauzeert zijn eigen commandowachtrij om conflicten te voorkomen.

### Betrouwbaarheid en netwerk

- **NTP-tijdsynchronisatie** met instelbare tijdzone.
- **Niet-blokkerend WiFi-herverbindingsstatemachine** — verwarming gaat door terwijl WiFi
  herstelt.
- **Drievoudige-reset WiFi-herstel** — drie snelle stroomonderbrekingen openen de
  WiFiManager-captive portal opnieuw zonder flashen.
- Hardware-watchdog herstart bij een vastgelopen firmware.
- Optionele HTTP Basic Auth voor instellingen- en onderhoudseindpunten.
- Telnet-diagnosticserver (poort 23) voor veldprobleemoplossing.

### Automatisch PIC-firmwarebeheer

De OTGW-firmware controleert op nieuwe PIC-firmwareversies, downloadt ze van
[otgw.tclcode.com](https://otgw.tclcode.com/) en programmeert ze in de PIC — allemaal
vanuit de webinterface, zonder programmeerapparaat of seriële kabel. De PIC-variant
(PIC16F88 vs. PIC16F1847) wordt automatisch gedetecteerd en het juiste image geselecteerd.

---

## De ESP8266 flashen

### Flashtools in één oogopslag

| Tool | Python vereist? | Beste voor |
|---|---|---|
| `flash_otgw.sh` / `flash_otgw.bat` | **Nee** | Eindgebruikers — uitvoeren vanuit terminal of dubbelklikken op `.bat` |
| `flash_esp.py` | Ja (Python 3.6+) | Ontwikkelaars, OTA-download, geavanceerde opties |

Beide tools zijn opgenomen in elke release-download en in de root van de repository.

---

### Vereisten

#### Hardware
- ESP8266-ontwikkelbord (NodeMCU of Wemos D1 mini)
- Micro-USB-kabel (datakabel, geen laad-only kabel)
- Computer met USB-poort

#### Software
- **Eenvoudige methode**: geen extra software — `flash_otgw.sh`/`flash_otgw.bat` download
  esptool automatisch bij de eerste uitvoering
- **Geavanceerde methode**: Python 3.6 of hoger — `flash_esp.py` installeert esptool via
  pip indien nodig

#### USB-stuurprogramma's
Afhankelijk van je bord en besturingssysteem installeer je de benodigde stuurprogramma's:
- Windows: CP210x of CH340 USB-naar-UART stuurprogramma (controleer Apparaatbeheer als
  de poort ontbreekt)
- macOS: Stuurprogramma's zijn doorgaans inbegrepen in recente versies
- Linux: Zorg dat je gebruiker lid is van de groep `dialout` voor toegang tot de seriële
  poort (`sudo usermod -aG dialout $USER`)

---

### Eerste installatie (eerste keer flashen)

> **Belangrijk**: Een eerste installatie vereist ZOWEL het firmware-binair
> (`OTGW-firmware-<versie>.ino.bin`) ALS het bestandssysteem-binair
> (`OTGW-firmware-<versie>.ino.littlefs.bin`). Alleen de firmware flashen zorgt ervoor
> dat het apparaat bij de eerste opstart enkele minuten bezig is met het formatteren van
> een leeg bestandssysteem — of in het slechtste geval resulteert in een opstartlus.

#### Eenvoudige methode (geen Python vereist)

1. Download van de GitHub-releasepagina `flash_otgw.sh` (Linux/macOS) of
   `flash_otgw.bat` (Windows) **en** beide binaire bestanden:
   - `OTGW-firmware-*.ino.bin` (firmware)
   - `OTGW-firmware*.littlefs.bin` (bestandssysteem)
2. Plaats alle drie bestanden in dezelfde map.
3. Voer het script uit:

**Linux / macOS:**
```bash
chmod +x flash_otgw.sh
./flash_otgw.sh
```

**Windows** (uitvoeren vanuit Command Prompt of PowerShell):
```bat
flash_otgw.bat
```

Het script downloadt esptool bij de eerste uitvoering, detecteert automatisch je seriële
poort, wist de flash en schrijft beide binaire bestanden — zonder vragen, zonder extra
software.

Als er meerdere seriële poorten aanwezig zijn, wordt de eerste gebruikt. Geef `--port`
mee om een specifieke poort te kiezen:
```bash
./flash_otgw.sh --port /dev/ttyUSB1
```
```bat
flash_otgw.bat --port COM5
```

#### Geavanceerde methode (Python)

```bash
# Download de nieuwste release en wis de flash voor een schone start
python3 flash_esp.py --download --erase
```

Dit doet alles in één stap:
1. Downloadt de nieuwste `OTGW-firmware-<versie>.ino.bin` (firmware) en
   `OTGW-firmware-<versie>.ino.littlefs.bin` (bestandssysteem) van GitHub
2. Wist de volledige flashchip voor het schrijven (verwijdert verouderde data van
   oudere versies)
3. Flasht firmware op `0x0` en bestandssysteem op `0x200000` in één enkele esptool-
   bewerking

#### Waarom `--erase` belangrijk is bij een eerste installatie

Oudere firmwareversies (v1.3.x en eerder) gebruikten een andere bestandssysteempartitie-
indeling (1 MB LittleFS op `0x300000`). Als je `--erase` overslaat, kunnen resten van het
oude bestandssysteem op het oude adres achterblijven. De nieuwe firmware vindt dan geen
geldig bestandssysteem op het nieuwe adres (`0x200000`) en zal:
- 5–10 minuten bezig zijn met het stil formatteren van de flash (apparaat lijkt niet te
  reageren — geen opstartlus)
- Herhaaldelijk opstarten in een foutstatus die op een opstartlus lijkt

Het gebruik van `--erase` elimineert deze problemen volledig.

---

### Upgraden via USB (aanbevolen bij grote versiewijzigingen)

Bij upgraden van **v1.3.x of eerder** naar **v1.4.x of later** is de LittleFS-partitie-
grootte gewijzigd van 1 MB naar 2 MB. Een firmware-only upgrade via USB activeert een
bestandssysteemherformattering bij de eerste opstart en **wist al je instellingen**.

#### Upgradeprocedure (USB, instellingen waar mogelijk bewaren)

Voor upgraden v1.4.x → v1.5.x (zelfde partitie-indeling, geen herformattering nodig):

```bash
python3 flash_esp.py --download
```

Zowel firmware als bestandssysteem worden in één bewerking geschreven. Wissen is niet
nodig; instellingen opgeslagen in het bestandssysteem blijven bewaard.

> **Let op**: `flash_otgw.sh` en `flash_otgw.bat` wissen altijd de volledige flash voor
> het schrijven. Ze zijn niet geschikt voor upgrades waarbij instellingen bewaard moeten
> blijven. Gebruik `flash_esp.py --download` (zonder `--erase`) als je je instellingen
> wilt bewaren.

Voor upgraden v1.3.x of eerder → v1.4.x+ (partitie-indeling gewijzigd, instellingen
gaan verloren):

```bash
# Maak eerst een back-up van instellingen via de webinterface (Instellingen → Exporteren)
python3 flash_esp.py --download --erase
# of de no-Python-scripts, die ook wissen:
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

Importeer na het flashen je instellingen opnieuw via de webinterface.

---

### Upgraden via webinterface OTA

> **WAARSCHUWING**: Bij upgraden van v1.3.x of eerder naar v1.4.x via de webinterface
> OTA-pagina **moet** je het bestandssysteem-binair als eerste flashen, dan pas het
> firmware-binair. Flashen in de verkeerde volgorde zorgt ervoor dat de nieuwe firmware
> opstart tegen de oude 1 MB bestandssysteem-indeling. Het apparaat zal dan 5–10 minuten
> bezig zijn met stil formatteren en verliest daarna alle instellingen.

**Juiste OTA-volgorde voor v1.3.x → v1.4.x upgrades:**

1. Exporteer je instellingen via de webinterface (Instellingenpagina → Exporteren)
2. Upload op de OTA-pagina eerst `OTGW-firmware-*.littlefs.bin` en wacht op de herstart
3. Upload daarna `OTGW-firmware-*.ino.bin` en wacht op de herstart
4. Vernieuw de browser hard (Ctrl+F5)
5. Importeer je instellingen opnieuw

**Deze volgorde-eis geldt NIET voor v1.4.x → v1.5.x upgrades** (de partitie-indeling
is identiek).

---

### Snel aan de slag (standaard)

#### Voorkeursmethode: no-Python-scripts

Download `flash_otgw.sh` (Linux/macOS) of `flash_otgw.bat` (Windows) en beide binaire
bestanden van de GitHub-releasepagina. Plaats alle drie in dezelfde map en voer uit:

```bash
chmod +x flash_otgw.sh
./flash_otgw.sh          # Linux/macOS
```

```bat
flash_otgw.bat           :: Windows — Command Prompt of PowerShell
```

Het script downloadt esptool bij de eerste uitvoering (geen Python nodig), wist de flash
en schrijft beide images in één stap.

#### Nieuwste release downloaden en flashen (Python)

```bash
python3 flash_esp.py
```

Of expliciet:

```bash
python3 flash_esp.py --download
```

#### Ontwikkelaarsmodus — bouwen vanuit broncode en flashen

```bash
python3 flash_esp.py --build
```

#### Handmatige modus — bestaande binaire bestanden gebruiken

```bash
python3 flash_esp.py --firmware OTGW-firmware-1.5.0.ino.bin --filesystem OTGW-firmware-1.5.0.ino.littlefs.bin
```

---

### Veelgebruikte opties

| Optie | Omschrijving |
|---|---|
| `--port POORT` | Seriële poort (bijv. `COM5` of `/dev/ttyUSB0`) |
| `--baud BAUD` | Flash-baudrate (standaard: 460800; probeer 115200 bij onstabiele verbindingen) |
| `--erase` | Volledige flash wissen voor het schrijven. **Gebruik bij eerste installaties en generatie-overschrijdende upgrades.** |
| `--download` | Nieuwste release van GitHub downloaden en flashen |
| `--build` | Firmware lokaal bouwen en flashen (vereist arduino-cli) |
| `--no-interactive` / `-y` | Alle vragen overslaan (voor automatisering) |

---

### Opstartlussen oplossen

Een opstartlus (apparaat herstart herhaaldelijk en bereikt nooit de webinterface) na het
flashen wordt vrijwel altijd veroorzaakt door een van de volgende:

#### 1. Firmware geflasht zonder bijpassend bestandssysteem

De firmware kan geen geldig bestandssysteem vinden en herstart.

**Oplossing:** Wis de flash en schrijf zowel firmware als bestandssysteem in één stap.

No-Python-scripts (wissen is altijd inbegrepen):
```bash
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

Python:
```bash
python3 flash_esp.py --download --erase
```

#### 2. Upgraden van v1.3.x zonder wissen (verouderd bestandssysteem op verkeerd adres)

v1.4.x verplaatste de bestandssysteempartitie van `0x300000` (1 MB) naar `0x200000`
(2 MB). Als de oude bestandssysteemdata nog aanwezig is, kan de nieuwe firmware onverwacht
gedrag vertonen.

**Oplossing:**

No-Python-scripts:
```bash
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

Python:
```bash
python3 flash_esp.py --download --erase
```

#### 3. Flash onvolledig of onderbroken

**Oplossing:** Probeer opnieuw met een lagere baudrate.

No-Python-scripts:
```bash
./flash_otgw.sh --baud 115200
flash_otgw.bat --baud 115200
```

Python:
```bash
python3 flash_esp.py --download --erase --baud 115200
```

#### 4. Diagnose met de seriële monitor

Als het apparaat in een opstartlus zit, verbind dan via USB en open een seriële terminal
op **74880 baud** om de ESP8266 ROM-bootloader-berichten op te vangen. Schakel daarna over
naar **115200 baud** zodra de firmware-banner begint te verschijnen (als dat gebeurt). De
eerste 20–30 regels identificeren vrijwel altijd de oorzaak van de crash (bijv.
`Exception 3`, `Fatal exception 28`, `LittleFS mount failed`).

Hulpmiddelen: Arduino IDE Seriële Monitor, PuTTY, `screen /dev/ttyUSB0 74880`, of elke
terminal op de juiste baudrate.

#### 5. Harde herstel (apparaat volledig niet-reagerend)

Als zowel de webinterface als de seriële uitvoer niet beschikbaar zijn, geef dan de
poort en een conservatieve baudrate expliciet op.

No-Python-scripts:
```bash
./flash_otgw.sh --port /dev/ttyUSB0 --baud 115200
flash_otgw.bat --port COM3 --baud 115200
```

Python:
```bash
python3 flash_esp.py --download --erase --baud 115200 --port <jouw-poort>
```

Als er geen poort verschijnt, controleer dan Apparaatbeheer (Windows) of
`ls /dev/tty*` (Linux/macOS) voor de USB-serieel-adapter. Veelgebruikte
stuurprogrammapakketten: CP210x (Silicon Labs) of CH340 (WCH).

---

### Na het flashen

Na een eerste flash (via de eenvoudige scripts of `flash_esp.py`) opent het apparaat een
WiFi-toegangspunt met de naam `OTGW-<MAC-adres>`. Verbind hiermee en ga naar
`http://192.168.4.1` om je WiFi-netwerk en andere instellingen te configureren. Bij
volgende opstarten verbindt het apparaat automatisch met je geconfigureerde netwerk.

---

### Opmerkingen

- **Flash de PIC-firmware nooit via Wi-Fi met OTmonitor** — dit kan de PIC-microcontroller
  beschadigen.
- Gebruik een betrouwbare, directe USB-kabel (vermijd hubs) om flashfouten te minimaliseren.
- Als automatisch installeren van esptool mislukt, installeer het dan handmatig:
  `pip install esptool`
- Voeg op Linux jezelf toe aan de groep `dialout` en log daarna opnieuw in voor het
  flashen. Bij de eerste uitvoering escaleert `flash_otgw.sh` automatisch met `sudo`
  als toegang tot de seriële poort wordt geweigerd.

Voor volledige gebruiksdetails van de Python-tool:

```bash
python3 flash_esp.py --help
```
