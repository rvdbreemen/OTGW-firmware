# Beeldbrief: de OTGW agentic werkplaats als overzichtsplaat

| | |
|---|---|
| Doel | Een image generator (of een illustrator) genoeg houvast geven om een mooie, inhoudelijk juiste overzichtsplaat te maken van de agentic werkplaats en van BlueFlame Bot |
| Inhoudelijke bron | [`agentic-setup.md`](agentic-setup.md): elke knoop en elke pijl in deze brief staat daar beschreven met bronvermelding |
| Peildatum | 2026-11-15, `dev` @ `e22c59e` |
| Taak | TASK-1984 |
| Referentiewireframes | [`images/agentic-setup-plate-a.svg`](images/agentic-setup-plate-a.svg) (overzicht), [`images/agentic-setup-plate-b.svg`](images/agentic-setup-plate-b.svg) (BlueFlame Bot), met PNG-versies ernaast |
| Thema en taal | Licht thema en Engelse labels (maintainer, 2026-11-15: "light theme voor de plaat", "alle diagrammen in het Engels"). De brief zelf is Nederlands; prompts en labels zijn Engels |
| Generator | [`tools/agentic-plates/`](../../tools/agentic-plates/README.md) maakt beide wireframes opnieuw |

Deze brief beschrijft drie platen. **Plaat A** is het overzicht van de hele werkplaats. **Plaat B** zoomt in op BlueFlame Bot: hoe hij Discord, GitHub en Tweakers bewaakt en waar hij antwoordt. **Plaat C** is een smalle banner met de pijplijn van taak tot tester. Per plaat staan hier de boodschap, het raster, de zones met hun knopen, de verbindingen, de legenda, een scènebeschrijving en kant-en-klare prompts.

---

## 1. Hoe je deze brief gebruikt

Image generators zijn goed in sfeer, compositie en iconen, en slecht in veel kleine, exacte tekst. Kies daarom bewust een route:

| Route | Wanneer | Werkwijze |
|---|---|---|
| **A. Basis zonder tekst, labels erover** (aanbevolen) | Als de plaat inhoudelijk moet kloppen, bijvoorbeeld voor een presentatie of de README | Genereer de plaat met de prompt uit de sectie "zonder tekst". Leg daarna de labels uit de SVG-wireframe eroverheen in Figma, Inkscape of Affinity. De wireframe heeft dezelfde zones en coördinaten als de prompt. |
| **B. Directe prompt met labels** | Voor een snelle sfeerplaat of een model dat tekst goed rendert | Gebruik de volledige prompt. Houd je aan maximaal ongeveer 25 korte labels per plaat; de rest valt weg of wordt onleesbaar. De volledige prompts voor plaat A en B noemen er meer: laat voor deze route eerst de ondertitels en daarna de kennislades of de skillchips weg, of kies route A. |
| **C. Structuurreferentie** | Bij generators die een referentiebeeld accepteren (image-to-image, structure of layout reference) | Geef de PNG van de wireframe mee als layoutreferentie met lage tot middelhoge sterkte, en de prompt voor stijl en sfeer. |

Werkafspraken:

- **Prompts en labels zijn in het Engels.** Alle diagrammen van dit project zijn Engels (maintainer, 2026-11-15), en de meeste generators volgen Engelse compositie-instructies nauwkeuriger. Hoofdstuk 6 geeft de Engelse labelset; de Nederlandse kolom is alleen ter vertaling.
- **Korte labels.** Maximaal drie woorden per knoop. Details horen in het document, niet op de plaat.
- **Publicatieregels van het project gelden ook voor de plaat.** Noem de OTGW32-hardware niet bij naam (CLAUDE.md, AGENT-CORE, Discord-regel), zet geen Discord-ID's (van gebruikers of kanalen), webhook-URL's, tokens of IP-adressen van testunits in beeld, en gebruik voor personen de Engelse rolnamen ("Maintainer", "SAT designer", "Alpha testers"), geen foto-achtige portretten. Een mention staat alleen als de letterlijke placeholder `<@ID>` op de plaat.
- **Geen merklogo's.** Gebruik generieke glyphs voor Discord, GitHub en Tweakers (een spraakwolk, een vertakking, een forumtegel). Generators vervormen logo's, en een vervormd logo oogt slordiger dan een neutraal icoon.
- **Controleer het resultaat** met de lijst in hoofdstuk 7 voordat je het deelt. Een mooie plaat met een pijl de verkeerde kant op is erger dan geen plaat.

---

## 2. Visuele taal (gedeeld door alle platen)

### 2.1 Metafoor

Het project maakt firmware voor een OpenTherm-gateway: het apparaat tussen thermostaat en cv-ketel. De visuele metafoor volgt daaruit. De werkplaats is een **ketelruimte**, en in het hart brandt een **blauwe waakvlam**: BlueFlame Bot, de agent die altijd aan staat, alle kanalen in de gaten houdt en de werkplaats van warmte voorziet. Mensen staan bovenaan, de kennis staat als een archief links, de pijplijn loopt als een transportband rechts, en onderaan gaat het product de deur uit.

### 2.2 Stijl

`light technical infographic on an off-white background, clean flat vector with a slight isometric depth, thin precise dark-grey linework, white cards with soft shadows, a soft blue glow on the central flame only, generous negative space, editorial and calm, no clutter, no photorealism, no 3D render look`

### 2.3 Palet

Het lichte palet van de bestaande visuele versie (`agentic-setup.html`, `:root`), uitgebreid met één blauwe accentkleur voor de vlam. Elke kleur heeft een zachte vulkleur voor vlakken en een volle kleur voor randen, lijnen en iconen.

| Rol | Kleur | Rand / lijn | Vulling | Betekenis |
|---|---|---|---|---|
| Achtergrond | gebroken wit | | `#f4f6f7` | De ruimte |
| Vlak | wit | `#d5dce2` | `#ffffff` | Kaarten en panelen, met een zachte schaduw |
| Tekst | inktzwart | | `#151a1f` | Labels |
| Gedempte tekst en grijze lijnen | leigrijs | `#56616c` | | Subtitels, gestippelde randen van de kanaaltegels, gebruikersinput, de kennislijn (alleen-lezen-pijlen zijn petrol gestippeld) |
| **Mensen en beslismacht** | gebrand oranje | `#c2410c` | `#fde7d9` | Personen, mandaten, besluiten |
| **Agents en AI-werk** | petrol | `#0e7490` | `#d9eff4` | Sessies, subagents, opdrachten tussen agents |
| **BlueFlame Bot** | kobaltblauw naar cyaan, witte kern | `#1d4ed8` → `#06b6d4` | halo `#dbeafe` | Alleen de waakvlam en zijn bewakingsstralen |
| Poorten | oker | `#a15c00` | `#fdf0d5` | Controles die een stap kunnen weigeren |
| Geslaagd, distributie | bosgroen | `#2f7d32` | `#e3f1e4` | Groen licht, product naar buiten |
| Harde stop | steenrood | `#b42318` | `#fde4e1` | Acties die per keer Roberts "ja" nodig hebben |

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
| Subagent | kleine zeshoek met een monogram (S, H, O voor het modeltype; A voor de advisor, L voor de reviewlenzen) |
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
| Poort | schild met vinkje (oker) |
| Harde stop | hangslot (steenrood) |
| Staand mandaat | klok met twee wijzers op 08:00 en 20:00 |
| Watermerk (laatst gecontroleerd) | zandloper |

