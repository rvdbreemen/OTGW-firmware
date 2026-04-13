## Hoofdstuk 2: Hardware en Installatie

### Ondersteunde hardware

#### NodoShop OTGW met ESP8266

De primaire en meest gebruikte configuratie is het NodoShop OpenTherm Gateway printje met een ESP8266-module (Wemos D1 Mini of NodeMCU-compatibel). Dit is de bewezen productieplatform voor OTGW-firmware en wordt volledig ondersteund.

Kenmerken:
- ESP8266 op 80 of 160 MHz, ~80 KB bruikbaar RAM
- PIC16F88 of PIC16F1847 co-processor voor de OpenTherm bus
- 4 MB SPI-flash (2 MB beschikbaar voor LittleFS)
- Micro-USB aansluiting voor voeding en flashen
- I2C-pinnen voor optionele OLED en externe watchdog
- 1-Wire GPIO voor Dallas DS18B20 temperatuursensoren

#### NodoShop OTGW32 met ESP32

De tweede generatie hardware met een ESP32-module. Nieuw in v2.0.0.

Kenmerken:
- ESP32 op 240 MHz, ~300 KB bruikbaar RAM
- PIC co-processor aanwezig (zelfde als ESP8266-variant)
- OTDirect optie: OpenTherm rechtstreeks op GPIO, zonder PIC
- Optionele W5500 SPI Ethernet-module
- Bluetooth LE radio voor temperatuursensoren (Xiaomi LYWSD03MMC)
- 4 MB SPI-flash met aangepaste partitietabel (2x 1,5 MB OTA + 768 KB LittleFS)
- USB-C aansluiting

#### Andere boards

OTGW-firmware kan in principe ook gebouwd worden voor andere ESP8266- of ESP32-boards, maar is uitsluitend getest en ondersteund op de NodoShop OTGW-hardware. De PIC co-processor op het NodoShop-printje is specifiek afgesteld op de OTGW-bedrading. Gebruik van andere hardware valt buiten de scope van dit handboek.

---

### Hardware-capability-vlaggen (boards.h)

Het headerbestand `boards.h` definieert compile-time capability-vlaggen die bepalen welke code in elke firmwarebinary wordt opgenomen. Deze vlaggen zijn nuttig bij het lezen van buildlogs of bij bijdragen aan de firmware.

| Vlag | ESP8266 OTGW | ESP32 OTGW32 | Betekenis |
|---|---|---|---|
| `HAS_PIC` | 1 | 0 | PIC co-processor aanwezig. Omvat OTGWSerial, PIC-detectie, PIC-firmware-upgrade en I2C-watchdog. |
| `HAS_DIRECT_OT` | 0 | 1 | Directe GPIO OpenTherm-businterface aanwezig. Omvat OTDirect.ino, de opentherm_library, step-up converter-aansturing en de GPIO OT-pinnen. |
| `HAS_ETH_CAPABLE` | 0 | 1 | W5500 SPI Ethernet aanwezig. Omvat Ethernet.ino en de EthernetESP32-bibliotheek. |

Deze vlaggen sluiten elkaar uit tussen de twee buildtargets: een enkele firmwarebinary voor beide boards is niet mogelijk omdat de GPIO-pinnen voor OT-direct en I2C op hardwareniveau conflicteren. Het commando `python build.py` (of `pio run`) bouwt beide binaries vanuit dezelfde broncode.

Applicatiecode bewaakt platformspecifieke blokken met `#if HAS_PIC`, `#if HAS_DIRECT_OT` en `#if HAS_ETH_CAPABLE`. Gebruiksvriendelijke runtimecontroles maken gebruik van de helperfuncties `isPICEnabled()` en `isOTDirectEnabled()`, die een runtimedetectielaag toevoegen bovenop de compile-time vlag.

---

### Fysieke aansluitingen

#### OpenTherm bus bedrading

De OTGW wordt aangesloten in serie op de twee-draads OpenTherm bus tussen de thermostaat en de ketel. De volgorde is: **thermostaat → OTGW → ketel**.

```
  [Thermostaat] ──── OT-bus ──── [OTGW] ──── OT-bus ──── [Ketel]
```

De OpenTherm bus is polariteitsonafhankelijk: het maakt niet uit welke draad u op welke aansluitklem aansluit. De bus werkt op 24 V en levert zelf de voeding voor de OTGW. U hoeft de bestaande bedrading tussen thermostaat en ketel dus alleen op te knippen en de OTGW daartussen te plaatsen.

**Aansluitstappen:**

