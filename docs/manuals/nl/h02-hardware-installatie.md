## Hoofdstuk 2: Hardware en Installatie

### Ondersteunde hardware

#### NodoShop OTGW met ESP8266

De primaire en meest gebruikte configuratie is het NodoShop OpenTherm Gateway printje met een ESP8266-module (Wemos D1 Mini of NodeMCU-compatibel). Dit is het bewezen productieplatform voor OTGW-firmware en wordt volledig ondersteund. Twee boardrevisies bestaan:

| NodoShop OTGW-versie | ESP8266-module |
|---|---|
| 1.x t/m 2.0 | NodeMCU ESP8266 |
| 2.3 en later | Wemos D1 mini ESP8266 |

Beide revisies gebruiken dezelfde firmwarebinary. De NodeMCU en Wemos D1 mini zijn pin-compatibel vanuit firmwareperspectief.

Kenmerken:

- ESP8266 op 160 MHz (vastgelegd in `platformio.ini`), circa 80 KB bruikbaar RAM
- PIC16F88 of PIC16F1847 co-processor voor de OpenTherm bus
- 4 MB SPI-flash, linkerscript `eagle.flash.4m2m.ld`: 1 MB firmware, 2 MB LittleFS, 1 MB gereserveerd voor OTA
- Micro-USB aansluiting voor voeding en flashen
- I2C-pinnen voor optionele OLED en externe watchdog
- 1-Wire GPIO voor Dallas DS18B20 temperatuursensoren

De PlatformIO-omgeving heet `esp8266` (board `d1_mini`, platform `espressif8266@4.2.1`, Arduino Core 3.1.2). Dezelfde binary draait op zowel de NodeMCU- als de D1 mini-variant.

#### NodoShop OTGW32 met ESP32-S3

De OTGW32 is een apart board van NodoShop, gebouwd rond een ESP32-S3-module. Het board bevat een eigen OpenTherm-buscircuit waarmee de ESP32-S3 rechtstreeks op de OpenTherm bus communiceert zonder PIC co-processor. De PIC-firmwarechip is optioneel: als deze aanwezig is, biedt hij het klassieke serial bridge pad; als hij ontbreekt, neemt de OTDirect GPIO-modus het over.

Kenmerken:

- ESP32-S3 op 240 MHz, circa 300 KB bruikbaar RAM
- Geen PIC co-processor: OpenTherm wordt native afgehandeld via OTDirect op GPIO
- OTDirect master/slave GPIO-pinnen voor directe OpenTherm-buscommunicatie
- Step-up converter (18V) voor OT-busvoeding, aangestuurd via GPIO
- Bypass relay-uitgang (GPIO 47)
- W5500 SPI Ethernet-header voor bekabeld netwerk
- I2C-header voor SSD1306 OLED-display
- Bluetooth LE radio voor BLE-temperatuursensoren (Xiaomi LYWSD03MMC)
- 4 MB SPI-flash met een single-app partitielayout (geen dual-OTA in 4 MB, zie Partitietabel hieronder)
- USB-C aansluiting
- Twee knoppen: boot button (GPIO 0) en config button (GPIO 9)
- Drie LEDs: OT rood (GPIO 2), status (GPIO 8) en OT groen (GPIO 48)

#### Generieke ESP32 (experimenteel)

Standaard ESP32-modules (geen S3) worden geselecteerd door `BOARD_NODOSHOP_ESP32` te definieren en met de ESP32-toolchain te compileren, maar de pintoewijzingen in `boards.h` zijn afgestemd op het OTGW32-board met ESP32-S3. Op een gewone ESP32 sluiten de OTDirect-GPIO's en de W5500 SPI-pinnen niet noodzakelijk aan op de module. Beschouw dit als een ontwikkelaarsdoel.

#### Andere boards

OTGW-firmware kan in principe ook gebouwd worden voor andere ESP8266- of ESP32-boards, maar is uitsluitend getest en ondersteund op de NodoShop OTGW-hardware. De PIC co-processor op het NodoShop-printje is specifiek afgesteld op de OTGW-bedrading. Gebruik van andere hardware valt buiten de scope van dit handboek.

