/*
***************************************************************************
**  Program  : debugFlags.h
**
**  The telnet debug flags by name (TASK-1561, ADR-206): one table shared by the
**  telnet keys (handleDebug.ino) and the REST API /api/v2/debug/flags (restAPI.ino),
**  so a key and its REST name cannot drift apart.
**
**  The ten log flags only switch debug output. The two simulation switches
**  (sensorsim, key 'd': Dallas sensor simulation; otgwsim, key 's'/'S': OTGW serial
**  replay) change behaviour, so their entry carries an onChange action, run only
**  when the value really changes (Robert 2026-11-02: include them in the API).
**
**  Needs DebugSection (debugStuff.h) and the HAS_* flags (boards.h) before it.
***************************************************************************
*/
#pragma once

#include <string.h>

// Side effects of the simulation switches; defined in handleDebug.ino.
void debugFlagSensorSimChanged(bool on);   // initSensors(), and an immediate poll when switched on
void debugFlagOtgwSimChanged(bool on);     // restart the replay timer, signal the replay task (TASK-1321)

struct DebugFlagDef {
  const char* name;                   // REST name, lower case
  char        key;                    // telnet key
  bool DebugSection::* member;        // the flag in state.debug
  const char* label;                  // telnet text: "Debug <label>: true/false" (scripts parse it)
  void      (*onChange)(bool on);     // nullptr for a log flag; run after a real change
};

static const DebugFlagDef kDebugFlags[] = {
  { "otmsg",     '1', &DebugSection::bOTmsg,          "OTmsg",                    nullptr },
  { "restapi",   '2', &DebugSection::bRestAPI,        "RestAPI",                  nullptr },
  { "mqtt",      '3', &DebugSection::bMQTT,           "MQTT",                     nullptr },
  { "sensors",   '4', &DebugSection::bSensors,        "Sensors",                  nullptr },
  { "sat",       '5', &DebugSection::bSAT,            "SAT",                      nullptr },
  { "otdirect",  '6', &DebugSection::bOTDirect,       "OTDirect",                 nullptr },
#if HAS_SAT_BLE
  { "satble",    '7', &DebugSection::bSATBLE,         "SAT BLE",                  nullptr },
#endif
  { "sattrace",  '8', &DebugSection::bSATTrace,       "SAT trace (high volume)",  nullptr },
  { "mqttgate",  'g', &DebugSection::bMQTTGate,       "MQTT Gating",              nullptr },
  { "ntp",       'n', &DebugSection::bNTP,            "NTP",                      nullptr },
  { "sensorsim", 'd', &DebugSection::bSensorSim,      "Dallas sensor simulation", debugFlagSensorSimChanged },
  { "otgwsim",   's', &DebugSection::bOTGWSimulation, "OTGW serial simulation",   debugFlagOtgwSimChanged },
};
static const size_t kDebugFlagCount = sizeof(kDebugFlags) / sizeof(kDebugFlags[0]);

// ASCII case-insensitive equality; plain C, so the host test compilers need no POSIX header.
static inline bool debugFlagIEq(const char* a, const char* b)
{
  for (;; a++, b++) {
    char x = *a, y = *b;
    if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
    if (x != y) return false;
    if (x == '\0') return true;
  }
}

// 'S' is the second key of the serial simulation (kept from the old telnet switch).
static inline const DebugFlagDef* debugFlagByKey(char key)
{
  if (key == 'S') key = 's';
  for (size_t i = 0; i < kDebugFlagCount; i++) {
    if (kDebugFlags[i].key == key) return &kDebugFlags[i];
  }
  return nullptr;
}

static inline const DebugFlagDef* debugFlagByName(const char* name)
{
  if (name == nullptr) return nullptr;
  for (size_t i = 0; i < kDebugFlagCount; i++) {
    if (debugFlagIEq(kDebugFlags[i].name, name)) return &kDebugFlags[i];
  }
  return nullptr;
}

// "true"/"on"/"1" -> 1, "false"/"off"/"0" -> 0, anything else -> -1 (rejected).
static inline int debugFlagParseBool(const char* v)
{
  if (v == nullptr) return -1;
  const bool single = (v[0] != '\0' && v[1] == '\0');
  if (debugFlagIEq(v, "true") || debugFlagIEq(v, "on") || (single && v[0] == '1')) return 1;
  if (debugFlagIEq(v, "false") || debugFlagIEq(v, "off") || (single && v[0] == '0')) return 0;
  return -1;
}

// Write a flag; the onChange action runs only when the value really changes.
static inline void debugFlagWrite(DebugSection& d, const DebugFlagDef* f, bool on)
{
  bool& v = d.*(f->member);
  const bool changed = (v != on);
  v = on;
  if (changed && f->onChange != nullptr) f->onChange(on);
}

// Set one flag explicitly (idempotent). Returns the flag, or nullptr with *err = 1 (unknown name) or 2 (value not a bool).
static inline const DebugFlagDef* debugFlagSet(DebugSection& d, const char* name, const char* value, int* err)
{
  const DebugFlagDef* f = debugFlagByName(name);
  if (f == nullptr) { if (err) *err = 1; return nullptr; }
  const int b = debugFlagParseBool(value);
  if (b < 0) { if (err) *err = 2; return nullptr; }
  debugFlagWrite(d, f, b == 1);
  if (err) *err = 0;
  return f;
}
