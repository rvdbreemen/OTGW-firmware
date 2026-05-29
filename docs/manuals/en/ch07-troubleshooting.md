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
4. **Be patient.** MQTT discovery now uses an async drip publisher that sends one discovery message every 3 seconds. With approximately 250 entities, a full discovery cycle takes roughly 12 to 15 minutes under normal conditions. Under heap pressure the interval slows to 30 seconds per entity, which can extend the total to over an hour. This gradual approach is intentional: it prevents the heap exhaustion that occurred in earlier firmware versions when all discovery messages were sent in a burst.
5. Check HA MQTT logs in Settings, System, Logs, filter for `mqtt`.
6. If you run multiple OTGW devices, each must have a unique `settingMQTTuniqueid`.
7. After a firmware update or MQTT reconnect, all discovery messages are re-queued automatically. You do not need to restart the device.

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

> **WARNING: Do NOT flash PIC firmware over WiFi.**
>
> The ESP firmware can be updated safely over WiFi (OTA), but the PIC microcontroller on the OTGW board is a separate chip with its own flashing path. Using OTmonitor over the TCP serial socket to upload PIC firmware (for example, upgrading to PIC v6.6) is **known to brick the PIC**: a WiFi hiccup mid-transfer leaves the PIC in a half-programmed state with no recovery path short of in-circuit reprogramming.
>
> Always flash PIC firmware over a **wired USB serial connection** using OTmonitor or the dedicated PIC programming tool. This is a hardware-level constraint, not a firmware bug, and it is not something the OTGW-firmware can work around.

---

### 7.6 Web UI Not Loading

1. Confirm the device is on the network: ping `<ip>` or try the IP directly.
2. mDNS resolution failure: on Windows without Bonjour, try the IP address.
3. Filesystem not mounted: flash the filesystem image via `uploadfs` or upload files via `/fsexplorer`.
4. Device in AP fallback mode: connect to the fallback SSID (`OTGW-XXXXXX`) and browse to `http://192.168.4.1`.

---

### 7.7 Heap Health Monitoring

The firmware continuously monitors free heap memory and classifies it into four tiers:

| Level | Free heap threshold | Behaviour |
|---|---|---|
| HEALTHY | Above 8,192 bytes | Normal operation. All services active. |
| LOW | 5,120 to 8,191 bytes | Message frequency begins to decrease. MQTT discovery drip interval slows from 3s to 30s. |
| WARNING | 3,072 to 5,119 bytes | WebSocket and MQTT messages are throttled. Non-essential transmissions are skipped. |
| CRITICAL | Below 3,072 bytes | All non-essential operations blocked. Emergency heap recovery is triggered automatically. |

The current heap level is logged every 60 seconds in the Telnet debug log. Look for lines starting with `Heap:` followed by the free byte count and the tier name. The Web UI footer also displays the current free heap value.

#### Emergency heap recovery

When free heap drops below the CRITICAL threshold, the firmware automatically attempts recovery by:

1. Reducing the MQTT buffer to its minimum size.
2. Yielding to let the network stack release pending buffers.

The Telnet log reports `Emergency heap recovery starting` and shows the bytes recovered. If the situation persists across reboots, consider enabling the nightly restart option (see section 7.7.1).

#### 7.7.1 Nightly restart

As a preventive measure against gradual heap fragmentation, the firmware supports an optional scheduled nightly restart. When enabled in Settings, the device reboots automatically at the configured hour (local time, based on your NTP timezone). The restart only fires if the device has been running for more than one hour, preventing restart loops after a recent reboot.

To enable: go to Settings in the Web UI and enable "Nightly Restart", then choose the preferred restart hour (default: 04:00).

---

### 7.8 Serial Overrun and OTGW Online-Offline Flapping

**Root cause in versions before 2.0.0:** The `publishToSourceTopic()` function allocated approximately 1,600 bytes of local buffers on the cooperative stack. The ESP8266 CONT stack is only 4 KB. The fix in v2.0.0 converts those buffers to static or pre-allocated scratch memory.

If you see this on v2.0.0: update to the latest build. If the problem persists, connect to the Telnet debug console and look for `Exception` or `Stack smashing` messages.

---

### 7.9 LED Patterns

The firmware drives status LEDs to indicate the device state. On ESP32 boards (OTGW32), LEDs are PWM-dimmed for reduced brightness.

| LED pattern | Meaning |
|---|---|
| LED2 blinks once per second, LED1 off | No WiFi connection. The device is attempting to reconnect. |
| LED1 blinks once per second, LED2 blinks twice per second | WiFi is connected but no OpenTherm traffic has been received for more than 10 seconds. Check the PIC and boiler connection. |
| Both LEDs off (normal operation) | WiFi connected, OpenTherm traffic flowing. LEDs are only used for brief blinks during processing. |
| Rapid blinks during boot | Setup phase. Three quick blinks signal completion of a setup step. |

The physical OTGW board itself has indicator LEDs driven by the PIC:
- **Green LED**: PIC is running and communicating.
- **Yellow LED**: Boiler communication active.
- **Red LED**: Error or fault condition.

