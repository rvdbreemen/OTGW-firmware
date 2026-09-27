//=======================================================================
// test_mqttPayloadDesyncDisconnect.cpp
//
// Reproduces the corrupted payloads of GH #682 (TASK-1166) against the REAL
// vendored libraries/PubSubClient/src/PubSubClient.cpp, and checks the remedy
// contract. Only the platform (test/host/pubsub_shim/) and the TCP client are
// emulated; a small broker-side parser reads back what went over the wire.
//
// Field symptom: Home Assistant received b'10.\xe0\x00' on TSet and
// b'O\xe0\x00' on cooling_enable. The length was right and the last two bytes
// were always E0 00, which is the MQTT DISCONNECT packet.
//
// Mechanism: when a payload write comes back short, the firmware abandons the
// PUBLISH by dropping the link. Before TASK-1166 that drop was
// PubSubClient::disconnect(), which writes E0 00 before it closes the socket.
// The broker is still inside the PUBLISH the header announced, so it reads
// those two bytes as payload:
//   2 bytes short  -> the PUBLISH completes with "...E0 00" and is delivered
//   1 byte short   -> E0 ends the payload, 00 is read as the next packet
//   >2 bytes short -> the broker waits for more, gets EOF, drops the frame
//
// The remedy is to close the transport without writing anything, which is
// what mqttDropLinkOnDesync() (mqtt_configuratie.cpp) does since TASK-1166.
// That function is not compiled here (its translation unit pulls in the whole
// discovery module); the remedy cases below apply the same transport stop().
//=======================================================================

#include "pubsub_shim/Arduino.h"
#include "pubsub_shim/Client.h"

#include <cstdio>
#include <string>
#include <vector>

unsigned long g_hostMillis = 0;

#include "../../libraries/PubSubClient/src/PubSubClient.cpp"

static int g_checks = 0;
static int g_failures = 0;

static void check(bool condition, const char* what) {
  g_checks++;
  if (condition) {
    std::printf("  ok   %s\n", what);
  } else {
    g_failures++;
    std::printf("  FAIL %s\n", what);
  }
}

//---------------------------------------------------------------------
// A TCP client with a TOTAL byte budget: it accepts exactly `budget` more
// bytes and then stalls, the way ClientContext returns a short count after
// its send timeout. Everything accepted is recorded as what the broker reads.
//---------------------------------------------------------------------
class BudgetClient : public Client {
public:
  static const size_t kUnlimited = (size_t)-1;
  size_t budget   = kUnlimited;
  bool   linkUp   = true;
  int    stopCalls = 0;
  std::vector<uint8_t> wire;
  std::vector<uint8_t> rx;
  size_t rxPos = 0;

  void queueConnack() {
    const uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
    rx.insert(rx.end(), connack, connack + sizeof(connack));
  }

  int connect(IPAddress, uint16_t) override { linkUp = true; return 1; }
  int connect(const char*, uint16_t) override { linkUp = true; return 1; }

  size_t write(uint8_t b) override { return write(&b, 1); }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!linkUp) return 0;
    const size_t n = (size < budget) ? size : budget;
    wire.insert(wire.end(), buf, buf + n);
    if (budget != kUnlimited) budget -= n;
    return n;
  }

  int available() override { return linkUp ? (int)(rx.size() - rxPos) : 0; }
  int read() override {
    if (!linkUp || rxPos >= rx.size()) return -1;
    return rx[rxPos++];
  }
  int read(uint8_t* buf, size_t size) override {
    size_t n = 0;
    while (n < size) {
      const int c = read();
      if (c < 0) break;
      buf[n++] = (uint8_t)c;
    }
    return (int)n;
  }
  int peek() override {
    if (!linkUp || rxPos >= rx.size()) return -1;
    return rx[rxPos];
  }
  void flush() override {}
  void stop() override { linkUp = false; stopCalls++; }
  uint8_t connected() override { return linkUp ? 1 : 0; }
  operator bool() override { return linkUp; }
};

//---------------------------------------------------------------------
// Broker-side view of the byte stream, then EOF. Returns the PUBLISH frames
// the broker would deliver and whether it hit a malformed packet. A frame
// still incomplete at EOF is discarded, as a broker does on connection loss.
//---------------------------------------------------------------------
struct Delivered { std::string topic; std::string payload; };
struct BrokerView { std::vector<Delivered> frames; bool malformed = false; };

static BrokerView parseAsBroker(const std::vector<uint8_t>& w) {
  BrokerView v;
  size_t pos = 0;
  while (pos < w.size()) {
    const uint8_t hdr = w[pos];
    const uint8_t type = hdr >> 4;
    if (type == 0 || type == 15) { v.malformed = true; break; }  // reserved
    size_t p = pos + 1;
    size_t remaining = 0, mult = 1;
    bool lenDone = false;
    for (int i = 0; i < 4 && p < w.size(); i++) {
      const uint8_t b = w[p++];
      remaining += (b & 0x7F) * mult;
      mult *= 128;
      if (!(b & 0x80)) { lenDone = true; break; }
    }
    if (!lenDone || p + remaining > w.size()) break;   // incomplete at EOF
    if (type == 3) {
      if (remaining < 2) { v.malformed = true; break; }
      const size_t tlen = ((size_t)w[p] << 8) | w[p + 1];
      if (2 + tlen > remaining) { v.malformed = true; break; }
      Delivered d;
      d.topic.assign(w.begin() + p + 2, w.begin() + p + 2 + tlen);
      d.payload.assign(w.begin() + p + 2 + tlen, w.begin() + p + remaining);
      v.frames.push_back(d);
    }
    pos = p + remaining;
  }
  return v;
}

