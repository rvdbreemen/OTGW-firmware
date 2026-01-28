# ADR-021: S0 Pulse Counter Hardware Interrupt Architecture

**Status:** Accepted  
**Date:** 2020-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

Many energy meters (electricity, gas, water) provide S0 pulse output for monitoring consumption. An S0 pulse represents a fixed amount of energy (e.g., 1 pulse = 1 Wh).

**S0 Interface standard:**
- **Signal:** Normally Open (NO) relay contact
- **Closure:** Pulse occurs when contact closes
- **Duration:** 90-200ms per pulse
- **Rate:** Up to 40 pulses/second max (specification)
- **Typical:** 1000 pulses/kWh for electricity meters

**Integration requirements:**
- Count every pulse accurately (no missed pulses)
- Calculate real-time power consumption
- Track total consumption
- Integrate with MQTT for Home Assistant
- Handle meter noise/bouncing

**Technical challenges:**
- Pulses are fast (90-200ms)
- Polling would miss pulses
- Debouncing required (mechanical relays bounce)
- ISR context restrictions (no Serial, no delay())

## Decision

**Use hardware interrupt-driven pulse counting with ISR-safe debounce logic.**

**Architecture:**
- **Interrupt type:** `FALLING` edge (pulse = contact closure)
- **GPIO:** Configurable pin (user selects via settings)
- **Debounce:** Time-based in ISR (configurable, default 80ms)
- **Counters:** Dual counters (interval + cumulative)
- **ISR safety:** Volatile variables, interrupt guards
- **Power calculation:** Real-time from pulse interval
- **Calibration:** Pulses per kWh configurable (default 1000)

**Key features:**
1. **No missed pulses:** Interrupt triggers immediately on edge
2. **Debounce protection:** Ignore pulses within debounce window
3. **Power calculation:** `Power = 3600000 / (pulsesPerKWh * interval_ms)`
4. **Thread safety:** `noInterrupts()/interrupts()` guards for counter access

## Alternatives Considered

### Alternative 1: Polling-Based Counting
**Pros:**
- Simple implementation
- No ISR restrictions
- Easy to debug

**Cons:**
- High CPU overhead (need fast polling)
- Will miss pulses if loop blocked
- Timing errors from WiFi/other tasks
- Cannot achieve 40 Hz pulse rate

**Why not chosen:** Polling is unreliable for fast pulses. WiFi operations can block for 100ms+, causing missed pulses.

### Alternative 2: Timer-Based Sampling
**Pros:**
- Consistent sample rate
- No polling overhead
- Predictable timing

**Cons:**
- Limited by timer resolution
- Can miss pulses between samples
- Complex state machine
- Still loses pulses during WiFi

**Why not chosen:** Cannot sample fast enough to catch 90ms pulses reliably.

### Alternative 3: External Pulse Counter IC (PCF8583)
**Pros:**
- Hardware counting (never misses pulses)
- I2C interface
- Can count while ESP8266 sleeps
- Independent of firmware

**Cons:**
- Additional hardware cost (~$2)
- Uses I2C bus (shared with watchdog)
- Requires firmware to read periodically
- Overkill for typical pulse rates

**Why not chosen:** ESP8266 hardware interrupts are sufficient. Cost increase not justified.

### Alternative 4: Software Debounce in Main Loop
**Pros:**
- No ISR restrictions
- Can use delay()
- Easier to debug

**Cons:**
- Still requires interrupt to detect edge
- Debounce happens after ISR fires
- Doesn't prevent bounce-induced interrupts
- More complex state tracking

**Why not chosen:** Must debounce in ISR to prevent interrupt storm from bouncing.

## Consequences

### Positive
- **Accurate counting:** Hardware interrupts catch every pulse
- **Real-time power:** Calculate power from pulse interval
- **Low CPU:** Interrupt-driven, not polling
- **Flexible:** Works with any S0 pulse meter
- **Configurable:** Debounce and calibration adjustable
- **MQTT integration:** Publishes to Home Assistant
- **Dual counters:** Interval count + cumulative total

### Negative
- **GPIO reserved:** One GPIO pin dedicated to S0 input
  - Accepted: User can disable if not using S0
- **ISR restrictions:** Cannot use Serial, delay() in interrupt handler
  - Mitigation: Use volatile variables, minimal ISR code
- **Debounce trade-off:** May miss extremely fast pulses if debounce too long
  - Mitigation: Configurable debounce (default 80ms safe for most meters)
