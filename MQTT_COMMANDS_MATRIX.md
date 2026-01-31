---
# METADATA
Document Title: MQTT Commands - Complete Reference Matrix
Review Date: 2026-01-31
Status: COMPLETE
---

# MQTT Firmware Commands - Complete Reference Matrix

## All 52 Commands (After Implementation)

### EXISTING COMMANDS (29) ✅

| # | MQTT Topic | PIC Code | Type | Purpose | Status |
|---|-----------|----------|------|---------|--------|
| 1 | command | — | raw | Raw command pass-through | ✅ ACTIVE |
| 2 | setpoint | TT | temp | Temporary temperature override | ✅ ACTIVE |
| 3 | constant | TC | temp | Constant temperature override | ✅ ACTIVE |
| 4 | outside | OT | temp | Outside temperature config | ✅ ACTIVE |
| 5 | hotwater | HW | on/off | DHW enable/disable/push | ✅ ACTIVE |
| 6 | gatewaymode | GW | on/off | Gateway/Monitor mode | ✅ ACTIVE |
| 7 | setback | SB | temp | Setback temperature | ✅ ACTIVE |
| 8 | maxchsetpt | SH | temp | Max CH setpoint | ✅ ACTIVE |
| 9 | maxdhwsetpt | SW | temp | Max DHW setpoint | ✅ ACTIVE |
| 10 | maxmodulation | MM | level | Max modulation % | ✅ ACTIVE |
| 11 | ctrlsetpt | CS | temp | Control setpoint override | ✅ ACTIVE |
| 12 | ctrlsetpt2 | C2 | temp | Control setpoint 2nd circuit | ✅ ACTIVE |
| 13 | chenable | CH | on/off | Central heating enable | ✅ ACTIVE |
| 14 | chenable2 | H2 | on/off | Central heating 2nd circuit | ✅ ACTIVE |
| 15 | ventsetpt | VS | level | Ventilation setpoint % | ✅ ACTIVE |
| 16 | temperaturesensor | TS | function | Temperature sensor function | ✅ ACTIVE |
| 17 | addalternative | AA | function | Add alternative Data-ID | ✅ ACTIVE |
| 18 | delalternative | DA | function | Delete alternative Data-ID | ✅ ACTIVE |
| 19 | unknownid | UI | function | Mark Data-ID as unknown | ✅ ACTIVE |
| 20 | knownid | KI | function | Mark Data-ID as known | ✅ ACTIVE |
| 21 | priomsg | PM | function | Priority message | ✅ ACTIVE |
| 22 | setresponse | SR | function | Set custom response | ✅ ACTIVE |
| 23 | clearrespons | CR | function | Clear custom response | ✅ ACTIVE |
| 24 | resetcounter | RS | function | Reset boiler counter | ✅ ACTIVE |
| 25 | ignoretransitations | IT | function | Ignore transitions flag | ✅ ACTIVE |
| 26 | overridehb | OH | function | Override in high byte | ✅ ACTIVE |
| 27 | forcethermostat | FT | function | Force thermostat model | ✅ ACTIVE |
| 28 | voltageref | VR | function | Voltage reference level | ✅ ACTIVE |
| 29 | debugptr | DP | function | Debug pointer (dev) | ✅ ACTIVE |

---

## NEW STANDARD COMMANDS (10) — TO BE ADDED

| # | MQTT Topic | PIC Code | Type | Purpose | Priority | Hardware |
|---|-----------|----------|------|---------|----------|----------|
| 30 | setclock | SC | time | Set gateway clock (HH:MM/N) | HIGH | Both |
| 31 | ledA | LA | led | Configure LED A function | HIGH | Both |
| 32 | ledB | LB | led | Configure LED B function | HIGH | Both |
| 33 | ledC | LC | led | Configure LED C function | HIGH | Both |
| 34 | ledD | LD | led | Configure LED D function | HIGH | Both |
| 35 | ledE | LE | led | Configure LED E function | HIGH | Both |
| 36 | ledF | LF | led | Configure LED F function | HIGH | Both |
| 37 | gpioA | GA | gpio | Configure GPIO A function | HIGH | Both |
| 38 | gpioB | GB | gpio | Configure GPIO B function | HIGH | Both |
| 39 | printsummary | PS | on/off | Toggle continuous/summary reporting | HIGH | Both |

### LED Function Values
```
R = Receiving OpenTherm message
X = Transmitting OpenTherm message
T = Transmitting/receiving on master interface
B = Transmitting/receiving on slave interface
O = Remote setpoint override is active
F = Flame is on
H = Central heating is on
W = Hot water is on
C = Comfort mode (DHW Enable) is on
E = Transmission error detected
M = Boiler requires maintenance
P = Raised power mode active
```

