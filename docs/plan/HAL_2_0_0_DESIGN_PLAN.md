# OTGW-firmware: Hardware Abstraction Layer (HAL) Design Plan — 2.0.0

**Date:** 2026-05-22
**Target branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support` (2.0.0 line)
**Authored on branch:** `claude/hardware-hal-design-K7x1q` (dev-based, plan-only)
**Status:** Proposed — design artefact for review, no implementation in this branch
**Related ADRs (2.0.0):** ADR-031, ADR-060, ADR-072, ADR-079, ADR-082, ADR-083

> This plan describes work that belongs in the 2.0.0 worktree
> (`feature-dev-2.0.0-otgw32-esp32-sat-support`). It is committed here on a
> dev-based branch as a reviewable design artefact only; no firmware code
> changes are made on dev. Implementation will follow as a separate task
> against 2.0.0, per `CLAUDE.md` § "Worktree layout".

---

## Context

OTGW-firmware 2.0.0 moet meerdere ESP-borden ondersteunen: het bestaande
Nodoshop OTGW (ESP8266 + PIC), de nieuwe Nodoshop OTGW32 (ESP32-S3), een
ESP32-S3-mini upgrade voor het originele OTGW-bord, en op termijn diverse
losse OpenTherm-shields zonder PIC. De huidige 2.0.0 branch
(`feature-dev-2.0.0-otgw32-esp32-sat-support`, commit `a03a647b`) heeft al
*compile-time* board-defines in `src/OTGW-firmware/boards.h` plus een
`platform.h` toolchain-laag en een `Hardwaretypes.h` runtime mode-enum,
maar:

- Pin-mapping is statisch per compile (board kiezen = rebuild).
- Call sites lezen `boards.h` defines direct — geen HAL-grens.
- Geen ruimte voor user-defined shield-mappings zonder firmware build.
- Geen ADR die de HAL-grens vastlegt; ADR-083 dekt alleen PlatformIO-targets.

Doel: een runtime board-mapping laag — qua patroon vergelijkbaar met
Tasmota-templates ("ESPHome zonder YAML") — die bij boot ingelezen wordt,
zonder de bestaande compile-time defines weg te gooien. Built-in profielen
blijven als PROGMEM-seeds in flash; een optionele JSON-overlay in LittleFS
kan individuele pins overschrijven voor third-party shields. Eén binary
per MCU bedient alle borden van die MCU-familie.

## Scope

**In scope (deze taak):**
- HAL-grens (`hal.h` + `hal.cpp` / `hal.ino`) met typed accessors voor
  alle pin/peripheral interacties.
- `HardwareConfig` RAM-struct, gepopuleerd bij boot vanuit één gekozen
  built-in profiel + optionele LittleFS-overlay.
- Built-in profielen voor: `nodoshop_esp8266`, `nodoshop_otgw32`,
  `esp32_s3_mini_upgrade`. ESP8266-binary kent alleen ESP8266-profielen;
  ESP32-binary alleen ESP32-profielen.
- `settings.hardware.sBoardId` + `settings.hardware.bOverlayEnabled`
  settings-velden. Web-UI keuze in een nieuwe "Hardware" tab.
- Refactor alle bestaande call sites (`digitalWrite(PICRST, …)`,
  `setLed()`, watchdog-I2C, S0/relais pin-init) door HAL-accessors.
- Eén ADR (HAL-grens + config-laad-pad), één backlog task.

**Niet in scope (follow-up tasks):**
- Direct-OT implementatie. De HAL exposeert `HW_MODE_OT_DIRECT` en
  `ot_in_pin` / `ot_out_pin` slots, maar `hal_ot_direct_*()` faalt nog
  bewust met een logmelding + DEGRADED-mode tot een aparte taak de
  `src/libraries/OpenTherm/` library (nu leeg) vendor't en
  bedraadt — typisch ihormelnyk's library, ADR-keuze in follow-up.
- Concrete shield-profielen (Ihor Melnyk, DIYLESS Master, etc.). Volgen
  per shield met fysieke validatie.
- Backport naar dev (1.5.x). Dev blijft single-board (Nodoshop ESP8266
  hardcoded). HAL is 2.0.0-only — geen cross-worktree master-plan nodig.
- SAT-board pinning. ADR-072 dekt SAT-laag al; HAL exposeert alleen de
  bus-pins die SAT nodig heeft (UART/I2C) zonder SAT-logica te raken.

## Architectuur

```
                  ┌─────────────────────────────────────┐
                  │   Call sites (.ino feature files)   │
                  │   OTGW-Core, MQTTstuff, restAPI,    │
                  │   helperStuff, networkStuff, …       │
                  └────────────────┬─────────────────────┘
                                   │  hal.picReset()
                                   │  hal.ledSet(LED1, ON)
                                   │  hal.watchdogFeed()
                                   │  hal.otMode()  → enum
                                   │  hal.cfg()     → const HardwareConfig&
                                   ▼
                  ┌─────────────────────────────────────┐
                  │   HAL (hal.h + hal.cpp/.ino)         │
                  │   - Typed accessors                  │
                  │   - Reads HardwareConfig in RAM      │
                  │   - Guards capability flags          │
                  └────────────────┬─────────────────────┘
                                   │ reads
                                   ▼
                  ┌─────────────────────────────────────┐
                  │   HardwareConfig (RAM, ~80–120 B)    │
                  │   pins + caps, populated at boot     │
                  └─────┬───────────────────────────┬────┘
                        │ seeded from               │ overlaid by
                        ▼                           ▼
            ┌─────────────────────┐        ┌──────────────────────┐
            │ Built-in profiles   │        │ /boards/custom.json   │
            │ (PROGMEM seeds)     │        │ (LittleFS, optional)  │
            │ - nodoshop_esp8266  │        │ Named-slot overrides  │
            │ - nodoshop_otgw32   │        │ per field             │
            │ - esp32_s3_mini_…   │        └──────────────────────┘
            └─────────────────────┘
