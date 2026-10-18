//=======================================================================
// test_mqttBrokerAddress.cpp
//
// Host tests for the broker-address decision of handleMQTT() (TASK-1250),
// compiled against the REAL src/OTGW-firmware/mqttBrokerAddress.h.
//
// The defect: MQTT_STATE_INIT is the only state that looks the broker name
// up, and it wrote a failed lookup (INADDR_NONE, 255.255.255.255 in core
// 2.7.4) over the broker address before checking it. After that the state
// machine went ERROR -> WAIT_FOR_RECONNECT -> TRY_TO_CONNECT and never came
// back to INIT, so one failed lookup (DNS not answering yet after a router
// reboot, at boot or after a WiFi reconnect) kept MQTT down until something
// called startMQTT() again (a WiFi reconnect, a settings save or a reboot).
//
// What is real here: decideMqttBrokerLookup(). What the test supplies: a DNS
// that answers or not, a broker that accepts a connection only on its real
// address, and the plain state transitions of handleMQTT() around that
// decision (INIT, ERROR, WAIT_FOR_RECONNECT back to INIT, TRY_TO_CONNECT with
// its 5-attempt limit, startMQTT() resetting to INIT). Those transitions are
// written out below so a reader can compare them with MQTTstuff.ino; the
// test does not compile handleMQTT() itself. The address validity check
// mirrors isValidIP() for the addresses used here.
//=======================================================================

#include "../../src/OTGW-firmware/mqttBrokerAddress.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

static void check(bool condition, const char* what) {
  g_checks++;
  if (condition) {
    std::printf("  [ OK ] %s\n", what);
  } else {
    g_failures++;
    std::printf("  [FAIL] %s\n", what);
  }
}

static const uint32_t ADDR_NONE  = 0xFFFFFFFFu;  // INADDR_NONE: what a failed lookup returns
static const uint32_t ADDR_UNSET = 0;            // IPAddress default before any lookup
static const uint32_t ADDR_A     = 0xC0A801FAu;  // 192.168.1.250, broker "broker-a"
static const uint32_t ADDR_A2    = 0xC0A801FBu;  // 192.168.1.251, broker "broker-a" after a move
static const uint32_t ADDR_B     = 0xC0A80114u;  // 192.168.1.20,  broker "broker-b"

static bool validAddr(uint32_t a) { return a != ADDR_NONE && a != ADDR_UNSET; }

enum State { INIT, TRY, WAIT, ERR, CONNECTED };

// The network: whether DNS answers, and where each broker name really lives.
struct Net {
  bool        dnsAnswers = true;
  std::string brokerAName = "broker-a";
  uint32_t    brokerAAddr = ADDR_A;

  uint32_t addressOf(const std::string& name) const {
    if (name == brokerAName) return brokerAAddr;
    if (name == "broker-b")  return ADDR_B;
    return ADDR_NONE;
  }
  uint32_t lookup(const std::string& name) const { return dnsAnswers ? addressOf(name) : ADDR_NONE; }
  // Every broker that is up accepts a connection, the previous one after a
  // rename included: connecting to the wrong broker looks like success.
  bool isLiveBroker(uint32_t a) const { return a == brokerAAddr || a == ADDR_B; }
};

// One device driven one handleMQTT() step at a time.
struct Device {
  std::string name = "broker-a";   // settings.mqtt.sBroker
  uint32_t    brokerIP = ADDR_UNSET;
  std::string heldFor;             // MQTTbrokerResolvedFor
  State       state = INIT;
  int         attempts = 0;
  int         lookups = 0;
  uint32_t    connectedTo = ADDR_UNSET;

  void startMQTT() { state = INIT; connectedTo = ADDR_UNSET; }

