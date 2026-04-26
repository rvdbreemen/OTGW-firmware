# OTGW Design System — package

Een complete, self-contained design system voor de OTGW-firmware web UI.
Eén tokens-bestand, één componentbibliotheek, een referentiepagina, vijf
page-templates, en een Windows-script dat het in je repo dropt.

```
otgw_design_package/
├── README.md                         <-- je leest dit
├── handoff.md                        <-- voor Claude Code
├── SKILL.md                          <-- herbruikbare skill
├── apply.ps1                         <-- Windows installer (PowerShell)
├── apply.bat                         <-- dubbelklik wrapper
│
├── data/                             <-- gaat 1-op-1 naar src/OTGW-firmware/data/
│   ├── ds-tokens.css                 <-- alle tokens (color/type/space/radius/shadow/motion)
│   ├── components.css                <-- volledige componentbibliotheek
│   ├── theme-toggle.js               <-- ☀/☾, persist, dispatch theme:changed
│   ├── sat-slider.js                 <-- live-fill voor .ds-slider
│   ├── echarts-theme.js              <-- otgwChartTheme() leest de tokens
│   ├── design.html                   <-- /design.html — visuele regressie
│   └── fonts/                        <-- Inter 400/700 + JetBrains Mono 400 (~68 KB)
│
└── templates/                        <-- patch-blokken voor bestaande HTML
    ├── index.html.template
    ├── sat.html.template             <-- SAT panel + sat.js edits
    ├── settings.html.template
    ├── log.html.template
    └── FSexplorer.html.template
```

## Quick start (Windows)

1. Pak het zip uit naast je OTGW-firmware checkout.
2. Dubbelklik **`apply.bat`** — vraagt om het pad en draait de PS1 met
   `-ExecutionPolicy Bypass`.
3. Of in PowerShell direct:
   ```powershell
   .\apply.ps1 -RepoPath C:\dev\OTGW-firmware
   ```
4. Open `handoff.md` en geef het aan Claude Code voor de markup edits +
   commits.
5. Build LittleFS via PlatformIO en flash. Bezoek `/design.html` op het
   device als visuele regressie-check.

## Wat het script wel/niet doet

| Wel | Niet |
|---|---|
| Backup van `data/` als timestamped zip in `backups/` | HTML/JS markup aanpassen |
| Bestanden kopiëren naar `src/OTGW-firmware/data/` | Find/replace op bestaande hex codes |
| Per-file diff (NEW vs OVERWRITE) tonen | Git operations |
| Interactief y/n/q per file (default), `-Force` om door | LittleFS bouwen of flashen |
| `-WhatIf` voor dry-run | |

De markup edits zijn klein, gemarkeerd met `DS:…:BEGIN/END` sentinels in
de templates. Claude Code doet dat in de handoff-flow.

## Apply order

1. **A. Wire up** — link `ds-tokens.css` + `components.css` in
   `index.html`, voeg de drie JS-tags toe. Oude stylesheets blijven nog
   staan (worden geschaduwd).
2. **B. SAT panel** — vervang het SAT-paneel met `sat.html.template`.
   Twee kleine JS-edits in `sat.js`.
3. **C. Log viewer** — markup is al compatibel; alleen reviewen.
4. **D. Settings + FSexplorer** — markup compatibel; optionele
   verbeteringen in de templates.
5. **E. Drop legacy** — verwijder oude `index.css`, `index_dark.css`,
   `FSexplorer*.css`. Filesystem image krimpt.

Volledige checklist per stap: zie `handoff.md`.

## Smoke test

Na flashen, bezoek het device:

```
http://<device>/design.html         <-- alle componenten in light + dark
http://<device>/                    <-- home: cards neutraal, geen blauwe tint
http://<device>/sat                 <-- target = enige blauwe accent, slider live
```

DevTools console: geen 404s, geen CSS warnings. Theme toggle wisselt
elke pagina volledig (alle componenten gebruiken tokens).

## Rollback

```powershell
# Restore de backup zip die apply.ps1 maakte
Expand-Archive backups\data-backup-<stamp>.zip -DestinationPath C:\dev\OTGW-firmware\src\OTGW-firmware\data -Force
```

Of, als je nog niet gecommit hebt:

```sh
git checkout HEAD -- src/OTGW-firmware/data
```

## Tokens aanpassen

Eén bron van waarheid: `data/ds-tokens.css`. Pak je een hex of size aan —
elk component, elke pagina, en de chart volgen vanzelf.

## Niet inbegrepen

- LittleFS image bouwen — gebruik je bestaande PlatformIO flow.
- Functionele wijzigingen aan `graph.js` / OpenTherm protocol layer.
- Firmware-versie bump.
- Linux/macOS scripts (alleen Windows; PS1 draait wel op PS Core 7
  cross-platform als je hand-rolt).