### GPIO Function Values
```
0 = Input (default)
1 = Ground (0V output)
2 = Vcc (5V output)
3 = LED E
4 = LED F
5 = Home (setback when pulled low)
6 = Away (setback when pulled high)
7 = DS1820 (temperature sensor) - GPIO B only
8 = DHWBlock (DHW blocking) - GPIO B only, PIC16F1847
```

---

## NEW ADVANCED COMMANDS (13) — PIC16F1847 ONLY

| # | MQTT Topic | PIC Code | Type | Purpose | Priority | Hardware |
|---|-----------|----------|------|---------|----------|----------|
| 40 | coolinglevel | CL | level | Cooling level control % | MEDIUM | F1847 only |
| 41 | coolingenable | CE | on/off | Cooling enable control | MEDIUM | F1847 only |
| 42 | modeheating | MH | mode | Operating mode heating (0-6) | MEDIUM | F1847 only |
| 43 | mode2ndcircuit | M2 | mode | Operating mode 2nd circuit (0-6) | MEDIUM | F1847 only |
| 44 | modewater | MW | mode | Operating mode water/DHW (0-6) | MEDIUM | F1847 only |
| 45 | remoterequest | RR | request | Send service request (0-12) | MEDIUM | F1847 only |
| 46 | transparentparam | TP | tsp | Read/write TSP or FHB entry | MEDIUM | F1847 only |
| 47 | summermode | SM | on/off | Force summer mode control | MEDIUM | F1847 only |
| 48 | dhwblocking | BW | on/off | Force DHW blocking (e.g., solar) | MEDIUM | F1847 only |
| 49 | boilersetpoint | BS | temp | Override room setpoint to boiler | MEDIUM | F1847 only |
| 50 | messageinterval | MI | interval | Message interval in stand-alone mode (ms) | MEDIUM | F1847 only |
| 51 | failsafe | FS | on/off | Enable/disable fail-safe on reset | MEDIUM | F1847 only |

### Operating Mode Values (MH, M2, MW)
```
0 = No override (thermostat controls)
1 = Auto
2 = Comfort
3 = Precomfort
4 = Reduced
5 = Protection
6 = Off
```

### Remote Request Codes (RR)
```
0 = Back to normal operation
1 = Boiler Lock-out Reset
2 = CH water filling request
3 = Service mode max power
4 = Service mode min power
5 = Service mode spark test (no gas)
6 = Service mode fan max speed (no flame)
7 = Service mode fan min speed (no flame)
8 = Service 3-way valve to CH (no pump, no flame)
9 = Service 3-way valve to DHW (no pump, no flame)
10 = Reset service request flag
11 = Service test 1 (OEM specific)
12 = Automatic hydronic air purge
```

### Message Interval Values (MI)
```
Valid Range: 100-1275 milliseconds
Rounded to: nearest 5ms
Default: 1000ms (1 second)
Note: Must satisfy ≥100ms gap AND ≥1000ms message rate
```

---

## Comparison: Current vs. After Implementation

```
CURRENT (v1.0.0)
├─ Standard Commands: 29
├─ LED Commands: 0
├─ GPIO Commands: 0
├─ Advanced Commands: 0
└─ Total: 29 ✅

AFTER IMPLEMENTATION (v1.0.1)
├─ Standard Commands: 39 (29 existing + 10 new)
├─ LED Commands: 6 (LA-LF)
├─ GPIO Commands: 2 (GA-GB)
├─ Advanced Commands: 13 (PIC16F1847 only)
└─ Total: 52 ✅

COVERAGE INCREASE: 79% (29 → 52 commands)
```

---

## Implementation Phases

### Phase 1: Add Standard Commands (Recommended Now)

```
Current:  29 commands
Adding:   10 commands (SC, LA-LF, GA-GB, PS)
Result:   39 commands
```

**Effort:** 1-2 hours  
**Risk:** Minimal  
**Value:** High  
**Status:** Ready to implement

### Phase 2: Add Advanced Commands (Recommended After Phase 1)

```
Current:  39 commands
Adding:   13 commands (CL-FS, firmware-specific)
Result:   52 commands
```

**Effort:** 1-2 hours  
**Risk:** Low (firmware detection needed)  
**Value:** Medium  
**Status:** Ready to implement after Phase 1

### Phase 3: REST API Status Endpoints (Optional)

```
Add:      REST API endpoints for PR (print report) queries
Purpose:  Read-only status queries for gateway info, modes, configs
Status:   Lower priority, useful but not essential
```

---

## MQTT Topic Pattern (Unchanged)

```
<mqtt-prefix>/set/<node-id>/<command>=<payload>

Example:
homeautomation/set/OTGW/ledA=F
homeautomation/set/OTGW/modeheating=2
homeautomation/set/OTGW/setclock=09:00/1
```

---

## Firmware Compatibility Matrix

