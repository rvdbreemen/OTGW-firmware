# Oude Home Assistant discovery topics opruimen met MQTT Explorer

Een beginnersvriendelijke handleiding voor het opschonen van oude OTGW discovery
boodschappen en het verversen van Home Assistant sensoren.

> Dit is de Nederlandse beginnersversie. De volledige Engelstalige gids met drie
> methodes (MQTT Explorer, Home Assistant Developer Tools en de mosquitto
> command line) staat in [`MQTT_STALE_TOPICS_CLEANUP.md`](MQTT_STALE_TOPICS_CLEANUP.md).

## Voor je begint: wat gaan we doen?

Wanneer je nieuwe firmware op je OpenTherm Gateway zet, kan het gebeuren dat Home
Assistant dubbele sensoren laat zien — bijvoorbeeld twee keer dezelfde
temperatuur, of namen die eindigen op `_2`. Dat komt omdat de oude "discovery"
boodschappen nog op je MQTT broker staan opgeslagen.

We doen twee dingen:

1. De oude boodschappen weghalen via MQTT Explorer.
2. Home Assistant zijn sensoren laten vernieuwen.

Tijd: ongeveer 10 minuten. Geen voorkennis nodig.

---

## Wat heb je nodig?

- Een computer (Windows, Mac of Linux).
- Toegang tot je MQTT broker: gebruikersnaam en wachtwoord (dezelfde die Home
  Assistant gebruikt).
- Het adres van je OTGW: `http://otgw.local` of het IP-adres (bijvoorbeeld
  `http://192.168.1.50`).
- Toegang tot je Home Assistant interface.

---

## Stap 1 — Zoek je OTGW Unique ID op

Je OTGW heeft een unieke naam, zoiets als `otgw-a1b2c3`. Die heb je straks nodig
om je apparaat in MQTT Explorer terug te vinden.

1. Open een browser en ga naar je OTGW: `http://otgw.local` of
   `http://<IP-adres>`.
2. Klik bovenaan op **Settings**.
3. Scroll naar het kopje **MQTT**.
4. Bij het veldje **Unique ID** zie je een waarde zoals `otgw-a1b2c3`. Schrijf
   die op.

> Tip: laat dit tabblad open staan, je hebt het in Stap 6 nog een keer nodig.

> Heb je de **HA prefix** veranderd van het standaard `homeassistant` naar iets
> anders? Onthoud dan ook die waarde — overal waar hieronder `homeassistant`
> staat, gebruik je jouw eigen prefix.

---

## Stap 2 — MQTT Explorer downloaden

MQTT Explorer is een gratis programma waarmee je makkelijk in je MQTT broker kunt
kijken en boodschappen kunt verwijderen.

1. Ga naar **https://mqtt-explorer.com**.
2. Klik op **Download** en kies de versie voor jouw besturingssysteem.
3. Installeer het programma zoals je een gewoon programma installeert.
4. Open MQTT Explorer.

---

## Stap 3 — Verbinding maken met de MQTT broker

1. Klik in MQTT Explorer op het **+** teken bovenaan om een nieuwe verbinding toe
   te voegen.
2. Vul de gegevens in:
   - **Name**: kies een naam, bijvoorbeeld `Home MQTT`.
   - **Host**: het IP-adres van je MQTT broker (bijvoorbeeld `192.168.1.10`).
   - **Port**: meestal `1883`.
   - **Username**: dezelfde gebruikersnaam die Home Assistant gebruikt.
   - **Password**: het bijbehorende wachtwoord.
3. Klik onderaan op **Connect**.

Aan de linkerkant verschijnt nu een boom met topics.

> Lukt verbinden niet? Check de gegevens in Home Assistant onder
> **Settings → Devices & Services → MQTT → Configure**. Daar staan precies de
> juiste host, gebruikersnaam en wachtwoord.

---

## Stap 4 — Vind de discovery topics van je OTGW

Aan de linkerkant zie je een lijst met topics in een boomstructuur.

1. Klik op het driehoekje voor **`homeassistant`** om het uit te klappen.
2. Daaronder zie je mappen zoals **`sensor`**, **`binary_sensor`**,
   **`climate`** en **`number`**.
3. Klap **`sensor`** uit.
4. Zoek de map met jouw Unique ID (bijvoorbeeld `otgw-a1b2c3`).

Heb je hem gevonden? Mooi — daar staan de oude discovery boodschappen.

