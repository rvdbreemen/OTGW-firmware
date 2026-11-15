# Beeldbrief: de OTGW agentic werkplaats als overzichtsplaat

| | |
|---|---|
| Doel | Een image generator (of een illustrator) genoeg houvast geven om een mooie, inhoudelijk juiste overzichtsplaat te maken van de agentic werkplaats en van BlueFlame Bot |
| Inhoudelijke bron | [`agentic-architecture.md`](agentic-architecture.md): elke knoop en elke pijl in deze brief staat daar beschreven met bronvermelding |
| Peildatum | 2026-11-15, `dev` @ `e22c59e` |
| Taak | TASK-1984 |
| Referentiewireframes | [`images/agentic-architecture-plate-a.svg`](images/agentic-architecture-plate-a.svg) (overzicht), [`images/agentic-architecture-plate-b.svg`](images/agentic-architecture-plate-b.svg) (BlueFlame Bot), met PNG-versies ernaast |

Deze brief beschrijft drie platen. **Plaat A** is het overzicht van de hele werkplaats. **Plaat B** zoomt in op BlueFlame Bot: hoe hij Discord, GitHub en Tweakers bewaakt en waar hij antwoordt. **Plaat C** is een smalle banner met de pijplijn van taak tot tester. Per plaat staan hier de boodschap, het raster, de zones met hun knopen, de verbindingen, de legenda en kant-en-klare prompts.

---

## 1. Hoe je deze brief gebruikt

Image generators zijn goed in sfeer, compositie en iconen, en slecht in veel kleine, exacte tekst. Kies daarom bewust een route:

| Route | Wanneer | Werkwijze |
|---|---|---|
| **A. Basis zonder tekst, labels erover** (aanbevolen) | Als de plaat inhoudelijk moet kloppen, bijvoorbeeld voor een presentatie of de README | Genereer de plaat met de prompt uit de sectie "zonder tekst". Leg daarna de labels uit de SVG-wireframe eroverheen in Figma, Inkscape of Affinity. De wireframe heeft dezelfde zones en coördinaten als de prompt. |
| **B. Directe prompt met labels** | Voor een snelle sfeerplaat of een model dat tekst goed rendert | Gebruik de volledige prompt. Houd je aan maximaal ongeveer 25 korte labels per plaat; de rest valt weg of wordt onleesbaar. |
| **C. Structuurreferentie** | Bij generators die een referentiebeeld accepteren (image-to-image, structure of layout reference) | Geef de PNG van de wireframe mee als layoutreferentie met lage tot middelhoge sterkte, en de prompt voor stijl en sfeer. |

Werkafspraken:

- **Prompts zijn in het Engels.** De meeste generators zijn vooral op Engelstalige bijschriften getraind en volgen Engelse compositie-instructies nauwkeuriger. De labels staan in beide talen in hoofdstuk 6; kies er één per plaat.
- **Korte labels.** Maximaal drie woorden per knoop. Details horen in het document, niet op de plaat.
- **Publicatieregels van het project gelden ook voor de plaat.** Noem de OTGW32-hardware niet bij naam (CLAUDE.md, AGENT-CORE, Discord-regel), zet geen Discord-gebruikers-ID's, webhook-URL's of tokens in beeld, en gebruik voor personen rolnamen ("Maintainer", "SAT-ontwerper"), geen foto-achtige portretten.
- **Geen merklogo's.** Gebruik generieke glyphs voor Discord, GitHub en Tweakers (een spraakwolk, een vertakking, een forumtegel). Generators vervormen logo's, en een vervormd logo oogt slordiger dan een neutraal icoon.
- **Controleer het resultaat** met de lijst in hoofdstuk 7 voordat je het deelt. Een mooie plaat met een pijl de verkeerde kant op is erger dan geen plaat.

---

## 2. Visuele taal (gedeeld door alle platen)

### 2.1 Metafoor

Het project maakt firmware voor een OpenTherm-gateway: het apparaat tussen thermostaat en cv-ketel. De visuele metafoor volgt daaruit. De werkplaats is een **ketelruimte**, en in het hart brandt een **blauwe waakvlam**: BlueFlame Bot, de agent die altijd aan staat, alle kanalen in de gaten houdt en de werkplaats van warmte voorziet. Mensen staan bovenaan, de kennis staat als een archief links, de pijplijn loopt als een transportband rechts, en onderaan gaat het product de deur uit.

