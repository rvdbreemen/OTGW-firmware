# OTGW v2 Web UI: terminology glossary (nl, de, el)

Binding for the three translators of `v2.html` / `v2.js`. One English term = one target term,
everywhere in the UI. If a string needs a term that is not here, pick the natural hobbyist word,
use it consistently, and list it in your hand-off notes so the other languages can align.

Audience: home-automation hobbyists, not heating engineers. Prefer the word a user knows from
their boiler display, thermostat app or Home Assistant over the textbook word.

Sources used: the English strings in `base/v2.html` and `base/v2.js`, and the Greek OTGW site
page `otgw-site/el/index.html` (Greek terms marked "site" are taken from that page, so the UI
matches it). The other choices are my judgement of common usage, not checked against a
corpus; the notes say where a choice is a judgement call.

---

## 1. Never translate (keep exactly as in English)

- Product and protocol names: OTGW, OTGW firmware, OpenTherm, OT, OT-Direct, SAT,
  Smart Autotune Thermostat, Home Assistant, MQTT, PIC, BLE, Bluetooth, Dallas, 1-Wire,
  DS18B20, Wi-Fi (exception: de writes "WLAN" in translated text), LittleFS, OTA, REST, API, JSON, CSV, PNG, NTP, Open-Meteo, OTmonitor,
  FSexplorer, Schelte Bron, PID, PWM, COP.
- OpenTherm MsgID names and numbers, protocol commands (`TT=`, `CS=`, `GW=`, `HW=`, `PR=`,
  `PS=1`, `KI`, `UI`, `CM`, `CR`, `RM`, `SR` ...) and the protocol roles **master / slave**.
  In "KI · Mark known ID" translate only the text after the dot; the code stays.
- Units: °C, bar, %, kW, l/min, min, s, h. Abbreviations like `ΔT` stay.
- Anything inside `{{...}}`, HTML tags and attributes, CSS classes, ids, JSON keys, URLs and
  paths (`/api/v2/debug`, `scripts/diag_decode.py`).
- Runtime text from the device (setting labels from JSON, status words, PIC/OT messages,
  error strings): it is not in the translation files.

---

## 2. Glossary

`(short)` = form for tight labels, buttons, chart legends, column heads.

### 2.1 Heating system

