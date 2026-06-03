# HA Discovery Five-Device Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split HA MQTT discovery from one device into five (boiler, thermostat, gateway, ESP-firmware, SAT), modern-by-default, with one umbrella legacy switch that restores 1.x.x byte-for-byte.

**Architecture:** Add a `device` dimension to the HA discovery context; emit a per-device `device:` block with HA-core-aligned identifiers/metadata and per-device "full-block-once" tracking; route each entity to its owning device via a central `haDeviceForEntity()` (OT keys transcribed from HA core, firmware-native by rule); switch unique_id scheme (`{nodeId}-{device}-{label}` modern, `{nodeId}-{label}` legacy) on a new umbrella `bLegacyMode` that subsumes ADR-106's `bUseLegacyOtTopics`. Migration republishes discovery and orphan-cleans the old single-device configs.

**Tech Stack:** Arduino C/C++ (single TU, ESP8266 + ESP32-S3), manual JSON via `MqttJsonWriter` (no ArduinoJson), PROGMEM string discipline (ADR-004), `python build.py`/`evaluate.py` gates, Node + a host MQTT-capture harness for the golden-file discovery test.

**Spec:** `docs/superpowers/specs/2026-06-03-ha-discovery-five-device-split-design.md` · **Backlog:** TASK-648 · **Branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support` (2.0.0 only).

**Hard conventions:**
- No `String` in MQTT/discovery hot paths (ADR-004); PROGMEM (`F`/`PSTR`) for all literals.
- No raw `#ifdef ESP8266/ESP32` outside the abstraction layer; gate by `HAS_*` flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_SAT_BLE`, ...).
- Every firmware/data change needs a prerelease bump (`bin/bump-prerelease.sh`) before commit.
- Shared worktree + a concurrent TASK-817 session: the runner edits, then builds/commits on the MAIN thread; if a subagent does the edit it must NOT build/commit; exclude `OTGW-firmware.ino`/`networkStuff.ino` from staging (foreign edits live there); after `bin/bump-prerelease.sh`, stage explicit paths and verify `git diff --cached --ignore-cr-at-eol --numstat -- src/OTGW-firmware | awk '$1>2||$2>2'` shows only your files + `version.h`.
- `.pio` collisions are common in this shared tree; on a per-env build failure that names `sconsign`/`.ino.cpp: No such file`, `rm -rf .pio/build` and rebuild solo — it is infra, not your code.
- Firmware has no unit-test harness; per-task verification = `python build.py --firmware` (both envs) + `python evaluate.py --quick`. The behavioural gate for this feature is the **golden-file discovery test** (Task 3) plus manual HA validation at the end.

---

## File Structure

- `src/OTGW-firmware/MQTTstuff.h` — `HaDiscoveryContext` gains `HaDevice device` + per-device metadata accessors; `enum class HaDevice`; `bLegacyMode` declaration neighbourhood (the MQTT settings section, near `bUseLegacyOtTopics` at line 68).
- `src/OTGW-firmware/MQTTHaDiscovery.cpp` — `writeDeviceBlock()` per-device metadata + per-device first-entity tracking; unique_id builders insert `<device>`; `haDeviceForEntity()` routing table; gateway identity helper; per-entity emitters pass their `device`.
- `src/OTGW-firmware/settingStuff.ino` — load/save/migrate `bLegacyMode` (alias from `bUseLegacyOtTopics`); arm discovery republish+cleanup on toggle.
- `src/OTGW-firmware/MQTTstuff.ino` — discovery driver: per-device first-entity reset; migration orphan-cleanup of old single-device topics; legacy-mode gating.
- `tests/webui/` (host, not shipped) — `ha-discovery-golden.test.mjs`: capture/inspect emitted discovery, assert device blocks + routing + unique_ids + legacy byte-identical.
- `docs/adr/ADR-XXX-ha-discovery-five-device-topology.md` — new ADR.
- `docs/api/MQTT.md` — routing table + `bLegacyMode` docs.

---

## Task 1: Umbrella `bLegacyMode` setting (no behaviour change yet)

**Files:**
- Modify: `src/OTGW-firmware/MQTTstuff.h:68` (add field near `bUseLegacyOtTopics`)
- Modify: `src/OTGW-firmware/settingStuff.ino` (load/default + alias migration)

