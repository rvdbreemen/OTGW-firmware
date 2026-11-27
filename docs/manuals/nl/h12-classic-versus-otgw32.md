## Hoofdstuk 12: Verschillen tussen een OTGW Classic met PIC en een OTGW32

---

Firmware 2.0 ondersteunt twee hardwareconfiguraties: de **OTGW Classic met een ESP32-S3-module** (met een PIC-coprocessor) en de **OpenTherm Gateway OTGW32** (geen PIC, OT-Direct). Het meeste wat u doet is gelijk. Dit hoofdstuk zet op een rij wat verschilt, zodat u weet welke delen van deze handleiding voor uw apparaat gelden. Hoofdstuk 11 beschrijft de OTGW32 uitgebreider.

> **De OTGW32 is nieuw en de firmware ervoor heeft nog niet in een echt huis gedraaid.** Versie 1.0 van de OTGW32 is de eerste productierun. De firmware gebruikt geen bypass-relais. De Classic is in alpha- en betatests op meerdere apparaten gedraaid; de OTGW32 heeft alleen op een testapparaat gedraaid, zonder thermostaat en zonder echte ketel.

**Welke heeft u?** Na de eerste flash laat het apparaat het zien: de webinterface heeft op een OTGW Classic een tabblad Advanced, PIC firmware en op een OTGW32 niet, en `hardware_type` in de device info (`/api/v2/device/info`) is `otgw-classic` of `otgw32`. Weet u niet zeker welke print u heeft, vraag het dan in het Discord-supportkanaal voordat u flasht: een fabrieksinstallatie wist het apparaat.

### 12.1 Naast elkaar

| | OTGW Classic met een ESP32-S3-module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Wie spreekt OpenTherm** | De PIC-coprocessor. De ESP32-S3 praat met de PIC over een seriële verbinding. | De ESP32-S3 zelf (OT-Direct), met eigen aansluitingen naar de ketel en naar de thermostaat. |
| **PIC-firmware en updates daarvan** | Ja. Werk hem bij vanuit de webinterface (Advanced, PIC firmware). Nooit via WiFi met OTmonitor. | Er is geen PIC. Het tabblad PIC firmware is verborgen. |
| **Seriële brug, poort 25238** | Geeft de seriële stroom van de PIC door. Standaard uit. Zolang SAT aan staat, filtert hij besturingscommando's. | Regel voor regel nagebootst. Standaard uit. Op een OTGW32 niet met OTmonitor getest. |
| **OTmonitor** | OTmonitor (een pc-programma voor de OpenTherm Gateway) verbindt via poort 25238. | USB is geen OTmonitor-verbinding. Als een programma de USB-poort opent, kan het apparaat herstarten, afhankelijk van hoe het programma de USB-besturingslijnen zet; dit is niet met OTmonitor getest. |
| **Werkstanden** | De PIC bepaalt (gatewaygedrag). U stuurt hem met PIC-commando's. | Gateway (standaard), monitor, master, loopback (hoofdstuk 11.2). |
| **Grafiekgeschiedenis** | Tot 7 dagen op een module met PSRAM. | 1 uur. |
| **Bluetooth (BLE) kamersensor** | Ja, op een module met PSRAM. | Uit op deze print (heeft PSRAM nodig). |
| **Ethernet en OLED** | OLED optioneel. Geen Ethernet. | Ethernet (W5500) en OLED worden door de firmware ondersteund; in onze tests niet op echte hardware gezien. |
| **Bijwerken via WiFi** | De PIC blijft berichten doorgeven (niet op een testapparaat gemeten). | De verbinding thermostaat naar ketel is 1 tot 2 minuten onderbroken; de ketel draait op zijn eigen instellingen. |
| **Ervaring in het veld** | Alpha- en betatests op meerdere apparaten. | Nieuw apparaat, versie 1.0 is de eerste productierun. |

### 12.1b Voor de nieuwsgierigen

Technische rijen: u hebt ze niet nodig om te kiezen of om uw apparaat te gebruiken.

| | OTGW Classic met een ESP32-S3-module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Flash en geheugen** | 4 MB flash. De LOLIN S3 Mini en S3 Mini Pro hebben extra geheugen (PSRAM). | 4 MB flash, geen PSRAM. |
| **REST-verzoeken tegelijk** | Voorlopig maximaal 2. | Voorlopig maximaal 2; voor deze print niet gemeten. |
| **Detectie bij het opstarten** | Vindt de PIC en onthoudt dat. | Vindt geen PIC en start OT-Direct. Tot 3 PIC-zoekpogingen bij elke start zolang Board mode op Auto staat. |
| **Vrije headerpinnen** | Zoals op de Classic-print. | Op de OTGW32 beginnen de Dallas-sensoren op GPIO 4 (de 1-wire-lijn) en de S0-teller op GPIO 5 (klem 5, DI); beide kunt u in de instellingen wijzigen, en het tellen van S0-pulsen is op een OTGW32 niet getest. De GPIO-uitgang heeft op deze print geen pad (hoofdstuk 11.6). Het 1-wire- en het DI-pad zijn alleen 3,3 V-ingangen, niet 5 V-tolerant: geen 5 V of 12 V erop. |

### 12.2 Wat gelijk is

- De **webinterface**, de **REST-API**, **MQTT** en de **Home Assistant**-discovery.
- **SAT** (de Smart Autotune Thermostat) werkt op beide. Op de OTGW32 is het de enige regelaar die in de firmware is ingebouwd; zonder SAT regelt de thermostaat de ketel zoals gebruikelijk.
- De **instellingen**, de **updatepagina** (`/update`) en het **safeboot**-herstel.
- Beide gebruiken dezelfde release-zip: download het bestand waarvan de naam `esp32-combo` bevat. De firmware merkt zelf op welke van de twee hij draait.

### 12.3 Wat u in deze handleiding leest

- **OTGW Classic:** hoofdstuk 1 tot en met 10 zoals geschreven. Sla hoofdstuk 11 over.
- **OTGW32:** hoofdstuk 1 tot en met 10, met deze uitzonderingen: alles over de PIC (PIC-firmware, PIC-commando's zoals `PR=`, de `otgw-pic/`-MQTT-topics, de PIC-resetknop in Home Assistant) geldt niet, omdat er geen PIC is. Lees hoofdstuk 11 voor de OT-Direct-standen.
- **Een probleem melden:** vermeld welke van de twee u heeft. De device info toont het als `hardware_type`: `otgw-classic` of `otgw32`.