---

### Hardware-capability-vlaggen (boards.h)

Het headerbestand `boards.h` definieert compile-time capability-vlaggen die bepalen welke code in elke firmwarebinary wordt opgenomen. Deze vlaggen zijn nuttig bij het lezen van buildlogs of bij bijdragen aan de firmware.

| Vlag | ESP8266 OTGW | ESP32-S3 OTGW32 | Betekenis |
|---|---|---|---|
| `HAS_PIC` | 1 | 0 | PIC co-processor aanwezig. Omvat OTGWSerial, PIC-detectie, PIC-firmware-upgrade en I2C-watchdog. |
| `HAS_DIRECT_OT` | 0 | 1 | Directe GPIO OpenTherm-businterface aanwezig. Omvat OTDirect.ino, de opentherm_library, step-up converter-aansturing en de GPIO OT-pinnen. |
| `HAS_ETH_CAPABLE` | 0 | 1 | W5500 SPI Ethernet aanwezig. Omvat Ethernet.ino en de EthernetESP32-bibliotheek. |

Deze vlaggen sluiten elkaar uit tussen de twee buildtargets: een enkele firmwarebinary voor beide boards is niet mogelijk omdat de GPIO-pinnen voor OT-direct en I2C op hardwareniveau conflicteren. Het commando `python build.py` (of `pio run`) bouwt beide binaries vanuit dezelfde broncode.

Applicatiecode bewaakt platformspecifieke blokken met `#if HAS_PIC`, `#if HAS_DIRECT_OT` en `#if HAS_ETH_CAPABLE`. Gebruiksvriendelijke runtimecontroles maken gebruik van de helperfuncties `isPICEnabled()` en `isOTDirectEnabled()`, die een runtimedetectielaag toevoegen bovenop de compile-time vlag.

---

### GPIO-pintoewijzingen

De volgende tabellen tonen de GPIO-toewijzingen zoals gedefinieerd in `boards.h` per platform. Applicatiecode gebruikt de `PIN_*`-constanten en verwijst nooit rechtstreeks naar Dx-aliassen of ruwe GPIO-nummers.

#### ESP8266 (Wemos D1 mini)

| Functie | GPIO | Dx-alias | Opmerkingen |
|---|---|---|---|
| I2C SCL | 5 | D1 | Watchdog + OLED |
| I2C SDA | 4 | D2 | Watchdog + OLED |
| Button | 0 | D3 | Config/reset, actief LOW met pull-up |
| LED1 | 2 | D4 | Actief LOW, tevens onboard LED |
| PIC reset | 14 | D5 | Reset van de PIC co-processor |
| LED2 | 16 | D0 | Actief LOW |

#### ESP32-S3 (OTGW32)

| Functie | GPIO | Opmerkingen |
|---|---|---|
| Boot button | 0 | Actief LOW met pull-up |
| Config button | 9 | Actief LOW |
| OT rode LED | 2 | Actief LOW |
| Status-LED | 8 | Actief LOW |
| OT groene LED | 48 | Actief HIGH |
| I2C SCL | 17 | OLED-display |
| I2C SDA | 18 | OLED-display |
| OT master input | 21 | Van ketel |
| OT master output | 1 | Naar ketel |
| OT slave input | 6 | Van thermostaat |
| OT slave output | 7 | Naar thermostaat |
| Step-up enable | 10 | HIGH = 18V OT-busvoeding aan |
| 1-Wire (Dallas) | 4 | DS18B20-sensoren |
| Bypass relay | 47 | Relay-uitgang |
| SPI CS (W5500) | 14 | Ethernet chip select |
| SPI INT (W5500) | 15 | Ethernet interrupt |
| SPI RST (W5500) | 16 | Ethernet reset |
| SPI SCK | 12 | SPI clock |
| SPI MISO | 13 | SPI data in |
| SPI MOSI | 11 | SPI data out |

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

