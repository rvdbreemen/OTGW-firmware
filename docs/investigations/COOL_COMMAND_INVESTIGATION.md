# Investigation: COOL Command Feasibility

**Date:** 2026-02-16  
**Issue:** Add COOL command  
**Investigator:** GitHub Copilot Advanced Agent  

## Executive Summary

**Request:** Add MQTT set command for the OTGW command 'COOL' (ID 000:HB2: Master status: Cooling enable) to enable cooling control on heat pumps.

**Finding:** ❌ **NOT FEASIBLE** with current OTGW PIC firmware

**Reason:** The Schelte Bron OTGW PIC firmware does not provide a serial command to control the "Cooling enable" bit. This is a fundamental hardware/firmware limitation, not a limitation of this ESP8266 firmware.

---

## Detailed Analysis

### 1. OpenTherm Protocol Support ✅

The OpenTherm protocol **DOES support cooling control** and has since version 2.2:

- **Message ID 0 (Status flags)**:
  - **HB2 (High Byte, bit 2)**: "Cooling enable" - Master (thermostat) status flag
  - **LB4 (Low Byte, bit 4)**: "Cooling status" - Slave (boiler/heat pump) status flag

- **Message ID 7**: "Cooling control signal" (0-100%) - Write command for cooling modulation percentage

- **Message ID 3, HB2**: "Cooling configuration" - Slave configuration flag indicating cooling support

**Reference:** 
- `/Specification/OpenTherm-Protocol-Specification-v4.2.md` (lines 1434-1448)
- `/Specification/OpenTherm-Protocol-Specification-v4.2.md` (lines 2202-2210)

### 2. Current ESP8266 Firmware Implementation ✅

This ESP8266 firmware **ALREADY** monitors and publishes cooling status:

**MQTT Topics Published:**
- `cooling_enable` - Master status: Cooling enable (ID0:HB2)
- `cooling` - Slave status: Cooling active (ID0:LB4)
- `cooling_config` - Slave configuration: Cooling supported (ID3:HB2)
- `CoolingControl` - Cooling control signal percentage (ID7)

**Home Assistant Auto-Discovery:**
- All cooling sensors are configured in `/src/OTGW-firmware/data/mqttha.cfg`:
  - Line 56: `cooling` binary sensor
  - Line 62: `cooling_enable` binary sensor
  - Line 77: `cooling_config` binary sensor
  - Line 128: `CoolingControl` sensor (%)

**Code References:**
- `/src/OTGW-firmware/OTGW-Core.ino` lines 510, 549: Helper functions `isCoolingEnabled()`, `isCoolingActive()`
- `/src/OTGW-firmware/OTGW-Core.ino` lines 738, 777: MQTT publish for cooling status
- `/src/OTGW-firmware/OTGW-Core.h` line 35: `CoolingControl` state variable

### 3. OTGW PIC Firmware Limitation ❌

The **fundamental limitation** is in the OTGW PIC firmware by Schelte Bron:

**Current MQTT Set Commands (27 total):**

The ESP8266 firmware supports 27 MQTT set commands that map to OTGW PIC serial commands:

| MQTT Command | PIC Command | Purpose |
|--------------|-------------|---------|
| setpoint | TT | Temperature setpoint |
| hotwater | HW | Hot water control (0/1/P/auto) |
| chenable | CH | Central heating enable |
| chenable2 | H2 | CH2 enable |
| maxchsetpt | SH | Max CH setpoint |
| maxdhwsetpt | SW | Max DHW setpoint |
| ... | ... | ... (21 more commands) |

**Missing Command:**
- ❌ **No "COOL" or equivalent command exists** in the OTGW PIC firmware
- The PIC firmware command reference is at: https://otgw.tclcode.com/firmware.html

**Why This Matters:**

The OpenTherm Gateway (OTGW) hardware consists of two microcontrollers:
1. **PIC microcontroller** - Handles OpenTherm communication with thermostat/boiler
2. **ESP8266** - Provides network connectivity (MQTT, REST API, Web UI)

The ESP8266 can only send commands that the PIC firmware understands. Without a PIC command for "cooling enable," we cannot add MQTT control for this feature.

### 4. Technical Background

**How Commands Work:**

1. User sends MQTT message to topic: `{topTopic}/set/{nodeId}/command_name`
2. ESP8266 firmware (MQTTstuff.ino) maps command to PIC serial command
3. Command is queued and sent to PIC via serial interface
4. PIC firmware interprets the command and manipulates OpenTherm messages

**Example - Hot Water Control:**
- MQTT topic: `OTGW/set/otgw-1234/hotwater` with payload `1`
- ESP8266 maps to: `HW=1`
- Sends `HW=1` to PIC via serial
- PIC firmware understands `HW` command and sets the DHW enable bit

**For Cooling:**
- MQTT topic: `OTGW/set/otgw-1234/cooling` with payload `1` ← **Desired**
- ESP8266 would map to: `??=1` ← **No PIC command available**
- Cannot send to PIC ← **BLOCKED**

### 5. Why ID0:HB2 Cannot Be Controlled

The "Cooling enable" bit is part of the **Master status flags** in OpenTherm message ID 0. According to the OpenTherm specification:

- Message ID 0 uses **READ-DATA** message type (not WRITE-DATA)
- Master sends: `READ-DATA(id=0, MasterStatus, 0x00)`
- Slave responds: `READ-ACK(id=0, MasterStatus, SlaveStatus)`