### 2.2 Stijl

`dark technical infographic, clean flat vector with a slight isometric depth, thin precise linework, soft glow on the central flame only, generous negative space, editorial and calm, no clutter, no photorealism, no 3D render look`

### 2.3 Palet

Overgenomen uit de bestaande visuele versie (`agentic-setup.html`, donkere modus) en uitgebreid met één blauwe accentkleur voor de vlam.

| Rol | Kleur | Hex | Betekenis |
|---|---|---|---|
| Achtergrond | antraciet | `#101417` | De ruimte |
| Vlak | donker leisteen | `#171d21` | Kaarten en panelen |
| Lijnen en randen | grafiet | `#2b343b` | Rasters en kaders |
| Tekst | gebroken wit | `#e6eaed` | Labels |
| Gedempte tekst | staalgrijs | `#9aa6b0` | Subtitels, alleen-lezen-pijlen |
| **Mensen en beslismacht** | vlamoranje | `#ff8a4c` (zacht `#3a2216`) | Personen, mandaten, besluiten, wat naar buiten gaat |
| **Agents en AI-werk** | koel teal | `#4fc3d6` (zacht `#12313a`) | Sessies, subagents, opdrachten tussen agents |
| **BlueFlame Bot** | elektrisch blauw met witte kern | `#3d8bff` → `#4fc3d6`, kern `#e6f7ff` | Alleen de waakvlam en zijn bewakingsstralen |
| Poorten | amber | `#f0b04a` | Controles die een stap kunnen weigeren |
| Geslaagd, distributie | zachtgroen | `#7bc47f` | Groen licht, product naar buiten |
| Harde stop | koraalrood | `#ff7b6b` | Acties die per keer Roberts "ja" nodig hebben |

### 2.4 Typografie

Koppen in **Archivo** (vet, kapitalen met ruime letterafstand voor zonelabels), knooplabels in **Source Sans 3**, commando's en bestandsnamen in **JetBrains Mono**. Dezelfde families als `agentic-setup.html`.

### 2.5 Iconografie

| Element | Icoon |
|---|---|
| Eindgebruikers | groepje van drie eenvoudige figuren |
| Alpha-testers | figuur met reageerbuis of oscilloscoop |
| SAT-ontwerper (Sergeant D) | figuur met passer en tekendriehoek |
| Maintainer (Robert) | figuur met sleutel |
| Discord-kanaal | spraakwolk met hekje (`#`) |
| Direct bericht | gesloten envelop |
| GitHub | vertakkingssymbool (drie bolletjes met lijnen) |
| Tweakers | forumtegel met RSS-boogjes |
| BlueFlame Bot | blauwe vlam in een ronde brander, met drie bewakingsstralen |
| Subagent | kleine zeshoek met een monogram (S, H, O) voor het modeltype |
| Bench-tester | werkbank met twee printplaatjes en een WiFi-boog |
| Andere agents | drie kleine satellieten |
| Regels | boek met een bladwijzer |
| ADR's en ADR-kit | stempel met vinkje op een dossiermap |
| C4-documentatie | geneste rechthoeken (kaart) |
| SAT-beslisregister | gebonden logboek met genummerde regels |
| Open-vragenwachtrij | rij genummerde kaartjes |
| Backlog.md | kanbanbord met drie kolommen |
| OpenWolf | wolvenkop-silhouet boven een kaartenbak |
| graphify | netwerk van knopen en lijnen |
| Poort | schild met vinkje (amber) |
| Harde stop | hangslot (koraalrood) |
| Staand mandaat | klok met twee wijzers op 08:00 en 20:00 |
| Watermerk (laatst gecontroleerd) | zandloper |

### 2.6 Lijnsoorten

| Lijn | Betekenis |
|---|---|
| Doorgetrokken teal | opdracht of werk tussen agents |
| Gestippeld teal | alleen lezen, bewaken |
| Doorgetrokken blauw, gloeiend | bewakingsstraal van BlueFlame Bot (alleen plaat B) |
| Doorgetrokken oranje, dik | besluit of mandaat van een mens |
| Gestippeld grijs | input van gebruikers naar een kanaal |
| Dik groen met pijlpunt | product gaat naar buiten |
| Pijl met twee punten | tweerichtingsverkeer (lezen en antwoorden) |