De OTGW32 gebruikt een aangepaste partitielayout voor 4 MB flash. Omdat de volledige ESP32-S3 build (Wi-Fi + Ethernet + OTDirect) groter is dan 2 MB, past een dual-OTA layout niet. De 4 MB flash is als volgt ingedeeld:

| Partitie | Offset | Grootte | Doel |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | Non-volatile storage |
| otadata | 0xE000 | 8 KB | OTA state |
| app0 | 0x10000 | 2,875 MB | Applicatiefirmware |
| spiffs | 0x2F0000 | 1 MB | LittleFS filesystem (webbestanden, configuratie, PIC-firmware) |
| coredump | 0x3F0000 | 64 KB | Core dump opslag |

> **Let op**: Met slechts een app-partitie zijn OTA-updates op 4 MB flash niet mogelijk. Om OTA op ESP32 in te schakelen is een 8 MB flashmodule met een aangepaste partitietabel vereist. Gebruik voor reguliere updates USB-flashen via `flash_esp.py`.

---

### Optionele hardware

#### W5500 Ethernet (alleen ESP32-S3 / OTGW32)

Met een W5500 SPI Ethernet-module beschikt de OTGW32 over een bekabelde netwerkverbinding. Als een netwerkkabel is aangesloten, schakelt de firmware automatisch van Wi-Fi naar Ethernet. Bij loskoppelen van de kabel valt het systeem automatisch terug op Wi-Fi binnen circa vijf seconden. Geen configuratiewijziging is nodig.

De W5500 is aangesloten op de SPI-bus van de ESP32-S3. Raadpleeg de OTGW32-hardwaredocumentatie voor de precieze pinout (zie ook de GPIO-tabel hierboven).

#### SSD1306 OLED-display

Een 128x64 pixels OLED-display via I2C biedt lokale statusweergave zonder browser. Het display toont 4 wisselende pagina's met temperaturen, netwerkinformatie en keteltoestand.

Aansluiting via I2C op de daarvoor bestemde pinnen (GPIO 5/GPIO 4 op ESP8266, GPIO 17/GPIO 18 op de OTGW32). Het display wordt automatisch herkend op I2C-adres 0x3C of 0x3D.

Het display schakelt automatisch uit na een instelbare idle-timeout. Configureer het I2C-adres, de pintoewijzingen en de timeout via Instellingen onder de sectie Hardware.

#### Dallas DS18B20 temperatuursensoren

Maximaal 16 DS18B20 temperatuursensoren kunnen worden aangesloten op de 1-Wire bus. De sensoren delen een enkele GPIO-pin (parasitaire voeding of externe 3,3V voeding met pull-up weerstand van 4,7 kOhm).

De standaard GPIO-pin is GPIO 10 (SD3) op ESP8266 en GPIO 4 op ESP32-S3. De pin is instelbaar via de webinterface. Elke sensor heeft een uniek 64-bits adres en kan worden voorzien van een beschrijvend label via de webinterface. Alle sensorwaarden worden gepubliceerd via MQTT en zijn beschikbaar in Home Assistant via auto-discovery.

> **Opmerking voor gebruikers die upgraden van versies voor v1.0.0**: De standaard Dallas GPIO-pin is gewijzigd van GPIO 13 (D7) naar GPIO 10 (SD3) in v1.0.0. Als uw sensoren niet meer werken na een upgrade, controleer dan de Dallas GPIO-instelling in Instellingen en wijzig deze eventueel terug naar 13.

Bedrading van een DS18B20 sensor:

- Verbind VDD met 3,3 V (of gebruik parasitaire voedingsmodus met VDD aan GND).
- Verbind GND met massa.
- Verbind DATA met de geconfigureerde GPIO-pin.
- Voeg een 4,7 kOhm pull-up weerstand toe tussen VDD en DATA.

#### S0-pulsaansluiting voor energiemeters

