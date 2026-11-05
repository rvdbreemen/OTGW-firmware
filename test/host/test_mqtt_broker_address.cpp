//=======================================================================
// test_mqtt_broker_address.cpp
//
// TASK-1451 host harness, driven by test_mqtt_broker_address.py. Compiles
//  - the REAL header src/OTGW-firmware/mqttBrokerAddress.h (decideMqttBrokerLookup), and
//  - the REAL `case MQTT_STATE_INIT:` block of handleMQTT() in MQTTstuff.ino,
//    sliced by anchor from the revision under test (mqtt_init.inc).
// Only the surroundings are doubled: the DNS shim (platformResolveHost, which zeroes its
// target on failure like the real one), the MQTT client, IPAddress and the debug macros.
//
// Cases (CASE <id> pass|FAIL):
//   D1-D6  decideMqttBrokerLookup() truth table (header only)
//   H1  first lookup ok -> address adopted, TRY_TO_CONNECT, no disconnect
//   H2  same name, lookup fails, held address valid -> held address kept, TRY_TO_CONNECT,
//       'using the last known address' logged                                (Gap 1)
//   H3  renamed broker, lookup fails, session connected -> ERROR, exactly one GRACEFUL disconnect, no stale address (Gap 1+2)
//   H4  never resolved, lookup fails -> ERROR (control)
//   H5  rename, lookup ok, then the same name fails -> new address kept (name tracked on adoption)
//   H7  client still CONNECTING to the held address A, rename to an unresolvable B -> ERROR, FORCED disconnect
//   H6  rename A -> B, lookup of B fails, rename back to A, lookup fails -> ERROR, never B's address
//=======================================================================
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

// ---- doubles ------------------------------------------------------------
#define PSTR(x) x
#define F(x) x
#define CSTR(x) (x)
static size_t strlcpy(char *dst, const char *src, size_t n) {
  size_t l = strlen(src);
  if (n) { size_t c = l >= n ? n - 1 : l; memcpy(dst, src, c); dst[c] = 0; }
  return l;
}
#define snprintf_P snprintf

struct IPAddress {
  uint32_t v = 0;
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : v(a | (b << 8) | (c << 16) | ((uint32_t)d << 24)) {}
  uint8_t operator[](int i) const { return (v >> (8 * i)) & 0xFF; }
};
static bool isValidIP(const IPAddress &ip) { return ip.v != 0 && ip.v != 0xFFFFFFFFu; }

// scripted DNS: name -> address, absent = failure. Like the real shim it zeroes the target first.
static std::vector<std::pair<std::string, IPAddress>> gDns;
static bool platformResolveHost(const char *name, IPAddress &out) {
  out = IPAddress();
  for (auto &e : gDns) if (e.first == name) { out = e.second; return true; }
  return false;
}

struct { struct { char sBroker[65]; char sUser[32]; char sPasswd[32]; int iBrokerPort = 1883; } mqtt; } settings;

static std::vector<std::string> gLog;
static void logf(const char *fmt, const char *a = "", const char *b = "") {
  char buf[256]; snprintf(buf, sizeof(buf), fmt, a, b); gLog.push_back(buf);
}
// the INIT block calls MQTTDebugTf / MQTTDebugTln with a format and up to 4 args
#define MQTTDebugTf(fmt, ...) do { char _b[256]; snprintf(_b, sizeof(_b), fmt, ##__VA_ARGS__); gLog.push_back(_b); } while (0)
#define MQTTDebugTln(x) do { } while (0)

enum FakeState { FS_DISCONNECTED, FS_CONNECTING, FS_CONNECTED };
struct FakeClient {
  FakeState state = FS_DISCONNECTED;
  int disconnectCalls = 0;      // all calls that acted
  int gracefulCalls = 0;
  int forcedCalls = 0;
  IPAddress server;
  int setServerCalls = 0;
  bool connected() const { return state == FS_CONNECTED; }
  bool disconnected() const { return state == FS_DISCONNECTED; }
  // real semantics (espMqttClient MqttClient.cpp): disconnect(false) acts only when connected,
  // disconnect(true) acts whenever the state is not disconnected
  void disconnect(bool force = false) {
    if (force ? state != FS_DISCONNECTED : state == FS_CONNECTED) {
      disconnectCalls++; (force ? forcedCalls : gracefulCalls)++;
      state = FS_DISCONNECTED;
    }
  }
  void setServer(IPAddress ip, int) { server = ip; setServerCalls++; }
  void setKeepAlive(int) {}
  void setCleanSession(bool) {}
  void setClientId(const char *) {}
  void setWill(const char *, int, bool, const uint8_t *, int) {}
  void setCredentials(const char *, const char *) {}
} MQTTclient;
static void doMqttDisconnect() { if (MQTTclient.connected()) MQTTclient.disconnect(); }
struct { void macAddress(uint8_t *) {} } WiFi;
enum MQTTState { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_ERROR };
static MQTTState stateMQTT = MQTT_STATE_INIT;
static int reconnectAttempts = 0;
static char MQTTclientId[64];
static char MQTTPubNamespace[64] = "ns";
#define MQTT_NAME_DEFAULT "otgw"
#define RESTART_TIMER(x) do { } while (0)
static int timerMQTTwaitforconnect;

// ---- code under test ----------------------------------------------------
#include "mqttBrokerAddress.h"