- **Calibration required:** User must know pulses/kWh for their meter
  - Mitigation: Default 1000 p/kWh is common, Web UI allows setting

### Risks & Mitigation
- **Interrupt storm:** Bouncing contact causes many interrupts
  - **Mitigation:** Time-based debounce in ISR ignores pulses within window
- **Counter overflow:** 8-bit counter wraps at 255
  - **Mitigation:** Read counter frequently, accumulate to larger variable
  - **Mitigation:** 255 pulses = 0.255 kWh minimum before overflow
- **Timing errors:** millis() wraps after 49 days
  - **Mitigation:** Use unsigned math for duration calculation
- **False triggers:** Noise on GPIO causes false pulses
  - **Mitigation:** INPUT_PULLUP mode reduces noise
  - **Mitigation:** Debounce filters short glitches

## Implementation Details

**ISR (Interrupt Service Routine):**
```cpp
// Volatile variables for ISR/main communication
volatile uint8_t pulseCount = 0;
volatile unsigned long last_pulse_time = 0;
volatile unsigned long last_pulse_duration = 0;

// ISR - KEEP THIS FAST!
void ICACHE_RAM_ATTR onS0Pulse() {
  unsigned long now = millis();
  
  // Debounce: Ignore pulses within debounce window
  if ((now - last_pulse_time) < settingS0COUNTERdebouncetime) {
    return;  // Too soon, likely bounce
  }
  
  // Calculate interval since last pulse
  last_pulse_duration = now - last_pulse_time;
  last_pulse_time = now;
  
  // Increment counter
  pulseCount++;
  
  // Note: No Serial, no DebugTln, no delay() in ISR!
}
```

**Initialization:**
```cpp
void initS0Counter() {
  if (settingS0COUNTERpin == 0) {
    return;  // Disabled
  }
  
  // Validate GPIO range
  if (settingS0COUNTERpin > 16) {
    DebugTln(F("Invalid S0 GPIO pin"));
    return;
  }
  
  // Configure pin as input with pullup
  pinMode(settingS0COUNTERpin, INPUT_PULLUP);
  
  // Attach interrupt (FALLING = pulse closure)
  attachInterrupt(digitalPinToInterrupt(settingS0COUNTERpin), 
                  onS0Pulse, 
                  FALLING);
  
  DebugTf(PSTR("S0 counter on GPIO %d, debounce %d ms, %d pulses/kWh\r\n"),
    settingS0COUNTERpin,
    settingS0COUNTERdebouncetime,
    settingS0COUNTERpulsekw);
}
```

**Reading counter (thread-safe):**
```cpp
DECLARE_TIMER_SEC(s0Timer, 10);  // Every 10 seconds

void handleS0Counter() {
  if (!DUE(s0Timer)) return;
  
  // Read pulse count (thread-safe)
  uint8_t interval_pulses;
  unsigned long pulse_duration;
  
  noInterrupts();  // Critical section
  interval_pulses = pulseCount;
  pulse_duration = last_pulse_duration;
  pulseCount = 0;  // Reset interval counter
  interrupts();    // End critical section
  
  // Accumulate to total
  total_pulses += interval_pulses;
  
  // Calculate power from last pulse interval
  float power_kw = 0;
  if (pulse_duration > 0) {
    // Power (kW) = 3600000 / (pulses_per_kW * interval_ms)
    power_kw = 3600000.0 / (settingS0COUNTERpulsekw * pulse_duration);
  }
  
  // Publish to MQTT
  publishS0Data(interval_pulses, total_pulses, power_kw);
  
  DebugTf(PSTR("S0: %d pulses, total %lu, power %.3f kW\r\n"),
    interval_pulses, total_pulses, power_kw);
}
```

**Power calculation explained:**
```
Given:
- Meter rating: 1000 pulses/kWh
- Pulse interval: 3.6 seconds (3600 ms)

Energy per pulse = 1 kWh / 1000 pulses = 0.001 kWh = 1 Wh

Power = Energy / Time
      = 1 Wh / (3.6 seconds / 3600 seconds/hour)
      = 1 Wh / 0.001 hours
      = 1000 W = 1 kW

Formula: Power (kW) = 3600000 / (pulses_per_kWh * interval_ms)
         Power (kW) = 3600000 / (1000 * 3600) = 1 kW ✓
```

