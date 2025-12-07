# Frequently Asked Questions (FAQ)

## General Questions

### Q: What is the OTGW-firmware?
**A:** This is an ESP8266-based firmware for the Nodoshop OpenTherm Gateway hardware. It provides a web interface, MQTT support, REST API, and Home Assistant integration for monitoring and controlling your OpenTherm heating system.

### Q: What hardware do I need?
**A:** You need:
- Nodoshop OpenTherm Gateway hardware (version 1.x-2.x with NodeMCU or 2.3+ with Wemos D1mini)
- An OpenTherm-compatible boiler
- Optionally: an OpenTherm thermostat

### Q: Can I use the OTGW without a thermostat?
**A:** Yes, the OTGW can operate in standalone mode without a thermostat (requires firmware 4.0+ on the PIC). However, some boiler functions may be limited - see the DHW setpoint question below.

---

## Common Issues

### Q: Why can't I set the DHW (hot water) setpoint?
**A:** This is a common issue with many boilers. **Most boilers only accept DHW setpoint changes from an OpenTherm thermostat**, not from the gateway in standalone mode. 

**Solution:** Use the OTGW between your thermostat and boiler, not as a standalone device.

**More info:** See [DHW Setpoint Troubleshooting Guide](DHW-Setpoint-Troubleshooting.md)

### Q: The DHW setpoint changes for a few seconds then reverts back
**A:** This confirms your boiler is ignoring the command. The OTGW sends the command successfully, but your boiler rejects it and returns to its own setpoint. This is a **boiler hardware limitation**.

### Q: Why doesn't my boiler respond to commands from Home Assistant?
**A:** Check these things:
1. Is your boiler compatible with the specific OpenTherm messages?
2. Are you using a thermostat or standalone mode?
3. Check the Remote Boiler Parameter flags (Message ID 6) to see what your boiler supports
4. Some commands only work when a thermostat is present

### Q: My OTGW is showing "PS=1" - what does this mean?
**A:** PS=1 means the OTGW is in thermostat simulation mode. This is the standalone mode where the OTGW pretends to be a thermostat. Some features may work differently in this mode.

### Q: How do I update the PIC firmware?
**A:** 
1. Go to the web interface
2. Navigate to Settings -> PIC Firmware Update
3. Follow the on-screen instructions
4. **⚠️ WARNING:** Never update PIC firmware via WiFi using OTmonitor - this can brick your device!

---

## Configuration

### Q: How do I send OTGW commands?
**A:** There are multiple ways:

1. **Via REST API:**
   ```bash
   curl -X POST http://<ip>/api/v1/otgw/command/PS=0
   ```

2. **Via MQTT:**
   - Topic: `<mqtt-prefix>/set/<node-id>/command`
   - Payload: `PS=0`

3. **Via Telnet:**
   - Connect to port 25238
   - Type the command and press Enter

4. **Via Web UI:**
   - Use the Settings -> OTGW Commands on boot section
   - Commands sent here will be executed every time the ESP reboots

### Q: How do I change the room setpoint?
**A:** Use the TT (Temporary Temperature) command:

**Via MQTT:**
- Topic: `<mqtt-prefix>/set/<node-id>/setpoint`
- Payload: `20.5` (temperature in °C)

**Via REST API:**
```bash
curl -X POST http://<ip>/api/v1/otgw/command/TT=20.5
```

### Q: What MQTT topics are available?
**A:** The firmware uses these topic structures:
- **Publishing data:** `<mqtt-prefix>/value/<node-id>/<sensor>`
- **Subscribing to commands:** `<mqtt-prefix>/set/<node-id>/<command>`

**Example topics:**
- `OTGW/value/OTGW/roomtemp` - Current room temperature
- `OTGW/value/OTGW/boilertemp` - Boiler water temperature  
- `OTGW/value/OTGW/dhwsetpoint` - DHW setpoint
- `OTGW/set/OTGW/setpoint` - Set room temperature
- `OTGW/set/OTGW/maxdhwsetpt` - Set DHW setpoint (if supported)