| # | English | Nederlands (nl) | Deutsch (de) | Ελληνικά (el) | Note |
|---|---|---|---|---|---|
| 1 | boiler | cv-ketel (short: ketel) | Heizkessel (short: Kessel) | λέβητας (site) | DE: "Therme" is regional (gas wall boilers, AT); use Kessel. NL works for NL and BE. |
| 2 | central heating (CH) | centrale verwarming, cv | Heizung | κεντρική θέρμανση (short: θέρμανση) | NL "cv" lowercase mid-sentence, "Cv" at sentence start. |
| 3 | heating (on/active) | verwarming / verwarmen | Heizen / Heizbetrieb | θέρμανση | "Heating" as a mode = NL "verwarmen", DE "Heizbetrieb". |
| 4 | space heating | ruimteverwarming | Raumheizung | θέρμανση χώρου | |
| 5 | hot water, DHW | warm tapwater (short: tapwater) | Warmwasser (short: WW) | ζεστό νερό χρήσης (site) (short: ΖΝΧ) | Translate the abbreviation DHW too; keep the command `HW=`. |
| 6 | DHW setpoint | tapwatertemperatuur | Warmwasser-Solltemperatur (short: WW-Soll) | θερμοκρασία ΖΝΧ | |
| 7 | flow temperature | aanvoertemperatuur (short: aanvoer) | Vorlauftemperatur (short: Vorlauf) | θερμοκρασία προσαγωγής (site) (short: προσαγωγή) | "FLOW →" diagram label = AANVOER / VORLAUF / ΠΡΟΣΑΓΩΓΗ. |
| 8 | return temperature | retourtemperatuur (short: retour) | Rücklauftemperatur (short: Rücklauf) | θερμοκρασία επιστροφής (site) (short: επιστροφή) | |
| 9 | ΔT (flow minus return) | ΔT | ΔT (Spreizung in prose) | ΔT | |
| 10 | setpoint (generic) | setpoint | Sollwert | θερμοκρασία-στόχος (site) (short: στόχος) | NL hobbyists say "setpoint"; "gewenste temperatuur" in plain sentences is fine. |
| 11 | room setpoint | gewenste kamertemperatuur (short: kamer-setpoint) | Raum-Solltemperatur (short: Raum-Soll) | θερμοκρασία-στόχος χώρου | |
| 12 | control setpoint / boiler setpoint | gevraagde aanvoertemperatuur (short: ketel-setpoint) | Vorlauf-Sollwert (short: Kessel-Soll) | στόχος προσαγωγής | This is the flow temperature asked of the boiler (`CS=`), not the room. |
| 13 | max flow setpoint | max. aanvoertemperatuur | max. Vorlauftemperatur | μέγ. θερμοκρασία προσαγωγής | |
| 14 | target | doel | Ziel | στόχος | "target —" (no value) = "doel —". |
| 15 | room temperature | kamertemperatuur (short: kamer) | Raumtemperatur (short: Raum) | θερμοκρασία χώρου (site) (short: χώρος) | |
| 16 | outside temperature | buitentemperatuur (short: buiten) | Außentemperatur (short: außen) | εξωτερική θερμοκρασία (short: εξωτ.) | |
| 17 | modulation | modulatie | Modulation | διαμόρφωση (site) | |
| 18 | relative modulation level | modulatiegraad | Modulationsgrad | βαθμός διαμόρφωσης | "Max relative modulation" = max. modulatiegraad / max. Modulationsgrad / μέγ. βαθμός διαμόρφωσης. |
| 19 | burner | brander | Brenner | καυστήρας (site) | |
| 20 | flame | vlam | Flamme | φλόγα (site) | Status "flame on" = vlam aan / Flamme an / φλόγα αναμμένη. |
| 21 | burner cycle / cycle | branderstart, cyclus | Brennertakt, Takt | κύκλος καυστήρα (site), κύκλος | "Cycles this hour" = cycli dit uur / Takte in dieser Stunde / κύκλοι αυτή την ώρα. |
| 22 | short cycling / anti-cycling | pendelen / anti-pendel | Takten / Taktsperre | συχνές εκκινήσεις / προστασία από συχνές εκκινήσεις | |
| 23 | duty cycle | inschakelduur | Einschaltdauer | ποσοστό λειτουργίας | |
| 24 | pump | pomp (cv-pomp) | Pumpe (Heizungspumpe) | κυκλοφορητής | EL: heating trade word is κυκλοφορητής, not αντλία. |
| 25 | water pressure, pressure | waterdruk, druk | Wasserdruck, Druck | πίεση νερού (site), πίεση | |
| 26 | fault | storing | Störung | σφάλμα (site) | Fault code = storingscode / Störungscode / κωδικός σφάλματος. |
| 27 | thermostat | thermostaat | Thermostat | θερμοστάτης (site) | |
| 28 | heating curve | stooklijn | Heizkurve | καμπύλη θέρμανσης | |
| 29 | curve slope | helling (van de stooklijn) | Steilheit (der Heizkurve) | κλίση (καμπύλης) | |
| 30 | weather-compensated | weersafhankelijk | witterungsgeführt | με αντιστάθμιση εξωτερικής θερμοκρασίας | |
| 31 | heat pump | warmtepomp | Wärmepumpe | αντλία θερμότητας (site) | |
| 32 | cooling | koeling | Kühlung | ψύξη | |
| 33 | defrost | ontdooien | Abtauen | απόψυξη | |
| 34 | compressor | compressor | Verdichter | συμπιεστής | |
| 35 | appliance (type) | toestel(type) | Wärmeerzeuger (Typ) | τύπος συσκευής | "Gas boiler" = gasketel / Gaskessel / λέβητας αερίου. |
| 36 | emitters / radiators / underfloor | afgiftesysteem / radiatoren / vloerverwarming | Heizflächen / Heizkörper / Fußbodenheizung | τερματικές μονάδες / καλοριφέρ (σώματα) / ενδοδαπέδια θέρμανση | |
| 37 | preheat | voorverwarmen | Vorheizen | προθέρμανση (site) | |
| 38 | frost protection | vorstbeveiliging | Frostschutz | αντιπαγετική προστασία | |
| 39 | presets: Comfort / Eco / Home / Away / Sleep | Comfort / Eco / Thuis / Afwezig / Slapen | Komfort / Eco / Zuhause / Abwesend / Schlafen | Άνεση / Eco / Σπίτι / Απουσία / Ύπνος | Only if these appear as UI text, not as runtime values. |
| 40 | standby | stand-by | Standby | αναμονή | |
| 41 | idle | in rust | Leerlauf | αδράνεια | "Heating idle · pump running" = Verwarming in rust · pomp draait / Heizung im Leerlauf · Pumpe läuft / Θέρμανση σε αδράνεια · ο κυκλοφορητής λειτουργεί. |
| 42 | override (gateway override) | override (plural: overrides) | Übersteuerung | παράκαμψη | A value the gateway imposes instead of the thermostat's. "Active overrides" = Actieve overrides / Aktive Übersteuerungen / Ενεργές παρακάμψεις. NL: not "overschrijving" (reads as a bank transfer, especially in BE). |
| 43 | control loop / controller | regellus / regelaar | Regelkreis / Regler | βρόχος ελέγχου / ελεγκτής | "PID controller" = PID-regelaar / PID-Regler / ελεγκτής PID. |
| 44 | gain (PID gains) | versterking | Verstärkung | κέρδος | |
| 45 | self-tune / autotune | zelf inregelen | Selbstoptimierung | αυτόματη ρύθμιση | |
| 46 | calibration (markers) | kalibratie(markeringen) | Kalibrierung (Markierungen) | βαθμονόμηση (σημάδια) | |

