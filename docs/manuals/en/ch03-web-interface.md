## Chapter 3: The Web Interface

### Overview

The OTGW web interface is a single-page application (SPA) served directly by the device itself, with no cloud service or external server required. Open it in any modern browser by navigating to `http://otgw.local/` or to the device's IP address: `http://<device-ip>/`.

The supported browsers are the current and previous two major versions of Chrome, Firefox, and Safari. The UI is English-only on the 2.0.0 line: localised Dutch strings that used to appear in the OTTHING-platform port have been removed (TASK-569).

The interface provides:

- A live dashboard with real-time OpenTherm data and temperature graphs.
- A full settings panel for all device configuration.
- A real-time OpenTherm message log with search and export.
- SAT thermostat controls and diagnostics (dedicated tab).
- OT-Direct status and bus override controls (OTGW32 only).
- OTA firmware updates.
- A file manager for LittleFS.

The interface adapts to desktop and mobile screens and supports both light and dark themes. A persistent firmware/filesystem mismatch banner appears at the top of every page when the running firmware build hash does not match the LittleFS image hash, with a direct link to the flash utility.

> **Two coexisting UIs**: The 2.0.0 firmware ships two web UIs on the same device: the original **Classic UI** (documented in the sections that follow) and a newer **v2 UI** (the "New UI"). Both are served from LittleFS and can be used interchangeably. A switch button in the header (**New UI** on the classic side, **Classic UI** on the v2 side) toggles between them. The choice is stored device-wide in `settings.ini` (the `ui_usev2` flag), not per-browser, so every browser that opens the device sees the selected UI. See "The v2 Web UI" below.

### The v2 Web UI

The v2 UI is a second, fully functional web interface that coexists with the Classic UI. It targets the same firmware and the same REST/WebSocket endpoints, so both UIs stay in sync; you can move between them at any time without losing configuration.

**Selecting the UI.** The switch is device-wide, not per-browser. Clicking **Classic UI** in the v2 header (or **New UI** in the classic header) writes the `ui_usev2` flag to `settings.ini` via the REST API, then reloads the page so the firmware serves the chosen UI. No hard reload (CTRL-R / cache clear) is needed, and the other UI keeps working unchanged. The switch retries automatically if the device is briefly busy, and only reloads after the change is confirmed.

**Header and navigation.** The v2 header is a dark strip in both light and dark themes. It shows identity chips (hostname and firmware version, plus IP address and Wi-Fi signal bars), a connectivity status pill, a live connectivity summary strip, a clock, the UI-switch and theme buttons, and a **SIMULATION** badge that appears only while OT-replay simulation is active. Main navigation is a row of underlined text tabs: **Home**, **SAT**, **Monitor**, **Settings**, and **Advanced**. Sub-navigation within a page uses the same underlined text-tab style.

#### Home page

The Home page offers several layouts through a **View** picker dropdown at the top:

- **System view**: a live schematic of the heating system (boiler or heat pump, flow and return pipes, radiators, room, DHW, pressure and modulation readouts).
- **At a glance**: a phone-first layout built around a single large room-temperature dial with step buttons.
- **Mission control**: a live strip chart (flow / return / setpoint / modulation) plus a raw OpenTherm frame ticker.

When the gateway is injecting values onto the OT bus (active gateway overrides), an "injected" badge appears on the Home schematic and on the Connection map; clicking it opens a floating detail panel listing the active overrides.

#### Monitor page

The Monitor page has five sub-tabs:

