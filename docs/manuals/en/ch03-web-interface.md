## Chapter 3: The Web Interface

### Overview

The OTGW web interface is a single-page application (SPA) served directly by the device itself, with no cloud service or external server required. Open it in any modern browser (Chrome, Firefox, or Safari) by navigating to `http://otgw.local/` or to the device's IP address: `http://<device-ip>/`.

The interface provides:

- A live dashboard with real-time OpenTherm data and temperature graphs.
- A full settings panel for all device configuration.
- A real-time OpenTherm message log with search and export.
- SAT thermostat controls and diagnostics.
- OTA firmware updates.
- A file manager for LittleFS.

The interface adapts to desktop and mobile screens and supports both light and dark themes.

### Navigation

The interface is organized into four main tabs, accessible from the navigation bar at the top of the page:

| Tab | Purpose |
|---|---|
| **Home** | Live OpenTherm log, real-time temperature graphs, boiler status |
| **SAT** | Smart thermostat configuration, heating curve, PID diagnostics |
| **Settings** | All device configuration: network, MQTT, NTP, hardware, security |
| **Advanced** | File manager, PIC firmware upgrade, device info, debug tools |

A header bar across the top shows the device name, firmware version, network status indicator, and theme toggle button.

### Dashboard (Home Tab)

The Home tab is the primary monitoring view. It has three main areas.

#### OpenTherm Log Viewer

The log viewer displays every OpenTherm message received from the bus in real time, streamed from the device over a WebSocket connection. Each line shows:

- A direction indicator (`>` for messages sent to the boiler, `<` for responses received)
- The OpenTherm message ID and decoded label (for example, `CH water temperature`)
- The decoded value with units
- A timestamp (toggled with the clock icon)

The log viewer includes:

- **Search/filter**: type a message ID number or label keyword to filter the displayed lines.
- **Auto-scroll**: the log follows new messages automatically. Click anywhere in the log to pause auto-scroll; click the scroll-to-bottom button to resume.
- **Export**: download the current log buffer as a text file using the export button.
- **Command bar**: send one-shot PIC commands (`TT=20.5`, `GW=R`, etc.) directly from the log view and see the response in-line.

The browser persists the log buffer to `localStorage` between sessions, so a page refresh does not lose recent data. The buffer is cleared automatically after a firmware or filesystem flash.

> **Simulation mode**: When the firmware is running in simulation mode (no physical OTGW connected), a `SIMULATION` badge appears in the log header. This mode is useful for testing dashboards and automations without hardware.

#### Real-Time Temperature Graph

Below the log viewer, an ECharts temperature graph shows a rolling 24-hour view of:

- Boiler flow temperature (CH water temperature)
- Return temperature
- Room temperature (from the thermostat or SAT)
- Outside temperature (if available)
- Modulation level (percentage)
- Flame status
- DHW (hot water) temperature

Dallas DS18B20 sensors are automatically discovered and added to the graph with dynamically assigned colors. Sensor labels (configured on the Settings page) appear in the graph legend.

The graph synchronizes its color scheme with the active light or dark theme.

#### Boiler Status

A status row at the top of the Home tab shows key boiler readings at a glance: flame on/off, CH setpoint, current flow temperature, room temperature, modulation percentage, and DHW status. These values update in real time as new OpenTherm messages arrive.

### Network Status Indicator

The header bar shows the current network status using a visual indicator:

- **Wi-Fi signal bars**: shows signal quality from 0 to 4 bars based on RSSI, using a quadratic mapping that reflects actual usability rather than raw dBm values.
- **AP badge**: displayed when the device is operating in AP or AP fallback mode.
- **Ethernet indicator**: displayed on OTGW32 when a wired Ethernet link is active.

Hovering over the indicator shows the exact SSID and signal quality percentage (Wi-Fi) or IP address (Ethernet).

### Settings Page

The Settings tab contains all device configuration, organized into sections. After changing any settings, click **Save** at the bottom of the section to apply and persist the changes.

#### Wi-Fi

| Setting | Description |
|---|---|
| Hostname | mDNS and DHCP hostname for the device (default: `otgw`) |
| Connected SSID | Displays the currently connected Wi-Fi network (read-only) |
| Reset WiFi | Clears saved credentials and reopens the captive portal on next boot |

To change the Wi-Fi network, click **Reset WiFi**, then reconnect to the `otgw-XXXXXX` AP and enter the new credentials.

#### MQTT

| Setting | Description |
|---|---|
| MQTT Broker | IP address or hostname of your MQTT broker |
| MQTT Port | Broker port (default: `1883`) |
| MQTT User | Broker username (leave blank if not required) |
| MQTT Password | Broker password |
| MQTT Top Topic | Prefix for all published topics (default: `OTGW`) |
| MQTT Unique ID | Device identifier used in topic paths and HA discovery (default: `otgw`) |
| HA Discovery | Enable/disable Home Assistant MQTT auto-discovery payloads |
| HA Prefix | Discovery topic prefix (default: `homeassistant`) |
| Publish Interval | Heartbeat interval in seconds (0 = publish every message as received) |
| Separate Sources | Publish per-source sub-topics (`/thermostat`, `/boiler`, `/gateway`) |

#### NTP and Time

| Setting | Description |
|---|---|
| NTP Server | Hostname or IP of the NTP server (default: `pool.ntp.org`) |
| Timezone | POSIX timezone string for local time display and boiler clock sync |