static bool openSession(PubSubClient& mqtt, BudgetClient& net) {
  mqtt.setServer(IPAddress(192, 168, 1, 234), 1883);
  net.queueConnack();
  const bool ok = mqtt.connect("OTGWhosttest");
  net.wire.clear();
  return ok;
}

enum class Drop { PubSubDisconnect, TransportStop };

// Abandon a half-written PUBLISH. The send buffer has drained again by the
// time the drop runs, so whatever the drop writes does reach the broker.
static void dropLink(PubSubClient& mqtt, BudgetClient& net, Drop how) {
  net.budget = BudgetClient::kUnlimited;
  if (how == Drop::PubSubDisconnect) {
    mqtt.disconnect();          // the pre-TASK-1166 remedy
  } else {
    net.stop();                 // the TASK-1166 remedy: close, write nothing
    mqtt.connected();           // lets PubSubClient see the dead socket
  }
}

// The firmware's publish sequence (sendMQTTData): beginPublish, payload write,
// drop the link when the payload write comes back short. `payloadBudget` is
// how many payload bytes the stalled socket still takes.
static void publishWithShortPayload(PubSubClient& mqtt, BudgetClient& net,
                                    const char* topic, const char* payload,
                                    size_t payloadBudget, Drop how) {
  const size_t len = std::strlen(payload);
  if (!mqtt.beginPublish(topic, len, false)) { dropLink(mqtt, net, how); return; }
  net.budget = payloadBudget;
  const size_t written = mqtt.write(reinterpret_cast<const uint8_t*>(payload), len);
  if (written != len) { dropLink(mqtt, net, how); return; }
  mqtt.endPublish();
}

static const char* kTopicTSet = "OTGW/value/OTGW483FDAAA6F35/TSet";
static const char* kTopicCool = "OTGW/value/OTGW483FDAAA6F35/cooling_enable";

int main() {
  std::printf("test_mqttPayloadDesyncDisconnect\n");

  //-------------------------------------------------------------------
  // 1. The defect, byte for byte as the field report shows it.
  //-------------------------------------------------------------------
  std::printf(" defect: PubSubClient::disconnect() as the drop\n");
  {
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    publishWithShortPayload(mqtt, net, kTopicCool, "OFF", 1, Drop::PubSubDisconnect);
    const BrokerView v = parseAsBroker(net.wire);
    check(v.frames.size() == 1 && v.frames[0].topic == kTopicCool &&
          v.frames[0].payload == std::string("O\xE0\x00", 3),
          "\"OFF\" 2 bytes short is delivered as O E0 00 on cooling_enable");
  }
  {
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    publishWithShortPayload(mqtt, net, kTopicTSet, "10.00", 3, Drop::PubSubDisconnect);
    const BrokerView v = parseAsBroker(net.wire);
    check(v.frames.size() == 1 && v.frames[0].topic == kTopicTSet &&
          v.frames[0].payload == std::string("10.\xE0\x00", 5),
          "\"10.00\" 2 bytes short is delivered as 10. E0 00 on TSet");
  }
  {
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    publishWithShortPayload(mqtt, net, kTopicTSet, "10.00", 4, Drop::PubSubDisconnect);
    const BrokerView v = parseAsBroker(net.wire);
    check(v.malformed, "1 byte short: the trailing 00 is read as a malformed packet");
  }

  //-------------------------------------------------------------------
  // 2. The remedy: closing the transport writes nothing, so no shortfall
  //    of any size lets the broker complete the frame.
  //-------------------------------------------------------------------
  std::printf(" remedy: transport stop() as the drop\n");
  const size_t shortfalls[] = {1, 2, 3, 5};   // bytes missing from "10.00"
  for (size_t missing : shortfalls) {
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    publishWithShortPayload(mqtt, net, kTopicTSet, "10.00", 5 - missing, Drop::TransportStop);
    const size_t atDrop = net.wire.size();
    const BrokerView v = parseAsBroker(net.wire);
    char what[96];
    std::snprintf(what, sizeof(what), "%zu byte(s) short: no frame delivered, nothing malformed", missing);
    check(v.frames.empty() && !v.malformed, what);
    check(!mqtt.connected() && mqtt.state() == MQTT_CONNECTION_LOST,
          "PubSubClient reports the session lost, so handleMQTT() reconnects");
    mqtt.publish("OTGW/value/OTGW483FDAAA6F35/Tr", "20.50");
    check(net.wire.size() == atDrop, "nothing reaches the wire after the drop");
  }
  {
    // Header cut short inside beginPublish(): same remedy, same outcome.
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    net.budget = 8;
    publishWithShortPayload(mqtt, net, kTopicCool, "OFF", 3, Drop::TransportStop);
    const BrokerView v = parseAsBroker(net.wire);
    check(v.frames.empty() && !v.malformed, "header short: no frame delivered, nothing malformed");
    check(net.wire.size() == 8, "only the bytes the stalled socket took went out");
  }
  {
    // A healthy link is untouched: the full publish is delivered intact.
    BudgetClient net;
    PubSubClient mqtt(net);
    check(openSession(mqtt, net), "session established");
    publishWithShortPayload(mqtt, net, kTopicCool, "OFF", BudgetClient::kUnlimited, Drop::TransportStop);
    const BrokerView v = parseAsBroker(net.wire);
    check(v.frames.size() == 1 && v.frames[0].payload == "OFF" && net.stopCalls == 0,
          "full write: OFF delivered intact, link kept");
  }

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
