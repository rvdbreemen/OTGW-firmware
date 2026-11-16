// The driver of tests/test_sat_loop_integration.py: a stub-clock loop that calls the REAL handleCommandQueue(), checkCommandResponse(),
// the REAL processOT() feed hooks (text slices, see @HOOK_*@) and the REAL satControlLoop(), around a boiler MODEL (frames + water).
// It is a command interpreter on stdin; every observable fact is one line "EV <ms> <kind> <text>" on stdout.
#if !defined(HAS_SAT) || !HAS_SAT
#error "HAS_SAT is not defined: satIsEnabled() would be false and no scenario would reach the control tick"
#endif
#include <string>
#include <vector>

static uint32_t g_t0 = 1000000;                 // stub clock origin (millis() at "t=0"); keeps millis() clear of 0 and of the first-boot sentinels
static void ev(const char* kind, const char* fmt, ...) {
  char b[480]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  printf("EV %lu %s %s\n", (unsigned long)(g_ms - g_t0), kind, b);
}

// ---- boiler MODEL. STUB: first-order water + a burner with a minimum output; real boilers differ in every constant.
struct Boiler {
  bool ch = false, flame = false; float cs = 0; int mmCap = 100;            // what the PIC passed on (CH=, CS=, MM=)
  float flow = 30, ret = 28; float relmod = 0; uint32_t flameChangeMs = 0;
  float qMax = 24.0f, minModPct = 20.0f, loadK = 0.04f, roomC = 20.0f;      // kW, %, kW per K above room, room temperature
  float capKJperK = 400.0f, hystK = 5.0f, kp = 8.0f; uint32_t minOffMs = 60000;
  bool lowOffDenied = false;                                                // MsgID 3 HB bit 4
} bo;
static uint32_t g_nextFlow = 0, g_flowPer = 5000, g_nextRet = 0, g_retPer = 10000, g_nextStatus = 0, g_nextCfg = 0, g_nextTr = 0;
static bool g_linkDown = false, g_mute25 = false, g_muteRet = false, g_ret28Unknown = false, g_echoMute = false, g_pmAnswers = true;
static uint32_t g_pmDelay = 300, g_echoLat = 25, g_dt = 10, g_jit = 0;
static float g_outside = 8.0f, g_room = 20.0f;
static uint32_t g_ticks = 0;

// ---- processOT(): one boiler frame, then the REAL hooks. STUB: the OT value store of processOT() (print_*) is replaced by direct writes to
// OTcurrentSystemState; the 1-frame delay of delayedOTdata, the MQTT publish and the logging are not run.
static void frame(int id, int type, float fv = 0, int hb = -1, int lb = 0) {
  if (g_linkDown) return;
  OTdata = OpenthermData_t(); OTdata.buf[0] = 'B'; OTdata.id = (byte)id; OTdata.type = (byte)type; OTdata.masterslave = 1;
  const time_t now = 1700000000 + (g_ms - g_t0) / 1000;
  if (hb >= 0) { OTdata.valueHB = (byte)hb; OTdata.valueLB = (byte)lb; } else OTdata.f88(fv);
  OTdata.time = now;
  if (type == OT_READ_ACK) {
    if (id == 25) OTcurrentSystemState.Tboiler = fv;
    if (id == 28) OTcurrentSystemState.Tret = fv;
    if (id == 17) OTcurrentSystemState.RelModLevel = fv;
    if (id == 15) OTcurrentSystemState.MaxCapacityMinModLevel = (uint16_t)((hb << 8) | lb);
  }
  // the B-frame head of processOT() (real text): boiler presence stamp, rsptype
  @HOOK_STAMP@
  if (OTdata.id == 3 && OTdata.type == OT_READ_ACK) {
    @HOOK_CFG@
  }
  @HOOK_PM@
  @HOOK_FEED@
}