- [ ] **Step 1: Add the field.** In the same struct as `bUseLegacyOtTopics` (MQTTstuff.h:68), immediately after it add:

```cpp
  // TASK-648: umbrella 1.x.x compatibility mode. false (default) = modern
  // (HA-core five-device topology + {nodeId}-{device}-{label} unique_ids +
  // new self-describing OT-topic names). true = legacy 1.x.x (single device,
  // {nodeId}-{label} unique_ids, legacy OT-topic names). Subsumes
  // bUseLegacyOtTopics (kept as a deprecated load-time alias for one release).
  bool    bLegacyMode = false;
```

- [ ] **Step 2: Migrate the deprecated alias on load.** In `settingStuff.ino`, in the settings post-processing/defaults block (where other `settings.mqtt.*` defaults are applied — search for `settings.mqtt.sTopTopic`), add after the load completes:

```cpp
  // TASK-648: bUseLegacyOtTopics is subsumed by bLegacyMode. If a config from a
  // prior build set the old flag, honour it once by folding it into bLegacyMode.
  if (settings.mqtt.bUseLegacyOtTopics) settings.mqtt.bLegacyMode = true;
```

- [ ] **Step 3: Repoint legacy-topic reads at the umbrella.** Replace the three alias-selection reads of `settings.mqtt.bUseLegacyOtTopics` in `OTGW-Core.ino:1538,1553,1575` and the one in `MQTTstuff.ino:2366` (`const bool useLegacy = settings.mqtt.bUseLegacyOtTopics;`) with `settings.mqtt.bLegacyMode`. Leave the `bUseLegacyOtTopics` field itself in place (read only by Step 2).

Example (OTGW-Core.ino:1538):
```cpp
  const char* labelToPublish = (aliasLabel && !settings.mqtt.bLegacyMode) ? aliasLabel : topic;
```

- [ ] **Step 4: Build + evaluate.** `python build.py --firmware` (exit 0, both env `.bin`); `python evaluate.py --quick` (0 failures).

