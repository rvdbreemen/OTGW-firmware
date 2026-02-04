---
# METADATA
Document Title: Temperature Sensor Graph Implementation
Date: 2026-02-04
Author: GitHub Copilot Advanced Agent
Document Type: Implementation Documentation
Status: COMPLETE
---

# Temperature Sensor Graph Implementation

## Overview

This document describes the implementation of dynamic Dallas DS18B20 temperature sensor detection and graphing in the OTGW-firmware web interface.

## Features

### Dynamic Sensor Detection
- Automatically detects up to 16 Dallas temperature sensors
- Sensors are identified by their unique 16-character hex addresses
- Supports DS18B20 (0x28), DS18S20 (0x10), and DS1822 (0x22) sensor types
- Gracefully handles 0 to 16 sensors without configuration changes

### Graph Integration
- Sensors are added to the temperature grid (gridIndex 4) alongside other temperature readings
- Each sensor gets a unique color from a 16-color palette
- Colors are theme-aware (light/dark mode)
- Real-time updates via existing 1-second polling mechanism

### Main Display Integration
- Sensors already display on the main page
- Shows sensor count and individual readings
- Each sensor displays with its hex address and temperature in °C

## Technical Implementation

### File Changes

#### 1. `data/graph.js`

**Added Color Palettes:**
```javascript
sensorColorPalettes: {
    light: [
        '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
        '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B195', '#C06C84',
        '#6C5B7B', '#355C7D', '#2A9D8F', '#E76F51', '#F4A261',
        '#E9C46A'
    ],
    dark: [
        '#FF8787', '#5FE3D9', '#5BC8E8', '#FFB59A', '#ADE8D8',
        '#FFE66D', '#D1A5E6', '#A0D6F2', '#FFD1B5', '#D688A4',
        '#8677A1', '#4A7BA7', '#3EBFB0', '#FF8C71', '#FFB881',
        '#FFD78A'
    ]
}
```

**Added Sensor Tracking:**
```javascript
detectedSensors: [],           // Array of detected sensors
sensorAddressToId: {},         // Map hex address -> internal ID
```

**New Functions:**

1. `detectAndRegisterSensors(apiData)`
   - Scans API response for sensor data
   - Validates hex addresses (16 chars, proper format)
   - Registers new sensors and adds to series config
   - Assigns colors from palette
   - Initializes data arrays

2. `processSensorData(apiData, timestamp)`
   - Extracts temperature readings from API data
   - Validates temperature range (-50°C to 150°C)
   - Adds data points to graph

#### 2. `data/index.js`

**Modified `refreshOTmonitor()` function:**
```javascript
.then(json => {
  data = json.otmonitor;

  // Detect and register temperature sensors for the graph
  if (typeof OTGraph !== 'undefined' && OTGraph.running) {
    OTGraph.detectAndRegisterSensors(data);
    OTGraph.processSensorData(data, new Date());
  }
  
  // ... existing code continues
```

### API Data Format

The `/api/v2/otgw/otmonitor` endpoint returns sensor data in this format:

```json
{
  "otmonitor": {
    "numberofsensors": {
      "value": 2,
      "unit": ""
    },
    "28FF64D1841703F1": {
      "value": 21.5,
      "unit": "°C"
    },
    "28FF64D1841703F2": {
      "value": 19.8,
      "unit": "°C"
    }
  }
}
```

### Sensor Address Validation

Sensors must meet these criteria to be detected:
1. Key is exactly 16 characters long
2. Key matches regex: `/^[0-9A-Fa-f]{16}$/`
3. Key starts with '28' (DS18B20), '10' (DS18S20), or '22' (DS1822)

### Color Assignment

- Colors are assigned sequentially from the palette
- Index wraps around using modulo 16: `colorIndex = sensorIndex % 16`
- Each sensor gets a color from both light and dark palettes
- Colors are stored in the palettes object using the sensor ID as key

## Testing Scenarios

### Test Case 1: No Sensors
**Setup:** `settingGPIOSENSORSenabled = false` or `DallasrealDeviceCount = 0`
**Expected:** 
- detectAndRegisterSensors returns early
- No sensors added to graph
- Main display shows "Number of temperature sensors: 0"
- No errors or warnings

### Test Case 2: Single Sensor
**Setup:** One DS18B20 connected
**Expected:**
- One sensor registered with label "Sensor 1 (28FF64D1)"
- First color from palette assigned
- Sensor line appears on temperature grid
- Real-time updates every second

### Test Case 3: Multiple Sensors
**Setup:** 2-16 sensors connected
**Expected:**
- All sensors detected and registered
- Unique colors assigned to each
- All sensors graphed on temperature grid
- Labels distinguish sensors by number and partial address

### Test Case 4: Browser Compatibility
**Browsers:** Chrome, Firefox, Safari (latest + 2 versions back)
**Expected:**
- Graph renders correctly in all browsers
- Colors display properly
- No JavaScript errors
- Real-time updates work

## Browser Compatibility

All code follows ES5+ standards for maximum compatibility:
- ✅ Uses `for...in` loops (not `for...of`)
- ✅ Uses `var` declarations
- ✅ Uses function expressions
- ✅ No arrow functions in critical paths (except event handlers in init)
- ✅ Uses `Array.prototype` methods with proper polyfills available
- ✅ No modern ES6+ features that aren't widely supported

## Performance Considerations

1. **Sensor Detection:** Only runs when new sensors appear
2. **Data Processing:** Minimal overhead, only processes sensor keys
3. **Graph Updates:** Uses existing 2-second batched update mechanism
4. **Memory:** Each sensor adds ~3 data arrays (data, pendingData, series config)
5. **Max Data Points:** 864,000 points per sensor (24h at 10 msgs/sec)

## Limitations

1. Maximum 16 sensors supported (MAXDALLASDEVICES in firmware)
2. Sensor colors cycle after 16 (same color reused)
3. Sensor labels use first 8 hex characters for brevity
4. Sensors must be connected at startup or require page refresh to detect

## Future Enhancements (Optional)

- [ ] Add sensor aliases/custom names in settings
- [ ] Add sensor-specific Y-axis scale
- [ ] Add ability to show/hide individual sensors
- [ ] Add sensor data export per-sensor
- [ ] Add min/max/average display per sensor
- [ ] Add sensor trend indicators

## Verification Checklist

- [x] Code follows project style guidelines
- [x] JavaScript syntax validated
- [x] Browser compatibility verified (ES5+ standard)
- [x] Memory management follows PROGMEM guidelines (N/A - frontend only)
- [x] Error handling for edge cases (0 sensors, invalid data)
- [x] Graceful degradation when sensors disabled
- [x] Real-time updates integrated with existing polling
- [x] Theme-aware color palettes (light/dark)
- [ ] Live testing with actual hardware
- [ ] Screenshot documentation
- [ ] Cross-browser testing completed

## Related Files

- `data/graph.js` - Graph implementation
- `data/index.js` - Main page logic and API integration
- `sensors_ext.ino` - Backend sensor reading
- `restAPI.ino` - API endpoint that exposes sensor data
- `OTGW-firmware.h` - Sensor configuration and data structures

## References

- [Dallas DS18B20 Datasheet](https://www.maximintegrated.com/en/products/sensors/DS18B20.html)
- [ECharts Documentation](https://echarts.apache.org/en/index.html)
- [OTGW Firmware Wiki](https://github.com/rvdbreemen/OTGW-firmware/wiki)
