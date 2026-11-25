## Chapter 3: The Web Interface

### Overview

The OTGW web interface is a single-page application (SPA) served directly by the device itself, with no cloud service or external server required. Open it in any modern browser by navigating to `http://otgw2.local/` or to the device's IP address: `http://<device-ip>/`.

The supported browsers are the current and previous two major versions of Chrome, Firefox, and Safari. The UI is English-only on the 2.0.0 line: the localised Dutch strings it used to carry have been removed (TASK-569).

The interface provides:

- A live dashboard with real-time OpenTherm data and temperature graphs.
- A full settings panel for all device configuration.
- A real-time OpenTherm message log with search and export.
- SAT thermostat controls and diagnostics (dedicated tab).
- OT-Direct status and bus override controls (only on hardware without a PIC).
- OTA firmware updates.
- A file manager for LittleFS.

The interface adapts to desktop and mobile screens and supports both light and dark themes. A persistent firmware/filesystem mismatch banner appears at the top of every page when the running firmware build hash does not match the LittleFS image hash, with a direct link to the flash utility.

> **One UI**: The 2.0.0 firmware ships one web UI, the **v2 UI**, served at `http://<device>/` (and at `/index.html` for old bookmarks). The classic UI and its **Classic UI** / **New UI** switch were removed in TASK-1223.12 so the filesystem fits the smaller LittleFS partition of the WiFi-update layout (ADR-191). The features the classic UI had and v2 does not are listed in `docs/history/2026-10-15-retired-classic-web-ui-features.md`.

### The v2 Web UI

The v2 UI is the device's web interface. It uses the same firmware REST and WebSocket endpoints as before, so no configuration is lost when you update from a build that still had the classic UI.

**Upgrading from a build with the classic UI.** Nothing to do. The old `ui_usev2` flag and the classic display preferences in `settings.ini` are ignored and disappear at the next settings save.

**Header and navigation.** The v2 header is a dark strip in both light and dark themes. It shows identity chips (hostname and firmware version, plus IP address and Wi-Fi signal bars), a connectivity status pill, a live connectivity summary strip, a clock, the theme button, and a **SIMULATION** badge that appears only while OT-replay simulation is active. Main navigation is a row of underlined text tabs: **Home**, **SAT**, **Monitor**, **Settings**, and **Advanced**. Sub-navigation within a page uses the same underlined text-tab style.

#### Home page

The Home page offers several layouts through a **View** picker dropdown at the top:

- **System view**: a live schematic of the heating system (boiler or heat pump, flow and return pipes, the space heating emitters, room, DHW, pressure and modulation readouts). With a heat pump, or a boiler that reports a DHW storage tank, the status line reads "Hot water demand · charging the tank" while hot water is heated; a gas boiler without a tank reads "Hot water running · tap open".
- **At a glance**: a phone-first layout built around a single large room-temperature dial with step buttons.
- **Mission control**: a live strip chart (flow / return / setpoint / modulation) plus a raw OpenTherm frame ticker.

The room temperature readouts in all three views (the System schematic, the At a glance dial and chip strip, and the Mission control cells) follow SAT while SAT owns the temperature: they show the room temperature SAT controls on, and the thermostat's own value (MsgID 24) moves into a tooltip on the System and At a glance readouts. While SAT is off, or has no valid room temperature, they show the thermostat's MsgID 24 as before.

When the gateway is injecting values onto the OT bus (active gateway overrides), an "injected" badge appears on the Home schematic and on the Connection map; clicking it opens a floating detail panel listing the active overrides. The badge stays when one refresh of the overrides list fails (for example when the gateway closes the connection on a starved heap); the next refresh decides.

#### Monitor page

The Monitor page has five sub-tabs:

