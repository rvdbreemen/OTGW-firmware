//=======================================================================
// test_mqttBrokerAddress.cpp
//
// Host tests for the broker-address decisions of handleMQTT() (TASK-1250),
// compiled against the REAL src/OTGW-firmware/mqttBrokerAddress.h.
//
// The defect: MQTT_STATE_INIT is the only state that looks the broker name
// up, and it wrote a failed lookup (INADDR_NONE, 255.255.255.255 in core
// 2.7.4) over the broker address before checking it. After that the state
// machine went ERROR -> WAIT_FOR_RECONNECT -> TRY_TO_CONNECT and never came
// back to INIT, so one failed lookup (DNS not answering yet after a router
// reboot, at boot or after a WiFi reconnect) kept MQTT down until a reboot.
//
// What is real here: the two decisions (decideMqttBrokerLookup and
// mqttResolveAfterWait). What the test supplies: the lookup result, a
// connect that succeeds only on the broker's real address, and the plain
// state transitions of handleMQTT() around those decisions (INIT, ERROR,
// WAIT_FOR_RECONNECT, TRY_TO_CONNECT with its 5-attempt limit), written out
// below so a reader can compare them with MQTTstuff.ino. The address
// validity check mirrors isValidIP() for the addresses used here.
//=======================================================================

#include "../../src/OTGW-firmware/mqttBrokerAddress.h"

#include <cstdint>
#include <cstdio>

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

static const uint32_t ADDR_NONE   = 0xFFFFFFFFu;  // INADDR_NONE: what a failed lookup returns
static const uint32_t ADDR_UNSET  = 0;            // IPAddress default before any lookup
static const uint32_t ADDR_BROKER = 0xC0A801FAu;  // 192.168.1.250

static bool validAddr(uint32_t a) { return a != ADDR_NONE && a != ADDR_UNSET; }

enum State { INIT, TRY, WAIT, ERR, CONNECTED };

// One device: the broker address it holds, the state machine, and a DNS that
// answers or not, driven one handleMQTT() step at a time.
struct Device {
  uint32_t brokerIP = ADDR_UNSET;
  State    state    = INIT;
  int      attempts = 0;
  int      lookups  = 0;

  void step(bool dnsAnswers) {
    switch (state) {
      case INIT: {
        lookups++;
        const uint32_t lookedUp = dnsAnswers ? ADDR_BROKER : ADDR_NONE;
        const MqttBrokerLookup d = decideMqttBrokerLookup(validAddr(lookedUp), validAddr(brokerIP));
        if (d.bUseResult) brokerIP = lookedUp;
        if (d.bUsable) { attempts = 0; state = TRY; } else { state = ERR; }
        break;
      }
      case TRY:
        attempts++;
        if (brokerIP == ADDR_BROKER) { state = CONNECTED; break; }
        state = (attempts >= 5) ? WAIT : TRY;   // back-off waits are not modelled
        break;
      case ERR:
        state = WAIT;
        break;
      case WAIT:   // one call = the 42 s wait has run out
        if (mqttResolveAfterWait(validAddr(brokerIP))) state = INIT;
        else { attempts = 0; state = TRY; }
        break;
      case CONNECTED:
        break;
    }
  }

  // Runs steps until connected or the step budget is spent.
  bool runUntilConnected(int budget, bool dnsAnswers) {
    for (int i = 0; i < budget && state != CONNECTED; i++) step(dnsAnswers);
    return state == CONNECTED;
  }
};

int main() {
  std::printf("mqttBrokerAddress host tests\n");

  //-------------------------------------------------------------------
  std::printf("decideMqttBrokerLookup\n");
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(true, false);
    check(d.bUseResult && d.bUsable, "valid result, nothing held: take it, usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(true, true);
    check(d.bUseResult && d.bUsable, "valid result, address held: take the new one, usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(false, true);
    check(!d.bUseResult, "failed lookup does not overwrite a good address");
    check(d.bUsable, "failed lookup with a good address held: still usable");
  }
  {
    const MqttBrokerLookup d = decideMqttBrokerLookup(false, false);
    check(!d.bUsable, "failed lookup, nothing held: not usable");
  }

  //-------------------------------------------------------------------
  std::printf("mqttResolveAfterWait\n");
  check(mqttResolveAfterWait(false), "no usable address after the wait: look the name up again");
  check(!mqttResolveAfterWait(true), "usable address after the wait: connect, no new lookup");

  //-------------------------------------------------------------------
  std::printf("scenario: boot while DNS does not answer, DNS answers later\n");
  {
    Device dev;
    dev.runUntilConnected(40, false);   // DNS down for a long while
    check(dev.state != CONNECTED, "no connection while DNS is down");
    const int lookupsBefore = dev.lookups;
    const bool connected = dev.runUntilConnected(40, true);   // DNS back
    check(dev.lookups > lookupsBefore, "the name is looked up again once DNS is back");
    check(connected, "MQTT connects without a reboot once DNS answers");
  }

  //-------------------------------------------------------------------
  std::printf("scenario: connected, WiFi reconnect, DNS not answering at that moment\n");
  {
    Device dev;
    dev.runUntilConnected(10, true);
    check(dev.state == CONNECTED, "first connect works");
    dev.state = INIT;                    // startMQTT() after a WiFi reconnect
    dev.step(false);                     // the lookup in INIT fails
    check(dev.brokerIP == ADDR_BROKER, "the last good broker address survives the failed lookup");
    check(dev.runUntilConnected(10, false), "MQTT reconnects on the last good address while DNS is still down");
  }

  std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
