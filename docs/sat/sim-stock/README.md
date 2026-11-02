# Dutch housing stock for the SAT simulations (TASK-1563)

Primary source: RVO, *Voorbeeldwoningen 2022 bestaande bouw* (2023-01), commissioned by the Ministry of the Interior,
made by Stichting W/E adviseurs from WoON 2018 (energy module) and calculated with NTA 8800:

- brochure: https://www.rvo.nl/sites/default/files/2023-01/brochure-voorbeeldwoningen-bestaande-bouw-2022.pdf
- data: https://www.rvo.nl/sites/default/files/2023-01/data-voorbeeldwoningen-2022.xlsx
- method report: https://rvo.nl/sites/default/files/2023-01/verantwoordingsrapportage-voorbeeldwoningen-bestaande-bouw-2022.pdf

51 reference dwellings that together represent the Dutch stock up to build year 2018: nine types (detached,
semi-detached, terraced mid, terraced end, maisonnette, gallery flat, porch flat, other flat) x period classes
(up to 1945/1964, 1946-64, 1965-74, 1975-91, 1992-2005, 2006-14, 2015-18). Each dwelling has five energy levels:
*oorspronkelijk* (as built), *huidig* (current state from WoON 2018, including later insulation), and three
improvement packages (insulation to the national standard with: 1 gas boiler, 2 heat pump, 3 heat pump with balanced
ventilation).

Extraction: `scripts/sat_stock_extract_rvo2022.py` (run with `python -I`, paths as arguments) writes
`rvo_voorbeeldwoningen_2022.json`: per dwelling the floor area, the area and U-value of floor, facade, roof, windows
and doors per level, ventilation, emitter and its temperature level, heater, NTA 8800 heat demand, and from the
brochure text the number of dwellings and the stock share (51 entries, 7.47 million dwellings, 100.2 % by rounding).

## Facts read (not derived)

- Stock shares per type x period: brochure chapters 3-10, one sentence per dwelling
  ("vertegenwoordigen met N woningen, X% van de Nederlandse woningvoorraad"). Largest: terraced mid 1975-91 7.7 %,
  detached up to 1964 5.6 %, terraced mid 1965-74 5.4 %, terraced end 1975-91 4.8 %, terraced mid up to 1945 4.5 %.
- Shares by type (computed from the 51 counts): terraced mid 26.6 %, terraced end 14.3 %, detached 13.9 %,
  porch flat 13.1 %, semi-detached 11.8 %, gallery flat 9.1 %, other flat 5.8 %, maisonnette 5.4 %; apartments
  together 33.4 %. By period: up to 1945 9.9 %, 1946-64 8.3 %, up to 1964 (types without the 1946 split) 14.8 %,
  1965-74 17.0 %, 1975-91 24.9 %, 1992-2005 14.9 %, 2006-14 7.8 %, 2015-18 2.4 %.
- Current state of old homes: e.g. detached up to 1964 (brochure p.25): 95 % HR107 boiler, 2 % heat pump; 13 % of the
  glass still single, 58 % of the closed facade insulated, 47 % of the floor, 70 % of the pitched roof; 90 % natural
  ventilation.
- Emitter in the data: radiators for most dwellings; floor/wall heating (45/40) for several 2006-2018 dwellings.

## Not in this source

- Thermal capacity per dwelling (NTA 8800 has mass classes; the data sheet does not list them).
- Radiator size. The model will size radiators for the *oorspronkelijk* heat loss (see the plan in TASK-1563), which
  makes them oversized after later insulation; that is a modelling choice, not a fact from RVO.
- Mixed emitters (underfloor downstairs, radiators upstairs) and occupant behaviour.
- The H values in the json (`H_trans_simple_w_k`) are a plain sum of U x A without the NTA 8800 reductions for floors
  over a crawl space or ground, and without ventilation; they are an upper bound for the transmission loss.
