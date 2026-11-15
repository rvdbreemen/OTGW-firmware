## Chapter 6: Network Configuration

This chapter covers everything the firmware does on the network: from first-time WiFi setup through hostname resolution, time synchronization, and OTA firmware updates.

### 6.1 WiFi Setup

On first boot the firmware has no saved WiFi credentials. It opens a captive portal so you can enter them.

#### 6.1.1 Initial Configuration via Captive Portal

When no credentials are stored, the device starts a WiFi access point named after its hostname and MAC address, for example `OTGW2-A4:CF:12:21:B4:F8`. Connect to that SSID from a phone or laptop and a configuration page opens automatically (or navigate to `192.168.4.1` manually). Enter your SSID and password and click Save. The device reboots, connects to your network, and the AP disappears.

#### 6.1.2 Reconnection Behavior

The firmware uses a two-tier reconnection strategy:

1. The ESP SDK's built-in auto-reconnect (`WiFi.setAutoReconnect(true)`) handles short blips (typically under 30 seconds) transparently at the radio level.
2. For longer outages, the application-level `loopWifi()` state machine takes over. The state machine is fully non-blocking: each call returns in well under a millisecond, so the watchdog, OpenTherm message processing, MQTT keepalives, and the web UI all keep running during reconnection. It retries with a 30-second window per attempt (enough for a full scan, association, and DHCP exchange) and gives up after 10 attempts, rebooting the device (max time before reboot: 300 seconds). In beta builds it enters AP fallback mode after 2 failed retries instead (see section 6.1.3). These timing parameters are codified in ADR-075 (which supersedes the original 5-second/15-retry values from ADR-047) to give `WiFi.begin()` a full association window and avoid cancelling the SDK auto-reconnect mid-flight.

After a successful reconnect, the firmware re-applies the configured hostname for the next DHCP exchange and restarts Telnet, MQTT, and WebSocket services automatically. It makes no DHCP calls of its own: the WiFi SDK handles DHCP as part of the reconnect.

#### 6.1.3 AP Fallback Mode

Available in beta builds (v2.0.0-beta and later). When WiFi retries are exhausted at runtime, the device switches into AP fallback mode instead of rebooting indefinitely. In production releases the device reboots after 10 failed retries instead.

At boot, if credentials are stored but WiFi is unreachable, beta builds also skip the WiFiManager config portal and go straight to AP fallback.

- **SSID**: `OTGW-<last 3 bytes of MAC address in uppercase hex>`, for example `OTGW-AABBCC`
- **IP address**: `192.168.4.1`
- **Password**: `otgw1234`
- **MQTT**: disabled in fallback mode
- **OTA update**: available from fallback mode so you can flash a corrected firmware without serial access

In AP fallback mode the device keeps attempting to reconnect to the configured WiFi network every 5 minutes. When WiFi comes back, services (MQTT, Telnet, WebSocket) restart automatically and the AP is torn down.

**How to recover from fallback:**
1. Connect to the `OTGW-XXXXXX` SSID using password `otgw1234`.
2. Open `http://192.168.4.1` in your browser.
3. Correct your WiFi credentials in Settings or use Reset WiFi.
4. Reboot the device.

#### 6.1.4 Resetting WiFi Credentials

| Method | How |
|---|---|
| Web UI | Settings page, click "Reset WiFi" button |
| Triple-reset detection | Reset the gateway 3 times in a row, each reset within 10 seconds of the board coming back up: press the reset button or pull the plug (both count, and they may be mixed). Software, USB and watchdog resets do not count. See `docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md` |
| Telnet debug console | Send the `resetwifi` command on port 23 |

---

### 6.2 Ethernet

The OTGW Classic has no Ethernet, so it always uses WiFi; the Ethernet settings only appear on hardware with Ethernet.

---

### 6.3 Static IP vs DHCP

By default the device uses DHCP. If your router supports DHCP reservations, assigning a fixed IP by MAC address is the recommended approach.

For a true static IP configured in firmware:

1. Open Settings in the web interface.
2. Set the Static IP, Subnet Mask, Gateway, and DNS fields.
3. Save and reboot.

Leave all fields empty to revert to DHCP.