---

## 3. Plaat A: de werkplaats in één beeld

### 3.1 Boodschap

De kijker moet na tien seconden drie dingen begrijpen:

1. **Eén coördinerende sessie, met BlueFlame Bot als stem, staat in het midden** en verbindt mensen, kanalen, kennis en pijplijn.
2. **Mensen beslissen, agents voeren uit.** Oranje lijnen (besluiten) komen alleen van mensen; teal lijnen (werk) lopen tussen agents.
3. **Niets gaat naar buiten zonder poorten.** De pijplijn rechts is een reeks controles, en de harde stops zitten op slot.

### 3.2 Formaat en raster

- Liggend **16:9**, bij voorkeur 3840 × 2160 (wireframe: viewBox 1920 × 1080).
- Raster van 12 kolommen en 9 rijen. Coördinaten hieronder in procenten van breedte (x) en hoogte (y).
- Leesrichting van boven naar beneden (mensen → kanalen → agents → wat naar buiten gaat), met links de kennis en rechts de pijplijn als flanken.

```
 x→ 0%                25%                 50%                 75%               100%
 y  ┌──────────────────────────────────────────────────────────────────────────┐
 0% │ MENSEN   [Eindgebruikers] [Alpha-testers] [SAT-ontwerper] [Maintainer]   │
14% │ KANALEN  [Tweakers][GitHub][Discord-support][#alpha-testing][#dev-sat-mqtt][DM] │
30% ├──────────────┬────────────────────────────────────────┬──────────────────┤
    │ KENNIS EN    │                                        │ PIJPLIJN EN      │
    │ REGELS       │      ( BlueFlame Bot: waakvlam )       │ POORTEN          │
    │ [Regels]     │   coördinerende sessie, met een ring   │ [Taak]           │
    │ [ADR-kit]    │   subagents eromheen:                  │ [Plan+review]    │
    │ [C4-kaart]   │   Sonnet · Haiku · Opus · Advisor      │ [Rood → groen]   │
    │ [SAT-reg.]   │                                        │ [Reviewteam]     │
    │ [Vragen]     │  [Bench-tester + testunits]  [Andere   │ [Gates]          │
    │ [Backlog]    │                               agents]  │ [Build · lock]   │
    │ [OpenWolf]   │                                        │ [Bench · OTA]    │
    │ [graphify]   │                                        │ [Push · post]    │
86% ├──────────────┴────────────────────────────────────────┴──────────────────┤
    │ NAAR BUITEN  [Alpha-zip] [Beta 2.x] [Stabiel 2.x 🔒] [1.x LTS 🔒]   bronnen: ○○○○ │
100%└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Zones en knopen

**Zone 1, Mensen (y 0-13 %, oranje kaarten).** Vier kaarten op gelijke afstand, elk met icoon, label en één regel ondertitel.

| Knoop | Ondertitel | x-midden |
|---|---|---|
| Eindgebruikers | meldingen en vragen | 14 % |
| Alpha-testers | flashen, captures, geen beslismacht | 38 % |
| SAT-ontwerper | bron van waarheid voor SAT | 62 % |
| Maintainer | scope, prioriteit, vrijgave | 86 % |

**Zone 2, Kanalen (y 15-28 %, gestippelde tegels).** Zes tegels.

| Knoop | Ondertitel | Richting t.o.v. BlueFlame Bot |
|---|---|---|
| Tweakers-topic | RSS, alleen lezen | in |
| GitHub | issues, PR's, releases | beide |
| Discord-support | vier kanalen: gelezen, plus release-aankondigingen | in (aankondigingen uit) |
| #alpha-testing | builds, verslagen, SAT-vragen | beide |
| #dev-sat-mqtt | backlog-commando's | beide |
| DM met de maintainer | één vraag per bericht | beide |

**Zone 3, Agentlaag (midden, y 30-85 %, x 24-76 %).**

- **Hart:** een ronde brander met een blauwe vlam, label "BlueFlame Bot", ondertitel "coördinerende sessie". Dit is het visuele zwaartepunt: de enige gloed op de plaat.
- **Ring eromheen (teal zeshoeken):** "Sonnet · implementatie", "Haiku · runners", "Opus · correctheidsreview", "Advisor · zelfreview", "Reviewlenzen".
- **Linksonder in de zone:** "Bench-tester" als werkbank met twee printplaatjes en een WiFi-boog, ondertitel "flasht en test als enige". Verbonden met de pijplijnstap "Bench · OTA".
- **Rechtsonder in de zone:** drie kleine satellieten, label "Andere agents", ondertitel "Codex · Copilot · CI-Claude".
- Boven de brander, klein: de skills als chips in een boog: `/implement-next-task`, `/backlog_discord`, `/check_otgw_issues`, `/beta-release-2x`, `/release-2x`, `/update-docs-2x`, `/analyze-capture`.

**Zone 4, Kennis en regels (links, y 30-85 %, x 0-22 %).** Een archiefkast met acht laden van boven naar beneden: Regels (CLAUDE.md, AGENTS.md), ADR's + ADR-kit, C4-kaart, SAT-beslisregister, Open vragen, Backlog.md, OpenWolf-geheugen, graphify-codegraaf.

**Zone 5, Pijplijn en poorten (rechts, y 30-85 %, x 78-100 %).** Een verticale transportband met negen stations van boven naar beneden; elk station een chip met een amber schildje: Taak, Plan + zelfreview, Code rood → groen, Reviewteam, evaluate · tests, Bump · hooks, Build · lock, Bench · OTA, Push · post.

**Zone 6, Naar buiten (y 87-100 %).** Vier tegels: "Alpha-zip per taak" (groen, met een klein lusje terug naar #alpha-testing), "Beta 2.x" (met klokje 08:00/20:00, staand mandaat), "Stabiel 2.x" (hangslot), "1.x LTS" (hangslot). Rechts in dezelfde band, klein en grijs: vier ronde badges voor externe kennisbronnen: "OpenTherm-matrix", "thermo-nova", "OpenTherm-spec", "toolchains".

### 3.4 Verbindingen

| Van | Naar | Lijn | Label |
|---|---|---|---|
| Eindgebruikers | Tweakers, GitHub, Discord-support | gestippeld grijs | meldingen |
| Alpha-testers | #alpha-testing | gestippeld grijs, twee punten | feedback, captures |
| SAT-ontwerper | #alpha-testing, #dev-sat-mqtt | oranje, twee punten | antwoorden, besluiten |
| Maintainer | DM met de maintainer | oranje, dik | besluiten, "ja" per harde stop |
| Maintainer | BlueFlame Bot | oranje, dik | opdrachten in de sessie |
| Tweakers, Discord-support | BlueFlame Bot | gestippeld teal | bewaken |
| GitHub, #alpha-testing, #dev-sat-mqtt, DM | BlueFlame Bot | teal, twee punten | lezen en antwoorden |
| BlueFlame Bot | ring subagents | teal | opdrachten |
| BlueFlame Bot | Kennis en regels | grijs, twee punten | leest regels, schrijft register en notes |
| BlueFlame Bot | Pijplijn | teal | voert uit |
| Pijplijn "Bench · OTA" | Bench-tester | teal, twee punten | testverzoek en OK |
| Pijplijn "Push · post" | Alpha-zip | groen, dik | publiceert |
| Alpha-zip | #alpha-testing | groen, dun, lus omhoog | naar testers |

### 3.5 Legenda

Onderin rechts, klein: oranje lijn = besluit van een mens; teal lijn = werk tussen agents; gestippeld = alleen lezen; groen = gaat naar buiten; amber schild = poort; hangslot = harde stop, alleen met Roberts "ja".

### 3.6 Prompts voor plaat A

**Volledig (route B):**

```text
A dark technical infographic poster, 16:9, titled "OTGW agentic workshop". Charcoal background (#101417), panels in dark slate (#171d21), thin graphite grid lines. Clean flat vector style with slight isometric depth, calm editorial layout, generous spacing, no clutter.

Top band labelled "PEOPLE": four orange-outlined cards evenly spaced: "End users" (group icon), "Alpha testers" (figure with test tube), "SAT designer" (figure with compass), "Maintainer" (figure with key).

Second band labelled "CHANNELS": six dashed tiles: "Tweakers (RSS)", "GitHub", "Discord support", "DM", "#dev-sat-mqtt", "#alpha-testing".

Centre: a round burner with a glowing electric-blue flame with a white-hot core, labelled "BlueFlame Bot" and "coordinating session". It is the only glowing element. Around it a ring of five small teal hexagons: "Sonnet", "Haiku", "Opus", "Advisor", "Review lenses". Lower left of the centre: a workbench with two small circuit boards and a WiFi arc, labelled "Bench tester". Lower right: three small satellites labelled "Other agents".

Left column labelled "KNOWLEDGE & RULES": a filing cabinet with eight drawers: "Rules", "ADRs + ADR kit", "C4 map", "SAT register", "Open questions", "Backlog", "OpenWolf memory", "Code graph".

Right column labelled "PIPELINE & GATES": a vertical conveyor with nine stations, each with a small amber shield: "Task", "Plan + review", "Red to green", "Review team", "Gates", "Bump + hooks", "Build (lock)", "Bench (OTA)", "Push + post".

Bottom band labelled "WHAT SHIPS": four tiles: "Alpha zip" (green), "Beta 2.x" (small clock icon), "Stable 2.x" (coral padlock), "1.x LTS" (coral padlock).

Connectors: thick orange lines only from people (decisions), teal lines between agents (work), dashed teal lines for read-only monitoring from "Tweakers" and "Discord support" to the flame, double-headed teal arrows between the flame and "GitHub", "#alpha-testing", "#dev-sat-mqtt", "DM", a thick green arrow from "Push + post" to "Alpha zip" looping back up to "#alpha-testing".
Typography: Archivo for headings, Source Sans for labels, JetBrains Mono for small code labels. Palette: orange #ff8a4c, teal #4fc3d6, blue #3d8bff, amber #f0b04a, green #7bc47f, coral #ff7b6b, text #e6eaed.
```

**Zonder tekst (route A):** gebruik dezelfde prompt, vervang elk label tussen aanhalingstekens door "an empty label plate", en voeg toe: `No text anywhere, no letters, no numbers; leave clean empty label plates for overlay.`

**Kort (generators met een korte promptlimiet):**

```text
Dark charcoal tech infographic, 16:9: glowing electric-blue pilot flame in a round burner at the centre ringed by small teal hexagons; orange person cards across the top; dashed channel tiles below them; a filing cabinet on the left; a vertical conveyor with amber shield checkpoints on the right; four output tiles along the bottom, two with coral padlocks; thin orange, teal and green connector lines; flat vector, calm, precise, generous negative space.
```

**Negatief (waar ondersteund):**

```text
photorealism, 3D render, people's faces, brand logos, watermark, lens flare, neon cyberpunk overload, clutter, tiny unreadable text, random numbers, gibberish lettering, extra glowing elements besides the central flame
```

---

## 4. Plaat B: BlueFlame Bot, de waakvlam

### 4.1 Boodschap

1. **BlueFlame Bot is één persona met drie bewakingsstralen**: Discord, GitHub en Tweakers. Hij leest veel en antwoordt alleen waar dat mag.
2. **Per kanaal gelden andere regels.** Tweakers wordt alleen gelezen. De vier Discord-supportkanalen worden gelezen; daar post hij alleen release-aankondigingen, nooit antwoorden op meldingen. In #alpha-testing, #dev-sat-mqtt, de DM met de maintainer en op GitHub reageert hij op personen.
3. **Alles wat binnenkomt, eindigt als iets controleerbaars**: een antwoord met mention, een backlog-taak, een genummerde registerentry of een vraag aan de maintainer.

### 4.2 Formaat en raster

Liggend **16:9** (wireframe: viewBox 1920 × 1080). De vlam staat iets onder het midden (x 50 %, y 55 %), met daaronder als fundament de techniek. Drie stralen waaieren uit naar drie "buitenposten": Discord links (groot, want daar gebeurt het meeste), GitHub rechtsboven, Tweakers rechtsonder. Rechts van de vlam een "uitvoerrek" met wat de bot produceert. Bovenaan een band met de twee bewakingslussen en hun zandlopers.

```
 ┌────────────────────────────────────────────────────────────────────────────┐
 │  LUSSEN  ⟳ /backlog_discord  ⧗ 5 min      ⟳ /check_otgw_issues  ⧗ 2 uur    │
 │          ⟳ GitHub-sweep (stap 0b)  vóór elke taak                          │
 ├──────────────────────────┬──────────────────────┬──────────────────────────┤
 │ DISCORD (buitenpost)     │                      │ GITHUB (buitenpost)      │
 │  #alpha-testing    ⇄     │                      │  issues  ⇄  PR's  ⇄      │
 │  #dev-sat-mqtt     ⇄     │      ( blauwe        │  release ←               │
 │  DM maintainer     ⇄     │        waakvlam )    ├──────────────────────────┤
 │  #beta-testing     ←     │     BlueFlame Bot    │ TWEAKERS (buitenpost)    │
 │  #devs-esp-firmware ←    │                      │  forumtopic, RSS  ←      │
 │  #english-support  ←     │  [schilden eromheen] │  NL → EN-samenvatting    │
 │  #nederlandse-ondersteuning ←                   │                          │
 ├──────────────────────────┴──────────────────────┴──────────────────────────┤
 │ FUNDAMENT  discord-mcp (Java, :8085) · supervisor · geplande taak · webhook │
 │ UITVOER →  antwoord met mention · backlog-taak · register-entry · DM-vraag · alpha-zip │
 └────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Knopen

