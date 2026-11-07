// TASK-1664 host harness: PrintMQTTError() from MQTTstuff.ino, compiled against stubs.
// The function and its lastMqttDisconnectReason static are sliced from the source by
// test_mqtt_print_error.py into mqtt_print_error.inc.
//
// P1 connected, refused publish:   must say "not queued", must not say "TCP disconnected"
// C1 disconnected, NOT_AUTHORIZED: must say "unauthorized"           (control)
// C2 disconnected, TCP drop:       must say "TCP disconnected"       (control)
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

namespace espMqttClientTypes {
enum class DisconnectReason {
  USER_OK, MQTT_UNACCEPTABLE_PROTOCOL_VERSION, MQTT_IDENTIFIER_REJECTED,
  MQTT_SERVER_UNAVAILABLE, MQTT_MALFORMED_CREDENTIALS, MQTT_NOT_AUTHORIZED,
  TLS_BAD_FINGERPRINT, TCP_DISCONNECTED
};
}

static std::string g_log;
static bool g_connected = false;
struct FakeClient { bool connected() const { return g_connected; } };
static FakeClient MQTTclient;
static unsigned platformMaxFreeBlock() { return 12000; }

#define F(s) (s)
#define PSTR(s) (s)
static void MQTTDebugln() { g_log += "\n"; }
static void MQTTDebugTln(const char* s) { g_log += s; g_log += "\n"; }
static void MQTTDebugTf(const char* fmt, ...) {
  char buf[256];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  g_log += buf;
}

#include "mqtt_print_error.inc"

static int failures = 0;
static void check(const char* id, bool ok) {
  printf("CASE %s %s\n", id, ok ? "pass" : "FAIL");
  if (!ok) failures++;
}
static bool has(const char* s) { return g_log.find(s) != std::string::npos; }

int main() {
  // P1: an earlier TCP drop left the default reason; now connected, a publish is refused.
  g_log.clear(); g_connected = true;
  lastMqttDisconnectReason = espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED;
  PrintMQTTError();
  check("P1", has("not queued") && !has("TCP disconnected"));

  g_log.clear(); g_connected = false;
  lastMqttDisconnectReason = espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED;
  PrintMQTTError();
  check("C1", has("unauthorized"));

  g_log.clear(); g_connected = false;
  lastMqttDisconnectReason = espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED;
  PrintMQTTError();
  check("C2", has("TCP disconnected"));

  return failures ? 1 : 0;
}
