# MQTT API Reference

Complete reference for all MQTT commands supported by OTGW-firmware.

**Total Commands: 52** (29 standard + 10 new standard + 13 advanced PIC16F1847-specific)

## Topic Format

All MQTT commands follow this topic format:
```
<mqtt-prefix>/set/<node-id>/<command>=<payload>
```

Example:
```
OTGW/set/otgw/setpoint=18.5
OTGW/set/otgw/ledA=F
OTGW/set/otgw/modeheating=2
```

---

## Standard Commands (39 total)

Available on all PIC versions (PIC16F88 and PIC16F1847).

### Temperature & Setpoint Control (6 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| setpoint | TT | Temperature (0-30°C) | Temporary temperature override |
| constant | TC | Temperature (0-30°C) | Constant temperature override |
| outside | OT | Temperature (-40 to +64°C) | Outside temperature configuration |
| ctrlsetpt | CS | Temperature (0 or 8-90°C) | Control setpoint override |
| ctrlsetpt2 | C2 | Temperature (0 or 8-90°C) | Control setpoint 2nd circuit |
| setback | SB | Numeric | Setback temperature override |

**Examples:**
```bash
# Set temporary setpoint to 18.5°C
mosquitto_pub -h <broker> -t "OTGW/set/otgw/setpoint" -m "18.5"

# Set outside temperature to 12°C
mosquitto_pub -h <broker> -t "OTGW/set/otgw/outside" -m "12.0"
```

### DHW & Central Heating (6 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| hotwater | HW | Mode (0/1/P/auto) | DHW control: 0=off, 1=on, P=push, other=auto |
| maxchsetpt | SH | Numeric | Max central heating setpoint |
| maxdhwsetpt | SW | Numeric | Max DHW setpoint |
| chenable | CH | On/Off (0/1) | Central heating enable |
| chenable2 | H2 | On/Off (0/1) | Central heating 2nd circuit enable |
| boilersetpoint | BS | Temperature | Override room setpoint to boiler |

**Examples:**
```bash
# Enable central heating
mosquitto_pub -h <broker> -t "OTGW/set/otgw/chenable" -m "1"

# Disable DHW
mosquitto_pub -h <broker> -t "OTGW/set/otgw/hotwater" -m "0"

# Set max DHW setpoint to 60°C
mosquitto_pub -h <broker> -t "OTGW/set/otgw/maxdhwsetpt" -m "60"
```

### Control & Modulation (2 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| maxmodulation | MM | Level (0-100%) | Max relative modulation |
| ventsetpt | VS | Level (0-100%) | Ventilation setpoint |

### Gateway & Hardware Config (10 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| gatewaymode | GW | On/Off (0/1) | Gateway (1) or Monitor (0) mode |
| setclock | SC | HH:MM/N | Set gateway clock (N=1-7 for day of week) |
| ledA | LA | LED Function | Configure LED A |
| ledB | LB | LED Function | Configure LED B |
| ledC | LC | LED Function | Configure LED C |
| ledD | LD | LED Function | Configure LED D |
| ledE | LE | LED Function | Configure LED E |
| ledF | LF | LED Function | Configure LED F |
| gpioA | GA | GPIO Function | Configure GPIO A |
| gpioB | GB | GPIO Function | Configure GPIO B |
| printsummary | PS | On/Off (0/1) | Toggle continuous/summary reporting |

**LED Function Values:**
```
R = Receiving OpenTherm message
X = Transmitting OpenTherm message  
T = Master interface activity
B = Slave interface activity
O = Remote temperature override active
F = Flame is on
H = Central heating is on
W = Hot water is on
C = Comfort mode (DHW) is on
E = Transmission error detected
M = Boiler requires maintenance
P = Raised power mode active
```

**GPIO Function Values:**
```
0 = Input (default)
1 = Ground (0V output)
2 = Vcc (5V output)
3 = LED E function
4 = LED F function
5 = Home (setback when pulled low)
6 = Away (setback when pulled high)
7 = DS1820 temperature sensor (GPIO B only)
8 = DHW blocking (GPIO B only, PIC16F1847 only)
```

**Clock Command Format:**
```
HH:MM/N where:
- HH = Hour (00-23)
- MM = Minute (00-59)
- N = Day of week (1=Monday, 7=Sunday)

Example: 14:30/3 (2:30 PM, Wednesday)
```

**Examples:**
```bash
# Configure LED A to show flame status
mosquitto_pub -h <broker> -t "OTGW/set/otgw/ledA" -m "F"

# Configure LED B to show DHW status
mosquitto_pub -h <broker> -t "OTGW/set/otgw/ledB" -m "W"

# Set gateway clock to 14:30 on Wednesday
mosquitto_pub -h <broker> -t "OTGW/set/otgw/setclock" -m "14:30/3"

# Configure GPIO A as ground output
mosquitto_pub -h <broker> -t "OTGW/set/otgw/gpioA" -m "1"

# Configure GPIO B as Vcc output
mosquitto_pub -h <broker> -t "OTGW/set/otgw/gpioB" -m "2"

# Enable summary reporting mode
mosquitto_pub -h <broker> -t "OTGW/set/otgw/printsummary" -m "1"
```