**De vlam.** Blauwe vlam met witte kern in een ronde brander, label "BlueFlame Bot". Om de brander vijf kleine amber schilden met elk een regel (zie 4.4).

**Buitenpost Discord (links, x 0-36 %).** Een gebouw met zeven deuren, elk een kanaal. De drie bovenste deuren hebben een pijl met twee punten (lezen en antwoorden). De vier onderste hebben een inkomende gestippelde pijl (lezen); drie daarvan hebben daarnaast een dunne groene uitgaande pijl voor release-aankondigingen.

| Deur | Pijl | Ondertitel |
|---|---|---|
| #alpha-testing | twee punten | builds, verslagen, SAT-vragen, testers |
| #dev-sat-mqtt | twee punten | backlog-commando's |
| DM met de maintainer | twee punten | één vraag per bericht |
| #beta-testing | in, plus groen uit | gelezen · beta-aankondigingen |
| #devs-esp-firmware | in | alleen lezen |
| #english-support | in, plus groen uit | gelezen · stabiele release-post (EN) |
| #nederlandse-ondersteuning | in, plus groen uit | gelezen · stabiele release-post (NL) |

**Buitenpost GitHub (rechtsboven, x 64-100 %, y 25-55 %).** Drie tegels: "Issues" (twee punten: lezen, vragen om captures), "Pull requests" (twee punten: review als commentaar, mergen alleen na ja), "Releases" (alleen uit, beta-prerelease).