static void statusFrame() {                       // MsgID 0: flame bit and CH-active bit
  OTcurrentSystemState.Statusflags = bo.flame ? 0x08 : 0;
  OTcurrentSystemState.SlaveStatus = (bo.flame ? 0x08 : 0) | (bo.ch ? 0x02 : 0);
  frame(0, OT_READ_ACK, 0, OTcurrentSystemState.SlaveStatus, 0);
}
static void tr() {                                // MsgID 24 from the thermostat: room temperature
  OTcurrentSystemState.Tr = g_room; state.otBus.iTrThermostatMs = g_ms ? g_ms : 1;
}

// ---- the PIC: echoes every transmitted line a latency later (command_delivery pattern), passes the value to the boiler model
struct Pend { uint32_t at; char echo[40]; bool used; };
static Pend pend[64];
static void schedEcho(const char* line) {
  if (g_echoMute) return;
  for (auto& p : pend) if (!p.used) { p.used = true; p.at = g_ms + g_echoLat; snprintf(p.echo, sizeof p.echo, "%c%c: %s", line[0], line[1], line + 3); return; }
}
struct PmAns { uint32_t at; int id; bool used; };
static PmAns pma[8];
static void boilerGetsCmd(const char* line) {
  if (!strncmp(line, "CS=", 3)) bo.cs = (float)atof(line + 3);
  else if (!strncmp(line, "CH=", 3)) bo.ch = atoi(line + 3) != 0;
  else if (!strncmp(line, "MM=", 3)) bo.mmCap = (line[3] == 'T') ? 100 : atoi(line + 3);
  else if (!strncmp(line, "PM=", 3) || !strncmp(line, "TP=", 3)) {
    if (!g_pmAnswers) return;
    for (auto& p : pma) if (!p.used) { p.used = true; p.at = g_ms + g_pmDelay; p.id = atoi(line + 3); return; }
  }
}
static void sendFrameFor(int id) {
  switch (id) {
    case 25: if (!g_mute25) { ev("F25", "%.2f", bo.flow); frame(25, OT_READ_ACK, bo.flow); } break;   // F25: the flow SAT is fed (TASK-1990 scenario 6)
    case 28: if (g_muteRet) break; if (g_ret28Unknown) frame(28, OT_UNKNOWN_DATA_ID, 0); else frame(28, OT_READ_ACK, bo.ret); break;
    case 3:  frame(3, OT_READ_ACK, 0, bo.lowOffDenied ? 0x10 : 0x00, 7); break;
    case 15: frame(15, OT_READ_ACK, 0, (int)bo.qMax, (int)bo.minModPct); break;
    default: frame(id, OT_READ_ACK, 0, 0, 0); break;
  }
}

static void physics(uint32_t dtMs) {
  const float dt = dtMs / 1000.0f;
  const uint32_t now = g_ms;
  if (!bo.flame) {
    if (bo.ch && bo.cs > bo.flow + 0.5f && (uint32_t)(now - bo.flameChangeMs) >= bo.minOffMs) { bo.flame = true; bo.flameChangeMs = now; ev("BOILER", "flame on flow=%.1f cs=%.1f", bo.flow, bo.cs); }
  } else if (!bo.ch || bo.flow >= bo.cs + bo.hystK) { bo.flame = false; bo.flameChangeMs = now; ev("BOILER", "flame off flow=%.1f cs=%.1f", bo.flow, bo.cs); }
  float q = 0;
  if (bo.flame) {
    float m = bo.kp * (bo.cs - bo.flow);
    if (m < bo.minModPct) m = bo.minModPct;
    if (m > (float)bo.mmCap) m = (float)bo.mmCap;
    if (m < bo.minModPct) m = bo.minModPct;               // a burner cannot go below its minimum, whatever MM= asks
    if (m > 100) m = 100;
    bo.relmod = m; q = m / 100.0f * bo.qMax;
  } else bo.relmod = 0;
  const float load = bo.loadK * (bo.flow - bo.roomC);
  bo.flow += (q - load) * dt / bo.capKJperK * 1.0f;   // kW * s / (kJ/K) = K
  if (bo.flow < bo.roomC) bo.flow = bo.roomC;
  bo.ret = bo.flame ? bo.flow - 12.0f : (bo.ret + (bo.flow - 8.0f - bo.ret) * 0.02f);
}

