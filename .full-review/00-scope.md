# Review Scope

## Target

Full branch review of `1.4.1` vs `dev` (base branch). This branch is the "heap-pressure reduction + MQTT discovery verification + time-boundary dispatcher" release candidate for OTGW-firmware 1.4.1.

- Commits in range: 14 (0cc5dd10 → deaddd85)
- Source files changed: ~20 C/C++/INO files, plus frontend JS/CSS and build helpers
- Documentation: ADR-062 (retained discovery verification), ADR-064 (time-boundary single-caller contract)
- Backlog: TASK-338 through TASK-351

## Themes

1. **Heap pressure reduction** during Home Assistant MQTT discovery drip
   - Slower drip interval, wider HEAP_LOW backoff, fragmentation-aware publish gates, Status-burst cooldown
   - Lower heap guard thresholds tuned on tester log data
2. **Cumulative heap diagnostics** with hourly MQTT publishing (drop/tier counters)
3. **MQTT discovery verification & republish** (on-demand + daily automatic)
4. **Time-boundary dispatcher refactor** (unify hourChanged / dayChanged / yearChanged under single-caller contract)
5. **Nightly restart refactor** to use hourChanged hook
6. **Version bump** 1.4.0 → 1.4.1-beta, build artefact hygiene (remove .gz from git)

## Files (source / docs / build)

### Source code (C/C++/INO)
- `src/OTGW-firmware/MQTTstuff.ino` — major changes (discovery drip, verification, cooldown)
- `src/OTGW-firmware/OTGW-firmware.ino` — time dispatcher refactor, nightly restart hook
- `src/OTGW-firmware/OTGW-firmware.h` — pending flags, cooldown state, discovery verification state
- `src/OTGW-firmware/OTGW-Core.ino` + `.h` — Status-burst cooldown hooks, version bump
- `src/OTGW-firmware/helperStuff.ino` — heap guard thresholds, drop counters
- `src/OTGW-firmware/restAPI.ino` — on-demand discovery verification endpoints
- `src/OTGW-firmware/handleDebug.ino` — debug hooks for discovery verification
- `src/OTGW-firmware/settingStuff.ino` — settings for discovery verification cadence
- `src/OTGW-firmware/mqtt_configuratie.cpp` — discovery verification hooks
- `src/OTGW-firmware/networkStuff.{h,ino}` — time-dispatcher callers
- Smaller edits: `FSexplorer.ino`, `jsonStuff.ino`, `outputs_ext.ino`, `s0PulseCount.ino`, `sensors_ext.ino`, `webSocketStuff.ino`, `webhook.ino` (mostly version bumps)

### Frontend
- `src/OTGW-firmware/data/index.js` — heap diagnostics UI, discovery verification UI
- `src/OTGW-firmware/data/index.css` / `index_dark.css` — minor
- `src/OTGW-firmware/data/FSexplorer.{css,html}`, `FSexplorer_dark.css`, `graph.js` — version-only

### Config / build
- `evaluate.py` — extended with new checks
- `src/OTGW-firmware/data/mqttha.cfg` — version bump
- `src/OTGW-firmware/data/version.hash`, `version.h` — version tracking
- Removed generated `.gz` artefacts from git (404f7a48)

### Docs
- `docs/adr/ADR-062-retained-discovery-verification.md` (Proposed)
- `docs/adr/ADR-064-time-boundary-single-caller-contract.md` (Proposed)

### Backlog
- 14 task files covering the changes (TASK-338 through TASK-351)

## Flags

- Security Focus: no (embedded LAN-only firmware; see ADR "HTTP/WS only")
- Performance Critical: yes (ESP8266, ~40KB RAM, heap pressure is the main theme)
- Strict Mode: no
- Framework: Arduino / ESP8266 (auto-detected)

## Platform constraints reviewers must remember

- ESP8266 with ~40KB usable RAM and a **4KB cooperative CONT stack**
- `doBackgroundTasks()` IS re-entrant (MQTTstuff.ino:1055 reads files while auto-configuring)
- `feedWatchDog()` at OTGW-Core.ino:403 has `yield()` commented out — no re-entrancy from watchdog
- PROGMEM pointers must match helpers: never pass PROGMEM to `printf %s`, `MQTTclient.write()`, `writeMqttChunk()`
- No HTTPS / WSS — plain HTTP/WS only (ADR)
- No `String` class in hot paths (ADR-004)
- Static buffers `mqttAutoCfgScratch` / `ot_log_buffer` have specific ownership rules

## Review Phases

1. Code Quality & Architecture
2. Security & Performance
3. Testing & Documentation
4. Best Practices & Standards
5. Consolidated Report
