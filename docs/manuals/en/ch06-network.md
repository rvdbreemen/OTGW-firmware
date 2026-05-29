## Chapter 6: Network Configuration

This chapter covers everything the firmware does on the network: from first-time WiFi setup through Ethernet failover, hostname resolution, time synchronization, and OTA firmware updates.

### 6.1 WiFi Setup

On first boot the firmware has no saved WiFi credentials. It opens a captive portal so you can enter them.

#### 6.1.1 Initial Configuration via Captive Portal

When no credentials are stored, the device starts a WiFi access point named after its hostname and MAC address, for example `otgw-AABBCCDD`. Connect to that SSID from a phone or laptop and a configuration page opens automatically (or navigate to `192.168.4.1` manually). Enter your SSID and password and click Save. The device reboots, connects to your network, and the AP disappears.

#### 6.1.2 Reconnection Behavior

The firmware uses a two-tier reconnection strategy:

1. The ESP SDK's built-in auto-reconnect (`WiFi.setAutoReconnect(true)`) handles short blips (typically under 30 seconds) transparently at the radio level.
2. For longer outages, the application-level `loopWifi()` state machine takes over. The state machine is fully non-blocking: each call returns in well under a millisecond, so the watchdog, OpenTherm message processing, MQTT keepalives, and the web UI all keep running during reconnection. It retries with a 30-second window per attempt (enough for a full scan, association, and DHCP exchange) and gives up after 10 attempts, rebooting the device (max time before reboot: 300 seconds). In beta builds it enters AP fallback mode after 2 failed retries instead (see section 6.1.3). These timing parameters are codified in ADR-075 (which supersedes the original 5-second/15-retry values from ADR-047) to give `WiFi.begin()` a full association window and avoid cancelling the SDK auto-reconnect mid-flight.

After a successful reconnect, the firmware re-applies the configured hostname, forces a DHCP re-announce so the router learns the correct hostname, and restarts Telnet, MQTT, and WebSocket services automatically. Per the fix for issue #525, the SDK DHCP client is only restarted while the station is disconnected. Touching `wifi_station_dhcpc_start()` on an associated station resets the IP to `0.0.0.0` and prevents recovery after a router reboot.

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
| Triple-reset detection | Press the hardware reset button 3 times in a row, each press within 10 seconds of the previous (forces the config portal; not a power-cycle, do not hold) |
| Telnet debug console | Send the `resetwifi` command on port 23 |

---

### 6.2 Ethernet (ESP32 + W5500)

Wired Ethernet is available on ESP32 boards with a W5500 SPI Ethernet controller. Not available on ESP8266.

#### 6.2.1 Hardware Detection

The firmware probes the W5500 chip at boot by reading its SPI VERSION register (address 0x0039). If the register returns 0x04, the chip is recognized as a W5500 and Ethernet support is activated. No separate setting is needed: if the chip is present and correctly wired, it is used automatically.

#### 6.2.2 GPIO Assignments

The W5500 uses the SPI bus. The exact GPIO assignments depend on the board variant configured in `boards.h`. Consult the hardware documentation for your specific OTGW32 board for the precise pin numbers.

Typical connections:

| W5500 Signal | ESP32 GPIO |
|---|---|
| CS (chip select) | See boards.h `PIN_SPI_CS` |
| INT (interrupt) | See boards.h `PIN_SPI_INT` |
| RST (reset) | See boards.h `PIN_SPI_RST` |
| SCK (SPI clock) | See boards.h `PIN_SPI_SCK` |
| MISO | See boards.h `PIN_SPI_MISO` |
| MOSI | See boards.h `PIN_SPI_MOSI` |

#### 6.2.3 Priority over WiFi and runtime failover

Ethernet always takes priority. When the W5500 is present and a cable is plugged in, WiFi is disabled. If the cable is unplugged at runtime, the firmware detects this (the link is polled every 5 seconds in `loopEthernet()`) and switches back to WiFi automatically. When the cable is reconnected, DHCP runs (2-second timeout on hot-plug) and the firmware switches back to Ethernet. This failover happens without a reboot and without configuration.

On every transition the firmware re-registers mDNS on the new interface, restarts the MQTT client and the WebSocket server, and publishes a retained `otgw-firmware/network/mode` topic with value `wifi` or `ethernet`. The transition to `ethernet` is published as soon as the MQTT broker confirms the new connection (a deferred drain inside `loopEthernet()`). The transition to `wifi` is published before the cable-loss switch so the broker sees it while MQTT is still alive on the wired interface.

The current transport is exposed to the web UI and Home Assistant through `/api/v2/device/info`, which adds three fields on OTGW32 hardware: `networkmode` (`wifi` or `ethernet`), `ethernetpresent` (W5500 detected), and `ethernetlink` (cable present). The header bar in the web UI polls `/api/v2/device/time` once per second to keep the network icon (WiFi or Ethernet) in sync with the active transport.

#### 6.2.4 MAC Address

The firmware derives a unique locally-administered MAC address from the ESP32 eFuse MAC. This MAC address differs from the WiFi MAC address but is unique per device and consistent across reboots.

#### 6.2.5 Static IP on Ethernet

Ethernet has its own static IP configuration, independent of the WiFi static IP. Configure it in the Settings page under the Ethernet section, or via the REST settings keys `ETHstaticip` (boolean), `ETHipaddress`, `ETHgateway`, `ETHsubnet`, and `ETHdns`. When `ETHstaticip` is false (the default), Ethernet uses DHCP with a 1-second timeout at boot and a 2-second timeout on cable hot-plug. When `ETHdns` is `0.0.0.0`, the gateway address is used as the DNS server.

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

