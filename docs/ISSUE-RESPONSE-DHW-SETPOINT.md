# Response Template for DHW Setpoint Issues

This document provides a template response for issues related to DHW setpoint not working.

---

## Standard Response

Hi @[username],

Thank you for reporting this issue. I understand you're unable to set the DHW setpoint on your **[Boiler Model]** when using the OTGW in Gateway mode.

### This is a Known Hardware Limitation

Unfortunately, **this is not a firmware bug** but rather a **hardware limitation of your boiler**. Many boilers, including various Cosmogas, De Dietrich, and Remeha models, only accept DHW setpoint changes from an OpenTherm thermostat and will ignore or revert commands sent by the OTGW in standalone mode.

### Technical Explanation

The OpenTherm protocol includes Message ID 6 (Remote Boiler Parameter flags) which indicates whether the boiler supports remote parameter changes:
- **rbp_dhw_setpoint** (HB bit 0): Indicates if DHW setpoint transfer is enabled
- **rbp_rw_dhw_setpoint** (LB bit 0): Indicates if DHW setpoint is read/write

Even when these flags indicate support, many boilers only honor these commands when they come from an authenticated OpenTherm thermostat, not from the gateway operating without one.

### Why This Happens

Your boiler's firmware requires that DHW setpoint commands originate from a proper OpenTherm thermostat. When the OTGW sends the `SW` command (or you use the MQTT `maxdhwsetpt` topic) in standalone mode, your boiler:
1. Receives the command correctly ✅
2. May briefly acknowledge it ✅
3. But then ignores it and reverts to its own setpoint ❌

The OTGW firmware **is working correctly** - it's sending the proper OpenTherm messages. Your boiler is simply choosing not to accept them.

### Solutions

#### ✅ Recommended Solution: Use with a Thermostat

The most reliable solution is to use your OTGW in true **gateway mode** between your thermostat and boiler:

```
[Thermostat] <--OpenTherm--> [OTGW] <--OpenTherm--> [Boiler]
```

In this configuration:
- The thermostat sends DHW setpoint requests
- The OTGW can monitor and even modify these requests
- The boiler accepts the commands because they appear to come from the thermostat
- You can still override the setpoint via the `SW` command

#### ⚙️ Alternative: Check Boiler Settings

Some boilers have a service menu setting to enable "external control" or "OpenTherm override" mode. Check your boiler's manual or contact Cosmogas support to see if this option is available for the Q30B.

#### 📖 Manual Control

As a last resort, you may need to adjust the DHW setpoint directly on your boiler's control panel rather than remotely.

### What You Can Do

1. **Verify the limitation:** Check Message ID 6 flags via:
   - MQTT: `<prefix>/value/<node-id>/rbp_dhw_setpoint` and `rbp_rw_dhw_setpoint`
   - REST API: `http://<your-otgw-ip>/api/v1/otgw/id/6`
   - Web UI: Look at the OTmonitor interface

2. **Test with debug logs:** Connect via telnet to port 23 and watch the OTGW logs while sending the SW command to confirm the command is being sent.

3. **Consider using a thermostat:** This is the most reliable way to have full control over all OpenTherm parameters.

### Documentation

I've created comprehensive documentation to help with this issue:
- **[DHW Setpoint Troubleshooting Guide](DHW-Setpoint-Troubleshooting.md)** - Detailed explanation and workarounds
- **[FAQ](FAQ.md)** - Common questions and answers

### Conclusion

While I understand this is frustrating, this behavior is **by design of your boiler**, not a bug in the OTGW firmware. The firmware is functioning correctly and following the OpenTherm protocol specification. Your boiler's implementation simply requires a thermostat for DHW setpoint control.

If you'd like to discuss this further or if you find a workaround specific to the Cosmogas Q30B, please share it in the comments so others can benefit!

---

## Related Issues
- #259 - Setting DHW setpoint not working (Cosmogas Q30B)
- Home Assistant Core #38078 - DHW setpoint changes reverted

---

*This template can be customized based on the specific boiler model and user situation.*