```

**Compile-time keuze**: alleen MCU (ESP8266 vs ESP32). PlatformIO-envs
blijven zoals ze zijn.
**Runtime keuze** (binnen één binary): welk built-in profiel + of de
overlay actief is.

## Kritieke bestanden (in 2.0.0 worktree)

Alle paden onder `src/OTGW-firmware/` tenzij anders aangegeven. **Deze
plan beschrijft de doelstaat in de 2.0.0 worktree** (branch
`feature-dev-2.0.0-otgw32-esp32-sat-support`). De huidige container heeft
alleen dev — implementatie gebeurt in de 2.0.0 worktree na worktree-setup
zoals beschreven in `CLAUDE.md` § "Worktree layout".

### Nieuw

- **`hal.h`** — publieke HAL API. Bevat:
  - `enum class Led : uint8_t { LED1, LED2, RGB_R, RGB_G, RGB_B };`
    (ESP32-S3 RGB slots zijn `kPinNone` op ESP8266-profielen).
  - `enum class OtMode { Unknown, Pic, OtDirect, Degraded };`
  - `struct HardwareConfig { … }` — named-slot pin map + cap flags
    (zie volgende sectie).
  - Vrije functies (geen klasse — past bij single-TU `.ino`-stijl):
    `void halBegin();`
    `const HardwareConfig& halCfg();`
    `void halLedSet(Led, bool);`
    `void halLedBlink(Led, uint8_t count, uint16_t ms);`
    `bool halButtonPressed();`
    `void halPicReset();`
    `bool halPicPresent();` (`HAS_PIC` cap)
    `OtMode halOtMode();`
    `void halWatchdogFeed();` (no-op als geen I2C watchdog cap)
    `int  halPin(PinSlot);` (escape hatch voor settings.outputs/s0 die
                              configurable pin gebruiken).

- **`hal.cpp`** (of `hal.ino` als de build het anders niet pakt — zie
  "Gotchas") — implementeert bovenstaande, bevat `static HardwareConfig
  s_cfg;` en het laad-pad.

- **`board_profiles.h`** — bevat de PROGMEM seeds. Eén compile-time
  array per MCU-familie, gegate door `#if defined(ARDUINO_ARCH_ESP8266)`
  vs `#if defined(ARDUINO_ARCH_ESP32)`.

