## Hoofdstuk 2: Hardware en Installatie

### Ondersteunde hardware

#### NodoShop OTGW Classic met LOLIN S3 Mini of S3 Mini Pro

De 2.x-firmware draait op de NodoShop OTGW Classic met een LOLIN S3 Mini of S3 Mini Pro (ESP32-S3) in de Wemos D1 mini-voet. De S3 Mini past in de Wemos D1 mini-voet: elk Classic-signaal komt uit op de S3-GPIO achter het bijbehorende gaatje. De PIC blijft zitten. U flasht een enkel image, `esp32-combo`, dat bij het opstarten de PIC detecteert en in PIC-gatewaymodus draait.

Kenmerken:

- PIC16F88 of PIC16F1847 co-processor voor de OpenTherm bus
- ESP32-S3 (LOLIN S3 Mini of S3 Mini Pro) in de Wemos D1 mini-voet
- 4 MB SPI-flash met een safeboot-partitielayout: een firmwareslot plus een updater voor firmware-updates via WiFi (zie Partitietabel hieronder)
- I2C-header voor de externe watchdog en een optioneel SSD1306 OLED-display
- 1-Wire GPIO voor Dallas DS18B20 temperatuursensoren
- Bluetooth LE radio voor BLE-temperatuursensoren
- De RGB-led op de LOLIN-module toont de WiFi-status
- Geen Ethernet en geen OT-Direct-hardware

De 1.x-lijn (onderhoudsbranch `otgw-1.x.x`) draait op de Wemos D1 mini (ESP8266). Terug naar 1.x betekent de Wemos D1 mini terug in de voet zetten en een 1.x-release flashen; een 1.x-zip draait niet op een S3.

#### Andere boards

OTGW-firmware kan in principe ook gebouwd worden voor andere ESP32-S3-boards, maar is uitsluitend getest en ondersteund op de NodoShop OTGW-hardware. De PIC co-processor op het NodoShop-printje is specifiek afgesteld op de OTGW-bedrading. Gebruik van andere hardware valt buiten de scope van dit handboek.

---

### Hardware-capability-vlaggen (boards.h)

Het headerbestand `boards.h` definieert compile-time capability-vlaggen die bepalen welke code in de firmwarebinary wordt opgenomen. Deze vlaggen zijn nuttig bij het lezen van buildlogs of bij bijdragen aan de firmware.

| Vlag | Meegecompileerd in esp32-combo | Betekenis |
|---|---|---|
| `HAS_PIC` | 1 | Ondersteuning voor de PIC co-processor. Omvat OTGWSerial, PIC-detectie, PIC-firmware-upgrade en I2C-watchdog. |
| `HAS_DIRECT_OT` | 1 | OT-Direct-modus, alleen gebruikt op hardware zonder PIC. Omvat OTDirect.ino en de opentherm_library. |
| `HAS_ETH_CAPABLE` | 1 | Ethernet-ondersteuning, alleen gebruikt met Ethernet-hardware. Omvat Ethernet.ino en de EthernetESP32-bibliotheek. |

Het `esp32-combo`-image bevat zowel het PIC-pad als het OT-Direct-pad en kiest er bij het opstarten een (`HAS_RUNTIME_HW_DETECT`). Op de OTGW Classic detecteert het de PIC en draait het in PIC-gatewaymodus. `build.bat` (Windows) of `./build.sh` (Linux/macOS) bouwt dit image.

Applicatiecode bewaakt platformspecifieke blokken met `#if HAS_PIC`, `#if HAS_DIRECT_OT` en `#if HAS_ETH_CAPABLE`. Gebruiksvriendelijke runtimecontroles maken gebruik van de helperfuncties `isPICEnabled()` en `isOTDirectEnabled()`, die een runtimedetectielaag toevoegen bovenop de compile-time vlag.

---

### GPIO-pintoewijzingen

De volgende tabel toont de GPIO-toewijzingen voor de OTGW Classic zoals gedefinieerd in `boards.h`. De S3 Mini en S3 Mini Pro zitten in de Wemos D1 mini-voet, dus elk signaal staat bij het gaatje in de voet. De firmware detecteert bij het opstarten of er een Mini of een Mini Pro in zit. Applicatiecode gebruikt de `PIN_*`-constanten en verwijst nooit rechtstreeks naar gaatjeslabels of ruwe GPIO-nummers.