### 2.2 Gateway, connections, devices

| # | English | Nederlands (nl) | Deutsch (de) | Ελληνικά (el) | Note |
|---|---|---|---|---|---|
| 47 | gateway | gateway | Gateway | gateway (site, neuter: το gateway) | |
| 48 | device | apparaat | Gerät | συσκευή (site) | |
| 49 | sensor | sensor | Sensor (temperature: Fühler is fine in prose) | αισθητήρας (site) | "External sensors" = externe sensoren / externe Sensoren / εξωτερικοί αισθητήρες. |
| 50 | room sensor (BLE) | ruimtesensor | Raumsensor | αισθητήρας χώρου | |
| 51 | connected / disconnected | verbonden / niet verbonden | verbunden / getrennt | συνδεδεμένο / αποσυνδεδεμένο | |
| 52 | connection | verbinding | Verbindung | σύνδεση | |
| 53 | link up / link down | verbinding actief / verbinding weg | Verbindung steht / Verbindung unterbrochen | σύνδεση ενεργή / σύνδεση διακόπηκε | |
| 54 | live | live | live | ζωντανά (site) | "WS ● live" stays as is (technical badge). |
| 55 | OT bus | OT-bus | OT-Bus | δίαυλος OT | |
| 56 | message | bericht | Nachricht | μήνυμα (site) | "msgIDs" and "MsgID" stay. |
| 57 | command | commando | Befehl | εντολή (site) | |
| 58 | response | antwoord | Antwort | απάντηση | |
| 59 | supported / not supported | ondersteund / niet ondersteund | unterstützt / nicht unterstützt | υποστηρίζεται / δεν υποστηρίζεται | "Boiler: not supported" = Ketel: niet ondersteund. |
| 60 | never seen / unknown | nooit gezien / onbekend | nie gesehen / unbekannt | δεν εμφανίστηκε ποτέ / άγνωστο | |
| 61 | status | status | Status | κατάσταση | |
| 62 | health | toestand | Zustand | υγεία συστήματος | Judgement call: NL "gezondheid" reads oddly for a device. |
| 63 | degraded / operational | beperkt / werkt normaal | eingeschränkt / betriebsbereit | υποβαθμισμένο / σε λειτουργία | |
| 64 | network / router | netwerk / router | Netzwerk / Router | δίκτυο (site) / router | AP mode stays "AP-modus" / "AP-Modus" / "λειτουργία AP". |
| 65 | scan / rescan | scannen / opnieuw scannen | suchen / erneut suchen | σάρωση / νέα σάρωση | |
| 66 | weather / location | weer / locatie | Wetter / Standort | καιρός (site) / τοποθεσία | "Detect location" = Locatie bepalen / Standort ermitteln / Εντοπισμός τοποθεσίας. |
| 67 | simulation / bench simulation | simulatie / testopstelling-simulatie | Simulation / Prüfstand-Simulation | προσομοίωση / προσομοίωση πάγκου δοκιμών | |
| 68 | diagnostics / diagnose | diagnose | Diagnose | διάγνωση | "Diagnostic recording" = diagnose-opname / Diagnoseaufzeichnung / καταγραφή διάγνωσης. |