### Data ID & Advanced Features (15 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| temperaturesensor | TS | O/R | Temperature sensor function |
| addalternative | AA | Data-ID (1-255) | Add alternative Data-ID |
| delalternative | DA | Data-ID (1-255) | Delete alternative Data-ID |
| unknownid | UI | Numeric | Mark Data-ID as unknown |
| knownid | KI | Numeric | Mark Data-ID as known |
| priomsg | PM | Numeric | Send priority message |
| setresponse | SR | ID:data | Set custom response |
| clearrespons | CR | Numeric | Clear custom response |
| resetcounter | RS | Counter type | Reset boiler counter |
| ignoretransitations | IT | On/Off (0/1) | Ignore transitions flag |
| overridehb | OH | On/Off (0/1) | Override in high byte |
| forcethermostat | FT | C/I/S/other | Force thermostat model |
| voltageref | VR | Level (0-9) | Voltage reference level |
| debugptr | DP | Hex address | Debug pointer |

**Reset Counter Types:**
```
HBS, HBH - Central heating (boiler supplied)
HPS, HPH - Central heating (pump supplied)
WBS, WBH - DHW (boiler supplied)
WPS, WPH - DHW (pump supplied)
```

---

## Advanced Commands (13 total)

**PIC16F1847 ONLY** - These commands are available on PIC16F1847 equipped gateways only. They are silently ignored on PIC16F88.

### Cooling Control (2 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| coolinglevel | CL | Level (0-100%) | Cooling output level |
| coolingenable | CE | On/Off (0/1) | Enable/disable cooling |

**Examples:**
```bash
# Set cooling level to 75%
mosquitto_pub -h <broker> -t "OTGW/set/otgw/coolinglevel" -m "75"

# Enable cooling
mosquitto_pub -h <broker> -t "OTGW/set/otgw/coolingenable" -m "1"
```

### Operating Modes (3 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| modeheating | MH | Mode (0-6) | Central heating operating mode |
| mode2ndcircuit | M2 | Mode (0-6) | Second circuit operating mode |
| modewater | MW | Mode (0-6) | DHW operating mode |

**Operating Mode Values (0-6):**
```
0 = No override
1 = Auto
2 = Comfort
3 = Pre-comfort
4 = Reduced
5 = Protection (standby)
6 = Off
```

**Examples:**
```bash
# Set heating to comfort mode
mosquitto_pub -h <broker> -t "OTGW/set/otgw/modeheating" -m "2"

# Set 2nd circuit to auto mode
mosquitto_pub -h <broker> -t "OTGW/set/otgw/mode2ndcircuit" -m "1"

# Set DHW to reduced mode
mosquitto_pub -h <broker> -t "OTGW/set/otgw/modewater" -m "4"
```

### Service & Configuration (8 commands)

| Command | Code | Parameter | Description |
|---------|------|-----------|-------------|
| remoterequest | RR | Code (0-12) | Remote service request |
| transparentparam | TP | id:entry[=value] | Transparent parameter access |
| summermode | SM | On/Off (0/1) | Force summer mode |
| dhwblocking | BW | On/Off (0/1) | Force DHW blocking |
| messageinterval | MI | Interval (100-1275ms) | Message interval in stand-alone mode |
| failsafe | FS | On/Off (0/1) | Enable/disable fail-safe feature |

**Remote Request Codes (0-12):**
```
0  = Back to normal operation
1  = Boiler Lock-out Reset
2  = CH water filling request
3  = Service mode maximum power
4  = Service mode minimum power
5  = Service mode spark test (no gas)
6  = Service mode fan maximum speed (no flame)
7  = Service mode fan minimum speed (no flame)
8  = Service 3-way valve to CH (no pump, no flame)
9  = Service 3-way valve to DHW (no pump, no flame)
10 = Reset service request flag
11 = Service test 1 (OEM specific)
12 = Automatic hydronic air purge
```

**Examples:**
```bash
# Reset boiler lock-out
mosquitto_pub -h <broker> -t "OTGW/set/otgw/remoterequest" -m "1"

# Enable summer mode
mosquitto_pub -h <broker> -t "OTGW/set/otgw/summermode" -m "1"

# Block DHW
mosquitto_pub -h <broker> -t "OTGW/set/otgw/dhwblocking" -m "1"

# Set message interval to 500ms
mosquitto_pub -h <broker> -t "OTGW/set/otgw/messageinterval" -m "500"

# Enable fail-safe
mosquitto_pub -h <broker> -t "OTGW/set/otgw/failsafe" -m "1"
```

---

## Home Assistant Integration

See [MQTT_IMPLEMENTATION.md](MQTT_IMPLEMENTATION.md#home-assistant-integration) for Home Assistant MQTT Auto Discovery details and configuration examples.

---

## Reference Documentation

- [MQTT_IMPLEMENTATION.md](MQTT_IMPLEMENTATION.md) - Implementation details and HA integration
- [example-api/hotwater_examples.md](../example-api/hotwater_examples.md) - DHW control examples
- [example-api/outside_temperature_override_examples.md](../example-api/outside_temperature_override_examples.md) - Temperature override examples
- [README.md](../README.md#mqtt-commands) - Quick reference in README

---

**Last Updated:** 2026-01-31  
**API Version:** 2.0 (52 commands, PIC16F1847 support)