#### OTGW Classic met LOLIN S3 Mini / S3 Mini Pro

| Functie | Gaatje in voet | S3 Mini GPIO | S3 Mini Pro GPIO | Opmerkingen |
|---|---|---|---|---|
| PIC UART (ESP TX naar PIC RX) | TX | 43 | 43 | Seriele verbinding met de PIC |
| PIC UART (ESP RX van PIC TX) | RX | 44 | 44 | Seriele verbinding met de PIC |
| I2C SCL | D1 | 36 | 11 | Watchdog + OLED |
| I2C SDA | D2 | 35 | 12 | Watchdog + OLED |
| Button | D3 | 18 | 13 | Config/reset, actief LOW |
| LED1 | D4 | 16 | 14 | Actief LOW |
| PIC reset | D5 | 12 | 40 | Reset van de PIC co-processor |
| LED2 | D0 | 4 | 41 | Actief LOW |
| RGB-statusled | (op de module) | 47 | 8 | WiFi-status |

---

### Fysieke aansluitingen

#### OpenTherm bus bedrading

De OTGW wordt aangesloten in serie op de twee-draads OpenTherm bus tussen de thermostaat en de ketel. De volgorde is: **thermostaat > OTGW > ketel**.

```
  [Thermostaat] ──── OT-bus ──── [OTGW] ──── OT-bus ──── [Ketel]
```

De OpenTherm bus is polariteitsonafhankelijk: het maakt niet uit welke draad u op welke aansluitklem aansluit. De bus werkt op 24 V en levert zelf de voeding voor de OTGW. U hoeft de bestaande bedrading tussen thermostaat en ketel dus alleen op te knippen en de OTGW daartussen te plaatsen.

**Aansluitstappen:**

1. Zet de cv-installatie uit via de schakelaar op de ketel.
2. Koppel de twee OpenTherm-draden los van de thermostaataansluitingen op de ketel.
3. Sluit deze twee draden aan op de OTGW-aansluitklemmen gemarkeerd als "THERMOSTAT" (of "T").
4. Gebruik twee nieuwe draden van minimaal 0,5 mm2 om de OTGW te verbinden met de ketel. Sluit aan op de aansluitklemmen gemarkeerd als "BOILER" (of "B") op de OTGW en op de OpenTherm-ingang van de ketel.
5. Controleer of alle klemmen goed vastgedraaid zijn.

**Let op:** Gebruik uitsluitend de voor de OTGW bestemde aansluitklemmen. Andere aansluitingen op de ketel (220V, potentiaalvrij) zijn niet bedoeld voor OpenTherm.

#### Voeding

De OTGW wordt gevoed via de OpenTherm bus. Een externe voeding is normaal gesproken niet nodig. Als u de OTGW wilt voeden zonder actieve OpenTherm-verbinding (bijv. tijdens testen op een bureau), gebruik dan de USB-aansluiting van de ESP-module.

De USB-aansluiting is ook de primaire manier om de firmware voor het eerst te flashen.

---

### ESP32-S3 partitietabel

De ESP32-S3-firmware gebruikt een aangepaste partitielayout voor 4 MB flash (ADR-191). De firmware past er geen twee keer in. Daarom is er in plaats van twee firmwaresloten een enkel firmwareslot, plus een kleine updater (safeboot) die nieuwe firmware via WiFi installeert:

| Partitie | Offset | Grootte | Doel |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | Non-volatile storage (WiFi-gegevens) |
| otadata | 0xE000 | 8 KB | OTA state |
| safeboot | 0x10000 | 960 KB | Updater voor firmware-updates via WiFi |
| app0 | 0x100000 | 2240 KB | Applicatiefirmware |
| spiffs | 0x330000 | 768 KB | LittleFS filesystem (webinterface, instellingen, PIC-firmware) |
| coredump | 0x3F0000 | 64 KB | Core dump opslag |

> **Let op**: Een unit met de eerdere layout (een vroege 2.0.0-alpha) moet eenmalig via USB gemigreerd worden (`flash_otgw --migrate --host <ip>`). Daarna werken firmware-updates via WiFi. Zie [Firmware-updates via WiFi](../../guides/WIFI_FIRMWARE_UPDATE.md) (Engels).

---

### Optionele hardware

#### Ethernet

De OTGW Classic heeft geen Ethernet. De Ethernet-instellingen in de webinterface gelden alleen voor Ethernet-hardware.