static void weather() {                           // STUB: a fixed outside temperature, refreshed every pass, never stale
  state.sat.weather.bValid = true; state.sat.weather.iLastUpdateMs = g_ms ? g_ms : 1; state.sat.weather.fTemperature = g_outside;
}

static uint32_t rnd() { return (uint32_t)rand(); }
static void loopOnce() {
  uint32_t dt = g_dt + (g_jit ? (rnd() % (g_jit + 1)) : 0);   // the loop period: 10 ms, plus a seeded jitter when asked
  g_ms += dt;
  const uint32_t now = g_ms;
  for (auto& p : pend) if (p.used && (int32_t)(now - p.at) >= 0) { p.used = false; ev("PIC_ECHO", "%s", p.echo); checkCommandResponse(p.echo, (unsigned)strlen(p.echo)); }
  for (auto& p : pma) if (p.used && (int32_t)(now - p.at) >= 0) { p.used = false; sendFrameFor(p.id); }
  physics(dt);
  weather();
  if (!g_linkDown) {
    if ((int32_t)(now - g_nextFlow) >= 0) { g_nextFlow = now + g_flowPer; sendFrameFor(25); }
    if ((int32_t)(now - g_nextRet) >= 0) { g_nextRet = now + g_retPer; sendFrameFor(28); }
    if ((int32_t)(now - g_nextStatus) >= 0) { g_nextStatus = now + 1000; statusFrame(); frame(17, OT_READ_ACK, bo.relmod); }
    if ((int32_t)(now - g_nextCfg) >= 0) { g_nextCfg = now + 20000; sendFrameFor(3); }
    if ((int32_t)(now - g_nextTr) >= 0) { g_nextTr = now + 5000; tr(); }
  }
  g_sent[0] = 0;
  handleCommandQueue();
  { char line[64]; const char* s = g_sent;
    while (sscanf(s, "%63[^;];", line) == 1) { ev("TX", "%s", line); schedEcho(line); boilerGetsCmd(line); s = strchr(s, ';') + 1; } }
  satControlLoop();
  { static int pm = -1, pp = -1;   // edges of the control mode and of the PWM phase (observed from state.sat, not decided here)
    if (state.sat.eControlMode != pm) { pm = state.sat.eControlMode; ev("MODE", "%d", pm); }
    const int ph = (state.sat.eControlMode == SAT_MODE_PWM) ? (state.sat.bPwmFlameRequested ? 1 : 0) : -1;
    if (ph != pp) { pp = ph; ev("PWMPHASE", "%s", ph < 0 ? "none" : (ph ? "ON" : "OFF")); }
    static int pf = -1; if ((int)_flood_active != pf) { pf = (int)_flood_active; ev("FLOOD", "%d", pf); } }
  g_ticks++;
}

static void run(uint32_t untilRelMs) { while ((uint32_t)(g_ms - g_t0) < untilRelMs) loopOnce(); }

static void snap(const char* tag) {
  ev("STATE", "%s mode=%d bActive=%d flame=%d flow=%.1f cs=%.1f relmod=%.0f lastCS=%.1f lastCH=%u lastMM=%u pwmOn=%d otLost=%d outMissing=%d extRefused=%u final=%.1f mod=%d idle=%d flood=%d miSent=%d",
     tag, state.sat.eControlMode, (int)state.sat.bActive, (int)bo.flame, bo.flow, bo.cs, bo.relmod, state.sat.fLastSentCS, (unsigned)state.sat.iLastSentCH,
     (unsigned)state.sat.iLastSentMM, (int)state.sat.bPwmFlameRequested, (int)state.sat.bOtLinkLost, (int)state.sat.bOutsideTempMissing,
     (unsigned)state.sat.iExtCmdRefused, state.sat.fFinalSetpoint, (int)state.sat.iCurrentModulation, (int)_pwm_idle, (int)_flood_active, (int)_sat_miSent);
}

