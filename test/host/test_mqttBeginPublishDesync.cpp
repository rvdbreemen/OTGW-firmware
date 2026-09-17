//=======================================================================
// test_mqttBeginPublishDesync.cpp
//
// Proves the defect behind GH #682 and the shape of its remedy, against the
// REAL vendored libraries/PubSubClient/src/PubSubClient.cpp. Nothing about
// PubSubClient is reimplemented here; only the platform underneath it
// (test/host/pubsub_shim/) and the TCP client are emulated.
//
// The defect: PubSubClient::beginPublish() writes the fixed header and the
// topic with a single _client->write() and returns only whether the full count
// went out. On ESP8266 that write returns a partial count once the lwIP send
// buffer stays full until the socket timeout (core 2.7.4,
// ClientContext.h::_write_from_source returns _written after _is_timeout()).
// A caller that treats false as "nothing happened" leaves a half-written
// PUBLISH header on a live connection. The broker reads whatever is sent next
// as the payload that header promised and drops the client with
// "malformed packet".
//
// The remedy under test is the caller contract, not a library change: on a
// false from beginPublish(), drop the TCP link. That is what
// beginMqttPublish() (MQTTstuff.ino) and beginDiscoveryPublish()
// (mqtt_configuratie.cpp) now do.
//=======================================================================

#include "pubsub_shim/Arduino.h"
#include "pubsub_shim/Client.h"

#include <cstdio>
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
// A TCP client whose write() accepts only `acceptBytes` bytes per call, the
// way ClientContext returns _written after its send timeout. Everything it
// accepts is recorded, so the test can see what the broker would have read.
// The RX side replays a scripted CONNACK so a real MQTT session can be
// established; beginPublish() is gated on PubSubClient::connected(), which
// reports the session state, not the socket.
//---------------------------------------------------------------------
class ShortWritingClient : public Client {
public:
  size_t acceptBytes = 1024;  // bytes accepted per write() call
  bool   linkUp      = true;  // cleared by stop()
  int    stopCalls   = 0;
  std::vector<uint8_t> wire;  // everything that reached the socket
  std::vector<uint8_t> rx;    // what the broker sends back
  size_t rxPos = 0;

  void queueConnack() {
    rx.push_back(0x20);  // CONNACK
    rx.push_back(0x02);  // remaining length
    rx.push_back(0x00);  // no session present
    rx.push_back(0x00);  // accepted
  }

  int connect(IPAddress, uint16_t) override { linkUp = true; return 1; }
  int connect(const char*, uint16_t) override { linkUp = true; return 1; }

  size_t write(uint8_t b) override { return write(&b, 1); }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!linkUp) return 0;
    const size_t n = (size < acceptBytes) ? size : acceptBytes;
    for (size_t i = 0; i < n; i++) wire.push_back(buf[i]);
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

// Bring the client into a live MQTT session, then clear the record so each
// case only sees the bytes its own publish produced.
static bool openSession(PubSubClient& mqtt, ShortWritingClient& net) {
  net.queueConnack();
  const bool ok = mqtt.connect("OTGWhosttest");
  net.wire.clear();
  return ok;
}

//---------------------------------------------------------------------
// The caller contract as the firmware now implements it: any false from
// beginPublish() drops the link. A partial write and a write that never
// started both arrive as false and cannot be told apart from here, so the
// link goes down either way.
//---------------------------------------------------------------------
static bool beginPublishOrDropLink(PubSubClient& mqtt, const char* topic, size_t payloadLen) {
  if (mqtt.beginPublish(topic, payloadLen, true)) return true;
  mqtt.disconnect();
  return false;
}

int main() {
  std::printf("test_mqttBeginPublishDesync\n");

  const char* kTopic = "OTGW/value/otgw/status_master";
  const char* kPayload = "1";

  //-------------------------------------------------------------------
  // 1. The defect. A short write inside beginPublish() puts bytes on the
  //    wire and still reports failure, and the connection stays up.
  //-------------------------------------------------------------------
  {
    ShortWritingClient net;
    PubSubClient mqtt(net);
    mqtt.setServer(IPAddress(192, 168, 1, 234), 1883);
    check(openSession(mqtt, net), "a session is established before the case runs");

    net.acceptBytes = 8;  // fewer than header + topic, so the header is cut
    const bool started = mqtt.beginPublish(kTopic, std::strlen(kPayload), true);

    check(!started, "beginPublish reports failure on a short header write");
    check(!net.wire.empty(), "the partial header did reach the socket");
    check(net.wire.size() < std::strlen(kTopic),
          "fewer bytes went out than the header alone needs");
    check(net.connected() == 1,
          "the connection is still up, so the next packet appends to a half-written one");

    // What the broker sees next is the defect itself: the following packet is
    // read as the payload the truncated header promised.
    const size_t bytesBeforeNextPacket = net.wire.size();
    net.acceptBytes = 1024;                 // send buffer drains again
    mqtt.publish("OTGW/value/otgw/status_slave", "0");
    check(net.wire.size() > bytesBeforeNextPacket,
          "a following publish is appended to the desynchronised stream");
  }

  //-------------------------------------------------------------------
  // 2. The remedy. The same short write, now through the caller contract the
  //    firmware uses: the link is dropped, so no later packet can be appended
  //    behind the partial header.
  //-------------------------------------------------------------------
  {
    ShortWritingClient net;
    PubSubClient mqtt(net);
    mqtt.setServer(IPAddress(192, 168, 1, 234), 1883);
    check(openSession(mqtt, net), "a session is established before the case runs");

    net.acceptBytes = 8;
    const bool started = beginPublishOrDropLink(mqtt, kTopic, std::strlen(kPayload));

    check(!started, "the wrapper reports failure too");
    check(net.stopCalls == 1, "the wrapper dropped the TCP link");
    check(net.connected() == 0, "the connection is closed");

    // A publish attempted afterwards cannot reach the wire, so the broker
    // never reads anything behind the partial header.
    const size_t bytesAtDrop = net.wire.size();
    net.acceptBytes = 1024;
    mqtt.publish("OTGW/value/otgw/status_slave", "0");
    check(net.wire.size() == bytesAtDrop,
          "nothing is appended after the link was dropped");
  }

  //-------------------------------------------------------------------
  // 3. The wrapper must not drop a healthy connection. A send buffer with
  //    room takes the whole header, and the link stays up.
  //-------------------------------------------------------------------
  {
    ShortWritingClient net;
    PubSubClient mqtt(net);
    mqtt.setServer(IPAddress(192, 168, 1, 234), 1883);
    check(openSession(mqtt, net), "a session is established before the case runs");

    net.acceptBytes = 1024;
    const bool started = beginPublishOrDropLink(mqtt, kTopic, std::strlen(kPayload));

    check(started, "a full header write succeeds");
    check(net.stopCalls == 0, "a healthy connection is not dropped");
    check(net.connected() == 1, "the connection stays up");
  }

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