### 2.6 Lijnsoorten

| Lijn | Betekenis |
|---|---|
| Doorgetrokken petrol | opdracht of werk tussen agents |
| Gestippeld petrol | alleen lezen, bewaken |
| Blauw | plaat B: brede doorschijnende bewakingsbundel van BlueFlame Bot; plaat A: gestippeld kobalt met twee punten tussen de vlam en de comms-sessie (de tweede stem) |
| Doorgetrokken oranje, dik | besluit of mandaat van een mens |
| Grijs | gestippeld: input van gebruikers naar een kanaal; doorgetrokken met twee punten (plaat A): de vlam leest de kennis en schrijft register en notes |
| Groen met pijlpunt | dik: product gaat naar buiten; dun gestippeld op plaat A: de zip terug naar #alpha-testing; dun op plaat B: aankondiging of antwoord in een supportkanaal |
| Pijl met twee punten | tweerichtingsverkeer (lezen en antwoorden) |

---

## 3. Plaat A: de werkplaats in één beeld

### 3.1 Boodschap

De kijker moet na tien seconden drie dingen begrijpen:

1. **BlueFlame Bot staat in het midden**: de stem van de coördinerende sessie en van de comms-sessie. De vlam verbindt mensen, kanalen, kennis en pijplijn.
2. **Mensen beslissen, agents voeren uit.** Oranje lijnen (besluiten) komen alleen van mensen; petrol lijnen (werk) lopen tussen agents.
3. **Niets gaat naar buiten zonder poorten.** De pijplijn rechts is een reeks controles, en de harde stops zitten op slot.

### 3.2 Formaat en raster

- Liggend **16:9**, bij voorkeur 3840 × 2160 (wireframe: viewBox 1920 × 1080).
- Raster van 12 kolommen en 9 rijen. Coördinaten hieronder in procenten van breedte (x) en hoogte (y).
- Leesrichting van boven naar beneden (mensen → kanalen → agents → wat naar buiten gaat), met links de kennis en rechts de pijplijn als flanken.

