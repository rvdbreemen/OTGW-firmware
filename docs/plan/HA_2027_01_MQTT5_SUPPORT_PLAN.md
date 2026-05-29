# Home Assistant 2027.1 MQTT5 Support Plan

Date: 2026-05-27
Milestone: HA-2027.01-MQTT5-Support
Backlog task: TASK-734
Scope: planning only; no firmware or library implementation changes.

## Planning Position

Home Assistant is warning that its own MQTT integration broker connection is moving from MQTT protocol 3.1.1 to MQTT protocol 5 before Home Assistant 2027.1.0. That warning does not automatically require OTGW-firmware to change.

Current project position:

- OTGW-firmware currently uses PubSubClient through `src/OTGW-firmware/MQTTstuff.ino`.
- The local PubSubClient copy supports MQTT 3.1 and MQTT 3.1.1 only, defaulting to 3.1.1.
- The main local build path is ESP8266 Arduino Core 2.7.4 through `build.py`.
- Mosquitto supports MQTT 5.0, 3.1.1, and 3.1, so Home Assistant can use MQTT 5 while OTGW continues using MQTT 3.1.1.

Decision for now:

1. Do not change OTGW firmware only because this Home Assistant repair warning appears.
2. Keep PubSubClient/MQTT 3.1.1 as the production default.
3. Track MQTT5 support as planned future compatibility work under `HA-2027.01-MQTT5-Support`.
4. Only implement MQTT5 if we have a concrete requirement for OTGW to connect to an MQTT5-only broker/listener, or if we intentionally decide to advertise OTGW MQTT5 client support.

## Goals

- Keep the current ESP8266/D1 mini firmware stable.
- Avoid broad MQTT rewrites unless there is a real compatibility requirement.
- Preserve Home Assistant discovery, retained payloads, availability/LWT behavior, `homeassistant/status` handling, and command subscriptions.
- Have a ready plan if MQTT5 support becomes required.

## Non-Goals

- Do not implement MQTT5 in this planning task.
- Do not replace PubSubClient in the current firmware as a first move.
- Do not add full MQTT5 feature support unless a separate product requirement is accepted.
- Do not create ESP32-only MQTT behavior that breaks the ESP8266 path.

## Support Options

| Option | ESP8266 Core 2.7.4 | ESP32 | MQTT5 | Plan decision |
| --- | --- | --- | --- | --- |
| Keep PubSubClient MQTT 3.1.1 | Yes | Yes | No | Default production path. |
| Minimal PubSubClient MQTT5 fork | Likely | Likely | Basic only | Preferred ESP8266-compatible spike if MQTT5 is required. |
| Espressif ESP-MQTT | No | Yes | Yes | Preferred future ESP32-first path. |
| H4AsyncMQTT | Claimed | Claimed | Claimed | Spike only; async stack shift is high impact. |
| ESP32_MQTTv5 | No | Yes | Yes | ESP32 experiment only; low adoption signal. |
| robotutor-tech/mqtt-client | Claimed | Claimed | Claimed | Not a main path; maturity risk. |
| espMqttClient | Yes | Yes | No | Not useful for MQTT5 support. |
| PicoMQTT | No, needs newer core | Yes | No | Not useful for this milestone. |
| ArduinoMqttClient / 256dpi arduino-mqtt | Varies | Varies | No clear MQTT5 path | Not useful for this milestone. |

## Phased Plan

### Phase 0: User Guidance And Triage

Purpose: avoid unnecessary firmware work.

Actions:

- Add a short troubleshooting/release-note entry explaining the Home Assistant repair warning.
- State that the warning is for Home Assistant's broker connection.
- State that standard Mosquitto setups can allow HA to use MQTT5 while OTGW stays on MQTT 3.1.1.
- Tell users to migrate the Home Assistant MQTT integration setting first.

Exit gate:

- There is no firmware change unless a user can show OTGW failing against an MQTT5-only broker/listener.

### Phase 1: Requirements Gate

Purpose: decide whether MQTT5 is actually needed in firmware.

Trigger this phase only if one of these is true:

- A supported broker setup rejects MQTT 3.1.1 clients.
- Home Assistant or the official Mosquitto add-on announces a future MQTT5-only client/device requirement.
- The project explicitly chooses MQTT5 as a product capability.

Actions:

- Capture the broker, HA version, Mosquitto version, and failure logs.
- Confirm whether the broker accepts mixed protocol versions.
- Confirm whether OTGW's MQTT discovery and command topics fail because of protocol version, not credentials, retained messages, or discovery changes.

Exit gate:

- A concrete MQTT5 firmware need is documented before implementation starts.

### Phase 2: ESP8266-Compatible PubSubClient MQTT5 Spike

Purpose: prove whether the smallest possible PubSubClient change can support basic MQTT5.

Implementation boundaries:

- Add compile-time `MQTT_VERSION_5`, default off.
- Keep MQTT 3.1.1 as the default build.
- Implement "MQTT5 framing with zero properties", not full MQTT5 feature support.
- Preserve the existing OTGW `PubSubClient` API usage.

Packet work to spike:

