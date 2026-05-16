# PIC Firmware Handleiding

*→ Also available in [English](PIC_FIRMWARE_EN.md)*

Deze handleiding legt de rol uit van de PIC-microcontroller firmware in de OpenTherm
Gateway, hoe je die bijwerkt en wat de drie beschikbare firmware-images doen.

Raadpleeg voor gezaghebbende en actuele informatie altijd de
[website van Schelte Bron](https://otgw.tclcode.com/) — de oorspronkelijke ontwerper van
de OpenTherm Gateway hardware en PIC-firmware.

---

## Twee firmwares, één apparaat

De NodoShop OpenTherm Gateway bevat **twee aparte processors**, elk met zijn eigen firmware.
Begrijpen wat elk onderdeel doet helpt je de gateway werkend en up-to-date te houden.

### De PIC-microcontroller — het OpenTherm-brein

De gateway is gebouwd rondom een **PIC-microcontroller** (PIC16F88 of PIC16F1847). De PIC
zit fysiek op de OpenTherm-bus, de tweedraadse verbinding tussen jouw thermostaat en de
cv-ketel. Het is het enige onderdeel dat het OpenTherm-protocol rechtstreeks kan spreken:

- Hij leest elk bericht dat de thermostaat naar de ketel stuurt, en elke reactie van de ketel.
- Hij laat de gateway die berichten *onderscheppen* zodat de gateway bijvoorbeeld de aanvoer-
  watertemperatuur onafhankelijk van de thermostaat kan aanpassen.
- Hij stuurt commando's naar de ketel namens de gateway wanneer je een overschrijving aanvraagt.

Zonder werkende PIC-firmware:

- Kunnen er geen OpenTherm-berichten worden gelezen.
- Kunnen er geen setpoints worden overschreven.
- Bereikt er niets jouw smart home (geen MQTT-data, geen Home Assistant-sensoren, geen
  webinterface-waarden).

De PIC-firmware is geschreven en onderhouden door **Schelte Bron**, de oorspronkelijke
ontwerper van de OpenTherm Gateway hardware. De OTGW-firmware downloadt de nieuwste
PIC-firmware van [otgw.tclcode.com](https://otgw.tclcode.com/) en programmeert die voor
je in de PIC.

### De ESP8266 — de domotica-integratielaag

Naast de PIC zit een **ESP8266 Wi-Fi-module** met daarop de OTGW-firmware. Terwijl de PIC
het ruwe OpenTherm-bussignaal afhandelt, begrijpt de ESP8266 het protocol volledig — hij
decodeert alle 80+ bericht-ID's uit de OpenTherm-specificatie, bedient de ketel namens jou
en verbindt alles met je smart home via MQTT, Home Assistant en een REST-API.

Zie de **[ESP8266 Firmware Handleiding](FLASH_GUIDE_NL.md)** voor een volledig overzicht
van functies en instructies voor het flashen van de ESP8266-firmware.

---

## De drie PIC firmware-images

Schelte Bron biedt drie firmware-images aan voor de PIC. De OTGW-firmware slaat ze op in
het LittleFS-bestandssysteem zodat ze direct vanuit de webinterface kunnen worden geflasht.

### 1. `gateway.hex` — Standaard gateway-firmware

Dit is de normale bedrijfsfirmware. Hiermee wordt de volledige OpenTherm Gateway-
functionaliteit geïmplementeerd:

- Onderschept alle OpenTherm-berichten tussen thermostaat en cv-ketel.
- Laat de ESP8266 setpoints overschrijven, sensordata uitlezen en commando's injecteren.
- Ondersteunt alle OTGW-seriële commando's (`TT`, `SW`, `PR`, `GW`, enz.).
- Maakt Home Assistant-integratie, MQTT-publicatie en REST API-antwoorden mogelijk.

**Dit is de firmware die tijdens normaal gebruik actief moet zijn.**
Deze wordt automatisch geselecteerd bij de updatecontrole wanneer het apparaat in de
standaard gateway-modus werkt.

Broninformatie: [Gateway firmware details](https://otgw.tclcode.com/firmware.html)

### 2. `interface.hex` — Interface-firmware

De interface-firmware maakt van de OTGW-hardware een **serieel-naar-OpenTherm interface**
voor experimenten. De firmware rapporteert OpenTherm-berichten op de seriële poort, maar
stuurt ze niet zelfstandig door tussen thermostaat en ketel. Je moet zelf een OpenTherm-
bericht als 8 hex-tekens (plus carriage return) aanbieden; daarna bepaalt de firmware op
basis van de bericht-richting of het bericht naar thermostaat of ketel wordt gestuurd.

Gebruikssituaties:
- Experimenteren met het OpenTherm-protocol en eigen berichtmanipulatie.
- Situaties waarin je handmatige berichtcontrole wilt in plaats van volledige gateway-logica.

> **Let op:** Met de interface-firmware kunnen de gateway-specifieke overschrijvingscommando's
> (`TT`, `SW`, enz.) niet worden gebruikt. Schakel terug naar `gateway.hex` om de volledige
> gateway-functionaliteit te herstellen.

Zie voor meer details:
- [Interface firmware details](https://otgw.tclcode.com/interface.html)
- [Firmware-overzicht](https://otgw.tclcode.com/firmware.html)

### 3. `diagnose.hex` — Diagnostische firmware

De diagnostische firmware vervangt tijdelijk de normale gateway-firmware om problemen met
gateway-hardware en OpenTherm-bedrading te onderzoeken. De firmware biedt zes tests met
gedetailleerde timing- en spanningsinformatie, terwijl communicatie tussen thermostaat en
ketel in de meeste tests actief blijft.

Normale communicatie tussen thermostaat en ketel gaat door terwijl de meeste tests lopen.
De uitzondering is Test #4 (vertragingssymmetrie), waarvoor de master- en slave-interfaces
via een loopback op elkaar moeten worden aangesloten.

**Beschikbare tests:**

| # | Naam | Omschrijving |
|---|------|--------------|
| 1 | LED-test | Trekt LED-outputs één voor één laag om de LED-bedrading te controleren. Werkt zonder aangesloten thermostaat/ketel. Stuur `<CR>` om te beëindigen. |
| 2 | Bittiming — thermostaat | Rapporteert hoog/laag-duur (µs) voor elk thermostaatbericht. Elke waarde moet vallen in een halve-bit (400–650 µs) of een hele-bit (900–1150 µs). Vereist aangesloten thermostaat. Stuur `<CR>` om te beëindigen. |
| 3 | Bittiming — cv-ketel | Zelfde als test #2, maar voor de ketelzijde. Als er geen thermostaat is aangesloten, wordt elke seconde een testbericht verzonden. Stuur `<CR>` om te beëindigen. |
| 4 | Vertragingssymmetrie | Meet de voortplantingsvertraging van de optocoupler in beide richtingen (laag-naar-hoog en hoog-naar-laag). Vereist een loopback tussen master- en slave-interface. Test eindigt automatisch zodra alle vier metingen zijn voltooid. |
| 5 | Spanningsniveaus | Voert A/D-metingen uit op beide interfaces en de referentiespanning. Rapporteert de logische hoge en lage spanningsniveaus. Aan het einde vraagt het om een nieuwe referentiespanning (`VR`-commando). Nuttig als berichten niet correct worden gedecodeerd. |
| 6 | Rusttijden | Meet de rusttijd (ms) tussen OpenTherm-berichten. |

Flash `gateway.hex` terug nadat je de gewenste tests hebt uitgevoerd om de normale
werking te herstellen.

Broninformatie: [Diagnostische firmware details](https://otgw.tclcode.com/diagnose.html)

---

## Hoe je de PIC-firmware bijwerkt

### Via de webinterface (aanbevolen)

De OTGW-firmware webinterface biedt een ingebouwde PIC-firmware-upgrademogelijkheid. Dit
is de aanbevolen methode omdat de upgraderoutine (oorspronkelijk afkomstig van Schelte Brons
NodeMCU-firmware) alle low-level PIC-programmeerprotocolstappen automatisch afhandelt.

1. Open de webinterface in je browser (bijv. `http://<apparaat-ip>/`).
2. Ga naar **Instellingen → PIC Firmware** (of het tabblad **Update**, afhankelijk van
   je versie).
3. Het huidige PIC-firmware-type en -versie worden weergegeven.
4. Klik op **Controleer op update** om de geïnstalleerde versie te vergelijken met de
   nieuwste beschikbare versie op [otgw.tclcode.com](https://otgw.tclcode.com/).
5. Als er een update beschikbaar is (of als je van firmware-type wilt wisselen), selecteer
   het gewenste image: **gateway**, **interface** of **diagnose**.
6. Klik op **Flash** (of **Upgrade**). De firmware downloadt het `.hex`-bestand van
   `otgw.tclcode.com` via HTTP en programmeert het direct vanuit de ESP8266 in de PIC.
7. De voortgang wordt weergegeven in de interface. Schakel het apparaat niet uit tijdens
   het programmeren.
8. Na voltooiing herstart de PIC automatisch en wordt de nieuwe versie weergegeven.

> **Beveiligingsopmerking:** Het `.hex`-bestand wordt gedownload via onbeveiligd HTTP van
> `otgw.tclcode.com`. Zorg ervoor dat het apparaat op een vertrouwd lokaal netwerk is
> aangesloten.

> **Belangrijk:** Flash de PIC-firmware nooit met de **OTmonitor**-applicatie via Wi-Fi.
> Dit kan de PIC beschadigen en in een onherstelbare toestand achterlaten. Gebruik altijd
> de ingebouwde webinterface-upgradefunctie.

### Handmatig uploaden via de bestandssysteemverkenner *(alleen voor gevorderde gebruikers)*

> **⚠️ Alleen voor gevorderde gebruikers.** Deze methode is bedoeld voor ontwikkelaars of
> gebruikers die een eigen of gepatcht `.hex`-bestand hebben. Als je niet zeker weet of je
> dit nodig hebt, gebruik dan de hierboven beschreven webinterface-upgrade.

1. Open de bestandssysteemverkenner op `http://<apparaat-ip>/fs`.
2. Upload je `.hex`-bestand (bijv. `gateway.hex`) naar de root van het bestandssysteem.
3. Start de upgrade via de webinterface zoals hierboven beschreven.

---

## Welke PIC-processor heb ik?

De webinterface toont het gedetecteerde processortype naast de firmwareversie. Er zijn
twee hardwarevarianten:

| Processor | Gebruikt in firmwareversies |
|-----------|-----------------------------|
| PIC16F88 | Oudere gateway.hex-versies (< 5.x voor gateway) |
| PIC16F1847 | Nieuwere versies (geïntroduceerd in gateway 5.x / 6.x) |

De fout `OTGW_ERROR_DEVICE` tijdens een upgrade betekent dat je een `.hex`-bestand hebt
geprobeerd te flashen dat is gebouwd voor de verkeerde PIC-variant. De OTGW-firmware
detecteert de juiste variant automatisch en selecteert het juiste image van
`otgw.tclcode.com`.

---

## Meer informatie

- [OTGW-project van Schelte Bron](https://otgw.tclcode.com/) — hardware-ontwerp,
  PIC firmware-downloads, commandoreferentie en ondersteuningsforum.
- [OTGW firmware-commando's](https://otgw.tclcode.com/firmware.html) — volledige lijst
  van seriële commando's ondersteund door de PIC-firmware.
- [OTGW ondersteuningsforum](https://otgw.tclcode.com/support/forum) —
  gemeenschapsondersteuning voor hardware- en PIC-firmware-problemen.
- [ESP8266 Firmware Handleiding](FLASH_GUIDE_NL.md) — volledig functieoverzicht van de
  ESP8266-firmware en instructies voor het flashen.