```
 x→ 0%                25%                 50%                 75%               100%
 y  ┌──────────────────────────────────────────────────────────────────────────┐
 0% │ PEOPLE   [End users]     [Maintainer]     [SAT designer]   [Alpha testers] │
14% │ CHANNELS [Tweakers][GitHub][Discord support][DM][#dev-sat-mqtt][#alpha-testing] │
    │ ────────────────────────── channel bus ──────────────────────────────────  │
30% ├──────────────┬────────────────────────────────────────┬──────────────────┤
    │ KNOWLEDGE &  │  skill chips          skill chips      │ PIPELINE &       │
    │ RULES        │      ( BlueFlame Bot: pilot flame )    │ GATES            │
    │ [Rules]      │  coordinator and comms session, ring:  │ [1 Task]         │
    │ [ADR kit]    │  Sonnet · Haiku · Opus · Advisor ·     │ [2 Plan+review]  │
    │ [C4 map]     │  Review lenses                         │ [3 Red→green]    │
    │ [SAT reg.]   │                                        │ [4 Review team]  │
    │ [Questions]  │                                        │ [5 Gates]        │
    │ [Backlog]    │ [Comms session][Other agents] [Bench   │ [6 Bump+hooks]   │
    │ [OpenWolf]   │                                tester] │ [7 Build·lock]   │
    │ [graphify]   │                                        │ [8 Bench][9 Push]│
86% ├──────────────┴────────────────────────────────────────┴──────────────────┤
    │ EXTERNAL KNOWLEDGE ○ ○ ○ ○      WHAT SHIPS [1.x LTS 🔒][Stable 2.x 🔒][Beta 2.x ◷][Alpha zip] │
100%└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Zones en knopen

**Zone 1, Mensen (y 0-13 %, oranje kaarten).** Vier kaarten op gelijke afstand, elk met icoon, label en één regel ondertitel.

| Knoop (label) | Ondertitel | x-midden |
|---|---|---|
| End users | reports and questions | 14 % |
| Maintainer | scope, priority, release | 38 % |
| SAT designer | source of truth for SAT | 62 % |
| Alpha testers | flash, capture, no decisions | 86 % |

**Zone 2, Kanalen (y 15-28 %, gestippelde tegels).** Zes tegels, met daaronder een dunne petrol "kanaalbus" waarop elk kanaal aansluit.

| Knoop (label) | Ondertitel | Richting t.o.v. BlueFlame Bot |
|---|---|---|
| Tweakers topic | RSS · read only | in |
| GitHub | issues · PRs · releases | beide |
| Discord support | 4 channels · read · announcements · replies in practice | in; uit: aankondigingen en, in de praktijk, antwoorden |
| DM with maintainer | one question per message | beide |
| #dev-sat-mqtt | backlog commands | beide |
| #alpha-testing | builds · reports · SAT | beide |

**Zone 3, Agentlaag (midden, y 30-85 %, x 24-76 %).**

- **Hart:** een ronde brander met een blauwe vlam, label "BlueFlame Bot", ondertitel "coordinator and comms session". Dit is het visuele zwaartepunt: het enige element met een zachte gloed.
- **Ring eromheen (petrol zeshoeken met een monogram):** "Sonnet · implementation", "Haiku · runners", "Opus · correctness review", "Advisor · plan self-review", "Review lenses · rules, tests, SAT".
- **Boven de brander:** de skills als chips in twee kolommen: `/implement-next-task`, `/backlog_discord`, `/check_otgw_issues` links; `/beta-release-2x`, `/release-2x`, `/update-docs-2x`, `/analyze-capture` rechts.
- **Linksonder in de zone:** twee kaarten naast elkaar: "Comms session" (blauwe rand, "Discord talk · post drafts", met een gestippelde blauwe lijn naar de brander, want ook de comms-sessie spreekt als BlueFlame Bot) en "Other agents" (drie kleine satellieten, "Codex · Copilot · CI Claude").
- **Rechtsonder in de zone:** "Bench tester" als werkbank met twee printplaatjes en een WiFi-boog, ondertitel "the only one that flashes", verbonden met pijplijnstap 8 "Bench via OTA".

**Zone 4, Kennis en regels (links, y 30-85 %, x 0-22 %).** Een archiefkast met acht laden van boven naar beneden: Regels (CLAUDE.md, AGENTS.md), ADR's + ADR-kit, C4-kaart, SAT-beslisregister, Open vragen, Backlog.md, OpenWolf-geheugen, graphify-codegraaf.

**Zone 5, Pijplijn en poorten (rechts, y 30-85 %, x 78-100 %).** Een verticale transportband met negen genummerde witte stations van boven naar beneden, elk met een oker schildje: Backlog task, Plan + self-review, Code red → green, Review team, evaluate · tests, Bump + commit hooks, Build esp32-combo · lock, Bench via OTA, Push dev · post zip. Stations 5 tot en met 8 hebben een oker rand (de harde poorten), de andere een petrol rand.

**Zone 6, Naar buiten (y 87-100 %, rechterhelft).** Vier tegels van links naar rechts, de alpha-zip direct onder de pijplijn: "1.x LTS" (steenrood hangslot), "Stable 2.x" (steenrood hangslot), "Beta 2.x" (oker klokje, "standing permission 08:00/20:00"), "Alpha zip" (groen, "per tested task", met een gestippelde groene lus langs de rechterrand terug naar #alpha-testing). Links in dezelfde band, klein en grijs: vier ronde badges voor externe kennisbronnen: "OpenTherm matrix", "thermo-nova", "OpenTherm spec", "toolchains".

### 3.4 Verbindingen

| Van | Naar | Lijn | Label |
|---|---|---|---|
| Eindgebruikers | Tweakers, GitHub, Discord-support | gestippeld grijs | meldingen |
| Alpha-testers | #alpha-testing | gestippeld grijs, twee punten | feedback, captures |
| SAT-ontwerper | #alpha-testing, #dev-sat-mqtt | oranje, twee punten | antwoorden, besluiten |
| Maintainer | DM met de maintainer | oranje, dik | besluiten, "ja" per harde stop |
| Maintainer | BlueFlame Bot | oranje, dik | opdrachten in de sessie |
| Tweakers, Discord-support | BlueFlame Bot | gestippeld petrol | bewaken |
| GitHub, #alpha-testing, #dev-sat-mqtt, DM | BlueFlame Bot | petrol, twee punten | lezen en antwoorden |
| BlueFlame Bot | ring subagents | petrol | opdrachten |
| BlueFlame Bot | Kennis en regels | grijs, twee punten | leest regels, schrijft register en notes |
| BlueFlame Bot | Pijplijn | petrol | voert uit |
| Pijplijn "Bench · OTA" | Bench-tester | petrol, twee punten | testverzoek en OK |
| Pijplijn "Push · post" | Alpha-zip | groen, dik | publiceert |
| Alpha-zip | #alpha-testing | groen, dun, gestippeld, lus omhoog langs de rechterrand | naar testers |
| Comms-sessie | BlueFlame Bot | gestippeld kobalt, twee punten | tweede stem van de persona: postconcepten en antwoorden in de supportkanalen |
| BlueFlame Bot | Discord-support | dun groen, terug | aankondigingen en, in de praktijk, antwoorden |

De referentieplaat tekent Discord-support nog als "4 channels · read + announcements" met alleen de gestippelde leeslijn. De dunne groene terugpijl en de ondertitel uit de tabel in 3.3 horen bij een volgende render.

### 3.5 Legenda

Rechtsboven op de titelregel, klein: oranje lijn = besluit van een mens; petrol lijn = werk tussen agents; gestippeld = alleen lezen; groen = gaat naar buiten; oker schild = poort; hangslot = harde stop, alleen met Roberts "ja".

### 3.6 Prompts voor plaat A

**Scènebeschrijving (wat de kijker ziet, van linksboven naar rechtsonder).** Linksboven staat in vette inktzwarte letters de titel "OTGW agentic workshop", met rechts op dezelfde regel een smalle legenda van vier lijntjes, een schildje en een hangslot. Daaronder hangen vier perzikkleurige kaarten met een gebrand-oranje rand als naamborden boven de werkvloer: de eindgebruikers, de maintainer met zijn sleutel, de SAT-ontwerper met zijn passer en de alpha-testers met hun reageerbuis. Dunne grijze stippellijnen en een paar stevige oranje lijnen zakken naar een rij van zes gestippelde kanaaltegels, die allemaal op één dunne petrol leiding uitkomen, als kranen op een verdeelbalk. Vanuit de maintainer valt één dikke oranje lijn dwars door die rij recht omlaag, het hart van de plaat in. Daar brandt, groot en als enige met een zachte gloed, de blauwe waakvlam met witte kern in een ronde lichtblauwe brander; vijf petrol zeshoeken staan er als gereedschap omheen en twee kolommen commandochips zweven erboven. Links leunt een witte archiefkast met acht laden; rechts loopt een transportband met negen genummerde stations omlaag, elk met een oker schildje, de stations 5 tot en met 8 met een oker rand. Linksonder in het middenvlak staat de kaart van de comms-sessie met een kobaltblauwe rand, door een gestippelde blauwe lijn met de vlam verbonden, naast drie kleine satellieten; rechtsonder staat de werkbank van de bench-tester, die als enige aan station 8 hangt. Helemaal onderaan liggen links vier grijze ronde badges met externe kennis en rechts vier uitgangstegels: twee zachtrode met een hangslot, een okergele met een klokje en een zachtgroene alpha-zip. Uit het laatste station valt een dikke groene pijl in die groene tegel, en een groene stippellijn klimt langs de rechterrand terug omhoog naar #alpha-testing. Het geheel is licht, rustig en ruim: wit op gebroken wit, met kleur alleen waar ze iets betekent.

**Volledig (route B):**

```text
A light technical infographic poster, 16:9, titled "OTGW agentic workshop", with a small legend of line styles, a shield and a padlock at the top right. Off-white background (#f4f6f7), white panels with thin light-grey borders (#d5dce2) and soft shadows, dark ink text (#151a1f). Clean flat vector style with slight isometric depth, calm editorial layout, generous spacing, no clutter.

Top band labelled "PEOPLE": four cards with a pale peach fill (#fde7d9) and a burnt-orange outline (#c2410c), evenly spaced: "End users" (group icon), "Maintainer" (figure with key), "SAT designer" (figure with compass), "Alpha testers" (figure with test tube).

Second band labelled "CHANNELS": six dashed tiles: "Tweakers (RSS)", "GitHub", "Discord support", "DM", "#dev-sat-mqtt", "#alpha-testing", all joined to one thin petrol-blue bus line below them.

Centre: a round burner with a pale blue face (#dbeafe) and a cobalt rim, holding a vivid blue flame (cobalt #1d4ed8 at the base fading to cyan #06b6d4 at the tip) with a white core, labelled "BlueFlame Bot" and "coordinator and comms session". It is the only element with a soft glow. Around it a ring of five small petrol-blue hexagons (#0e7490 outline, #d9eff4 fill): "Sonnet", "Haiku", "Opus", "Advisor", "Review lenses". Above the flame, two short columns of small monospace command chips. Lower left of the centre: two small cards side by side, "Comms session" (white card with a cobalt outline, joined to the flame by a dashed cobalt line with arrowheads at both ends, because it is the second voice of the bot) and "Other agents" (three small satellites). Lower right: a workbench with two small circuit boards and a WiFi arc, labelled "Bench tester", connected to the pipeline station "Bench (OTA)".

Left column labelled "KNOWLEDGE & RULES": a filing cabinet with eight drawers: "Rules", "ADRs + ADR kit", "C4 map", "SAT register", "Open questions", "Backlog", "OpenWolf memory", "Code graph".

Right column labelled "PIPELINE & GATES": a vertical conveyor with nine white stations, each with a small ochre shield (#a15c00); stations 5 to 8 have an ochre outline, the others a petrol outline: "Task", "Plan + review", "Red to green", "Review team", "Gates", "Bump + hooks", "Build (lock)", "Bench (OTA)", "Push + post".

Bottom band, right half, labelled "WHAT SHIPS": four tiles from left to right: "1.x LTS" (pale red fill, brick-red padlock #b42318), "Stable 2.x" (pale red fill, brick-red padlock), "Beta 2.x" (pale ochre fill, small ochre clock icon), "Alpha zip" (pale green fill, forest-green outline #2f7d32) directly under the conveyor. Bottom band, left half, labelled "EXTERNAL KNOWLEDGE": four small grey round badges: "OpenTherm matrix", "thermo-nova", "OpenTherm spec", "toolchains".

Connectors: burnt-orange lines only from people (decisions): two thick ones from "Maintainer", down to "DM" and straight down to the flame, and two double-headed ones from "SAT designer" to "#dev-sat-mqtt" and "#alpha-testing"; thin dashed grey lines from "End users" to "Tweakers", "GitHub" and "Discord support", and a double-headed dashed grey line between "Alpha testers" and "#alpha-testing"; one double-headed solid grey line between the flame and the filing cabinet; petrol-blue lines between agents (work): from the flame to each hexagon, from the flame to the conveyor, and double-headed between "Bench (OTA)" and the workbench; dashed petrol-blue lines for read-only monitoring from "Tweakers" and "Discord support" to the flame, a thin green return arrow from the flame to "Discord support" (announcements and, in practice, replies), a dashed cobalt line between the flame and "Comms session", double-headed petrol-blue arrows between the flame and "GitHub", "#alpha-testing", "#dev-sat-mqtt", "DM", a thick forest-green arrow from "Push + post" to "Alpha zip", and a thin dashed green line from "Alpha zip" up the right edge to "#alpha-testing".
Typography: Archivo for headings, Source Sans for labels, JetBrains Mono for small code labels. Palette: background #f4f6f7, cards #ffffff, text #151a1f, orange #c2410c, petrol #0e7490, cobalt #1d4ed8, ochre #a15c00, green #2f7d32, brick red #b42318.
```

**Zonder tekst (route A):** gebruik dezelfde prompt, vervang elk label tussen aanhalingstekens door "an empty label plate", en voeg toe: `No text anywhere, no letters, no numbers; leave clean empty label plates for overlay.`

**Kort (generators met een korte promptlimiet):**

```text
Light off-white tech infographic, 16:9: a vivid cobalt-to-cyan pilot flame in a round pale-blue burner at the centre ringed by five small petrol-blue hexagons; a small cobalt-outlined card lower left of the flame joined to it by a dashed cobalt line; a workbench with two circuit boards lower right; peach person cards with burnt-orange outlines across the top; dashed channel tiles below them; a white filing cabinet on the left; a vertical conveyor of white stations with ochre shield checkpoints on the right; four small grey round badges bottom left and four output tiles bottom right, two with brick-red padlocks, one with a clock, one pale green; thin orange, petrol and green connector lines; flat vector, white cards, soft shadows, calm, precise, generous negative space.
```

**Negatief (waar ondersteund):**

```text
dark background, night mode, photorealism, 3D render, people's faces, brand logos, watermark, lens flare, neon, heavy gradients, clutter, tiny unreadable text, random numbers, gibberish lettering, extra glowing elements besides the central flame
```

---

## 4. Plaat B: BlueFlame Bot, de waakvlam

### 4.1 Boodschap

1. **BlueFlame Bot is één persona met drie bewakingsstralen**: Discord, GitHub en Tweakers. Hij is geen apart programma maar de stem van twee sessies, de coördinator en de comms-sessie. Hij leest veel en antwoordt alleen waar dat mag.
2. **Per kanaal gelden andere regels.** Tweakers wordt alleen gelezen. De vier Discord-supportkanalen worden gelezen; daar post hij release-aankondigingen en, in de praktijk, ook antwoorden van de coördinator en de comms-sessie, zonder vastgelegde procedure. #beta-testing is sinds 2026-11-15 bovendien een kanaal voor verzoeken. #devs-esp-firmware wordt alleen gelezen. In #alpha-testing, #dev-sat-mqtt, de DM met de maintainer en op GitHub reageert hij op personen.
3. **De bewaking volgt het werk, niet de klok.** Twee lussen draaien vóór elke nieuwe taak (`/backlog_discord` en de GitHub-sweep). De derde, `/check_otgw_issues`, wordt met de hand gestart; alleen die leest de supportkanalen en Tweakers.
4. **Alles wat binnenkomt, eindigt als iets controleerbaars**: een antwoord met mention, een backlog-taak, een genummerde registerentry, een triagelijst, een vraag aan de maintainer of een alpha-zip met verslag.

### 4.2 Formaat en raster

Liggend **16:9** (wireframe: viewBox 1920 × 1080). De vlam staat in het midden van het middenvlak (x 50 %, y 44 %) en is het enige element met een gloed. Drie stralen waaieren uit naar drie "buitenposten": Discord links (groot, want daar gebeurt het meeste), GitHub rechtsboven, Tweakers rechtsonder. Bovenaan ligt een band met de drie bewakingslussen. Onder het middenvlak ligt eerst een rail met wat de bot oplevert, en helemaal onderaan het fundament met de techniek.

| Band of paneel | x | y |
|---|---|---|
| Titelregel met legenda | 0-100 % | 0-6 % |
| WATCH LOOPS, drie kaarten | 2-98 % | 7-15 % |
| DISCORD, zeven deuren | 2-33 % | 17-71 % |
| Vlam met vijf schilden en drie stralen | 34-66 % | 22-66 % |
| GITHUB, drie tegels | 67-98 % | 17-43 % |
| TWEAKERS, één tegel | 67-98 % | 46-65 % |
| WHAT IT PRODUCES, zes tegels | 2-97 % | 73-87 % |
| FOUNDATION, drie balken | 2-98 % | 89-98 % |

```
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ BlueFlame Bot: the pilot flame          legend: monitoring · read and reply │
 │                              · read only · to the maintainer · ships · rule │
 │ WATCH LOOPS                                                                 │
 │ [⟳ /backlog_discord ⧗]   [⟳ GitHub sweep]        [⟳ /check_otgw_issues ⧗]   │
 │   step 0, every task       step 0b, every task      started by hand         │
 ├────────────────────────┬───────────────────────┬────────────────────────────┤
 │ DISCORD                │      (English,        │ GITHUB  (maintainer acct)  │
 │  #alpha-testing    ⇄   │     answer first)     │  ⇄ Issues                  │
 │  #dev-sat-mqtt     ⇄   │ (One token) (No board │  ⇄ Pull requests           │
 │  DM with maintainer ⇄  │   holder)      name)  │  → Releases                │
 │ ┄#beta-testing     ⇠→  │     ( pilot flame )   ├────────────────────────────┤
 │ ┄#devs-esp-firmware ⇠  │ (Testers)   (Record   │ TWEAKERS  (RSS, read only) │
 │ ┄#english-support  ⇠→  │  advise)     first)   │  ⇠ Forum topic             │
 │ ┄#nederlandse-     ⇠→  │     BlueFlame Bot     │    Dutch → English summary │
 │   ondersteuning        │ voice of coordinator  │                            │
 │                        │  and comms session    │                            │
 ├────────────────────────┴───────────┬───────────┴────────────────────────────┤
 │ WHAT IT PRODUCES                   │ (one line down from the flame)         │
 │ [Reply][Backlog task][Register entry][Triage list][Question to maintainer]  │
 │                                                       [Alpha zip + report]  │
 │ FOUNDATION                                                                  │
 │ [discord-mcp · :8085 · supervisor · scheduled task] [watermarks]            │
 │ [gh as maintainer · RSS · build webhook · bot REST]                         │
 └─────────────────────────────────────────────────────────────────────────────┘
   ⇄ read and reply    ⇠ read only (dashed)    → ships (green)    ┄ dashed frame
```

### 4.3 Knopen

**De vlam.** Blauwe vlam met witte kern in een ronde lichtblauwe brander met kobaltrand, label "BlueFlame Bot", ondertitel "voice of the coordinator and the comms session". Om de brander vijf oker schilden met elk een regel (zie 4.4).

**Buitenpost Discord (links).** Een wit paneel met de kop "DISCORD" en de ondertitel "through discord-mcp (and bot REST) · posts as BlueFlame Bot". Daarin zeven deuren onder elkaar, elk een kanaal. De drie bovenste hebben een dichte petrol rand: daar leest en antwoordt de bot. De vier onderste hebben een gestippelde grijze rand: het zijn luisterposten.

| Deur | Rand | Pijl | Ondertitel op de plaat |
|---|---|---|---|
| #alpha-testing | dicht | twee punten, petrol | builds · reports · SAT questions · testers |
| #dev-sat-mqtt | dicht | twee punten, petrol | backlog commands · SAT rounds |
| DM with maintainer | dicht | twee punten, oranje | one question per message, with advice |
| #beta-testing | gestippeld | stippelpijl naar de vlam, plus dunne groene pijl terug | read · beta announcements · requests since Nov 15 |
| #devs-esp-firmware | gestippeld | alleen een stippelpijl naar de vlam | read only |
| #english-support | gestippeld | stippelpijl naar de vlam, plus dunne groene pijl terug | read · release post (EN) · replies in practice |
| #nederlandse-ondersteuning | gestippeld | stippelpijl naar de vlam, plus dunne groene pijl terug | read · release post (NL) · replies in practice |

De dunne groene pijl terug staat voor twee dingen tegelijk: de aankondiging (beta in #beta-testing, een stabiele release in de twee ondersteuningskanalen) en de antwoorden die coördinator en comms-sessie er in de praktijk geven. In #beta-testing komen sinds 2026-11-15 ook verzoeken binnen, met een ontwerpvoorstel van de bot als antwoord. Geen van de twee lussen die vóór elke taak draaien leest deze vier kanalen; alleen de met de hand gestarte `/check_otgw_issues` doet dat.

**Buitenpost GitHub (rechtsboven).** Kop "GITHUB", ondertitel "through gh · under the maintainer account": de bot heeft op GitHub geen eigen identiteit. Drie tegels:

| Tegel | Rand | Pijl | Ondertitel op de plaat |
|---|---|---|---|
| Issues | petrol | twee punten | read · ask for captures · answer questions |
| Pull requests | petrol | twee punten | review as comment · merge after yes |
| Releases | groen | alleen uit, groen | beta prerelease (draft first) |

**Buitenpost Tweakers (rechtsonder).** Kop "TWEAKERS", ondertitel "forum topic via RSS · read only". Eén gestippelde forumtegel "Forum topic" met de ondertitel "Dutch posts → English summary · keyword filter", een regel in codeletter met de publieke hostnaam van het forum en "RSS", en alleen een stippelpijl naar de vlam toe. Er is geen pijl terug: een manier om op Tweakers te posten bestaat niet.

**Lusband (boven).** Drie witte kaarten met kobaltrand, elk met een ronde luspijl. Een zandloper rechtsboven in de kaart betekent: deze lus heeft een watermerk en stopt vroeg als dat vers is.

| Lus | Zandloper | Regel 1 op de plaat | Regel 2 op de plaat (kobalt) |
|---|---|---|---|
| `/backlog_discord` | ja | early exit under 5 min · unanswered sweep always runs | step 0, before every new task |
| GitHub sweep | nee | open issues and PRs · dedupe on URL | step 0b, before every new task |
| `/check_otgw_issues` | ja | early exit when all three sources < 2 h | started by hand, with checkpoints |

`/check_otgw_issues` wordt met de hand gestart; geen skill roept het aan en er is geen planner. Het antwoordt zelf niet: het levert een triagelijst en taken op.

**Uitvoerrail "WHAT IT PRODUCES" (onder het middenvlak).** Vanuit de naam onder de vlam zakt één rechte petrol lijn naar een horizontale rail die zich over zes tegels verdeelt:

| Tegel | Kleur | Ondertitel op de plaat |
|---|---|---|
| Reply | wit, petrol rand | with a real `<@ID>` mention |
| Backlog task | wit, petrol rand | bug · github · needs-maintainer-ok |
| Register entry | wit, petrol rand | SAT decision before the build |
| Triage list | wit, petrol rand | in the session, developer picks |
| Question to maintainer | perzik, oranje rand, oranje pijl | DM: options + advice |
| Alpha zip + report | zachtgroen, groene rand, groene pijl | after the bench OK |

`<@ID>` is hier een letterlijke placeholder; zet nooit een echt gebruikers-ID op de plaat.

**Fundament "FOUNDATION" (helemaal onderaan).** Drie smalle witte balken met grijze rand en tekst in codeletter:

| Balk | Tekst op de plaat | Betekenis |
|---|---|---|
| links | discord-mcp (Java) · :8085 · supervisor · scheduled task | het lokale proces dat het bottoken vasthoudt, met zijn bewaker |
| midden | watermarks: *_last_checked.txt | waar de lussen onthouden tot waar ze gelezen hebben |
| rechts | gh as maintainer · RSS · build webhook · bot REST | de vier andere paden: GitHub, de Tweakers-feed, de webhook die de zip post (alleen naar #alpha-testing) en de Discord REST API met het bottoken |

Over "bot REST": lezen langs dat pad is aangetoond, posten niet. Teken het daarom als onderdeel van het fundament en niet als een eigen pijl naar een kanaal.

### 4.4 Schilden rond de vlam

| Schild | Plaats t.o.v. de vlam | Regel |
|---|---|---|
| English, answer first | boven | Engels, het antwoord eerst, echte mention |
| One token holder | linksboven | één proces houdt het bottoken vast |
| No board name | rechtsboven | de hardware wordt nooit bij naam genoemd |
| Testers advise | linksonder | testers geven input, beslissen niet; een verzoek wordt een taak met `needs-maintainer-ok` |
| Record first | rechtsonder | een besluit van de SAT-ontwerper wordt eerst vastgelegd, dan uitgevoerd |

### 4.5 Verbindingen en legenda

| Van | Naar | Lijn | Betekenis |
|---|---|---|---|
| Vlam | Discord, GitHub, Tweakers | drie brede doorschijnende lichtblauwe bundels, elk met een dikkere lichtblauwe lijn als kern | bewaken |
| #alpha-testing, #dev-sat-mqtt | Vlam | petrol, twee punten | lezen en antwoorden |
| DM with maintainer | Vlam | oranje, twee punten | vraag aan en besluit van de maintainer |
| De vier gestippelde Discord-deuren | Vlam | petrol stippelpijl | alleen lezen, via `/check_otgw_issues` |
| Vlam | #beta-testing, #english-support, #nederlandse-ondersteuning | dun groen | aankondiging en, in de praktijk, antwoorden |
| Issues, Pull requests | Vlam | petrol, twee punten | lezen en antwoorden, onder het maintainer-account |
| Vlam | Releases | groen | beta-prerelease gaat naar buiten |
| Forum topic | Vlam | petrol stippelpijl | alleen lezen |
| Vlam | de zes uitvoertegels | petrol rail; oranje pijl bij de vraag aan de maintainer, groene pijl bij de alpha-zip | wat de bot oplevert |

De legenda staat rechtsboven op de titelregel, in zes tekens: "monitoring" (brede lichtblauwe lijn), "read and reply" (petrol pijl met twee punten), "read only" (petrol stippelpijl), "to the maintainer" (oranje pijl), "ships" (groene pijl), "ground rule" (oker schild).

### 4.6 Prompts voor plaat B

**Scènebeschrijving (wat de kijker ziet, van linksboven naar rechtsonder).** Linksboven staat in vette inktzwarte letters de titel "BlueFlame Bot: the pilot flame", met rechts op dezelfde regel een legenda van zes tekens: een brede lichtblauwe lijn, een petrol pijl met twee punten, een stippelpijl, een oranje pijl, een groene pijl en een oker schildje. Daaronder liggen drie brede witte kaarten met een kobaltblauwe rand naast elkaar, elk met een ronde luspijl; de twee buitenste dragen een klein zandlopertje, de middelste niet. In het midden van de plaat brandt de waakvlam, kobalt aan de voet en cyaan in de punt, met een witte druppel als kern, in een ronde lichtblauwe brander met een zachte halo. Uit de brander waaieren drie doorschijnende lichtblauwe bundels als het licht van een vuurtoren: een brede naar links, een naar rechtsboven en een naar rechtsonder. Vijf oker schildjes zweven als wachters rond de vlam, elk met twee of drie woorden. De linkerbundel valt op een hoog wit paneel met zeven deuren onder elkaar: de bovenste drie hebben een dichte rand en een pijl met twee punten, die van de DM in oranje; de onderste vier zijn gestippeld, met een stippelpijl naar de vlam en bij drie ervan een dunne groene pijl terug. Rechtsboven vangt een paneel met drie tegels het licht, de onderste groen omrand omdat daar iets naar buiten gaat; rechtsonder ligt één gestippelde forumtegel waar alleen een pijl vandaan komt. Vanuit de naam onder de vlam zakt een rechte petrol lijn naar een rail met zes tegels: vier wit, één perzikkleurig voor de vraag aan de maintainer en één zachtgroen voor de alpha-zip. Helemaal onderaan dragen drie smalle witte balken met codeletters het geheel, als de fundering onder een gebouw.

**Volledig (route B):**

```text
A light technical infographic poster, 16:9, titled "BlueFlame Bot: the pilot flame". Off-white background (#f4f6f7), white panels with thin light-grey borders (#d5dce2) and soft shadows, dark ink text (#151a1f), flat vector with slight isometric depth, calm and precise, generous spacing.

Title row: the title at top left; at top right a small legend with six marks: "monitoring" (wide pale-blue line), "read and reply" (double-headed petrol arrow), "read only" (dashed petrol arrow), "to the maintainer" (orange arrow), "ships" (green arrow), "ground rule" (ochre shield).

Top band labelled "WATCH LOOPS": three wide white cards with a cobalt outline, each with a circular loop arrow: "/backlog_discord" with a small hourglass and the caption "before every new task"; "GitHub sweep" without an hourglass, caption "before every new task"; "/check_otgw_issues" with a small hourglass and the caption "started by hand".

Centre: a round burner with a pale blue face (#dbeafe) and a cobalt rim, holding a vivid blue flame (cobalt #1d4ed8 at the base fading to cyan #06b6d4 at the tip) with a white core and a soft blue halo, labelled "BlueFlame Bot" with the caption "voice of the coordinator and the comms session". Five small ochre shields (#a15c00) surround the burner: "English, answer first" above, "One token holder" upper left, "No board name" upper right, "Testers advise" lower left, "Record first" lower right. Three translucent pale-blue beams fan out from the flame like a lighthouse, one to the left, one to the upper right, one to the lower right.

Left beam reaches a tall white panel labelled "DISCORD" with seven doors in a column. The top three have a solid petrol outline (#0e7490) and a double-headed arrow to the flame: "#alpha-testing", "#dev-sat-mqtt", and "DM with maintainer" whose arrow is burnt orange (#c2410c). The lower four have a dashed grey outline and a dashed petrol arrow pointing to the flame: "#beta-testing", "#devs-esp-firmware", "#english-support", "#nederlandse-ondersteuning". Three of these four, all except "#devs-esp-firmware", also have a thin forest-green arrow (#2f7d32) pointing back to the door, for announcements and, in practice, replies.

Upper-right beam reaches a white panel labelled "GITHUB" with the caption "under the maintainer account" and three tiles: "Issues" and "Pull requests" with double-headed petrol arrows, "Releases" with a green outline and a single green outgoing arrow.

Lower-right beam reaches a white panel labelled "TWEAKERS" with the caption "RSS, read only": one dashed forum tile with RSS arcs, "Forum topic", caption "Dutch posts, English summary", and one dashed arrow pointing from the tile to the flame, no arrow back.

Below the centre, a band labelled "WHAT IT PRODUCES": one straight petrol line drops from the flame to a horizontal rail feeding six tiles: "Reply", "Backlog task", "Register entry", "Triage list" (white with petrol outlines), "Question to maintainer" (pale peach fill #fde7d9, orange outline, orange arrow), "Alpha zip + report" (pale green fill #e3f1e4, green outline, green arrow).

Bottom band labelled "FOUNDATION": three thin white bars with grey outlines and monospace text: "discord-mcp · supervisor · scheduled task", "watermarks", "gh as maintainer · RSS · build webhook · bot REST".

Typography: Archivo for headings, Source Sans for labels, JetBrains Mono for commands and the foundation bars. Only the flame glows. Palette: background #f4f6f7, cards #ffffff, text #151a1f, orange #c2410c, petrol #0e7490, cobalt #1d4ed8, ochre #a15c00, green #2f7d32.
```

**Zonder tekst (route A):** gebruik dezelfde prompt, vervang elk label tussen aanhalingstekens door "an empty label plate", en voeg toe: `No text anywhere, no letters, no numbers; leave clean empty label plates for overlay.`

**Kort:**

```text
Light off-white infographic, 16:9: a vivid cobalt-to-cyan pilot flame with a white core in a round pale-blue burner at the centre, five small ochre shields around it, three translucent pale-blue lighthouse beams reaching a tall white panel with seven doors on the left (top three solid with double-headed arrows, lower four dashed with dashed arrows toward the flame and thin green arrows back on three of them), a panel with three tiles upper right and a single dashed forum tile with RSS arcs lower right; three white cards with loop arrows across the top, two with a small hourglass; below the flame a rail of six output tiles, one peach and one pale green; three thin white foundation bars along the bottom; flat vector, white cards, soft shadows, precise, calm.
```

Negatief: zoals bij plaat A.

### 4.7 Nog niet op de render

Twee wensen uit `agentic-setup.md` (hoofdstuk 6) staan nog niet op de referentieplaat en zijn optioneel voor een volgende versie. Voeg ze alleen toe als de plaat er rustig bij blijft.

- Twee kleine petrol sessiefiguren achter de vlam met het label "coordinator · comms", om de twee stemmen zichtbaar te maken.
- Een dun rood gestippeld randje om de deur #beta-testing met het label "no watch loop": verzoeken en besluiten komen daar binnen, maar geen lus die vóór elke taak draait leest dat kanaal.

---

## 5. Plaat C: van taak tot tester (banner)

### 5.1 Boodschap

Elke wijziging loopt dezelfde reeks poorten door, en alleen de bench-tester raakt hardware aan.

### 5.2 Formaat en compositie

Breed **3:1** (wireframe niet nodig; de compositie is één lijn). Een horizontale transportband van links naar rechts met elf stations. Petrol stations zijn werk van agents, oker schilden zijn poorten, het oranje station is de mens, groen is publicatie.

| # | Station | Kleur | Poort die kan weigeren |
|---|---|---|---|
| 1 | Backlog-taak | petrol | geen taak, geen werk (commit-msg-hook) |
| 2 | Plan + zelfreview | petrol | advisor of Opus-review |
| 3 | SAT? → OK van de SAT-ontwerper | oranje | geen bouw zonder zijn OK bij regelgedrag |
| 4 | Code rood → groen | petrol | oude code faalt, fix slaagt |
| 5 | Reviewteam | petrol | reviewers moeten weerleggen |
| 6 | evaluate · tests | oker | geen nieuwe FAIL |
| 7 | Bump + commit-hooks | oker | geen nieuwe prerelease-tag, geen taakbestand |
| 8 | Build esp32-combo | oker | één build tegelijk op de machine |
| 9 | Bench via OTA | oker | bench-tester flasht en test |
| 10 | Push `dev` | petrol | pas na het OK van de bench |
| 11 | Post alpha-zip | groen | githash ≠ HEAD, vuile `src/`, tag al gepubliceerd |

Boven de band, op afstand: "Beta 2.x" met een klokje (08:00/20:00, staand mandaat, geen hangslot) en met een steenrood hangslot "Stable 2.x" en "1.x LTS" (`otgw-1.x.x` naar `main`).

### 5.3 Scènebeschrijving en prompt

**Scènebeschrijving (van links naar rechts).** Over de volle breedte van een gebroken witte strook loopt één transportband, dun en strak getekend, van de linkerrand naar de rechterrand. Helemaal links ligt het eerste witte station met een petrol rand, een kanbanbordje: de taak waarmee alles begint. Daarna volgen de stations elkaar op als kralen aan een snoer; het derde valt op door zijn perzikkleurige vlak en oranje rand, want daar beslist een mens. Voorbij het midden staan vier stations met een oker rand en een schildje op een rij, als poortjes waar de band doorheen moet. Bij het negende station staat een kleine werkbank met twee printplaatjes en een WiFi-boog: de enige plek waar hardware in beeld is. Rechts eindigt de band in een zachtgroen station, waar een pakje de strook naar rechts verlaat. Hoog boven de band, met duidelijke afstand, hangen drie tegels: één met een klokje op 08:00 en 20:00 en twee met een steenrood hangslot.

```text
A wide 3:1 light infographic banner: a horizontal conveyor belt from left to right on an off-white background with eleven stations as rounded white chips: petrol-blue chips for agent work, ochre shield chips for gates, one burnt-orange chip for a human approval, a forest-green chip at the end with a small package leaving to the right. A small workbench with two circuit boards and a WiFi arc stands at the ninth station. Above the belt, at a distance, one tile with a small clock and two tiles with brick-red padlocks. Flat vector, precise thin lines, calm editorial style, labels short and legible.
```

---

## 6. Labelsets

De platen gebruiken de Engelse kolom (alle diagrammen zijn Engels). De Nederlandse kolom is alleen een vertaalhulp voor wie de plaat toelicht.

### 6.1 Plaat A

| Knoop | Nederlands | English |
|---|---|---|
| Zonelabels | MENSEN · KANALEN · AGENTLAAG · KENNIS EN REGELS · PIJPLIJN EN POORTEN · EXTERNE KENNIS · NAAR BUITEN | PEOPLE · CHANNELS · AGENT LAYER · KNOWLEDGE & RULES · PIPELINE & GATES · EXTERNAL KNOWLEDGE · WHAT SHIPS |
| Rollen | Eindgebruikers · Maintainer · SAT-ontwerper · Alpha-testers | End users · Maintainer · SAT designer · Alpha testers |
| Kanalen | Tweakers (RSS) · GitHub · Discord-support · DM · #dev-sat-mqtt · #alpha-testing | Tweakers (RSS) · GitHub · Discord support · DM · #dev-sat-mqtt · #alpha-testing |
| Hart | BlueFlame Bot · coördinator en comms-sessie | BlueFlame Bot · coordinator and comms session |
| Subagents | Sonnet · Haiku · Opus · Advisor · Reviewlenzen | Sonnet · Haiku · Opus · Advisor · Review lenses |
| Overige agents | Comms-sessie · Andere agents · Bench-tester | Comms session · Other agents · Bench tester |
| Kennis | Regels · ADR's + ADR-kit · C4-kaart · SAT-register · Open vragen · Backlog · OpenWolf · Codegraaf | Rules · ADRs + ADR kit · C4 map · SAT register · Open questions · Backlog · OpenWolf · Code graph |
| Pijplijn | Taak · Plan + review · Rood → groen · Reviewteam · Poorten · Bump + hooks · Build (lock) · Bench (OTA) · Push + post | Task · Plan + review · Red to green · Review team · Gates · Bump + hooks · Build (lock) · Bench (OTA) · Push + post |
| Externe kennis | OpenTherm-matrix · thermo-nova · OpenTherm-spec · toolchains | OpenTherm matrix · thermo-nova · OpenTherm spec · toolchains |
| Naar buiten | 1.x LTS · Stabiel 2.x · Beta 2.x · Alpha-zip | 1.x LTS · Stable 2.x · Beta 2.x · Alpha zip |
| Legenda | besluit van een mens · werk tussen agents · alleen lezen · gaat naar buiten · poort · harde stop | human decision · work between agents · read only · ships · gate · hard stop |

### 6.2 Plaat B

| Knoop | Nederlands | English |
|---|---|---|
| Zonelabels | BEWAKINGSLUSSEN · DISCORD · GITHUB · TWEAKERS · WAT HIJ OPLEVERT · FUNDAMENT | WATCH LOOPS · DISCORD · GITHUB · TWEAKERS · WHAT IT PRODUCES · FOUNDATION |
| Hart | BlueFlame Bot · stem van de coördinator en de comms-sessie | BlueFlame Bot · voice of the coordinator and the comms session |
| Lussen | /backlog_discord · GitHub-sweep · /check_otgw_issues | /backlog_discord · GitHub sweep · /check_otgw_issues |
| Lus-ondertitels | vóór elke nieuwe taak · vóór elke nieuwe taak · met de hand gestart | before every new task · before every new task · started by hand |
| Discord, lezen en antwoorden | #alpha-testing · #dev-sat-mqtt · DM met de maintainer | #alpha-testing · #dev-sat-mqtt · DM with maintainer |
| Discord, luisterposten | #beta-testing · #devs-esp-firmware · #english-support · #nederlandse-ondersteuning | #beta-testing · #devs-esp-firmware · #english-support · #nederlandse-ondersteuning |
| Ondertitels luisterposten | gelezen · beta-aankondigingen · verzoeken sinds 15 nov / alleen lezen / gelezen · release-post · in de praktijk antwoorden | read · beta announcements · requests since Nov 15 / read only / read · release post · replies in practice |
| GitHub | Issues · Pull requests · Releases · onder het maintainer-account | Issues · Pull requests · Releases · under the maintainer account |
| Tweakers | Forumtopic · alleen lezen · Nederlandse berichten, Engelse samenvatting | Forum topic · read only · Dutch posts, English summary |
| Schilden | Engels, antwoord eerst · Eén tokenhouder · Geen boardnaam · Testers adviseren · Eerst vastleggen | English, answer first · One token holder · No board name · Testers advise · Record first |
| Uitvoer | Antwoord · Backlog-taak · Registerentry · Triagelijst · Vraag aan maintainer · Alpha-zip + verslag | Reply · Backlog task · Register entry · Triage list · Question to maintainer · Alpha zip + report |
| Fundament | discord-mcp · supervisor · geplande taak · watermerken · gh als maintainer · RSS · build-webhook · bot-REST | discord-mcp · supervisor · scheduled task · watermarks · gh as maintainer · RSS · build webhook · bot REST |
| Legenda | bewaken · lezen en antwoorden · alleen lezen · naar de maintainer · gaat naar buiten · spelregel | monitoring · read and reply · read only · to the maintainer · ships · ground rule |

---

## 7. Controlelijst voor het resultaat

Loop deze lijst na voordat je een gegenereerde plaat deelt. Elk punt is een inhoudelijke fout die een generator makkelijk maakt. De lijst is op 2026-11-15 tegen de twee referentieplaten gelegd; waar een referentieplaat zelf nog afwijkt, staat dat erbij.

**Alle platen**

- [ ] Het thema is licht (gebroken witte achtergrond, witte kaarten, donkere tekst) en alleen de vlam heeft een zachte gloed.
- [ ] Alle tekst op de plaat is Engels, leesbaar en correct gespeld; bij twijfel route A (labels erover).
- [ ] Er staat nergens een hardwarenaam, gebruikers-ID, webhook-URL of token in beeld. Wat wel mag: de naam van het builddoel in station 7, de publieke hostnaam van het forum en de letterlijke placeholder `<@ID>`.
- [ ] Geen merklogo's en geen portretten: generieke glyphs en rolnamen.
- [ ] Oranje lijnen beginnen of eindigen altijd bij een mens (op plaat B ook bij de tegel "Question to maintainer"). Geen oranje lijn tussen twee agents.

**Plaat A**

- [ ] De vlam heet "BlueFlame Bot" met de ondertitel "coordinator and comms session", en de kaart "Comms session" hangt met een gestippelde kobaltlijn aan de vlam.
- [ ] Tweakers heeft alleen een gestippelde leeslijn naar de vlam. Discord-support heeft een gestippelde leeslijn en hooguit een dunne groene terugpijl, nooit de pijl met twee punten van de werkkanalen. (De referentieplaat toont hier nog alleen de leeslijn.)
- [ ] GitHub, de DM, #dev-sat-mqtt en #alpha-testing hebben pijlen in beide richtingen.
- [ ] De SAT-ontwerper is verbonden met #alpha-testing en #dev-sat-mqtt, niet met GitHub of Tweakers.
- [ ] De bench-tester is de enige agent die aan station 8 "Bench via OTA" hangt; de vlam en de subagents raken geen hardware aan. Tekent de generator testunits, dan staan die alleen bij de bench-tester.
- [ ] De pijplijn heeft negen stations in de juiste volgorde; de stations 5 tot en met 8 hebben een oker rand.
- [ ] "Stable 2.x" en "1.x LTS" dragen een hangslot; "Beta 2.x" heeft een klokje, geen hangslot.
- [ ] De dikke groene pijl gaat van station 9 naar "Alpha zip", en de groene stippellijn van "Alpha zip" naar #alpha-testing, niet naar GitHub. De beta loopt niet over die pijl.
- [ ] De archiefkast heeft acht laden en de onderband vier externe badges.

**Plaat B**

- [ ] De ondertitel van de vlam noemt beide stemmen: "voice of the coordinator and the comms session".
- [ ] Er zijn drie lussen. `/backlog_discord` en de GitHub-sweep staan op "before every new task"; `/check_otgw_issues` staat op "started by hand", niet op een klok of interval. Alleen de eerste en de derde dragen een zandloper.
- [ ] Discord heeft zeven deuren: drie met een dichte rand en een pijl met twee punten (die van de DM oranje), vier met een gestippelde rand.
- [ ] #beta-testing, #english-support en #nederlandse-ondersteuning hebben een stippelpijl naar de vlam en een dunne groene pijl terug; #devs-esp-firmware heeft alleen de stippelpijl.
- [ ] #beta-testing noemt de verzoeken ("requests since Nov 15"); de twee ondersteuningskanalen noemen "replies in practice".
- [ ] GitHub draagt "under the maintainer account"; "Issues" en "Pull requests" hebben pijlen met twee punten, "Releases" alleen een groene pijl naar buiten.
- [ ] Tweakers heeft alleen een pijl naar de vlam toe, geen pijl terug.
- [ ] Er zijn vijf oker schilden met de juiste tekst, waaronder "No board name" en "One token holder".
- [ ] De uitvoerrail heeft zes tegels; alleen "Question to maintainer" is oranje en alleen "Alpha zip + report" is groen.
- [ ] Het fundament ligt onder de uitvoerrail en noemt de Java-server met supervisor, de watermerken en "gh as maintainer · RSS · build webhook · bot REST". Bot-REST heeft geen eigen pijl naar een kanaal.

**Plaat C**

- [ ] Elf stations van links naar rechts; alleen station 3 is oranje en alleen het laatste groen.
- [ ] Hardware staat alleen bij station 9.
- [ ] Boven de band: "Beta 2.x" met een klokje, "Stable 2.x" en "1.x LTS" met een hangslot.

---

## 8. Deze brief bijwerken

- Verandert de architectuur in `agentic-setup.md`, werk dan eerst de knopen- en verbindingstabellen hier bij, daarna de SVG-wireframes in `images/`, en pas als laatste de prompts.
- Houd de labels kort en in beide talen gelijk.
- `agentic-setup.md` verwijst op een paar plaatsen naar regelnummers van deze brief (de "Voor de plaat"-blokken). Loop die na als hier regels bij komen of wegvallen.
- Docs-only: geen prerelease-bump, wel een `TASK-NNN` in de commit.
