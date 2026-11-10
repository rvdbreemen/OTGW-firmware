## Chapter 7: Troubleshooting

This chapter covers the most common problems, their root causes, and step-by-step remedies. When in doubt, start with the Telnet debug log (section 7.10) and work forward from there.

### 7.1 OTGW Appears Offline After Boot

**Symptom:** The web UI shows the OTGW as offline. MQTT publishes nothing. The live log is empty.

| Check | What to look for |
|---|---|
| PIC serial connection | A loose or broken UART wire between ESP and PIC is the most common hardware cause |
| PIC powered | The OTGW PIC needs power from the OpenTherm bus or an external supply |
| PIC firmware mode | If in diagnostic or interface mode from a previous OTmonitor session, use the web UI to reset the PIC |

**Steps:**
1. Connect to the Telnet debug console (see section 7.10).
2. Look for `OTGW online` or `OTGW offline` messages.
3. If you see `OTGW offline` repeating, the PIC is not responding on serial.
4. Power-cycle the entire OTGW board (not just the ESP).

---

### 7.1a PIC Not Detected on an OTGW Classic with an S3 Mini

**Symptom:** you moved your OTGW Classic (the NodoShop board with the PIC chip) from a Wemos D1 mini to a LOLIN S3 Mini and flashed 2.0.0. The web UI comes up, but there is no OpenTherm traffic. Open `http://<ip-of-your-otgw>/api/v2/device/info` in a browser and look for these two fields:

```json
"hardwaremode": "Degraded",
"otcommandinterface": "None",
```

On a working Classic they read `"PIC"` and `"PIC"`. `Degraded` means the firmware looked for the PIC at boot and got no answer. `None` means it has no way to talk OpenTherm.

#### Quick checks (start here)

1. **Check how the S3 Mini sits in the socket.** The S3 Mini has two pin rows on each side. Only the **outer** rows match the Wemos D1 mini footprint; the inner rows carry extra S3 pins. Make sure the outer rows are the ones in the socket. A module shifted by one row, or a pin that is bent or not pushed in all the way, cuts the serial link to the PIC. Look at the board from the side: every outer pin should go fully into the socket.
2. **Power-cycle the whole board.** Remove the power, wait ten seconds, power it again. A reboot from the web UI restarts the ESP only.
3. **Check Board mode.** In the web UI go to Settings, System, **Board mode**. **Auto-detect** (the default) and **PIC (Classic)** both work on a Classic. While you are troubleshooting, **PIC (Classic)** is the better choice: it keeps the serial link to the PIC open and keeps asking the PIC once a minute (see below). Save and reboot. Do not choose **OT-Direct (no PIC)** on a Classic: that mode is for a board without a PIC.
4. **Cross-check with the old module.** If you still have the D1 mini with 1.x firmware, put it back. If 1.x sees the PIC, the PIC and the board are fine and the problem sits between the S3 Mini and the socket.

Still `Degraded`? Collect the evidence below and ask for help (section 7.14).

#### What the firmware does at boot

Each boot, after WiFi is up, the firmware looks for the PIC. It resets the PIC and waits for the end-of-text byte (ETX, 0x03) that the PIC's bootloader sends after a reset. An ETX means a PIC is there and the mode becomes `PIC`. No ETX means `Degraded`.

The **Board mode** setting decides how often it tries and what it remembers (ADR-201):

| Board mode | What happens at boot |
|---|---|
| Auto-detect (0, default) | Up to 3 tries, 60 ms apart. A found PIC is saved as "Classic", and from then on every boot takes the forced-PIC path, which still checks the PIC. A miss is **not** saved, so the next boot tries again. On an S3 Mini without a PIC answer the firmware stays `Degraded` and does not switch to OT-Direct. |
| PIC (Classic) (1) | One try per boot. No answer gives `Degraded`. |
| OT-Direct (no PIC) (2) | Checks once for a PIC. If one answers, it switches back to Classic and saves that. If none answers, it starts OT-Direct, which is wrong for a Classic. |