**Buitenpost Tweakers (rechtsonder, x 64-100 %, y 58-80 %).** Eén forumtegel met RSS-boogjes, inkomende gestippelde pijl, ondertitel "NL-berichten, EN-samenvatting".

**Lusband (boven, y 0-16 %).** Drie ronde pijlen met elk een zandloper:

| Lus | Zandloper | Wanneer |
|---|---|---|
| `/backlog_discord` | 5 min | vóór elke nieuwe taak; de onbeantwoord-sweep draait altijd |
| GitHub-sweep (stap 0b) | geen | vóór elke nieuwe taak |
| `/check_otgw_issues` | 2 uur | op verzoek; stopt als alle drie bronnen recent zijn bekeken |

**Fundament (onder de vlam, y 82-90 %).** Een smalle grijze balk: "discord-mcp (Java) · :8085 · supervisor · geplande taak · één tokenhouder", met aan het eind een apart klein blokje "webhook: BlueFlame Build Bot" dat alleen naar #alpha-testing wijst.

**Uitvoerrek (onder, y 90-100 %).** Vijf groene of oranje tegels: "Antwoord met mention", "Backlog-taak", "Register-entry", "Vraag aan maintainer" (oranje), "Alpha-zip".

### 4.4 Schilden rond de vlam

| Schild | Regel |
|---|---|
| English · answer first | Engels, het antwoord eerst, echte mention |
| No board name | de hardware wordt nooit bij naam genoemd |
| Testers advise | testers geven input, beslissen niet; verzoek wordt een taak met `needs-maintainer-ok` |
| Record first | een besluit van de SAT-ontwerper wordt eerst vastgelegd, dan uitgevoerd |
| One token | één proces houdt het bottoken vast |