The Master status flags are set by the **thermostat** (or OTGW acting as master), not by explicit write commands. The OTGW PIC firmware would need to provide a command to manipulate these flags, similar to how `CH` enables central heating.

**Reference:** OpenTherm Protocol Specification v4.2, Section 5.1 (Message Types)

---

## Alternative Solutions

### Option 1: Request PIC Firmware Enhancement (Recommended)

**Action:** Contact Schelte Bron and request a new PIC firmware command for cooling control.

**Rationale:**
- This is the proper architectural solution
- Follows the same pattern as existing commands (CH, HW, etc.)
- Would benefit the entire OTGW community

**Contact:**
- Website: https://otgw.tclcode.com/
- Support Forum: https://otgw.tclcode.com/support/forum

**Example Request:**
> "Could you add a 'CO' or 'COOL' command to control the Cooling enable bit (ID0:HB2) similar to how 'CH' controls the CH enable bit? This would enable cooling control for heat pumps via MQTT/REST APIs in the ESP8266 firmware."

### Option 2: Thermostat/Controller Solution

**Action:** Use a thermostat or smart controller that supports cooling mode.

**Rationale:**
- Cooling control is typically a thermostat function
- Many modern thermostats support OpenTherm cooling
- The OTGW can monitor and report the status (which it already does)

**Compatible Devices:**
- Nest Thermostat (with OpenTherm module)
- Tado Smart Thermostat
- Honeywell Evohome
- Remeha iSense

### Option 3: Direct OpenTherm Message Injection (Advanced)

**Action:** Use the OTGW PIC firmware's "Set Response" (SR) command to inject custom OpenTherm messages.

**Complexity:** HIGH - Requires deep OpenTherm protocol knowledge

**Risks:**
- May interfere with normal thermostat operation
- Could confuse the boiler/heat pump
- Not recommended without extensive testing

**Not Recommended:** This approach is fragile and goes against the OTGW design philosophy.

---

## Conclusion

**Cannot implement the requested feature** because:

1. ✅ OpenTherm protocol supports cooling
2. ✅ ESP8266 firmware already monitors cooling status
3. ❌ **OTGW PIC firmware lacks a command for cooling control**
4. ❌ Cannot add MQTT set command without underlying PIC support

**Recommendation:**

The issue should be **closed** with an explanation that this is a PIC firmware limitation, not an ESP8266 firmware limitation. Users who need cooling control should:

1. Contact Schelte Bron to request the feature in the PIC firmware
2. Use a thermostat that supports OpenTherm cooling
3. Continue to monitor cooling status via existing MQTT topics

**Response to Maintainer (@rvdbreemen):**

Your initial suspicion was correct - this is indeed a limitation of the OpenTherm Gateway PIC firmware, not the OpenTherm protocol itself. The protocol fully supports cooling, and your ESP8266 firmware already monitors all cooling-related parameters. However, Schelte Bron's PIC firmware doesn't provide a serial command to control the cooling enable bit, similar to how it lacks many other master control functions.

The proper solution would be to request this feature from Schelte Bron for inclusion in a future PIC firmware release.

---

## References

1. **OpenTherm Protocol Specification v4.2**
   - `/Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
   - `/Specification/OpenTherm-Protocol-Specification-v4.2.md`

2. **OTGW PIC Firmware Documentation**
   - https://otgw.tclcode.com/firmware.html
   - Command reference for PIC serial interface

3. **Current Implementation**
   - `/src/OTGW-firmware/MQTTstuff.ino` (lines 188-226): Command mapping structure
   - `/src/OTGW-firmware/OTGW-Core.ino` (lines 710-780): Cooling status monitoring

4. **Architecture Documentation**
   - `/docs/adr/ADR-031-two-microcontroller-coordination-architecture.md`
   - Explains PIC + ESP8266 architecture

---

## Appendix: Example Response for Issue

**Suggested comment to close the issue:**

> Thank you for the feature request! After investigating, I've found that this is unfortunately **not possible** with the current OTGW hardware/firmware architecture.
>
> **The Issue:**
> 
> The OTGW consists of two microcontrollers:
> - **PIC**: Handles OpenTherm communication (by Schelte Bron)
> - **ESP8266**: Provides network connectivity (this firmware)
> 
> While the OpenTherm protocol fully supports cooling control, and this ESP8266 firmware already **monitors** cooling status (via `cooling_enable`, `cooling`, and `CoolingControl` MQTT topics), we cannot **control** it because Schelte Bron's PIC firmware doesn't provide a serial command for the cooling enable bit.
>
> **What Already Works:**
> - ✅ Monitor cooling enable status (ID0:HB2)
> - ✅ Monitor cooling active status (ID0:LB4)  
> - ✅ Monitor cooling control signal (ID7)
> - ✅ Home Assistant auto-discovery for all cooling sensors
>
> **What's Missing:**
> - ❌ PIC firmware command to SET the cooling enable bit
>
> **Alternatives:**
> 1. Request this feature from Schelte Bron: https://otgw.tclcode.com/support/forum
> 2. Use a thermostat with OpenTherm cooling support
> 3. Continue monitoring cooling status (which works perfectly)
>
> I've created a detailed investigation document: `/docs/investigations/COOL_COMMAND_INVESTIGATION.md`
>
> Closing as **won't fix** (hardware limitation).
