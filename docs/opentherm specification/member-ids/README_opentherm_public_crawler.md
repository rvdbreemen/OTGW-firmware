# OpenTherm publieke crawler

Bestanden:
- `opentherm_public_crawler.py`: crawler/script
- outputvoorbeeld: `opentherm_public.json`

## Snel starten

```bash
pip install requests beautifulsoup4
python opentherm_public_crawler.py --output opentherm_public.json --verbose
```

## Wat dit script doet

Het script crawlt uitsluitend publiek toegankelijke pagina's van `opentherm.eu` en probeert deze categorieën te extraheren:

- leden / vendors
- documentpagina's en PDF-links
- `/ids/...` archieven met Data-ID / producthints
- productpagina's met extra data-id-links

## Belangrijke nuance

Voor firmware moet je **member/manufacturer ID** leidend houden voor merkidentificatie. De `data_id_brand_hints` in de JSON zijn aanvullend en vaak heuristisch.

## Handige firmware-strategie

1. Lees formele manufacturer/member ID uit OpenTherm.
2. Gebruik deze als primaire merkdetectie.
3. Gebruik `data_id_brand_hints_index` alleen als extra fingerprintlaag.