- **`docs/adr/ADR-NNN-hardware-abstraction-layer.md`** (Proposed,
  next-available nummer in 2.0.0 worktree). Bevat Decision, Alternatives
  Considered (≥ 2: compile-time-only, JSON-as-source-of-truth zonder
  built-in seeds), Consequences (RAM-cost ~120 B, brick-risico via
  overlay → mitigatie via safe-boot fallback), Related (ADR-031, ADR-060,
  ADR-072, ADR-079, ADR-083). Enforcement-blok: `forbid_pattern` voor
  directe `digitalWrite\((PICRST|LED1|LED2|BUTTON|I2CSCL|I2CSDA),` in
  niet-`hal.*` files. `llm_judge: true` voor "nieuwe peripheral mag niet
  zonder HAL-accessor".

- **`data/boards/custom.json.example`** — voorbeeld overlay (commented
  named slots) voor users die een shield mappen.

### Bestaand, te wijzigen

- **`boards.h`** — wordt *seed-source*, niet langer call-site-target. De
  bestaande `#define PIN_*`-constants per board worden de waardes
  waarmee de PROGMEM seed arrays gevuld worden (één seed per
  `BOARD_*`-blok). Géén call site mag deze nog rechtstreeks gebruiken na
  refactor — verifieer met `grep -nE 'PIN_(I2C_SCL|I2C_SDA|BUTTON|PIC_RST|LED1|LED2|OT_)' src/OTGW-firmware/`
  en eis 0 hits buiten `hal.cpp` + `board_profiles.h`.

- **`Hardwaretypes.h`** — `OTGWHardwareMode` enum blijft, maar wordt
  geconsolideerd met `hal.h`'s `OtMode`. Kies één, herexporteer de
  ander als alias om diff-omvang te beperken. `HardwareSection` in
  `state` blijft *detected* state (PIC reageert wel/niet), distinct van
  *configured* mode in `settings.hardware`.

- **`OTGWSettings.h`** — nieuwe `HardwareSection` in `settings` (naast
  bestaande gewone secties):
  ```cpp
  struct HardwareSection {        // settings.hardware
    char sBoardId[24];            // "nodoshop_esp8266" | "nodoshop_otgw32" |
                                  // "esp32_s3_mini_upgrade" | "custom"
    bool bOverlayEnabled;         // /boards/custom.json overlay actief?
    bool bSafeBoot;               // ignore overlay (set by long-press boot)
  };
  ```
  Default `sBoardId` per MCU via `#if defined(ARDUINO_ARCH_ESP8266)` in
  de settings-init. Volg ADR-051 (Hungarian prefixes, twee-niveau
  named sub-section).

- **`OTGW-firmware.ino setup()`** — vroege call naar `halBegin()` *vóór*
  `setLed()`, `resetOTGW()`, `initWatchDog()`. Volgorde:
  1. `LittleFS.begin()`
  2. `readSettings()`
  3. `halBegin()` ← leest `settings.hardware.sBoardId`, kopieert seed,
     past overlay toe als enabled + safe-boot pin niet ingedrukt.
  4. Bestaande vervolg: WiFi, MQTT, PIC reset (nu via `halPicReset()`),
     watchdog, web handlers.

- **`OTGW-Core.ino`** — `resetOTGW()` (regel 531, 541 in dev; 2.0.0
  equivalent) wordt thin wrapper rond `halPicReset()`. Watchdog macro
  `FEEDWATCHDOGNOW` blijft maar wordt geherdefiniëerd als `halWatchdogFeed()`
  (één call site verandert, alle macro-users blijven werken).

- **`helperStuff.ino`** — `setLed()`, `blinkLED()`, `blinkLEDnow()`
  worden thin wrappers rond `halLedSet()` / `halLedBlink()`. Functie-
  signatures blijven gelijk (parameter blijft `uint8_t led` met
  bestaande LED1/LED2 macro-waardes), implementatie delegeert. Houdt
  call-site-diff minimaal.

- **`networkStuff.ino`** — MAC-address `#if defined(ESP8266) / ESP32`
  blok (regel 573-582 in dev) blijft; HAL is geen MCU-abstractie, dat
  doet `platform.h`. Wel: zet de eventuele Ethernet-init (W5500 SPI op
  OTGW32) achter `if (halCap(HAS_ETH_CAPABLE)) { … }`.

- **Web-UI** (`data/index.html`, `data/index.js`) — nieuwe "Hardware"
  card / pagina met:
  - dropdown `sBoardId` met built-in profielen voor *huidige* MCU
    (front-end krijgt lijst via `/api/v2/hardware/profiles`),
  - toggle `bOverlayEnabled`,
  - read-only weergave van actieve `HardwareConfig` velden (handig voor
    field-debug op Discord),
  - "Upload custom.json" button (POST naar `/api/v2/hardware/overlay`).