- **Log**: a live console fed by the WebSocket stream, one OpenTherm frame per line (dense rendering). A toolbar carries Pause, **Clear**, **Download**, a frame filter, and toggle chips for Auto-scroll, Timestamps, **SAT only** (show only SAT narration lines), **Auto-download** (save the log every 15 minutes), and **Stream to file** (append frames straight to a local file in Chrome/Edge). Below the toolbar a command bar sends a raw OTGW command (for example `PS=1`, `TT=20.5`, `GW=1`); the command echoes into the log and the prompt label reflects the live command interface (a `PIC ›` prompt on PIC hardware, `OT ›` on OT-Direct).
- **Statistics**: a per-message-ID table (msgID, description, direction, interval, count, value) with a search box, followed by an **Active gateway overrides** panel and a **Boiler unsupported messages** panel.
- **OT Support**: a matrix of all 128 OpenTherm message IDs colour-coded by where each ID was seen (thermostat + boiler, thermostat only, boiler only, or never seen). Click a cell to pin a detail panel with the decoded message.
- **Graph**: a live flow / return / setpoint / modulation chart with window chips (10 min, 1 hour, 4 h, 24 h), one-off **PNG** and **CSV** export buttons, and **Auto-PNG** / **Auto-CSV** chips for periodic auto-save.
- **Connection**: an OT-bus and connectivity map. The OT bus is modelled as two separate links (thermostat and boiler), and the map separates **MODE** (a setting, such as gateway or monitor) from **HEALTH** using a five-state vocabulary: Connected, Degraded, Disconnected, Off, and Unknown (ADR-155). Nodes cover thermostat, boiler, OTGW, router (with a Wi-Fi signal-strength icon and dBm), MQTT broker, and this browser. On OT-Direct hardware an **OT-Direct overrides** panel lets you apply or clear stored responses and response modifiers (SR/CR/RM/CM/UI/KI) directly.

#### SAT page

A dedicated Smart Autotune Thermostat page provides the thermostat control surface in three cumulative depth layers: **Thermostat** (simple), **Control** (operational), and **Technical** (the control loop). It is bound to the SAT REST endpoints. For full details of SAT, presets, the heating curve, and diagnostics, see Chapter 5.

#### Settings page

The Settings page is driven by the REST API and presents settings with human-readable labels, categories, and hints, plus a search box and a rail of category links. A save bar tracks unsaved changes with **Discard** and **Save settings** actions. Additional panels include a **BLE sensor** roster (discovered Bluetooth Low Energy sensors) and, in the Webhook group, a **Send test call** action that fires the real saved ON webhook. Advanced OT-Direct and SAT settings are exposed here as well.

#### Advanced page

The Advanced page collects the power-user screens in four sub-tabs:

- **PIC firmware**: shows PIC device, type, and firmware version, a "Check for updates" action, a list of available firmware files with a flash progress bar, and a cached gateway-settings (`PR=`) table. Hidden on boards without a PIC (OT-Direct / OTGW32).
- **Debug Information**: device info groups, the crash log (or a "no crash logs" note), friendly Wi-Fi labels, and a raw debug dump.
- **File System**: an in-page FSexplorer (browse, upload, usage bar) with a link to the classic `/FSexplorer`.
- **System**: live device status (link, OT-rewrite mode, simulation), and **System Actions** buttons: Update Firmware (OTA sketch / filesystem), ReBoot, **Run setup wizard**, Reset Wireless, and Home.

#### First-time setup wizard

On a genuinely fresh device the v2 UI shows a first-time-setup onboarding wizard once (existing installs are migrated so it never appears again). It can be re-run at any time from **Advanced > System > Run setup wizard**.

#### Mobile use and asset caching

The v2 UI is designed to be usable on a smartphone. Its assets are served with a `no-cache` policy (ADR-163), so after a filesystem OTA update the browser picks up the new assets immediately without a manual cache clear.

### Navigation

The remaining sections of this chapter describe the **Classic UI**. The Classic UI is organized into main tabs, accessible from the navigation bar at the top of the page:

| Tab | Purpose |
|---|---|
| **Home** | Live OpenTherm log, real-time temperature graphs, boiler status |
| **SAT** | Smart thermostat dashboard: temperature cards, presets, heating curve, PID diagnostics |
| **Settings** | All device configuration: network, MQTT, NTP, hardware, nightly restart, security |
| **Advanced** | File manager, PIC firmware upgrade, device info, debug tools |

A header bar across the top shows the device name, firmware version, network status indicator (Wi-Fi signal bars or Ethernet icon), heap memory display, and theme toggle button.

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
- **Timestamps**, **Capture mode** (in-memory buffer up to about 1 M lines), and **Stream to file** (live append to a local file in Chrome/Edge).
- **Download** the current buffer as a text file; **Auto-download** writes a snapshot every 15 minutes.
- **Command bar**: send one-shot commands (`TT=20.5`, `GW=R`, etc.) directly from the log view and see the response in-line. On ESP8266, these are PIC commands. On OTGW32, the command bar is also available when OT-Direct provides the command interface.

The OpenTherm Monitor panel exposes three sub-tabs:

