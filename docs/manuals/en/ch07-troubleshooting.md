## Chapter 7: Troubleshooting

This chapter covers the most common problems, their root causes, and step-by-step remedies. When in doubt, start with the Telnet debug log (section 7.9) and work forward from there.

### 7.1 OTGW Appears Offline After Boot

**Symptom:** The web UI shows the OTGW as offline. MQTT publishes nothing. The live log is empty.

| Check | What to look for |
|---|---|
| PIC serial connection | A loose or broken UART wire between ESP and PIC is the most common hardware cause |
| PIC powered | The OTGW PIC needs power from the OpenTherm bus or an external supply |
| PIC firmware mode | If in diagnostic or interface mode from a previous OTmonitor session, use the web UI to reset the PIC |

**Steps:**
1. Connect to the Telnet debug console (see section 7.9).
2. Look for `OTGW online` or `OTGW offline` messages.
3. If you see `OTGW offline` repeating, the PIC is not responding on serial.
4. Power-cycle the entire OTGW board (not just the ESP).

---

### 7.2 MQTT Not Connecting

**Symptom:** MQTT status shows disconnected.

Common MQTT return codes:

| Code | Meaning |
|---|---|
| -1 | Connection timeout (broker unreachable, wrong IP, firewall) |
| -2 | Connection refused (wrong port, broker not running) |
| 1 | Connection refused: unacceptable protocol version |
| 4 | Connection refused: bad username or password |
| 5 | Connection refused: not authorized |

**Diagnosis:**
1. Open the Telnet debug console (port 23).
2. Search for `MQTT` in the output for error codes.
3. Verify the broker is reachable: `mosquitto_sub -h <broker-ip> -t "test" -v` from another machine.

---

### 7.3 Home Assistant Entities Not Appearing

**Checklist:**

1. MQTT integration enabled in HA: Settings, Devices and Services.
2. Discovery prefix matches: default is `homeassistant` in both HA and firmware.
3. Discovery enabled in firmware: check Settings in the web UI.
4. Wait 30-60 seconds after MQTT connect for discovery to run.
5. Check HA MQTT logs in Settings, System, Logs, filter for `mqtt`.
6. If you run multiple OTGW devices, each must have a unique `settingMQTTuniqueid`.

---

### 7.4 WiFi Keeps Dropping

| Cause | Diagnosis | Fix |
|---|---|---|
| Weak signal | Check RSSI in web UI header. Below -75 dBm is marginal. | Move router or add an AP closer. |
| IP address conflict | Another device has the same IP. | Use a DHCP reservation on the router. |
| AP fallback loop | Check Telnet log for `entering AP fallback mode`. | Fix SSID/password or triple-reset. |

---

### 7.5 OTA Update Failed

| Symptom detail | Cause | Fix |
|---|---|---|
| Browser shows "Authentication required" | HTTP password is set | Use username `admin` and your configured password |
| Device reboots to old firmware | Binary failed MD5 check | Confirm you are uploading the correct `.bin` for your board |
| "Not enough space" error | Filesystem too full | Free space via the File System Explorer at `/fsexplorer` |

---

### 7.6 Web UI Not Loading

1. Confirm the device is on the network: ping `<ip>` or try the IP directly.
2. mDNS resolution failure: on Windows without Bonjour, try the IP address.
3. Filesystem not mounted: flash the filesystem image via `uploadfs` or upload files via `/fsexplorer`.
4. Device in AP fallback mode: connect to the fallback SSID (`OTGW-XXXXXX`) and browse to `http://192.168.4.1`.

---

### 7.7 Serial Overrun and OTGW Online-Offline Flapping

**Root cause in versions before 2.0.0:** The `publishToSourceTopic()` function allocated approximately 1,600 bytes of local buffers on the cooperative stack. The ESP8266 CONT stack is only 4 KB. The fix in v2.0.0 converts those buffers to static or pre-allocated scratch memory.

If you see this on v2.0.0: update to the latest build. If the problem persists, connect to the Telnet debug console and look for `Exception` or `Stack smashing` messages.

---

### 7.8 LED Patterns

The firmware does not implement a dedicated LED status pattern on ESP8266 NodeMCU or Wemos D1 Mini boards.

The physical OTGW board itself has indicator LEDs driven by the PIC:
- **Green LED**: PIC is running and communicating.
- **Yellow LED**: Boiler communication active.
- **Red LED**: Error or fault condition.

---

### 7.9 Telnet Debug Output

The firmware streams a real-time debug log over Telnet on port 23.

#### Connecting

```
telnet otgw.local 23
```

Or use PuTTY: connection type "Other", protocol "Telnet", host `otgw.local`, port 23.

#### Reading Common Error Patterns

| Pattern in log | Meaning |
|---|---|
| `WiFi disconnected` / `WiFi reconnected` in short loops | Intermittent signal or IP conflict |
| `MQTT connect failed, rc=-2` | Broker not reachable; check IP and port |
| `MQTT connect failed, rc=4` | Wrong username or password |
| `OTGW offline` without `OTGW online` ever appearing | PIC not responding on serial |
| `Serial overrun` | Main loop blocked too long |
| `Exception (2)` | Illegal memory access; note the stack trace and report as a bug |
| `Exception (3)` | Load/store alignment error; PROGMEM pointer mismatch |
| `NTP: bogus initial time ignored` | Normal on first boot; SDK quirk guard working correctly |

---

### 7.10 Factory Reset

**Method 1: Web UI**
1. Open `http://otgw.local/fsexplorer`.
2. Delete the file `/settings.json`.
3. Reboot the device.

**Method 2: Triple-reset (WiFi credentials only)**
Power-cycle the device 3 times within 10 seconds.

**Method 3: Telnet debug console**
Connect on port 23 and type `resetwifi` to clear WiFi credentials only.

---

### 7.11 Where to Get Help

1. **GitHub Issues:** `https://github.com/rvdbreemen/OTGW-firmware/issues`
   Include: firmware version, board type, Telnet debug log, and settings (redact passwords).

2. **Discord:** Check the GitHub repository for the invite link.

3. **Tweakers.net forum:** The Dutch home automation community has a long-running OTGW discussion thread.