static IPAddress MQTTbrokerIP;
static char MQTTbrokerIPchar[20];
static char MQTTbrokerResolvedFor[sizeof(settings.mqtt.sBroker)];

static void runInit() {
  stateMQTT = MQTT_STATE_INIT;
  gLog.clear();
  switch (stateMQTT) {
    case MQTT_STATE_INIT:
#include "mqtt_init.inc"
    default: break;
  }
}

// ---- harness ------------------------------------------------------------
static int failures = 0;
static void report(const char *id, bool ok) {
  printf("CASE %s %s\n", id, ok ? "pass" : "FAIL");
  if (!ok) failures++;
}
static bool logHas(const char *s) {
  for (auto &l : gLog) if (l.find(s) != std::string::npos) return true;
  return false;
}
static void reset(const char *name) {
  gDns.clear();
  MQTTbrokerIP = IPAddress();
  MQTTbrokerResolvedFor[0] = 0;
  MQTTbrokerIPchar[0] = 0;
  MQTTclient = FakeClient();
  strlcpy(settings.mqtt.sBroker, name, sizeof(settings.mqtt.sBroker));
}
static const IPAddress IP_A(192, 168, 1, 10), IP_B(10, 0, 0, 5);

int main() {
  // header truth table
  {
    MqttBrokerLookup d;
    d = decideMqttBrokerLookup(true, false, "", "x");
    report("D1", d.bUseResult && d.bUsable);
    d = decideMqttBrokerLookup(false, true, "x", "x");
    report("D2", !d.bUseResult && d.bUsable);
    d = decideMqttBrokerLookup(false, true, "x", "y");
    report("D3", !d.bUseResult && !d.bUsable);
    d = decideMqttBrokerLookup(false, false, "x", "x");
    report("D4", !d.bUseResult && !d.bUsable);
    d = decideMqttBrokerLookup(true, true, "x", "y");
    report("D5", d.bUseResult && d.bUsable);
    d = decideMqttBrokerLookup(false, true, nullptr, "x");
    report("D6", !d.bUseResult && !d.bUsable);
  }
  // H1
  reset("a.local"); gDns = {{"a.local", IP_A}};
  runInit();
  report("H1", stateMQTT == MQTT_STATE_TRY_TO_CONNECT && MQTTbrokerIP.v == IP_A.v && MQTTclient.server.v == IP_A.v
               && strcmp(MQTTbrokerIPchar, "192.168.1.10") == 0);
  // H2: DNS goes away, same name
  gDns.clear();
  runInit();
  report("H2", stateMQTT == MQTT_STATE_TRY_TO_CONNECT && MQTTbrokerIP.v == IP_A.v && MQTTclient.server.v == IP_A.v
               && logHas("lookup of [a.local] failed, using the last known address 192.168.1.10"));
  // H3: rename, lookup fails
  strlcpy(settings.mqtt.sBroker, "b.local", sizeof(settings.mqtt.sBroker));
  MQTTclient.state = FS_CONNECTED;
  runInit();
  report("H3", stateMQTT == MQTT_STATE_ERROR && MQTTclient.gracefulCalls == 1 && MQTTclient.forcedCalls == 0
               && !isValidIP(MQTTbrokerIP));
  // H4
  reset("c.local");
  runInit();
  report("H4", stateMQTT == MQTT_STATE_ERROR && !isValidIP(MQTTbrokerIP));
  // H5
  reset("a.local"); gDns = {{"a.local", IP_A}};
  runInit();
  strlcpy(settings.mqtt.sBroker, "b.local", sizeof(settings.mqtt.sBroker));
  gDns = {{"b.local", IP_B}};
  runInit();
  bool ok5 = stateMQTT == MQTT_STATE_TRY_TO_CONNECT && MQTTbrokerIP.v == IP_B.v;
  gDns.clear();
  runInit();
  report("H5", ok5 && stateMQTT == MQTT_STATE_TRY_TO_CONNECT && MQTTbrokerIP.v == IP_B.v
               && strcmp(MQTTbrokerResolvedFor, "b.local") == 0);
  // H6
  reset("a.local"); gDns = {{"a.local", IP_A}};
  runInit();
  strlcpy(settings.mqtt.sBroker, "b.local", sizeof(settings.mqtt.sBroker));
  gDns = {{"b.local", IP_B}};
  runInit();
  gDns.clear();
  strlcpy(settings.mqtt.sBroker, "a.local", sizeof(settings.mqtt.sBroker));
  runInit();
  report("H6", stateMQTT == MQTT_STATE_ERROR && MQTTclient.server.v == IP_B.v /*untouched since H6 step 2*/
               && !isValidIP(MQTTbrokerIP));
  // H7: CONNECT in flight to A, rename to unresolvable B
  reset("a.local"); gDns = {{"a.local", IP_A}};
  runInit();
  MQTTclient.state = FS_CONNECTING;
  gDns.clear();
  strlcpy(settings.mqtt.sBroker, "b.local", sizeof(settings.mqtt.sBroker));
  runInit();
  report("H7", stateMQTT == MQTT_STATE_ERROR && MQTTclient.forcedCalls == 1 && MQTTclient.gracefulCalls == 0
               && MQTTclient.disconnected() && !isValidIP(MQTTbrokerIP));
  printf("%s\n", failures ? "SOME CASES FAILED" : "ALL CASES PASSED");
  return failures ? 1 : 0;
}
