# OpenTherm Brand ID's

Verzameling van OpenTherm MemberID-codes per fabrikant, voor gebruik in het [OTGW-firmware](https://github.com/rvdbreemen/OTGW-firmware) project.

## Bestanden

| Bestand | Beschrijving |
|---------|-------------|
| `opentherm_brand_ids.json` | Volledige dataset met alle bekende MemberIDs, modellen, bronvermelding en confidence-levels. Bevat zowel slave (ketel/warmtepomp) als master (thermostaat) IDs. |
| `opentherm_member_ids_simple.json` | Vereenvoudigde lookup-tabel: alleen MemberID → fabrikant(en). Geschikt voor directe integratie in firmware. |
| `boilers.txt` | Ruwe data van Schelte Bron's OTGW database — ketels/warmtepompen. Formaat: `naam\|msgid 3 LB (Slave MemberID)\|msgid 127 (product version)` |
| `thermo.txt` | Ruwe data van Schelte Bron's OTGW database — thermostaten. Formaat: `naam\|msgid 2 LB (Master MemberID)\|msgid 126 (product version)` |

## Protocol context

- **Slave MemberID**: msgid 3 low byte — identificeert de ketel/warmtepomp
- **Master MemberID**: msgid 2 low byte — identificeert de thermostaat/regelaar
- **Bereik**: 0-255 (u8), waarbij 0 = generiek/niet-specifiek
- **IDs 128-255**: gereserveerd voor fabrikant-specifieke Test & Diagnostics

## Bronnen

- Schelte Bron's OTGW thermostat & boiler database (otgw.tclcode.com)
- OpenTherm Association ledenlijst (opentherm.eu/members/)
- SAT Boiler Compatibility Discussion (GitHub)
- OTGateway Compatibility Wiki (GitHub)
- OTGW-firmware Issue #10
- OpenTherm Protocol Specification v2.2

## Status

- **33 unieke MemberIDs** geïdentificeerd (25 slave, 19 master)
- **Laatste update**: 2026-04-05