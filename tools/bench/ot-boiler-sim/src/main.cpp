// TASK-1654: OTGW32 as a scripted OpenTherm boiler for SAT wire-level bench tests.
//
// This sketch is NOT the OTGW firmware and is not part of its build. It runs on a second OTGW32 and
// uses that board's thermostat-side interface (the OpenTherm slave port) as the boiler, so a bench
// unit's boiler side can be wired to it. All boiler behaviour lives in boiler_sim.cpp (host-tested
// by test/host/test_ot_boiler_sim.py); this file is glue: pins, bus power, the OpenTherm library,
// LittleFS scenario loading, USB log and a few serial commands.
//
// USB serial (the S3's native USB, not a PIC link: this board runs no OTGW firmware):
//   log lines  "<ms> T<hex8>" for every request, "<ms> B<hex8>" for every answer, "<ms> -" for a
//              request left unanswered on purpose, "<ms> X<hex8>" for a frame received invalid
//   commands   "list", "run <file.scn>", "status", "restart" (newline terminated)
#include <Arduino.h>
#include <LittleFS.h>
#include <OpenTherm.h>

#include "boiler_sim.h"

// OTGW32 pins, as in src/libraries/Platform/src/boards.h:75-76, :79 and :85 (checked by the host test).
static const int SIM_PIN_OT_SLAVE_IN = 6;     // PIN_OT_SLAVE_IN:  OT input from the thermostat side
static const int SIM_PIN_OT_SLAVE_OUT = 7;    // PIN_OT_SLAVE_OUT: OT output to the thermostat side
static const int SIM_PIN_STEPUP_ENABLE = 10;  // PIN_STEPUP_ENABLE: bus power for the thermostat side
static const int SIM_PIN_BYPASS_RELAY = 47;   // PIN_BYPASS_RELAY: LOW keeps the two OT ports apart

static OpenTherm ot(SIM_PIN_OT_SLAVE_IN, SIM_PIN_OT_SLAVE_OUT, true);  // true = slave (boiler)

static otsim::Scenario scenario;
static otsim::Boiler *boiler = nullptr;
static char activeFile[64] = "/SB05-id25-data-invalid.scn";
static bool started = false;          // the run clock starts at the first valid request
static uint32_t startMs = 0;
static volatile bool pending = false; // an answer waits for its send time
static uint32_t pendingFrame = 0, pendingDueMs = 0;
static uint32_t nRequests = 0, nAnswers = 0, nSilent = 0, nInvalid = 0;

static void IRAM_ATTR otInterrupt() { ot.handleInterrupt(); }

static bool loadScenario(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f) { Serial.printf("cannot open %s\n", path); return false; }
  static char text[6144];
  size_t n = f.readBytes(text, sizeof(text) - 1);
  f.close();
  text[n] = 0;
  static otsim::Scenario next;
  char err[160];
  if (!otsim::parseScenario(text, next, err, sizeof(err))) { Serial.printf("parse error in %s: %s\n", path, err); return false; }
  scenario = next;
  delete boiler;
  boiler = new otsim::Boiler(scenario);
  snprintf(activeFile, sizeof(activeFile), "%s", path);
  started = false;
  pending = false;
  nRequests = nAnswers = nSilent = nInvalid = 0;
  File a = LittleFS.open("/active.txt", "w");
  if (a) { a.print(path); a.close(); }
  Serial.printf("scenario %s (%s), %d phases; clock starts at the first request\n", scenario.id, path, scenario.n_phases);
  return true;
}

static void onRequest(unsigned long request, OpenThermResponseStatus status) {
  uint32_t now = millis();
  if (status != OpenThermResponseStatus::SUCCESS) {
    nInvalid++;
    Serial.printf("%lu X%08lX\n", (unsigned long)now, request);
    return;
  }
  if (!boiler) return;
  if (!started) { started = true; startMs = now; }
  nRequests++;
  Serial.printf("%lu T%08lX\n", (unsigned long)now, request);
  otsim::Answer a = boiler->onRequest((uint32_t)request, now - startMs);
  if (!a.send) { nSilent++; Serial.printf("%lu -\n", (unsigned long)now); return; }
  pendingFrame = a.frame;
  pendingDueMs = now + a.delay_ms;
  pending = true;
}

static void handleCommand(char *line) {
  if (!strcmp(line, "list")) {
    File root = LittleFS.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) Serial.printf("  %s\n", f.name());
  } else if (!strncmp(line, "run ", 4)) {
    char path[64];
    snprintf(path, sizeof(path), "%s%s", line[4] == '/' ? "" : "/", line + 4);
    loadScenario(path);
  } else if (!strcmp(line, "restart")) {
    loadScenario(activeFile);
  } else if (!strcmp(line, "status")) {
    uint32_t t = started ? millis() - startMs : 0;
    Serial.printf("scenario %s t=%lu s phase=%d ch=%d flame=%d req=%lu ans=%lu silent=%lu invalid=%lu\n", scenario.id,
                  (unsigned long)(t / 1000), boiler ? boiler->phaseIndexAt(t) : -1, boiler && boiler->chActive(),
                  boiler && boiler->flameOn(), (unsigned long)nRequests, (unsigned long)nAnswers,
                  (unsigned long)nSilent, (unsigned long)nInvalid);
  } else if (*line) {
    Serial.println("commands: list | run <file.scn> | status | restart");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SIM_PIN_BYPASS_RELAY, OUTPUT);
  digitalWrite(SIM_PIN_BYPASS_RELAY, LOW);
  pinMode(SIM_PIN_STEPUP_ENABLE, OUTPUT);
  digitalWrite(SIM_PIN_STEPUP_ENABLE, HIGH);
  delay(50);  // let the bus voltage settle (as OTDirect.ino enableStepUp)
  if (!LittleFS.begin(false)) Serial.println("LittleFS mount failed: upload the scenarios with -t uploadfs");
  File a = LittleFS.open("/active.txt", "r");
  if (a) {
    String s = a.readString();  // setup only
    a.close();
    s.trim();
    if (s.length()) snprintf(activeFile, sizeof(activeFile), "%s", s.c_str());
  }
  loadScenario(activeFile);
  ot.begin(otInterrupt, onRequest);
  Serial.println("ot-boiler-sim ready (TASK-1654)");
}

void loop() {
  ot.process();
  if (pending && (int32_t)(millis() - pendingDueMs) >= 0) {
    if (ot.sendResponse(pendingFrame)) {  // false while the library's own minimum response gap runs
      pending = false;
      nAnswers++;
      Serial.printf("%lu B%08lX\n", (unsigned long)millis(), (unsigned long)pendingFrame);
    }
  }
  static char line[96];
  static size_t len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { line[len] = 0; handleCommand(line); len = 0; }
    else if (len < sizeof(line) - 1) line[len++] = c;
  }
}