#### SSD1306 OLED-display

Een 128x64 pixels OLED-display via I2C biedt lokale statusweergave zonder browser. Het display toont 4 wisselende pagina's met temperaturen, netwerkinformatie en keteltoestand.

Aansluiting via de I2C-header van de OTGW Classic. Het display wordt automatisch herkend op I2C-adres 0x3C.

Het display schakelt automatisch uit na een instelbare idle-timeout. Configureer het I2C-adres, de pintoewijzingen en de timeout via Instellingen onder de sectie Hardware.

#### Dallas DS18B20 temperatuursensoren

Maximaal 16 DS18B20 temperatuursensoren kunnen worden aangesloten op de 1-Wire bus. De sensoren delen een enkele GPIO-pin (parasitaire voeding of externe 3,3V voeding met pull-up weerstand van 4,7 kOhm).

De standaard GPIO-pin is GPIO 10. De pin is instelbaar via de webinterface (bereik 0 tot 16). Elke sensor heeft een uniek 64-bits adres en kan worden voorzien van een beschrijvend label via de webinterface. Alle sensorwaarden worden gepubliceerd via MQTT en zijn beschikbaar in Home Assistant via auto-discovery.

> **Opmerking voor gebruikers die upgraden van versies voor v1.0.0**: De standaard Dallas GPIO-pin is gewijzigd van GPIO 13 (D7) naar GPIO 10 (SD3) in v1.0.0. Als uw sensoren niet meer werken na een upgrade, controleer dan de Dallas GPIO-instelling in Instellingen en wijzig deze eventueel terug naar 13.

> **Van een Wemos D1 mini (1.x) naar een S3 Mini (2.x): stel de pin opnieuw in.** De instelling is een GPIO-nummer, geen gaatje op de printplaat, en hetzelfde gaatje heeft op de S3 een ander GPIO-nummer. Zet u uw 1.x-instellingen terug of houdt u het oude nummer, dan komt de 1-Wire-bus op een ander gaatje uit en verdwijnen de sensoren. Voorbeeld: een sensor op gaatje D7 is GPIO 13 op de Wemos, maar GPIO 11 op de S3 Mini. Hetzelfde geldt voor de pin van de S0-teller en de GPIO-uitgang.

| Gaatje | Wemos D1 mini GPIO (1.x) | S3 Mini GPIO (2.x) | S3 Mini Pro GPIO (2.x) | Vrij voor een sensor op de Classic? |
|---|---|---|---|---|
| D6 | 12 | **13** | 39 | Ja |
| D7 | 13 | **11** | 38 | Ja |
| D8 | 15 | **10** | 37 | Ja |
| D0 | 16 | 4 | 41 | Nee: LED2 |
| D1 | 5 | 36 | 11 | Nee: I2C (watchdog, OLED) |
| D2 | 4 | 35 | 12 | Nee: I2C (watchdog, OLED) |
| D3 | 0 | 18 | 13 | Nee: knop |
| D4 | 2 | 16 | 14 | Nee: LED1 |
| D5 | 14 | 12 | 40 | Nee: PIC-reset |

De firmware weigert een pin die het bord zelf gebruikt: de sensoren starten dan niet en het telnet-log meldt waarom. Op de S3 Mini Pro zijn de vrije gaatjes D6, D7 en D8 GPIO 39, 38 en 37; sinds beta.125 accepteert de instelling elke S3-GPIO (0 tot 21 en 26 tot 48). De S3 Mini Pro-nummers voor D6 tot D8 komen uit de SPI-pinnen van de Arduino-borddefinitie (`lolin_s3_mini_pro`); de andere rijen staan in [PINOUT.md](../../hardware/PINOUT.md), paragraaf 3a en 4a.

Bedrading van een DS18B20 sensor:

- Verbind VDD met 3,3 V (of gebruik parasitaire voedingsmodus met VDD aan GND).
- Verbind GND met massa.
- Verbind DATA met de geconfigureerde GPIO-pin.
- Voeg een 4,7 kOhm pull-up weerstand toe tussen VDD en DATA.

#### S0-pulsaansluiting voor energiemeters

De S0-ingang maakt het mogelijk een S0-compatibele energiemeter (bijv. een slimme meter of zonnepaneelomvormer) aan te sluiten. Elke puls wordt geteld en omgezet naar momentaan vermogen (W) en gecumuleerde energie (kWh). De waarden worden gepubliceerd via MQTT.

