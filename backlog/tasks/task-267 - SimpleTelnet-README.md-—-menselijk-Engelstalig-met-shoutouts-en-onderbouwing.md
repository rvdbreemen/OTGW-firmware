---
id: TASK-267
title: 'SimpleTelnet: README.md — menselijk Engelstalig met shoutouts en onderbouwing'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:11'
updated_date: '2026-04-12 20:51'
labels:
  - telnet
  - docs
  - publication
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Schrijf de README.md voor de SimpleTelnet library. De tekst moet menselijk en toegankelijk zijn — niet robot-achtig, niet droog technisch. Schrijf alsof een ervaren developer enthousiast vertelt waarom hij dit gebouwd heeft.\n\nInhoud en structuur:\n\n## SimpleTelnet\nKorte, krachtige intro: wat het is, voor wie, in één alinea.\n\n## Why SimpleTelnet?\nEerlijke uitleg waarom een clean implementation de juiste keuze was. Geen neerbuigendheid naar bestaande libraries — juist respect. Uitleggen:\n- TelnetStream: geweldig voor multi-client broadcast, maar geen callbacks\n- ESPTelnet: geweldig voor CLI met callbacks, maar single-client en String-heavy\n- ESPTelnetStream: al een hybride poging, maar fundamenteel single-client\n- De keuze: fork analyseren bleek ~270 regels transformeren met template-complexiteit en permanent fork-onderhoud. Clean sheet: 365 regels met volledige controle, geen String, multi-client by design.\n- Vermelding dat ESPTelnet en TelnetStream de directe inspiratie zijn — zonder hen was dit niet ontstaan.\n\n## Shoutout sectie\nExpliciete credits:\n- Lennart Hennigs (ESPTelnet) — voor de callback-architectuur en keep-alive aanpak\n- Juraj Andrassy (TelnetStream) — voor de elegante server.write() broadcast aanpak\n\n## Features\nBullet list van wat SimpleTelnet biedt dat de anderen niet hebben.\n\n## Installation\nArduino Library Manager én PlatformIO instructies.\n\n## Quick Start\nTwee korte code-blokken: streaming mode en CLI mode.\n\n## API Reference\nLink naar API.md.\n\n## License\nMIT.\n\nToon-richtlijnen voor de tekst:\n- Schrijf als een developer die een probleem oploste en het wil delen\n- Geen marketing-taal, geen overdrijving\n- Gebruik humor spaarzaam maar niet nul (dit is een hobbyproject van een hacker)\n- Eerlijk over wat het WEL en NIET kan\n- Shoutouts zijn oprecht, niet pro forma
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README.md bevat een Why-sectie die eerlijk uitlegt waarom clean sheet beter was dan fork
- [x] #2 Expliciete shoutout naar Lennart Hennigs (ESPTelnet) met GitHub-link
- [x] #3 Expliciete shoutout naar Juraj Andrassy (TelnetStream) met GitHub-link
- [x] #4 Features-sectie toont concreet verschil met bestaande libraries (multi-client, no-String, printf_P)
- [x] #5 Installation-sectie beschrijft zowel Arduino Library Manager als PlatformIO install
- [x] #6 Twee Quick Start code-blokken: streaming mode en CLI mode, beide compileerbaar
- [x] #7 Toon is menselijk en toegankelijk — niet droog technisch, niet overdreven marketing
- [x] #8 Tekst bevat geen em-dashes (—) als interpunctie binnen zinnen
- [x] #9 README.md volledig in het Engels
- [x] #10 Why section honestly describes the fork analysis: ~270 lines of risky template transformation vs ~365 lines of clean new code
- [x] #11 Shoutout credits Lennart Hennigs ESPTelnet for callback architecture, keep-alive, and emptyClientStream pattern
- [x] #12 Shoutout credits Juraj Andrassy TelnetStream for server.write() broadcast insight and polling read pattern
- [x] #13 Features table compares SimpleTelnet vs ESPTelnet vs ESPTelnetStream vs TelnetStream side by side
- [x] #14 Breaking change section clearly states: callbacks now take const char* instead of String — with before/after code snippet
- [x] #15 Quick Start section shows both SimpleTelnet<4> streaming and SimpleTelnet<1> CLI patterns with minimal working code
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wrote complete README.md for SimpleTelnet library, replacing the stub.

Changes:
- Added a strong opening paragraph: what SimpleTelnet is, who it's for, why it exists.
- Added a detailed "Why SimpleTelnet?" section covering the honest fork-vs-clean-sheet analysis: TelnetStream's hidden global and lack of callbacks, ESPTelnet's single-client limitation and String callbacks, ESPTelnetStream as a prior hybrid attempt, the ~270-line fork transformation cost, and the clean-sheet outcome at ~365 lines.
- Tied the origin story to the concrete OTGW firmware heap regression from three simultaneous WiFiServer instances.
- Added warm, specific Credits section for Lennart Hennigs (ESPTelnet: callback architecture, keepalive design, emptyClientStream pattern) and Juraj Andrassy (TelnetStream: broadcast write insight, polling read pattern).
- Added feature comparison table with five columns: TelnetStream, ESPTelnet, ESPTelnetStream, SimpleTelnet; rows cover multi-client, streaming, CLI, callbacks, callback type, no hidden globals, printf_P, platform support, and RAM design.
- Added Installation section (Library Manager, PlatformIO, manual).
- Added two Quick Start examples: streaming mode (SimpleTelnet<4>) and CLI mode (SimpleTelnet<1>) with printf_P callback.
- Added Breaking Change section with before/after snippets for ESPTelnet migrants: const char* vs String callbacks, constructor port argument.
- Added API Reference section with key diffs from ESPTelnet and TelnetStream.
- Added Memory Footprint table (~489B for <1>, ~1033B for <4>) with note on shared input buffer design.
- Added MIT license line.

Tone: direct, experienced developer voice, dry wit, no em dashes, no marketing language, honest about why the existing libraries are good but weren't enough.
<!-- SECTION:FINAL_SUMMARY:END -->