- [ ] **Step 5: Commit.**
```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware  # then unstage foreign files + verify numstat per conventions
git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): add umbrella bLegacyMode, subsumes bUseLegacyOtTopics (TASK-648)"
```
(behaviour identical so far: `bLegacyMode` defaults false = today's modern topic naming.)

---

## Task 2: `HaDevice` enum + context device dimension + per-device device block

**Files:**
- Modify: `src/OTGW-firmware/MQTTstuff.h:352-361` (`HaDiscoveryContext`)
- Modify: `src/OTGW-firmware/MQTTHaDiscovery.cpp:2219` (`writeDeviceBlock`)

- [ ] **Step 1: Add the device enum + context fields.** In `MQTTstuff.h` above `HaDiscoveryContext` (line 352) add:

```cpp
// TASK-648: HA discovery device topology (five-device split).
enum class HaDevice : uint8_t { Boiler = 0, Thermostat, Gateway, Esp, Sat };
```
Then inside `HaDiscoveryContext` add fields:
```cpp
    HaDevice    device = HaDevice::Esp;   // owning device for the current entity
    // Per-device "full block once" flags (replace the single isFirstEntity).
    bool        firstSeen[5] = { true, true, true, true, true };
```
Keep `isFirstEntity` for now (Task removes its uses); do not delete yet.

- [ ] **Step 2: Per-device identifier + metadata helpers.** In `MQTTHaDiscovery.cpp` above `writeDeviceBlock` (line 2219) add:

```cpp
// TASK-648: device-identifier suffix per HaDevice (modern). Legacy uses bare nodeId.
static PGM_P haDeviceSuffix(HaDevice d) {
  switch (d) {
    case HaDevice::Boiler:     return PSTR("-boiler");
    case HaDevice::Thermostat: return PSTR("-thermostat");
    case HaDevice::Gateway:    return PSTR("-gateway");
    case HaDevice::Esp:        return PSTR("-esp");
    case HaDevice::Sat:        return PSTR("-sat");
  }
  return PSTR("-esp");
}
```

- [ ] **Step 3: Rewrite `writeDeviceBlock` to emit the per-device block.** Replace the body (lines 2219-2241) so the identifier is `nodeId + haDeviceSuffix(device)` in modern mode (bare `nodeId` in legacy), and the full metadata block (mfr/model/name/sw/hw) is written once per device using `ctx.firstSeen[(uint8_t)ctx.device]`, sourcing per-device metadata via a `haDeviceMeta(ctx, device, &mfr,&model,&name,&sw,&hw)` helper (added in Task 5 for boiler/thermostat/gateway; for now Esp/Sat use firmware version + board). Modern vs legacy gated on `settings.mqtt.bLegacyMode`:

```cpp
static bool writeDeviceBlock(MqttJsonWriter &w, HaDiscoveryContext &ctx) {
  const bool legacy = settings.mqtt.bLegacyMode;
  if (!w.writeChar('"') || !w.writeProgmem(kDev) || !w.writeProgmem(PSTR("\":{"))) return false;
  // identifiers: legacy = nodeId; modern = nodeId + device suffix
  if (!w.writeChar('"') || !w.writeProgmem(kIds) || !w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.nodeId)) return false;
  if (!legacy && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;
  if (!w.writeChar('"')) return false;

  bool *seen = legacy ? &ctx.isFirstEntity : &ctx.firstSeen[(uint8_t)ctx.device];
  if (*seen) {
    HaDeviceMeta m; haDeviceMeta(ctx, ctx.device, m);   // Task 5 fills boiler/thermostat/gateway
    if (!writeJsonComma(w) || !writeJsonKV(w, kMfr, m.manufacturer)) return false;
    if (!writeJsonComma(w) || !writeJsonKV(w, kModel, m.model)) return false;
    if (!writeJsonComma(w) || !w.writeChar('"') || !w.writeProgmem(kDevName) || !w.writeProgmem(PSTR("\":\"")) ) return false;
    if (!w.writeRam(m.name)) return false;
    if (!w.writeProgmem(PSTR("\""))) return false;
    if (m.swVersion && m.swVersion[0] && (!writeJsonComma(w) || !writeJsonKV(w, kSwVer, m.swVersion))) return false;
    if (m.hwVersion && m.hwVersion[0] && (!writeJsonComma(w) || !writeJsonKV(w, PSTR("hw_version"), m.hwVersion))) return false;
    *seen = false;
  }
  return w.writeChar('}');
}
```
Add a minimal `struct HaDeviceMeta { const char *manufacturer, *model, *name, *swVersion, *hwVersion; };` near the top of the file and a stub `haDeviceMeta()` that, for THIS task, returns today's single-device values for every device (so behaviour is unchanged until Task 4 routes entities). Mark the stub `// Task 5 replaces per-device sourcing`.

- [ ] **Step 4: Reset per-device firstSeen at the start of a discovery run.** Find where `ctx.isFirstEntity` is initialised true before the discovery loop (grep `isFirstEntity` in `MQTTstuff.ino`/`MQTTHaDiscovery.cpp`); alongside it set `for (auto &f : ctx.firstSeen) f = true;`.

- [ ] **Step 5: Build + evaluate.** Both envs green; evaluator 0-fail. (Behaviour: all entities still `device = Esp` default → modern emits one `{nodeId}-esp` device; this is INTENTIONALLY not final — Task 4 assigns real devices. Legacy unaffected.)

- [ ] **Step 6: Commit.**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware; git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): per-device HA discovery device block + HaDevice dimension (TASK-648)"
```

---

## Task 3: unique_id scheme switch + golden-file legacy test (test-first)

**Files:**
- Create: `tests/webui/ha-discovery-golden.test.mjs`
- Create: `tests/fixtures/ha-discovery-legacy.golden.txt` (captured from a legacy-mode build)
- Modify: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (unique_id builders)

- [ ] **Step 1: Insert `<device>` into the unique_id builders.** Every uniq_id site writes `ctx.nodeId` then `-<label>`. At each (the sites near MQTTHaDiscovery.cpp:2297, 2410, 2654, 3110, 3211, and the climate/switch/select/button builders), insert the device suffix in modern mode immediately after `w.writeRam(ctx.nodeId)`:
```cpp
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!settings.mqtt.bLegacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648 modern: -<device>
```
Do this uniformly at every uniq_id construction site (grep `uniq_id`/`writeRam(ctx.nodeId)` to enumerate; there are ~10).

- [ ] **Step 2: Write the golden-capture harness.** `tests/webui/ha-discovery-golden.test.mjs` runs a host build of the discovery generator OR (simpler, no firmware host build available) parses a captured discovery dump. Decision: capture via the existing MQTT debug capture (`scripts/capture-mqtt-debug.bat` / the device's discovery republish) into a text file of `topic<TAB>payload` lines. The test asserts, given a captured **legacy-mode** dump, that it is byte-for-byte equal to the committed golden fixture; and given a **modern-mode** dump, that (a) five distinct `"identifiers":"{nodeId}-{boiler|thermostat|gateway|esp|sat}"` appear, (b) every `uniq_id` matches `^{nodeId}-(boiler|thermostat|gateway|esp|sat)-`, (c) no `via_device` key is present.

```js
import fs from 'node:fs';
function loadDump(p){ return fs.readFileSync(p,'utf8').split('\n').filter(Boolean)
  .map(l=>{const i=l.indexOf('\t'); return {topic:l.slice(0,i), payload:l.slice(i+1)};}); }