#### BLE temperatuursensoren

Op de ESP32-S3 kunnen tot vier Xiaomi LYWSD03MMC Bluetooth LE sensoren worden uitgelezen; de LOLIN S3 Mini en S3 Mini Pro hebben allebei de BLE-radio. De firmware scant passief via het BTHome v2-protocol: de sensoren zenden hun waarden uit als advertisements, er is geen koppeling of verbinding nodig. Ruimtetemperatuur en luchtvochtigheid van deze sensoren worden gebruikt voor de SAT multi-zone middeling en als individuele sensorentiteiten gepubliceerd naar Home Assistant.

---

### Firmware flashen

#### Eerste installatie via flash_otgw (aanbevolen)

Een nieuwe S3 Mini flasht u de eerste keer via USB. De release-zip (`OTGW-firmware-esp32-combo-<versie>-flash.zip`) bevat de flashscripts `flash_otgw.bat` (Windows) en `flash_otgw.sh` (Linux/macOS) en een merged-full image dat bootloader, partitietabel, firmware en filesystem combineert in een enkel bestand dat op adres 0x0 wordt geflasht.

De scripts hebben geen Python nodig: bij het eerste gebruik downloaden ze de standalone esptool van Espressif naar `./tools/esptool/`. Standaard doen ze een factory reset: ze wissen de flash en schrijven het merged-full image. WiFi-gegevens en instellingen worden daarbij verwijderd.

**Vereisten:**

- Een USB-kabel tussen uw computer en de USB-poort van de S3 Mini
- De S3 Mini gebruikt de ingebouwde ESP32-S3 USB-Serial/JTAG interface (Espressif VID 0x303A, PID 0x1001), normaal geen extra driver nodig. Een nieuw board met nog de fabrieksfirmware kan tot de eerste flash als PID 0x4001 verschijnen; `flash_otgw` vindt beide zonder `--port`
- Linux: het script schakelt over op `sudo` als uw gebruiker niet in de `dialout`-groep zit. Om dat te voorkomen voegt u uw gebruiker toe aan de groep (`sudo usermod -aG dialout $USER`, dan uitloggen en weer inloggen)

**Stappen:**

1. Pak de release-zip uit en sluit de USB-kabel aan op de S3 Mini.

2. Open een terminal in de uitgepakte map en start het script:

```bash
flash_otgw.bat          # Windows
./flash_otgw.sh         # Linux/macOS
```

3. Het script kiest het merged-full image uit de map en detecteert de seriele poort automatisch via USB VID/PID. Als autodetectie niet lukt, of als er meer dan een S3 is aangesloten, geef de poort dan zelf op:

```bash
flash_otgw.bat --port COM3              # Windows
./flash_otgw.sh --port /dev/ttyACM0     # Linux
./flash_otgw.sh --port /dev/cu.usbmodem-XXXX  # macOS
```

4. Na het flashen herstart het apparaat automatisch.

Latere updates gaan via WiFi (zie Upgraden van een vorige versie hieronder). Een unit met de oude partitielayout (een vroege 2.0.0-alpha) heeft eenmalig een `--migrate` nodig: `flash_otgw.bat --migrate --host <ip>` of `./flash_otgw.sh --migrate --host <ip>`. Dat bewaart `settings.ini` via HTTP, schrijft de nieuwe layout met behoud van WiFi en zet de instellingen terug. Start het script met `--help` voor alle opties.

#### Bouwen vanuit broncode

Op de 2.x-lijn is `esp32-combo` de enige build. Bouw die met de wrapperscripts, die zelf hun Python-omgeving en de PlatformIO-toolchain opzetten:

```bash
# Kloon de repository
git clone --recursive https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

build.bat                # Windows: firmware + filesystem, esp32-combo
./build.sh               # Linux/macOS: firmware + filesystem, esp32-combo
./build.sh --firmware    # Alleen firmware (zonder filesystem)
./build.sh --filesystem  # Alleen filesystem
./build.sh --clean       # Build-artefacten opruimen
```

De build schrijft zijn uitvoer naar `build/`, inclusief de zip zoals bij een release, met het merged-full image en de flashscripts. Flash die met `flash_otgw` zoals hierboven beschreven. Gebruik `./build.sh --help` voor de actuele, complete lijst met vlaggen. Hoofdstuk 8 beschrijft de build uitgebreider.