### Q: How do I integrate with Home Assistant?
**A:** The firmware has built-in Home Assistant Discovery:
1. Enable MQTT in Settings
2. Configure your Home Assistant MQTT broker details
3. Enable "Home Assistant Discovery" in settings
4. Entities will automatically appear in Home Assistant

---

## Troubleshooting

### Q: My ESP keeps rebooting
**A:** Check:
1. Power supply - make sure it provides stable 5V with enough current (>500mA)
2. WiFi signal strength - weak signal can cause issues
3. Memory issues - check the logs for "low heap" errors
4. Try reflashing the firmware

### Q: WiFi connection is unstable
**A:** 
1. Check WiFi signal strength in the web UI
2. Move the device closer to your router
3. Check for 2.4GHz interference
4. Try setting a static IP address
5. Check if your router's DHCP pool is full

### Q: MQTT is not working
**A:** Verify:
1. MQTT broker is running and accessible
2. Username/password are correct (if using authentication)
3. MQTT topics are correctly configured
4. Check the MQTT debug logs in telnet (port 23)
5. Try the "Test Connection" button in MQTT settings

### Q: The web UI doesn't update
**A:** 
1. Clear your browser cache
2. Try a different browser
3. Check if JavaScript is enabled
4. Reload the page (Ctrl+F5 or Cmd+Shift+R)
5. The web UI updates periodically, not in real-time (websockets coming in future updates)

### Q: I can't connect via telnet
**A:** 
1. Make sure you're using the correct IP address
2. Port 23 is for debug output
3. Port 25238 is for OTmonitor-compatible serial communication
4. Check if a firewall is blocking the connection
5. Make sure you're on the same network

---

## Advanced

### Q: Can I use Dallas temperature sensors?
**A:** Yes! Connect DS18B20 sensors to the configured GPIO pin (default GPIO10). The firmware will automatically detect them and publish values to MQTT. Home Assistant Discovery is supported.

### Q: Can I read my electricity meter's S0 pulse output?
**A:** Yes! Configure the S0 GPIO pin in settings. The firmware will count pulses and publish energy consumption to MQTT.

### Q: How do I enable debug logging?
**A:** 
1. Connect via telnet to port 23
2. Debug output is automatically sent to telnet
3. You can also enable verbose MQTT debug and REST API debug in the settings

### Q: Can I use this with a ventilation/heat recovery system?
**A:** Yes, the firmware supports OpenTherm 2.3b messages including ventilation and heat recovery message IDs.

### Q: What OpenTherm message IDs are supported?
**A:** The firmware supports all OpenTherm 2.2, 2.3, and 2.3b message IDs, plus Remeha-specific messages. See `OTGW-Core.h` for the complete list.

---

## Safety and Warranty

### Q: Will this void my boiler warranty?
**A:** The OTGW is a passive monitoring device when used in gateway mode (with a thermostat). It does not modify the boiler hardware. However, check with your boiler manufacturer to be sure.

### Q: Is it safe to leave the OTGW running 24/7?
**A:** Yes, the device is designed for continuous operation. However:
- Ensure proper ventilation
- Use a quality power supply
- Monitor the device temperature
- Keep firmware updated

### Q: What happens if the OTGW or WiFi fails?
**A:** When using in gateway mode (with a thermostat):
- Your thermostat and boiler continue to work normally
- The OTGW is just monitoring/modifying the communication
- If the OTGW fails, the thermostat talks directly to the boiler

---

## Getting More Help

### Where can I find more documentation?
- GitHub Wiki: https://github.com/rvdbreemen/OTGW-firmware/wiki
- OTGW Official Site: http://otgw.tclcode.com/
- Discord Community: See README.md for invite link

### How do I report a bug?
1. Check if it's a known issue in GitHub Issues
2. Gather debug information:
   - Firmware version
   - Boiler model
   - OTGW hardware version
   - Detailed description of the problem
   - Debug logs (from telnet port 23)
3. Open a new issue on GitHub with all the information

### How do I request a feature?
Open a feature request on GitHub Issues with:
- Clear description of the feature
- Use case / why it's needed
- Any relevant technical details

---

*Last updated: December 2024*