### 4.5 Prompts voor plaat B

**Volledig:**

```text
A dark technical infographic poster, 16:9, titled "BlueFlame Bot: the pilot flame". Charcoal background (#101417), dark slate panels (#171d21), flat vector with slight isometric depth, calm and precise.

Centre, slightly below middle: a round burner with a glowing electric-blue flame (#3d8bff fading to teal #4fc3d6) with a white-hot core, labelled "BlueFlame Bot". Five small amber shields orbit the burner, labelled "English, answer first", "No board name", "Testers advise", "Record first", "One token". Three soft blue beams fan out from the flame like a lighthouse.

Left beam reaches a building labelled "DISCORD" with seven doors in a column: "#alpha-testing", "#dev-sat-mqtt", "DM maintainer" each with a double-headed teal arrow; "#beta-testing", "#devs-esp-firmware", "#english-support", "#nederlandse-ondersteuning" each with a dashed incoming arrow, and all of them except "#devs-esp-firmware" also with a thin green outgoing arrow for release announcements.

Upper-right beam reaches a panel labelled "GITHUB" with three tiles: "Issues" and "Pull requests" with double-headed arrows, "Releases" with an outgoing arrow.

Lower-right beam reaches a panel labelled "TWEAKERS" with one forum tile with RSS arcs and a dashed incoming arrow, caption "Dutch posts, English summary".

Top band: three circular loop arrows, each with a small hourglass: "/backlog_discord · 5 min", "GitHub sweep", "/check_otgw_issues · 2 h".

Under the flame a thin grey foundation bar: "discord-mcp (Java) · supervisor · one token", with a separate small block "Build webhook" pointing only to "#alpha-testing".

Bottom shelf of five output tiles: "Reply with mention", "Backlog task", "Register entry", "Question to maintainer" (orange), "Alpha zip" (green).
Typography: Archivo headings, Source Sans labels. Only the flame glows.
```