| Command | PIC16F88 | PIC16F1847 |
|---------|----------|-----------|
| SC (Clock) | ✅ | ✅ |
| LA-LF (LEDs) | ✅ | ✅ |
| GA-GB (GPIO) | ✅ | ✅ |
| PS (Summary) | ✅ | ✅ |
| **All 29 existing** | ✅ | ✅ |
| CL (Cooling) | ❌ | ✅ |
| CE (Cooling) | ❌ | ✅ |
| MH (Mode Heat) | ❌ | ✅ |
| M2 (Mode 2nd) | ❌ | ✅ |
| MW (Mode Water) | ❌ | ✅ |
| RR (Remote Req) | ❌ | ✅ |
| TP (TSP) | ❌ | ✅ |
| SM (Summer) | ❌ | ✅ |
| BW (DHW Block) | ❌ | ✅ |
| BS (Boiler SP) | ❌ | ✅ |
| MI (Interval) | ❌ | ✅ |
| FS (Fail-safe) | ❌ | ✅ |

---

## Example MQTT Commands (After Implementation)

### Setting LED Functions
```mqtt
Topic: homeautomation/set/OTGW/ledA
Payload: F
# LED A shows Flame status

Topic: homeautomation/set/OTGW/ledB  
Payload: H
# LED B shows Central Heating status
```

### Setting GPIO Functions
```mqtt
Topic: homeautomation/set/OTGW/gpioB
Payload: 7
# GPIO B configured for DS1820 temperature sensor
```

### Setting Clock
```mqtt
Topic: homeautomation/set/OTGW/setclock
Payload: 14:30/3
# Set gateway clock to 2:30 PM Wednesday
```

### Setting Operating Mode (PIC16F1847)
```mqtt
Topic: homeautomation/set/OTGW/modeheating
Payload: 2
# Set heating mode to Comfort
```

### Setting Message Interval (PIC16F1847)
```mqtt
Topic: homeautomation/set/OTGW/messageinterval
Payload: 300
# Send requests to boiler every 300ms (in stand-alone mode)
```

### Triggering Service Request (PIC16F1847)
```mqtt
Topic: homeautomation/set/OTGW/remoterequest
Payload: 2
# Request CH water filling on boiler
```

---

## Backward Compatibility

✅ **All 29 existing commands remain unchanged**
✅ **No breaking changes to MQTT payload format**
✅ **Existing Home Assistant automations continue to work**
✅ **Existing REST API endpoints unaffected**
✅ **Command queue and serial communication unchanged**

---

## Code File Changes Summary

| File | Change | Lines | Complexity |
|------|--------|-------|------------|
| MQTTstuff.ino | Add 23 constants | ~100 | Low |
| MQTTstuff.ino | Expand setcmds[] | ~30 | Low |
| version.h | Bump version | 1 | Trivial |
| README.md | Document new commands | ~50 | Low |

**Total New Code:** ~200 lines (mostly constants)  
**Modified Existing Code:** 0 lines (only additions)  
**Risk Level:** Minimal ✅

---

## Testing Matrix

| Test | Scope | Status |
|------|-------|--------|
| Compilation | All 52 commands parse | Not tested (new) |
| MQTT Connection | Connection stability | Verified ✅ |
| Command Queue | Queue handling | Verified ✅ |
| Serial Output | PIC receives commands | Will verify on hardware |
| HA Discovery | Auto-discovery works | Will verify |
| Backward Compat | Existing 29 commands | Will verify |
| PIC16F88 | Standard commands work | Will verify on hardware |
| PIC16F1847 | Advanced commands work | Will verify on hardware |

---

## Dependencies & References

1. **PIC Firmware Spec:** https://otgw.tclcode.com/firmware.html
2. **otgwmcu Source:** https://otgw.tclcode.com/support/dir?ci=tip
3. **OTGW-firmware ADRs:** docs/adr/
4. **MQTTstuff.ino:** Current implementation reference
5. **OTGW-Core.ino:** Serial communication implementation

---

## Success Criteria

✅ All 52 commands compile without errors  
✅ MQTT connection remains stable  
✅ All 29 existing commands still work  
✅ All 10 new standard commands work on PIC16F88 and PIC16F1847  
✅ All 13 advanced commands work on PIC16F1847  
✅ Home Assistant auto-discovery unaffected  
✅ No regression in performance or memory usage  
✅ Documentation updated  

---

## Next Steps

1. **Review** this analysis document and implementation guide
2. **Approve** Phase 1 implementation
3. **Implement** Phase 1 code changes (1-2 hours)
4. **Test** on actual hardware (PIC16F88 and PIC16F1847)
5. **Deploy** version 1.0.1 with 39 total commands
6. **Plan** Phase 2 if Phase 1 successful

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-31  
**Status:** Ready for Implementation  
**Confidence Level:** High ✅
