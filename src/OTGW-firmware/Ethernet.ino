/*
***************************************************************************
**  Program  : Ethernet.ino
**  Version  : v2.0.0-alpha.228
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  W5500 SPI Ethernet support for OTGW32 (ESP32-S3).
**  Provides hardware detection, DHCP, and automatic WiFi↔Ethernet failover.
**
**  Guard: HAS_ETH_CAPABLE — only compiled on boards with W5500 hardware.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE

#include <SPI.h>
#include <EthernetESP32.h>

// SPI bus instance (FSPI on ESP32-S3)
static SPIClass ethSPI(FSPI);

// W5500 driver + MAC address
static W5500Driver w5500Driver(PIN_SPI_CS, PIN_SPI_INT, PIN_SPI_RST);
static uint8_t ethMac[6] = {0};

// Network management state
static bool ethInitialized = false;
// Set when a mode transition to Ethernet occurred but the MQTT publish is still
// pending (MQTT reconnect is async; the publish fires on the next loopEthernet
// tick once the broker is confirmed connected).
static bool ethModePubPending = false;

//=======================================================================
// Derive a locally-administered MAC from the ESP32 eFuse MAC.
// Sets bit 1 of the first octet to mark it as local, ensuring it
// differs from the WiFi MAC while remaining unique per device.
//=======================================================================
static void deriveEthMAC() {
  uint64_t mac64 = ESP.getEfuseMac();
  for (int i = 0; i < 6; i++) {
    ethMac[5 - i] = (mac64 >> (8 * i)) & 0xFF;
  }
  ethMac[0] |= 0x02;  // locally-administered bit
}

//=======================================================================
// Probe the W5500 VERSION register (0x0039) via raw SPI.
// Returns true if the chip responds with 0x04 (W5500 silicon).
// This works without a cable — pure hardware detection in ~1ms.
//=======================================================================
static bool probeW5500() {
  // Hardware reset
  pinMode(PIN_SPI_RST, OUTPUT);
  digitalWrite(PIN_SPI_RST, LOW);
  delayMicroseconds(500);
  digitalWrite(PIN_SPI_RST, HIGH);
  delay(2);  // W5500 needs ~1ms after reset for PLL lock

  // Init SPI bus
  ethSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  // Read VERSION register (0x0039) from Common Register block
  // W5500 SPI frame: [addr_hi][addr_lo][control_byte][data...]
  // Control byte for Common Register, Read, Variable-length: 0x00
  pinMode(PIN_SPI_CS, OUTPUT);
  ethSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SPI_CS, LOW);
  ethSPI.transfer(0x00);   // address high byte
  ethSPI.transfer(0x39);   // address low byte
  ethSPI.transfer(0x00);   // control: Common block, Read, VDM
  uint8_t ver = ethSPI.transfer(0x00);  // read VERSION
  digitalWrite(PIN_SPI_CS, HIGH);
  ethSPI.endTransaction();

  DebugTf(PSTR("W5500 VERSION register: 0x%02X (%s)\r\n"),
          ver, (ver == 0x04) ? "W5500 found" : "not found");

  return (ver == 0x04);
}

//=======================================================================
// Initialize the Ethernet library and attempt DHCP.
// Called once at boot if probeW5500() succeeds, and again on hot-plug.
// Returns true if DHCP succeeds and a link is present.
//=======================================================================
static bool startEthernet(uint16_t dhcpTimeoutMs) {
  w5500Driver.setSPI(ethSPI);
  w5500Driver.setSpiFreq(10);  // 10 MHz
  w5500Driver.setPhyAddress(0);
  Ethernet.init(w5500Driver);

  if (settings.eth.bStaticIP) {
    // Static IP configuration
    IPAddress ip, gw, sn, dns;
    ip.fromString(settings.eth.sIPaddress);
    gw.fromString(settings.eth.sGateway);
    sn.fromString(settings.eth.sSubnet);
    dns.fromString(settings.eth.sDNS);
    // If DNS is 0.0.0.0, use gateway as DNS
    if (dns == IPAddress(0, 0, 0, 0)) dns = gw;
    Ethernet.begin(ethMac, ip, dns, gw, sn);
    DebugTf(PSTR("Ethernet: static IP=%s, GW=%s\r\n"),
            ip.toString().c_str(), gw.toString().c_str());
  } else {
    // DHCP
    if (!Ethernet.begin(ethMac, dhcpTimeoutMs)) {
      DebugTln(F("Ethernet DHCP failed"));
      return false;
    }
  }

  if (Ethernet.linkStatus() != LinkON) {
    DebugTln(F("Ethernet: no link"));
    return false;
  }

  DebugTf(PSTR("Ethernet: IP=%s, link=UP\r\n"),
          Ethernet.localIP().toString().c_str());
  return true;
}

//=======================================================================
// Switch from WiFi to Ethernet: disconnect WiFi, update state, restart services.
//=======================================================================
static void switchToEthernet() {
  DebugTln(F("Network: switching to Ethernet — disabling WiFi"));
  // Set mode FIRST so loopWifi() (gated on eMode) stops driving reconnects, then
  // power the radio down. WIFI_OFF also neutralizes the SDK setAutoReconnect(true)
  // set in startWiFi(); a bare WiFi.disconnect() would let auto-reconnect resurrect
  // the station and fight the wired link.
  state.net.eMode = NET_ETHERNET;
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  state.net.bEthernetLink = true;
  ethInitialized = true;
  // Re-register mDNS on the new interface
  startMDNS(CSTR(settings.sHostname));
  // Restart MQTT and WebSocket on the new interface.
  // MQTT connect is async — actual broker connection happens in subsequent
  // handleMQTT() calls. Set a flag so loopEthernet() publishes the mode
  // topic on the next tick once the broker confirms the connection.
  startMQTT();
  startWebSocket();
  ethModePubPending = true;
}

//=======================================================================
// Switch from Ethernet to WiFi: re-enable WiFi, update state, restart services.
// Service restart happens in loopWifi() WIFI_RECONNECTED state once connected.
//=======================================================================
static void switchToWiFi() {
  DebugTln(F("Network: switching to WiFi — re-enabling radio"));
  state.net.eMode = NET_WIFI;
  state.net.bEthernetLink = false;
  ethInitialized = false;
  // Symmetric undo of switchToEthernet()'s WIFI_OFF: power the station radio
  // back up and restore SDK auto-reconnect before begin(), otherwise begin()
  // does nothing while the radio is still off.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  // Note: MQTT/WebSocket restart happens in loopWifi()/WIFI_RECONNECTED once
  // the WiFi association is confirmed. Publishing "wifi" here would race with
  // a not-yet-connected WiFi — loopWifi() handles the reconnect announcement.
}

//=======================================================================
// initEthernet() — called once from setup() after I2C init.
// 1. Probes W5500 via SPI VERSION register
// 2. If found, derives MAC and attempts DHCP (1s timeout)
// 3. If cable is in and DHCP succeeds, switches away from WiFi
//=======================================================================
void initEthernet() {
  // Step 1: hardware probe
  state.hw.bEthernetPresent = probeW5500();
  if (!state.hw.bEthernetPresent) return;

  // Step 2: derive MAC
  deriveEthMAC();
  DebugTf(PSTR("Ethernet MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n"),
          ethMac[0], ethMac[1], ethMac[2], ethMac[3], ethMac[4], ethMac[5]);

  // Step 3: try DHCP (1s timeout — fast boot)
  if (startEthernet(1000)) {
    switchToEthernet();
  } else {
    DebugTln(F("Ethernet: W5500 present but no link/DHCP — staying on WiFi"));
  }
}

//=======================================================================
// loopEthernet() — called from doBackgroundTasks(), every 5 seconds.
// Monitors link state and handles automatic WiFi↔Ethernet failover.
// Self-guarded: returns immediately if no W5500 hardware.
//
// Transition logic:
//   WiFi active + cable inserted  → DHCP → switchToEthernet() (publishes "ethernet")
//   Ethernet active + cable lost  → switchToWiFi()             (publishes "wifi" via loopWifi/RECONNECTED)
//=======================================================================
void loopEthernet() {
  if (!state.hw.bEthernetPresent) return;

  DECLARE_TIMER_SEC(timerEthCheck, 5, CATCH_UP_MISSED_TICKS);
  if (!DUE(timerEthCheck)) return;

  // Drain pending "ethernet" mode publish once MQTT has reconnected
  if (ethModePubPending && state.mqtt.bConnected) {
    sendMQTTData(F("otgw-firmware/network/mode"), F("ethernet"), true);
    ethModePubPending = false;
  }

  bool linkUp = (Ethernet.linkStatus() == LinkON);
  state.net.bEthernetLink = linkUp;

  if (linkUp && state.net.eMode == NET_WIFI) {
    // Cable just plugged in — try to switch to Ethernet
    DebugTln(F("Ethernet: link detected, attempting DHCP"));
    if (startEthernet(2000)) {
      switchToEthernet();
    }
  } else if (!linkUp && state.net.eMode == NET_ETHERNET) {
    // Cable unplugged — fall back to WiFi.
    // Publish while MQTT is still connected via Ethernet (before the switch drops it).
    sendMQTTData(F("otgw-firmware/network/mode"), F("wifi"), true);
    DebugTln(F("Ethernet: link lost, falling back to WiFi"));
    switchToWiFi();
  }

  // Keep DHCP lease alive when on Ethernet
  if (state.net.eMode == NET_ETHERNET) {
    Ethernet.maintain();
  }
}

//=======================================================================
// getEthernetIPString() / getEthernetMACString()
// Helpers called by restAPI.ino when on Ethernet to show the correct IP/MAC.
//=======================================================================
String getEthernetIPString() {
  return Ethernet.localIP().toString();
}

String getEthernetMACString() {
  char buf[18];
  snprintf_P(buf, sizeof(buf), PSTR("%02X:%02X:%02X:%02X:%02X:%02X"),
             ethMac[0], ethMac[1], ethMac[2], ethMac[3], ethMac[4], ethMac[5]);
  return String(buf);
}

#endif // HAS_ETH_CAPABLE

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/