  void step(const Net& net) {
    switch (state) {
      case INIT: {
        lookups++;
        const uint32_t lookedUp = net.lookup(name);
        const MqttBrokerLookup d = decideMqttBrokerLookup(validAddr(lookedUp), validAddr(brokerIP),
                                                          heldFor.c_str(), name.c_str());
        if (d.bUseResult) { brokerIP = lookedUp; heldFor = name; }
        if (d.bUsable) { attempts = 0; state = TRY; } else { state = ERR; }
        break;
      }
      case TRY:
        attempts++;
        if (net.isLiveBroker(brokerIP)) { state = CONNECTED; connectedTo = brokerIP; break; }
        state = (attempts >= 5) ? WAIT : TRY;   // back-off waits are not modelled
        break;
      case ERR:
        state = WAIT;
        break;
      case WAIT:   // one call = the 42 s wait has run out: look the name up again
        state = INIT;
        break;
      case CONNECTED:
        break;
    }
  }

  bool runUntilConnected(int budget, const Net& net) {
    for (int i = 0; i < budget && state != CONNECTED; i++) step(net);
    return state == CONNECTED;
  }
};

int main() {
  std::printf("mqttBrokerAddress host tests\n");

  //-------------------------------------------------------------------
  std::printf("decideMqttBrokerLookup\n");
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(true, false, "", "broker-a");
    check(d.bUseResult && d.bUsable, "valid result, nothing held: take it, usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(true, true, "broker-a", "broker-a");
    check(d.bUseResult && d.bUsable, "valid result, address held: take the new one, usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(false, true, "broker-a", "broker-a");
    check(!d.bUseResult, "failed lookup does not overwrite a good address");
    check(d.bUsable, "failed lookup with a good address for this name: still usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(false, false, "", "broker-a");
    check(!d.bUsable, "failed lookup, nothing held: not usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(false, true, "broker-a", "broker-b");
    check(!d.bUsable, "failed lookup after a rename: the old broker's address is not used");
  }

  //-------------------------------------------------------------------
  std::printf("scenario: boot while DNS does not answer, DNS answers later\n");
  {
    Net net; net.dnsAnswers = false;
    Device dev;
    dev.runUntilConnected(40, net);
    check(dev.state != CONNECTED, "no connection while DNS is down");
    const int lookupsBefore = dev.lookups;
    net.dnsAnswers = true;
    const bool connected = dev.runUntilConnected(40, net);
    check(dev.lookups > lookupsBefore, "the name is looked up again once DNS is back");
    check(connected, "MQTT connects without a reboot once DNS answers");
  }

  //-------------------------------------------------------------------
  std::printf("scenario: connected, WiFi reconnect, DNS not answering at that moment\n");
  {
    Net net;
    Device dev;
    dev.runUntilConnected(10, net);
    check(dev.state == CONNECTED, "first connect works");
    net.dnsAnswers = false;
    dev.startMQTT();                     // refreshServicesAfterWifiReconnect()
    dev.step(net);                       // the lookup in INIT fails
    check(dev.brokerIP == ADDR_A, "the last good broker address survives the failed lookup");
    check(dev.runUntilConnected(10, net), "MQTT reconnects on the last good address while DNS is still down");
  }

  //-------------------------------------------------------------------
  std::printf("scenario: broker renamed while DNS does not answer\n");
  {
    Net net;
    Device dev;
    dev.runUntilConnected(10, net);
    net.dnsAnswers = false;
    dev.name = "broker-b";               // settings save, startMQTT()
    dev.startMQTT();
    dev.runUntilConnected(40, net);
    check(dev.connectedTo != ADDR_A, "never connects to the previous broker after a rename");
    net.dnsAnswers = true;
    check(dev.runUntilConnected(40, net) && dev.connectedTo == ADDR_B,
          "connects to the renamed broker once DNS answers, without a reboot");
  }

  //-------------------------------------------------------------------
  std::printf("scenario: broker moves to a new address while the old one is held\n");
  {
    Net net;
    Device dev;
    dev.runUntilConnected(10, net);
    net.brokerAAddr = ADDR_A2;           // new DHCP lease for the broker host
    dev.state = WAIT;                    // the connection to the old address dropped
    check(dev.runUntilConnected(40, net) && dev.connectedTo == ADDR_A2,
          "the name is looked up again after the wait and the new address is used");
  }

  std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