let ok=true; const log=(p,m)=>{console.log((p?'PASS':'FAIL')+': '+m); if(!p)ok=false;};
// 1. Legacy byte-identical
const legacy = fs.readFileSync(process.env.LEGACY_DUMP,'utf8');
const golden = fs.readFileSync(process.env.LEGACY_GOLDEN,'utf8');
log(legacy===golden, 'legacy-mode discovery is byte-identical to the 1.x.x golden fixture');
// 2. Modern device split
const modern = loadDump(process.env.MODERN_DUMP);
const nodeId = process.env.NODE_ID;
const ids = new Set();
for (const {payload} of modern){ const m=payload.match(/"identifiers":"([^"]+)"/); if(m) ids.add(m[1]); }
for (const d of ['boiler','thermostat','gateway','esp','sat'])
  log(ids.has(`${nodeId}-${d}`), `modern emits device ${nodeId}-${d}`);
log(modern.every(e=>{const m=e.payload.match(/"uniq_id":"([^"]+)"/); return !m || new RegExp(`^${nodeId}-(boiler|thermostat|gateway|esp|sat)-`).test(m[1]);}),
    'every modern uniq_id is {nodeId}-{device}-...');
log(modern.every(e=>!/"via_device"/.test(e.payload)), 'no via_device key present (matches HA core)');
console.log(ok?'RESULT: ALL PASS':'RESULT: FAILURES'); process.exitCode = ok?0:1;
```

- [ ] **Step 3: Capture fixtures.** On the bench: build legacy-mode (`bLegacyMode=true`), trigger a discovery republish, capture to `tests/fixtures/ha-discovery-legacy.golden.txt` (this is the 1.x.x baseline — capture it from the PRE-Task-2 commit or with bLegacyMode forced, before modern routing exists, so it is a true 1.x.x reference). Commit the fixture. (If a bench capture is unavailable in CI, this test is bench-run; document that in the test header.)

- [ ] **Step 4: Run the test.** `LEGACY_DUMP=... LEGACY_GOLDEN=tests/fixtures/ha-discovery-legacy.golden.txt MODERN_DUMP=... NODE_ID=otgw-xxxx node tests/webui/ha-discovery-golden.test.mjs` → expect `RESULT: ALL PASS` once Task 4 routing exists (legacy byte-identical should pass already after Task 3).

- [ ] **Step 5: Build + evaluate + commit.**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware tests/webui/ha-discovery-golden.test.mjs tests/fixtures/ha-discovery-legacy.golden.txt
git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): device-prefixed modern unique_ids + golden legacy test (TASK-648)"
```

---

## Task 4: `haDeviceForEntity()` routing + bilateral replication

**Files:**
- Modify: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (routing table + emitters set `ctx.device`)
- Reference (transcribe from): HA core `sensor.py`, `binary_sensor.py`, `climate.py`, `switch.py`, `select.py`, `button.py` (dev branch)

- [ ] **Step 1: Add the routing table + function.** Build a PROGMEM table keyed by the firmware's entity `label` (the OTmap label / topic segment) → `HaDevice`, transcribed from HA core's `device_description.device_identifier`. Structure:
```cpp
struct HaRoute { const char *label; HaDevice dev; };
// OT entities: device per HA core. Slave-origin -> Boiler, master/room -> Thermostat,
// OTGW_* -> Gateway. Transcribe each key from HA core platform tables.
static const HaRoute kHaRoutes[] PROGMEM = {
  // --- examples (transcribe the full set from HA core) ---
  { "ch_water_temp", HaDevice::Boiler }, { "dhw_temp", HaDevice::Boiler },
  { "rel_mod_level", HaDevice::Boiler }, { "return_water_temp", HaDevice::Boiler },
  { "room_temp", HaDevice::Thermostat }, { "room_setpoint", HaDevice::Thermostat },
  { "outside_temp", HaDevice::Thermostat },
  // ... slave_* flags -> Boiler; master_* flags -> Thermostat; otgw_* -> Gateway ...
};
HaDevice haDeviceForEntity(const char *label);  // prototype in MQTTstuff.h
```
The implementer transcribes the COMPLETE OT key set from the HA core files cited in the spec (the per-key device is `device_description.device_identifier` in each entity_description). Firmware-native fallback rule inside the function:
```cpp
HaDevice haDeviceForEntity(const char *label) {
  if (!label) return HaDevice::Esp;
  // 1. explicit OT routes (PROGMEM table, binary or linear search)
  for (size_t i=0;i<ARRAY_SIZE(kHaRoutes);++i)
    if (!strcmp_P(label, (PGM_P)pgm_read_ptr(&kHaRoutes[i].label)))
      return (HaDevice)pgm_read_byte(&kHaRoutes[i].dev);
  // 2. firmware-native by prefix
  if (!strncmp_P(label, PSTR("sat/"), 4)) return HaDevice::Sat;
  if (!strncmp_P(label, PSTR("otgw_"), 5) || !strncmp_P(label, PSTR("settings/"), 9)) return HaDevice::Gateway;
  // 3. default: ESP node (heap/wifi/uptime/ip/ota/dallas/s0/oled/mqtt/ws)
  return HaDevice::Esp;
}
```

- [ ] **Step 2: Each emitter sets `ctx.device` before writing.** In every per-entity discovery emitter (sensor, binary_sensor, climate, switch, select, button, the SAT emitters, the ESP/diagnostic emitters), set `ctx.device = haDeviceForEntity(<this entity's label>);` immediately before `writeDeviceBlock`/uniq_id. For the SAT and ESP emitters that have no OT label, set the device explicitly (`HaDevice::Sat` / `HaDevice::Esp`).

- [ ] **Step 3: Bilateral replication.** For the OT keys HA core lists on BOTH boiler and thermostat (the bilateral set enumerated in the spec), emit the entity TWICE — once with `ctx.device=Boiler`, once with `ctx.device=Thermostat` — each producing a distinct device-prefixed uniq_id. Implement by iterating a per-key `HaDevice` list (extend `HaRoute` to allow two devices, e.g. a second optional `dev2`, sentinel `HaDevice::Esp`+flag, or a `bool bilateral`). For bilateral keys, loop the emit over {Boiler,Thermostat}.

- [ ] **Step 4: Verify routing with the golden test.** Capture a modern dump; run Task 3's test → all five devices present, every uniq_id device-prefixed, bilateral keys present under both `-boiler-` and `-thermostat-`. Add an assertion to the test: count that each bilateral key appears exactly twice (once per device).

- [ ] **Step 5: Build + evaluate + commit.**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware tests/webui/ha-discovery-golden.test.mjs
git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): per-entity device routing + bilateral replication (TASK-648)"
```

---

## Task 5: Per-device metadata sourcing (incl. gateway PIC xor OTDirect)

**Files:**
- Modify: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (`haDeviceMeta` real implementation)

- [ ] **Step 1: Implement `haDeviceMeta(ctx, device, HaDeviceMeta&)`.** Replace the Task-2 stub:
  - **Boiler:** manufacturer = decoded `SLAVE_MEMBERID` (numeric to string; "Unknown" if unpolled), model = `SLAVE_PRODUCT_TYPE`, sw = `SLAVE_OT_VERSION`, hw = `SLAVE_PRODUCT_VERSION`, name = `"Boiler ({hostname})"`.
  - **Thermostat:** same from `MASTER_*`, name `"Thermostat ({hostname})"`.
  - **Gateway:** PIC xor OTDirect by capability:
```cpp
    if (platformHasPic()) { m.manufacturer = "Schelte Bron"; m.model = "OpenTherm Gateway";
                            m.swVersion = picAboutVersion(); /* PR=A [18:] */ }
    else                  { m.manufacturer = "OTGW-firmware"; m.model = "OpenTherm Gateway (OTDirect)";
                            m.swVersion = ctx.version; }
    m.name = "Gateway";