- **Log**: a live console fed by the WebSocket stream, one OpenTherm frame per line (dense rendering). A toolbar carries Pause, **Clear**, **Download**, a frame filter, and toggle chips for Auto-scroll, Timestamps, **SAT only** (show only SAT narration lines), **Auto-download** (save the log every 15 minutes), and **Stream to file** (append frames straight to a local file in Chrome/Edge). Below the toolbar a command bar sends a raw OTGW command (for example `PS=1`, `TT=20.5`, `GW=1`); the command echoes into the log and the prompt label reflects the live command interface (a `PIC ›` prompt on PIC hardware, `OT ›` on OT-Direct).
- **Statistics**: a per-message-ID table (msgID, description, direction, interval, count, value) with a search box, followed by an **Active gateway overrides** panel and a **Boiler unsupported messages** panel.
- **OT Support**: a matrix of all 128 OpenTherm message IDs colour-coded by where each ID was seen (thermostat + boiler, thermostat only, boiler only, or never seen). Click a cell to pin a detail panel with the decoded message.
- **Graph**: a flow / return / setpoint / modulation chart that opens with the history the gateway has kept (the last hour at 30-second resolution on a board without PSRAM, or the last 7 days at 2-minute resolution on a board with PSRAM; it is held in the gateway's RAM, so it starts empty after a reboot) and then continues live at 5 seconds; Dallas sensor lines start at page load. A line is drawn with a break where more than 2.5 minutes of data are missing. While the room temperature is not valid (SAT on: SAT reports it lost; otherwise the thermostat has not reported it for 4.5 minutes) the room line stops and a dotted line in another colour (pink) runs along the bottom of the plot, and the CSV export has an empty room value for those samples; the history the gateway itself kept does not show this. It has window chips (10 min, 1 hour, 4 h, 24 h, and on a board with PSRAM also 7 days; the 7-day window is fetched from the gateway thinned to one point per 10 minutes, while the live part keeps 5 seconds for up to 24 hours), a time axis on round clock times, one-off **PNG** (with the legend of the visible series, axis labels and time axis, in the current theme) and **CSV** export buttons, and **Auto-PNG** / **Auto-CSV** chips for periodic auto-save. The control setpoint, the thermostat setpoint and the SAT target are drawn as dashed lines; from beta.396 their legend entries show the same dash, on screen and in the PNG. The SAT target is its own series (and its own CSV column) only while SAT is enabled; the gateway history carries it only for a window in which it held a SAT value. Click a legend entry to hide or show that series; the choice is remembered in your browser.
- **Connection**: an OT-bus and connectivity map. The OT bus is modelled as two separate links (thermostat and boiler), and the map separates **MODE** (a setting, such as gateway or monitor) from **HEALTH** using a five-state vocabulary: Connected, Degraded, Disconnected, Off, and Unknown (ADR-155). Nodes cover thermostat, boiler, OTGW, router (with a Wi-Fi signal-strength icon and dBm), MQTT broker, and this browser. On OT-Direct hardware an **OT-Direct overrides** panel lets you apply or clear stored responses and response modifiers (SR/CR/RM/CM/UI/KI) directly.

#### SAT page

A dedicated Smart Autotune Thermostat page provides the thermostat control surface in three cumulative depth layers: **Thermostat** (simple), **Control** (operational), and **Technical** (the control loop). It is bound to the SAT REST endpoints. Status messages on the page start with "SAT paused" when SAT has handed the boiler back: "SAT paused: no OpenTherm connection, the boiler is back under normal control" and "SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler". On a prerelease (beta) firmware a notice reads "SAT controls your boiler. This is a beta firmware." and asks for capture files; SAT itself is no longer called experimental (the on/off heat-pump appliance type with its 3 hour cycle still is). In the weather card, while Home Assistant publishes a fresh `sat/sun_elevation`, the card names it as the source of the sun position and asks for no location; with the weather fetch on and no latitude and longitude set, it still asks for them for the fetch itself. For full details of SAT, presets, the heating curve, and diagnostics, see Chapter 5.

The **Hot water** card sits in the SAT **Control** view. **Standalone DHW control** switches SAT's own hot-water handling on or off: SAT sends the DHW setpoint to the boiler only while it is on, and the setpoint slider is greyed out while it is off. The slider covers 30-60 °C, narrowed to the boiler's own DHW range when the boiler reports one (MsgID 48); a setpoint below 30 °C shows as **off**. On a boiler with a hot-water tank the card also shows **DHW enable (HW=)**, which sends the gateway's hot-water enable command.

#### Settings page

The Settings page is driven by the REST API and presents settings with human-readable labels, categories, and hints, plus a search box and a rail of category links. The rail lists the categories with their setting counts (on a narrow screen it scrolls sideways above the cards); the cards of the selected category show on the right, one category at a time, and a search shows every category with a match. A save bar tracks unsaved changes with **Discard** and **Save settings** actions. The SAT category is described as "Smart Autotune Thermostat". Additional panels include a **BLE sensor** roster (discovered Bluetooth Low Energy sensors; on a board without PSRAM the **Enable BLE sensors** switch is disabled and the card says "BLE needs PSRAM", because the firmware refuses to switch BLE on there) and, in the Webhook category, a **Send test call** action that fires the real saved ON webhook. Advanced OT-Direct and SAT settings are exposed here as well.

#### Advanced page

The Advanced page collects the power-user screens in four sub-tabs:

- **PIC firmware**: shows PIC device, type, and firmware version, a "Check for updates" action, a list of available firmware files with a flash progress bar, and a cached gateway-settings (`PR=`) table. Hidden on hardware without a PIC.
- **Debug Information**: device info groups, the crash log (or a "no crash logs" note), friendly Wi-Fi labels, and a raw debug dump.
- **File System**: an in-page FSexplorer (browse, upload, usage bar) with a link to the classic `/FSexplorer`.
- **System**: live device status (link, OT-rewrite mode, simulation), and **System Actions** buttons: Update Firmware (OTA sketch / filesystem), ReBoot, **Run setup wizard**, Reset Wireless, and Home.

#### First-time setup wizard

On a genuinely fresh device the v2 UI shows a first-time-setup onboarding wizard once (existing installs are migrated so it never appears again). It can be re-run at any time from **Advanced > System > Run setup wizard**.

#### Mobile use and asset caching

The v2 UI is designed to be usable on a smartphone. Its assets are served with a `no-cache` policy (ADR-163), so after a filesystem OTA update the browser picks up the new assets immediately without a manual cache clear.

### The classic UI

The classic web UI of the 1.x line is no longer shipped in 2.x (ADR-192): 2.0.0 has only the v2 web UI described above. The sections below describe the settings, the update page and the file manager, which the v2 UI also has.

### Settings Page

The Settings tab contains all device configuration, organized into sections. After changing any settings, click **Save** at the bottom of the section to apply and persist the changes.

Next to each setting label is an **i** button. Hover it, focus it with the keyboard or tap it to read a one- or two-sentence explanation of the setting. A click or tap keeps the text open and **Esc** closes it. The texts come with the firmware (served from `/settings-help.json`), not from the filesystem image.

#### Wi-Fi

| Setting | Description |
|---|---|
| Hostname | mDNS and DHCP hostname for the device (default: `OTGW2`; 1.x uses `OTGW`) |
| Wi-Fi Network (SSID) | Read-only display of the currently connected Wi-Fi network |
| Reset WiFi | Button next to the SSID field. Clears the stored Wi-Fi credentials and reboots the device in Access Point (AP) mode |

To move the gateway to a different Wi-Fi network, click **Reset WiFi** and confirm the prompt. The device reboots, starts the setup network `<hostname>-<MAC address>` (for example `OTGW2-A4:CF:12:21:B4:F8`, see Chapter 2), and reopens the captive portal so you can select and authenticate against the new network. The Reset WiFi action is also available under **FSexplorer > System Actions** as the *Reset Wireless* button.

#### Wi-Fi Scan

The v2 web UI has no Wi-Fi scan panel; to change the network, use Reset WiFi and the setup access point (see Chapter 2). The REST endpoint `GET /api/v2/network/scan` still lists nearby networks: the first call starts the scan and returns `{"status":"scanning"}`, a later call returns `{"status":"ready","networks":[...]}` with `ssid`, `rssi`, `channel`, `secured` and `connected` per network. It refuses a scan during an OTA or PIC flash (restAPI.ino:2419-2423).

#### MQTT

| Setting | Description |
|---|---|
| MQTT Broker | IP address or hostname of your MQTT broker |
| MQTT Port | Broker port (default: `1883`) |
| MQTT User | Broker username (leave blank if not required) |
| MQTT Password | Broker password |
| MQTT Top Topic | Prefix for all published topics (default: `OTGW`) |
| MQTT Unique ID | Device identifier used in topic paths and HA discovery (default: `otgw`) |
| HA discovery prefix | Discovery topic prefix (default: `homeassistant`); must match Home Assistant. Discovery itself is always on |
| Publish Interval | Heartbeat interval in seconds (0 = publish every message as received) |
| Separate Sources | Also publish per-source sibling topics (`<label>_thermostat`, `<label>_boiler`) next to the canonical topic |

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

The nightly restart causes a brief service interruption of approximately 30 seconds. It is only active when NTP is enabled and synced, so the device knows the correct local time.

#### Check GitHub for new versions

From firmware 2.0.0-beta.387. The web UI can tell you when a newer firmware is on GitHub. A notice then appears in the header next to the firmware version, with a link to the release page. From beta.393 the notice also shows on a phone (a window of 480 px or less).

- Who contacts GitHub: your browser, not the gateway. The gateway stays on your local network and sends nothing to the internet for this.
- What is sent: your browser asks api.github.com for the list of OTGW-firmware releases, at most every 6 hours. As with any website, GitHub sees your internet IP address. The gateway's address is not sent: the request carries no referrer and no cookies.
- First use: the check is off until you say yes. The web UI asks once, about 30 seconds after the page loads, and not while the setup wizard or the simulation prompt is open. From beta.393 the question is a chip "Check for new versions?" in the header, next to the identity chips; a click on it shows the explanation with No and "Yes, check", and closing it without an answer means not now (the chip stays). Before beta.393 the question opened as a dialog over the page. Your answer is stored on the gateway and applies to every browser.
- Switch it off: Settings > Behavior > "Check GitHub for new versions". When it is off, the browser makes no request to GitHub at all.
- Betas: a beta firmware is told about newer betas and stable releases. A stable firmware is only told about stable releases, unless you also switch on "Also tell about beta versions".
- Hide a notice: the × next to it hides that version until a newer one appears.

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

#### OpenTherm Gateway (PIC)

These settings apply to the OTGW Classic with its PIC. On hardware without a PIC they have no effect.

| Setting | Description | Default |
|---|---|---|
| Run commands at boot | Send the boot commands below to the PIC at startup | Off |
| Boot commands | PIC commands separated by semicolons, for example `PS=0;GW=1` (up to 128 characters) | `GW=1` in the shipped filesystem image |
| PIC temperature sensor | Read the temperature sensor wired to the PIC every 3 minutes and publish it to MQTT (`otgw-pic/temperature_reading`) and Home Assistant (sensor `PIC_Temperature_Reading`). A PIC without a sensor publishes nothing, so no entity appears. Leave off when no sensor is connected | Off |

The PIC temperature sensor reading is a temperature. The row **Temp sensor** under **Advanced > PIC firmware** is something else: it shows what the sensor is used for (`O` outside, `R` return water).

#### OT-Direct Mode (Hardware Without a PIC)

On hardware without a PIC, a dropdown appears in the settings to select the OT-Direct operating mode:

| Mode | Description |
|---|---|
| Bypass | Thermostat connected straight through to the boiler; no OT processing |
| Gateway | Full override processing (default) |
| Monitor | Transparent pass-through; frames are logged but not modified |
| Master / Standalone | No thermostat required; the gateway acts as sole OT master |
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

> **HTTP/WS only**: The firmware does not do HTTPS or WSS itself. Behind an HTTPS reverse proxy (or Home Assistant ingress) the whole interface works, the live log included: the page opens the WebSocket on its own path, as `wss://` when it was loaded over HTTPS, and the proxy forwards it to the device as plain `ws://`. See section 6.10. For remote access without such a proxy, use a VPN.

**Heap backpressure**: The firmware monitors free heap. It streams frames immediately and pauses WebSocket streaming only while free heap is critical (below 3 KB, ADR-167), then resumes on its own.

This prevents the device from crashing under heavy load. You may notice the log pausing briefly during peak activity; this is normal and expected behavior.

### OTA Firmware Update via the Web Interface

The **Update** section (accessible from the Advanced tab or the update icon in the header) provides two ways to update the firmware. The 2.x firmware does not download updates from GitHub itself: download the release zip and give it to the Update page.

#### One-Action Update on ESP32-S3 (4 MB flash)

The flash utility at `http://<ip>/update` installs firmware and web files in one action:

1. Open the page by IP address (not `otgw2.local`: the updater has no mDNS name).
2. Choose the release zip, or the `.ino.bin` and `.littlefs.bin` files. The page checks the files by content and refuses USB images, the updater itself and mismatched pairs.
3. Press **Update** and keep the tab open until it says Done (2 to 3 minutes). The unit restarts twice. The page switches to the safeboot updater, installs the firmware, waits for the new firmware, installs the web files and verifies both.

Settings and sensor names are kept, and so are the SAT learned state (energy totals, heating-curve data and markers, controller state), the learned OpenTherm profiles, the reboot log and the hot-water meter total. A file that has grown past its size limit (2 to 3 KB) is left out. Files uploaded in the File Explorer, the boot detection log and the MQTT topic cleanup state start fresh. **Download a settings backup** is a separate link on the page and not part of the update. The forms under **Advanced** keep the old one-upload-at-a-time flow. Recovery steps and the full list: [WiFi firmware updates](../../guides/WIFI_FIRMWARE_UPDATE.md).

#### Manual Binary Upload

1. Download or build a firmware binary (`.bin` file).
2. Use the file upload area on the Update page to select the file.
3. The device flashes the binary and reboots.

> **PIC firmware upgrade**: The Update page also supports upgrading the PIC microcontroller firmware. Select the correct PIC type (PIC16F88 or PIC16F1847), upload the `.hex` file, and follow the on-screen instructions. Never use OTmonitor to flash PIC firmware over Wi-Fi.

### File Manager (FSexplorer)

The **FSexplorer** is accessible from the Advanced tab dropdown under **File system contents**, or directly at `http://otgw2.local/FSexplorer.html`. It provides a browser-based view of the LittleFS filesystem on the device.

Features:

- Browse files and subdirectories. Directories are listed first and alphabetically sorted (case-insensitive). A `.. (Parent)` link appears in subdirectories for navigation.
- Upload new files via the **Upload File** form. A progress bar shows upload status, and the file size is checked against available free space before upload is allowed.
- Download any file by clicking **Download** on its row.
- Delete files with the **Delete** link.
- View storage usage at the bottom of the listing (used and total bytes of the LittleFS partition).

A **System Actions** panel below the file list provides quick buttons for **Update Firmware** (desktop browsers only), **ReBoot**, **Reset Wireless** (same action as Reset WiFi on the Settings page), and **Exit FSexplorer** (returns to the main page).

This is useful for manually backing up or restoring `settings.ini`, `dallas_labels.ini`, the SAT PID state file, or custom web assets. The firmware always streams files rather than buffering them into RAM, so there is no hard upload size limit beyond the available LittleFS partition space. The main `index.html` (now several tens of kilobytes on 2.0.0) and all other assets are served via `streamFile()` to avoid loading them into RAM (TASK-668).

When the FSexplorer routes detect that `index.html` is missing (filesystem not mounted or image not yet flashed), the routes serve `FSexplorer.html` directly so the user can still upload a filesystem image from the file manager.

If the file manager page itself is missing as well (no `FSexplorer.html.gz` on the filesystem), existing files are still served as normal, for example the reboot log; only a request for a file that does not exist gets the upload page (beta.244, TASK-1926).

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