- **Log**: the live streamed log described above.
- **Statistics**: a sortable per-message-ID table with hex/decimal ID, direction, description, interval, and last value. Useful for spotting bus traffic patterns at a glance.
- **Graph**: an ECharts time-series view with a configurable window (10 minutes to 24 hours), one-off **Screenshot** and **Export Data** (CSV) buttons, and matching **Auto-save PNG** / **Auto-save CSV** checkboxes for periodic snapshots.

The browser persists the log buffer to `localStorage` between sessions, so a page refresh does not lose recent data. The buffer is cleared automatically after a firmware or filesystem flash.

> **Simulation mode**: When the firmware is running in simulation mode (no physical OTGW connected), a `SIMULATION` badge appears in the log header. This mode is useful for testing dashboards and automations without hardware.

#### OT-Direct Status Panel (OTGW32 Only)

On OTGW32 devices with OT-Direct available, an OT-Direct status panel appears above the OpenTherm log on the Home tab. This panel shows:

- **Mode**: the current OT-Direct operating mode (Bypass, Gateway, Monitor, Master/Standalone, or Loopback Test).
- **OT Bus**: whether the OpenTherm bus is online, with a status indicator dot.
- **Thermostat**: whether a thermostat is detected on the bus.
- **Boiler**: whether the boiler is responding.
- **Setback / Step-Up**: whether setback or step-up overrides are active.
- **Schedule**: number of active schedules.
- **Active Overrides**: count of bus-level overrides currently in effect. Click to expand and see the override list.

The expanded override section lets you apply new overrides directly from the browser. Select an override action (SR, CR, RM, CM, UI, KI), enter the message ID and optional hex value, then click **Apply**. This is an advanced feature intended for diagnostics and testing.

The panel polls `/api/v2/otdirect/status` every 5 seconds while the Home tab is visible.

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

### SAT Tab

The SAT (Smart Autotune Thermostat) tab provides a dedicated dashboard for the built-in thermostat functionality. The tab is always visible in the navigation bar. When SAT is disabled, the dashboard shows the current state and an enable toggle.

#### Header and Controls

The SAT header bar contains:

- **Status badge**: shows "Disabled", "Idle", "Heating", or the current SAT state.
- **Simulation badge**: shown when SAT simulation mode is active.
- **Enable toggle**: a switch to enable or disable SAT without navigating to settings.
- **View selector**: choose between Thermostat (simple), Expert, and Diagnostics views. Each view shows progressively more detail.
- **Settings button**: navigates to the SAT Settings page for full configuration.

#### Temperature Cards

Four temperature cards are always visible across all views:

| Card | Description |
|---|---|
| Room Temperature | Current room temperature reading |
| Target Temperature | Desired setpoint, with +/- buttons to adjust in 0.5 degree steps |
| Outside Temperature | Outdoor temperature from OT bus or weather service |
| Boiler Setpoint | Current calculated boiler flow setpoint |

#### Presets and Modes

A row of preset buttons (Activity, Away, Eco, Home, Comfort, Sleep) lets you switch temperature presets with a single click. Below the presets, mode buttons select between Continuous and PWM heating modes. A simulation toggle is also available.

#### Expert and Diagnostics Views

The Expert view adds sections for:

- **Weather data**: outdoor temperature, humidity, wind speed from Open-Meteo (if enabled).
- **Temperature history chart**: an ECharts graph within the SAT dashboard.
- **Heating curve (Stooklijn)**: a collapsible chart showing the calculated heating curve.
- **Control status**: current mode, boiler status, active preset, heating system type, PID output, error, coefficient, deadband, overshoot margin, modulation values, and OVP calibration.
- **PID controller**: individual P, I, D terms and tuning parameters (Kp, Ki, Kd).
- **PWM and cycle tracking**: duty cycle, flame status, cycle count, overshoot data.
- **Smart features**: solar gain, summer mode, thermal learning, comfort offset, simmer index, auto-tune status.
- **External sensors**: status indicators for indoor and outdoor sensor sources.

The Diagnostics view adds:

- **Health indicators**: colored dots for Device, Cycle, Flame, Pressure, Setpoint Sync, and Modulation Sync health.
- **Simulation and diagnostics**: simulation mode details, fallback status, OVP phase and value.
- **Raw data**: collapsible section showing the raw JSON data from the SAT API.

#### SAT Settings Page