---

## Stap 5 — De oude topics verwijderen

Voor elk van deze mappen, als hij bestaat, doe je dezelfde actie:

- `homeassistant` → `sensor` → `<jouw Unique ID>`
- `homeassistant` → `binary_sensor` → `<jouw Unique ID>`
- `homeassistant` → `climate` → `<jouw Unique ID>`
- `homeassistant` → `number` → `<jouw Unique ID>`

**Per map:**

1. Klik met de **rechter** muisknop op de map met je Unique ID.
2. Kies **Delete topic** (of **Delete retained messages** — de naam verschilt
   per versie van MQTT Explorer).
3. Bevestig de actie.

De map verdwijnt uit de boom. Dat is precies de bedoeling.

> Bang om iets fout te doen? Geen zorgen: de OTGW publiceert deze boodschappen in
> Stap 6 vanzelf opnieuw, met de nieuwste, juiste inhoud.

---

## Stap 6 — De OTGW vragen om nieuwe discovery te sturen

Nu de oude boodschappen weg zijn, vragen we de OTGW om frisse versies.

**Optie 1 — via de webinterface (makkelijkst):**

1. Ga terug naar je OTGW webinterface.
2. Open het **MQTT** tabblad.
3. Klik op de knop **Re-publish discovery**.

**Optie 2 — gewoon even wachten:**

Doe je niets, dan stuurt de OTGW binnen 1 à 2 minuten vanzelf nieuwe discovery
boodschappen.

Je ziet in MQTT Explorer dat de mappen onder
`homeassistant/sensor/<jouw Unique ID>/` weer terugkomen. Dat is goed.

---

## Stap 7 — Home Assistant sensoren verversen

Home Assistant pakt de nieuwe discovery boodschappen meestal automatisch op.
Soms blijven er oude sensoren hangen. Hieronder vier opties, van mild naar
streng. Probeer ze in volgorde — vaak is A of B al genoeg.

### Optie A — Even wachten (1 à 2 minuten)

Open je Home Assistant dashboard en check of de OTGW sensoren weer kloppen. Vaak
lossen ze zichzelf op.

### Optie B — MQTT integratie herladen

1. Open Home Assistant.
2. Ga naar **Settings → Devices & Services**.
3. Zoek **MQTT** in de lijst.
4. Klik op de drie puntjes rechtsboven op de MQTT kaart.
5. Kies **Reload**.

### Optie C — Eén specifieke sensor verwijderen

Zie je nog een dubbele sensor, bijvoorbeeld eentje met `_2` aan het eind?

1. Ga naar **Settings → Devices & Services**.
2. Klik op **MQTT**.
3. Zoek je OTGW apparaat in de lijst en klik erop.
4. Scroll naar de lijst met sensoren.
5. Klik op de sensor die je wilt verwijderen.
6. Klik op het tandwieltje rechtsboven.
7. Kies **Delete**.

> Belangrijk: verwijder alléén de oude "spook"-sensor, niet de echte! Een echte
> sensor toont een actuele waarde. Een spooksensor toont meestal `unavailable`,
> `unknown`, of een hele oude waarde die niet meer verandert.

### Optie D — Home Assistant herstarten (laatste redmiddel)

Werkt geen van de bovenstaande? Een herstart helpt vrijwel altijd:

- **Settings → System → Restart**.

---

## Klaar — hoe weet je dat het gelukt is?

Open Home Assistant en bekijk je OTGW apparaat. Als je geen dubbele sensoren meer
ziet, geen `_2` namen, en alle waardes kloppen — dan ben je klaar.

---

## Wanneer moet je dit opschonen?

Je hoeft dit alleen te doen na een **firmware-upgrade die entiteitnamen of
topic-paden verandert**. Dat gebeurt meestal bij grote versie-upgrades
(bijvoorbeeld 1.3.x → 1.4.x of 1.4.x → 1.5.x). Patch-releases binnen dezelfde
minor versie vereisen normaal geen opschoning. De release notes vermelden het
wanneer opschonen wordt aangeraden.

---

## Hulp nodig?

Loopt iets vast? Vraag het op de OpenTherm Gateway Discord. Vermeld in je
bericht:

- Je firmware versie (te vinden onderaan de OTGW webinterface).
- Bij welke stap het misgaat.
- Een schermafbeelding van MQTT Explorer of Home Assistant — dat helpt enorm.