**Kort:**

```text
Dark charcoal infographic, 16:9: a glowing electric-blue pilot flame in a round burner at the centre, five small amber shields orbiting it, three soft blue lighthouse beams reaching a Discord building with seven doors on the left, a GitHub panel upper right and a forum tile with RSS arcs lower right; looping arrows with hourglasses across the top; a thin grey foundation bar under the flame; five output tiles along the bottom; flat vector, precise, calm.
```

Negatief: zoals bij plaat A.

---

## 5. Plaat C: van taak tot tester (banner)

### 5.1 Boodschap

Elke wijziging loopt dezelfde reeks poorten door, en alleen de bench-tester raakt hardware aan.

### 5.2 Formaat en compositie

Breed **3:1** (wireframe niet nodig; de compositie is één lijn). Een horizontale transportband van links naar rechts met elf stations. Teal stations zijn werk van agents, amber schilden zijn poorten, het oranje station is de mens, groen is publicatie.

| # | Station | Kleur | Poort die kan weigeren |
|---|---|---|---|
| 1 | Backlog-taak | teal | geen taak, geen werk (commit-msg-hook) |
| 2 | Plan + zelfreview | teal | advisor of Opus-review |
| 3 | SAT? → OK van de SAT-ontwerper | oranje | geen bouw zonder zijn OK bij regelgedrag |
| 4 | Code rood → groen | teal | oude code faalt, fix slaagt |
| 5 | Reviewteam | teal | reviewers moeten weerleggen |
| 6 | evaluate · tests | amber | geen nieuwe FAIL |
| 7 | Bump + commit-hooks | amber | geen nieuwe prerelease-tag, geen taakbestand |
| 8 | Build esp32-combo | amber | één build tegelijk op de machine |
| 9 | Bench via OTA | amber | bench-tester flasht en test |
| 10 | Push `dev` | teal | pas na het OK van de bench |
| 11 | Post alpha-zip | groen | githash ≠ HEAD, vuile `src/`, tag al gepubliceerd |

Boven de band, op afstand en met hangsloten: "Beta 2.x (08:00/20:00, staand mandaat)", "Stabiel 2.x", "main (1.x)".

### 5.3 Prompt