1. Zet de cv-installatie uit via de schakelaar op de ketel.
2. Koppel de twee OpenTherm-draden los van de thermostaataansluitingen op de ketel.
3. Sluit deze twee draden aan op de OTGW-aansluitklemmen gemarkeerd als "THERMOSTAT" (of "T").
4. Gebruik twee nieuwe draden van minimaal 0,5 mm² om de OTGW te verbinden met de ketel. Sluit aan op de aansluitklemmen gemarkeerd als "BOILER" (of "B") op de OTGW en op de OpenTherm-ingang van de ketel.
5. Controleer of alle klemmen goed vastgedraaid zijn.

**Let op:** Gebruik uitsluitend de voor de OTGW bestemde aansluitklemmen. Andere aansluitingen op de ketel (220V, potentiaalvrij) zijn niet bedoeld voor OpenTherm.

#### Voeding

De OTGW wordt gevoed via de OpenTherm bus. Een externe voeding is normaal gesproken niet nodig. Als u de OTGW wilt voeden zonder actieve OpenTherm-verbinding (bijv. tijdens testen op een bureau), gebruik dan de USB-aansluiting van de ESP-module.

De USB-aansluiting is ook de primaire manier om de firmware voor het eerst te flashen.

---

### Optionele hardware

#### W5500 Ethernet (alleen ESP32)

Met een W5500 SPI Ethernet-module beschikt de OTGW32 over een bekabelde netwerkverbinding. Als een netwerkkabel is aangesloten, schakelt de firmware automatisch van Wi-Fi naar Ethernet. Bij loskoppelen van de kabel valt het systeem automatisch terug op Wi-Fi.

De W5500 is aangesloten op de SPI-bus van de ESP32. Raadpleeg de OTGW32-hardwaredocumentatie voor de precieze pinout.

#### SSD1306 OLED-display

Een 128x64 pixels OLED-display via I2C biedt lokale statusweergave zonder browser. Het display toont 4 wisselende pagina's met temperaturen, netwerkinformatie en keteltoestand.

Aansluiting via I2C op de daarvoor bestemde pinnen (GPIO4/GPIO5 op ESP8266, of de I2C-pinnen op de OTGW32). Het display wordt automatisch herkend op I2C-adres 0x3C of 0x3D.

#### Dallas DS18B20 temperatuursensoren

Maximaal 16 DS18B20 temperatuursensoren kunnen worden aangesloten op de 1-Wire bus. De sensoren delen een enkele GPIO-pin (parasitaire voeding of externe 3,3V voeding met pull-up weerstand van 4,7 kΩ).

De GPIO-pin voor de 1-Wire bus is instelbaar via de webinterface. Elke sensor heeft een uniek 64-bits adres en kan worden voorzien van een beschrijvend label via de webinterface. Alle sensorwaarden worden gepubliceerd via MQTT en zijn beschikbaar in Home Assistant via auto-discovery.

#### S0-pulsaansluiting voor energiemeters

De S0-ingang maakt het mogelijk een S0-compatibele energiemeter (bijv. een slimme meter of zonnepaneelomvormer) aan te sluiten. Elke puls wordt geteld en omgezet naar momentaan vermogen (W) en gecumuleerde energie (kWh). De waarden worden gepubliceerd via MQTT.

#### BLE temperatuursensoren (alleen ESP32)

Op de OTGW32 kan de SAT-thermostaat de ruimtetemperatuur uitlezen van een Xiaomi LYWSD03MMC-sensor via Bluetooth LE. De sensor zendt zijn waarden uit via het BTHome v2-protocol. De firmware scant passief: er is geen koppeling of verbinding nodig.

---

### Firmware flashen

#### Eerste installatie via flash_esp.py

Voor de eerste installatie heeft u een USB-verbinding nodig. Het `flash_esp.py`-script verwerkt alle stappen automatisch.

**Vereisten:**
- Python 3.x
- `esptool` Python-pakket: `pip install esptool`
- USB-kabel (micro-USB voor ESP8266, USB-C voor ESP32/OTGW32)

**Stappen:**

1. Download de nieuwste release van GitHub:
   `https://github.com/rvdbreemen/OTGW-firmware/releases`

2. Pak het ziparchief uit. U vindt hierin de firmwarebinaries en het `flash_esp.py`-script.

3. Sluit de OTGW aan via USB.

4. Bepaal de seriële poort (Windows: `COM3` of hoger; Linux: `/dev/ttyUSB0` of `/dev/ttyACM0`; macOS: `/dev/cu.usbserial-*`).

5. Flash de firmware:

```bash
# ESP8266
python flash_esp.py --port COM3 --chip esp8266

# ESP32
python flash_esp.py --port COM3 --chip esp32
```

Het script flasht zowel de firmwarebinary als het LittleFS-bestandssysteem.

**Let op:** De seriële poort van de ESP wordt op Windows mogelijk niet automatisch herkend. Installeer dan de CH340- of CP210x-driver, afhankelijk van de USB-chip op uw module.

#### Bouwen vanuit broncode met PlatformIO