LED blinking can be disabled entirely in Settings (`LED blink`).

---

### 7.10 Telnet Debug Output

The firmware streams a real-time debug log over Telnet on port 23. In addition to passive logging, the Telnet console accepts single-keypress commands for live diagnostics.

#### Connecting

```
telnet otgw.local 23
```

Or use PuTTY: connection type "Other", protocol "Telnet", host `otgw.local`, port 23.

#### Debug Menu Keys (v2.0.0)

Press `h` at any time to print the help menu with the current device status and toggle states. The set of keys changed compared to v1.3.5; if you are used to the old layout, consult the table below.

**Debug log toggles** (each key flips a category on or off):

| Key | Category |
|---|---|
| 1 | OT message parsing |
| 2 | API handling (REST) |
| 3 | MQTT module |
| 4 | Sensor modules |
| 5 | SAT control loop, cycles and HCR (enabled by default) |
| 6 | OTDirect frame handling and PI loop (enabled by default, ESP32 only) |
| g | MQTT interval gating (shows why a message was or was not published) |
| n | NTP time sync details |
| d | Dallas sensor simulation helper |

**Commands:**

| Key | Action |
|---|---|
| h | Show help menu with current status and toggle states |
| q | Force re-read of `settings.json` from flash |
| F | Force full Home Assistant discovery for ALL message IDs (clears the internal discovery bitmap and re-publishes everything) |
| r | Reconnect WiFi, Telnet, OTGW serial stream and MQTT |
| p | Reset the PIC manually |
| a | Send `PR=A` to the PIC to report firmware version, type and device ID |
| s / S | Toggle OTGW serial simulation replay (for development without a PIC) |
| b | Blink LED 1 five times |
| i | Initialize relay outputs |
| u | Drive the configured GPIO output ON |
| o | Drive the configured GPIO output OFF |
| j | Read the current GPIO output state |
| l | Toggle generic MyDEBUG flag |
| f | Show the MyDEBUG flag state |

The `F` key is useful after a Home Assistant MQTT wipe or topic-prefix change: it forces a complete re-publish of all discovery entries rather than waiting for the next natural trigger.

The `r` key is the fastest way to recover from a transient network hiccup without power-cycling the device.

#### Reading Common Error Patterns

| Pattern in log | Meaning |
|---|---|
| `WiFi disconnected` / `WiFi reconnected` in short loops | Intermittent signal or IP conflict |
| `MQTT connect failed, rc=-2` | Broker not reachable; check IP and port |
| `MQTT connect failed, rc=4` | Wrong username or password |
| `MQTT: heap before connect = XXXX` | Heap diagnostic logged just before each MQTT (re)connect; useful to correlate heap pressure with connect failures |
| `MQTT: heap after birth = XXXX` | Heap snapshot right after the birth/online publish |
| `MQTT: heap after discovery republish = XXXX` | Heap snapshot after a discovery republish batch |
| `Hour changed: X -> Y, heap=ZZZZ` | Hourly heartbeat from `hourChanged()`; a steadily falling heap here indicates a slow leak |
| `OTGW offline` without `OTGW online` ever appearing | PIC not responding on serial |
| `Serial overrun` | Main loop blocked too long, or UART buffer overflowed |
| `Exception (2)` | Illegal memory access; in v2.0.0 this class of crash in `strncmp_P`/`strstr_P` on binary data was fixed. If you still see it on 2.0.0, capture the crash log and file a bug. |
| `Exception (3)` | Load/store alignment error; almost always a PROGMEM pointer fed to a function that expects RAM. The well-known case from Arduino Core 3.1.2+ was fixed in 2.0.0. |
| `NTP: bogus initial time ignored` | Normal on first boot; the SDK sometimes returns `2106-02-07` before the first real sync. Guard is working correctly. |
| `Heap: XXXXX (HEALTHY)` | Normal heap status logged every 60 seconds |
| `Heap: XXXXX (LOW)` | Heap is getting low; message frequency is being reduced |
| `HEAP-CRITICAL: Blocking WebSocket` | Heap critically low; WebSocket messages are being dropped |
| `HEAP-CRITICAL: Blocking MQTT` | Heap critically low; MQTT messages are being dropped |
| `Emergency heap recovery starting` | Automatic recovery triggered due to critical heap |
| `[drip] publishing discovery for OT ID ...` | MQTT discovery publishing one entity at a time (normal) |
| `[drip] slowed to 30s (heap pressure)` | Discovery interval increased due to low heap |
| `Nightly restart triggered` | Scheduled restart executing at the configured hour |

---

### 7.10a Issues Fixed in v2.0.0 (What You May Still See in the Wild)

If you are coming from an older firmware or reading older forum threads, the problems below were common on v1.3.5 and earlier. They are fixed in v2.0.0. If any of these still occur on a confirmed v2.0.0 build, please file a GitHub issue with the crash log.