- **`restAPI.ino` (`kV2Routes[]`)** — drie nieuwe endpoints:
  - `GET  /api/v2/hardware/profiles` → array of `{id, name, mcu, caps}`.
  - `GET  /api/v2/hardware/config`   → actieve `HardwareConfig` als JSON
    (named slots).
  - `POST /api/v2/hardware/overlay`  → schrijft naar
    `/boards/custom.json` na schema-validatie, vraagt om reboot.

### Te hergebruiken (niet nieuw schrijven)

- **JSON-parsing**: `parseJsonKVLine()` (manueel, geen ArduinoJson — zie
  ADR-042). Named-slot schema is platte key=value, past perfect bij
  deze parser.
- **JSON-uitvoer**: `sendJsonMapEntry()` + `snprintf_P` helpers in
  `restAPI.ino`.
- **Settings-laad-pad**: bestaande `readSettings()` / `writeSettings()`
  in `settingStuff.ino` krijgt automatisch de nieuwe `hardware`-sectie
  als die in `OTGWSettings.h` is toegevoegd via de bestaande
  twee-niveau named-section macro's (zie ADR-051).
- **Bestaande `digitalWrite`/`pinMode`-helpers**: niet vervangen; HAL
  roept ze nog steeds aan, het is alleen de pin-bron die abstractie wordt.

## Named-slot schema (`HardwareConfig`)

```cpp
constexpr int8_t kPinNone = -1;

struct HardwareConfig {
  // Identity
  char     id[24];                  // matches sBoardId
  uint8_t  mcuFamily;               // 8266 | 32
  uint16_t caps;                    // bitfield: HAS_PIC, HAS_DIRECT_OT,
                                    //          HAS_ETH, HAS_OLED, HAS_RGB_LED,
                                    //          HAS_I2C_WATCHDOG, HAS_S0_INPUT
  // I2C bus (watchdog / OLED)
  int8_t   i2c_scl;
  int8_t   i2c_sda;
  uint8_t  i2c_watchdog_addr;       // 0x26 default, 0 = none

  // PIC interface (only if HAS_PIC)
  int8_t   pic_rst;                 // MCLR
  int8_t   pic_uart_num;            // -1 = use default Serial; ESP32 UART idx
  int8_t   pic_rx;
  int8_t   pic_tx;

  // Direct OpenTherm (only if HAS_DIRECT_OT) — slots reserved, impl follow-up
  int8_t   ot_in;
  int8_t   ot_out;

  // LEDs (kPinNone = absent)
  int8_t   led1;
  int8_t   led2;
  int8_t   rgb_r, rgb_g, rgb_b;
  uint8_t  led_brightness;          // 0–100, RGB only

  // Button(s)
  int8_t   button;                  // user button, also safe-boot trigger

  // OTGW32 extras (kPinNone op andere borden)
  int8_t   step_up_enable;
  int8_t   bypass_relay;
  int8_t   onewire;
  int8_t   eth_cs, eth_int, eth_rst;
};
```

Overlay JSON-formaat (LittleFS `/boards/custom.json`):
```json
{
  "based_on": "nodoshop_esp8266",
  "led2": 12,
  "ot_in": 13,
  "ot_out": 15,
  "caps_add": ["HAS_DIRECT_OT"],
  "caps_remove": ["HAS_PIC"]
}
```
`based_on` is verplicht en moet matchen met een built-in seed van de
*huidige* MCU. Onbekende keys → log warning, niet fataal. Validatie:
geen duplicate pins toegestaan; `pic_*` en `ot_*` mogen niet beide
gevuld zijn (mutually exclusive caps).

## Boot-time laad-pad (in `halBegin()`)

1. Lees `settings.hardware.sBoardId`. Leeg → fallback naar default per
   MCU (`nodoshop_esp8266` of `nodoshop_otgw32`).
2. Zoek seed in `board_profiles.h`'s array. Niet gevonden → log fatal
   error, val terug op MCU-default, zet `state.hw.eMode = DEGRADED`.
3. `memcpy_P(&s_cfg, seed, sizeof(HardwareConfig))`.
4. Check safe-boot: lees `cfg.button` GPIO direct (pinMode + digitalRead);
   ingedrukt vasthouden bij boot = sla overlay over, zet
   `settings.hardware.bSafeBoot = true` voor één boot.