---

### 6.4 mDNS and Hostname

The default hostname on 2.0.0 is `OTGW2`. After connecting to your network the device is reachable as `otgw2.local` from any mDNS-capable client (macOS, iOS, Linux with Avahi, Windows 10+ with Bonjour).

The 1.x firmware keeps the default `OTGW` (`otgw.local`), so a 1.x and a 2.0.0 gateway on the same network no longer claim the same name. A unit upgraded from 1.x keeps the hostname saved in its settings, usually `OTGW`; only a new install or a reset that discards `settings.ini` starts with `OTGW2`.

The 2.x firmware has no LLMNR responder, so the device itself does not answer for the bare name `otgw2` (without `.local`): use `otgw2.local` or the IP address.

Set the hostname in Settings. The new hostname propagates to the DHCP request, mDNS advertisement, MQTT client ID, and web UI title.

---

### 6.5 NTP Time Synchronization

#### 6.5.1 Settings

| Setting | Default | Description |
|---|---|---|
| NTP server | `pool.ntp.org` | Any NTP server hostname or IP address |
| Timezone | `Europe/Amsterdam` | IANA timezone database name (see below) |
| NTP enabled | `true` | Can be disabled if no internet access |

#### 6.5.2 Timezone Handling (AceTime)

The firmware uses the AceTime library (4.x) for timezone handling. You configure the timezone using standard IANA timezone names (for example `Europe/Amsterdam`, `America/New_York`, `Asia/Tokyo`). These names are resolved using AceTime's built-in timezone database, which covers DST rules and historical changes automatically.

Unlike a raw POSIX timezone string, you do not need to specify DST transition rules manually. Just enter the IANA name and AceTime handles the rest.

If the configured timezone name is invalid or not found in the database, the firmware falls back to the default (`Europe/Amsterdam`) and logs a warning.

#### 6.5.3 What NTP Is Used For

The firmware re-checks the time every 30 minutes. After each successful NTP sync, it sends clock commands to the boiler PIC (`SC=HH:MM/D`, `SR=21:MM,DD`, `SR=22:YH,YL`).

NTP time is also required for the nightly restart feature (see section 6.5.4) and for timestamps in debug logs and MQTT messages.

#### 6.5.4 Nightly Restart for Heap Recovery

The firmware offers an optional scheduled nightly restart. This feature is disabled by default.

| Setting | Default | Description |
|---|---|---|
| Nightly Restart | `off` | Enable to restart once per day at the configured hour |
| Nightly Restart Hour | `4` | Local hour (0-23) when the restart happens |

Requirements for the restart to trigger:
- The setting must be enabled.
- NTP must be enabled and synchronized.
- The device must have been running for more than 1 hour (prevents restart loops after a fresh reboot).

The restart causes a brief service interruption of approximately 30 seconds.

---

### 6.6 OTA Firmware Updates

The OTA update endpoint is at `/update`. It sits behind the protected-admin boundary defined in ADR-056 (which supersedes ADR-054). When `settingHTTPpasswd` is empty, the endpoint is open. When a password is set, the endpoint requires HTTP authentication with username `admin` (the main firmware challenges with Digest, the safeboot updater with Basic). The same password is propagated to the OTA update server at network startup, so a settings change takes effect on the next reboot without re-flashing.

Browser requests to protected admin endpoints (including `/update`, `/upload`, `/ReBoot`, `/ResetWireless`, and `POST /api/v2/settings`) are additionally checked for `Origin`/`Referer` matching `Host`. This is a lightweight CSRF mitigation, not a full session framework: requests without `Origin` or `Referer` (curl, OTmonitor, scripts) are allowed for backward compatibility; mismatched origins are rejected with HTTP 403.

**Steps (ESP32-S3, 4 MB flash):**
1. Open `http://<ip>/update` in your browser (the IP address, not `otgw2.local`).
2. Choose the release zip, or the `.ino.bin` and `.littlefs.bin` files.
3. Press **Update** and keep the tab open until it says Done. The page runs the safeboot handover, the firmware upload, a 35 s settle and the filesystem upload, and verifies the result.

