# MQTT Last Will and Testament (LWT)

This guide explains how the OTGW-firmware uses MQTT's Last Will and Testament mechanism to provide reliable device availability to Home Assistant and other MQTT consumers.

## The Problem

An MQTT broker has no way to know *why* a client disappears. When the ESP32-S3 crashes, loses power, or drops its WiFi connection, it cannot publish an "I'm offline" message: it is already gone. LWT solves this by letting the client **pre-register** a message that the broker will publish on the client's behalf when the connection is unexpectedly lost.

## How LWT Works (3 Steps)

### Step 1: Register the "testament" before connecting

The firmware uses the asynchronous espMqttClient library. The testament is set on the client before `connect()`, so the broker receives it in the CONNECT packet: *"If I disappear unexpectedly, publish this message on this topic."*

In this codebase, see the `setServer` block in `MQTTstuff.ino`:

```cpp
MQTTclient.setClientId(MQTTclientId);
MQTTclient.setWill(MQTTPubNamespace, 0, true, (const uint8_t*)"offline", 7);
//                 willTopic        QoS retain willMessage           length
// then setCredentials(...) and, later, MQTTclient.connect()
```

The LWT parameters passed to `setWill()`:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `willTopic` | `MQTTPubNamespace` (e.g. `OTGW/value/otgw-AABBCCDDEEFF`) | Topic the testament is published to |
| `willQoS` | `0` | QoS level of the testament |
| `willRetain` | `true` | Message is retained on the broker, so new subscribers see the status immediately |
| `willMessage` | `"offline"` | Payload published when the client unexpectedly disappears |

The key insight: **LWT is an agreement sent with the CONNECT packet, not a message you publish yourself.** The broker stores it and only publishes it when the client drops unexpectedly.

### Step 2: Publish a "birth message" after connecting

When the broker confirms the connection, `onMqttConnect()` calls `publishBirthOnline()` in `MQTTstuff.ino`. It publishes `"online"`, retained, on the same topic. This publish skips the low-memory gate that other MQTT messages pass, because a lost birth message would leave every entity "unavailable". While connected, the firmware publishes the birth message again every 5 minutes (`timerMQTTbirthreassert`), so a birth that was dropped once heals by itself.

This overwrites any lingering `"offline"` message. Every subscriber now knows: the device is online.

### Step 3: Broker publishes the testament on unexpected loss

When the TCP connection drops (crash, power loss, network failure) without a proper `disconnect()`, the **broker itself** publishes `"offline"` to `MQTTPubNamespace`, as agreed in step 1. Because `retain = true`, this message persists until the device comes back online and overwrites it with `"online"`.

## Complete Lifecycle Diagram

```
ESP32-S3 boot
    |
    v
setWill(willTopic="OTGW/value/otgw-XX", "offline", retain=true), then connect()
    |
    v  connection established
    |
    +---> publish "online" on OTGW/value/otgw-XX  (retained)  <-- birth message
    |
    |   ... normal operation ...
    |
    +---> [crash / power loss / WiFi drops]
    |
    v
Broker detects TCP timeout
    |
    v
Broker publishes "offline" on OTGW/value/otgw-XX  (retained)  <-- LWT
```

## Commands sent while the gateway is reconnecting are lost

The firmware connects with a clean session (`MQTTclient.setCleanSession(true)` in the `setServer` block of `MQTTstuff.ino`) and subscribes to its command topics at QoS 0 (`MQTTclient.subscribe(..., 0)` in `onMqttConnect()`). The broker therefore keeps nothing for the gateway while it is disconnected. A command that Home Assistant publishes to a `set/` topic during a reconnect, even one of a few seconds, is dropped by the broker and never reaches the boiler.

You can see this in Home Assistant: the entity keeps showing the value the gateway reports, not the value you asked for. Send the command again once the availability topic shows `online`.

This is a deliberate choice (TASK-1212 on the 1.x line, GH #682). A persistent session with QoS 1 would keep commands, but Mosquitto queues them without an expiry time, also during an outage of hours. After such an outage the gateway would apply every queued command at once, including a setpoint that is hours old.

This applies to commands that arrive over MQTT. With the opt-in SAT setting "Take over when MQTT is lost" (`SATmqttfallback`, off by default), SAT controls the boiler itself after more than 5 minutes without MQTT; see `docs/api/MQTT.md`.

## How Home Assistant Uses This

The firmware builds the Home Assistant discovery configs itself (`MQTTHaDiscovery.cpp`). Every entity includes an `avty_t` (availability topic), shortened here:

```json
{
  "avty_t": "OTGW/value/otgw-AABBCCDDEEFF",
  "stat_t": "OTGW/value/otgw-AABBCCDDEEFF/flame"
}
```

Here `avty_t` points to the same topic where the LWT and birth message are published (e.g. `OTGW/value/otgw-AABBCCDDEEFF`). Home Assistant monitors this topic:

- `"online"`: entity is available, values are shown normally
- `"offline"`: entity is marked as "unavailable" in the UI

## The Role of Retain

The `retain = true` flag is essential for this mechanism to work correctly:

1. **Birth message retained:** a subscriber connecting *after* the device is already online still sees `"online"` immediately, without waiting for the next publish cycle.
2. **LWT retained:** a subscriber connecting *after* the device has crashed still sees `"offline"` immediately, rather than assuming the device is available.

Without retain, availability would only be known to subscribers who happened to be connected at the exact moment of the state change.

## Summary Table

| Moment | Who publishes | Topic | Payload | Retain |
|--------|--------------|-------|---------|--------|
| Connection established | ESP32-S3 (firmware) | `MQTTPubNamespace` | `"online"` | yes |
| Unexpected disconnect | MQTT broker | `MQTTPubNamespace` | `"offline"` | yes |

## Related Code

- **LWT**: `src/OTGW-firmware/MQTTstuff.ino`, `MQTTclient.setWill(...)` in the `setServer` block
- **Birth message**: `src/OTGW-firmware/MQTTstuff.ino`, `publishBirthOnline()`, called from `onMqttConnect()` and every 5 minutes while connected
- **Availability macro**: `src/OTGW-firmware/OTGW-firmware.h`, `CONLINEOFFLINE(x)`
- **HA discovery configs**: `src/OTGW-firmware/MQTTHaDiscovery.cpp`, `avty_t` field
- **MQTT topic documentation**: `docs/api/MQTT.md`, Connection Lifecycle section