Clicking the **Settings** button in the SAT header opens a dedicated SAT Settings page. Settings are organized into collapsible groups: Thermostat, Heating, PID, DHW, Pressure, Smart Features, Safety, Energy, Weather, Sync, and Advanced. Each group has its own **Save** button to persist changes individually.

Two extra panels appear above the generic settings groups on ESP32 builds:

- **BLE Sensors**: lists Bluetooth Low Energy sensors discovered by the firmware (Xiaomi LYWSD03MMC with ATC/pvvx custom firmware, BTHome v2). Each row shows the MAC, an editable name, the freshness of the last advertisement, and **Select** / **Forget** actions. The roster is capped at eight entries and is also exposed over MQTT for Home Assistant. The `SATblemac` field below the panel is read-only context: the active sensor is chosen here rather than typed by hand (TASK-508).
- **DS18B20 Area Sensor Mapping**: maps each SAT zone area to a discovered DS18B20 sensor via dropdowns. The selected mapping is forwarded to SAT on every poll cycle so a wired sensor can act as the room-temperature source for a specific area.

#### DHW Controls

A DHW (Domestic Hot Water) section is visible on all SAT views. It provides a slider to set the hot water setpoint temperature (40-60 degrees C) and an optional force switch to manually trigger hot water heating.

### Network Status Indicator

The header bar shows the current network status using a visual indicator:

- **Wi-Fi signal bars**: shows signal quality from 0 to 4 bars based on RSSI, using a quadratic mapping that reflects actual usability rather than raw dBm values. The bars are visible in both light and dark themes.
- **AP badge**: displayed when the device is operating in AP or AP fallback mode.
- **Ethernet indicator**: displayed on OTGW32 when a wired Ethernet link is active, replacing the Wi-Fi icon.

Hovering over the indicator shows the exact SSID and signal quality percentage (Wi-Fi) or IP address (Ethernet).

The network indicator updates automatically via the periodic device time poll (`/api/v2/device/time`) and on initial page load from `/api/v2/device/info`.

### Heap Memory Display

The header bar shows a heap memory indicator displaying the current free heap and largest free block in bytes (for example, `Heap: (12480 / 8192)`). This information is updated with every device time poll and provides a quick way to monitor device memory health without opening the debug console.

### Settings Page

The Settings tab contains all device configuration, organized into sections. After changing any settings, click **Save** at the bottom of the section to apply and persist the changes.

#### Wi-Fi

| Setting | Description |
|---|---|
| Hostname | mDNS and DHCP hostname for the device (default: `otgw`) |
| Wi-Fi Network (SSID) | Read-only display of the currently connected Wi-Fi network |
| Reset WiFi | Button next to the SSID field. Clears the stored Wi-Fi credentials and reboots the device in Access Point (AP) mode |

To move the gateway to a different Wi-Fi network, click **Reset WiFi** and confirm the prompt. The device reboots, starts an AP named `OTGW-XXXXXX` (where `XXXXXX` is derived from the chip ID), and reopens the captive portal so you can select and authenticate against the new network. The Reset WiFi action is also available under **FSexplorer > System Actions** as the *Reset Wireless* button.

#### Wi-Fi Scan (Settings page)

The Settings page also includes a Wi-Fi scan panel that lists nearby networks (TASK-585). Press **Scan** to call `/api/v2/network/scan`; the result populates a dropdown with the detected SSIDs, RSSI, and security flags. Selecting a network from the dropdown fills the SSID field so you can pair the scan with the Reset WiFi flow without typing the SSID by hand.

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

#### Nightly Restart

| Setting | Description |
|---|---|
| Scheduled Nightly Restart | Enable or disable a daily automatic device restart to recover heap memory |
| Nightly Restart Hour | Local hour (0-23) when the restart runs (default: `4`, meaning 04:00) |

The nightly restart causes a brief service interruption of approximately 30 seconds. It is only active when NTP is enabled and synced, so the device knows the correct local time. This feature is useful on ESP8266 where long uptimes can lead to heap fragmentation.

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

#### OT-Direct Mode (OTGW32 Only)

On OTGW32 devices, a dropdown appears in the settings to select the OT-Direct operating mode:

