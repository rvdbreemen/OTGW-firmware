# Antwoord op de Vraag: Labels Aanpassen in Dashboard

## Vraag (Nederlands)
> De labels die aangepast moeten worden. Hoe pas ik die aan? Waar moet ik klikken? En is dat te doen door de panels in het dashboard van no edit naar edit input veld om te zetten?

## Antwoord

Ja, dit is nu mogelijk! Ik heb een nieuwe functie geïmplementeerd waarmee je labels in het dashboard kunt aanpassen door er simpelweg op te klikken.

### Hoe Werkt Het?

#### Stap 1: Ga naar de Dashboard
Open de OTGW web interface en ga naar de Home pagina waar je de OpenTherm monitor data ziet.

#### Stap 2: Vind een Aanpasbaar Veld
Velden die je kunt aanpassen herken je aan:
- Een hand-cursor wanneer je er met de muis overheen gaat
- Een gestippelde onderlijn (in het blauw) bij hoveren
- Een tooltip die zegt "Click to edit label"

**Voorbeeld van velden die je kunt aanpassen:**
- "Boiler Temperature" → kun je wijzigen naar "Ketel Temperatuur"
- "Room Temperature" → kun je wijzigen naar "Kamer Temperatuur"  
- "Water Pressure" → kun je wijzigen naar "Water Druk"
- En alle andere OpenTherm velden

#### Stap 3: Klik op het Label
Klik gewoon op het label dat je wilt aanpassen. Er verschijnt een popup venster.

#### Stap 4: Voer je Gewenste Label In
In het popup venster zie je:
- **Field**: De interne veldnaam (bijvoorbeeld `boilertemperature`)
- **Default Label**: Het originele label (bijvoorbeeld `Boiler Temperature`)
- **Custom Label**: Een tekstveld waar je jouw eigen label kunt invullen (maximaal 50 tekens)

Voer je gewenste tekst in, bijvoorbeeld "Ketel Temperatuur".

#### Stap 5: Opslaan
- Druk op **Enter** of klik op de **Save** knop om de wijziging op te slaan
- Klik op **Cancel** of druk op **Escape** om te annuleren zonder op te slaan
- Klik op **Reset to Default** om terug te keren naar het originele label

### Voorbeelden

#### Voorbeeld 1: Nederlands Dashboard
Je kunt alle velden vertalen naar het Nederlands:

**Voor:**
```
Flame status              On
Boiler Temperature        65.5  °C
Water Pressure            1.5   bar
Modulation Level          45    %
```

**Na aanpassing:**
```
Vlam status               Aan
Ketel Temperatuur         65.5  °C
Water Druk                1.5   bar
Modulatie Niveau          45    %
```

#### Voorbeeld 2: Kortere Namen
Je kunt ook langere namen inkorten:

**Voor:**
```
Domestic Hotwater Temperature    55.0  °C
Relative Modulation Level        45    %
```

**Na aanpassing:**
```
Warmwater Temp                   55.0  °C
Modulatie                        45    %
```

### Belangrijke Punten

✅ **Permanent Opgeslagen**: Je wijzigingen worden opgeslagen op het apparaat en blijven behouden na een herstart

✅ **Onbeperkt Aanpasbaar**: Je kunt zoveel velden aanpassen als je wilt

✅ **Eenvoudig Terugzetten**: Met de "Reset to Default" knop zet je elk veld terug naar het origineel

✅ **Geen Programmering Nodig**: Alles werkt via simpele klikken in de web interface

### Dallas Temperatuur Sensoren

**Let op:** Dallas temperatuur sensoren (de lange hexadecimale adressen zoals `28A1B2C3D4E5F607`) gebruiken een apart systeem dat al bestond:
- Maximaal 16 tekens per label
- Eigen popup venster
- Werkt op dezelfde manier (klik om te wijzigen)

### Technische Details

Als je meer wilt weten over de technische achtergrond:
- Zie `docs/EDITABLE_LABELS_GUIDE.md` voor een volledige handleiding (Engels)
- Zie `docs/EDITABLE_LABELS_VISUAL_GUIDE.md` voor visuele voorbeelden
- REST API beschikbaar op `/api/v1/labels/custom`

### Vragen?

Als je problemen ondervindt:
1. Probeer de pagina te verversen (F5)
2. Controleer of het veld wel klikbaar is (cursor verandert in een hand)
3. Kijk in de browser console (F12) voor foutmeldingen

## Samenvatting

**Je vraag was:** "Hoe pas ik labels aan? Waar moet ik klikken?"

**Antwoord:** Klik gewoon op een label in het dashboard. Als het aanpasbaar is, verschijnt er een popup waar je een nieuwe naam kunt invoeren. Simpel en intuïtief - geen complexe instellingen of edit-modes nodig!