De S0-ingang maakt het mogelijk een S0-compatibele energiemeter (bijv. een slimme meter of zonnepaneelomvormer) aan te sluiten. Elke puls wordt geteld en omgezet naar momentaan vermogen (W) en gecumuleerde energie (kWh). De waarden worden gepubliceerd via MQTT.

#### BLE temperatuursensoren (alleen ESP32-S3 / OTGW32)

Op de OTGW32 kunnen tot vier Xiaomi LYWSD03MMC Bluetooth LE sensoren worden uitgelezen. De firmware scant passief via het BTHome v2-protocol: de sensoren zenden hun waarden uit als advertisements, er is geen koppeling of verbinding nodig. Ruimtetemperatuur en luchtvochtigheid van deze sensoren worden gebruikt voor de SAT multi-zone middeling en als individuele sensorentiteiten gepubliceerd naar Home Assistant.

---

### Firmware flashen

#### Eerste installatie via flash_esp.py (aanbevolen)

Het `flash_esp.py`-script verwerkt alle stappen automatisch: het kan de nieuwste release van GitHub downloaden, uw board en seriele poort automatisch detecteren via USB VID/PID, en de firmware flashen. Voor ESP32-S3 gebruikt het script een merged binary die de bootloader, partitietabel, firmware en filesystem in een enkel image combineert.

Onder de motorkap roept het script `python -m esptool` aan (esptool.py v5 of nieuwer) met de v5-subcommando's: `erase-flash` en `write-flash` met koppeltekens. De oude v4-vormen met underscore (`erase_flash`, `write_flash`) worden door esptool v5 niet meer geaccepteerd. Het script installeert esptool zo nodig zelf via `pip`, dus u hoeft dit normaal gesproken niet handmatig te doen.

**Vereisten:**

- Python 3.6 of hoger
- esptool v5 (wordt automatisch geinstalleerd door `flash_esp.py` bij eerste gebruik)
- USB-kabel (micro-USB voor ESP8266, USB-C voor ESP32-S3/OTGW32)
- De juiste USB-to-UART driver:
  - ESP8266: CP210x driver (NodeMCU) of CH340 driver (Wemos D1 mini klonen)
  - ESP32-S3 OTGW32: gebruikt de ingebouwde ESP32-S3 USB-Serial/JTAG interface (Espressif VID 0x303A, PID 0x1001), normaal geen extra driver nodig
  - Linux: voeg uw gebruiker toe aan de `dialout`-groep (`sudo usermod -aG dialout $USER`, dan uitloggen en weer inloggen)

**Stappen:**

1. Sluit de OTGW aan via USB.

2. Open een terminal en start het script:

```bash
python3 flash_esp.py
```

Zonder extra argumenten start het script in interactieve modus: het vraagt u een board te selecteren (ESP8266 of ESP32), en biedt vervolgens aan om bestaande build-artefacten te flashen, opnieuw te bouwen, of de nieuwste GitHub-release te downloaden. Volg de aanwijzingen.

3. Voor niet-interactief gebruik, specificeer het board en een modus:

```bash
# Nieuwste release downloaden en flashen (ESP8266)
python3 flash_esp.py --board esp8266 --download

# Nieuwste release downloaden en flashen (ESP32)
python3 flash_esp.py --board esp32 --download

# Bouwen vanuit broncode en flashen
python3 flash_esp.py --board esp32 --build
```

4. Het script detecteert automatisch de seriele poort door USB VID/PID te matchen met bekende boardconfiguraties. Als autodetectie niet lukt, specificeer de poort handmatig:

```bash
python3 flash_esp.py --port COM3          # Windows
python3 flash_esp.py --port /dev/ttyUSB0  # Linux
python3 flash_esp.py --port /dev/cu.usbserial-0001  # macOS
```

5. Voor een schone eerste installatie kunt u de `--erase` vlag gebruiken om de flash eerst te wissen. Let op: voor ESP32-S3 wist het script altijd de flash voor het schrijven, om de otadata-partitie te legen en bootloops te voorkomen.

```bash
python3 flash_esp.py --board esp8266 --erase --download
```

