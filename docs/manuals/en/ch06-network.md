## Chapter 6: Network Configuration

This chapter covers everything the firmware does on the network: from first-time WiFi setup through Ethernet failover, hostname resolution, time synchronization, and OTA firmware updates.

### 6.1 WiFi Setup

On first boot the firmware has no saved WiFi credentials. It opens a captive portal so you can enter them.

#### 6.1.1 Initial Configuration via Captive Portal

When no credentials are stored, the device starts a WiFi access point named after its hostname and MAC address, for example `otgw-AABBCCDD`. Connect to that SSID from a phone or laptop and a configuration page opens automatically (or navigate to `192.168.4.1` manually). Enter your SSID and password and click Save. The device reboots, connects to your network, and the AP disappears.

#### 6.1.2 Reconnection Behavior

The firmware uses a two-tier reconnection strategy:

1. The ESP SDK's built-in auto-reconnect handles short blips (typically under 30 seconds).
2. If the connection stays down longer, the application-level `loopWifi()` state machine retries non-blocking with a 30-second window per attempt. In production builds it retries up to 10 times before rebooting. In beta builds it enters AP fallback mode after 2 failed retries (see section 6.1.3).

After reconnecting, the firmware re-applies the configured hostname, forces a DHCP re-announce so the router learns the correct hostname, and restarts Telnet, MQTT, and WebSocket services automatically.

#### 6.1.3 AP Fallback Mode

Available in beta builds (v2.0.0-beta and later, all platforms). When WiFi retries are exhausted at runtime, the device switches into AP fallback mode instead of rebooting indefinitely. In production releases the device reboots after 10 failed retries instead.

At boot, if credentials are stored but WiFi is unreachable, beta builds also skip the WiFiManager config portal and go straight to AP fallback.

- **SSID**: `OTGW-<last 3 bytes of MAC address in uppercase hex>`, for example `OTGW-AABBCC`
- **IP address**: `192.168.4.1`
- **Password**: `otgw123`
- **MQTT**: disabled in fallback mode
- **OTA update**: available from fallback mode so you can flash a corrected firmware without serial access

In AP fallback mode the device keeps attempting to reconnect to the configured WiFi network every 5 minutes. When WiFi comes back, services (MQTT, Telnet, WebSocket) restart automatically and the AP is torn down.

**How to recover from fallback:**
1. Connect to the `OTGW-XXXXXX` SSID using password `otgw123`.
2. Open `http://192.168.4.1` in your browser.
3. Correct your WiFi credentials in Settings or use Reset WiFi.
4. Reboot the device.

#### 6.1.4 Resetting WiFi Credentials

| Method | How |
|---|---|
| Web UI | Settings page, click "Reset WiFi" button |
| Triple-reset detection | Power-cycle the device 3 times within 10 seconds |
| Telnet debug console | Send the `resetwifi` command on port 23 |

---

### 6.2 Ethernet (ESP32 + W5500)

Wired Ethernet is available on ESP32 boards with a W5500 SPI Ethernet controller. Not available on ESP8266.

The W5500 uses these default GPIO assignments:

| W5500 Signal | ESP32 GPIO |
|---|---|
| CS (chip select) | GPIO 5 |
| INT (interrupt) | GPIO 34 |
| RST (reset) | GPIO 15 |
| SCK (SPI clock) | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |

**Priority over WiFi**: Ethernet always takes priority. When the W5500 is present and a cable is plugged in, WiFi is disabled. If the cable is unplugged at runtime, the firmware detects this (checked every 5 seconds) and switches back to WiFi automatically.

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

The default hostname is `otgw`. After connecting to your network the device is reachable as `otgw.local` from any mDNS-capable client (macOS, iOS, Linux with Avahi, Windows 10+ with Bonjour).

The firmware also registers an LLMNR responder on ESP8266, which allows Windows machines to resolve `otgw` (without the `.local` suffix).

Set the hostname in Settings. The new hostname propagates to the DHCP request, mDNS advertisement, MQTT client ID, and web UI title.

---

### 6.5 NTP Time Synchronization

| Setting | Default | Description |
|---|---|---|
| NTP server | `pool.ntp.org` | Any NTP server hostname or IP address |
| Timezone | `Europe/Amsterdam` | IANA timezone database name |
| NTP enabled | `true` | Can be disabled if no internet access |

The firmware re-checks the time every 30 minutes. After each successful NTP sync, it sends clock commands to the boiler PIC (`SC=HH:MM/D`, `SR=21:MM,DD`, `SR=22:YH,YL`).

---

### 6.6 OTA Firmware Updates

The OTA update endpoint is at `/update`. Authentication uses HTTP Basic Auth when a password is configured.

**Steps:**
1. Open `http://otgw.local/update` in your browser.
2. Select the `.bin` firmware file.
3. Click Upload.
4. The device flashes the firmware, verifies the image, and reboots.

OTA is available from AP fallback mode as well, so you can recover a device stuck on a bad firmware image without serial access.

---

### 6.7 Port Usage

| Port | Protocol | Service | Notes |
|---|---|---|---|
| 80 | TCP / HTTP | Web interface and REST API | All web UI routes, file serving, OTA update |
| 81 | TCP / WebSocket | Live OpenTherm log stream | Used by the web UI for real-time data |
| 23 | TCP / Telnet | Debug console | Plain-text debug log; served by SimpleTelnet library |
| 25238 | TCP | Serial bridge (ser2net) | Raw OTGW PIC serial over TCP; served by SimpleTelnet library; OTmonitor compatible |
| 123 | UDP (outbound) | NTP (SNTP) | Outbound only |
| 5353 | UDP | mDNS | Local network name resolution |
| 5355 | UDP | LLMNR | Windows name resolution (ESP8266 only) |
| 1883 | TCP (outbound) | MQTT | Outbound to your MQTT broker |

---

### 6.8 Firewall and Router Considerations

The firmware is designed for a local, trusted network. No inbound ports need to be forwarded through your router.

mDNS (`otgw.local`) only works within a single Layer 2 broadcast domain. If Home Assistant is on a different VLAN, use the device's static IP or a DNS entry instead of `.local`.

---

### 6.9 Using Behind a Reverse Proxy

The REST API works correctly behind an HTTPS-terminating reverse proxy (Nginx, Caddy, etc.). The WebSocket live log (port 81) requires a plain `ws://` connection and does not support `wss://`. A reverse proxy that upgrades WebSocket connections to WSS will break the live log in the web interface.

---