int main() {
  srand(1);
  g_ms = g_t0;
  settings.sat.bEnabled = true;
  settings.sat.fTargetTemp = 20.0f;
  state.net.eMode = state.net.eMode;                                   // (touch)
  state.hw.eMode = HW_MODE_PIC; g_pic = true; state.pic.bAvailable = true;
  g_nextFlow = g_t0 + 2000; g_nextRet = g_t0 + 3000; g_nextStatus = g_t0 + 500; g_nextCfg = g_t0 + 1000; g_nextTr = g_t0 + 700;
  initSAT();
  char line[256];
  while (fgets(line, sizeof line, stdin)) {
    char cmd[32] = {0}, a[64] = {0}; double v = 0;
    const int n = sscanf(line, "%31s %63s %lf", cmd, a, &v);
    if (n < 1) continue;
    v = atof(a);
    if (!strcmp(cmd, "run")) run((uint32_t)atoi(a));
    else if (!strcmp(cmd, "snap")) snap(a);
    else if (!strcmp(cmd, "mark")) ev("MARK", "%s", a);
    else if (!strcmp(cmd, "seed")) srand((unsigned)atoi(a));
    else if (!strcmp(cmd, "jitter")) g_jit = (uint32_t)atoi(a);
    else if (!strcmp(cmd, "link")) g_linkDown = atoi(a) == 0;
    else if (!strcmp(cmd, "mute25")) g_mute25 = atoi(a) != 0;
    else if (!strcmp(cmd, "muteret")) g_muteRet = atoi(a) != 0;
    else if (!strcmp(cmd, "ret28unknown")) { g_ret28Unknown = atoi(a) != 0; g_unsup28 = g_ret28Unknown; }
    else if (!strcmp(cmd, "pmanswers")) g_pmAnswers = atoi(a) != 0;
    else if (!strcmp(cmd, "echomute")) g_echoMute = atoi(a) != 0;
    else if (!strcmp(cmd, "echolat")) g_echoLat = (uint32_t)atoi(a);
    else if (!strcmp(cmd, "flowper")) g_flowPer = (uint32_t)atoi(a);
    else if (!strcmp(cmd, "retper")) g_retPer = (uint32_t)atoi(a);
    else if (!strcmp(cmd, "outside")) g_outside = (float)v;
    else if (!strcmp(cmd, "room")) g_room = (float)v;
    else if (!strcmp(cmd, "lowoff")) bo.lowOffDenied = atoi(a) != 0;
    else if (!strcmp(cmd, "qmin")) bo.minModPct = (float)v;
    else if (!strcmp(cmd, "loadk")) bo.loadK = (float)v;
    else if (!strcmp(cmd, "flow")) bo.flow = (float)v;
    else if (!strcmp(cmd, "user")) { const char* c = line + strlen("user "); char cc[32]; snprintf(cc, sizeof cc, "%s", c); cc[strcspn(cc, "\r\n")] = 0;
                                      const CmdQueueResult r = addCommandToQueue(cc, (int)strlen(cc), false, 0, false); ev("USER", "%s %s", cc, rname(r)); }
    else if (!strcmp(cmd, "fill")) { char cc[8]; for (int k = 0; k < atoi(a); k++) { snprintf(cc, sizeof cc, "%c%c=1", 'A' + k / 6, 'A' + k % 6); addCommandToQueue(cc, (int)strlen(cc), false, 0, false); } ev("QFILL", "size=%d", cmdQueueSize); }
    else if (!strcmp(cmd, "cool")) { bo.flow = (float)v; bo.flame = false; bo.flameChangeMs = g_ms; ev("BOILER", "cooled to %.1f", bo.flow); }   // MODEL action: a cold-water draw or a pause cools the heat exchanger
    else return 2;
  }
  return 0;
}
