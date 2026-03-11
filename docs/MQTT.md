# MQTT Documentation for OpenTherm Gateway (OTGW)

This document provides a comprehensive overview of the MQTT integration in the OTGW firmware, including the topic structure, available commands, system topics, and the Home Assistant (HA) integration.

## 1. Topic Structure and Namespaces

The MQTT topic structure is highly configurable. The base configuration relies on three main placeholders:
- **Base Topic** (`settingMQTTtopTopic`): The root topic for all MQTT messages. The default is `OTGW`.
- **Node ID** (`NodeId` / `%hostname%`): The unique identifier for this gateway, usually its hostname (e.g., `otgw-01`).

Based on these, the firmware sets up three main namespaces:
1. **Readout / Sensor Topics** (Publish): `<Base_Topic>/value/<Node_ID>/`
2. **Command Topics** (Subscribe): `<Base_Topic>/set/<Node_ID>/`
3. **Availability (LWT)**: `<Base_Topic>/value/<Node_ID>`

### LWT (Last Will and Testament)
The topic `<Base_Topic>/value/<Node_ID>` is used as the availability topic. 
- Payload `online` is published upon successful connection to the MQTT broker.
- Payload `offline` is broadcast by the broker if the OTGW disconnects unexpectedly.

---

## 2. Readout Topics (Sensors & Status)

All incoming OpenTherm data and the gateway status are published to the **Readout Namespace**: `<Base_Topic>/value/<Node_ID>/<sensor_name>`.

### Common OpenTherm Sensor Topics
Here is a list of the most common sensor names published:
- `Tr`: Current Room Temperature
- `TrSet`: Room Setpoint Temperature
- `Tboiler`: Boiler Water Temperature
- `Tdhw`: Domestic Hot Water Temperature
- `TOutside`: Outside Temperature
- `Tret`: Return Water Temperature
- `ch_enable`: Central Heating Enable (ON/OFF)
- `dhw_enable`: Domestic Hot Water Enable (ON/OFF)
- `flame`: Flame Status (ON/OFF)
- `fault`: Fault Indicator (ON/OFF)
- `centralheating`: Central Heating Active (ON/OFF)
- `domestichotwater`: Domestic Hot Water Active (ON/OFF)
- `cooling`: Cooling Status (ON/OFF)
- `modulation`: Relative Modulation Level (%)
- `ch_water_pressure`: Central Heating Water Pressure (bar)

*Note: Any valid OpenTherm message intercepted by the gateway will be published as its corresponding short-name string.*

### Separate Source Topics
If `settingMQTTSeparateSources` is enabled, sensors are published with the origin source suffix.
- `_thermostat`
- `_boiler`
- `_gateway`

Example: `<Base_Topic>/value/<Node_ID>/Tr_thermostat`

---

## 3. Command and Set Topics

You can control your heating system by publishing commands to the **Command Namespace**: `<Base_Topic>/set/<Node_ID>/<command>`.

For the `<command>`, you can use **either** the long descriptive name (e.g., `setpoint`) **or** the two-letter OpenTherm Gateway raw command (e.g., `TT`).

The payload of the message contains the value (e.g., Temperature, ON/OFF, or a specific string).