```
    (Use the existing PIC-present capability flag — `HAS_PIC`/runtime detect; confirm the accessor name. `picAboutVersion()` = the stored `PR=A` about string offset 18; confirm/define the accessor.)
  - **Esp:** manufacturer = "Espressif" (or board vendor from `boards.h`), model = board name, sw = `ctx.version`, hw = chip id, name = `"OTGW-firmware ({hostname})"`.
  - **Sat:** manufacturer = "OTGW-firmware", model = "Smart Autotune Thermostat", sw = `ctx.version`, name = "SAT".
  Read OT product values from the OT data cache (the same source REST/`/value` uses). Where a product id is unpolled, leave sw/hw empty (the device block already omits empty sw/hw per Task 2 Step 3).

- [ ] **Step 2: Build + evaluate.** Both envs; evaluator 0-fail.

- [ ] **Step 3: Verify metadata in the modern dump.** Extend the golden test: assert the boiler/thermostat/gateway device blocks carry the expected `manufacturer`/`model` keys (presence; values depend on polled data so assert keys exist + gateway model is one of the two strings).

- [ ] **Step 4: Commit.**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware tests/webui/ha-discovery-golden.test.mjs
git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): per-device discovery metadata + gateway PIC/OTDirect identity (TASK-648)"
```