The firmware sends a clock synchronization command to the boiler PIC when NTP time is acquired.

#### Device

| Setting | Description |
|---|---|
| Hostname | Device hostname (also shown in the Wi-Fi section) |
| LED Pin | GPIO pin for the status LED |
| Dallas GPIO | GPIO pin for the 1-Wire DS18B20 bus (default: GPIO 10) |
| S0 Pin | GPIO pin for the S0 pulse counter input |
| S0 Pulses/kWh | Pulses per kWh from your energy meter |
| OLED Address | I2C address of the SSD1306 OLED display (typically `0x3C` or `0x3D`) |
| OLED Timeout | Display off timeout in seconds (0 = always on) |

#### Security

| Setting | Description |
|---|---|
| Endpoint Password | HTTP Basic Auth password for settings, file management, and OTA endpoints (leave blank to disable) |

When an endpoint password is set, the following actions require authentication:

- Reading and changing device settings
- File upload, download, and delete
- Reboot and Wi-Fi reset
- OTA firmware and filesystem updates
- Webhook test

Live monitoring, sensor values, and the WebSocket stream remain accessible without authentication.

#### Webhook

| Setting | Description |
|---|---|
| Enable Webhook | Enable outbound HTTP callback on OpenTherm status bit change |
| Trigger Bit | OpenTherm status bit to monitor (for example, flame on/off) |
| URL On | HTTP POST URL to call when the bit goes high |
| URL Off | HTTP POST URL to call when the bit goes low |
| Payload | POST body template |
| Content-Type | Content-Type header for the POST request |

A **Test** button sends a test webhook immediately so you can verify your endpoint is reachable.

> **Security note**: Webhook URLs are restricted to local network addresses to prevent server-side request forgery. Do not attempt to configure public internet URLs.

### Real-Time OpenTherm Log

The OpenTherm log on the Home tab is powered by a WebSocket connection to the device (port 80, path `/ws`). The firmware streams every decoded OpenTherm frame as it arrives. The browser maintains a circular buffer of recent messages and persists it to `localStorage`.

**Heap backpressure**: On the ESP8266, available RAM is limited. The firmware monitors free heap and automatically reduces WebSocket streaming frequency when memory is low:

- Above 8 KB free: immediate streaming (HEALTHY).
- 5-8 KB free: 50 ms throttle between frames (LOW).
- 3-5 KB free: 200 ms throttle (WARNING).
- Below 3 KB free: WebSocket streaming paused (CRITICAL).

This prevents the device from crashing under heavy load. You may notice the log slowing down or pausing briefly during peak activity; this is normal and expected behavior.

### OTA Firmware Update via the Web Interface

The **Update** section (accessible from the Advanced tab or the update icon in the header) provides two ways to update the firmware.

#### One-Click GitHub Release Update

1. The page fetches the list of available releases from GitHub and displays them with version badges: **Installed** (current version), **Update** (newer available), or **Rollback** (older version).
2. Click **Update** next to the target release.
3. The firmware binary is downloaded directly from GitHub to the device and flashed to the inactive OTA partition.
4. The device reboots automatically. The web interface polls `/api/v2/health` until the device responds, confirming a successful update.
5. Update the filesystem (LittleFS) using the same page to ensure web assets match the new firmware version.

#### Manual Binary Upload

1. Download or build a firmware binary (`.bin` file).
2. Use the file upload area on the Update page to select the file.
3. The device flashes the binary and reboots.

> **PIC firmware upgrade**: The Update page also supports upgrading the PIC microcontroller firmware. Select the correct PIC type (PIC16F88 or PIC16F1847), upload the `.hex` file, and follow the on-screen instructions. Never use OTmonitor to flash PIC firmware over Wi-Fi.

### File Manager (FSexplorer)

The **FSexplorer** is accessible from the Advanced tab. It provides a browser-based view of the LittleFS filesystem on the device, with the ability to:

- Browse all files stored in LittleFS.
- Upload new files (drag and drop or file select).
- Download existing files.
- Delete or rename files.

This is useful for manually backing up or restoring `settings.ini`, `dallas_labels.ini`, the SAT PID state file, or custom web assets. The file listing is sorted and filtered in the browser; hidden files (those with a `.` prefix) are not displayed.

> **File size limits**: Files larger than 10 KB should be streamed rather than loaded into RAM. The firmware always streams files rather than buffering them, so there is no hard upload size limit beyond the available LittleFS partition space.

### Light and Dark Theme

A sun/moon toggle button in the header switches between light and dark themes. The preference is saved in browser `localStorage` and persists across sessions and page refreshes. The real-time temperature graph automatically synchronizes its color scheme with the active theme.

### Browser Debug Console

For developers and advanced troubleshooting, the firmware exposes a diagnostic toolkit in the browser JavaScript console. Open the browser developer tools and type `otgwDebug` to see the available functions:

```
otgwDebug.status()     - current device status
otgwDebug.info()       - device information
otgwDebug.settings()   - current settings (sanitized)
otgwDebug.wsStatus()   - WebSocket connection state
otgwDebug.wsReconnect()- force WebSocket reconnection
otgwDebug.logs()       - recent log buffer
otgwDebug.api()        - run an API call
otgwDebug.health()     - health endpoint
otgwDebug.sendCmd()    - send a PIC command
otgwDebug.exportLogs() - download log buffer
otgwDebug.exportData() - download all buffered data
```

These tools are always available and do not require any configuration to enable.