While no PIC is known, the firmware logs `PIC is unknown, probe pic using PR=A` once a minute and sends `PR=A` to the PIC. In Board mode **PIC (Classic)** the serial link stays open, so if the PIC answers, the mode changes to `PIC` without a reboot. In **Auto-detect** the firmware closes the serial link after a missed boot detection; the log line still appears, but only the next reboot really tries again.

#### The boot record: `/bootdetect.log`

The boot detection runs **before** the Telnet server starts, so a Telnet capture never shows it, not even across a restart. The firmware writes the result to a small file on the device instead. Open the FSexplorer (`http://<ip>/FSexplorer.html`) and download `/bootdetect.log`. The newest boot is on top; the file keeps the last 30 boots.

A Classic with an S3 Mini that found its PIC:

```text
boot#12 t=6843ms eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36
```

A Classic whose PIC did not answer:

```text
boot#13 t=6851ms eMode=3 pic=0 mode=0 pro=0 RST=12 RX=44 TX=43 I2C=35/36
```

(The `t=` values above are examples; yours will differ.)

| Field | Meaning |
|---|---|
| `eMode` | Result: `0` not yet detected, `1` PIC, `2` OT-Direct, `3` Degraded |
| `pic` | `1` when the PIC answered |
| `mode` | The Board mode setting after detection: `0` auto, `1` PIC on S3 Mini, `2` OT-Direct, `3` PIC on S3 Mini Pro |
| `pro` | `1` when an S3 Mini Pro was recognised (by its on-board motion sensor) |
| `RST` `RX` `TX` | GPIOs used for PIC reset and the serial link. S3 Mini: 12, 44, 43 |
| `I2C` | SDA/SCL pins. S3 Mini: 35/36 |

If `pic=0` on every line while `RST`, `RX` and `TX` show 12, 44 and 43, the firmware used the right pins and the PIC did not answer. That points at the socket seating, the module or the PIC itself, not at a setting.

#### What to send when you ask for help

1. `/bootdetect.log` (download from the FSexplorer).
2. The output of `/api/v2/device/info`.
3. A capture with the capture tool from the flash zip (`capture-mqtt-debug.bat` on Windows, `capture-mqtt-debug.sh` on Linux, `capture-mqtt-debug-macos.sh` on macOS). Start it, reboot the gateway from the web UI while it runs, let it run a few minutes, stop with `Q`, and post the upload zip. It shows the per-minute `PR=A` lines and the reboot log; the boot detection itself comes from `/bootdetect.log`.
4. Side photos of both pin rows of the S3 Mini in the socket.
5. The firmware version and the board revision printed on the PCB.

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
3. Filesystem not mounted: install the release zip again on the `/update` page, or upload files via `/fsexplorer`.
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

#### What is a serial overrun?

A serial overrun happens when the PIC sends more data to the ESP than the firmware reads before the UART buffer is full. Individual bytes are lost. The firmware detects this through the UART hardware overrun flag.

#### Symptoms

- A `Serial Overrun` line in the Telnet debug console.
- A `Serial Overrun` event in the Web UI event log and on the MQTT `event_report` topic. The event is suppressed during the first 15 seconds after the PIC starts.
- Incomplete OpenTherm lines in the debug log.
- OTGW availability that switches quickly between `online` and `offline`.

#### What to do

1. **Check the MQTT connection.** A busy or unstable MQTT connection slows processing and can cause indirect overruns (see section 7.2).
2. **Check the free heap.** Low memory slows the firmware down (see section 7.7).
3. **Check for OTmonitor on port 25238.** OTmonitor that is active and sending commands adds load.
4. **Rule out other causes of flapping:** WiFi instability (section 7.4) or an MQTT broker restart.
5. **Capture evidence.** Connect to the Telnet debug console and note any `Exception` or `Stack smashing` messages, then attach the crash log (section 7.11) and the Telnet log to a GitHub issue.