---

## Task 6: Migration — orphan-clean old single-device discovery on republish

**Files:**
- Modify: `src/OTGW-firmware/MQTTstuff.ino` (discovery republish entry; reuse the ADR-070 / `bUseLegacyOtTopics`-toggle cleanup path at ~2861-2920)
- Modify: `src/OTGW-firmware/settingStuff.ino` (arm cleanup when `bLegacyMode` toggles)

- [ ] **Step 1: Detect the topology change.** Persist the last-published topology (a small `settings.mqtt.bLastPublishedLegacy` or a state flag). On boot/republish, if the effective mode differs from last-published, arm a full discovery republish + orphan cleanup of the OTHER scheme's retained config topics (modern→clear single-device `{nodeId}` configs; legacy→clear the five-device configs).

- [ ] **Step 2: Reuse the existing cleanup mechanism.** The ADR-106 toggle already clears the other set's retained discovery topics (MQTTstuff.ino:2861-2920, "arm cleanup when bUseLegacyOtTopics flips"). Extend that arming to also fire when `bLegacyMode` flips, and broaden the cleared topic set to include stale device-scheme configs (publish empty retained payload to each old config topic).

- [ ] **Step 3: Verify.** On the bench: start legacy, switch to modern, confirm (MQTT explorer) the old single-device config topics get an empty retained payload (HA removes them) and the five-device configs appear. Add a note to the golden test header describing this manual check (automating retained-clear needs a live broker).

- [ ] **Step 4: Build + evaluate + commit.**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware; git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "feat(mqtt): migrate discovery topology with orphan cleanup on mode change (TASK-648)"
```

---

## Task 7: ESP8266 flash/burst verification + fallback decision

**Files:**
- Possibly modify: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (conditional bilateral on ESP8266, only if needed)

- [ ] **Step 1: Measure ESP8266.** `python build.py --firmware` and read the esp8266 flash usage from the build log; capture the discovery-burst behaviour (the ADR-088 window) by counting modern-mode discovery publishes and confirming the burst-cooldown still bounds them.

- [ ] **Step 2: Decide.** If esp8266 flash fits AND the burst stays within the ADR-088 window: no change; record the measured headroom in the task notes. If it does NOT fit: implement an ESP8266-only single-device-per-bilateral-value fallback (gate the bilateral second-emit behind a capability check, NOT a raw `#ifdef` — add a `HAS_*`/board capability flag), and document the platform deviation in the ADR (Task 8).