6. Na het flashen herstart het apparaat automatisch.

**ESP32-S3 merged binary**: Het flashscript gebruikt de `merged-full` image voor ESP32-S3. Een merged-full binary combineert bootloader, partitietabel, firmware en filesystem in een enkel bestand dat op adres 0x0 wordt geflasht. De helpers wissen flash eerst, dus het resultaat is een factory reset met verse firmware en filesystem.

#### Bouwen vanuit broncode met PlatformIO

PlatformIO is het primaire buildsysteem. De repository bevat een `platformio.ini` met twee omgevingen: `esp8266` (Wemos D1 mini, ESP8266) en `esp32` (OTGW32, ESP32-S3).

```bash
# Installeer PlatformIO (als extensie in VS Code of via pip)
pip install platformio

# Kloon de repository
git clone https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

# Bouw en flash voor ESP8266
pio run -e esp8266 --target upload
pio run -e esp8266 --target uploadfs

# Bouw en flash voor ESP32-S3 / OTGW32
pio run -e esp32 --target upload
pio run -e esp32 --target uploadfs
```

De `upload`-target flasht de firmwarebinary. De `uploadfs`-target flasht het LittleFS-filesystemimage met webbestanden, de Home Assistant discovery-configuratie en PIC-firmwarebestanden.

Beide targets vereisen een USB-verbinding.

Als alternatief kunt u het `build.py`-hulpscript gebruiken. Dit wrapt standaard PlatformIO en kan optioneel arduino-cli als backend inzetten; voor het flashen zelf wordt esptool v5 aangeroepen:

```bash
python build.py                      # Volledige build, beide platformen, PlatformIO backend
python build.py --target esp8266     # Alleen ESP8266
python build.py --target esp32       # Alleen ESP32-S3 / OTGW32
python build.py --firmware           # Alleen firmware (zonder filesystem)
python build.py --filesystem         # Alleen filesystem
python build.py --merged             # Maak ook een merged binary (ESP32)
python build.py --merged --compress  # Merged binary gzip-gecomprimeerd
python build.py --clean              # Build-artefacten opruimen
python build.py --distclean          # Inclusief gecachte cores en libraries
python build.py --arduino-cli        # Gebruik de legacy arduino-cli backend
```

Gebruik `python build.py --help` voor de actuele, complete lijst met vlaggen.

#### Handmatig flashen met bestaande binaries

Als u al firmware- en filesystembinaries heeft:

```bash
python3 flash_esp.py --board esp8266 --firmware OTGW-firmware-esp8266.ino.bin --filesystem OTGW-firmware-esp8266.littlefs.bin
python3 flash_esp.py --board esp32 --firmware OTGW-firmware-esp32.ino.bin --filesystem OTGW-firmware-esp32.littlefs.bin
```

---

### Eerste Wi-Fi-configuratie

Bij de allereerste start, of na het wissen van de Wi-Fi-instellingen, opent de OTGW een eigen Wi-Fi-accesspoint (AP).

**AP-naamformaat**: `<hostname>-<MAC suffix>`

De standaard hostname is `otgw`, dus het AP heet doorgaans iets als `otgw-A1B2C3`.

**Stappen voor de eerste configuratie:**

1. Zet de OTGW aan. Wacht 10-15 seconden.

2. Zoek op uw telefoon of laptop naar Wi-Fi-netwerken. U ziet een netwerk met de naam `otgw-XXXXXXXXXXXX` (waarbij de suffix het MAC-adres is).

3. Verbinding maken met dit netwerk. Er is geen wachtwoord vereist.

4. Een captive portal opent automatisch. Als dat niet het geval is, open dan een browser en navigeer naar `http://192.168.4.1`.

5. Selecteer uw thuisnetwerk (SSID) in de lijst en voer het Wi-Fi-wachtwoord in.

6. Klik op **Opslaan** (of **Save**). De OTGW verbindt met uw thuisnetwerk en herstart.

