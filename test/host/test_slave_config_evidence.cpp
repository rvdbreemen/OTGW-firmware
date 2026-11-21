// test/host/test_slave_config_evidence.cpp  (TASK-1857, TASK-1903 item 1)
//
// test_slave_config_evidence.py slices the REAL print_slavememberid() out of OTGW-Core.ino. Only what it
// touches is a test double: the frame, the log/MQTT output, is_value_valid() and the three SAT sinks.

#include <cstdio>
#include <cstdint>
#include <cstdlib>

enum { OTGW_BOILER = 1, OTGW_THERMOSTAT = 2 };
enum { SAT_SRC_UNKNOWN = 0, SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2 };
struct OpenthermData_t {
  int id; int rsptype; uint8_t valueHB; uint8_t valueLB; bool bLocalAnswer; bool bReplayed;
  uint16_t u16() const { return (uint16_t)((valueHB << 8) | valueLB); }
};
OpenthermData_t OTdata;
struct { const char* label; } OTlookupitem = { "SlaveConfigMemberIDcode" };
struct { struct { bool bUseLegacyOtTopics; } mqtt; } settings;
struct { struct { int iDetectedHeatingSource; } sat; } state;
#define F(x) x
static void AddLogf(const char*, ...) {}
static const char* byte_to_binary(uint8_t) { return "00000000"; }
static void sendMQTTData(const char*, const char*) {}
static bool is_value_valid(const OpenthermData_t&, const decltype(OTlookupitem)&) { return true; }
#if defined(_MSC_VER)
static char* utoa(unsigned v, char* buf, int radix) { return _itoa((int)v, buf, radix); }
#else   // TASK-3020: no _itoa outside MSVC
static char* utoa(unsigned v, char* buf, int radix) {
  char t[34]; int n = 0;
  do { unsigned d = v % (unsigned)radix; t[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= (unsigned)radix; } while (v);
  for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
  buf[n] = 0;
  return buf;
}
#endif
static int g_slaveCfg = 0, g_manuf = 0;
static void satNoteSlaveConfig(uint8_t) { g_slaveCfg++; }
static void satDetectManufacturer(uint8_t) { g_manuf++; }

#include "generated/slave_config_evidence.inc"

static int g_failures = 0;
static void check(const char* name, long got, long want) {
  bool ok = got == want;
  std::printf("  %s  %-62s got %ld want %ld\n", ok ? "PASS" : "FAIL", name, got, want);
  if (!ok) g_failures++;
}

static void frame(const char* what, int rsp, bool local, bool replayed, int wantCfg, int wantManuf, int wantSrc) {
  OTdata = { 3, rsp, 0x04, 11, local, replayed };   // HB bit 2 set (cooling supported), MemberID 11 (Remeha)
  g_slaveCfg = 0; g_manuf = 0; state.sat.iDetectedHeatingSource = SAT_SRC_UNKNOWN;
  uint16_t value = 0;
  print_slavememberid(value);
  std::printf("%s\n", what);
  check("satNoteSlaveConfig() calls", g_slaveCfg, wantCfg);
  check("satDetectManufacturer() calls", g_manuf, wantManuf);
  check("state.sat.iDetectedHeatingSource", state.sat.iDetectedHeatingSource, wantSrc);
}

int main() {
  frame("OT-Direct loopback B Read-Ack MsgID 3 (bLocalAnswer)", OTGW_BOILER, true, false, 0, 0, SAT_SRC_UNKNOWN);
  frame("replayed B Read-Ack MsgID 3 (bReplayed)", OTGW_BOILER, false, true, 0, 0, SAT_SRC_UNKNOWN);
  frame("real boiler B Read-Ack MsgID 3", OTGW_BOILER, false, false, 1, 1, SAT_SRC_HEAT_PUMP);
  std::printf(g_failures ? "FAIL: %d\n" : "PASS\n", g_failures);
  return g_failures ? 1 : 0;
}