For a complete explanation of all available commands, refer to the [OTGW Firmware Documentation](https://otgw.tclcode.com/firmware.html).

### Complete MQTT Command Table

The OTGW firmware supports three practical ways to send commands over MQTT:

1. **Long topic name + simple payload**: `<Base_Topic>/set/<Node_ID>/setpoint` with payload `21.0`
2. **Two-letter topic name + simple payload**: `<Base_Topic>/set/<Node_ID>/TT` with payload `21.0`
3. **Raw `command` topic + full OTGW command**: `<Base_Topic>/set/<Node_ID>/command` with payload `TT=21.0`

For the dedicated topic forms (1 and 2), the firmware converts the MQTT payload to the native OTGW serial command internally. For the raw `command` topic, the payload is passed through as-is.

The table below documents **all MQTT command topics implemented by this firmware**. Descriptions and allowed values are based on the official OTGW firmware documentation, but rewritten here so this document is usable on its own.

| Long topic | Two-letter topic | MQTT payload | Meaning |
|---|---|---|---|
| `command` | *(raw passthrough)* | Full OTGW command string, for example `TT=21.0`, `PR=A`, `GW=R` | Sends a raw OTGW serial command unchanged. Use this when you want direct access to the original serial command interface, including commands that do not have a dedicated MQTT topic. |
| `setpoint` | `TT` | Temperature from `0.0` to `30.0` | **Temporary room setpoint override.** The thermostat program resumes at the next scheduled setpoint change. A value of `0` cancels the override. |
| `constant` | `TC` | Temperature from `0.0` to `30.0` | **Constant room setpoint override.** Unlike `TT`, this is not automatically cleared by the next scheduled thermostat change. A value of `0` cancels the override. |
| `outside` | `OT` | Temperature from `-40.0` to `64.0`, or a non-numeric value to clear | **Outside temperature override.** Sends an outside temperature to the thermostat. A non-numeric payload clears the configured override. |
| `hotwater` | `HW` | `0`, `1`, `P`, or another single character for auto | **Domestic hot water control.** `0` disables DHW keep-warm, `1` enables it, `P` requests a one-time DHW push, any other single character returns control to the thermostat. |
| `gatewaymode` | `GW` | `0`, `1`, or `R` | **Gateway operating mode.** `0` puts the device in monitor mode, `1` enables normal gateway mode, and `R` resets the OTGW. |
| `setback` | `SB` | Temperature, typically used around setback values such as `15` or `16.5` | **Setback temperature.** Used together with GPIO Home/Away functions. The OTGW website notes this command should be sent last when writing multiple EEPROM-backed configuration commands. |
| `maxchsetpt` | `SH` | Temperature, `0` to release control | **Maximum CH water setpoint.** Overrides the thermostat's maximum central-heating setpoint if the boiler supports it. `0` returns control to the thermostat. |
| `maxdhwsetpt` | `SW` | Temperature, `0` to release control | **Domestic hot water setpoint.** Overrides the DHW setpoint if the boiler supports it. `0` returns control to the thermostat. |
| `maxmodulation` | `MM` | Percentage `0` to `100`, or non-numeric to clear | **Maximum relative modulation override.** Limits boiler modulation. A non-numeric payload clears the setting. |
| `ctrlsetpt` | `CS` | Temperature, `0` to pass through thermostat value | **Primary control setpoint override.** Directly manipulates the control setpoint sent to the boiler. The OTGW documentation warns that values of `8` or higher must be refreshed at least every minute while active. |
| `ctrlsetpt2` | `C2` | Temperature, `0` to pass through thermostat value | **Second heating-circuit control setpoint override.** Same idea as `CS`, but for the second CH circuit. Values of `8` or higher also require periodic refresh. |
| `chenable` | `CH` | `0` or `1` | **Central-heating enable bit.** Used together with external control via `CS`. When `CS=0`, the thermostat controls this bit again. |
| `chenable2` | `H2` | `0` or `1` | **Second-circuit heating enable bit.** Used together with external control via `C2`. When `C2=0`, the thermostat controls this bit again. |
| `ventsetpt` | `VS` | Percentage, commonly `0` to `100`, or non-numeric to clear | **Ventilation setpoint override.** Configures a ventilation override value; a non-numeric payload clears it. |
| `temperaturesensor` | `TS` | `O` or `R` | **External temperature sensor function.** `O` uses the sensor as outside temperature, `R` uses it as return-water temperature. |
| `addalternative` | `AA` | Data-ID from `1` to `255` | **Add alternative Data-ID.** Adds a boiler request to the OTGW's alternative request table for use when an unsupported Data-ID creates an available slot. |
| `delalternative` | `DA` | Data-ID from `1` to `255` | **Delete alternative Data-ID.** Removes one occurrence of a Data-ID from the alternative request table. Repeat if the same Data-ID was added multiple times. |
| `unknownid` | `UI` | Data-ID | **Mark Data-ID as unknown.** Forces the OTGW to treat a Data-ID as unsupported by the boiler, creating room for alternative requests. |
| `knownid` | `KI` | Data-ID | **Mark Data-ID as known again.** Restores normal forwarding of a Data-ID and resets the support-detection counter for it. |
| `priomsg` | `PM` | Data-ID | **One-time priority message.** Schedules a priority request to the boiler at the first available opportunity. |
| `setresponse` | `SR` | `Data-ID:data` or `Data-ID:byte1,byte2` | **Override boiler response to thermostat.** Configures a synthetic response for a specific Data-ID instead of the real boiler response. |
| `clearrespons` | `CR` | Data-ID | **Clear a configured response override.** Removes a response previously set with `SR`. |
| `resetcounter` | `RS` | Counter name such as `HBS`, `HBH`, `HPS`, `HPH`, `WBS`, `WBH`, `WPS`, `WPH` | **Reset a boiler counter.** Clears a supported boiler counter. Exact support depends on the boiler. |
| `ignoretransitations` | `IT` | `0` or `1` | **Ignore transition bouncing.** `0` reports rapid signal bouncing as Error 01, `1` ignores bouncing transitions. The OTGW documentation states that `1` is the default. |
| `overridehb` | `OH` | `0` or `1` | **Override bits in high byte.** Controls whether the FunctionOverride bits are copied into the high byte as well as the low byte. |
| `forcethermostat` | `FT` | `C`, `I`, `S`, or another character to restore autodetect | **Force thermostat model.** `C` = Remeha Celcia 20, `I` = Remeha iSense, `S` = Standard. Any other letter returns to auto-detect. |
| `voltageref` | `VR` | Digit `0` to `9` | **Comparator reference voltage.** Sets the PIC comparator threshold. The normal value depends on PIC type; the OTGW website lists `4` for PIC16F88 and `5` for PIC16F1847. |
| `debugptr` | `DP` | Hex register address, or `0`/`00` to disable | **Debug pointer.** Repeatedly reports the selected file register after each OpenTherm message. Set to zero to disable. |

### Notes for advanced commands

- **`SR` / `setresponse`**: This command overrides the response that the thermostat receives for a specific Data-ID. It is powerful, but it can also make diagnostics harder because the thermostat will no longer see the real boiler response for that message. Use it only when you fully understand the OpenTherm message you are replacing.
- **`VR` / `voltageref`**: This changes the PIC comparator reference voltage, which directly affects OpenTherm signal detection. An incorrect setting can make communication unstable or fail completely. In normal installations, keep the documented default for your PIC type unless you are troubleshooting hardware-level signal issues.
- **`DP` / `debugptr`**: This enables repeated low-level register reporting after every OpenTherm message. It is mainly a diagnostic tool for advanced debugging. Leave it disabled during normal use because it increases serial/debug output and is not needed for regular heating control.

### Examples

**Example 1: Setting a temporary room temperature using the long name topic**
Publish payload `21.0` to `<Base_Topic>/set/<Node_ID>/setpoint`
*(Sets the temporary room temperature to 21.0 °C)*

**Example 2: Setting a temporary room temperature using the two-letter raw command in the payload**
Publish payload `TT=21.0` to `<Base_Topic>/set/<Node_ID>/command`
*(Identical outcome to Example 1, but uses the universal `command` topic with the raw string payload)*

**Example 3: Setting a temporary room temperature using the two-letter topic (The 3rd Way)**
Publish payload `21.0` to `<Base_Topic>/set/<Node_ID>/TT`
*(The firmware allows the raw command identifier directly in the topic path)*

**Example 4: Enabling Gateway mode**
Publish payload `1` to `<Base_Topic>/set/<Node_ID>/gatewaymode`
*(Alternatively, publish payload `GW=1` to `<Base_Topic>/set/<Node_ID>/command`, or `1` to `<Base_Topic>/set/<Node_ID>/GW`)*

---

## 4. General Settings & Behavior

- **Real-Time Updates**: MQTT messages are pushed instantaneously as soon as a change on the OpenTherm bus is detected. There is no polling delay for sensor updates.
- **Retained Messages**: By default, standard sensor readouts (`/value/#`) are *not* retained (QoS 0). Only the LWT (Last Will and Testament) availability topic and Home Assistant Auto-Discovery configuration payloads are retained.
- **Broker Configuration**: You can configure your MQTT Broker IP, Port, Username, and Password through the **Web UI -> Settings -> MQTT**.
- **HA Reboot Detection**: To prevent the OpenTherm Gateway from constantly republishing discovery payloads if not needed, there is an `HA Reboot Detection` setting in the Web UI. When enabled, the gateway intelligently monitors the `homeassistant/status` topic to know exactly when to push discovery templates again.

---

## 4. System Topics

The OTGW publishes system-level information and diagnostic state data to specific topics. These help monitor the health of the hardware.

**Firmware and PIC Information:**
- `<Base_Topic>/value/<Node_ID>/otgw-firmware/version`: Current firmware version
- `<Base_Topic>/value/<Node_ID>/otgw-firmware/uptime`: Uptime in seconds
- `<Base_Topic>/value/<Node_ID>/otgw-firmware/reboot_count`: Number of reboots
- `<Base_Topic>/value/<Node_ID>/otgw-firmware/reboot_reason`: Reason for the last reset
- `<Base_Topic>/value/<Node_ID>/otgw-firmware/error`: Firmware level errors (e.g., LittleFS mount failed)
- `<Base_Topic>/value/<Node_ID>/otgw-pic/version`: Gateway PIC firmware version
- `<Base_Topic>/value/<Node_ID>/otgw-pic/deviceid`: Gateway PIC device ID
- `<Base_Topic>/value/<Node_ID>/otgw-pic/firmwaretype`: Type of PIC firmware
- `<Base_Topic>/value/<Node_ID>/otgw-pic/picavailable`: PIC Communication status (ON/OFF)

**Communication Status:**
- `<Base_Topic>/value/<Node_ID>/otgw-pic/boiler_connected`: Boiler communication (ON/OFF)
- `<Base_Topic>/value/<Node_ID>/otgw-pic/thermostat_connected`: Thermostat communication (ON/OFF)
- `<Base_Topic>/value/<Node_ID>/otgw-pic/gateway_mode`: Gateway Mode active (ON/OFF)

---

## 5. Home Assistant Integration

The OTGW firmware supports **Home Assistant MQTT Discovery**. This allows Home Assistant to automatically find and configure all devices, sensors, and climate entities with zero manual YAML configuration.

### How It Works
1. **Discovery File (`mqttha.cfg`)**: The firmware maintains a configuration file in its filesystem named `mqttha.cfg`. It contains mappings for all supported OpenTherm sensors to Home Assistant MQTT Discovery payloads.
2. **Template Expansion**: During startup or reconnection, the firmware reads this file and replaces template variables sequentially:
   - `%homeassistant%` -> `homeassistant` (the HA discovery prefix)
   - `%version%` -> The firmware version
   - `%hostname%` -> The node ID
   - `%ip%` -> The IP address of the OTGW
   - `%mqtt_pub_topic%` -> The readout namespace
   - `%mqtt_sub_topic%` -> The command namespace
3. **Chunked Publishing**: Because discovery JSON payloads are large, the firmware implements a streaming MQTT publishing strategy. It streams payloads in manageable chunks directly from the parser or memory to prevent heap fragmentation.
4. **HA Status Monitoring**: The firmware subscribes to `homeassistant/status`. If Home Assistant restarts (an `online` payload is detected), the OTGW will automatically re-publish all discovery configurations, ensuring devices are never orphaned.

### Configuration

You can enable HA Discovery through the Web UI Settings -> MQTT -> `Enable Home Assistant Auto Discovery`.

For more details on Home Assistant MQTT discovery natively, refer to the [Home Assistant MQTT Discovery Documentation](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery).

### Customizing `mqttha.cfg`
Advanced users can upload a customized `mqttha.cfg` using the filesystem browser in the Web UI to add alternative metrics, switch icons, or adjust entity names. Ensure you keep the format `<ID> ; <TopicTemplate> ; <PayloadTemplate>` intact.