- CONNECT: protocol level 5, clean-start mapping, keepalive, username/password, LWT, CONNECT properties length, Will Properties length.
- CONNACK: parse flags, reason code, and properties; success only on reason code `0x00`.
- PUBLISH outbound: add MQTT5 properties length before payload.
- PUBLISH inbound: decode and skip MQTT5 properties before invoking the existing callback.
- SUBSCRIBE and UNSUBSCRIBE: add zero property length after packet id.
- SUBACK and UNSUBACK: tolerate MQTT5 reason-code/property shape.
- DISCONNECT: keep fixed-header clean disconnect unless diagnostics are needed.

Out of scope for the spike:

- User properties.
- Topic aliases.
- Response topic and correlation data.
- Message expiry.
- Payload format indicator.
- Enhanced authentication.
- Shared subscription helper API.
- Subscription identifiers exposed to application code.
- Server redirection.
- Full QoS2 support.
- Dynamic buffer growth.

Exit gate:

- MQTT5 opt-in build connects, publishes, subscribes, receives commands, handles `homeassistant/status`, and reconnects against Mosquitto.
- MQTT 3.1.1 default build behavior remains unchanged.
- ESP8266 heap and buffer behavior remain acceptable.

### Phase 3: Productize MQTT5 Opt-In

Purpose: make the spike maintainable if it passes.

Actions:

- Keep MQTT 3.1.1 default.
- Add a clearly named compile-time or build-profile opt-in for MQTT5.
- Document "basic MQTT5 compatibility only; no MQTT5 properties API."
- Add packet construction/parser tests if practical in the local test harness.
- Add hardware validation notes for ESP8266.
- Add ESP32 validation only after ESP8266 behavior is stable.

Exit gate:

- Users can choose MQTT5 knowingly.
- Default users see no behavior change.
- Tests catch packet layout regressions.

### Phase 4: ESP32 Future Path

Purpose: avoid locking ESP32 into an ESP8266-driven PubSubClient fork if ESP32 becomes primary.

Actions:

- Evaluate Espressif ESP-MQTT for ESP32-first firmware.
- Compare behavior against the current OTGW MQTT surface: LWT, discovery streaming, subscriptions, reconnect, and command callbacks.
- Keep ESP32-specific MQTT work isolated unless the project chooses to split MQTT implementations by target.

Exit gate:

- ESP32 has a clean MQTT5 path without destabilizing ESP8266.

## Validation Gates

For any future firmware implementation:

- Build firmware and filesystem image in the same validation pass.
- Verify default MQTT 3.1.1 behavior against Mosquitto.
- Verify MQTT5 opt-in behavior against Mosquitto with broker logs or packet capture.
- Verify Home Assistant discovery payloads and retained state.
- Verify `homeassistant/status` online/offline handling.
- Verify command topics still reach OTGW.
- Verify LWT/availability topic behavior.
- Verify reconnect after broker restart.
- Verify wrong credentials produce a useful failure state.
- Measure ESP8266 free heap and max block before connect, after connect, after subscribe, and after discovery publish.

## Risks

- MQTT5 parser offsets are easy to get wrong because property lengths appear in several packet types.
- Even "zero properties" requires correct variable-byte-integer decoding for inbound packets.
- PubSubClient has tight packet-buffer assumptions; one extra byte can expose edge cases.
- Reason-code mapping does not match PubSubClient's existing MQTT 3 state model.
- A partial MQTT5 implementation can mislead users unless the limits are documented clearly.
- Maintaining a local PubSubClient protocol fork has long-term cost.

## Proposed Backlog Breakdown

No follow-up tasks are created by this plan. If the milestone moves forward, create separate tasks in this order:

1. Document Home Assistant MQTT5 repair guidance for OTGW users.
2. Build a PubSubClient MQTT5 spike behind an opt-in compile flag.
3. Add MQTT packet layout tests or broker-log validation harness.
4. Hardware-validate MQTT5 opt-in on ESP8266.
5. Evaluate ESP-MQTT for ESP32-first MQTT5 support.

## Planning Sources

- User-provided Home Assistant repair screenshot, 2026-05-27.
- Home Assistant MQTT integration documentation: https://www.home-assistant.io/integrations/mqtt/
- Eclipse Mosquitto broker man page: https://mosquitto.org/man/mosquitto-8.html
- MQTT specification index: https://mqtt.org/mqtt-specification/
- PubSubClient upstream README: https://github.com/knolleary/pubsubclient
- Espressif ESP-MQTT documentation: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/protocols/mqtt.html
- H4AsyncMQTT repository: https://github.com/HamzaHajeir/H4AsyncMQTT
- ESP32_MQTTv5 repository: https://github.com/JorgeGBeltre/ESP32_MQTTv5
- robotutor-tech MQTT client repository: https://github.com/robotutor-tech/mqtt-client
- espMqttClient documentation: https://www.emelis.net/espMqttClient/
- PicoMQTT repository: https://github.com/mlesniew/PicoMQTT
- ArduinoMqttClient repository: https://github.com/arduino-libraries/ArduinoMqttClient
- 256dpi arduino-mqtt repository: https://github.com/256dpi/arduino-mqtt