### 2.3 App, files, actions

| # | English | Nederlands (nl) | Deutsch (de) | Ελληνικά (el) | Note |
|---|---|---|---|---|---|
| 69 | Home (nav) | Overzicht | Übersicht | Αρχική | Not "Start": that is the Start/Stop button. |
| 70 | Monitor (nav) | Monitor | Monitor | Παρακολούθηση | |
| 71 | Settings | Instellingen | Einstellungen | Ρυθμίσεις (site) | |
| 72 | Advanced | Geavanceerd | Erweitert | Για προχωρημένους (short: Προχωρημένα) | |
| 73 | System | Systeem | System | Σύστημα | |
| 74 | firmware | firmware | Firmware | firmware (site) | "Update Firmware" = Firmware bijwerken / Firmware aktualisieren / Ενημέρωση firmware. |
| 75 | update (noun / verb) | update / bijwerken | Update / aktualisieren | ενημέρωση (site) | "Check for updates" = Controleren op updates / Nach Updates suchen / Έλεγχος για ενημερώσεις. |
| 76 | flash (write firmware) | flashen | flashen | εγγραφή (flash) | "Ready to flash" = Klaar om te flashen / Bereit zum Flashen / Έτοιμο για εγγραφή. |
| 77 | restart / reboot | herstarten | neu starten | επανεκκίνηση | "ReBoot"/"REBOOT" buttons: Herstarten / Neustart / Επανεκκίνηση. |
| 78 | reset | resetten (Wi-Fi: Wi-Fi wissen) | zurücksetzen | επαναφορά | "Reset Wireless" = Wi-Fi resetten / WLAN zurücksetzen / Επαναφορά Wi-Fi. |
| 79 | setup wizard / run setup | installatiehulp / instellen starten | Einrichtungsassistent / Einrichtung starten | οδηγός ρύθμισης / εκτέλεση ρύθμισης | "Run SAT setup" = SAT instellen / SAT einrichten / Ρύθμιση SAT. |
| 80 | upload | uploaden | hochladen | μεταφόρτωση | |
| 81 | download | downloaden | herunterladen | λήψη | "⬇ Download" button: Downloaden / Herunterladen / Λήψη. |
| 82 | file / directory / file system | bestand / map / bestandssysteem | Datei / Ordner / Dateisystem | αρχείο / φάκελος / σύστημα αρχείων | |
| 83 | log | log | Log | καταγραφή (site) | NL/DE hobbyists say "log"; "logboek" is too formal. |
| 84 | graph / chart | grafiek | Diagramm | γράφημα (site) | |
| 85 | statistics | statistieken | Statistik | στατιστικά (site) | |
| 86 | capture / stream to file | opname / naar bestand schrijven | Aufzeichnung / in Datei schreiben | καταγραφή / εγγραφή σε αρχείο | |
| 87 | value / unit / name / description / direction / count | waarde / eenheid / naam / omschrijving / richting / aantal | Wert / Einheit / Name / Beschreibung / Richtung / Anzahl | τιμή / μονάδα / όνομα / περιγραφή / κατεύθυνση / πλήθος | |
| 88 | timestamp / interval / last change | tijdstempel / interval / laatste wijziging | Zeitstempel / Intervall / letzte Änderung | χρονοσφραγίδα / διάστημα / τελευταία αλλαγή | |
| 89 | enabled / disabled | ingeschakeld / uitgeschakeld | aktiviert / deaktiviert | ενεργό / ανενεργό | Toggle state on its own: aan/uit, an/aus, ενεργό/ανενεργό. "SAT is off" = SAT staat uit / SAT ist aus / Το SAT είναι ανενεργό. |
| 90 | warning / error / failed | waarschuwing / fout / mislukt | Warnung / Fehler / fehlgeschlagen | προειδοποίηση / σφάλμα / απέτυχε | "Save failed" = Opslaan mislukt / Speichern fehlgeschlagen / Η αποθήκευση απέτυχε. |
| 91 | unsaved changes | niet-opgeslagen wijzigingen | ungespeicherte Änderungen | μη αποθηκευμένες αλλαγές | |
| 92 | Loading… / Checking… | Laden… / Controleren… | Wird geladen… / Wird geprüft… | Φόρτωση… / Έλεγχος… | |

