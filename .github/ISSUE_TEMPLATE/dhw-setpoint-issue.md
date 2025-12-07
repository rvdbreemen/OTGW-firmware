---
name: DHW Setpoint Issue
about: Cannot set DHW (hot water) setpoint via OTGW
title: '[DHW] '
labels: 'question'
assignees: ''
---

## ⚠️ Before Opening This Issue

**Please read the [DHW Setpoint Troubleshooting Guide](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/DHW-Setpoint-Troubleshooting.md) first!**

Most DHW setpoint issues are **hardware limitations of your boiler**, not bugs in the firmware. The boiler may only accept DHW setpoint changes from a thermostat.

---

## Issue Description

**Have you read the troubleshooting guide above?**
- [ ] Yes, I've read it

**What is happening?**
<!-- Describe the issue -->

**What did you expect to happen?**
<!-- What behavior did you expect? -->

---

## Configuration

**Boiler Information:**
- Boiler Make: <!-- e.g., Cosmogas, Remeha, Viessmann -->
- Boiler Model: <!-- e.g., Q30B -->
- Boiler Year: <!-- approximate year of manufacture -->

**OTGW Configuration:**
- OTGW Firmware Version: <!-- Check in web UI or API -->
- PIC Firmware Version: <!-- Check in web UI -->
- Hardware Version: <!-- NodeMCU or Wemos D1mini -->

**Thermostat Configuration:**
- [ ] Using OTGW in standalone mode (no thermostat)
- [ ] Using OTGW with thermostat (gateway mode)
- Thermostat Make/Model (if applicable): 

---

## Diagnostic Information

**Remote Boiler Parameter Flags (Message ID 6):**

Please provide the values you see for these:
- `rbp_dhw_setpoint`: <!-- ON or OFF -->
- `rbp_rw_dhw_setpoint`: <!-- ON or OFF -->

You can check these via:
- MQTT topic: `<prefix>/value/<node-id>/rbp_dhw_setpoint`
- REST API: `http://<ip>/api/v1/otgw/id/6`
- Web UI OTmonitor: Message ID 6

**Current DHW Setpoint Reading:**
- Can you READ the current DHW setpoint? <!-- YES/NO -->
- What value does it show? <!-- temperature in °C -->

**Test Results:**

What happened when you tried to change the setpoint?
- [ ] Value changed and stayed changed ✅
- [ ] Value changed briefly then reverted ⚠️
- [ ] No change at all ❌
- [ ] Error message (describe below)

**Command(s) You Tried:**
<!-- e.g., SW=50 via REST API, or MQTT topic with payload -->

---

## Debug Logs

If possible, please provide:
1. Telnet debug output (connect to port 23)
2. MQTT debug messages showing the command being sent
3. Any error messages from logs

```
<!-- Paste debug logs here -->
```

---

## Additional Context

**Have you tried the workarounds from the [troubleshooting guide](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/DHW-Setpoint-Troubleshooting.md)?**
- [ ] Checked boiler's "external control" mode setting
- [ ] Tried using with a thermostat (gateway mode)
- [ ] Verified OTGW is sending the command
- [ ] Other (describe):

**Additional information:**
<!-- Any other relevant details -->

---

## Checklist

- [ ] I have read the [DHW Setpoint Troubleshooting Guide](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/DHW-Setpoint-Troubleshooting.md)
- [ ] I have checked Message ID 6 (RBP flags)
- [ ] I have verified the OTGW is sending the command
- [ ] I have searched for similar issues
- [ ] I understand this may be a boiler hardware limitation, not a firmware bug