7. Zoek het IP-adres van de OTGW in uw router, of gebruik de mDNS-hostnaam `http://otgw.local/` in uw browser.

**mDNS-opmerking:** De hostnaam `otgw.local` werkt op de meeste thuisnetwerken. Op sommige Windows-systemen werkt mDNS alleen als de Bonjour-service actief is. Gebruik bij twijfel het IP-adres van uw router.

> **ESP32-S3 met Ethernet**: Als u een W5500 Ethernet-module en een netwerkkabel hebt aangesloten, verkrijgt het apparaat bij het opstarten automatisch een IP-adres via DHCP op de bekabelde interface en opent het geen AP. Wi-Fi-configuratie is niet nodig.

> **AP-fallbackmodus (beta, alleen ESP32-S3)**: Als het geconfigureerde Wi-Fi-netwerk na de initialisatie onbereikbaar wordt, opent het apparaat een fallback-AP genaamd `OTGW-<MAC>` met wachtwoord `otgw123` en IP `192.168.4.1`, zodat de webinterface bereikbaar blijft.

---

### Upgraden van een vorige versie

#### Via OTA vanuit de webinterface (aanbevolen voor ESP8266)

Vanaf v1.3.0 biedt de webinterface een OTA-updater met een druk op de knop:

1. Open de webinterface (`http://otgw.local/`).
2. Navigeer naar **Update** (of het update-icoontje in de header).
3. De pagina haalt de lijst met beschikbare GitHub-releases op en toont Installed, Update en Rollback badges per versie.
4. Klik op **Update** naast de gewenste versie.
5. De firmware downloadt de nieuwe binary van GitHub, flasht deze en herstart.
6. De webinterface pollt het apparaat totdat het weer reageert en bevestigt succes.
7. Update daarna het filesystemimage via dezelfde pagina om te zorgen dat de webbestanden overeenkomen met de nieuwe firmwareversie.

> **ESP32-S3 op 4 MB flash**: OTA is niet beschikbaar met de standaard 4 MB partitielayout (er past slechts een app-partitie). Gebruik in plaats daarvan USB-flashen via `flash_esp.py`.

#### Handmatige OTA via bestandsupload

Als alternatief kunt u een firmwarebinary handmatig uploaden:

1. Download de `.bin`-file van de GitHub-releasepagina.
2. Ga in de webinterface naar **Update > Handmatig uploaden**.
3. Selecteer het gedownloade bestand en klik op **Uploaden**.

**Let op:** Upload altijd de juiste binary voor uw platform: de ESP8266-binary voor ESP8266, de ESP32-binary voor ESP32. Het uploaden van een verkeerde binary kan het apparaat in een bootloop brengen; in dat geval is serieel herflasen via USB nodig.

#### Opmerkingen bij upgraden van v1.x

- Geen settings-migratie nodig. Instellingenbestanden van v1.3.x laden zonder wijziging in v2.0.0.
- MQTT-topics en de REST API zijn ongewijzigd ten opzichte van v1.x.
- Als u upgradet van een versie voor v1.2.0: REST API v0 en v1 endpoints zijn verwijderd in v1.2.0. Alleen `/api/v2/` endpoints zijn beschikbaar. Zie [docs/BREAKING_CHANGES.md](../docs/BREAKING_CHANGES.md).
- Verwijder na de upgrade eventuele verweesde Home Assistant-entiteiten en laat MQTT auto-discovery ze opnieuw aanmaken om nieuwe of hernoemde entiteiten op te pikken.

#### Belangrijke waarschuwingen

- **Flash nooit PIC-firmware via Wi-Fi met OTmonitor.** Dit kan de PIC-microcontroller onherstelbaar beschadigen. Gebruik in plaats daarvan de ingebouwde PIC-firmware-upgradefunctie in de webinterface.
- **Wanneer OTDirect actief is (alleen ESP32-S3)**, wordt de PIC-UART omzeild. Probeer geen PIC-firmware-upgrades terwijl OTDirect het actieve OpenTherm-pad is.

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