```text
A wide 3:1 dark infographic banner: a horizontal conveyor belt from left to right on a charcoal background with eleven stations as rounded chips: teal chips for agent work, amber shield chips for gates, one orange chip for a human approval, a green chip at the end with a small package leaving to the right. Above the belt, at a distance, three locked tiles with coral padlocks and one tile with a small clock. Flat vector, precise thin lines, calm editorial style, labels short and legible.
```

---

## 6. Labelsets

Kies één taal per plaat.

| Knoop | Nederlands | English |
|---|---|---|
| Zonelabels | MENSEN · KANALEN · AGENTLAAG · KENNIS EN REGELS · PIJPLIJN EN POORTEN · NAAR BUITEN | PEOPLE · CHANNELS · AGENT LAYER · KNOWLEDGE & RULES · PIPELINE & GATES · WHAT SHIPS |
| Rollen | Eindgebruikers · Alpha-testers · SAT-ontwerper · Maintainer | End users · Alpha testers · SAT designer · Maintainer |
| Kanalen | Tweakers (RSS) · GitHub · Discord-support · #alpha-testing · #dev-sat-mqtt · DM | Tweakers (RSS) · GitHub · Discord support · #alpha-testing · #dev-sat-mqtt · DM |
| Hart | BlueFlame Bot · coördinerende sessie | BlueFlame Bot · coordinating session |
| Subagents | Sonnet · Haiku · Opus · Advisor · Reviewlenzen | Sonnet · Haiku · Opus · Advisor · Review lenses |
| Overige agents | Bench-tester · Andere agents | Bench tester · Other agents |
| Kennis | Regels · ADR's + ADR-kit · C4-kaart · SAT-register · Open vragen · Backlog · OpenWolf · Codegraaf | Rules · ADRs + ADR kit · C4 map · SAT register · Open questions · Backlog · OpenWolf · Code graph |
| Pijplijn | Taak · Plan + review · Rood → groen · Reviewteam · Poorten · Bump + hooks · Build (lock) · Bench (OTA) · Push + post | Task · Plan + review · Red to green · Review team · Gates · Bump + hooks · Build (lock) · Bench (OTA) · Push + post |
| Naar buiten | Alpha-zip · Beta 2.x · Stabiel 2.x · 1.x LTS | Alpha zip · Beta 2.x · Stable 2.x · 1.x LTS |
| Schilden (plaat B) | Engels, antwoord eerst · Geen boardnaam · Testers adviseren · Eerst vastleggen · Eén token | English, answer first · No board name · Testers advise · Record first · One token |

---

## 7. Controlelijst voor het resultaat

Loop deze lijst na voordat je een gegenereerde plaat deelt. Elk punt is een inhoudelijke fout die een generator makkelijk maakt.

- [ ] Oranje lijnen beginnen alleen bij mensen. Geen oranje lijn van een agent naar een andere agent.
- [ ] De pijl van Tweakers wijst alleen naar BlueFlame Bot toe (alleen lezen). De Discord-supportkanalen hebben een inkomende leespijl en hooguit een dunne groene aankondigingspijl, nooit een pijl met twee punten.
- [ ] #alpha-testing, #dev-sat-mqtt, de DM en GitHub hebben pijlen in beide richtingen.
- [ ] Alleen de bench-tester is met de testunits verbonden; de centrale sessie en de subagents raken geen hardware aan.
- [ ] "Stabiel 2.x" en "1.x LTS" dragen een hangslot; "Beta 2.x" heeft een klokje, geen hangslot.
- [ ] De alpha-zip gaat naar #alpha-testing, niet naar GitHub-releases.
- [ ] De SAT-ontwerper is verbonden met #alpha-testing en #dev-sat-mqtt, niet met GitHub of Tweakers.
- [ ] Er staat nergens een hardwarenaam, gebruikers-ID, URL of token in beeld.
- [ ] Er gloeit maar één element: de vlam.
- [ ] Labels zijn leesbaar en correct gespeld; bij twijfel route A (labels erover).

---

## 8. Deze brief bijwerken

- Verandert de architectuur in `agentic-architecture.md`, werk dan eerst de knopen- en verbindingstabellen hier bij, daarna de SVG-wireframes in `images/`, en pas als laatste de prompts.
- Houd de labels kort en in beide talen gelijk.
- Docs-only: geen prerelease-bump, wel een `TASK-NNN` in de commit.