| Mode | Description |
|---|---|
| Bypass | Thermostat connected directly to boiler via relay; no OT processing |
| Gateway | Full override processing (default) |
| Monitor | Transparent pass-through; frames are logged but not modified |
| Master / Standalone | No thermostat required; OTGW32 acts as sole OT master |
| Loopback Test | Simulated boiler data for testing (no real boiler communication) |

Changing the mode prompts a confirmation dialog explaining the consequences before applying.

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

The 2.0.0 firmware issues the HTTP Basic Auth challenge on the root page (`/`) up front, so the browser caches the credentials before any REST call is dispatched from the loaded UI. This avoids a mid-session popup the first time an authenticated API endpoint is called. The server also collects the `Origin` and `Referer` headers on every request so the REST layer can enforce a same-origin CSRF check (ADR-056).

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

The OpenTherm log on the Home tab is powered by a plain WebSocket connection to the device (`ws://<device>/ws` on port 80). The firmware streams every decoded OpenTherm frame as it arrives. The browser maintains a circular buffer of recent messages and persists it to `localStorage`.

> **HTTP/WS only**: The firmware does not support HTTPS or WSS. This is a deliberate design decision for a trusted-LAN device with limited ESP8266 resources. The REST API can be exposed through an HTTPS reverse proxy, but the live OpenTherm log assumes a direct plain WebSocket to the device and will not work through an HTTPS proxy that does not also bridge `ws://` to `wss://`. For remote access, use a VPN.

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

The **FSexplorer** is accessible from the Advanced tab dropdown under **File system contents**, or directly at `http://otgw.local/FSexplorer.html`. It provides a browser-based view of the LittleFS filesystem on the device.

Features:

- Browse files and subdirectories. Directories are listed first and alphabetically sorted (case-insensitive). A `.. (Parent)` link appears in subdirectories for navigation.
- Upload new files via the **Upload File** form. A progress bar shows upload status, and the file size is checked against available free space before upload is allowed.
- Download any file by clicking **Download** on its row.
- Delete files with the **Delete** link.
- View storage usage at the bottom of the listing (used and total bytes of the LittleFS partition).

A **System Actions** panel below the file list provides quick buttons for **Update Firmware** (desktop browsers only), **ReBoot**, **Reset Wireless** (same action as Reset WiFi on the Settings page), and **Exit FSexplorer** (returns to the main page).

This is useful for manually backing up or restoring `settings.ini`, `dallas_labels.ini`, the SAT PID state file, or custom web assets. The firmware always streams files rather than buffering them into RAM, so there is no hard upload size limit beyond the available LittleFS partition space. The main `index.html` (now several tens of kilobytes on 2.0.0) and all other assets are served via `streamFile()` to avoid loading them into the limited ESP8266 heap (TASK-668).

When the FSexplorer routes detect that `index.html` is missing (filesystem not mounted or image not yet flashed), the routes serve `FSexplorer.html` directly so the user can still upload a filesystem image from the file manager.

#### Asset caching and the ETag flow

The web UI uses an ETag-based caching scheme that combines fast revalidation with a versioned URL strategy for JavaScript assets:

- `index.html` is served with the current filesystem hash as its `ETag`. The browser caches the page but always revalidates via `If-None-Match`. Unchanged filesystem returns `304 Not Modified`; a flashed filesystem returns `200` with fresh content.
- `index.js` and `graph.js` are loaded with a `?v=<fsHash>` query string. Versioned requests get `Cache-Control: public, max-age=86400`; a bare request (no `?v=`) gets `Cache-Control: no-cache` so that a stale URL never serves stale JavaScript.
- If a pre-gzipped `index.js.gz` or `graph.js.gz` exists on the LittleFS image, the server prefers it and lets `streamFile()` emit `Content-Encoding: gzip` automatically (TASK-304/TASK-433).

When an endpoint password is configured, an HTTP Basic Auth challenge is issued on the `/` route up front so the browser caches credentials before any API call runs. This avoids a mid-session popup when an authenticated REST call is dispatched from the loaded page. The server also collects `Origin` and `Referer` headers so the REST API can enforce the same-origin CSRF check (ADR-056).

### Light and Dark Theme

A sun/moon toggle button in the header switches between light and dark themes. The preference is saved in browser `localStorage` and persists across sessions and page refreshes. The real-time temperature graph and SAT charts automatically synchronize their color scheme with the active theme.

All UI elements, including Wi-Fi signal bars, status indicators, and the OT-Direct panel, render correctly in both themes.

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