- [ ] **Step 3: Build + evaluate (both envs) + commit (only if code changed).**
```bash
bin/bump-prerelease.sh; git add src/OTGW-firmware; git restore --staged src/OTGW-firmware/OTGW-firmware.ino src/OTGW-firmware/networkStuff.ino
git commit -m "perf(mqtt): bound ESP8266 discovery footprint for five-device split (TASK-648)"
```
(If no code change, record the measurement in the backlog task and skip the commit.)

---

## Task 8: ADR + MQTT.md + finalize

**Files:**
- Create: `docs/adr/ADR-XXX-ha-discovery-five-device-topology.md`
- Modify: `docs/api/MQTT.md`

- [ ] **Step 1: Write the ADR** (use `/adr-kit:adr`). Status Proposed. Document: the five-device topology + identifiers; modern-default + umbrella `bLegacyMode` (supersedes ADR-106's standalone `bUseLegacyOtTopics` decision); the HA-core-authoritative OT routing + firmware-native ESP/SAT/gateway buckets; bilateral replication; PIC-xor-OTDirect gateway identity; deliberate deviations (no via_device; ESP+SAT beyond HA core; any ESP8266 single-device fallback from Task 7). Reference `haDeviceForEntity()` as the auditable mapping and the golden test as the gate.

- [ ] **Step 2: Update MQTT.md** with the `bLegacyMode` setting, the modern unique_id scheme, the five-device table, and the per-entity device routing table (boiler/thermostat/gateway/esp/sat).

- [ ] **Step 3: Commit (docs-only, no bump).**
```bash
git add docs/adr/ADR-XXX-ha-discovery-five-device-topology.md docs/api/MQTT.md
git commit -m "docs(adr): five-device HA discovery topology + MQTT.md routing table (TASK-648)"
```

- [ ] **Step 4: Full build + finalize.** `python build.py` (firmware + filesystem, both envs); `python evaluate.py --quick`; run the golden test (legacy byte-identical + modern split). Then:
```bash
backlog task edit 648 --check-ac 13 --final-summary "Five-device HA discovery split, modern-default, umbrella bLegacyMode restores 1.x.x byte-for-byte; per-entity routing from HA core + firmware-native ESP/SAT/gateway; bilateral replication; PIC/OTDirect gateway identity; migration orphan-cleanup; ADR + MQTT.md; golden-file legacy test green." -s Done
git push origin feature-dev-2.0.0-otgw32-esp32-sat-support
```
(Note: the original 3-device ACs #1-#12 are superseded by the spec's five-device decisions; map the surviving ones — opt-in→modern-default, identifiers, metadata, migration, byte-identical-legacy, MQTT.md, build/evaluate — to the tasks above and check them off as their tasks complete.)

---

## Self-Review notes (author)

- **Spec coverage:** five devices + identifiers (Task 2/5) ✓; modern-default + umbrella legacy (Task 1) ✓; HA-core unique_ids (Task 3) ✓; routing incl. bilateral (Task 4) ✓; per-device metadata + PIC/OTDirect (Task 5) ✓; migration/orphan-clean (Task 6) ✓; ESP8266 resource gate (Task 7) ✓; no via_device (asserted in Task 3 golden test) ✓; ADR + MQTT.md (Task 8) ✓; legacy byte-identical (Task 3 golden) ✓.
- **Known data-transcription step (not a placeholder, flagged):** the full OT key→device table in Task 4 Step 1 must be transcribed from the cited HA core platform files; the structure, the bucket rules, the bilateral set, and the verification are concrete. Treat the transcription as the bulk of Task 4.
- **Implementer confirmations flagged inline:** PIC-present accessor + `picAboutVersion()` name (Task 5); the exact uniq_id construction sites (Task 3 Step 1, ~10 — enumerate by grep); where `isFirstEntity` is reset pre-loop (Task 2 Step 4); whether a bench MQTT capture is available for the golden fixture vs documenting it as a bench-only gate (Task 3).
- **Type consistency:** `HaDevice` enum, `haDeviceSuffix`, `haDeviceForEntity`, `haDeviceMeta`/`HaDeviceMeta`, `ctx.device`, `ctx.firstSeen[]`, `bLegacyMode` used consistently across tasks.