**MQTT publishing:**
```cpp
void publishS0Data(uint8_t interval_pulses, 
                   unsigned long total_pulses,
                   float power_kw) {
  char topic[128];
  char payload[32];
  
  // Publish pulse count
  snprintf_P(topic, sizeof(topic),
    PSTR("%s/value/%s/s0_pulses"),
    settingMqttTopTopic, settingMqttUniqueID);
  snprintf_P(payload, sizeof(payload), PSTR("%d"), interval_pulses);
  mqttClient.publish(topic, payload);
  
  // Publish total
  snprintf_P(topic, sizeof(topic),
    PSTR("%s/value/%s/s0_total"),
    settingMqttTopTopic, settingMqttUniqueID);
  snprintf_P(payload, sizeof(payload), PSTR("%lu"), total_pulses);
  mqttClient.publish(topic, payload, true);  // Retain
  
  // Publish power
  snprintf_P(topic, sizeof(topic),
    PSTR("%s/value/%s/s0_power"),
    settingMqttTopTopic, settingMqttUniqueID);
  dtostrf(power_kw, 5, 3, payload);
  mqttClient.publish(topic, payload);
}
```

**Configuration (settings.json):**
```json
{
  "S0COUNTERpin": 12,
  "S0COUNTERdebouncetime": 80,
  "S0COUNTERpulsekw": 1000
}
```

## Debounce Tuning

**Bounce characteristics:**
- Mechanical relays: 5-20ms typical bounce
- Solid-state relays: <1ms (minimal bounce)
- Wire noise: Can cause 1-5ms glitches

**Debounce settings:**
```
Too short (<20ms): May count bounce as multiple pulses
Optimal (80ms):    Filters bounce, works up to 12 pulses/sec
Too long (>200ms): May miss legitimate pulses at high rates
```

**Maximum pulse rate vs debounce:**
```
Debounce  Max Rate     Use Case
20ms      50 Hz        High-power meters (>50 kW)
80ms      12.5 Hz      Typical residential (up to 12 kW)
200ms     5 Hz         Low-power or slow meters
```

## Home Assistant Integration

**Auto-Discovery:**
```cpp
void publishS0Discovery() {
  // Power sensor
  DynamicJsonDocument doc(1024);
  doc["name"] = "S0 Power";
  doc["state_topic"] = "otgw/value/123456/s0_power";
  doc["unit_of_measurement"] = "kW";
  doc["device_class"] = "power";
  doc["state_class"] = "measurement";
  
  mqttClient.publish(
    "homeassistant/sensor/otgw_123456_s0_power/config",
    /* ... */);
  
  // Energy sensor (total)
  doc.clear();
  doc["name"] = "S0 Energy";
  doc["state_topic"] = "otgw/value/123456/s0_total";
  doc["unit_of_measurement"] = "Wh";
  doc["device_class"] = "energy";
  doc["state_class"] = "total_increasing";
  
  mqttClient.publish(
    "homeassistant/sensor/otgw_123456_s0_energy/config",
    /* ... */);
}
```

**HA Energy Dashboard integration:**
- S0 total counter appears as energy source
- Power sensor shows real-time consumption
- Can track costs based on electricity rate

## Common Meter Configurations

| Meter Type | Pulses/kWh | Debounce | Notes |
|------------|------------|----------|-------|
| Residential electricity | 1000 | 80ms | Most common |
| Industrial electricity | 2000-10000 | 20ms | High pulse rate |
| Gas meter | 10-100 | 200ms | Slow pulse rate |
| Water meter | 1-10 | 200ms | Very slow |

## Troubleshooting

**Problem:** Missing pulses (count lower than actual)
- **Cause:** Debounce too long
- **Solution:** Reduce debounce time

**Problem:** Double counting (count higher than actual)
- **Cause:** Contact bounce, insufficient debounce
- **Solution:** Increase debounce time

**Problem:** Erratic power readings
- **Cause:** Noise on GPIO line
- **Solution:** Add capacitor (0.1µF) across S0 terminals

**Problem:** No pulses detected
- **Cause:** Wrong GPIO pin or wiring
- **Solution:** Verify GPIO in settings, check wiring polarity

## Related Decisions
- ADR-006: MQTT Integration Pattern (S0 data publishing)
- ADR-007: Timer-Based Task Scheduling (periodic S0 processing)

## References
- Implementation: `s0PulseCount.ino`
- S0 Interface specification: IEC 62053-31
- ESP8266 interrupts: https://arduino-esp8266.readthedocs.io/en/latest/reference.html#interrupts
- Debouncing guide: https://www.arduino.cc/en/Tutorial/Debounce