| Old symptom | Root cause (pre-2.0.0) | Status in 2.0.0 |
|---|---|---|
| Random `Exception (3)` reboots, often during MQTT activity | PROGMEM pointer passed to a function that does word-aligned reads (Arduino Core 3.1.2+ tightened this). A flash read at `0x402xxxxx` with word alignment triggers the exception. | Fixed. PROGMEM-safe helpers (`pgm_strncmp_PP`, `pgm_read_char`) used throughout; `writeMqttProgmemChunk()` used for PROGMEM to MQTT. |
| `Exception (2)` while parsing OT frames | `strncmp_P` / `strstr_P` used on binary data. These helpers assume C-strings and walk past embedded NULs. | Fixed. Binary compares now use `memcmp_P`. |
| Flood of MQTT messages and broker hammering right after a reconnect | Full MQTT discovery burst re-sent in one go on every reconnect, exhausting heap and overwhelming the broker. | Fixed. Discovery is now async and drip-published (see 7.3). On reconnect the bitmap is preserved; use `F` in the Telnet console to force a full re-publish if needed. |
| Clock jumps to `2106-02-07` shortly after boot | SDK sometimes hands back a bogus `time_t` before the first successful NTP exchange. | Fixed. NTP code now ignores the pre-epoch/post-2106 values until a real sync succeeds. Enable key `n` in the Telnet console to watch the handshake. |
| Device unreachable after the router reboots, until the OTGW is power-cycled | DHCP lease renewal raced with the WiFi reconnect logic, leaving the interface without an IP. | Fixed. DHCP renewal and WiFi recovery are coordinated (ADR-047 two-layer recovery logic). |
| OTGW online/offline flapping, serial overruns during MQTT bursts | `publishToSourceTopic()` allocated ~1.6 KB of local buffers on the 4 KB CONT stack. | Fixed. Large buffers converted to static or pre-allocated scratch memory. See section 7.8. |

---

### 7.11 Crash Log

Starting with v2.0.0, the firmware saves crash information to flash storage when an unhandled exception or watchdog reset occurs. After a crash reboot, this information is available in several ways:

1. **Web UI:** The Device Info section on the main page shows crash log details when available.
2. **REST API:** `GET /api/v2/device/crashlog` returns a JSON object with `available`, `summary`, and `details` fields.
3. **Telnet:** Register dumps and exception causes are printed during boot.

When reporting a bug on GitHub, include the crash log output. The `summary` field contains the exception type, and `details` contains register values (epc1, epc2, excvaddr, etc.) that help developers locate the crash in the code.

---

### 7.12 ESP32-Specific Troubleshooting (OTGW32)

The OTGW32 board uses an ESP32-S3 and introduces hardware capabilities not present on the ESP8266 version.

#### OTDirect (direct OpenTherm without PIC)

The OTGW32 has no PIC microcontroller. Instead, the ESP32-S3 handles the OpenTherm protocol directly via GPIO (OTDirect). If OpenTherm communication is not working:

1. Verify that the 18V step-up converter is enabled (hardware prerequisite for OT signalling).
2. Check the Telnet debug log for OTDirect-related messages. Enable debug channel 6 in the Telnet console to see detailed OTDirect frame handling.
3. Ensure the boiler and thermostat are connected to the correct MASTER and SLAVE terminals.

#### BLE Temperature Sensors

The ESP32 supports BLE (Bluetooth Low Energy) temperature sensor scanning. If BLE sensors are not appearing:

1. Confirm BLE scanning is enabled in Settings.
2. BLE sensors are polled on a timer-guarded interval. Allow at least one full scan cycle after enabling.
3. Check the Telnet debug log for BLE scan results.

#### Ethernet (W5500)

The OTGW32 board supports a W5500 Ethernet module as an alternative to WiFi:

1. Ethernet is only available on boards where `HAS_ETH_CAPABLE` is set (OTGW32).
2. When Ethernet is connected and active, WiFi can remain as a fallback.
3. Check the Telnet debug log for Ethernet link status messages.

---

### 7.13 Factory Reset

**Method 1: Web UI**
1. Open `http://otgw.local/fsexplorer`.
2. Delete the file `/settings.json`.
3. Reboot the device.

**Method 2: Triple-reset (forces the WiFi config portal)**
Press and release the hardware reset button 3 times in a row, each press within 10 seconds of the previous one (wait for the board to boot between presses). On the 3rd press the device boots into the WiFiManager config portal so you can enter new credentials. Use the reset button, not a power-cycle, and do not hold it: a held reset cannot be detected on ESP8266. See section 6.1.4 and `docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md`.

**Method 3: Telnet debug console**
Connect on port 23 and type `resetwifi` to clear WiFi credentials only.

---

### 7.14 Where to Get Help

1. **GitHub Issues:** `https://github.com/rvdbreemen/OTGW-firmware/issues`
   Include: firmware version, board type (ESP8266 or ESP32), Telnet debug log, crash log (if available), and settings (redact passwords).

2. **Discord:** Check the GitHub repository for the invite link.

3. **Tweakers.net forum:** The Dutch home automation community has a long-running OTGW discussion thread.
