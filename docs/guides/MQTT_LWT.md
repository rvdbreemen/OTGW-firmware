# MQTT Last Will and Testament (LWT)

This guide explains how the OTGW-firmware uses MQTT's Last Will and Testament mechanism to provide reliable device availability to Home Assistant and other MQTT consumers.

## The Problem

An MQTT broker has no way to know *why* a client disappears. When the ESP8266 crashes, loses power, or drops its WiFi connection, it cannot publish an "I'm offline" message — it's already gone. LWT solves this by letting the client **pre-register** a message that the broker will publish on the client's behalf when the connection is unexpectedly lost.

## How LWT Works (3 Steps)

### Step 1 — Register the "testament" at connect time

When calling `connect()`, the client tells the broker: *"If I disappear unexpectedly, publish this message on this topic."*

In this codebase, see `MQTTstuff.ino:790-795`:

```cpp
// Without credentials:
MQTTclient.connect(MQTTclientId, MQTTPubNamespace, 0, true, "offline");
//                  clientId       willTopic        QoS retain willMessage

// With credentials:
MQTTclient.connect(MQTTclientId, CSTR(settings.mqtt.sUser), CSTR(settings.mqtt.sPasswd),
                   MQTTPubNamespace, 0, true, "offline");
//                 willTopic        QoS retain willMessage
```

The LWT parameters passed to `connect()`:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `willTopic` | `MQTTPubNamespace` (e.g. `OTGW/value/otgw-AABBCCDDEEFF`) | Topic the testament is published to |
| `willQoS` | `0` | QoS level of the testament |
| `willRetain` | `true` | Message is retained on the broker — new subscribers see the status immediately |
| `willMessage` | `"offline"` | Payload published when the client unexpectedly disappears |

The key insight: **LWT is an agreement made at `connect()` time, not a message you publish yourself.** The broker stores it and only publishes it when the client drops unexpectedly.

### Step 2 — Publish a "birth message" after connecting

Immediately after a successful connection, the firmware publishes `"online"` on the same topic (`MQTTstuff.ino:806-807`):

```cpp
// birth message, sendMQTT retains by default
sendMQTT(MQTTPubNamespace, "online");
```

This overwrites any lingering `"offline"` message. Every subscriber now knows: the device is online.

### Step 3 — Broker publishes the testament on unexpected loss

When the TCP connection drops (crash, power loss, network failure) without a proper `disconnect()`, the **broker itself** publishes `"offline"` to `MQTTPubNamespace` — exactly as agreed in step 1. Because `retain = true`, this message persists until the device comes back online and overwrites it with `"online"`.

## Complete Lifecycle Diagram

```
ESP8266 boot
    |
    v
connect(willTopic="OTGW/value/otgw-XX", willMessage="offline", retain=true)
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

## How Home Assistant Uses This

In the HA auto-discovery configuration (`data/mqttha.cfg`), every entity includes an `avty_t` (availability topic):

```json
{
  "avty_t": "%mqtt_pub_topic%",
  "name": "%hostname%_Flame",
  "stat_t": "%mqtt_pub_topic%/flame"
}
```

Here `avty_t` points to the same topic where the LWT and birth message are published (e.g. `OTGW/value/otgw-AABBCCDDEEFF`). Home Assistant monitors this topic:

- `"online"` — entity is available, values are shown normally
- `"offline"` — entity is marked as "unavailable" in the UI

## The Role of Retain

The `retain = true` flag is essential for this mechanism to work correctly:

1. **Birth message retained** — a subscriber connecting *after* the device is already online still sees `"online"` immediately, without waiting for the next publish cycle.
2. **LWT retained** — a subscriber connecting *after* the device has crashed still sees `"offline"` immediately, rather than assuming the device is available.

Without retain, availability would only be known to subscribers who happened to be connected at the exact moment of the state change.

## Summary Table

| Moment | Who publishes | Topic | Payload | Retain |
|--------|--------------|-------|---------|--------|
| Connection established | ESP8266 (firmware) | `MQTTPubNamespace` | `"online"` | yes |
| Unexpected disconnect | MQTT broker | `MQTTPubNamespace` | `"offline"` | yes |

## Related Code

- **LWT + birth message**: `src/OTGW-firmware/MQTTstuff.ino` (lines 786-807)
- **Availability macro**: `src/OTGW-firmware/OTGW-firmware.h:73` — `CONLINEOFFLINE(x)`
- **HA discovery configs**: `src/OTGW-firmware/data/mqttha.cfg` — `avty_t` field
- **MQTT topic documentation**: `docs/api/MQTT.md` — Connection Lifecycle section
