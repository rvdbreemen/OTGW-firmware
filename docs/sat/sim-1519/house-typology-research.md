# House typology for the TASK-1519 heating-curve self-tune study

Robert asked (2026-10-31 07:30, #dev-sat-mqtt) for a house model that varies with build quality, following the Dutch
building regulations over the years and the German codes. The aim is to show the "thermal battery" Sergeant D described
(entry 168: walls need about 3 days to charge, system response 60-72 h). This page separates what the sources say from
what we derived. Every derived number is labelled **estimate**.

## 1. What the sources say

### Netherlands: required facade insulation (Rc, m2K/W) by era

| Era | Facade Rc | Source |
|---|---|---|
| 1965-1975 | about 0.43 (practice, no requirement) | Stimular overview; MP article (Dijs, Burgmeijer, Höngens) |
| 1975-1983 | first requirement about 1.3 | same overviews |
| 1983-1992 | 1.3 rising to 2.0 | same overviews |
| 1992 Bouwbesluit | 2.5 (art. 70) | TNO 2023 P12572, table citing Bouwbesluit 1992 |
| 2012 Bouwbesluit | 3.5 | Kamerstuk 32757 / Omgevingsweb (proposal to go to 5 not carried out) |
| 2015 | 4.5 (facade) | KNB infoblad 49 (2017); Stimular; TNO table |
| 2021 BENG | an energy-performance requirement (no single facade Rc); new-build facades about 4.7-5.5 in practice | Isover brochure (EPC to BENG); Dijs estimate 5.5 (estimate, not a legal value) |

Conflicts: the sources differ on the exact years of the 2003-2015 steps; the values above are the common reading.
For legal use, check the regulation text on bouwbesluitonline.nl.

### Germany: regulation chronology and wall U-values

- WSchV 1977 was the first binding minimum; it was revised in 1984 and 1995. EnEV replaced it in February 2002 and was amended in 2004, 2007, 2009 and 2014. GEG replaced EnEV in 2020 (TEAG leaflet; Glaswelt; Baunetz Wissen).
- Wall U-values by era, rough guide values only (W/m2K): pre-war 1.5-2.0; 1950-1970 1.2-1.5; 1970-1995 0.6-1.0; EnEV 2002 about 0.35; GEG new-build about 0.24 (reduco guide; a forum summary says 0.23 under GEG Anlage 1). **Not confirmed in the regulation texts.** The IWU Materialband for the EnEV 2009 evaluation is the solid source for model buildings and was not read in full.

### Thermal mass classes

- ISO 13790 effective heat capacity per m2 floor area: light 110, medium 165, heavy 260 kJ/m2K. Confirmed in the LBL/OpenModelica ISO 13790 data records.
- The "very light 80" and "very heavy 370" classes were **not confirmed**. A UK guidance page places the very-light to very-heavy classes in EN ISO 52016-1:2017 Table B.14, which was not read.

### Typologies

TABULA/EPISCOPE has a Dutch building-type matrix (terraced, single-family, multi-family, apartment block, by year class) and a German one (IWU). The per-period U-value tables are in the EPISCOPE Netherlands chapter (TU Delft repository) and the TABULA WebTool. **They were not opened**, so the heat-loss coefficients below are derived, not taken from TABULA.

## 2. Derived model houses (estimates)

Reference: a Dutch terraced house, 120 m2 floor, 300 m3 volume, facade (front + back) 60 m2 opaque, roof 60 m2,
ground floor 60 m2, windows 25 m2. H = sum(U x A) + 0.34 x n x V. The U-values come from the Rc above (U ~ 1/(Rc + 0.17)).
Glazing, ventilation and roof/floor values are **estimates** that are typical for the era.

| # | Era (NL / DE analogue) | U wall / roof / floor / window | n (1/h, effective) | H (W/K) | C (heavy, MJ/K) | Opaque G_wo (W/K) | Wall time constant (h) |
|---|---|---|---|---|---|---|---|
| 1 | pre-1965 / pre-WSchV | 2.0 / 2.5 / 2.5 / 4.0 | 1.0 | ~620 | 31 | ~420 | ~18 |
| 2 | 1965-1975 / pre-WSchV | 1.7 / 1.5 / 1.5 / 3.0 | 0.8 | ~440 | 31 | ~280 | ~28 |
| 3 | 1983-1992 / WSchV 1984 | 0.6 / 0.6 / 0.7 / 2.9 | 0.6 | ~250 | 31 | ~115 | ~65 |
| 4 | 1992-2012 / WSchV 1995 | 0.37 / 0.37 / 0.37 / 1.8 | 0.5 | ~160 | 31 | ~66 | ~115 |
| 5 | 2015 / EnEV 2014 | 0.21 / 0.16 / 0.28 / 1.4 | 0.2 (heat recovery) | ~95 | 31 | ~40 | ~190 |
| 6 | 2021 BENG / GEG | 0.21 / 0.16 / 0.27 / 1.0 | 0.15 (heat recovery) | ~80 | 31 | ~39 | ~200 |

Light timber-frame variants use 110 kJ/m2K (C ~13 MJ/K), which gives about 40 % of the time constants above.
The fitted field house (ratio tau ~110 h, section 4) sits in era 4. Sergeant D's 60-72 h response matches eras 3-4.

## 3. Two-node model (replaces the one-mass model)

- **Air node** (C_air): room air, furniture and inner leaf surface, about 10 % of C (**estimate**). It reacts in minutes to hours.
- **Wall node** (C_wall): the rest of the structure, which is the thermal battery.
- **Couplings:**
  - G_aw air to wall: interior surface about 3 x floor area x 8 W/m2K, capped by the wall's own conduction (**estimate**).
  - G_wo wall to outside: the opaque losses.
  - G_ao air to outside: windows plus ventilation.
- **Heat sources:** radiator heat and internal gains go to the air node; solar gain is split 50/50 over air and wall (**estimate**).

The radiator constant K is set per house so that the best curve coefficient is about 1.6-2.4 (Sergeant D entry 166). Most
Dutch homes run with radiators that suit that range, often after insulation upgrades. Houses with original high-temperature
radiators (coefficient about 3) are kept as edge cases.

## 4. Validation reference

A field graph capture (4 h, 2,870 samples of flow, return, room and outside temperature) was fitted with the one-mass model.
The fit identifies the ratios K/H ~ 1 and tau ~ 110 h, with RMSE 0.033 C on the fit half and 0.069 C on the held-out half.
The absolute scale is not identifiable from 4 h. **A 24-72 h capture of a real house is needed to validate the two-node
model**: the device keeps a 24 h graph history (ADR-193), so one export from a tester would do.

## Sources

- Stimular, isolatiemaatregelen: https://stimular.nl/maatregelen/grootschalige-isolatiemaatregelen
- MP, Dijs/Burgmeijer/Höngens: https://mp.nl/sites/default/files/2022-11/GEL02-art%20Dijs%20Burgmeijer%20H%C3%B6ngens.pdf
- TNO 2023 P12572: https://publications.tno.nl/publication/34642039/rdc0GA/TNO-2023-P12572.pdf
- Kamerstuk 32757-35: https://zoek.officielebekendmakingen.nl/kst-32757-35.pdf
- Omgevingsweb, Rc-verhoging naar 5 niet uitgevoerd: https://omgevingsweb.nl/nieuws/uitvoering-moties-bouwbesluit-2012-rc-verhoging-naar-5-wordt/
- KNB infoblad 49: https://www.knb-keramiek.nl/media/243791/knb_infoblad_49_het_isoleren_van_de_spouwmuur__2017-11-17_.pdf
- Isover brochure: https://www.isover.nl/documenten/brochures/rkl-31-faade-brochure-grijze-bekleding.pdf
- TEAG Merkblatt Wärmeschutzverordnung: https://thueringerenergie.de/Content/Documents/TEAG_Merkblatt_Waermeschutzverordnung.pdf
- Baunetz Wissen, Entwicklung der Regelwerke: https://www.baunetzwissen.de/nachhaltig-bauen/fachwissen/regelwerke/entwicklung-der-regelwerke-zur-energieeinsparung-674672
- IWU EnEV 2009 Materialband: https://www.iwu.de/fileadmin/publikationen/energie/2011_IWU_EnselingEtAl_Materialband-zum-Endbericht-Evaluierung-und-Fortentwicklung-der-EnEV-2009.pdf
- reduco U-Wert guide (secondary): https://reduco.ai/blog/u-wert-erklaerung
- LBL Modelica ISO 13790 data: https://simulationresearch.lbl.gov/modelica/releases/latest/help/Buildings_ThermalZones_ISO13790_Data.html
- UK mass distribution guidance (EN ISO 52016-1 Table B.14): https://energy-calculator-integration.digital.communities.gov.uk/guidance/mass-distribution-guidance
- IWU TABULA: https://www.iwu.de/research/gebaeudebestand/tabula-en/?L=1