#### Handmatig flashen met bestaande binaries

Als u al een merged-full image heeft, geef dat dan expliciet op:

```bash
flash_otgw.bat --bin OTGW-firmware-esp32-combo-<versie>-merged-full.bin
./flash_otgw.sh --bin OTGW-firmware-esp32-combo-<versie>-merged-full.bin
```

---

### Eerste Wi-Fi-configuratie

Bij de allereerste start, of na het wissen van de Wi-Fi-instellingen, opent de OTGW een eigen Wi-Fi-accesspoint (AP).

**AP-naamformaat**: `<hostname>-<MAC suffix>`

De standaard hostname op 2.0.0 is `OTGW2`, dus het AP heet doorgaans iets als `OTGW2-A4:CF:12:21:B4:F8`.

**Stappen voor de eerste configuratie:**

1. Zet de OTGW aan. Wacht 10-15 seconden.

2. Zoek op uw telefoon of laptop naar Wi-Fi-netwerken. U ziet een netwerk met de naam `OTGW2-XX:XX:XX:XX:XX:XX` (waarbij de suffix het MAC-adres is).

3. Verbinding maken met dit netwerk. Er is geen wachtwoord vereist.

4. Een captive portal opent automatisch. Als dat niet het geval is, open dan een browser en navigeer naar `http://192.168.4.1`.

5. Selecteer uw thuisnetwerk (SSID) in de lijst en voer het Wi-Fi-wachtwoord in.

6. Klik op **Opslaan** (of **Save**). De OTGW verbindt met uw thuisnetwerk en herstart.

7. Zoek het IP-adres van de OTGW in uw router, of gebruik de mDNS-hostnaam `http://otgw2.local/` in uw browser.

**mDNS-opmerking:** De hostnaam `otgw2.local` werkt op de meeste thuisnetwerken. Op sommige Windows-systemen werkt mDNS alleen als de Bonjour-service actief is. Gebruik bij twijfel het IP-adres van uw router.

> **AP-fallbackmodus (beta)**: Als het geconfigureerde Wi-Fi-netwerk na de initialisatie onbereikbaar wordt, opent het apparaat een fallback-AP genaamd `OTGW-<MAC>` met wachtwoord `otgw1234` en IP `192.168.4.1`, zodat de webinterface bereikbaar blijft.

---

### Upgraden van een vorige versie

#### Via OTA vanuit de webinterface (aanbevolen)

Open `http://<ip>/update`, kies de release-zip (of de bestanden `.ino.bin` en `.littlefs.bin`) en druk op **Update**. De pagina schakelt over naar de safeboot-updater, installeert de firmware, wacht op de herstart en installeert daarna de webbestanden. Uw instellingen en sensornamen blijven behouden. Laat het tabblad 2 tot 3 minuten open. Een unit met de eerdere layout moet eerst eenmalig via USB gemigreerd worden (`flash_otgw --migrate --host <ip>`). Details: [Firmware-updates via WiFi](../../guides/WIFI_FIRMWARE_UPDATE.md) (Engels).

#### Handmatige OTA via bestandsupload

Als alternatief kunt u een firmwarebinary handmatig uploaden:

1. Download de `.bin`-file van de GitHub-releasepagina.
2. Ga in de webinterface naar **Update > Handmatig uploaden**.
3. Selecteer het gedownloade bestand en klik op **Uploaden**.

**Let op:** Upload altijd de `esp32-combo`-binary. Een 1.x-binary draait niet op de S3. Het uploaden van een verkeerde binary kan het apparaat in een bootloop brengen; in dat geval is serieel herflashen via USB nodig.

#### Opmerkingen bij upgraden van v1.x

