export const meta = {
  name: 'arduinojson-fanout',
  description: 'Convert hand-rolled JSON output to ArduinoJson v7 per-file (TASK-867)',
  phases: [{ title: 'Migrate', detail: 'one agent per JSON-output file' }],
}

// restAPI.ino is handled on the main thread (441 entries, core file). These five
// are self-contained JSON producers, one agent each, disjoint files (no conflict).
const FILES = [
  { f: 'src/OTGW-firmware/SATcontrol.ino', note: 'satSendStatusJSON: the 132-field flat SAT status + satSendHealthJSON (22-field ?detail=full). Has CUSTOM float-precision helpers (kp %.4f, ki %.6f) — assign the native float value, drop the precision formatting; ArduinoJson round-trips the number. PRESERVE all 132 / 22 keys exactly.' },
  { f: 'src/OTGW-firmware/SATweather.ino', note: '~20 entries, weather + needs-setup objects.' },
  { f: 'src/OTGW-firmware/SATble.ino', note: '~14 entries, BLE status + roster (roster is an ARRAY of objects — use JsonArray + a.add<JsonObject>()).' },
  { f: 'src/OTGW-firmware/OTDirect.ino', note: 'otdirect status/settings/overrides. One restBeginStream block builds JSON with raw stream writes — read it and rebuild as a JsonDocument.' },
  { f: 'src/OTGW-firmware/FSexplorer.ino', note: '3 restBeginStream blocks (filesystem files listing is a heterogeneous array + trailing storage-summary object; firmware files). Rebuild each as a JsonDocument/JsonArray.' },
]

const RULES = `
You are migrating ONE FILE from the firmware's hand-rolled streaming JSON to ArduinoJson v7.
Branch: feature-2.0.0-esp32s3-async. Single translation unit (Arduino .ino).

CANONICAL EXAMPLE (already shipped, sendHealth in restAPI.ino):
  OLD:
    sendStartJsonMap(F("health"));
    sendJsonMapEntry(F("status"), LittleFSmounted ? F("UP") : F("DEGRADED"));
    sendJsonMapEntry(F("heap"), platformFreeHeap());
    sendJsonMapEntry(F("mqttconnected"), CBOOLEAN(state.mqtt.bConnected)); // BROKEN string-bool
    sendEndJsonMap(F("health"));
  NEW:
    JsonDocument doc;
    JsonObject h = doc[F("health")].to<JsonObject>();
    h[F("status")]        = LittleFSmounted ? "UP" : "DEGRADED";
    h[F("heap")]          = platformFreeHeap();
    h[F("mqttconnected")] = state.mqtt.bConnected;   // real JSON bool
    restSendJson(doc);   // helper in webServerCompat.h: serializeJson into the AsyncResponseStream

MACRO / TYPE RULES (the whole point is to FIX broken types, NOT reproduce them):
  - CBOOLEAN(x)  -> emits the STRING "true"/"false" = BROKEN boolean. Replace with the real bool x.
  - raw bool passed to sendJsonMapEntry(bool) -> was already a real JSON bool; keep as bool.
  - CONOFF(x)="On"/"Off", CCONOFF(x)="ON"/"OFF", CONLINEOFFLINE(x)="online"/"offline" -> these are
    GENUINE string labels, NOT booleans. KEEP them as strings: h[k] = CONOFF(x);
  - CBINARY(x)="1"/"0" -> KEEP as the string it is (do not change type); note it in your return.
  - float via sendJsonMapEntry(float) (%.3f) or a custom-precision helper -> assign the native float
    value (drop the printf formatting); ArduinoJson serializes the number.
  - int/uint/int32 -> native integer assignment.
  - const char* / String / F() string -> native string assignment.

STRUCTURE RULES:
  - Preserve EXACT key names, order, nesting, and the COMPLETE set of fields. No additions, no removals.
  - Object wrapper sendStartJsonMap(F("x")) -> JsonObject o = doc[F("x")].to<JsonObject>();
  - Nested object -> JsonObject child = parent[F("k")].to<JsonObject>();
  - Array -> JsonArray a = parent[F("k")].to<JsonArray>(); then a.add(v) or JsonObject e = a.add<JsonObject>();
  - Entries emitted inside a for-loop -> emit into a JsonArray inside the loop.
  - Replace each whole sendStart...(N x sendJsonMapEntry)...sendEnd OR restBeginStream(...)+serialize block
    with ONE JsonDocument built up, then a single restSendJson(doc).
  - If a function streams a LARGE/unbounded body in chunks (e.g. file dumps), and converting it to a single
    JsonDocument would buffer megabytes, LEAVE IT and report it instead.

HARD CONSTRAINTS:
  - Edit ONLY the file assigned to you. Do NOT touch any other file.
  - Do NOT build, flash, commit, bump the version, or run git.
  - Do NOT change inbound JSON parsing (that is a separate task). Output only.
  - Do NOT touch settings persistence.
  - Use F() for keys and string literals (PROGMEM rule). No String in hot paths beyond what the old code did.

Return STRICT JSON: { file, functions_converted:[...], type_changes:[{field, from, to}], left_as_is:[{what, why}], notes }
`

phase('Migrate')
const SCHEMA = {
  type: 'object',
  properties: {
    file: { type: 'string' },
    functions_converted: { type: 'array', items: { type: 'string' } },
    type_changes: { type: 'array', items: { type: 'object', properties: { field: {type:'string'}, from:{type:'string'}, to:{type:'string'} }, required:['field','from','to'] } },
    left_as_is: { type: 'array', items: { type: 'object', properties: { what:{type:'string'}, why:{type:'string'} }, required:['what','why'] } },
    notes: { type: 'string' },
  },
  required: ['file','functions_converted','type_changes','left_as_is','notes'],
}

const results = await parallel(FILES.map(item => () =>
  agent(`${RULES}\n\nYOUR FILE: ${item.f}\nFILE NOTE: ${item.note}\n\nConvert every hand-rolled JSON OUTPUT function in this file now.`,
    { label: `migrate:${item.f.split('/').pop()}`, phase: 'Migrate', schema: SCHEMA })
))

return { migrated: results.filter(Boolean) }