On the 1.x line this was caused by large stack buffers in `publishToSourceTopic()`; on 2.x that function uses a static buffer.

---

### 7.9 LED Patterns

The firmware drives status LEDs to indicate the device state. On the S3 Mini the LEDs are PWM-dimmed for reduced brightness.

| LED pattern | Meaning |
|---|---|
| LED2 blinks once per second, LED1 off | No WiFi connection. The device is attempting to reconnect. |
| LED1 blinks once per second, LED2 blinks twice per second | WiFi is connected but no OpenTherm traffic has been received for more than 10 seconds. Check the PIC and boiler connection. |
| Both LEDs off (normal operation) | WiFi connected, OpenTherm traffic flowing. LEDs are only used for brief blinks during processing. |
| Rapid blinks during boot | Setup phase. Three quick blinks signal completion of a setup step. |

An OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro also uses the LOLIN's on-board RGB pixel to show the WiFi state. It flashes once per second (the `LED blink` setting does not affect it):

| Pixel colour | Meaning |
|---|---|
| Red | WiFi not connected. |
| Purple | An access point is open: the WiFi config portal, or the AP fallback when the saved network is unreachable at boot. Connect to it to configure WiFi. |
| Green | WiFi connected. |

`/api/v2/device/info` reports the driven pixel GPIO as `status_pixel_gpio` (`0` when none).

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
telnet otgw2.local 23
```

Or use PuTTY: connection type "Other", protocol "Telnet", host `otgw2.local`, port 23.

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
| 6 | OTDirect frame handling (enabled by default) |
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
| OTGW online/offline flapping, serial overruns during MQTT bursts (1.x line) | `publishToSourceTopic()` allocated ~1.6 KB of local buffers on the stack (1.x line). | Fixed. The buffer is static. See section 7.8. |

---

### 7.11 Crash Log

Starting with v2.0.0, the firmware saves crash information to flash storage when an unhandled exception or watchdog reset occurs. After a crash reboot, this information is available in several ways:

1. **Web UI:** The Device Info section on the main page shows crash log details when available.
2. **REST API:** `GET /api/v2/device/crashlog` returns a JSON object with `available`, `summary`, and `details` fields.
3. **Telnet:** Register dumps and exception causes are printed during boot.

When reporting a bug on GitHub, include the crash log output. The `summary` field contains the exception type, and `details` contains register values (epc1, epc2, excvaddr, etc.) that help developers locate the crash in the code.

---

### 7.12 BLE Temperature Sensors

The S3 Mini supports BLE (Bluetooth Low Energy) temperature sensor scanning. If BLE sensors are not appearing:

1. Confirm BLE scanning is enabled in Settings.
2. BLE sensors are polled on a timer-guarded interval. Allow at least one full scan cycle after enabling.
3. Check the Telnet debug log for BLE scan results.

---

### 7.13 Factory Reset

**Method 1: Web UI**
1. Open `http://otgw2.local/fsexplorer`.
2. Delete the file `/settings.json`.
3. Reboot the device.

**Method 2: Triple-reset (forces the WiFi config portal)**
Press and release the hardware reset button 3 times in a row, each press within 10 seconds of the previous one (wait for the board to boot between presses). On the 3rd press the device boots into the WiFiManager config portal so you can enter new credentials. Use the reset button, not a power-cycle, and do not hold it. See section 6.1.4 and `docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md`.

**Method 3: Telnet debug console**
Connect on port 23 and type `resetwifi` to clear WiFi credentials only.

---

### 7.14 Where to Get Help

1. **GitHub Issues:** `https://github.com/rvdbreemen/OTGW-firmware/issues`
   Include: firmware version, board (for example OTGW Classic with LOLIN S3 Mini), Telnet debug log, crash log (if available), and settings (redact passwords).

2. **Discord:** Join the OTGW Discord server: `https://discord.gg/zjW3ju7vGQ`.

3. **Tweakers.net forum:** The Dutch home automation community has a long-running OTGW discussion thread.