Als u de firmware wilt bouwen vanuit de broncode, gebruik dan PlatformIO:

```bash
# Installeer PlatformIO (als extensie in VS Code of via pip)
pip install platformio

# Kloon de repository
git clone https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

# Bouw voor ESP8266
pio run -e esp8266

# Bouw voor ESP32
pio run -e esp32

# Of gebruik het build-script
python build.py              # firmware + filesystem
python build.py --firmware   # alleen firmware
python build.py --clean      # schone build
```

#### OTA-update via de webinterface

Zodra de firmware eenmaal via USB geflasht is, kunt u daarna altijd updaten via Wi-Fi (OTA). Zie Hoofdstuk 3 (sectie OTA firmware-update) voor de stappen.

---

### Eerste Wi-Fi-configuratie

Bij de allereerste start, of na het wissen van de Wi-Fi-instellingen, opent de OTGW een eigen Wi-Fi-accesspoint (AP).

**Stappen voor de eerste configuratie:**

1. Zet de OTGW aan. Wacht 10-15 seconden.

2. Zoek op uw telefoon of laptop naar Wi-Fi-netwerken. U ziet een netwerk met de naam `otgw-XXXXXXXXXXXX` (waarbij `XXXXXXXXXXXX` het MAC-adres is).

3. Verbinding maken met dit netwerk. Er is geen wachtwoord vereist.

4. Een captive portal opent automatisch. Als dat niet het geval is, open dan een browser en navigeer naar `http://192.168.4.1`.

5. Selecteer uw thuisnetwerk (SSID) in de lijst en voer het Wi-Fi-wachtwoord in.

6. Klik op **Opslaan** (of **Save**). De OTGW verbindt met uw thuisnetwerk en herstart.

7. Zoek het IP-adres van de OTGW in uw router, of gebruik de mDNS-hostnaam `http://otgw.local/` in uw browser.

**mDNS-opmerking:** De hostnaam `otgw.local` werkt op de meeste thuisnetwerken. Op sommige Windows-systemen werkt mDNS alleen als de Bonjour-service actief is. Gebruik bij twijfel het IP-adres van uw router.

---

### AP-fallbackmodus

Als de OTGW drie opeenvolgende keren niet succesvol verbinding kan maken met het geconfigureerde Wi-Fi-netwerk (bijv. door een verkeerd wachtwoord of een tijdelijk onbeschikbaar netwerk), opent het apparaat automatisch het accesspoint als fallback. Zo kunt u altijd opnieuw configureren zonder serieel te hoeven aansluiten.

---

### Upgraden van een vorige versie

#### Via OTA vanuit de webinterface (aanbevolen)

1. Open de webinterface (`http://otgw.local/`).
2. Navigeer naar **Update** (of **Instellingen > Firmware-update**).
3. De pagina haalt de lijst met beschikbare GitHub-releases op.
4. Klik op **Update** naast de gewenste versie.
5. De firmware downloadt en installeert zichzelf. Het apparaat herstart automatisch.
6. Na de herstart controleert de webinterface of het apparaat weer online is.

**Van v1.3.x naar v2.0.0:** Uw bestaande instellingen worden automatisch geladen. Er zijn geen breaking changes in de instellingenstructuur, MQTT-topics of REST API-eindpunten.

#### Handmatige OTA via bestandsupload

Als alternatief kunt u een firmwarebinary handmatig uploaden:

1. Download de `.bin`-file van de GitHub-releasepagina.
2. Ga in de webinterface naar **Update > Handmatig uploaden**.
3. Selecteer het gedownloade bestand en klik op **Uploaden**.

**Let op:** Upload altijd de juiste binary voor uw platform: `firmware-esp8266.bin` voor ESP8266, `firmware-esp32.bin` voor ESP32. Het uploaden van een verkeerde binary kan het apparaat in een bootloop brengen; in dat geval is serieel herflasen via USB nodig.

---

### Driedubbele reset: Wi-Fi-instellingen wissen

Als u geen toegang meer heeft tot de webinterface en de Wi-Fi-instellingen wil wissen (bijv. omdat u naar een ander netwerk gaat), kunt u dit doen zonder USB-kabel:

1. Zet de OTGW aan.
2. Wacht tot de status-LED begint te knipperen (normaal opstartproces, 5-10 seconden).
3. Zet de OTGW uit.
4. Wacht 2 seconden.
5. Herhaal stappen 1-4 nog twee keer (dus in totaal drie keer aan- en uitzetten).
6. Bij de vierde start wist de firmware de Wi-Fi-instellingen en opent het AP.

Na het wissen van de Wi-Fi-instellingen volgt u de stappen voor eerste Wi-Fi-configuratie (zie hierboven). Overige instellingen (MQTT, SAT, sensorlabels) blijven bewaard.