### 2.4 Buttons (fixed set)

| English | nl | de | el |
|---|---|---|---|
| Save / Save settings | Opslaan / Instellingen opslaan | Speichern / Einstellungen speichern | Αποθήκευση / Αποθήκευση ρυθμίσεων |
| Apply | Toepassen | Übernehmen | Εφαρμογή |
| Cancel | Annuleren | Abbrechen | Ακύρωση |
| Discard | Verwerpen | Verwerfen | Απόρριψη |
| Close / Dismiss | Sluiten | Schließen | Κλείσιμο |
| Retry | Opnieuw | Erneut versuchen | Επανάληψη |
| Delete | Verwijderen | Löschen | Διαγραφή |
| Forget (BLE sensor) | Vergeten | Entfernen | Αφαίρεση |
| Assign / Select | Toewijzen / Kiezen | Zuweisen / Auswählen | Ανάθεση / Επιλογή |
| Ignore / Not now | Negeren / Niet nu | Ignorieren / Nicht jetzt | Παράβλεψη / Όχι τώρα |
| Send | Versturen | Senden | Αποστολή |
| Clear | Wissen | Leeren | Εκκαθάριση |
| Start / Stop / Pause | Start / Stop / Pauze | Start / Stopp / Pause | Έναρξη / Διακοπή / Παύση |
| Enable / Disable | Inschakelen / Uitschakelen | Aktivieren / Deaktivieren | Ενεργοποίηση / Απενεργοποίηση |
| Back / Return | Terug | Zurück | Επιστροφή |

---

## 3. Style per language

### Common to all three

- **Placeholders**: copy `{{name}}` exactly; you may move it within the sentence. Never
  translate the name inside the braces.
- **Plurals**: nl, de and el all have only the CLDR forms `one` and `other`. Provide
  `key_one` and `key_other` whenever the English has them; no other plural keys.
- **Length**: UI labels stay short. Aim for at most about 130 % of the English length on
  buttons, tabs, chart legends and diagram labels; use the `(short)` forms of the glossary
  and standard abbreviations (max., min., temp.). Sentences in help text may be longer.
- **Case**: sentence case, as the English. Where the English is ALL CAPS (diagram labels like
  SPACE HEATING, OUTSIDE), write the translation in caps too.
- **Decimal comma**: nl, de and el all write `1,5 bar`, `0,1 bar`, `21,5 °C`. Applies to numbers
  you write inside a translated string. Values the page formats at runtime are not yours to
  change. Keep the space and unit exactly as the English (`°C`, `l/min`).
- **Symbols and punctuation**: keep the icons and separators of the English (`⬇`, `▸`, `·`, `→`,
  `…`, `—`). Use the single character `…`, not three dots.