5. Als `bOverlayEnabled && !bSafeBoot`: open `/boards/custom.json`, parse
   met `parseJsonKVLine`-stijl loop, valideer `based_on`, pas
   field-overrides toe op `s_cfg`. Fail → log, ga door zonder overlay.
6. Detect runtime: I2C-scan voor watchdog op `i2c_watchdog_addr`, OLED
   op vast adres → vul `state.hw` (separaat van `settings.hardware`).
7. `pinMode()`-calls voor alle non-`kPinNone` pins.
8. Return — HAL is bedrijfsklaar.

## ADR + Backlog

**Eén ADR** (`docs/adr/ADR-NNN-hardware-abstraction-layer.md`,
status Proposed → wachten op gebruikersgoedkeuring → Accepted) met
voor de Enforcement:
- `forbid_pattern` op `digitalWrite\(\s*(PIN_LED[12]|PIN_PIC_RST|PIN_BUTTON|PIN_I2C_(SCL|SDA))` buiten `hal.*`/`board_profiles.h`.
- `forbid_pattern` op `pinMode\(\s*(PIN_LED[12]|PIN_PIC_RST|…)` idem.
- `forbid_import` van `boards.h` in `.ino`-files anders dan `hal.cpp` /
  `board_profiles.h` (forceert dat call sites HAL-accessors gebruiken).
- `llm_judge: true` — vangt nieuwe peripheral-toevoegingen zonder HAL.

**Eén backlog task** (CLI: `backlog task create` in 2.0.0 worktree):
- Titel: `feat-2.0.0: HAL voor multi-board ESP support (named-slot mapping)`
- Acceptance Criteria (per AC zelfstandig verifieerbaar):
  1. `hal.h` + `hal.cpp` aanwezig met alle accessors uit het schema.
  2. `board_profiles.h` definieert seeds voor `nodoshop_esp8266`
     (ESP8266-binary), `nodoshop_otgw32` en `esp32_s3_mini_upgrade`
     (ESP32-binary).
  3. `settings.hardware.{sBoardId,bOverlayEnabled,bSafeBoot}`
     gepersisteerd in LittleFS na reboot.
  4. `halBegin()` laadt seed + optionele overlay; logregels tonen
     bron (`seed: nodoshop_otgw32, overlay: applied`).
  5. Safe-boot: knop ingedrukt bij boot negeert overlay, gelogd, en
     `state.hw.bSafeBoot == true`.
  6. Geen niet-HAL call site refereert nog naar `PIN_*` defines —
     `grep` na refactor levert 0 hits buiten `hal.cpp`/
     `board_profiles.h`/`boards.h` zelf.
  7. `python build.py --firmware` exit 0 voor *beide* env's
     (`esp8266` én `esp32`).
  8. `python evaluate.py --quick` toont geen nieuwe failures.
  9. ADR-NNN is Accepted (door user, niet self-approved) en het
     `adr-judge`-Enforcement-blok blokkeert een test-commit die
     `digitalWrite(PIN_LED1, …)` toevoegt in een niet-HAL `.ino`.
  10. REST `/api/v2/hardware/profiles` levert array, `/config` levert
      actieve `HardwareConfig`, `/overlay POST` valideert en bewaart.
  11. Web-UI Hardware-tab toont en bewerkt board-keuze.
- Definition of Done: bovenstaande + build/evaluator green + smoke-test
  OTA-flash op Nodoshop ESP8266 (PIC reset werkt) én OTGW32-prototype
  (LEDs, knop, watchdog).

## Cross-worktree

HAL is **2.0.0-only**. Dev (1.5.x) blijft single-board zoals het is —
backporten zou de maintenance-branch noodloos uitbreiden. Géén
parallel-task op dev nodig. Implementatie gebeurt in de 2.0.0 worktree
(`git worktree add ../OTGW-firmware-2.0.0 feature-dev-2.0.0-otgw32-esp32-sat-support`
indien afwezig, zoals `CLAUDE.md` § Worktree layout beschrijft).

De huidige container heeft alleen de dev tree. Twee opties bij executie:
1. **Voorkeur**: deze branch (`claude/hardware-hal-design-K7x1q`)
   abandonneren of mergen als plan-only artefact; werk doen vanuit de
   2.0.0-worktree in een lokale checkout.
