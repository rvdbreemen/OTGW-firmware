## Hoofdstuk 12: Verschillen tussen een OTGW Classic met PIC en een OTGW32

---

Firmware 2.0 ondersteunt twee hardwareconfiguraties: de **OTGW Classic met een ESP32-S3-module** (met een PIC-coprocessor) en de **OpenTherm Gateway OTGW32** (geen PIC, OT-Direct). Het meeste wat u doet is gelijk. Dit hoofdstuk zet op een rij wat verschilt, zodat u weet welke delen van deze handleiding voor uw apparaat gelden. Hoofdstuk 11 beschrijft de OTGW32 uitgebreider.

> **De OTGW32 is een nieuw apparaat en moet zich nog in het veld bewijzen.** De hardwareversies 1.0 en 1.1 lijken sterk op elkaar; versie 1.1 heeft een optioneel bypass-relais dat niet is gemonteerd bij levering en dat de firmware niet gebruikt. Achter de Classic zitten alfa- en betaruns op meerdere apparaten; de OTGW32 heeft alleen op een testapparaat gedraaid, zonder thermostaat en zonder echte ketel.

### 12.1 Naast elkaar

| | OTGW Classic met een ESP32-S3-module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Wie spreekt OpenTherm** | De PIC-coprocessor. De ESP32-S3 praat met de PIC over een seriële verbinding. | De ESP32-S3 zelf (OT-Direct), met een eigen interface naar de ketel en naar de thermostaat. |
| **PIC-firmware en updates daarvan** | Ja. Werk hem bij vanuit de webinterface (Advanced, PIC firmware). Nooit via WiFi met OTmonitor. | Er is geen PIC. Het tabblad PIC firmware is verborgen. |
| **Seriële brug, poort 25238** | Geeft de seriële stroom van de PIC door. Standaard uit. Zolang SAT aan staat, filtert hij besturingscommando's. | Regel voor regel nagebootst. Standaard uit. Op een OTGW32 niet met OTmonitor getest. |
| **OTmonitor** | OTmonitor verbindt via poort 25238. | USB is geen OTmonitor-verbinding. Als een programma de USB-poort opent, kan het apparaat herstarten, afhankelijk van hoe het programma de USB-besturingslijnen zet; dit is niet met OTmonitor getest. |
| **Werkstanden** | De PIC bepaalt (gatewaygedrag). U stuurt hem met PIC-commando's. | Gateway (standaard), monitor, master, loopback (hoofdstuk 11.2). |
| **Flash en geheugen** | 4 MB flash. De LOLIN S3 Mini en S3 Mini Pro hebben extra geheugen (PSRAM). | 4 MB flash, geen PSRAM. |
| **Grafiekgeschiedenis** | Tot 7 dagen op een module met PSRAM. | 1 uur. |
| **Bluetooth (BLE) kamersensor** | Ja, op een module met PSRAM. | Kan zonder PSRAM niet worden aangezet. |
| **REST-verzoeken tegelijk** | Voorlopig maximaal 2. | Voorlopig maximaal 2; voor deze print niet gemeten. |
| **Ethernet en OLED** | OLED optioneel. Geen Ethernet. | Ethernet (W5500) en OLED worden door de firmware ondersteund; in onze tests niet op echte hardware gezien. |
| **Detectie bij het opstarten** | Vindt de PIC en onthoudt dat. | Vindt geen PIC en start OT-Direct. Tot 3 PIC-zoekpogingen bij elke start zolang Board mode op Auto staat. |
| **Bijwerken via WiFi** | De PIC blijft berichten doorgeven (niet op een testopstelling gemeten). | De verbinding thermostaat naar ketel is 1 tot 2 minuten onderbroken; de ketel draait op zijn eigen instellingen. |
| **Vrije headerpinnen** | Zoals op de Classic-print. | De standaardpinnen voor S0, uitgang en Dallas botsen met pinnen die de print gebruikt, en er komt geen vrije pin op het klemmenblok uit; zet de Dallas-pin op 4 (hoofdstuk 11.6). |
| **Ervaring in het veld** | Alfa- en betaruns op meerdere apparaten. | Nieuw apparaat, hardwareversies 1.0 en 1.1. |

### 12.2 Wat gelijk is

- De **webinterface**, de **REST-API**, **MQTT** en de **Home Assistant**-discovery.
- **SAT** (de Smart Autotune Thermostat) werkt op beide. Op de OTGW32 is het de enige regelaar die in de firmware is ingebouwd; zonder SAT regelt de thermostaat de ketel zoals gebruikelijk.
- De **instellingen**, de **updatepagina** (`/update`) en het **safeboot**-herstel.
- **Eén firmware-image** voor beide: de release-zip voor `esp32-combo`. De firmware merkt bij het opstarten op welke van de twee hij draait.

### 12.3 Wat u in deze handleiding leest

- **OTGW Classic:** hoofdstuk 1 tot en met 10 zoals geschreven. Sla hoofdstuk 11 over.
- **OTGW32:** hoofdstuk 1 tot en met 10, met deze uitzonderingen: alles over de PIC (PIC-firmware, PIC-commando's zoals `PR=`, de `otgw-pic/`-MQTT-topics, de PIC-resetknop in Home Assistant) geldt niet, omdat er geen PIC is. Lees hoofdstuk 11 voor de OT-Direct-standen.
- **Een probleem melden:** vermeld welke van de twee u heeft. De device info toont het als `hardware_type`: `otgw-classic` of `otgw32`.
