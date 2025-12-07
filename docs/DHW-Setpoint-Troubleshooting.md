# DHW Setpoint Troubleshooting Guide

## Problem Description

Some users report that they cannot set the DHW (Domestic Hot Water) setpoint when using the OTGW (OpenTherm Gateway) in Gateway mode without a thermostat connected. The `SW` command or MQTT `maxdhwsetpt` topic appears to have no effect, and the boiler continues to use its own setpoint.

## Root Cause

This is **not a firmware bug**, but a **boiler hardware limitation**. Many boilers only accept DHW setpoint changes from an OpenTherm thermostat and will ignore setpoint changes coming from the OTGW when operating without a thermostat (standalone mode).

### Technical Background

The OpenTherm protocol uses Message ID 6 (Remote Boiler Parameter flags) to indicate which parameters can be changed remotely:
- Bit 0 of HB (High Byte): DHW setpoint transfer-enable
- Bit 0 of LB (Low Byte): DHW setpoint read/write capability

Even when the OTGW sends the `SW` command to change the DHW setpoint (Message ID 56), the boiler may:
1. Ignore the command completely
2. Accept it temporarily but revert to its own setpoint after a short time
3. Only accept changes when they come from an authenticated thermostat

### Known Affected Boilers

Users have reported this issue with the following boilers:
- Cosmogas Q30B
- De Dietrich models
- Various Remeha models

**Note:** This is not an exhaustive list. Any boiler may exhibit this behavior depending on its OpenTherm implementation.

## How to Check If Your Boiler Supports Remote DHW Setpoint Changes

You can monitor the Remote Boiler Parameter flags (Message ID 6) to see what your boiler reports:

### Via MQTT
Subscribe to these topics:
```
<your-mqtt-prefix>/value/<node-id>/rbp_dhw_setpoint
<your-mqtt-prefix>/value/<node-id>/rbp_rw_dhw_setpoint
```

- `rbp_dhw_setpoint` = ON means the boiler advertises DHW setpoint transfer capability
- `rbp_rw_dhw_setpoint` = ON means the boiler advertises DHW setpoint is read/write

### Via REST API
Check the OTGW monitor endpoint:
```
http://<your-otgw-ip>/api/v1/otgw/id/6
```

### Via Web UI
Look at the OTmonitor interface in the web UI and check Message ID 6.

## Workarounds and Solutions

### Solution 1: Use with an OpenTherm Thermostat (Recommended)

The most reliable solution is to use the OTGW in true "gateway mode" - **between** your thermostat and boiler:

```
[Thermostat] <--OpenTherm--> [OTGW] <--OpenTherm--> [Boiler]
```

In this configuration:
- The thermostat sends the DHW setpoint request
- The OTGW can monitor and modify the communication
- The boiler accepts commands because they appear to come from the thermostat
- You can still use the `SW` command to override the thermostat's setpoint

### Solution 2: Check for Boiler "External Control" Mode

Some boilers have a setting in their service menu to enable "external control" or "OpenTherm override" mode. Check your boiler's manual or contact the manufacturer.

### Solution 3: Use OTGW Firmware Commands on Boot

If your boiler supports it, you can configure the OTGW to send commands on boot. Go to:
```
Settings -> OTGW Commands on boot
```

Add commands like:
```
SW=50
```

This sets the DHW setpoint to 50°C on startup. However, this only works if your boiler accepts the command.

### Solution 4: Adjust DHW Setpoint Manually on Boiler

As a last resort, you may need to adjust the DHW setpoint directly on your boiler's control panel rather than remotely via the OTGW.

## Verifying if Commands Are Being Sent

You can verify that the OTGW is actually sending the command:

### Via Telnet Debug
1. Connect to your OTGW via telnet on port 23
2. Send the command: `SW=50`
3. Watch for the response - you should see either:
   - `SW: 50.0` (command accepted by OTGW)
   - `NG` (command not accepted)

### Via MQTT Debug
Subscribe to all OTGW topics:
```
<your-mqtt-prefix>/#
```

Send a command to:
```
<your-mqtt-prefix>/set/<node-id>/maxdhwsetpt
```
With payload: `50`

Watch for confirmation messages or changes in the `dhwsetpoint` value topic.

## Testing Your Configuration

1. **Check current DHW setpoint:**
   ```
   http://<your-otgw-ip>/api/v1/otgw/label/TdhwSet
   ```

2. **Try to change DHW setpoint via REST API:**
   ```bash
   curl -X POST http://<your-otgw-ip>/api/v1/otgw/command/SW=50
   ```

3. **Check if the value changed:**
   Wait 10-30 seconds, then check the API again to see if the new value is reported.

4. **Monitor for reversion:**
   If the value changes briefly but reverts back, your boiler is ignoring the command.

## MQTT Command Examples

### Set DHW setpoint to 50°C:
Topic: `<your-mqtt-prefix>/set/<node-id>/maxdhwsetpt`  
Payload: `50`

### Send raw OTGW command:
Topic: `<your-mqtt-prefix>/set/<node-id>/command`  
Payload: `SW=50`

## Conclusion

If your boiler does not support remote DHW setpoint changes without a thermostat, this is a **hardware limitation of your boiler**, not a bug in the OTGW firmware. The firmware is functioning correctly and sending the proper OpenTherm commands, but your boiler is choosing to ignore them.

### What Works:
✅ Reading current DHW setpoint  
✅ Reading DHW temperature  
✅ Monitoring all OpenTherm messages  
✅ Setting DHW setpoint when using with a thermostat  

### What Might Not Work (Boiler Dependent):
❌ Setting DHW setpoint in standalone mode (no thermostat)  
❌ Overriding boiler's DHW setpoint without thermostat  

## Additional Resources

- OTGW Official Documentation: http://otgw.tclcode.com/
- OTGW Standalone Operation: https://otgw.tclcode.com/standalone.html
- OTGW-firmware Wiki: https://github.com/rvdbreemen/OTGW-firmware/wiki
- OpenTherm Protocol Specification: Check the `Specification` folder in this repository

## Getting Help

If you've tried all the above and still have issues:

1. Check if others with your boiler model have reported similar issues
2. Post in the GitHub Discussions or Discord channel (see README.md)
3. Include:
   - Your boiler make and model
   - OTGW firmware version
   - Whether you're using a thermostat or standalone mode
   - Output of Message ID 6 (RBP flags)
   - Debug logs showing the command being sent

## Related GitHub Issues

- #259 - Setting DHW setpoint not working (Cosmogas Q30B)
- Home Assistant Core #38078 - DHW setpoint changes reverted

---

*This document was created to help users understand the limitations of DHW setpoint control in OpenTherm systems.*