- **SAT**: always "SAT", never expanded except the product name "Smart Autotune Thermostat",
  which stays English. Grammatical gender: nl "de SAT-regeling" or just "SAT"; de "SAT" without
  article where possible, else "die SAT-Regelung"; el "το SAT" (neuter, like the site does with
  "το gateway").
- **Experimental/beta**: do not soften or strengthen warnings. A risk warning stays a risk
  warning ("Enable anyway (accept risk)").
- Do not translate what you cannot place: if a string's meaning is unclear, keep the English
  for that key and list it in your notes.

### Nederlands (nl)

- Register: informal **je/jouw**, never "u". Netherlands and Belgium both read it.
- Buttons and menu items: the **infinitive** ("Opslaan", "Toepassen", "Firmware bijwerken").
- Instructions in sentences: imperative with je-form ("Kies eerst een bestand.",
  "Druk op Opnieuw scannen.").
- Status texts: short, no subject where possible ("Instellingen opgeslagen",
  "Apparaat herstart…").
- Compounds: write together ("aanvoertemperatuur"); with a product name or abbreviation, use a
  hyphen ("SAT-instellingen", "OT-bus", "BLE-sensoren", "cv-ketel", "Wi-Fi-instellingen").
- Common hobbyist anglicisms are fine where the Dutch word is stiffer: setpoint, override, log,
  flashen, uploaden, downloaden, firmware, gateway, live.
- Quotes: "..." as in the English source.

### Deutsch (de)

- Register: **neutral, without "Sie" or "du"** wherever possible.
- Buttons and menu items: the **infinitive** ("Speichern", "Übernehmen", "Neu starten").
- Instructions: infinitive constructions ("Zuerst eine Datei auswählen.") or impersonal
  passive ("Die Einstellungen werden gespeichert."). If a sentence truly needs an address
  (rare, e.g. a warning to the user), use **"du"** in lowercase. Reason: hobbyist home-automation
  UIs lean informal (Home Assistant's German UI uses "du", to my knowledge; not re-checked
  for this glossary).
- Status texts: noun or participle style ("Einstellungen gespeichert", "Gerät startet neu…").
- Compounds: together or with a hyphen next to a product name or abbreviation
  ("SAT-Einstellungen", "OT-Bus", "BLE-Sensoren", "WLAN-Einstellungen").
- Wi-Fi: always **"WLAN"** in translated German text ("WLAN zurücksetzen", "WLAN-Einstellungen").
- German runs about 30 % longer: use `(short)` forms in labels (Vorlauf, Rücklauf, WW,
  max. Vorlauftemp.).
- Quotes: „…“.

### Ελληνικά (el)

- Register: **polite plural**, as on the OTGW site ("Κατεβάστε", "Εγκαταστήστε", "σας").
- Buttons and menu items: the **noun form** that Greek UIs use ("Αποθήκευση", "Εφαρμογή",
  "Ακύρωση", "Επανεκκίνηση"), not the imperative.
- Instructions in sentences: plural imperative ("Επιλέξτε πρώτα ένα αρχείο.",
  "Πατήστε Νέα σάρωση.").
- Status texts: short, impersonal ("Οι ρυθμίσεις αποθηκεύτηκαν", "Η συσκευή επανεκκινείται…").
- Keep the English words the site keeps: firmware, gateway, browser, log in technical lines,
  and all names in section 1. Give them neuter gender ("το firmware", "το gateway").
- **All caps**: Greek capitals drop the accent (tonos): write "ΠΡΟΣΑΓΩΓΗ", "ΕΠΙΣΤΡΟΦΗ",
  "ΕΞΩΤΕΡΙΚΑ", not "ΠΡΟΣΑΓΩΓΉ". Where the page uppercases via CSS, the browser only drops the
  tonos correctly when `lang="el"` is set on the page: flag this for the Apply step
  (setting `document.documentElement.lang` on a language switch).
- Final sigma `ς` at word end, also before a hyphen or slash.
- Greek question mark: plain ASCII `;` only. Greek quotes «…».
- Abbreviations: μέγ. (max), ελάχ. (min), θερμ. (temperature), ΖΝΧ (DHW).