2. Alternatief: rebase deze branch op `feature-dev-2.0.0-otgw32-esp32-sat-support`
   in de container, implementeer, push naar een 2.0.0-derived branch en
   open een PR tegen 2.0.0. Risico: per ongeluk dev-style code op 2.0.0
   raken. Optie 1 heeft voorkeur.

## Gotchas

- **Single-TU `.ino`-build**: alle `.ino` in `src/OTGW-firmware/` worden
  geconcateneerd. `hal.cpp` als `.cpp` werkt mits geen forward-declared
  symbol uit de Arduino-preprocess-magic nodig is; zo niet, hernoem naar
  `hal.ino`. Begin met `.cpp`, val terug op `.ino` als linker mokkert.
- **PROGMEM-pointer-veiligheid (ADR onbekend, project-rule)**: seeds in
  PROGMEM — gebruik `memcpy_P` voor de kopie naar RAM, nooit `strncmp_P`
  op `HardwareConfig` bytes. Zie CLAUDE.md § "Binary data: use memcmp_P".
- **Heap-fragmentatie**: geen `String`-class in `halBegin()`-pad, alleen
  `char[]` + `strlcpy`. Overlay-JSON-parse: hergebruik
  `parseJsonKVLine` op een lokale `static char buf[256]` (stack te
  groot).
- **Re-entrancy `doBackgroundTasks()`**: `halBegin()` draait in `setup()`,
  vóór de re-entrant loop start — geen yield-window-risico op `s_cfg`.
  Latere accessors zijn read-only op `s_cfg` → veilig.
- **ADR-027 conflict?** Check de 2.0.0-versie van ADR-051 (settings
  schema): nieuwe `settings.hardware`-sectie moet het bestaande
  Hungarian-prefix-patroon respecteren. Conflict onwaarschijnlijk maar
  expliciet verifiëren.

## Verificatie

End-to-end, te draaien in de 2.0.0-worktree na implementatie:

1. **Build, beide envs**:
   ```
   pio run -e esp8266
   pio run -e esp32
   ```
   óf via wrapper `./build.sh` (zoals `CLAUDE.md` § Build Commands).
   Beide moeten exit 0.

2. **Evaluator**:
   ```
   python evaluate.py --quick
   ```
   Geen nieuwe PROGMEM/string-class/heap warnings.

3. **ADR-judge**:
   - Plaats tijdelijk een `digitalWrite(PIN_LED1, HIGH);` in
     `MQTTstuff.ino`, stage, commit → moet geblokkeerd worden door
     `bin/adr-judge` met verwijzing naar ADR-NNN. Revert.

4. **HAL-grens-grep**:
   ```
   grep -rnE 'digitalWrite\(\s*(PIN_LED|PIN_PIC|PIN_BUTTON|PIN_I2C)' \
        src/OTGW-firmware/ --include='*.ino' --include='*.h' \
     | grep -v 'hal\.\(cpp\|ino\|h\)' \
     | grep -v 'board_profiles\.h'
   ```
   → 0 hits.

5. **Boot-log inspectie** (telnet poort 23 na flash):
   - Default boot: `HAL: seed=nodoshop_otgw32 overlay=disabled`
   - Met overlay enabled: `HAL: seed=nodoshop_otgw32 overlay=applied (3 fields)`
   - Safe-boot (knop ingedrukt): `HAL: seed=nodoshop_otgw32 overlay=skipped (safe-boot)`

6. **REST-smoke**:
   ```
   curl http://<dev-ip>/api/v2/hardware/profiles
   curl http://<dev-ip>/api/v2/hardware/config
   curl -X POST -H 'Content-Type: application/json' \
        --data @custom.json http://<dev-ip>/api/v2/hardware/overlay
   ```

7. **Field validatie** (Discord `#beta-testing`):
   - Nodoshop ESP8266: PIC-reset werkt, LEDs reageren, externe watchdog
     blijft tikken, MQTT/HA-discovery onveranderd.
   - OTGW32: zelfde checks, plus RGB-LED kleur-test, knop, OLED-detect
     bij boot (alleen detectie — gebruik valt onder ADR-072).
   - ESP32-S3-mini upgrade-bord: PIC-reset via UART (niet default
     Serial), LEDs werken — bewijst dat HAL pin- + uart-mapping
     daadwerkelijk runtime werkt.