mDNS works on both ESP8266 and ESP32. When the network interface changes (for example, switching from WiFi to Ethernet), the firmware re-registers the mDNS hostname on the new interface automatically.

The firmware also registers an LLMNR responder on ESP8266, which allows Windows machines to resolve `otgw` (without the `.local` suffix). LLMNR is not available on ESP32.

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

The ESP8266 suffers from heap fragmentation over time. The firmware offers an optional scheduled nightly restart to reclaim memory. This feature is disabled by default.

| Setting | Default | Description |
|---|---|---|
| Nightly Restart | `off` | Enable to restart once per day at the configured hour |
| Nightly Restart Hour | `4` | Local hour (0-23) when the restart happens |

Requirements for the restart to trigger:
- The setting must be enabled.
- NTP must be enabled and synchronized.
- The device must have been running for more than 1 hour (prevents restart loops after a fresh reboot).

The restart causes a brief service interruption of approximately 30 seconds.

#### 6.5.5 ESP8266 SDK Time Bug

The ESP8266 SDK initializes `time()` to `0xFFFFFFFF` (year 2106) before the first SNTP sync completes. The firmware detects this bogus value and ignores it, so timestamps are never poisoned by a wrong initial time.

---

### 6.6 OTA Firmware Updates

The OTA update endpoint is at `/update`. It sits behind the protected-admin boundary defined in ADR-056 (which supersedes ADR-054). When `settingHTTPpasswd` is empty, the endpoint is open. When a password is set, the endpoint requires HTTP Basic Auth with username `admin`. The same password is propagated to the OTA update server at WiFi or Ethernet startup, so a settings change takes effect on the next reboot without re-flashing.

Browser requests to protected admin endpoints (including `/update`, `/upload`, `/ReBoot`, `/ResetWireless`, and `POST /api/v2/settings`) are additionally checked for `Origin`/`Referer` matching `Host`. This is a lightweight CSRF mitigation, not a full session framework: requests without `Origin` or `Referer` (curl, OTmonitor, scripts) are allowed for backward compatibility; mismatched origins are rejected with HTTP 403.

**Steps:**
1. Open `http://otgw.local/update` in your browser.
2. Select the firmware file.
3. Click Upload.
4. The device flashes the firmware, verifies the image, and reboots.

OTA is available from AP fallback mode as well, so you can recover a device stuck on a bad firmware image without serial access.

#### 6.6.1 ESP32: Merged Binary Support

On ESP32, the OTA update page accepts a merged binary (firmware + filesystem combined in a single `.bin` file). The update server detects the merged format automatically and writes each part to the correct partition. You do not need to upload firmware and filesystem separately.

#### 6.6.2 Limitations

- Never flash PIC firmware via OTA. The OTA update page is for ESP firmware only. Flashing PIC firmware over the network with OTmonitor can permanently damage the PIC.
- On ESP8266, the available OTA space is limited by flash size. The `build.py` tool reports the available space.

---

### 6.7 ESP8266: lwIP Low Memory Variant and Arduino Core 3.1.2

Since v2.0.0-beta, the ESP8266 build has moved from Arduino core 2.7.4 to 3.1.2 and is compiled with the lwIP v2 Low Memory variant (`ip=lm2f`, TCP MSS=536). Compared to the Higher Bandwidth variant this roughly halves the TCP receive/send buffers per socket, which matters on the ESP8266's ~40KB of usable RAM. The lwIP variant is a compile-time choice and requires no user configuration.

On the MQTT publish path the firmware also enables WiFiClient sync mode (`setSync(true)`). This skips the temporary ~1KB `TCP_SND_BUF` copy inside the Arduino WiFiClient: writes flow straight into lwIP instead of being staged in an intermediate buffer. Combined with `setNoDelay(true)`, the data-plane RAM per MQTT connection drops from about 4.8KB to 3.7KB. This applies to the MQTT publish path only; the web server, WebSocket, and telnet sockets continue to use the default (buffered) WiFiClient behaviour.

---

### 6.8 Port Usage

| Port | Protocol | Service | Notes |
|---|---|---|---|
| 80 | TCP / HTTP | Web interface and REST API | All web UI routes, file serving, OTA update |
| 81 | TCP / WebSocket | Live OpenTherm log stream | Used by the web UI for real-time data |
| 23 | TCP / Telnet | Debug console | Plain-text debug log; served by SimpleTelnet library |
| 25238 | TCP | Serial bridge (ser2net) | Raw OTGW PIC serial over TCP; served by SimpleTelnet library; OTmonitor compatible |
| 123 | UDP (outbound) | NTP (SNTP) | Outbound only |
| 5353 | UDP | mDNS | Local network name resolution (both platforms) |
| 5355 | UDP | LLMNR | Windows name resolution (ESP8266 only) |
| 1883 | TCP (outbound) | MQTT | Outbound to your MQTT broker |

---

### 6.9 Firewall and Router Considerations

The firmware is designed for a local, trusted network. No inbound ports need to be forwarded through your router.

mDNS (`otgw.local`) only works within a single Layer 2 broadcast domain. If Home Assistant is on a different VLAN, use the device's static IP or a DNS entry instead of `.local`.

---

### 6.10 Using Behind a Reverse Proxy

The REST API works correctly behind an HTTPS-terminating reverse proxy (Nginx, Caddy, etc.). The WebSocket live log (port 81) requires a plain `ws://` connection and does not support `wss://`. A reverse proxy that upgrades WebSocket connections to WSS will break the live log in the web interface.

---