- Overstappen van 1.x naar 2.x betekent de Wemos D1 mini vervangen door een LOLIN S3 Mini of S3 Mini Pro en die via USB flashen (zie Firmware flashen). Terug naar 1.x betekent de Wemos D1 mini terugplaatsen en een 1.x-release flashen; een 1.x-zip draait niet op een S3.
- Geen settings-migratie nodig. Instellingenbestanden van v1.3.x laden zonder wijziging in v2.0.0.
- MQTT-topics en de REST API zijn ongewijzigd ten opzichte van v1.x.
- Als u upgradet van een versie voor v1.2.0: REST API v0 en v1 endpoints zijn verwijderd in v1.2.0. Alleen `/api/v2/` endpoints zijn beschikbaar. Zie [docs/BREAKING_CHANGES.md](../../BREAKING_CHANGES.md).
- **SAT zit in de firmware: het oude SAT-component in Home Assistant is niet meer nodig.** In 2.0 is SAT (Smart Autotune Thermostat) in de firmware zelf ingebouwd; het staat standaard uit. Laat de firmware-SAT en een SAT-integratie in Home Assistant (bijvoorbeeld via HACS) nooit tegelijk regelen: zolang de firmware-SAT aan staat, weigert de gateway `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` en `TC=` via MQTT en de REST API, dus de integratie kan dan niet sturen. Wilt u de integratie houden, laat dan de firmware-SAT uit.
- **Houdt u de SAT-integratie in Home Assistant, let dan op het commando-topic.** Een integratie die op de Wemos is ingesteld, blijft na de wissel naar de S3 sturen naar `OTGW/set/otgw-<oud MAC>/command`. De nieuwe gateway krijgt dan niets binnen en de thermostaat regelt zelf verder, terwijl alles in Home Assistant er normaal uitziet. Oplossing: neem de unique id van de Wemos mee (zie hieronder), of verwijder de integratie, wis de oude `otgw-<oud MAC>`-takken (bijvoorbeeld in MQTT Explorer) en voeg de integratie opnieuw toe met het nieuwe apparaat.
- **Dezelfde Home Assistant-entiteiten houden: neem de MQTT unique id mee.** De standaard unique id is `otgw-` plus het MAC-adres van de module. De S3 heeft een ander MAC-adres dan de Wemos, dus zonder actie krijgt hij een nieuwe unique id. Home Assistant ziet dan een nieuw apparaat en maakt nieuwe entiteiten aan; omdat de oude namen nog bezet zijn, krijgen die een achtervoegsel zoals `_2`. Er zijn twee oplossingen:
  1. **Oude entiteiten houden.** Zet vóór het wisselen de unique id van de Wemos over: zet uw 1.x-instellingenbestand terug op de S3 (de unique id gaat mee), of neem bij de MQTT-instellingen de waarde van **Unique id** van de Wemos over in de S3. Laat de twee gateways daarna nooit tegelijk draaien: met dezelfde unique id schrijven ze naar dezelfde entiteiten.
  2. **Opnieuw beginnen.** Verwijder in Home Assistant het oude OTGW-apparaat en geef de nieuwe entiteiten zo nodig de oude namen terug, zodat uw automatiseringen en dashboards blijven werken.

  Het wissen van de retained topics met MQTT Explorer lost de `_2`-namen niet op: de oude entiteiten staan dan nog in het entiteitenregister van Home Assistant. Verwijder ze daar.
- Verwijder na de upgrade eventuele verweesde Home Assistant-entiteiten en laat MQTT auto-discovery ze opnieuw aanmaken om nieuwe of hernoemde entiteiten op te pikken.

#### Belangrijke waarschuwingen

- **Flash nooit PIC-firmware via Wi-Fi met OTmonitor.** Dit kan de PIC-microcontroller onherstelbaar beschadigen. Gebruik in plaats daarvan de ingebouwde PIC-firmware-upgradefunctie in de webinterface.
- **In OT-Direct-modus** (alleen op hardware zonder PIC) wordt de PIC-UART omzeild. Probeer geen PIC-firmware-upgrades terwijl OT-Direct het actieve OpenTherm-pad is.

---

### Driedubbele reset: Wi-Fi-instellingen wissen

Als u geen toegang meer heeft tot de webinterface en de Wi-Fi-instellingen wilt wissen (bijv. omdat u naar een ander netwerk gaat), kunt u dit doen zonder USB-kabel:

1. Zet de OTGW aan.
2. Wacht tot de status-LED begint te knipperen (normaal opstartproces, 5-10 seconden).
3. Zet de OTGW uit.
4. Wacht 2 seconden.
5. Herhaal stappen 1-4 nog twee keer (dus in totaal drie keer aan- en uitzetten).
6. Bij de vierde start wist de firmware de Wi-Fi-instellingen en opent het AP.

Na het wissen van de Wi-Fi-instellingen volgt u de stappen voor eerste Wi-Fi-configuratie (zie hierboven). Overige instellingen (MQTT, SAT, sensorlabels) blijven bewaard.
