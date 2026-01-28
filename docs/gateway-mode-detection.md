# Gateway Mode Detection Implementation

## Overview

This document describes the implementation of reliable gateway mode detection using the OTGW PIC firmware's `PR=M` command.

## Problem Statement

The original implementation detected gateway mode by monitoring OpenTherm message types:
- Messages prefixed with 'R' (Request to Boiler) or 'A' (Answer to Thermostat) indicated gateway activity
- If these messages weren't seen for 30 seconds, gateway mode was assumed to be inactive

This approach had several issues:
1. **Unreliable**: Message-based detection doesn't reflect the actual `GW=` setting in the PIC
2. **Confusing**: Users reported that Web UI showed "Gateway/Standalone false" even when `GW=1` was set
3. **Delayed**: Changes to gateway mode setting wouldn't be reflected until message traffic changed

## Solution

### Command-Based Detection

The solution uses the OTGW PIC firmware's `PR=M` command to query the actual gateway mode setting:

```
Command:  PR=M
Response: PR: G  (Gateway mode, when GW=1)
          PR: M  (Monitor mode, when GW=0)
```

### Implementation Details

#### New Function: `queryOTGWgatewaymode()`

Located in `OTGW-Core.ino`:

```cpp
bool queryOTGWgatewaymode(){
  if (!bPICavailable) {
    OTGWDebugTln(F("queryOTGWgatewaymode: PIC not available"));
    return false;
  }
  
  String response = executeCommand("PR=M");
  response.trim();
  
  OTGWDebugTf(PSTR("queryOTGWgatewaymode: PR=M response=[%s]\r\n"), CSTR(response));
  
  // Response should be "G" for Gateway mode or "M" for Monitor mode
  // executeCommand() strips the "PR: " prefix, so we just get the value
  bool isGatewayMode = false;
  
  if (response.length() > 0) {
    char mode = response.charAt(0);
    if (mode == 'G' || mode == 'g') {
      isGatewayMode = true;
      OTGWDebugTln(F("queryOTGWgatewaymode: Gateway mode (G) detected"));
    } else if (mode == 'M' || mode == 'm') {
      isGatewayMode = false;
      OTGWDebugTln(F("queryOTGWgatewaymode: Monitor mode (M) detected"));
    } else {
      OTGWDebugTf(PSTR("queryOTGWgatewaymode: Unexpected response [%s], defaulting to false\r\n"), CSTR(response));
      isGatewayMode = false;
    }
  } else {
    OTGWDebugTln(F("queryOTGWgatewaymode: Empty response, defaulting to false"));
  }
  
  return isGatewayMode;
}
```

This function includes comprehensive error handling and debug logging to aid troubleshooting.

#### Periodic Polling

Gateway mode is queried every 30 seconds in `doTaskEvery30s()`:

```cpp
void doTaskEvery30s(){
  // Query the actual gateway mode setting from PIC using PR=M command
  // This provides reliable detection of Gateway vs Monitor mode
  static bool bOTGWgatewaypreviousstate = false;
  static bool firstRun = true;
  
  if (bPICavailable && bOTGWonline) {
    bool newGatewayState = queryOTGWgatewaymode();
    
    bOTGWgatewaystate = newGatewayState;
    
    if ((bOTGWgatewaystate != bOTGWgatewaypreviousstate) || firstRun) {
      sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(bOTGWgatewaystate));
      bOTGWgatewaypreviousstate = bOTGWgatewaystate;
      firstRun = false;
    }
  }
}
```

Note: Static variables are declared outside the conditional block to ensure proper initialization even when the system starts with PIC unavailable or OTGW offline.

#### Message Processing Changes

In `processOT()`, the old gateway mode detection logic was replaced:

**Before:**
```cpp
bOTGWgatewaystate = (now < (epochGatewaylastseen+30));
if ((bOTGWgatewaystate != bOTGWgatewaypreviousstate) || (cntOTmessagesprocessed==1)){      
  sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(bOTGWgatewaystate));
  bOTGWgatewaypreviousstate = bOTGWgatewaystate;
}
```

**After:**
```cpp
// Gateway mode is now detected via PR=M command in doTaskEvery30s()
// We still track gateway message activity (R/A messages) for online status detection
// but don't use it to determine gateway mode anymore
bool bOTGWgatewayactive = (now < (epochGatewaylastseen+30));
```

The gateway activity tracking is still used for determining `bOTGWonline` status, but not for gateway mode.

## Benefits

1. **Accurate**: Reflects the actual `GW=` setting in the PIC firmware
2. **Reliable**: Not dependent on message traffic patterns
3. **Consistent**: Works correctly whether thermostat is connected or not
4. **Clear**: Users can now trust the Web UI gateway mode indicator

## Testing

To test the implementation:

1. **Monitor Mode Test**:
   ```
   Send command: GW=0
   Expected: Web UI shows "Gateway/Standalone: false"
   Expected: MQTT topic "otgw-pic/gateway_mode" shows "OFF"
   ```

2. **Gateway Mode Test**:
   ```
   Send command: GW=1
   Expected: Web UI shows "Gateway/Standalone: true"
   Expected: MQTT topic "otgw-pic/gateway_mode" shows "ON"
   ```

3. **Mode Change Test**:
   ```
   Toggle between GW=0 and GW=1
   Expected: Web UI updates within 30 seconds
   Expected: MQTT publishes update on each change
   ```

## References

- OTGW PIC Firmware Documentation: https://otgw.tclcode.com/firmware.html
- PR Command: Returns various PIC firmware settings
- PR=M: Returns gateway mode (G=Gateway, M=Monitor)
- Issue: "Web UI Gateway Mode always false"

## Files Modified

- `OTGW-Core.ino`: Added `queryOTGWgatewaymode()` function, updated `processOT()`
- `OTGW-firmware.ino`: Updated `doTaskEvery30s()` to poll gateway mode

## Backward Compatibility

The implementation maintains backward compatibility:
- Global variable `bOTGWgatewaystate` is still used by REST API and MQTT
- Gateway activity tracking (`epochGatewaylastseen`) is still used for online status
- No changes required to Web UI or REST API endpoints