The forms under **Advanced** upload one file at a time. See [WiFi firmware updates](../../guides/WIFI_FIRMWARE_UPDATE.md) for what is kept, what starts fresh and what to do when a step fails.

OTA is available from AP fallback mode as well, so you can recover a device stuck on a bad firmware image without serial access.

#### 6.6.1 ESP32: Merged Binaries Are USB-Only

The OTA update page does not accept merged binaries (`*-merged*.bin`, `*-migrate-*.bin`): they are USB images for `flash_otgw`, and the page refuses them. Use the release zip or the `.ino.bin` and `.littlefs.bin` files.

#### 6.6.2 Limitations

- Never flash PIC firmware via OTA. The OTA update page is for ESP firmware only. Flashing PIC firmware over the network with OTmonitor can permanently damage the PIC.

---

### 6.7 Connecting OTmonitor

OTmonitor (Schelte Bron's monitoring tool for the OpenTherm Gateway) connects to a 2.x gateway over the network, on TCP port 25238. The gateway speaks the same line protocol there as a classic OTGW on its serial cable, also on hardware without a PIC.

1. In the web interface, open **Settings > OpenTherm Gateway** and switch on **Legacy TCP port 25238**. It is off by default. Save.
2. In OTmonitor, open the connection settings and choose **TCP connection**.
3. Enter the gateway's IP address as **Remote host** (the address you use in the browser) and **25238** as **Remote port**.
4. Connect. OTmonitor shows the OpenTherm messages as it does with a classic OTGW.

Things to know:

- **USB is not an OTmonitor connection.** On hardware without a PIC the USB port is for flashing and diagnostics only. Opening that USB port from OTmonitor resets the gateway (the same USB line signals that start a USB flash), so the gateway would reboot in the middle of heating. Use TCP port 25238.
- **Never flash the PIC over WiFi with OTmonitor.** A failed or interrupted upload over the network can leave the PIC unusable. To update the PIC firmware on a Classic, use **Advanced > PIC firmware** in the web interface.
- **SAT changes what OTmonitor may do.** While SAT is enabled, the gateway refuses commands that would change the boiler control (for example `CS=`), so they cannot fight SAT. On a Classic, the port then passes only plain text command lines to the PIC and drops binary data, so a PIC upgrade through OTmonitor is not possible while SAT is on.
- Port 25238 has no password. Keep it off when you do not use it, and do not forward it from your router.

---

### 6.8 Port Usage

| Port | Protocol | Service | Notes |
|---|---|---|---|
| 80 | TCP / HTTP | Web interface and REST API | All web UI routes, file serving, OTA update |
| 80 | TCP / WebSocket (`/ws`) | Live OpenTherm log stream | Same port as the web UI, path `/ws` (port 81 was the 1.x line) |
| 23 | TCP / Telnet | Debug console | Plain-text debug log; served by SimpleTelnet library |
| 25238 | TCP | Serial bridge (ser2net) | Raw OTGW PIC serial over TCP; served by SimpleTelnet library; OTmonitor compatible |
| 123 | UDP (outbound) | NTP (SNTP) | Outbound only |
| 5353 | UDP | mDNS | Local network name resolution |
| 1883 | TCP (outbound) | MQTT | Outbound to your MQTT broker |

---

### 6.9 Firewall and Router Considerations

The firmware is designed for a local, trusted network. No inbound ports need to be forwarded through your router.

mDNS (`otgw2.local`) only works within a single Layer 2 broadcast domain. If Home Assistant is on a different VLAN, use the device's static IP or a DNS entry instead of `.local`.

---

### 6.10 Using Behind a Reverse Proxy

The web interface, the REST API and the live log work behind a reverse proxy, also on a path (for example `https://example.lan/otgw/` or Home Assistant ingress) and over HTTPS: the interface builds every URL relative to the page it was loaded from, and the live log is a WebSocket at `ws` under that path (`wss://` on an HTTPS page). The proxy must pass the browser's Host header (otherwise every change is refused with 403 once a gateway password is set) and carry WebSocket. See [docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md](../../guides/REVERSE_PROXY_AND_HA_INGRESS.md) for a tested nginx example and section 4.12 for Home Assistant.

---
